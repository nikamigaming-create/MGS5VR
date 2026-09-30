import {ControllerPlayer} from './controller-player.js';
import {CassetteDeck} from './cassette-deck.js';
import {paintCommunity,openCommunityProof} from './community-notes.js';
import {inputNames,pretty,parseBinding,serializeBinding,bindingLabel,resolveLesson,settingOptions,settingWidget,touchForControl,bindingHands} from './binding-model.js';
import {controlsTour,tourAt} from './controls-tour.js';
import {FitPreview,fitContexts,fitContext} from './fit-preview.js';
const $=id=>document.getElementById(id),escape=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let state=null,catalog=null,player=null,deck=null,tab='home',settingsKind='controls',selectedAction='',selectedLesson=null,alternative=0,editAlternative=0,draftKind='controls',draft={},requestId=0,ready=false,loading=false,autoView=true,mode='gameplay',pickedToken=null;
const pending=new Map(),video=$('lesson-video');let lessonGeneration=0;
let fit=null,fitFilter='all';
document.querySelector('head').insertAdjacentHTML('beforeend','<link rel="stylesheet" href="fit-preview.css">');
$('settings-grid').insertAdjacentHTML('beforebegin',`<aside id="fit-panel" class="fit-panel"><div class="fit-heading"><span class="eyebrow">LIVE / 3D FIT</span><h2 id="fit-title">iDroid</h2><p id="fit-subtitle">Right hand · device · projector · screen</p></div><div class="fit-contexts" aria-label="Preview object">${Object.entries(fitContexts).map(([key,[label]])=>`<button data-fit-context="${key}" aria-pressed="${key==='idroid'}">${label}</button>`).join('')}</div><div class="fit-stage"><canvas id="fit-canvas" aria-label="Rotatable 3D VR fitting preview" tabindex="0"></canvas></div><div class="fit-tools" aria-label="Preview view"><button data-fit-view="front">Front</button><button data-fit-view="side">Side</button><button data-fit-view="top">Top</button><button data-fit-view="back">Back</button><button data-fit-view="front" aria-label="Reset preview camera">Reset view</button></div><p class="help">Drag to rotate · scroll to zoom · arrow keys to turn</p><div class="fit-options"><label><input id="fit-touch" type="checkbox"> Preview touch curl</label><label><input id="fit-motion" type="checkbox"> Preview aim shake</label></div><output id="fit-readout"></output><p class="help">Reference models show your current edits. Save to apply them in game, then check the fit in your headset.</p><div class="fit-actions"><button id="fit-filter" aria-pressed="false">Show only these settings</button><button id="fit-reset">Reset this fit</button></div></aside>`);
const settingsLayout=document.createElement('div');settingsLayout.id='fit-layout';$('fit-panel').before(settingsLayout);settingsLayout.append($('fit-panel'),$('settings-grid'));
$('fit-touch').closest('.fit-options').insertAdjacentHTML('beforebegin','<label id="fit-arm-label" for="fit-arm-display" hidden>Preview arm display<select id="fit-arm-display"><option value="status">Weapon / ammo</option><option value="hud">HUD popup</option><option value="picker">Equipment picker</option></select></label>');
function fitValues(){return {...catalog?.defaultValues,...state?.values,...state?.runtime,...draft,'fit.theatre':settingsKind==='runtime'};}
function updateFit(name){
 if(!fit||settingsKind==='display')return;
 const context=fitContext(name||'');if(context&&context!==fit.context)fit.setContext(context);
 if(name&&context==='wrist'){if(/picker_width|selector_height/.test(name))fit.armDisplay='picker';else if(/hud_mode/.test(name))fit.armDisplay='hud';else if(/weapon_hud/.test(name))fit.armDisplay='status';}
 fit.update(fitValues());
 const [title,subtitle]=fitContexts[fit.context];$('fit-title').textContent=title;$('fit-subtitle').textContent=subtitle;
 document.querySelectorAll('[data-fit-context]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.fitContext===fit.context)));
 const v=(key,fallback=0)=>fit.value(key,fallback),kind=fit.context;
 const messages={
  idroid:`Screen ${v('idroid_screen_width_cm',45)} × ${(v('idroid_screen_width_cm',45)*9/16).toFixed(1)} cm · projector distance ${v('idroid_screen_depth_cm',8)} cm. ${v('handheld_menus')?'Handheld mode is on.':'Handheld mode is off; this previews its saved fit.'}`,
  wrist:`LEFT bionic forearm · clearance ${v('wrist_surface_lift_cm',2)} cm · text ${v('wrist_text_scale',1.5)}× · HUD ${fit.values['settings.hud_mode']||'binoculars_only'}.`,
  hands:`Green: acquire within ${v('support_grip_radius_cm',10)} cm. Amber: release beyond ${v('support_detach_radius_cm',30)} cm. Stabilization ${v('weapon_smoothing_ms')} ms.`,
  optics:`Scope eye distance ${v('scope_eye_relief_cm',10)} cm · binocular activation within ${v('binocular_max_eye_distance_cm',30)} cm. Auto mark ${v('binocular_auto_mark',1)?'on, '+v('binocular_mark_dwell_ms',650)+' ms':'off'} · target glow ${v('binocular_actor_glow',1)?'on':'off'}.`,
  menus:settingsKind==='runtime'?`Theatre ${fit.values['theatre.width_cm']} cm wide · ${fit.values['theatre.distance_cm']} cm away.`:`Panel ${v('menu_quad_width_cm',120)} cm wide · ${v('menu_quad_distance_cm',130)} cm away · ${v('menu_quad_tilt_degrees',-10)}° tilt.`,
  movement:`Eye height shift ${v('player_height_offset_cm')} cm · turning ${fit.values['settings.turn_mode']||'native_smooth'} · physical melee ${v('motion_melee',1)?'on':'off'} · animal touch ${v('animal_touch',1)?'on':'off'}.`
 };
 $('fit-readout').textContent=messages[kind];
 $('fit-touch').closest('label').hidden=kind!=='hands';$('fit-motion').closest('label').hidden=!['hands','optics'].includes(kind);
 $('fit-arm-label').hidden=kind!=='wrist';$('fit-arm-display').value=fit.armDisplay;
 $('fit-reset').disabled=!state?.writable;$('fit-filter').textContent=fitFilter==='all'?'Show only these settings':'Show all settings';$('fit-filter').setAttribute('aria-pressed',String(fitFilter!=='all'));
}
let tourSteps=[],tourTime=0,tourPlaying=false,tourAnchor=0,tourStepKey='';
document.querySelector('#home .archive-copy').insertAdjacentHTML('beforeend','<button data-quick-tour>Quick controls tour →</button>');
$('refresh-bindings').insertAdjacentHTML('beforebegin','<button data-quick-tour>Quick controls tour</button>');
$('binding-view').insertAdjacentHTML('afterbegin','<p class="help">Ten light-touch inputs: four face buttons, two sticks, two triggers and two thumb rests. Touch is separate from a press or squeeze.</p>');
$('controller-canvas').closest('.controller-panel').insertAdjacentHTML('beforeend','<label id="contact-kind-label" for="contact-kind" hidden>Pick a surface as<select id="contact-kind"><option value="press">Press / squeeze</option><option value="touch">Light touch</option></select></label>');
document.querySelector('.instruction-left').insertAdjacentHTML('beforeend',`<div id="tour-view" hidden><div class="eyebrow">QUICK CONTROLS TOUR / YOUR SAVED MAPPING</div><div class="tour-heading"><span id="tour-number"></span><h2 id="tour-title"></h2></div><span id="tour-hands" class="tour-hands"></span><strong id="tour-binding" class="tour-binding"></strong><p id="tour-description"></p><p id="tour-context" class="help"></p><div class="transport"><button id="tour-play" class="primary">Pause tour</button><button id="tour-previous" aria-label="Previous control">←</button><button id="tour-next" aria-label="Next control">→</button><input id="tour-seek" type="range" min="0" step=".01" value="0" aria-label="Controls tour position"><output id="tour-clock"></output></div><p class="help">Red shows a press or squeeze. Amber shows light touch without pressing. All ten contacts are available to map; your existing bindings stay as shown.</p></div>`);
function configureContactPicker(){
 if(player.contactPickerInstalled)return;
 const pick=player.onPick;
 player.onPick=token=>{if(['controls','modes'].includes(tab)&&$('contact-kind').value==='touch'){
  const touch=touchForControl(token);if(!touch){status('This surface uses a squeeze. Choose Press / squeeze to map it.');return;}token=touch;
 }pick(token)};
 player.contactPickerInstalled=true;
}
function tourDuration(){const last=tourSteps.at(-1);return last?last.start+last.seconds:0;}
function renderTour(at=tourTime,now=performance.now()){
 if(!tourSteps.length||!player)return;
 tourTime=Math.max(0,Math.min(tourDuration(),at));const step=tourAt(tourSteps,tourTime),index=tourSteps.indexOf(step);
 if(tourTime>=tourDuration())tourPlaying=false;
 const key=index+':'+step.inputs.join('+');if(key!==tourStepKey){player.setInputs(step.inputs);player.zoomHand=null;player.closeUp=false;tourStepKey=key;}
 player.autoKey=step.inputs.join('+');player.autoSince=now-(tourTime-step.start)*1000;player.autoPresent(step.inputs,now);player.render();
 $('tour-number').textContent=String(index+1).padStart(2,'0')+' / '+tourSteps.length;
 $('tour-title').textContent=step.title;$('tour-hands').textContent=step.hands;$('tour-binding').textContent=step.label;
 $('tour-description').textContent=step.description;$('tour-context').textContent=step.context;
 $('tour-seek').max=tourDuration();$('tour-seek').value=tourTime;$('tour-clock').textContent=tourTime.toFixed(1)+' / '+tourDuration().toFixed(1)+' s';
 $('tour-play').textContent=tourPlaying?'Pause tour':'Play tour';$('binding-label').textContent=step.label;
 $('mapping-status').textContent=step.kind==='touch'?'AVAILABLE TOUCH INPUTS':'SAVED BINDING';
 $('cue-status').textContent=step.hands+' · '+step.context;
 return step;
}
async function startTour(){
 if(changed())throw Error('Save or discard your edits before playing the saved mapping tour.');
 await refresh(true);tourSteps=controlsTour(state.bindings);tourTime=0;tourStepKey='';
 await selectTab('tour');tourPlaying=true;tourAnchor=performance.now();player.manualUntil=0;player.lastAuto=0;renderTour();
}
function seekTour(time,deterministic=false){
 const previous=tourTime;tourPlaying=false;tourTime=Math.max(0,Math.min(tourDuration(),Number(time)||0));
 if(tourTime<=previous)player.lastAuto=0;
 player.manualUntil=0;const step=renderTour(tourTime,deterministic?1000+tourTime*1000:performance.now());
 if(deterministic&&Math.abs(tourTime-previous)>.25){player.azimuth=player.autoTarget.azimuth;player.pitch=player.autoTarget.pitch;player.render();}
 return step;
}
window.chrome?.webview?.addEventListener('message',e=>{const task=pending.get(e.data.id);if(task){clearTimeout(task.timer);pending.delete(e.data.id);e.data.error?task.reject(Error(e.data.error)):task.resolve(e.data.result)}});
function rpc(method,payload={}){
 if(window.__testBridge)return window.__testBridge(method,payload);
 if(!window.chrome?.webview)return Promise.reject(Error('Open MGS5VR-Launcher.exe to read or save your game settings.'));
 return new Promise((resolve,reject)=>{const id=++requestId;const timer=setTimeout(()=>{pending.delete(id);reject(Error('The operation is still taking longer than expected. Reopen the launcher before retrying a save.'))},method==='install'?600000:60000);pending.set(id,{resolve,reject,timer});window.chrome.webview.postMessage({id,method,payload})});
}
function status(message,error=false){$('status').textContent=message;$('status').classList.toggle('error',error)}
async function act(fn){try{return await fn()}catch(e){status(e.message,true);console.error(e);return null}}
function changed(){return Object.keys(draft).length>0}
function mark(name,value,kind='controls'){
 if(!state?.writable)throw Error('Install VR in your selected game before saving settings. The guide currently shows defaults.');
 if(changed()&&draftKind!==kind)throw Error('Save or discard your current edits before editing the other settings file.');
 draftKind=kind;const base=kind==='runtime'?state.runtime:state.values;
 if(String(base[name])===String(value))delete draft[name];else draft[name]=String(value);
 paintDirty();if(name.startsWith('settings.')||fitContext(name))updateFit(name);
}
function paintDirty(){$('save-bar').hidden=!changed();$('dirty-count').textContent=`${Object.keys(draft).length} unsaved changes`;$('save').textContent=draftKind==='runtime'?'Save runtime settings':'Save controls & tuning';$('mapping-status').textContent=tab==='controls'&&changed()?'UNSAVED BINDING':'SAVED BINDING'}
function currentValue(name,kind='controls'){return draftKind===kind&&name in draft?draft[name]:(kind==='runtime'?state.runtime:state.values)[name]}
async function refresh(silent=false){
 if(loading)return;loading=true;
 try{const next=await rpc('load');if(changed()&&state&&(next.controlsRevision!==state.controlsRevision||next.runtimeRevision!==state.runtimeRevision))throw Error('Your settings changed outside the launcher. Discard and reload, or keep your draft for review.');if(!changed())state={...catalog.previewState,...next};
  paintInstallation();if(state.bindings){fillActions();if(tab==='settings')paintSettings();if(tab==='controls')paintBinding();if(tab==='tapes')cue();if(tab==='modes')paintModes();}if(!silent)status(state.needsGame?'Default controls shown. Choose your installed game to edit its bindings.':'Bindings loaded from your game installation.');
 }finally{loading=false}
}
let displayGame='';
function displaySelection(){return {preset:$('display-preset').value,scale:Number($('display-scale').value),width:Number($('display-width').value),height:Number($('display-height').value)}}
function paintDisplay(){
 if(displayGame!==state?.gameExe){
  displayGame=state?.gameExe||'';
  let saved;try{saved=JSON.parse(localStorage.getItem('display:'+displayGame)||'null')}catch{}
  if(!saved&&state?.display?.requestedWidth)saved={preset:'Custom',scale:100,width:state.display.requestedWidth,height:state.display.requestedHeight};
  if(saved&&['Current','Headset','Custom'].includes(saved.preset)){
   $('display-preset').value=saved.preset;$('display-scale').value=saved.scale;$('display-width').value=saved.width;$('display-height').value=saved.height;
  }
  displayMode();$('scale-value').textContent=$('display-scale').value+'%';
 }
 const d=state?.display;
 if(!d)return;
 $('display-actual').textContent=(d.actualWidth?`${d.live?'Live game image':'Last observed game image'}: ${d.actualWidth} × ${d.actualHeight} per eye. `:'No game image measured yet. ')+(d.requestedWidth?`Saved request: ${d.requestedWidth} × ${d.requestedHeight}. `:'')+(d.live&&d.requestedWidth&&(d.actualWidth!==d.requestedWidth||d.actualHeight!==d.requestedHeight)?'The game is not rendering at the requested size. Apply with the game closed, then relaunch.':'');
}
function paintInstallation(){
 paintDisplay();
 $('connection').textContent=state?.gameRunning?'GAME RUNNING':state?.needsGame?'CHOOSE GAME':'GAME FOUND';$('game-path').textContent=state?.gameExe||'Select mgsvtpp.exe from your installed game.';
 $('game-name').textContent=state?.gameExe?.toLowerCase().endsWith('mgsgroundzeroes.exe')?'Ground Zeroes':'The Phantom Pain';
 $('launch').disabled=!state?.installed||!!state?.gameRunning;$('install').disabled=!!state?.gameRunning||!!state?.needsGame;
 $('install').textContent=state?.installed?'Update VR':'Install VR';$('install-status').textContent=state?.gameRunning?'Game is running. Put on your headset to continue.':state?.installed?'VR is installed.':'Choose your game folder, then install VR.';
}
async function controller(){if(!player){player=new ControllerPlayer($('controller-canvas'));await player.load();player.onPick=token=>{if(tab==='controls')act(async()=>{const rows=parseBinding(currentValue(selectedAction));if(!rows.length)rows.push({gesture:'press',milliseconds:300,inputs:[]});const row=rows[Math.min(editAlternative,rows.length-1)];row.inputs=row.inputs.includes(token)?row.inputs.filter(t=>t!==token):[...row.inputs,token];if(!row.inputs.length)row.inputs=[token];mark(selectedAction,serializeBinding(rows));paintBinding()});else if(tab==='modes'){pickedToken=token;paintModes();player.setInputs([token]);player.render();$('binding-label').textContent=pretty(token);$('cue-status').textContent='Actions for this control are listed in the selected mode.'}else{status(`${pretty(token)} selected. Game modes lists its actions in every context.`);player.setInputs([token]);player.render()}};}player.render();return player}
async function selectTab(next){
 $('training').dataset.kind=next;
 tab=next;if(next!=='tour')tourPlaying=false;document.body.dataset.page=next;for(const id of ['home','training','settings','notes'])$(id).hidden=id!==(next==='tapes'||next==='controls'||next==='modes'||next==='tour'?'training':next);
 document.querySelectorAll('nav [data-tab]').forEach(b=>b.setAttribute('aria-selected',String(b.dataset.tab===next)));
 if(next!=='tapes')video.pause();
 if(next==='tapes'||next==='controls'||next==='modes'||next==='tour'){
  $('lesson-view').hidden=next!=='tapes';$('binding-view').hidden=next!=='controls';$('mode-view').hidden=$('mode-cards').hidden=next!=='modes';$('tape-list').hidden=next!=='tapes';$('lesson-alternatives').hidden=next!=='tapes';$('auto-view').hidden=next!=='tapes';$('instruction-title').textContent=next==='tapes'?'Training tapes':next==='modes'?'Game modes':'Controller bindings';$('instruction-kicker').textContent=next==='tapes'?'02 / VIDEO GUIDE':next==='modes'?'03 / CONTROL REFERENCE':'04 / CONFIGURATION';
  $('controller-help').textContent=next==='tapes'?'Drag to rotate, scroll to zoom. Auto view turns the controllers to show the button used in the lesson.':next==='modes'?'Click a button on the controller to see what it does in this mode. Drag to rotate; scroll to zoom.':'Click a button on the controller to add or remove it from the selected binding. Drag to rotate; scroll to zoom.';
  $('tour-view').hidden=next!=='tour';$('contact-kind-label').hidden=!['controls','modes'].includes(next);
  if(next==='tour'){$('instruction-title').textContent='Your control map';$('instruction-kicker').textContent='QUICK TOUR';$('controller-help').textContent='Touch the surface shown in amber, or press the control shown in red. Drag to inspect either controller.';}
  await controller();configureContactPicker();if(next==='controls')paintBinding();else if(next==='modes')paintModes();else if(next==='tour')renderTour();else if(selectedLesson)cue();else if(catalog.lessons.length)await openLesson(catalog.lessons[0].id,false);
 }if(next==='home')deck?.render();if(next==='settings')paintSettings();if(next==='notes')paintNotes();paintDirty();
}
function fillActions(){if(!state?.bindings)return;const query=$('action-search').value.toLowerCase();const actions=state.bindings.actions.filter(a=>(a.name+' '+a.label).toLowerCase().includes(query));if(!actions.some(a=>a.name===selectedAction))selectedAction=actions[0]?.name||'';$('action-select').innerHTML=actions.map(a=>`<option value="${escape(a.name)}">${escape(a.name.replaceAll('_',' ').replace('.',' / '))}</option>`).join('');$('action-select').value=selectedAction}
function paintBinding(){
 if(!state?.bindings||!selectedAction)return;const action=state.bindings.actions.find(a=>a.name===selectedAction);$('action-context').textContent='Used in: '+action.contexts.join(', ')+'. Select buttons below or on the 3D controller.';
 let rows=parseBinding(currentValue(selectedAction));$('binding-expression').value=currentValue(selectedAction);editAlternative=Math.min(editAlternative,Math.max(0,rows.length-1));
 $('binding-rows').innerHTML=rows.map((row,i)=>`<div class="binding-row" data-binding="${i}"><div class="row-head"><span>OPTION ${String(i+1).padStart(2,'0')} ${i?'· ALTERNATIVE':''}</span><button data-remove="${i}" aria-label="Remove option ${i+1}">×</button></div><label for="gesture-${i}">Gesture</label><select id="gesture-${i}" data-gesture="${i}">${[['level','While held'],['press','Press once'],['tap','Short tap'],['hold','Long hold'],['release','On release']].map(([id,label])=>`<option value="${id}" ${row.gesture===id?'selected':''}>${label}</option>`).join('')}</select><div class="binding-tokens">${inputNames.map(t=>`<button data-token="${t}" data-row="${i}" aria-pressed="${row.inputs.includes(t)}">${pretty(t)}</button>`).join('')}</div>${['tap','hold'].includes(row.gesture)?`<label for="threshold-${i}">Gesture threshold</label><div class="threshold"><input id="threshold-${i}" data-threshold="${i}" type="range" min="100" max="3000" step="50" value="${row.milliseconds}"><output>${row.milliseconds} ms</output></div>`:''}</div>`).join('')||'<p>This action is unbound. Add an alternative to assign it.</p>';
 $('axis-rows').innerHTML=state.bindings.axes.map(a=>`<label>${escape(a.name.replace('axes.','').replaceAll('_',' '))}<select data-axis="${a.name}">${['left_stick','right_stick','disabled'].map(t=>`<option value="${t}" ${currentValue(a.name)===t?'selected':''}>${pretty(t)}</option>`).join('')}</select></label>`).join('');
 const binding=rows[editAlternative];player?.setInputs(binding?.inputs||[]);player?.render();$('binding-label').textContent=bindingLabel(binding);$('cue-status').textContent=binding?'Click controls to add or remove them from this option.':'Unbound · no controller input will trigger this action.';paintDirty();
}
function updateBinding(index,fn){const rows=parseBinding(currentValue(selectedAction));editAlternative=index;fn(rows[index],rows);mark(selectedAction,serializeBinding(rows));paintBinding()}
function paintTapes(){$('tape-total').textContent=catalog.lessons.length;$('tape-list').innerHTML=catalog.lessons.map((l,i)=>`<button data-lesson="${l.id}" aria-selected="${l.id===selectedLesson?.id}"><small>SIDE A / ${String(i+1).padStart(2,'0')}</small>${escape(l.title)}</button>`).join('')}
async function openLesson(id,reload=true){
 const generation=++lessonGeneration;if(reload&&!changed())await refresh(true);if(generation!==lessonGeneration)return;const lesson=catalog.lessons.find(l=>l.id===id);if(!lesson)throw Error('There is no recording for this lesson yet.');selectedLesson=lesson;alternative=0;video.pause();await selectTab('tapes');paintTapes();
 $('lesson-name').textContent=lesson.title;$('lesson-number').textContent=String(catalog.lessons.indexOf(lesson)+1).padStart(2,'0');$('lesson-description').textContent=lesson.description;
 $('lesson-evidence').innerHTML=`<p>Recorded from the ${escape(lesson.source_eye)} eye. The controller diagram uses your saved bindings. Changing them does not alter the recording.</p><p>Game check: ${escape(lesson.native_status.replaceAll('_',' '))}. Visual review: ${escape(lesson.visual_status.replaceAll('_',' '))}. ${escape(lesson.visual_finding||'This feature still needs a full headset test.')}</p><p>Recorded DLL: <code>${escape(lesson.dll_sha256)}</code><br>Video SHA-256: <code>${escape(lesson.source_sha256)}</code></p>`;
 const crop=lesson.crop;$('video-frame').style.aspectRatio=`${crop.width}/${crop.height}`;video.style.width=`${100*lesson.source_width/crop.width}%`;video.style.height=`${100*lesson.source_height/crop.height}%`;video.style.left=`${-100*crop.x/crop.width}%`;video.style.top=`${-100*crop.y/crop.height}%`;sizeVideo();
 video.src=lesson.media;await new Promise((resolve,reject)=>{const done=()=>{video.removeEventListener('error',fail);resolve()},fail=()=>{video.removeEventListener('loadedmetadata',done);reject(Error('The recorded gameplay file could not load.'))};video.addEventListener('loadedmetadata',done,{once:true});video.addEventListener('error',fail,{once:true});video.load()});if(generation!==lessonGeneration)return;video.currentTime=lesson.start;video.playbackRate=Number($('speed').value);$('play').textContent='Play';
 const action=state?.bindings?.actions.find(a=>a.name===lesson.action);$('lesson-alternatives').innerHTML=(action?.bindings||[]).length>1?action.bindings.map((b,i)=>`<button data-lesson-option="${i}">Option ${i+1}</button>`).join(''):'';cue();
}
function sizeVideo(){if(!selectedLesson)return;const ratio=selectedLesson.crop.width/selectedLesson.crop.height;$('video-frame').style.width=Math.min($('lesson-view').clientWidth,Math.max(240,Math.min(440,innerHeight-480))*ratio)+'px';}
function cue(){if(!selectedLesson||!state?.bindings||tab!=='tapes')return;const t=Math.max(0,video.currentTime-selectedLesson.start),resolved=resolveLesson(selectedLesson,state.bindings.actions,t,alternative);player?.setInputs(resolved.inputs);if(autoView)player?.autoPresent(resolved.binding?.inputs||[]);player?.render();$('binding-label').textContent=resolved.label;$('mapping-status').textContent='SAVED BINDING';$('cue-status').textContent=resolved.unbound?'This action is unbound. Assign a control to try it.':resolved.cue?'The highlighted controls perform this action.':'Drag to rotate the controllers; scroll to zoom.';$('seek').value=String(Math.min(1,t/selectedLesson.duration));$('clock').textContent=`${Math.min(t,selectedLesson.duration).toFixed(1)} / ${selectedLesson.duration.toFixed(1)} s`;if(video.currentTime>=selectedLesson.end){video.pause();$('play').textContent='Play'}}
function loop(){if(tab==='tapes')cue();if(tab==='tour'&&tourPlaying)renderTour((performance.now()-tourAnchor)/1000);if(tab==='settings')fit?.tick(performance.now());requestAnimationFrame(loop)}
const modeInfo={gameplay:['On foot','Walking, turning, crouching, weapons and melee.'],menus:['iDroid & menus','Snake stays put while the iDroid is open. Use the sticks to navigate menus and move the map. You can still move your head and hands.'],equipment:['Equipment picker','Hold the equipment button to choose weapons and items.'],commands:['Buddy commands','Hold the command button to open your buddy’s orders.'],binoculars:['Binoculars','Raise the binoculars, zoom, mark targets and put them away.'],horse:['Horse','Controls for riding D-Horse.'],vehicle:['Vehicles','Controls for driving and riding in vehicles.'],nativeButtons:['Native controls','Standard gamepad buttons passed through to the game.']};
const modeAxes={gameplay:['move','turn'],menus:['menu','map'],equipment:['equipment'],commands:['commands'],binoculars:['move','turn'],horse:['move','turn'],vehicle:['vehicle_steering','turn'],nativeButtons:['native_move','native_look']};
function paintModes(){
 if(!state?.bindings)return;const contexts=[...new Set(state.bindings.actions.flatMap(a=>a.contexts))];
 $('mode-cards').innerHTML=contexts.map((c,i)=>`<button data-mode="${c}" aria-selected="${c===mode}"><small>MODE ${String(i+1).padStart(2,'0')}</small><b>${escape(modeInfo[c]?.[0]||c)}</b><span>${state.bindings.actions.filter(a=>a.contexts.includes(c)).length} actions</span></button>`).join('');
 $('mode-title').textContent=modeInfo[mode]?.[0]||mode;$('mode-description').textContent=modeInfo[mode]?.[1]||'Select an action to see its controls.';
 $('picked-control').hidden=!pickedToken;$('picked-control').innerHTML=pickedToken?`<span>CONTROL / ${pretty(pickedToken)}</span><button id="clear-picked">Show all actions ×</button>`:'';
 const usesToken=t=>t===pickedToken||pickedToken?.endsWith('_stick_click')&&t.startsWith(pickedToken.replace('_click','_'));
 const actions=state.bindings.actions.filter(a=>a.contexts.includes(mode)&&(!pickedToken||a.bindings.some(b=>b.inputs.some(usesToken))));
 $('mode-actions').innerHTML=actions.map(a=>`<article class="mode-action"><button data-inspect="${a.name}"><b>${escape(a.name.replace(/^[^.]+\./,'').replaceAll('_',' '))}</b><span>${escape(a.bindings.map(bindingLabel).join(' OR ')||'UNBOUND')}</span></button><div class="mode-action-detail" id="detail-${a.name.replace('.','-')}" hidden><p>${escape(a.modifier?'Hold this button to open the controls for this mode.':a.name.startsWith('system.')?'This action works in the modes listed below the controller.':'Available in this mode. Select Edit mapping to change the controls.')}</p><button data-edit-action="${a.name}">Edit mapping</button>${catalog.lessons.filter(l=>l.action===a.name).map(l=>`<button data-lesson="${l.id}">Play lesson ↗</button>`).join('')}${a.bindings.length>1?a.bindings.map((b,i)=>`<button data-inspect="${a.name}" data-option="${i}">Option ${i+1}</button>`).join(''):''}</div></article>`).join('')||'<p>No action uses this control in the selected mode.</p>';
 const axes=state.bindings.axes.filter(a=>modeAxes[mode]?.includes(a.name.replace('axes.',''))&&(!pickedToken||pickedToken.startsWith(a.source+'_')));
 $('mode-actions').insertAdjacentHTML('beforeend',axes.map(a=>`<article class="mode-action"><button data-axis-inspect="${a.name}"><b>${escape(a.name.replace('axes.','').replaceAll('_',' '))} · stick axis</b><span>${pretty(a.source)}</span></button></article>`).join(''));
 if(!pickedToken){player.zoomHand=null;player?.setInputs([]);player?.render();$('binding-label').textContent='SELECT A CONTROL';$('cue-status').textContent='Click a 3D button or an action in the list to inspect its mapping.'}
}
function inspectAction(name,index=0){const action=state.bindings.actions.find(a=>a.name===name),binding=action?.bindings[index],inputs=binding?.inputs||[],missing=player.setInputs(inputs);if(missing.length)throw Error('3D location unavailable: '+missing.join(', '));player.focusControl();player.render();$('binding-label').textContent=bindingLabel(binding);$('cue-status').textContent=name.replaceAll('_',' ')+' · '+action.contexts.map(c=>modeInfo[c]?.[0]||c).join(', ');document.querySelectorAll('.mode-action-detail').forEach(e=>e.hidden=e.id!=='detail-'+name.replace('.','-'));return {name,inputs,missing};}
const settingCopy={
 snap_turn_degrees:['Snap turn angle','How far each snap turn rotates you.'],
 turn_mode:['Stick turning','Choose snap turns, smooth turning or no stick turning.'],
 hud_mode:['HUD visibility','Show the full HUD, show it only with binoculars, or hide it.'],
 motion_melee:['Physical melee','Use a free-hand punch or a weapon bash. Pull your hand back before repeating.'],
 animal_touch:['Animal interactions','Enable open-palm dog petting and rat pickup gestures.'],
 wrist_surface_lift_cm:['Wrist display clearance','Raise the displays away from the surface of your arm.'],
 wrist_selector_height_cm:['Equipment menu height','Height of the equipment picker above your wrist.'],
 weapon_hud_setback_cm:['Weapon info position','Move the weapon and ammo readout toward your elbow. This does not move the equipment popup.'],
 wrist_picker_width_cm:['Equipment menu width','Make the equipment cards and text larger or smaller.'],
 wrist_text_scale:['Arm text size','Enlarge arm captions, prompts and the ammo readout. 1.5 is 50% larger; equipment card size is separate.'],
 handheld_menus:['Handheld iDroid','Hold the iDroid in your palm. Snake stays still while you use it. Pause always opens as a large panel in front of you.'],
 idroid_screen_width_cm:['iDroid screen width','Size of the projected screen.'],
 idroid_screen_depth_cm:['iDroid projection distance','Distance between the device in your palm and its projected screen.'],
 idroid_screen_x_cm:['iDroid horizontal position','Move the screen left or right. Positive values move it right.'],
 idroid_screen_y_cm:['iDroid vertical position','Move the screen down or up. Positive values move it up.'],
 idroid_screen_pitch_degrees:['iDroid screen tilt','Tilt the screen up or down without moving your hand.'],
 idroid_screen_yaw_degrees:['iDroid screen angle','Turn the screen left or right without moving your hand.'],
 idroid_screen_roll_degrees:['iDroid screen roll','Rotate the screen within its frame to level it with your hand.'],
 idroid_grip_x_cm:['iDroid grip: left / right','Move the hand and device together in the controller grip. Positive moves right.'],
 idroid_grip_y_cm:['iDroid grip: up / down','Move the hand and device together. Positive moves up.'],
 idroid_grip_z_cm:['iDroid grip: forward / back','Move the hand and device together. Positive moves toward you.'],
 idroid_grip_pitch_degrees:['iDroid grip: tilt','Tilt the complete hand, device and projection to match your natural hold.'],
 idroid_grip_yaw_degrees:['iDroid grip: angle','Turn the complete hand, device and projection left or right.'],
 idroid_grip_roll_degrees:['iDroid grip: roll','Rotate the complete hand, device and projection around your grip.'],
 menu_quad_width_cm:['World menu width','Width of Pause, and of iDroid when handheld mode is off.'],
 menu_quad_distance_cm:['World menu distance','Distance to Pause, and to iDroid when handheld mode is off.'],
 menu_quad_tilt_degrees:['World menu tilt','Negative values tilt the menu panel upward.'],
 scope_eye_relief_cm:['Scope eye distance','Preferred distance from your eye to the scope glass.'],
 binocular_pitch_degrees:['Binocular tilt','Tilt the binoculars in your hand. The lenses and grip move together.'],
 binocular_yaw_degrees:['Binocular angle','Turn the binoculars left or right in your grip.'],
 binocular_roll_degrees:['Binocular roll','Rotate the binoculars to level them in your grip.'],
 binocular_max_eye_distance_cm:['Binocular viewing distance','Maximum distance from your eye at which the binocular view opens.'],
 binocular_auto_mark:['Automatic binocular marking','Keep the binocular sight on a visible person to mark them.'],
 binocular_actor_glow:['Binocular target glow','Highlight targets with a glow. Turning this off keeps their labels.'],
 binocular_mark_dwell_ms:['Time before automatic marking','How long to keep a person in your sights before marking them.'],
 player_height_offset_cm:['Eye height adjustment','Raise or lower your viewpoint without changing the size of the world.'],
 weapon_smoothing_ms:['Weapon and scope stabilization','0 is off; 100 strongly steadies small shakes. Gun, sight and hands move together; deliberate aim changes follow quickly.'],
 support_grip_radius_cm:['Support grip distance','How close your free hand must be to take hold of the weapon.'],
 support_detach_radius_cm:['Support grip release distance','How far you can pull your support hand away before it lets go.'],
 hand_rest_curl_percent:['Relaxed finger curl','How much your fingers curl when they are not holding anything.'],
 hand_touch_curl_percent:['Finger curl when touching controls','Finger curl when your fingers rest on the controller’s touch sensors.']
};
for(const side of ['left','right']) {
 const hand=side[0].toUpperCase()+side.slice(1)+' hand';
 for(const [axis,label,description] of [['x','left / right','Positive values move the hand right.'],['y','up / down','Positive values move the hand up.'],['z','forward / back','Positive values move the hand toward you.']]) settingCopy[side+'_hand_'+axis+'_cm']=[hand+': '+label,description+' Held objects and aim move with it.'];
 for(const [axis,label,description] of [['pitch','tilt','Tilt the hand up or down.'],['yaw','angle','Turn the hand left or right.'],['roll','roll','Rotate the hand around the grip.']]) settingCopy[side+'_hand_'+axis+'_degrees']=[hand+': '+label,description+' Held objects and aim rotate with it.'];
}
function settingGroup(name){if(/idroid|handheld|menu_quad/.test(name))return 'iDroid & menus';if(/wrist|weapon_hud|hud_mode/.test(name))return 'Wrist & HUD';if(/binocular|scope/.test(name))return 'Optics';if(/hand_|support_|smoothing/.test(name))return 'Hands & weapons';return 'Movement & interaction'}
function runtimeRows(){const ranges={'theatre.width_cm':[100,3000],'theatre.distance_cm':[100,3000]};return Object.keys(state.runtime).map(name=>{const bool=/enabled$|native_actions$|camera_observer$|experiment$|interactive_cabin$/.test(name);const [min,max]=ranges[name]|| (bool?[0,1]:[undefined,undefined]);return {name,value:state.runtime[name],min,max,default:catalog.runtimeDefaults?.[name]??state.runtime[name]}})}
function paintSettings(){
 if(!state?.settings){$('settings-grid').innerHTML='<p>Choose your game installation to edit its settings.</p>';return;}
 $('fit-layout').hidden=settingsKind==='display';$('display-settings').hidden=settingsKind!=='display';$('settings-help').textContent=settingsKind==='runtime'?'Runtime and asset changes apply after restarting the game.':settingsKind==='display'?'Choose a render size for the next game launch.':'Preview changes immediately. Save, then release all buttons and center both sticks for two seconds to apply in game.';
 if(!fit&&settingsKind!=='display')try{fit=new FitPreview($('fit-canvas'));}catch(e){$('fit-readout').textContent='3D preview could not start. Settings can still be edited.';console.error(e);}
 const query=$('setting-search').value.toLowerCase(),rows=(settingsKind==='runtime'?runtimeRows():state.settings).filter(r=>(fitFilter==='all'||fitContext(r.name)===fit.context)&&(r.name+' '+(settingCopy[r.name.split('.')[1]]||[]).join(' ')).toLowerCase().includes(query));let lastGroup='';
 $('settings-grid').innerHTML=rows.sort((a,b)=>settingGroup(a.name).localeCompare(settingGroup(b.name))).map(row=>{const name=row.name,key=name.split('.')[1],id=name.replace('.','-'),unit=name.endsWith('_cm')?'cm':name.endsWith('_degrees')?'°':name.endsWith('_ms')?'ms':name.endsWith('_percent')?'%':'';const label=settingCopy[key]?.[0]||key.replace(/_(cm|degrees|ms|percent)$/,'').replaceAll('_',' ');const value=currentValue(name,settingsKind),type=settingWidget(row),group=settingsKind==='runtime'?name.split('.')[0]:settingGroup(name);let heading='';if(group!==lastGroup){heading=`<h2 class="setting-group">${escape(group.toUpperCase())}</h2>`;lastGroup=group}
 const attrs=`id="${id}" data-setting="${name}"`,input=type==='toggle'?`<input ${attrs} type="checkbox" ${Number(value)?'checked':''}>`:type==='select'?`<select ${attrs}>${settingOptions[name].map(([v,t])=>`<option value="${v}" ${value===v?'selected':''}>${t}</option>`).join('')}</select>`:type==='range'?`<div class="setting-slider"><input ${attrs} type="range" min="${row.min}" max="${row.max}" step="${unit==='cm'?.1:key==='wrist_text_scale'?.05:1}" value="${Number(value)}"><input type="number" required data-number="${name}" aria-label="${escape(label)} exact value" min="${row.min}" max="${row.max}" step="${settingsKind==='runtime'?1:'any'}" value="${Number(value)}"><span>${unit}</span></div>`:`<input ${attrs} type="text" value="${escape(value)}" spellcheck="false">`;
 return heading+`<article class="setting-card" data-fit="${fitContext(name)||''}"><div class="setting-title"><label for="${id}">${escape(label[0].toUpperCase()+label.slice(1))}</label>${type==='toggle'?input:''}</div><p class="setting-description">${escape(settingCopy[key]?.[1]||(settingsKind==='runtime'?'Restart the game after changing this.':''))}</p>${type==='toggle'?'':input}<div class="setting-limits"><span>${type==='range'?row.min+' — '+row.max+' '+unit:escape(name)}</span><button class="reset" data-reset="${name}" data-default="${escape(settingOptions[name]?settingOptions[name][row.default]?.[0]:row.default)}">Reset</button></div></article>`;
 }).join('');
 updateFit();
}
let catalogKind='historical_feature',numbers=null,numericLimit=100;async function paintNotes(){const query=$('note-search').value.toLowerCase();$('notes-counts').innerHTML=`<div><b>${catalog.features.length}</b><span>features</span></div><div><b>${catalog.reports.length}</b><span>bug reports</span></div><div><b>${catalog.lessons.length}</b><span>recordings</span></div>`;
 $('report-filter').hidden=catalogKind!=='report';
 if(catalogKind==='report'){await paintCommunity(query,$('report-filter').value);return;}
 if(catalogKind==='numbers'){
  if(!numbers){$('note-list').textContent='Reading values from the code…';numbers=await(await fetch('numeric-audit.json')).json();if(catalogKind!=='numbers')return;}
  const rows=numbers.literals.filter(r=>!query||(r.file+' '+r.literal+' '+r.context+' '+r.category).toLowerCase().includes(query));
  $('note-list').innerHTML=`<p>${numbers.literal_count.toLocaleString()} numbers found in ${numbers.source_files.length} source files. These still need review. The ${state.settings.length} settings you can adjust are under VR settings.</p><p class="help">${rows.length.toLocaleString()} matches · showing ${Math.min(rows.length,numericLimit)} · some numbers are memory addresses or fixed game values.</p>`+rows.slice(0,numericLimit).map(r=>`<details class="note-row"><summary><span>${escape(r.file)}:${r.line} · <b>${escape(r.literal)}</b></span><small>${escape(r.category.replaceAll('_',' '))}</small></summary><pre>${escape(r.context)}</pre><p>The category was assigned automatically. Check how this value is used before making it a setting.</p></details>`).join('')+(rows.length>numericLimit?'<p><button id="more-numbers">Show 100 more</button></p>':'');return;
 }
 $('note-list').innerHTML=(catalogKind==='report'?catalog.reports:catalog.features).filter(r=>(r.title+' '+r.id).toLowerCase().includes(query)).map(r=>`<details class="note-row"><summary><span>${escape(r.id)} / ${escape(r.title)}</span><small>${escape(r.status.replaceAll('_',' '))}</small></summary>${(r.acceptance||[]).map(t=>`<p>${escape(t)}</p>`).join('')}<p>${escape(r.latest_report||'Still needs a recorded test and visual review.')}</p></details>`).join('')}
document.addEventListener('click',e=>act(async()=>{
 const b=e.target.closest('button');if(!b)return;
 if(b.dataset.quickTour!==undefined)await startTour();
 if(b.dataset.tab)await selectTab(b.dataset.tab);
 if(b.dataset.lesson)await openLesson(b.dataset.lesson);
 if(b.dataset.view)player?.setView(b.dataset.view);
 if(b.dataset.mode){mode=b.dataset.mode;pickedToken=null;paintModes()}
 if(b.dataset.inspect)inspectAction(b.dataset.inspect,Number(b.dataset.option||0));
 if(b.dataset.axisInspect){const axis=state.bindings.axes.find(a=>a.name===b.dataset.axisInspect);player.setInputs(axis.source==='disabled'?[]:[axis.source+'_up']);player.focusControl();$('binding-label').textContent=pretty(axis.source);$('cue-status').textContent=axis.name.replace('axes.','').replaceAll('_',' ')+' · analog stick motion in this mode.'}
 if(b.dataset.editAction){selectedAction=b.dataset.editAction;$('action-search').value='';fillActions();await selectTab('controls')}
 if(b.id==='clear-picked'){pickedToken=null;paintModes()}
 if(b.dataset.settings){settingsKind=b.dataset.settings;document.querySelectorAll('[data-settings]').forEach(el=>el.setAttribute('aria-selected',String(el===b)));paintSettings()}
 if(b.dataset.fitContext){fit?.setContext(b.dataset.fitContext);if(fitFilter!=='all'){fitFilter=b.dataset.fitContext;paintSettings()}else updateFit()}
 if(b.dataset.fitView)fit?.setView(b.dataset.fitView);
 if(b.id==='fit-filter'){fitFilter=fitFilter==='all'?fit.context:'all';paintSettings()}
 if(b.id==='fit-reset'){const context=fit.context;for(const row of settingsKind==='runtime'?runtimeRows():state.settings){if(fitContext(row.name)===context&&Number.isFinite(row.min)&&Number.isFinite(row.max)&&!settingOptions[row.name]&&!(row.min===0&&row.max===1))mark(row.name,row.default,settingsKind)}paintSettings()}
 if(b.dataset.catalog){catalogKind=b.dataset.catalog;numericLimit=100;document.querySelectorAll('[data-catalog]').forEach(el=>el.setAttribute('aria-selected',String(el===b)));await paintNotes()}
 if(b.id==='more-numbers'){numericLimit+=100;await paintNotes()}
 if(b.dataset.proof!==undefined)await openCommunityProof(Number(b.dataset.proof));
 if(b.id==='close-proof')$('proof-dialog').close();
 if(b.dataset.token)updateBinding(Number(b.dataset.row),(r)=>{r.inputs=r.inputs.includes(b.dataset.token)?r.inputs.filter(t=>t!==b.dataset.token):[...r.inputs,b.dataset.token];if(!r.inputs.length)throw Error('Keep at least one input, or use Unbind action.');});
 if(b.dataset.remove!==undefined)updateBinding(Number(b.dataset.remove),(r,rows)=>rows.splice(Number(b.dataset.remove),1));
 if(b.dataset.reset){mark(b.dataset.reset,b.dataset.default,settingsKind);paintSettings()}
 if(b.dataset.lessonOption!==undefined){alternative=Number(b.dataset.lessonOption);cue()}
}));
document.addEventListener('change',e=>act(async()=>{const el=e.target;if(el.dataset.gesture!==undefined)updateBinding(Number(el.dataset.gesture),r=>r.gesture=el.value);if(el.dataset.axis)mark(el.dataset.axis,el.value);if(el.dataset.setting&&el.type!=='range')mark(el.dataset.setting,el.type==='checkbox'?(el.checked?'1':'0'):el.value,settingsKind);if(el.dataset.number){if(!el.checkValidity())throw Error('Use a value within this setting’s range.');mark(el.dataset.number,el.value,settingsKind);document.querySelector(`[data-setting="${el.dataset.number}"]`).value=el.value}}));
document.addEventListener('input',e=>{const el=e.target;try{if(el.dataset.setting&&el.type==='range'){mark(el.dataset.setting,el.value,settingsKind);document.querySelector(`[data-number="${el.dataset.setting}"]`).value=el.value}if(el.dataset.number&&el.checkValidity()){mark(el.dataset.number,el.value,settingsKind);document.querySelector(`[data-setting="${el.dataset.number}"]`).value=el.value}if(el.dataset.threshold!==undefined){const rows=parseBinding(currentValue(selectedAction));rows[Number(el.dataset.threshold)].milliseconds=Number(el.value);mark(selectedAction,serializeBinding(rows));el.nextElementSibling.textContent=el.value+' ms';$('binding-expression').value=serializeBinding(rows);$('binding-label').textContent=bindingLabel(rows[Number(el.dataset.threshold)])}}catch(err){status(err.message,true)}});
document.addEventListener('focusin',e=>{const name=e.target.dataset.setting||e.target.dataset.number;if(name)updateFit(name)});
$('fit-touch').onchange=()=>{if(fit){fit.touch=$('fit-touch').checked;updateFit()}};
$('fit-motion').onchange=()=>{if(fit){fit.motion=$('fit-motion').checked;fit.weapon.rotation.y=fit.scope.rotation.y=0;updateFit()}};
$('fit-arm-display').onchange=()=>{if(fit){fit.armDisplay=$('fit-arm-display').value;updateFit()}};
const click=(id,fn)=>$(id).onclick=()=>act(fn);
click('tour-play',async()=>{if(tourPlaying){seekTour(tourTime);}else{if(tourTime>=tourDuration())tourTime=0;tourAnchor=performance.now()-tourTime*1000;tourPlaying=true;renderTour();}});
click('tour-previous',async()=>{const step=tourAt(tourSteps,tourTime),index=tourSteps.indexOf(step);seekTour(tourSteps[Math.max(0,index-1)].start);});
click('tour-next',async()=>{const step=tourAt(tourSteps,tourTime),index=tourSteps.indexOf(step);seekTour(tourSteps[Math.min(tourSteps.length-1,index+1)].start);});
$('tour-seek').oninput=()=>seekTour($('tour-seek').value);
click('choose-game',async()=>{if(changed())throw Error('Save or discard your edits before choosing another installation.');state=await rpc('chooseGame');paintInstallation();fillActions();status('Game installation selected.');});
click('launch',async()=>{if(changed())throw Error('Save or discard your edits before launching.');$('launch').disabled=true;status('Applying the selected resolution and starting VR…');try{await rpc('launch',{runtime:$('runtime').value,...displaySelection()});localStorage.setItem('display:'+state.gameExe,JSON.stringify(displaySelection()));await refresh(true);status('VR launched. Continue in your headset.')}finally{paintInstallation()}});
click('install',async()=>{if(changed())throw Error('Save or discard your edits before updating.');$('install').disabled=true;status('Preparing VR and importing your owned game assets. This can take several minutes.');try{await rpc('install');await refresh(true);status('Installation finished. Your settings are preserved.')}finally{paintInstallation()}});
click('refresh-bindings',async()=>{if(changed())throw Error('Save or discard your edits before reloading.');await refresh()});
click('close-up',async()=>player?.focusControl(true));
click('maintenance',async()=>{await rpc('maintenance');status('Maintenance tools opened.');});
click('auto-view',async()=>{autoView=true;player.manualUntil=0;player.autoSince=performance.now();$('auto-view').setAttribute('aria-pressed','true');cue();status('Auto view is on.');});
click('add-alternative',async()=>{const rows=parseBinding(currentValue(selectedAction));rows.push({gesture:'press',milliseconds:300,inputs:['a']});editAlternative=rows.length-1;mark(selectedAction,serializeBinding(rows));paintBinding()});
click('disable-action',async()=>{mark(selectedAction,'disabled');paintBinding()});
click('default-action',async()=>{mark(selectedAction,catalog.defaultValues[selectedAction]);paintBinding()});
click('apply-expression',async()=>{parseBinding($('binding-expression').value);mark(selectedAction,$('binding-expression').value);paintBinding()});
click('remap-lesson',async()=>{selectedAction=selectedLesson.action;fillActions();await selectTab('controls')});
click('discard',async()=>{draft={};paintDirty();await refresh();});
click('save',async()=>{if(!changed())return;$('save').disabled=true;status('Checking the complete configuration…');try{state=await rpc(draftKind==='runtime'?'saveRuntime':'saveControls',{revision:draftKind==='runtime'?state.runtimeRevision:state.controlsRevision,changes:draft});const kind=draftKind;draft={};paintDirty();fillActions();if(tab==='controls')paintBinding();if(tab==='settings')paintSettings();cue();status(kind==='runtime'?'Saved with a backup. Restart the game to apply runtime changes.':'Saved with a backup. Lessons use the new bindings. Release all controls and center both sticks for two seconds to apply in game.')}finally{$('save').disabled=false}});
click('play',async()=>{if(!selectedLesson)return;if(video.paused){if(!changed())await refresh(true);if(video.currentTime>=selectedLesson.end-.03)video.currentTime=selectedLesson.start;await video.play();$('play').textContent='Pause'}else{video.pause();$('play').textContent='Play'}});
click('replay',async()=>{if(!selectedLesson)return;if(!changed())await refresh(true);video.currentTime=selectedLesson.start;await video.play();$('play').textContent='Pause'});
click('apply-display',async()=>{const width=$('display-width'),height=$('display-height');if(!width.checkValidity()||!height.checkValidity())throw Error('Choose a valid render size.');status('Applying render settings…');await rpc('display',{runtime:$('runtime').value,...displaySelection()});localStorage.setItem('display:'+state.gameExe,JSON.stringify(displaySelection()));await refresh(true);status('Render settings saved for the next launch.');});
click('refresh-display',async()=>{await refresh(true);status('Actual game render size refreshed.');});
$('action-select').onchange=()=>{selectedAction=$('action-select').value;editAlternative=0;paintBinding()};$('action-search').oninput=()=>{fillActions();paintBinding()};$('setting-search').oninput=paintSettings;$('note-search').oninput=()=>{numericLimit=100;act(paintNotes)};
 $('report-filter').onchange=()=>act(paintNotes);
$('seek').oninput=()=>{if(selectedLesson){video.currentTime=selectedLesson.start+Number($('seek').value)*selectedLesson.duration;cue()}};$('speed').onchange=()=>video.playbackRate=Number($('speed').value);$('display-scale').oninput=()=>$('scale-value').textContent=$('display-scale').value+'%';
function displayMode(){const preset=$('display-preset').value;$('display-scale').disabled=preset!=='Headset';$('display-width').disabled=$('display-height').disabled=preset!=='Custom'}$('display-preset').onchange=displayMode;displayMode();
window.addEventListener('resize',()=>{player?.render();deck?.render();fit?.render();sizeVideo()});window.addEventListener('beforeunload',e=>{if(changed()){e.preventDefault();e.returnValue='Unsaved settings';}});
window.addEventListener('focus',()=>{if(ready&&!changed()&&!loading&&!pending.size)act(()=>refresh(true))});
window.launcherQA={get ready(){return ready},snapshot:()=>({ready,tab,controllers:!!player?.ready,lessons:catalog?.lessons.length,actions:state?.bindings?.actions.length,settings:state?.settings?.length,game:state?.gameExe,overlays:document.querySelectorAll('.video-frame canvas,.detail-inset').length,dirty:Object.keys(draft),selectedLesson:selectedLesson?.id,inputs:player?.active,camera:player?{azimuth:player.azimuth,pitch:player.pitch,target:player.autoTarget}:null}),controlPoint:token=>player.inputScreenPoint(token),inspectAction,openLesson,selectTab,refresh,seek:async t=>{video.pause();const target=selectedLesson.start+t;if(Math.abs(video.currentTime-target)>.00001||video.seeking){const done=new Promise(r=>video.addEventListener('seeked',r,{once:true}));video.currentTime=target;await done;}cue();return {inputs:player.active,media:video.getAttribute('src'),label:$('binding-label').textContent}},get state(){return state},get lesson(){return selectedLesson}};
Object.assign(window.launcherQA,{startTour,seekTour,getTour:()=>tourSteps,fitSnapshot:()=>fit?.snapshot(),fitContext,
 inspectInputs:inputs=>({missing:player.setInputs(inputs),changedTransforms:player.restTransforms.filter(r=>r.node.position.distanceTo(r.position)>1e-8||!r.node.quaternion.equals(r.quaternion)).map(r=>r.node.name)}),
 pickInput:token=>player.onPick(token)});
await act(async()=>{catalog=await (await fetch('catalog.json')).json();paintTapes();deck=new CassetteDeck($('cassette-canvas'),catalog.lessons,id=>act(()=>openLesson(id)));await refresh(true);if(!state.needsGame){selectedAction='gameplay.reload';fillActions()}ready=true;status(state.needsGame?'Choose your game folder to load its settings.':'Settings loaded.');if(window.__launcherStartPage==='tour')await startTour();else if(['tapes','modes','controls','settings','notes'].includes(window.__launcherStartPage))await selectTab(window.__launcherStartPage);loop()});
