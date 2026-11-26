const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('tom',{
 connect:p=>ipcRenderer.invoke('connect',p),install:v=>ipcRenderer.invoke('install',v),configure:v=>ipcRenderer.invoke('configure',v),
 start:()=>ipcRenderer.invoke('start'),stop:()=>ipcRenderer.invoke('stop'),status:()=>ipcRenderer.invoke('status'),
 validateKey:v=>ipcRenderer.invoke('validate-key',v),openLink:id=>ipcRenderer.invoke('open-link',id),
 onProgress:fn=>ipcRenderer.on('progress',(_,text)=>fn(text))
});
