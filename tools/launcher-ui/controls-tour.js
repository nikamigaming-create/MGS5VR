import {bindingLabel,bindingHands} from './binding-model.js';

// Resolve on playback so personal remaps and disabled actions stay truthful.
const actions=[
 ['gameplay.ready_weapon','Ready the weapon','Hold the physical gun. Touching the right trigger can be an optional alternative to squeezing the grip.'],
 ['gameplay.support_grip','Support the weapon','Bring your left hand to the weapon’s support grip.'],
 ['gameplay.reload','Reload','Use the complete chord together. Release it before repeating.'],
 ['gameplay.switch_weapon','Switch weapon','Use the saved chord; the guide shows which hand it needs.'],
 ['equipment.open','Wrist equipment picker','Hold to open the left-arm picker; release to close.'],
 ['gameplay.equip_binoculars','Equip binoculars','Keep them physical: raise them, aim with your hand and look through the lenses.'],
 ['binoculars.zoom','Binocular zoom','This binding applies while holding binoculars.'],
 ['binoculars.mark','Mark a target','Aim at a visible target, then explicitly mark it.'],
 ['system.idroid','Open iDroid','The iDroid stays in your right hand.'],
 ['system.pause','Pause','Pause opens on the spatial panel in front of you.']
];
export function controlsTour(bindings){
 const steps=actions.flatMap(([name,title,description])=>{
  const action=bindings.actions.find(a=>a.name===name);
  return (action?.bindings?.length?action.bindings:[undefined]).map((binding,index)=>({name,alternative:index,title:index?title+' · alternative '+(index+1):title,description,inputs:binding?.inputs||[],label:bindingLabel(binding),
   hands:bindingHands(binding?.inputs||[]),context:action?.contexts?.join(' · ')||'Unbound',
   seconds:Math.max((binding?.inputs?.length||0)>1?3.2:2.4,binding?.gesture==='hold'?binding.milliseconds/1000+.8:0),kind:'mapping'}));
 });
 for(const [name,title] of [['axes.move','Move'],['axes.turn','Turn']]){
  const axis=bindings.axes.find(a=>a.name===name);
  const inputs=axis&&axis.source!=='disabled'?[axis.source+'_up']:[];
  steps.push({name,title,description:'The highlighted stick follows your saved stick assignment.',
   inputs,label:axis?.source?.replaceAll('_',' ').toUpperCase()||'UNBOUND',
   hands:bindingHands(inputs),context:'Gameplay',seconds:2.4,kind:'mapping'});
 }
 for(const [title,inputs,description] of [
  ['Four face-button touches',['a_touch','b_touch','x_touch','y_touch'],'A, B, X and Y each have separate light-touch inputs. Contact does not press the button.'],
  ['Two thumbstick touches',['left_stick_touch','right_stick_touch'],'Rest your thumb on a stick without clicking it or moving it. Useful for a deliberate modifier.'],
  ['Two trigger touches',['left_trigger_touch','right_trigger_touch'],'Rest a finger on the trigger without squeezing. Try right-trigger touch as an alternative weapon-ready binding.'],
  ['Two thumb-rest touches',['left_thumbrest','right_thumbrest'],'The resting pads are separate inputs too. A thumb-rest contact can hold open the wrist picker.']
 ])steps.push({name:'touch',title,description,inputs,label:'LIGHT TOUCH · NO CLICK',
   hands:'Both controllers',context:'Available mapping inputs',seconds:3.2,kind:'touch'});
 let start=0;
 return steps.map(step=>{const result={...step,start};start+=step.seconds;return result});
}
export function tourAt(steps,time){
 return steps.find(step=>step.start<=time&&time<step.start+step.seconds)||steps.at(-1);
}
