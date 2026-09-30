import * as THREE from 'three';

// Reference geometry only: no retail model or texture is distributed here.
// Metres and Y * X * Z rotation match the native fitting controls.
const V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z);
const degrees=Math.PI/180;
const rotation=(pitch=0,yaw=0,roll=0)=>new THREE.Quaternion().setFromEuler(new THREE.Euler(pitch*degrees,yaw*degrees,roll*degrees,'YXZ'));
export const fitContexts={
 idroid:['iDroid','Right hand · device · projector · screen'],
 wrist:['Left forearm','Bionic arm · weapon info · HUD · equipment'],
 hands:['Hands & weapons','Grip position · finger contact · support distance'],
 optics:['Optics','Physical binoculars · scope · eye clearance'],
 menus:['Spatial menus','Pause · off-wrist iDroid · theatre'],
 movement:['Movement','Eye height · turning · physical reach']
};
export function fitContext(name){
 if(/idroid|handheld/.test(name))return 'idroid';
 if(/wrist|weapon_hud|hud_mode/.test(name))return 'wrist';
 if(/binocular|scope/.test(name))return 'optics';
 if(/menu_quad|theatre\./.test(name))return 'menus';
 if(/hand_|support_|smoothing|controller_rig/.test(name))return 'hands';
 if(/height|turn|melee|animal_touch|head_camera/.test(name))return 'movement';
 return null;
}

export class FitPreview {
 constructor(canvas){
  this.canvas=canvas;this.context='idroid';this.values={};this.azimuth=.32;this.pitch=.18;this.zoom=1;this.touch=false;this.motion=false;
  this.scene=new THREE.Scene();this.scene.background=new THREE.Color('#202c2e');
  this.scene.add(new THREE.HemisphereLight(0xe4f4ef,0x454032,2.5));
  const light=new THREE.DirectionalLight(0xffedcb,3);light.position.set(1,2,3);this.scene.add(light);
  this.camera=new THREE.PerspectiveCamera(38,1,.005,100);
  this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:false});this.renderer.setPixelRatio(Math.min(devicePixelRatio,2));this.renderer.outputColorSpace=THREE.SRGBColorSpace;
  this.root=new THREE.Group();this.scene.add(this.root);this.target=V(0,.045,0);this.distance=.9;
  this.build();this.controls();this.render();
 }
 material(color,extra={}){return new THREE.MeshStandardMaterial({color,roughness:.7,metalness:.2,...extra});}
 box(parent,size,position,color,extra={}){const mesh=new THREE.Mesh(new THREE.BoxGeometry(...size),this.material(color,extra));mesh.position.copy(V(...position));parent.add(mesh);return mesh;}
 cylinder(parent,radius,length,position,color){const mesh=new THREE.Mesh(new THREE.CylinderGeometry(radius,radius,length,24),this.material(color));mesh.position.copy(V(...position));parent.add(mesh);return mesh;}
 finger(parent,width,length,position,color){const mesh=new THREE.Mesh(new THREE.CapsuleGeometry(width/2,Math.max(.002,length-width),4,12),this.material(color));mesh.rotation.x=Math.PI/2;mesh.position.copy(V(...position));parent.add(mesh);return mesh;}
 hand(parent,bionic=false){
  const hand=new THREE.Group();parent.add(hand);const skin=bionic?'#bda052':'#707765',joint=bionic?'#333c39':'#3e4840';
  this.box(hand,[.076,.024,.09],[0,0,-.012],skin);this.box(hand,[.064,.035,.035],[0,-.004,.046],joint);
  this.box(hand,[.052,.048,.22],[0,-.013,.17],skin);this.box(hand,[.053,.05,.048],[0,-.013,.072],joint);
  if(bionic){for(let i=0;i<3;i++)this.box(hand,[.056,.008,.044],[0,.014,.102+i*.048],'#cfb265');}
  const fingers=[];
  for(let i=0;i<4;i++){
   const knuckle=new THREE.Group();knuckle.position.set((i-1.5)*.018,0,-.053);hand.add(knuckle);
   const length=(bionic?i===3:i===0)?.027:.034;
   this.finger(knuckle,.015,length,[0,0,-length*.5],skin);
   const middle=new THREE.Group();middle.position.z=-length;knuckle.add(middle);this.finger(middle,.014,.026,[0,0,-.013],skin);
   const tip=new THREE.Group();tip.position.z=-.026;middle.add(tip);this.finger(tip,.012,.019,[0,0,-.0095],skin);
   fingers.push([knuckle,middle,tip]);
  }
  const thumb=new THREE.Group();thumb.position.set(bionic?-.038:.038,0,.009);thumb.rotation.y=bionic?.65:-.65;hand.add(thumb);
  this.finger(thumb,.02,.033,[0,0,-.016],skin);const thumbTip=this.finger(thumb,.018,.025,[0,0,-.045],skin);
  hand.userData={fingers,thumb,thumbTip};return hand;
 }
 curl(hand,percent){const a=Math.min(1,Math.max(0,percent/100));hand.userData.fingers.forEach(([base,middle,tip])=>{base.rotation.x=-a*1.25;middle.rotation.x=-a*1.45;tip.rotation.x=-a});hand.userData.thumb.rotation.x=-a*.8;}
 panel(parent,width,height,text){
  const surface=document.createElement('canvas');surface.width=1024;surface.height=576;
  const ctx=surface.getContext('2d');ctx.fillStyle='#122f3b';ctx.fillRect(0,0,1024,576);ctx.strokeStyle='#77dcdd';ctx.lineWidth=5;ctx.strokeRect(5,5,1014,566);
  const compact=/AMMO|STATUS/.test(text);
  ctx.fillStyle='#bceced';ctx.font=(compact?'96':'36')+'px Bahnschrift, sans-serif';ctx.fillText(text,48,compact?145:70);ctx.font=(compact?'54':'22')+'px Bahnschrift, sans-serif';ctx.fillText('LIVE FIT PREVIEW',48,compact?235:108);
  ctx.strokeStyle='#426d77';ctx.lineWidth=2;
  for(let i=1;i<7;i++){ctx.beginPath();ctx.moveTo(36,130+i*53);ctx.lineTo(984,130+i*53);ctx.stroke();}
  ctx.beginPath();ctx.moveTo(512,172);ctx.lineTo(512,492);ctx.moveTo(280,332);ctx.lineTo(744,332);ctx.stroke();
  const texture=new THREE.CanvasTexture(surface);texture.colorSpace=THREE.SRGBColorSpace;
  const panel=new THREE.Mesh(new THREE.PlaneGeometry(1,1),new THREE.MeshBasicMaterial({map:texture,side:THREE.DoubleSide,transparent:true,opacity:.94}));panel.scale.set(width,height,1);parent.add(panel);return panel;
 }
 ring(parent,radius,color){const mesh=new THREE.Mesh(new THREE.TorusGeometry(radius,.0015,8,64),new THREE.MeshBasicMaterial({color,transparent:true,opacity:.8}));parent.add(mesh);return mesh;}
 build(){
  this.views={};for(const context of Object.keys(fitContexts)){const group=new THREE.Group();this.root.add(group);this.views[context]=group;}
  const i=this.views.idroid;this.idroidGrip=new THREE.Group();i.add(this.idroidGrip);
  this.idroidHand=this.hand(this.idroidGrip);this.idroidHand.position.set(0,-.08,-.022);this.idroidHand.rotation.x=Math.PI/2;this.curl(this.idroidHand,65);
  this.device=new THREE.Group();this.idroidGrip.add(this.device);
  this.box(this.device,[.062,.14,.029],[0,-.016,0],'#455753');this.box(this.device,[.05,.018,.035],[0,.044,0],'#899478');
  const lens=this.cylinder(this.device,.016,.006,[0,.051,.012349601],'#a4f7f1');this.box(this.device,[.048,.088,.006],[0,-.014,.018],'#35433f');
  this.box(this.device,[.021,.032,.008],[.02,-.043,.019],'#798373');
  this.emitter=V(0,.043378498,.012349601);
  this.screenPivot=new THREE.Group();this.device.add(this.screenPivot);this.idroidScreen=this.panel(this.screenPivot,.45,.45*9/16,'iDROID / MAP');
  this.beamGeometry=new THREE.BufferGeometry();this.beamGeometry.setAttribute('position',new THREE.Float32BufferAttribute(new Float32Array(36),3));this.beam=new THREE.Mesh(this.beamGeometry,new THREE.MeshBasicMaterial({color:0x62e6ec,side:THREE.DoubleSide,transparent:true,opacity:.085,depthWrite:false}));this.device.add(this.beam);
  const beamLinesGeometry=new THREE.BufferGeometry();beamLinesGeometry.setAttribute('position',new THREE.Float32BufferAttribute(new Float32Array(24),3));this.beamLines=new THREE.LineSegments(beamLinesGeometry,new THREE.LineBasicMaterial({color:0x75edef,transparent:true,opacity:.42}));this.device.add(this.beamLines);
  const w=this.views.wrist;this.leftGrip=new THREE.Group();w.add(this.leftGrip);this.leftHand=this.hand(this.leftGrip,true);this.curl(this.leftHand,35);
  this.leftGrip.rotation.x=.6;this.leftGrip.position.set(0,-.07,-.05);
  this.status=this.panel(this.leftHand,.09,.045,'AMMO / 30');this.status.rotation.x=-Math.PI/2;
  this.hud=this.panel(this.leftHand,.16,.09,'LEFT ARM / STATUS');this.hud.rotation.x=-Math.PI/2;
  this.picker=this.panel(this.leftHand,.42,.42*9/16,'EQUIPMENT');this.picker.rotation.x=-Math.PI/2;
  this.offWristPanel=this.panel(w,1.2,.675,'HUD / SPATIAL PANEL');this.offWristPanel.visible=false;
  this.armDisplay='status';
  const h=this.views.hands;this.hands=[this.hand(h,true),this.hand(h)];this.hands[0].position.x=-.14;this.hands[1].position.x=.14;
  this.weapon=new THREE.Group();this.hands[1].add(this.weapon);this.box(this.weapon,[.035,.045,.42],[0,.032,-.17],'#3d4741');const barrel=this.cylinder(this.weapon,.009,.26,[0,.04,-.36],'#858b7f');barrel.rotation.x=Math.PI/2;
  const volume=(radius,color)=>{const mesh=new THREE.Mesh(new THREE.SphereGeometry(radius,16,12),new THREE.MeshBasicMaterial({color,wireframe:true,transparent:true,opacity:.25}));this.hands[1].add(mesh);mesh.position.set(0,.03,-.19);return mesh;};
  this.support=volume(.1,'#7fe6d1');this.detach=volume(.3,'#d29668');
  const o=this.views.optics;this.opticGrip=new THREE.Group();o.add(this.opticGrip);this.binocular=new THREE.Group();this.opticGrip.add(this.binocular);
  this.opticHand=this.hand(this.binocular);this.opticHand.position.set(.063,-.03,0);this.curl(this.opticHand,70);
  // TPP's physical binocular device has one large ocular, not two invented
  // eye windows. The retail centre is shared with the native optic contract.
  this.box(this.binocular,[.10,.055,.12],[-.02,0,0],'#556158');
  const ocular=this.cylinder(this.binocular,.027,.018,[-.0328,-.0006,.059],'#37473f');ocular.rotation.x=Math.PI/2;
  const glass=this.cylinder(this.binocular,.022,.004,[-.0328,-.0006,.070],'#4dabb8');glass.rotation.x=Math.PI/2;
  this.box(this.binocular,[.04,.022,.065],[.01,.038,-.018],'#728075');
  this.eyeRing=this.ring(o,.037,'#e8e7c6');this.eyeRing.position.set(0,.15,.2);
  this.scope=new THREE.Group();o.add(this.scope);this.scope.position.set(-.16,.04,.02);const tube=this.cylinder(this.scope,.022,.18,[0,0,-.09],'#4a574d');tube.rotation.x=Math.PI/2;this.scopeGlass=this.ring(this.scope,.020,'#e87861');
  this.scopeEye=this.ring(o,.024,'#e8e7c6');
  const m=this.views.menus;this.worldPanel=this.panel(m,1.2,.675,'PAUSE / SPATIAL MENU');this.menuEye=new THREE.Mesh(new THREE.SphereGeometry(.035,16,12),this.material('#e2dbc0'));m.add(this.menuEye);this.menuEye.position.set(.52,0,0);
  this.menuLine=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:0x89c8c4}));m.add(this.menuLine);
  const move=this.views.movement;const grid=new THREE.GridHelper(2,10,0x8dafa7,0x435554);move.add(grid);
  this.avatar=new THREE.Group();move.add(this.avatar);this.box(this.avatar,[.18,.5,.11],[0,.95,0],'#53635a');this.cylinder(this.avatar,.09,.18,[0,1.3,0],'#abb49e');this.box(this.avatar,[.14,.7,.1],[0,.36,0],'#39483f');
  this.heightEye=this.ring(move,.05,'#7ef0e6');this.heightEye.rotation.y=Math.PI/2;
  this.reach=this.ring(this.avatar,.45,'#aeceaa');this.reach.position.y=1.05;
  this.turnArrow=new THREE.ArrowHelper(V(0,0,-1),V(0,.01,0),.75,0xf2ba78,.12,.07);move.add(this.turnArrow);
  this.update({});
 }
 value(key,fallback=0){const value=Number(this.values['settings.'+key]);return Number.isFinite(value)?value:fallback;}
 fit(group,prefix,base=V()){
  group.position.copy(base).add(V(...['x','y','z'].map(axis=>this.value(prefix+axis+'_cm')*.01)));
  group.quaternion.copy(rotation(...['pitch','yaw','roll'].map(axis=>this.value(prefix+axis+'_degrees'))));
 }
 update(values){
  this.values=values;
  for(const [key,view] of Object.entries(this.views))view.visible=key===this.context;
  this.fit(this.idroidGrip,'right_hand_');const deviceFit=new THREE.Group();this.fit(deviceFit,'idroid_grip_');
  this.idroidGrip.position.add(deviceFit.position.applyQuaternion(this.idroidGrip.quaternion));this.idroidGrip.quaternion.multiply(deviceFit.quaternion);
  const width=this.value('idroid_screen_width_cm',45)*.01,height=width*9/16;
  this.screenPivot.position.copy(this.emitter).add(V(this.value('idroid_screen_x_cm')*.01,this.value('idroid_screen_y_cm')*.01,this.value('idroid_screen_depth_cm',8)*.01));
  this.screenPivot.quaternion.copy(rotation(this.value('idroid_screen_pitch_degrees'),this.value('idroid_screen_yaw_degrees'),this.value('idroid_screen_roll_degrees')));
  this.idroidScreen.position.y=height*.5;this.idroidScreen.scale.set(width,height,1);
  const corners=[V(-width/2,0),V(width/2,0),V(width/2,height),V(-width/2,height)].map(v=>v.applyQuaternion(this.screenPivot.quaternion).add(this.screenPivot.position));
  const triangles=[],lines=[];for(let i=0;i<4;i++){triangles.push(...this.emitter.toArray(),...corners[i].toArray(),...corners[(i+1)%4].toArray());lines.push(...this.emitter.toArray(),...corners[i].toArray());}
  this.beamGeometry.attributes.position.array.set(triangles);this.beamGeometry.attributes.position.needsUpdate=true;this.beamGeometry.computeBoundingSphere();
  this.beamLines.geometry.attributes.position.array.set(lines);this.beamLines.geometry.attributes.position.needsUpdate=true;this.beamLines.geometry.computeBoundingSphere();
  this.fit(this.leftHand,'left_hand_');const lift=this.value('wrist_surface_lift_cm',2)*.01;
  this.status.position.set(0,.023+lift,.07+this.value('weapon_hud_setback_cm',6)*.01);const text=this.value('wrist_text_scale',1.5);this.status.scale.set(.09*text,.045*text,1);
  this.hud.position.set(0,.038+lift,.17);this.hud.scale.set(.16*text,.09*text,1);this.hud.visible=this.armDisplay==='hud'&&this.values['settings.hud_mode']!=='off';this.status.visible=this.armDisplay==='status';this.picker.visible=this.armDisplay==='picker';
  this.picker.position.set(0,.02+lift+this.value('wrist_selector_height_cm',15)*.01,.07);const pickerWidth=this.value('wrist_picker_width_cm',42)*.01;this.picker.scale.set(pickerWidth,pickerWidth*9/16,1);
  this.hands.forEach((hand,index)=>{this.fit(hand,(index?'right':'left')+'_hand_',V(index?.14:-.14,0,0));this.curl(hand,this.value(this.touch?'hand_touch_curl_percent':'hand_rest_curl_percent',this.touch?20:8));});
  this.rightHandFit=this.hands[1].quaternion.clone();
  this.support.scale.setScalar(this.value('support_grip_radius_cm',10)/10);this.detach.scale.setScalar(this.value('support_detach_radius_cm',30)/30);
  this.fit(this.opticGrip,'right_hand_');this.binocular.quaternion.copy(rotation(this.value('binocular_pitch_degrees',-90),this.value('binocular_yaw_degrees'),this.value('binocular_roll_degrees')));
  const opticOrientation=this.opticGrip.quaternion.clone().multiply(this.binocular.quaternion);
  const ocular=V(-.0328,-.0006,.0551+this.value('binocular_max_eye_distance_cm',30)*.01).applyQuaternion(opticOrientation).add(this.opticGrip.position);
  this.eyeRing.position.copy(ocular);this.eyeRing.quaternion.copy(opticOrientation);
  this.scopeEye.position.set(-.16,.04,.02+this.value('scope_eye_relief_cm',10)*.01);
  this.worldPanel.scale.set(this.value('menu_quad_width_cm',120)*.01,this.value('menu_quad_width_cm',120)*.01*9/16,1);
  this.worldPanel.position.z=-this.value('menu_quad_distance_cm',130)*.01;this.worldPanel.quaternion.copy(rotation(this.value('menu_quad_tilt_degrees',-10)));
  const offWrist=String(this.values['diagnostics.wrist_hud_experiment'])==='0';this.offWristPanel.visible=offWrist;this.offWristPanel.position.copy(this.worldPanel.position);this.offWristPanel.scale.copy(this.worldPanel.scale);this.offWristPanel.quaternion.copy(this.worldPanel.quaternion);
  if(offWrist)this.hud.visible=this.picker.visible=this.status.visible=false;
  if(this.values['fit.theatre']){this.worldPanel.scale.set(Number(values['theatre.width_cm']||1200)*.01,Number(values['theatre.width_cm']||1200)*.01*9/16,1);this.worldPanel.position.z=-Number(values['theatre.distance_cm']||600)*.01;}
  this.menuLine.geometry.setFromPoints([this.menuEye.position,this.worldPanel.position]);
  this.heightEye.position.set(0,1.4+this.value('player_height_offset_cm')*.01,0);this.reach.visible=this.value('motion_melee',1)!==0||this.value('animal_touch',1)!==0;
  const turn=this.values['settings.turn_mode'],angle=turn==='off'?0:turn==='native_smooth'?45:this.value('snap_turn_degrees',30);this.turnArrow.setDirection(V(Math.sin(angle*degrees),0,-Math.cos(angle*degrees)));
  this.render();
 }
 setContext(context){if(!fitContexts[context])return;this.context=context;this.setView('front');if(context==='wrist'){this.pitch=.6;this.azimuth=-.35;}this.update(this.values);}
 setView(view){this.azimuth=view==='side'?Math.PI/2:view==='back'?Math.PI:0;this.pitch=view==='top'?1.35:view==='side'?.05:.12;this.zoom=1;this.render();}
 controls(){
  let drag=null;
  this.canvas.addEventListener('pointerdown',e=>{drag={x:e.clientX,y:e.clientY};this.canvas.setPointerCapture(e.pointerId);});
  this.canvas.addEventListener('pointermove',e=>{if(!drag)return;this.azimuth-=(e.clientX-drag.x)*.009;this.pitch=Math.max(-1.4,Math.min(1.4,this.pitch+(e.clientY-drag.y)*.009));drag={x:e.clientX,y:e.clientY};this.render();});
  const release=()=>drag=null;this.canvas.addEventListener('pointerup',release);this.canvas.addEventListener('pointercancel',release);
  this.canvas.addEventListener('wheel',e=>{e.preventDefault();this.zoom=Math.max(.35,Math.min(2.8,this.zoom*Math.exp(e.deltaY*.001)));this.render();},{passive:false});
  this.canvas.addEventListener('keydown',e=>{const keys=['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','+','=','-','0'];if(!keys.includes(e.key))return;e.preventDefault();if(e.key==='0')this.setView('front');else{this.azimuth+=(e.key==='ArrowLeft'?.12:e.key==='ArrowRight'?-.12:0);this.pitch=Math.max(-1.4,Math.min(1.4,this.pitch+(e.key==='ArrowUp'?.12:e.key==='ArrowDown'?-.12:0)));this.zoom=Math.max(.35,Math.min(2.8,this.zoom*(e.key==='+'||e.key==='='?.9:e.key==='-'?1.1:1)));this.render();}});
 }
 tick(time){
  if(!this.motion)return;
  const amplitude=.012/(1+this.value('weapon_smoothing_ms')/20),jitter=Math.sin(time*.019)*amplitude;
  this.hands[1].quaternion.copy(this.rightHandFit).multiply(rotation(0,jitter/degrees,0));this.scope.rotation.y=jitter;this.render();
 }
 render(){
  if(!this.renderer)return;const rect=this.canvas.getBoundingClientRect();if(rect.width<1||rect.height<1)return;
  let target=V(0,.06,0),distance=.95;
  if(this.context==='wrist'){target=V(0,.04,.07);distance=.82;if(this.offWristPanel.visible){target=V(0,0,this.offWristPanel.position.z*.65);distance=Math.max(2.9,this.offWristPanel.scale.x*1.7);}}
  if(this.context==='hands'){target=V(0,0,-.08);distance=1.25;}
  if(this.context==='optics'){target=V(0,.12,.015);distance=1.1;}
  if(this.context==='menus'){const d=-this.worldPanel.position.z;target=V(0,0,-d*.65);distance=Math.max(2.9,d*1.55,this.worldPanel.scale.x*1.7);}
  if(this.context==='movement'){target=V(0,.75,0);distance=3.1;}
  this.target.copy(target);this.distance=distance;
  if(this.renderWidth!==rect.width||this.renderHeight!==rect.height){this.renderer.setSize(rect.width,rect.height,false);this.renderWidth=rect.width;this.renderHeight=rect.height;this.camera.aspect=rect.width/rect.height;this.camera.updateProjectionMatrix();}
  const d=distance*this.zoom;this.camera.position.copy(target).add(V(Math.sin(this.azimuth)*Math.cos(this.pitch)*d,Math.sin(this.pitch)*d,Math.cos(this.azimuth)*Math.cos(this.pitch)*d));this.camera.lookAt(target);this.renderer.render(this.scene,this.camera);
 }
 snapshot(){
  this.root.updateMatrixWorld(true);const world=object=>object.getWorldPosition(V()).toArray();
  return {context:this.context,azimuth:this.azimuth,pitch:this.pitch,zoom:this.zoom,touch:this.touch,motion:this.motion,armDisplay:this.armDisplay,offWrist:this.offWristPanel.visible,device:world(this.device),emitter:this.device.localToWorld(this.emitter.clone()).toArray(),screen:world(this.idroidScreen),lowerEdge:world(this.screenPivot),screenSize:[this.idroidScreen.scale.x,this.idroidScreen.scale.y],screenRotation:this.screenPivot.quaternion.toArray(),hand:world(this.idroidHand),status:world(this.status),pickerSize:this.picker.scale.toArray(),scopeEye:world(this.scopeEye),eyeHeight:this.heightEye.position.y};
 }
}
