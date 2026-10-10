import {bindingLabel,pretty} from './binding-model.js';
import {ControllerPlayer} from './controller-player.js';

// One renderer serves the launcher and the offline PNG exporter. Every physical
// binding is resolved from the native checker's effective action catalog.
export const guideSize={width:1240,height:1754,scale:2};
export function controlGuidePages(bindings){
 const action=name=>bindings.actions.find(a=>a.name===name);
 const label=name=>(action(name)?.bindings||[]).map(bindingLabel).join('  OR  ')||'UNBOUND';
 const axis=name=>pretty(bindings.axes.find(a=>a.name==='axes.'+name)?.source||'disabled');
 const row=(title,name,note='')=>({title,binding:label(name),note,actions:[name]});
 const axisRow=(title,name,note='')=>({title,binding:axis(name),note,axes:['axes.'+name]});
 const note=(title,binding,detail)=>({title,binding,note:detail});
 const combo=(title,first,second,detail)=>({title,binding:label(first)+'  +  '+label(second),note:detail,actions:[first,second]});
 const modernVehicle=!!action('vehicle.ready_weapon')?.bindings?.length
     &&!!action('vehicle.fire')?.bindings?.length
     &&bindings.axes.some(a=>a.name==='axes.vehicle_pedals');
 const twoHandWheel=!!action('vehicle.right_wheel_grip')?.bindings?.length;
 const pages=[
  {id:'01-move-and-fight',title:'Move & fight',subtitle:'Physical hands. One control owner at a time.',illustration:'controllers',
   callout:['START WITH RELEASED CONTROLS','After changing mode, release buttons and center the sticks before the next action.'],
   columns:[[
    {title:'MOVE',rows:[axisRow('Move','move','Forward follows your head direction.'),axisRow('Turn','turn','Horizontal turn; vertical weapon gestures stay separate.'),row('Sprint','gameplay.run'),row('Crouch / stand / prone','gameplay.stance','Tap to crouch or stand. Hold to go prone.'),row('Dive','gameplay.dive'),row('Interact / climb / mount','gameplay.interact','Follow the native prompt; hold if the prompt requires it.')]},
    {title:'SYSTEM',rows:[row('Open / stow iDroid','system.idroid','Device and ordinary HUD stay on the LEFT.'),row('Pause / iDroid Help','system.pause'),row('Recenter','system.recenter','Keeps your current facing.')]}
   ],[
    {title:'WEAPONS',rows:[row('Ready the physical gun','gameplay.ready_weapon','Hold while aiming down its actual sights.'),row('Fire / throw / CQC','gameplay.fire_or_cqc','The equipped item and ready state decide the action.'),row('Support with the left hand','gameplay.support_grip','Squeeze near the gun’s support socket.'),row('Reload','gameplay.reload','Use the full chord; it consumes bare pickup.'),row('Quick-switch weapon','gameplay.switch_weapon','Full chord; its stick click does not also dive.'),row('Pickup / carry','gameplay.pickup_carry','Hold when the native carry prompt requests it.')]},
    {title:'PHYSICAL INTERACTION',rows:[note('Aim','MOVE THE GUN','Align your eye with the fitted iron sight, reflex sight or scope.'),note('Melee','PUNCH / WEAPON BASH','With physical melee enabled, make a deliberate motion and retract before repeating.'),note('Wrist status','LOOK AT LEFT FOREARM','Ammo, weapon status and ordinary HUD stay here.')]}
   ]]},
  {id:'02-idroid-and-menus',title:'iDroid & menus',subtitle:'The LEFT bionic hand carries the iDroid.',illustration:'idroid',
   callout:['HOLD TO FOCUS · RELEASE TO MOVE','While focused, both sticks and face buttons operate the screen. Ordinary movement and weapon chords are dormant.'],
   columns:[[
    {title:'OPEN, FOCUS, STOW',rows:[row('Open / stow','system.idroid'),row('Focus an open iDroid','system.idroid_focus','Hold while operating it. An unfocused screen ignores menu zoom and navigation.'),row('Next page','menus.next_tab','While focused: one page per fresh press. Release before repeating. Root tabs wrap to the first.'),...(action('idroid.previous_tab')?[row('Previous iDroid page','idroid.previous_tab','Its shared menu inputs stay consumed until released.')]:[]),row('Back / cancel','menus.back','Back leaves a page. Hold for native close where required.'),row('Help / Pause','system.pause','Pause uses the spatial stereo panel.')]},
    {title:'ALREADY HOLDING A GUN?',rows:[row('Keep its ready grip held','gameplay.ready_weapon','Carry an already-held gun into focus. That held grip does not also change pages.'),row('Fire the held gun','gameplay.fire_or_cqc','Trigger zoom is dormant while the gun owns the triggers.'),note('Return to page cycling','RELEASE, THEN PRESS AGAIN','Stow the gun first. A fresh tab press can then advance the iDroid.')]}
   ],[
    {title:'SCREEN CONTROLS · FOCUS HELD',rows:[axisRow('Navigate lists','menu','All stick directions belong to the menu.'),axisRow('Move map / cursor','map','Up, down, left and right do not also move or turn Snake.'),row('Confirm','menus.confirm'),row('Page action X','menus.action_x'),row('Page action Y / MAP–NAV','menus.action_y'),row('Zoom out / native LT','menus.left_trigger','Available when the triggers are not owned by a held gun.'),row('Zoom in / native RT','menus.right_trigger','Available when the triggers are not owned by a held gun.'),row('Zoom step / native stick click','menus.right_click')]},
    {title:'PAUSE & BLOCKING MENUS',rows:[row('Previous tab','menus.previous_tab','Works here. In handheld iDroid, the shared focus grip is consumed by focus.'),note('Context owns the buttons','ONE MODE AT A TIME','The same button can be reused in a different mode. It must not perform both actions at once.')]}
   ]]},
  {id:'03-optics-and-equipment',title:'Optics & equipment',subtitle:'Look through the actual optic. Aim with your hand.',illustration:'optic',
   callout:['FITTED SIGHT + MAGNIFIER','On the equipped SVG-76, ready the gun and use its zoom binding to switch 1× → 2× → 1×.'],
   columns:[[
    {title:'WEAPON OPTICS',rows:[combo('Change fitted optic power','gameplay.ready_weapon','gameplay.zoom','Keep the ready binding held. Center the stick before repeating.'),note('Fixed-power sights','ONE AUTHORED POWER','A fixed-power optic does not acquire extra zoom levels.'),note('Sight alignment','EYE → OPTIC → TARGET','Only the optic view magnifies; the surrounding world stays at normal scale.')]},
    {title:'PHYSICAL BINOCULARS',rows:[row('Equip','gameplay.equip_binoculars','Hold the complete chord, release, then raise the ocular to your eye.'),row('Zoom the lens','binoculars.zoom','Cycles 2× / 4×. Lowering the device keeps the selected power.'),row('Mark the visible target','binoculars.mark','Aim with your hand. Automatic marking has its own setting.'),row('Clear mark','binoculars.clear_mark'),row('Stow','binoculars.stow'),row('Sprint while carrying','binoculars.run'),row('Stance while carrying','binoculars.stance'),row('Dive while carrying','binoculars.dive')]}
   ],[
    {title:'LEFT-ARM EQUIPMENT PICKER',rows:[row('Hold open','equipment.open','Release without choosing to cancel; release after choosing to equip.'),row('Primary weapon category','equipment.primary'),row('Secondary weapon category','equipment.secondary'),row('Support category','equipment.support'),row('Items category','equipment.items'),axisRow('Browse the cards','equipment','Center after choosing a category, then point to a card.'),row('Use item','equipment.use','Only items with a native Use prompt support this.'),row('Back','equipment.back')]},
    {title:'ORDERS & COMMANDS',rows:[row('Open Commands','commands.open','Keep its hold binding down while choosing.'),axisRow('Select an order','commands'),row('Confirm the order','commands.confirm'),row('Close / back','commands.back')]}
   ]]},
  {id:'04-driving-and-travel',title:'Driving & travel',subtitle:'Physical steering. Native vehicle and travel controls.',illustration:'wheel',personalVehicleGun:modernVehicle,twoHandWheel,
   callout:[twoHandWheel?'DRIVE WITH ONE HAND OR TWO':'PHYSICAL WHEEL STEERING',modernVehicle?'Grip near the wheel to steer. Away from the wheel, the ready binding requests the gun where the native seat permits it.':twoHandWheel?'Grip near the wheel to take hold. The right hand joins only while empty; release to let go.':'Hold the left wheel grip near the wheel to steer. Release to let go.'],
   columns:[[
    {title:'DRIVING',rows:[row('Hold LEFT wheel contact','vehicle.wheel_grip','Squeeze near the wheel and rotate your hand.'),...(action('vehicle.right_wheel_grip')?[row('Join with an empty RIGHT hand','vehicle.right_wheel_grip','Fresh squeeze near the wheel; release to detach.')]:[]),axisRow('Steer without a wheel hold','vehicle_steering','Horizontal axis.'),row(modernVehicle?'Accelerate · gun stowed':'Accelerate','vehicle.accelerate'),row(modernVehicle?'Brake / reverse · gun stowed':'Brake / reverse','vehicle.brake_reverse'),row('Enter / exit vehicle','vehicle.interact','Use the native prompt. Stow a ready gun before exiting.')]},
    {title:'D-HORSE',rows:[axisRow('Ride','move'),row('Gallop','horse.gallop'),row('Mount / dismount','horse.interact','Release weapon ready and allow stow before dismounting.'),row('Mounted Commands','commands.mounted_open','Keep its mounted hold binding down while choosing.')]}
   ],[
    {title:modernVehicle?'PERSONAL GUN IN A VEHICLE':'NATIVE VEHICLE CONTROLS',rows:modernVehicle?[row('Ready away from the wheel','vehicle.ready_weapon','The wheel and gun do not own the same squeeze. Personal-gun availability depends on the native vehicle and seat.'),row('Fire the ready gun','vehicle.fire','The firing trigger does not also accelerate.'),row('Reload the ready gun','vehicle.reload'),axisRow('Pedals with a gun ready','vehicle_pedals','Push up to accelerate; down to brake / reverse. Stow the gun to return pedals to the triggers.'),axisRow('Camera / turret','equipment','Both horizontal and vertical axes remain available.'),row('Choose equipment','vehicle.equipment_open','Use the complete chord. Bare ready grip does not open this picker.')]:[row('Vehicle weapon / call','vehicle.weapon_or_call','Depends on the vehicle and seat; this is not a personal-gun draw binding.'),axisRow('Camera / turret','equipment','Both horizontal and vertical axes remain available.'),row('Choose equipment','vehicle.equipment_open','Use the complete saved binding.'),note('Personal gun while driving','NOT AVAILABLE','This build does not expose a personal-gun firing adapter for the driver.')]},
    {title:'ACC & DEPLOYMENT',rows:[row('Open iDroid from the ACC','system.idroid','Use it to choose a mission or Free Roam and prepare a sortie.'),row('Focus to select deployment','system.idroid_focus','Select a landing zone and vehicle in the native loadout.'),note('During the helicopter ride','MOVE YOUR HEAD & HANDS','The cabin moves with the helicopter.'),row('Exit when the prompt appears','vehicle.interact','Use the native exit prompt at the landing zone.')]},
    {title:'OPTIONAL NATIVE INPUT',rows:[row('Toggle native buttons','system.native_buttons','Ordinary VR action bindings become dormant. Toggle again to return.'),note('Personal remaps','LAUNCHER → CONTROLLER BINDINGS','Save, release all controls and center both sticks for two seconds. Export a fresh guide after changing a binding.')]}
   ]]}
 ];
 const heroAction=(title,names)=>{names=Array.isArray(names)?names:[names];const chosen=names.map(name=>action(name)?.bindings?.[0]).filter(Boolean);return {title,inputs:[...new Set(chosen.flatMap(b=>b.inputs))],binding:chosen.map(bindingLabel).join(' + ')||'UNBOUND'};};
 const heroAxis=(title,name)=>{const source=bindings.axes.find(a=>a.name==='axes.'+name)?.source;return {title,inputs:source&&source!=='disabled'?[source+'_up']:[],binding:axis(name)};};
 const diagrams=[
  [heroAxis('MOVE','move'),heroAction('SPRINT','gameplay.run'),heroAction('WRIST PICKER','equipment.open'),heroAction('READY WEAPON','gameplay.ready_weapon'),heroAction('FIRE / THROW','gameplay.fire_or_cqc'),heroAction('STANCE','gameplay.stance')],
  [heroAction('HOLD TO FOCUS','system.idroid_focus'),heroAction('OPEN / STOW','system.idroid'),heroAction('PREVIOUS PAGE','idroid.previous_tab'),heroAction('NEXT PAGE','menus.next_tab'),heroAxis('MAP / CURSOR','map'),heroAction('CONFIRM','menus.confirm')],
  [heroAction('EQUIP BINOCULARS','gameplay.equip_binoculars'),heroAction('WRIST PICKER','equipment.open'),heroAction('BINOCULAR ZOOM','binoculars.zoom'),heroAction('SCOPE POWER',['gameplay.ready_weapon','gameplay.zoom']),heroAxis('BROWSE CARDS','equipment'),heroAction('MARK TARGET','binoculars.mark')],
  [heroAction('LEFT WHEEL HOLD','vehicle.wheel_grip'),heroAction('BRAKE / REVERSE','vehicle.brake_reverse'),heroAction('ENTER / EXIT','vehicle.interact'),heroAction('RIGHT WHEEL HOLD','vehicle.right_wheel_grip'),heroAction('ACCELERATE','vehicle.accelerate'),heroAxis('CAMERA / TURRET','equipment')]
 ];
 pages.forEach((page,index)=>{page.diagram=diagrams[index];});
 return pages;
}

// Reuse the Field Terminal's art, materials and actual controller models. No
// historical binding text or invented game/device illustration is republished.
const theme={paper:'#f2efe5',ink:'#252625',muted:'#61615a',line:'#bcb8aa',red:'#af2624',panel:'#f7f3e9'};
function font(ctx,size,weight=400,family='Segoe UI'){ctx.font=`${weight} ${size}px "${family}",Arial,sans-serif`;}
function text(ctx,value,x,y,size=20,color=theme.ink,weight=400,family='Segoe UI'){font(ctx,size,weight,family);ctx.fillStyle=color;ctx.fillText(value,x,y);}
function display(ctx,value,x,y,size=27,color=theme.ink){text(ctx,value,x,y,size,color,400,'Impact');}
function mono(ctx,value,x,y,size=12,color=theme.muted){text(ctx,value,x,y,size,color,400,'Consolas');}
function lines(ctx,value,maxWidth,size=17,weight=400,family='Segoe UI'){font(ctx,size,weight,family);const result=[];for(const paragraph of String(value).split('\n')){let line='';for(const word of paragraph.split(/\s+/)){const next=line?line+' '+word:word;if(ctx.measureText(next).width>maxWidth&&line){result.push(line);line=word;}else line=next;}result.push(line);}return result;}
function paragraph(ctx,value,x,y,width,size=17,color=theme.muted,weight=400,lineHeight=size*1.3,family='Segoe UI'){for(const line of lines(ctx,value,width,size,weight,family)){text(ctx,line,x,y,size,color,weight,family);y+=lineHeight;}return y;}
function line(ctx,x1,y1,x2,y2,color=theme.line,width=1){ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();}
function imageAt(url){return new Promise((resolve,reject)=>{const img=new Image();img.onload=()=>resolve(img);img.onerror=()=>reject(Error('Could not load field-kit asset: '+url));img.src=url;});}
let assetsPromise;
function fieldAssets(){
 if(!assetsPromise)assetsPromise=(async()=>{
  const [header,grain]=await Promise.all([imageAt(new URL('assets/field-header.svg',import.meta.url)),imageAt(new URL('paper-grain.svg',import.meta.url))]);
  const canvas=document.createElement('canvas');canvas.setAttribute('aria-hidden','true');Object.assign(canvas.style,{position:'fixed',left:'-4096px',top:'0',width:'350px',height:'380px',pointerEvents:'none'});document.body.append(canvas);
  const player=new ControllerPlayer(canvas);await player.load();
  return {header,grain,player};
 })();
 return assetsPromise;
}
function tokenHand(token){return /^(x|y)(?:_touch)?$|^left_|^menu$/.test(token)?'left':'right';}
async function controllerPlate(player,item){
 const target=item.inputs.find(t=>!t.includes('grip'))||item.inputs[0];
 player.setInputs(target?[target]:[]);const focus=player.focus;
 player.setInputs(item.inputs);player.focus=focus;player.zoomHand=target?tokenHand(target):'left';player.closeUp=false;
 if(focus)player.focusControl(false);else {player.azimuth=0;player.pitch=.06;player.distance=.39;}
 player.distance=.36;player.render();
 const rect=player.renderer.domElement.getBoundingClientRect(),p=target?player.inputScreenPoint(target):null;
 const anchor=p?{x:(p.x-rect.left)/rect.width,y:(p.y-rect.top)/rect.height}:null;
 const captured=player.capture().controller;
 return {image:await imageAt(captured),anchor,target};
}
async function diagram(ctx,page,player){
 // Each control gets the asset's proper face/side/rear view. A single face
 // render cannot honestly point to the trigger hidden behind its housing.
 const width=377,height=162,gap=19,top=351;
 for(let index=0;index<page.diagram.length;index++){
  const item=page.diagram[index],plate=await controllerPlate(player,item),column=index%3,row=Math.floor(index/3);
  const x=35+column*(width+gap),y=top+row*(height+14);
  ctx.fillStyle='#e9e4d8';ctx.fillRect(x,y,width,height);line(ctx,x,y,x+width,y,theme.ink,2);
  ctx.drawImage(plate.image,x+4,y+5,148,151);
  const textX=x+159,textWidth=width-171;
  mono(ctx,'0'+(index+1)+' / '+(plate.target?tokenHand(plate.target).toUpperCase():'UNBOUND'),textX,y+24,11,theme.muted);
  let titleY=y+50;for(const titleLine of lines(ctx,item.title,textWidth,22,400,'Impact')){display(ctx,titleLine,textX,titleY,22);titleY+=24;}
  paragraph(ctx,item.binding,textX,titleY+3,textWidth,14,theme.red,700,18);
  if(plate.anchor){const tx=x+4+plate.anchor.x*148,ty=y+5+plate.anchor.y*151;
   line(ctx,textX-7,y+75,x+151,y+75,theme.red,1.2);line(ctx,x+151,y+75,tx,ty,theme.red,1.2);
   ctx.beginPath();ctx.arc(tx,ty,3.5,0,Math.PI*2);ctx.fillStyle=theme.red;ctx.fill();ctx.strokeStyle=theme.paper;ctx.lineWidth=1;ctx.stroke();
  }
 }
 mono(ctx,'QUEST TOUCH PLUS / ACTUAL 3D MODELS / RED = ACTIVE CONTROL',43,716,12);
 ctx.textAlign='right';mono(ctx,'FULL CHORDS + ALTERNATIVES BELOW',1197,716,11);ctx.textAlign='left';
}
function sectionHeight(ctx,section,width){let height=53;for(const row of section.rows){height+=23+lines(ctx,row.binding,width-22,18,700).length*23+(row.note?lines(ctx,row.note,width-22,15).length*20+3:0)+15;}return height+3;}
function section(ctx,section,x,y,width){const height=sectionHeight(ctx,section,width);ctx.fillStyle=theme.ink;ctx.fillRect(x,y,width,32);ctx.fillStyle=theme.red;ctx.fillRect(x,y,6,32);display(ctx,section.title,x+15,y+24,22,theme.paper);y+=53;for(const row of section.rows){text(ctx,row.title,x+11,y,17,theme.ink,600);y+=23;y=paragraph(ctx,row.binding,x+11,y,width-22,18,theme.red,700,23);if(row.note)y=paragraph(ctx,row.note,x+11,y+3,width-22,15,theme.muted,400,20);y+=15;line(ctx,x+11,y-24,x+width-11,y-24);}return height;}
export async function drawControlGuide(canvas,page,{revision='',profile='SAVED PERSONAL CONTROLS'}={}){
 const assets=await fieldAssets();await document.fonts.ready;
 const {width,height,scale}=guideSize;canvas.width=width*scale;canvas.height=height*scale;const ctx=canvas.getContext('2d');ctx.scale(scale,scale);ctx.fillStyle=theme.paper;ctx.fillRect(0,0,width,height);ctx.fillStyle=ctx.createPattern(assets.grain,'repeat');ctx.fillRect(0,0,width,height);
 ctx.drawImage(assets.header,28,8,width-56,(width-56)*302/1440);
 ctx.fillStyle=theme.red;ctx.fillRect(35,259,width-70,61);display(ctx,page.title.toUpperCase(),52,306,48,theme.paper);ctx.textAlign='right';display(ctx,page.id.slice(0,2)+' / 04',width-53,303,34,theme.paper);ctx.textAlign='left';
 mono(ctx,page.subtitle.toUpperCase(),44,336,13,theme.ink);
 await diagram(ctx,page,assets.player);
 ctx.fillStyle=theme.ink;ctx.fillRect(35,735,width-70,78);ctx.fillStyle=theme.red;ctx.fillRect(35,735,7,78);display(ctx,page.callout[0],53,764,23,theme.paper);paragraph(ctx,page.callout[1],53,790,width-105,17,theme.paper,400,21);
 const columnWidth=(width-98)/2,top=833,available=height-90-top;
 const heightAt=factor=>Math.max(...page.columns.map(column=>(column.reduce((sum,s)=>sum+sectionHeight(ctx,s,columnWidth/factor)+18,0)-18)*factor));
 let fit=1;if(heightAt(1)>available){let low=.5,high=1;for(let n=0;n<20;n++){const middle=(low+high)/2;if(heightAt(middle)>available)high=middle;else low=middle;}fit=low;}
 page.columns.forEach((column,index)=>{ctx.save();ctx.translate(35+index*(columnWidth+28),top);ctx.scale(fit,fit);let y=0;for(const s of column)y+=section(ctx,s,0,y,columnWidth/fit)+18;ctx.restore();});
 line(ctx,35,height-67,width-35,height-67,theme.red,2);mono(ctx,'MGS5VR / FIELD KIT',35,height-43,13,theme.ink);mono(ctx,profile+' / '+(revision?revision.slice(0,12):'LIVE CATALOG'),35,height-21,11);ctx.textAlign='right';mono(ctx,'THE PHANTOM PAIN / OPENXR',width-35,height-43,12,theme.ink);mono(ctx,'REFRESH THIS GUIDE AFTER REMAPPING',width-35,height-21,11);ctx.textAlign='left';
 return {id:page.id,width:canvas.width,height:canvas.height,contentScale:fit,art:'field-terminal-header-and-real-controller-models'};
}
export async function downloadControlGuide(page,state){const canvas=document.createElement('canvas');await drawControlGuide(canvas,page,{revision:state.controlsRevision});const blob=await new Promise(resolve=>canvas.toBlob(resolve,'image/png'));if(!blob)throw Error('Could not render the PNG guide.');const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='MGS5VR-'+page.id+'.png';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}

