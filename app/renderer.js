'use strict';
const $=id=>document.getElementById(id);let connected=false,busy=false,legacy=false;
const show=(message,error=false)=>{$('log').textContent=message;$('log').classList.toggle('error',error);};
function update(){for(const id of ['install-button','save','start','stop','status'])$(id).disabled=!connected||busy; $('install-button').disabled ||= !$('backup').checked||(legacy&&!$('legacy').checked);$('connect-button').disabled=busy;}
function config(){return {provider:$('provider').value,model:$('model').value.trim(),api_key:$('key').value.trim(),reasoning_effort:$('effort').value,timezone:Intl.DateTimeFormat().resolvedOptions().timeZone||'UTC'};}
async function run(fn,success){busy=true;update();try{const r=await fn();if(!r.ok)throw Error(r.error);if(success)success(r.result);}catch(e){show(e.message,true);}finally{busy=false;update();}}
$('connect-button').onclick=()=>run(()=>window.tom.connect($('password').value),r=>{connected=true;legacy=r.legacy;$('password').value='';$('connection').textContent='Connected · USB';$('device').textContent=`Paper Pro · ${r.version} · ready`;$('legacy-row').hidden=!legacy;show('');});
$('provider').onchange=()=>{const claude=$('provider').value==='anthropic';$('model').value=claude?'claude-haiku-5-5':'gpt-5-mini';$('key').value='';$('effort').disabled=claude;$('provider-note').textContent=claude?'Claude supports answers, tables and local diagrams. Web search and raster image generation require OpenAI in this beta. Responses are rendered after the full Claude answer arrives.':'Web search and image generation use additional OpenAI API calls. Your API plan is billed separately from this app.';};
$('backup').onchange=update;$('legacy').onchange=update;
$('developer-docs').onclick=()=>window.tom.openLink('developer');$('get-key').onclick=()=>window.tom.openLink($('provider').value);
$('validate-key').onclick=()=>run(()=>window.tom.validateKey(config()),r=>show(r.message));
$('install-button').onclick=()=>run(()=>window.tom.install({config:config(),replaceLegacy:$('legacy').checked}),()=>{$('key').value='';show('Installed. Unlock your tablet and try a question + zigzag.');});
$('save').onclick=()=>run(()=>window.tom.configure(config()),()=>{$('key').value='';show('Settings saved.');});
$('start').onclick=()=>run(()=>window.tom.start(),()=>show('Unlock your tablet to continue.'));
$('stop').onclick=()=>run(()=>window.tom.stop(),()=>show('Previous setup restored.'));
$('status').onclick=()=>run(()=>window.tom.status(),r=>show(r));window.tom.onProgress(show);update();
