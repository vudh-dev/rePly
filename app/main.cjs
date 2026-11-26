'use strict';
const {app,BrowserWindow,ipcMain,dialog,shell}=require('electron');
const path=require('node:path');
const {Tablet,validateConfig}=require('./core.cjs');
let win,tablet,running=false;
app.setName('rePly');
app.whenReady().then(()=>{
  win=new BrowserWindow({width:1100,height:850,minWidth:850,minHeight:700,backgroundColor:'#f5f1e8',title:'rePly',webPreferences:{preload:path.join(__dirname,'preload.cjs'),nodeIntegration:false,contextIsolation:true,sandbox:true}});
  win.on('close',e=>{if(running){e.preventDefault();dialog.showMessageBox(win,{type:'info',message:'Please wait for the current operation to finish before closing rePly.'});}});
  win.setMenuBarVisibility(false);win.webContents.setWindowOpenHandler(()=>({action:'deny'}));
  win.webContents.on('will-navigate',e=>e.preventDefault());
  tablet=new Tablet({userData:app.getPath('userData'),resources:__dirname,
    progress:text=>win?.webContents.send('progress',text),
    confirm:async fingerprint=>(await dialog.showMessageBox(win,{type:'question',buttons:['Cancel','Trust this USB tablet'],defaultId:0,cancelId:0,title:'Verify your tablet',message:'Trust the tablet connected at 10.11.99.1?',detail:`SSH host fingerprint:\n${fingerprint}\n\nOnly continue with your own tablet connected directly over USB. This identity will be pinned; unexpected changes are rejected.`})).response===1});
  function handler(name,fn){ipcMain.handle(name,async(event,arg)=>{
    if(event.sender!==win.webContents || event.senderFrame!==win.webContents.mainFrame)throw Error('Invalid sender');
    if(running)throw Error('Wait for the current operation to finish.');running=true;
    try{return {ok:true,result:await fn(arg)};}catch(e){return {ok:false,error:e.message};}finally{running=false;}
  });}
  handler('connect',password=>tablet.connect(password));
  handler('install',arg=>tablet.install(arg.config,!!arg.replaceLegacy));
  handler('configure',config=>tablet.configure(config));
  handler('start',()=>tablet.start());handler('stop',()=>tablet.stop());handler('status',()=>tablet.status());
  handler('validate-key',async value=>{
    const config=validateConfig(value);const claude=config.provider==='anthropic';
    const res=await fetch(claude?'https://api.anthropic.com/v1/models':'https://api.openai.com/v1/models',{
      headers:claude?{'x-api-key':config.api_key,'anthropic-version':'2023-06-01'}:{Authorization:'Bearer '+config.api_key},signal:AbortSignal.timeout(20000)});
    if(!res.ok)throw Error(`Key check failed (HTTP ${res.status}). Check billing, permissions and provider.`);
    const data=await res.json();const found=(data.data||[]).some(m=>m.id===config.model);
    return {valid:true,modelListed:found,message:found?'Key accepted; model listed.':'Key accepted. Model not in the first model-list page; confirm the exact ID in your provider console.'};
  });
  const links={developer:'https://developer.remarkable.com/documentation/developer-mode',openai:'https://platform.openai.com/api-keys',anthropic:'https://platform.claude.com/settings/keys'};
  ipcMain.handle('open-link',(event,id)=>{if(event.sender!==win.webContents||!links[id])return;return shell.openExternal(links[id]);});
  win.loadFile(path.join(__dirname,'index.html'));
});
app.on('before-quit',()=>tablet?.disconnect());app.on('window-all-closed',()=>app.quit());
