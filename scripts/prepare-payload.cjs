const fs=require('node:fs');const path=require('node:path');const {execFileSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),build=path.join(root,'.payload-build'),payload=path.join(build,'payload');fs.mkdirSync(payload,{recursive:true});
for(const [from,to] of [['tablet/bin/reply.so','reply.so'],['tablet/reply.qmd','reply.qmd'],['tablet/fonts/Caveat.ttf','Caveat.ttf'],['tablet/fonts/DejaVuSansMono.ttf','DejaVuSansMono.ttf'],['app/start-tablet.sh','start.sh'],['app/stop-tablet.sh','stop.sh'],['app/rebuild-tablet.sh','rebuild.sh']])fs.copyFileSync(path.join(root,from),path.join(payload,to));
for(const name of fs.readdirSync(path.join(root,'tablet/icons')).filter(x=>x.endsWith('.svg')))fs.copyFileSync(path.join(root,'tablet/icons',name),path.join(payload,name));
execFileSync('tar',['-czf',path.join(root,'app/tablet-payload.tar.gz'),'-C',build,'payload']);fs.rmSync(build,{recursive:true});
console.log('Tablet payload prepared');
