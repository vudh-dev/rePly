const path=require('node:path');const fs=require('node:fs');const crypto=require('node:crypto');const {execFileSync}=require('node:child_process');
(async()=>{
 const {packager}=await import('@electron/packager');const root=path.resolve(__dirname,'..');
 const targets=process.argv.slice(2);if(!targets.length)targets.push('darwin-arm64','darwin-x64','win32-x64','linux-x64','linux-arm64');
 const version=require('../package.json').version;
 for(const target of targets){const [platform,arch]=target.split('-');
  if(!['darwin','win32','linux'].includes(platform)||!['arm64','x64'].includes(arch))throw Error('Invalid target');
  const paths=await packager({dir:root,name:'rePly',platform,arch,out:path.join(root,'dist'),overwrite:true,asar:true,prune:true,
   icon:platform==='darwin'?path.join(root,'app/icon.icns'):platform==='linux'?path.join(root,'app/icon.png'):undefined,
   appBundleId:'org.reply.setup',appVersion:version,executableName:'rePly',
   ignore:[/^\/dist(\/|$)/,/^\/release(\/|$)/,/^\/tablet(\/|$)/,/^\/tests(\/|$)/,/^\/scripts(\/|$)/,/^\/docs(\/|$)/,/^\/\.git(\/|$)/,/^\/\.github(\/|$)/],
   osxSign:undefined});
  const folder=paths[0];
  if(platform==='darwin')execFileSync('/usr/bin/codesign',['--force','--deep','--sign','-',path.join(folder,'rePly.app')]);
  const output=path.join(root,'release',`rePly-${version}-${target}.zip`);fs.mkdirSync(path.dirname(output),{recursive:true});
  if(process.platform==='darwin')execFileSync('/usr/bin/ditto',['-c','-k','--sequesterRsrc','--keepParent',folder,output]);
  else if(process.platform==='win32')execFileSync('powershell',['-NoProfile','-Command',`Compress-Archive -Path '${folder}' -DestinationPath '${output}' -Force`]);
  else execFileSync('zip',['-qr',output,path.basename(folder)],{cwd:path.dirname(folder)});
  console.log(output);
 }
 const releases=fs.readdirSync(path.join(root,'release')).filter(x=>x.endsWith('.zip'));
 fs.writeFileSync(path.join(root,'release','SHA256SUMS'),releases.map(name=>crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'release',name))).digest('hex')+'  '+name).join('\n')+'\n');
})().catch(e=>{console.error(e);process.exit(1)});
