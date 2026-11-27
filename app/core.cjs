'use strict';
const crypto = require('node:crypto');
const fs = require('node:fs/promises');
const path = require('node:path');
const {Client} = require('ssh2');
const manifest = require('./dependencies.json');
const HOST = '10.11.99.1';
const SUPPORTED = new Set(['3.29.0.153', '3.29.1.0']);
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
function validateConfig(v) {
  if (!v || !['openai','anthropic'].includes(v.provider)) throw Error('Choose OpenAI or Claude.');
  if (typeof v.api_key !== 'string' || !/^sk-[A-Za-z0-9_-]{16,512}$/.test(v.api_key)) throw Error('Enter a valid provider API key.');
  if (v.provider === 'anthropic' && !v.api_key.startsWith('sk-ant-')) throw Error('Use an Anthropic API key for Claude.');
  if (v.provider === 'openai' && v.api_key.startsWith('sk-ant-')) throw Error('Use an OpenAI API key for OpenAI.');
  if (!/^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$/.test(v.model || '')) throw Error('Enter a valid model ID.');
  if (!['low','medium','high'].includes(v.reasoning_effort)) throw Error('Choose a reasoning effort.');
  try { new Intl.DateTimeFormat('en', {timeZone:v.timezone}).format(); } catch { throw Error('Invalid timezone.'); }
  return {provider:v.provider,api_key:v.api_key,model:v.model,reasoning_effort:v.reasoning_effort,service_tier:'default',timezone:v.timezone,demo_mode:false};
}
function parseDevice(text) {
  const lines=text.trim().split('\n');
  const version=(lines.find(x=>x.startsWith('IMG_VERSION='))||'').replace('IMG_VERSION=','').replaceAll('"','');
  if (!SUPPORTED.has(version)) throw Error(`Firmware ${version || 'unknown'} is not supported. This beta supports only 3.29.0.153 and 3.29.1.0. Do not downgrade to install it.`);
  if (!text.includes('reMarkable Ferrari') || !lines.includes('aarch64')) throw Error('This beta requires reMarkable Paper Pro (Ferrari). Other models are not supported.');
  return {version, legacy:text.includes('LEGACY_INK_DIARY'), existing:text.includes('TOM_INSTALLED')};
}
class Tablet {
  constructor({userData,resources,confirm,progress}) {Object.assign(this,{userData,resources,confirm,progress});this.conn=null;this.device=null;}
  async connect(password) {
    if(typeof password!=='string'||!password||password.length>256)throw Error('Enter the SSH password shown on your tablet.');
    this.disconnect();
    const pinsPath=path.join(this.userData,'device-host-key.json');
    let pinned;try{pinned=JSON.parse(await fs.readFile(pinsPath,'utf8')).fingerprint;}catch{}
    const conn=new Client();
    try {
      await new Promise((resolve,reject)=>{
        conn.once('ready',resolve);conn.once('error',reject);
        conn.connect({host:HOST,port:22,username:'root',password,readyTimeout:20000,keepaliveInterval:10000,
          hostVerifier:(key,cb)=>{
            const fingerprint='SHA256:'+crypto.createHash('sha256').update(key).digest('base64').replace(/=+$/,'');
            if(pinned===fingerprint)return cb(true);
            if(pinned)return cb(false); // Never silently replace a pinned host identity.
            this.confirm(fingerprint).then(async ok=>{
              if(ok){await fs.mkdir(this.userData,{recursive:true});await fs.writeFile(pinsPath,JSON.stringify({host:HOST,fingerprint}),{mode:0o600});}
              cb(ok);
            }).catch(()=>cb(false));
          }});
      });
      this.conn=conn;
      const info=await this.exec('cat /etc/os-release; uname -m; cat /sys/firmware/devicetree/base/model; echo; test ! -f /home/root/xovi/extensions.d/ink-diary.so || echo LEGACY_INK_DIARY; test ! -f /home/root/.config/reply/config.json || echo TOM_INSTALLED');
      this.device=parseDevice(info);return this.device;
    }catch(e){conn.end();this.conn=null;throw Error(e.message.includes('supported')||e.message.includes('requires')?e.message:'Could not connect or verify the tablet. Keep USB connected, enable developer mode, and check the SSH password. A changed host key must be reviewed in the app data folder.');}
  }
  disconnect(){this.conn?.end();this.conn=null;this.device=null;}
  exec(command,input,timeout=30000) {
    if(!this.conn)return Promise.reject(Error('Connect to your tablet first.'));
    return new Promise((resolve,reject)=>{
      let output='',error='',settled=false;
      const done=(err,value)=>{if(settled)return;settled=true;clearTimeout(timer);err?reject(err):resolve(value);};
      let channel;const timer=setTimeout(()=>{channel?.close();done(Error('Tablet operation timed out. Keep it awake and connected.'));},timeout);
      this.conn.exec(command,(err,stream)=>{
        if(err)return done(err);channel=stream;
        stream.on('data',b=>{if(output.length<200000)output+=b.toString();});
        stream.stderr.on('data',b=>{if(error.length<10000)error+=b.toString();});
        stream.on('error',e=>done(e));
        stream.on('close',code=>done(code===0?null:Error(`Tablet operation failed (${code}): ${error.slice(-1200) || output.slice(-1200)}`),output));
        if(input!==undefined)stream.end(input);
      });
    });
  }
  async put(local,remote) {
    if(!/^\/home\/root\/[A-Za-z0-9_./-]+$/.test(remote))throw Error('Invalid installation path');
    const sftp=await new Promise((resolve,reject)=>this.conn.sftp((e,s)=>e?reject(e):resolve(s)));
    try{await new Promise((resolve,reject)=>sftp.fastPut(local,remote,e=>e?reject(e):resolve()));}finally{sftp.end();}
  }
  async dependency(asset) {
    const file=path.join(this.userData,'downloads',asset.name);await fs.mkdir(path.dirname(file),{recursive:true});
    try{if(hash(await fs.readFile(file))===asset.sha256)return file;}catch{}
    this.progress('Downloading '+asset.name+' from its pinned upstream release…');
    const r=await fetch(asset.url,{signal:AbortSignal.timeout(120000)});if(!r.ok)throw Error('Dependency download failed. Try again when online.');
    const bytes=Buffer.from(await r.arrayBuffer());if(bytes.length>100*1024*1024||hash(bytes)!==asset.sha256)throw Error('Dependency checksum mismatch; installation stopped.');
    await fs.writeFile(file,bytes,{mode:0o600});return file;
  }
  async install(config,replaceLegacy=false) {
    config=validateConfig(config);if(!this.device)throw Error('Connect first.');
    if(this.device.legacy&&!replaceLegacy)throw Error('An existing Ink Diary installation was detected. Confirm switching to the isolated public runtime. Your existing installation stays in place.');
    const guard=path.join(this.resources,'check-tablet-runtime.sh');
    await this.exec('sh -s',await fs.readFile(guard));
    this.progress('Downloading and checking dependencies…');
    const downloads=[];for(const asset of manifest)downloads.push(await this.dependency(asset));
    await this.exec('umask 077; mkdir -p /home/root/reply-setup');
    for(const f of downloads)await this.put(f,'/home/root/reply-setup/'+path.basename(f));
    await this.put(path.join(this.resources,'tablet-payload.tar.gz'),'/home/root/reply-setup/tablet-payload.tar.gz');
    await this.put(path.join(this.resources,'install-tablet.sh'),'/home/root/reply-setup/install-tablet.sh');
    await this.exec('umask 077; cat > /home/root/reply-setup/check-tablet-runtime.sh',await fs.readFile(guard));
    this.progress('Installing tablet files; preserving existing settings…');
    await this.exec(`sh /home/root/reply-setup/install-tablet.sh ${replaceLegacy?'replace-legacy':'fresh'}`,undefined,60000);
    await this.configure(config);
    this.progress('Unlock the tablet now. Open any notebook so the firmware resource index can finish. This may take up to 3 minutes.');
    await this.exec("setsid sh /home/root/.local/share/reply/rebuild.sh > /home/root/.local/share/reply/rebuild.log 2>&1 < /dev/null &",undefined,10000);
    let ready=false;
    for(let i=0;i<90;i++){
      await new Promise(r=>setTimeout(r,2000));
      const state=await this.exec('if [ -f /home/root/.local/share/reply/rebuild.ok ]; then echo READY; elif [ -f /home/root/.local/share/reply/rebuild.failed ]; then echo FAILED; else echo WAIT; fi');
      if(state.includes('READY')){ready=true;break;}if(state.includes('FAILED'))throw Error('Firmware index rebuild failed. Use Return to previous setup and retry.');
    }
    if(!ready)throw Error('Waiting for tablet unlock timed out. Use Return to previous setup, then install again with the tablet unlocked.');
    await this.start();this.progress('Installed. Unlock the tablet and open a disposable notebook. Write 7+8 followed by a zigzag and lift the pen.');
    return {ok:true};
  }
  async configure(config) {
    config=validateConfig(config);
    await this.exec('set -e; umask 077; mkdir -p /home/root/.config/reply; cat > /home/root/.config/reply/config.json.new; chmod 600 /home/root/.config/reply/config.json.new; mv /home/root/.config/reply/config.json.new /home/root/.config/reply/config.json',JSON.stringify(config));
    return {ok:true};
  }
  async start(){
    await this.exec('sh -s',await fs.readFile(path.join(this.resources,'check-tablet-runtime.sh')));
    await this.exec('sh /home/root/.local/share/reply/start.sh');return {ok:true};
  }
  async stop(){await this.exec('sh /home/root/.local/share/reply/stop.sh');return {ok:true};}
  async status(){return this.exec('timeout 5 sh -c \'cat /run/xovi-mb-out & reader=$!; printf "%s\\n" ">reply\\$status:" > /run/xovi-mb; wait "$reader"\' || true').then(text => text.trim() || 'No plugin response. Unlock and open a notebook, or click Start.').catch(()=> 'No plugin response. Unlock and open a notebook, or click Start.');}
}
module.exports={Tablet,validateConfig,parseDevice,hash,SUPPORTED};
