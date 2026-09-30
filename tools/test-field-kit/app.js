'use strict';
const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.hash.slice(1));
let token = params.get('token') || sessionStorage.getItem('fieldkit-token');
if (token) sessionStorage.setItem('fieldkit-token', token);
if (location.hash) history.replaceState(null, '', location.pathname);
let state, ledger, lastImageId, latestResultKey = '', timelineKey = '', archived = false;
const selected = new Set(['equipment', 'commands', 'binoculars']);
const imageURLs = {};
const esc = text => String(text ?? '').replace(/[&<>"']/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
function error(message) { $('error').textContent = message; $('error').hidden = !message; }
async function api(path, options={}) {
  const response = await fetch(path, {...options, headers: {'Authorization': 'Bearer '+token, 'Content-Type':'application/json', ...options.headers}});
  if (!response.ok) { const body = await response.json(); throw Error(body.error || response.statusText); }
  return response;
}
async function command(action, extra={}) {
  error('');
  try {
    const observation = state?.observation;
    await api('/api/command', {method:'POST', body:JSON.stringify({action, epoch:state?.epoch, observation_id:observation?.id,
      reviewed_capture_sha256:observation?.captures?.[0]?.sha256, ...extra})});
    await refresh();
  } catch (reason) { error(reason.message); }
}
function page(name) {
  document.querySelectorAll('.page').forEach(node => node.hidden = node.id !== name);
  document.querySelectorAll('.nav').forEach(node => node.classList.toggle('active',node.dataset.page === name));
  $('heading').textContent = {operations:'Field operations',library:'Scenario library',coverage:'Coverage ledger',archive:'Run archive'}[name];
  if (name === 'coverage') loadCoverage();
}
document.querySelectorAll('.nav').forEach(node => node.onclick = () => page(node.dataset.page));
for (const action of ['connect','observe','continue','stop','disconnect']) $(action).onclick = () => {archived=false; command(action);};
for (const mode of ['human','llm']) $(mode).onclick = () => command('set_mode',{mode});
$('run').onclick = () => {archived=false; command('run',{scenarios:[...selected]});};
function badge(result) {
  const status = result?.status;
  return `<span class="badge ${status==='observed_pass'?'pass':status==='failed'?'failed':status==='stopped'?'stopped':''}">${status==='observed_pass'?'NATIVE PASS':status?esc(status.toUpperCase()):'NOT RUN'}</span>`;
}
function scenarios() {
  $('quick-scenarios').innerHTML = state.scenarios.map(item => `<div class="scenario-row"><input type="checkbox" id="pick-${esc(item.id)}" data-pick="${esc(item.id)}" ${selected.has(item.id)?'checked':''}><label for="pick-${esc(item.id)}">${esc(item.title)}</label>${badge(item.last_result)}</div>`).join('');
  $('library-cards').innerHTML = state.scenarios.map((item,index) => `<article class="panel library-card"><div class="eyebrow">SCENARIO ${String(index+1).padStart(2,'0')} / ${esc(item.category)}</div><h2>${esc(item.title)}</h2><p>${esc(item.description)}</p>${badge(item.last_result)}<div class="fine">Typical budget ~${item.seconds}s · guarded VR inputs · paired eye evidence</div><button data-select="${esc(item.id)}">Select for next run →</button></article>`).join('');
  document.querySelectorAll('[data-pick]').forEach(node => node.onchange = () => {node.checked ? selected.add(node.dataset.pick) : selected.delete(node.dataset.pick); updateSelection();});
  document.querySelectorAll('[data-select]').forEach(node => node.onclick = () => {selected.add(node.dataset.select); scenarios(); page('operations');});
  updateSelection();
}
function updateSelection() { $('selected-count').textContent=selected.size+' SELECTED'; $('run').disabled=!selected.size || !state?.connected || state?.busy || state?.supervisor !== 'human'; }
async function showObservation(observation, historical=false) {
  if (!observation || observation.id === lastImageId) return;
  lastImageId = observation.id;
  const requested = observation.id;
  for (const capture of observation.captures) {
    try {
      const response = await api(capture.url); const blob = await response.blob();
      if (lastImageId !== requested) return;
      if (imageURLs[capture.eye]) URL.revokeObjectURL(imageURLs[capture.eye]);
      imageURLs[capture.eye] = URL.createObjectURL(blob);
      $(capture.eye+'-eye').src=imageURLs[capture.eye]; $(capture.eye+'-eye').hidden=false; $(capture.eye+'-empty').hidden=true;
    } catch (reason) {error(reason.message);}
  }
  $('capture-note').textContent=(historical?'ARCHIVED EVIDENCE · ':'')+observation.capture_note;
}
function render() {
  $('connection').textContent=state.connected?'RUNNER CONNECTED / '+(state.identity.process?.ProcessId || ''):'RUNNER DISCONNECTED';
  $('signal').classList.toggle('online',state.connected);
  $('mode-stamp').textContent=state.supervisor==='human'?'HUMAN SUPERVISION':'EXTERNAL LLM SUPERVISION';
  $('human').classList.toggle('selected',state.supervisor==='human'); $('llm').classList.toggle('selected',state.supervisor==='llm');
  $('supervisor-note').textContent=state.supervisor==='human'?'You review the pictures and choose a scenario. Scripted steps run independently.':'External agent may submit the next decision. Switching alone does not connect a model. Human Stop remains available.';
  $('status').textContent=state.status;
  $('epoch').textContent='CONTROL GENERATION '+String(state.epoch).padStart(2,'0');
  const telem=state.telemetry, native=telem.native || {};
  $('scene').textContent=telem.scene || '—'; $('mission').textContent=native.mission ?? '—';
  $('context').textContent=telem.controls?.context || '—';
  $('posture').textContent=native.status_CRAWL?'Prone':native.status_SQUAT?'Crouched':native.status_STAND?'Standing':'—';
  const position=native.player_pos || native.position || native.player_position || (typeof native.player_x==='number'?[native.player_x,native.player_y,native.player_z]:null);
  $('position').textContent=Array.isArray(position)?position.map(value=>Number(value).toFixed(1)).join(' / '):position?JSON.stringify(position):'—';
  $('build').textContent=state.identity.dll_sha256?.slice(0,12) || '—';
  $('raw').textContent=JSON.stringify(state.observation || {},null,2);
  $('connect').disabled=state.connected || state.busy || state.supervisor!=='human';
  for (const id of ['observe','continue']) $(id).disabled=!state.connected || state.busy || state.supervisor!=='human';
  if (!archived) {
    showObservation(state.observation);
    const age=state.observation?Math.max(0,Math.round(Date.now()/1000-state.observation.time)):null;
    $('image-age').textContent=age===null?'NO CAPTURE':(!state.connected?'SAVED · ':age>30?'STALE · ':'CAPTURE · ')+age+'s AGO';
  }
  const resultKey=JSON.stringify(state.results.map(result=>[result.scenario,result.finished,result.status,result.visual_acceptance]));
  if (resultKey!==latestResultKey || !$('quick-scenarios').children.length) {latestResultKey=resultKey;scenarios();runs();}
  updateSelection();
  const nextTimeline=JSON.stringify(state.timeline);
  if (nextTimeline!==timelineKey) {
    timelineKey=nextTimeline;
    $('timeline').innerHTML=[...state.timeline].reverse().slice(0,30).map(item=>`<li><time>${new Date(item.time*1000).toLocaleTimeString()}</time><strong>${esc(item.kind.replaceAll('_',' '))}</strong><br>${esc(item.detail.error || item.detail.cleanup_error || item.detail.action || item.detail.scenario || item.detail.reason || item.detail.id || item.detail.mode || '')} ${esc(item.detail.status || '')}</li>`).join('') || '<li>No actions dispatched.</li>';
  }
}
function runs() {
  $('runs').innerHTML=[...state.results].reverse().map(result=>`<article class="panel run-card"><div class="eyebrow">${esc(new Date(result.finished*1000).toLocaleString())} / ${esc(result.identity?.dll_sha256?.slice(0,12))}</div><h2>${esc(result.scenario)}</h2>${badge(result)}<p>${result.cases?.filter(item=>item.status==='observed_pass').length || 0} native checks passed · visual acceptance: ${esc(result.visual_acceptance)}</p><p>${esc(result.error || result.cleanup_error || result.cases?.find(item=>item.error)?.error || 'No reported runner error.')}</p><code>${esc(result.output)}</code>${result.observation?`<p><button data-review="${esc(result.run_id)}">Review captured eyes</button></p>`:''}</article>`).join('') || '<div class="panel run-card"><h2>No runs yet</h2><p>Connect a regular simulator session, refresh the eyes and run your selected checks. Every result is saved locally.</p></div>';
  document.querySelectorAll('[data-review]').forEach(button=>button.onclick=()=>{const result=state.results.find(item=>item.run_id===button.dataset.review);archived=true;lastImageId=null;showObservation(result.observation,true);$('image-age').textContent='ARCHIVE / '+new Date(result.finished*1000).toLocaleString();page('operations');});
}
async function loadCoverage() {
  try { if(!ledger) ledger=await (await api('/api/coverage')).json();
    $('coverage-counts').innerHTML=Object.entries(ledger.counts).map(([name,count])=>`<div class="metric"><b>${count}</b><span>${esc(name.replaceAll('_',' ').toUpperCase())}</span></div>`).join('');
    $('coverage-scope').textContent=ledger.scope+' · '+ledger.reports.length+' reports'; filterCoverage();
  } catch(reason) {error(reason.message);}
}
function filterCoverage() {
  const search=$('coverage-filter').value.toLowerCase();
  $('coverage-list').innerHTML=ledger.reports.filter(report=>JSON.stringify(report).toLowerCase().includes(search)).map(report=>`<details class="report"><summary><b>${esc(report.id)} / ${esc(report.priority)}</b> · ${esc(report.title)} <span class="badge">${esc(report.status.replaceAll('_',' '))}</span></summary>${report.claims.map(claim=>`<p><b>${esc(claim.id)} — ${esc(claim.status.replaceAll('_',' '))}</b><br>${esc(claim.finding || claim.title)}<br>Next: ${esc(claim.next_action || 'Review source evidence.')}</p>`).join('')}</details>`).join('');
}
$('coverage-filter').oninput=filterCoverage;
async function refresh() { if(!token) {error('Open the authenticated field-kit link from the launcher or connection.json.');return;} try {state=await (await api('/api/state')).json();render();} catch(reason) {error(reason.message);} }
async function poll() {await refresh();setTimeout(poll,1200);} poll();
