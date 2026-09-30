export const inputNames=['a','b','x','y','menu','left_grip','right_grip','left_trigger','right_trigger','left_stick_click','right_stick_click','left_thumbrest','right_thumbrest','a_touch','b_touch','x_touch','y_touch','left_stick_touch','right_stick_touch','left_trigger_touch','right_trigger_touch',...['left','right'].flatMap(h=>['up','down','left','right'].map(d=>`${h}_stick_${d}`))];
export const pretty=s=>s==='menu'?'L MENU':s.replace(/_stick_(up|down|left|right)$/,(_,d)=>'_STICK '+({up:'↑',down:'↓',left:'←',right:'→'}[d])).replaceAll('_',' ').replace(/\bleft\b/g,'L').replace(/\bright\b/g,'R').toUpperCase();
export function parseBinding(raw){
 if(raw.trim()==='disabled')return [];
 return raw.split('|').map(part=>{part=part.trim();const match=part.match(/^(press|release|tap|hold)\((.*)\)$/);const body=match?match[2]:part;const [chord,ms]=body.split(',');const inputs=chord.split('+').map(s=>s.trim());
  if(inputs.some(t=>!inputNames.includes(t)))throw Error('Choose a supported controller input.');
  if(new Set(inputs).size!==inputs.length)throw Error('An input cannot appear twice in a chord.');
  return {gesture:match?match[1]:'level',milliseconds:ms===undefined?300:Number(ms),inputs};});
}
export function serializeBinding(bindings){return bindings.length?bindings.map(b=>b.gesture==='level'?b.inputs.join(' + '):`${b.gesture}(${b.inputs.join(' + ')}${['hold','tap'].includes(b.gesture)?','+b.milliseconds:''})`).join(' | '):'disabled'}
export function bindingLabel(binding){if(!binding)return 'UNBOUND';const gesture={level:'',press:'PRESS',release:'RELEASE',hold:'HOLD',tap:'TAP'}[binding.gesture]??binding.gesture.toUpperCase();return `${gesture} ${binding.inputs.map(pretty).join(' + ')}${['hold','tap'].includes(binding.gesture)?` · ${binding.gesture==='tap'?'<':'≥'} ${binding.milliseconds} ms`:''}`.trim()}
// Cues store actions and times only. Physical buttons always come from this load
// of the actual effective bindings, never from the video's recording manifest.
export function resolveLesson(lesson,actions,time,alternative=0){
 const cue=lesson.cues.find(c=>c.start<=time&&time<c.end);
 const action=actions.find(a=>a.name===(cue?.action||lesson.action));
 const binding=action?.bindings[alternative]||action?.bindings[0];
 return {action:action?.name||lesson.action,binding,label:bindingLabel(binding),inputs:cue&&binding?binding.inputs:[],cue:!!cue,unbound:!binding};
}
export const isTouchInput=token=>token.endsWith('_touch')||token.endsWith('_thumbrest');
export function touchForControl(token){
 if(isTouchInput(token))return token;
 if(['a','b','x','y'].includes(token)||token.endsWith('_trigger'))return token+'_touch';
 if(token.endsWith('_stick_click'))return token.replace('_click','_touch');
 return null;
}
export function bindingHands(inputs){
 const hands=new Set(inputs.map(t=>/^(x|y)(?:_touch)?$|^left_|^menu$/.test(t)?'left':'right'));
 return hands.size===2?'Both hands':hands.has('left')?'Left hand':hands.has('right')?'Right hand':'Unbound';
}
export const settingOptions={
 'settings.turn_mode':[['snap','Snap turn'],['native_smooth','Smooth turn'],['off','Turning off']],
 'settings.hud_mode':[['full','Full HUD'],['binoculars_only','Binoculars only'],['off','HUD off']]
};
export function settingWidget(row){
 if(settingOptions[row.name])return 'select';
 if(row.min===0&&row.max===1)return 'toggle';
 if(Number.isFinite(row.min)&&Number.isFinite(row.max))return 'range';
 return 'text';
}
