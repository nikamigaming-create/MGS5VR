import * as THREE from 'three';
import { GLTFLoader } from './vendor/three-r180/examples/jsm/loaders/GLTFLoader.js';

const tokens={a:['right','a_button'],b:['right','b_button'],x:['left','x_button'],y:['left','y_button'],
 menu:['left','menu_marker'],left_grip:['left','squeeze'],right_grip:['right','squeeze'],
 left_trigger:['left','trigger'],right_trigger:['right','trigger'],
 left_stick_click:['left','thumbstick'],right_stick_click:['right','thumbstick'],
 left_thumbrest:['left','thumbrest_pressed_value'],right_thumbrest:['right','thumbrest_pressed_value']};
for(const hand of ['left','right'])for(const direction of ['up','down','left','right'])tokens[`${hand}_stick_${direction}`]=[hand,'thumbstick'];

for(const face of ['a','b','x','y'])tokens[face+'_touch']=tokens[face];
for(const hand of ['left','right']){
 tokens[hand+'_stick_touch']=tokens[hand+'_stick_click'];
 tokens[hand+'_trigger_touch']=tokens[hand+'_trigger'];
}

export class ControllerPlayer{
 constructor(canvas,detail){
  this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:true});this.renderer.setPixelRatio(Math.min(devicePixelRatio,2));this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=.85;
  this.detailRenderer=detail?new THREE.WebGLRenderer({canvas:detail,antialias:true,alpha:true}):null;this.detailRenderer?.setPixelRatio(Math.min(devicePixelRatio,2));
  this.scene=new THREE.Scene();this.scene.add(new THREE.HemisphereLight(0xffffeb,0x686b64,1.1));
  const key=new THREE.DirectionalLight(0xffffff,2.3);key.position.set(-.3,.7,1);this.scene.add(key);
  const fill=new THREE.DirectionalLight(0xffe8bd,.7);fill.position.set(.5,.1,-.1);this.scene.add(fill);
  this.camera=new THREE.PerspectiveCamera(31,1,.002,5);this.detailCamera=new THREE.PerspectiveCamera(29,1,.001,3);
  this.groups={};this.models={};this.baseMaterials=[];this.restTransforms=[];this.markers={};this.directionArrows={};this.letterLabels=[];this.active=[];this.azimuth=0;this.pitch=.06;this.distance=.39;this.focus=null;this.zoomHand=null;
  this.manualUntil=0;this.autoKey='';this.autoSince=0;this.autoTarget={azimuth:0,pitch:.06};this.lastAuto=0;
  let drag=null,origin=null;canvas.addEventListener('pointerdown',e=>{this.manualUntil=performance.now()+5000;origin=[e.clientX,e.clientY];drag=[e.clientX,e.clientY];canvas.setPointerCapture(e.pointerId)});
  canvas.addEventListener('pointerup',e=>{if(origin&&Math.hypot(e.clientX-origin[0],e.clientY-origin[1])<5)this.pick(e.clientX,e.clientY);drag=null;origin=null});canvas.addEventListener('pointercancel',()=>{drag=null;origin=null});
  canvas.addEventListener('pointermove',e=>{if(!drag)return;this.manualUntil=performance.now()+5000;this.azimuth+=(e.clientX-drag[0])*.008;this.pitch=THREE.MathUtils.clamp(this.pitch+(e.clientY-drag[1])*.005,-.8,.8);drag=[e.clientX,e.clientY];this.render()});
  canvas.addEventListener('wheel',e=>{e.preventDefault();this.manualUntil=performance.now()+5000;this.distance=THREE.MathUtils.clamp(this.distance+e.deltaY*.0003,.28,1.1);this.render()},{passive:false});
 }
 async load(){
  const loader=new GLTFLoader();
  for(const hand of ['left','right']){
   const gltf=await loader.loadAsync(`assets/meta-quest-touch-plus-${hand}.glb`);const model=gltf.scene;
   model.updateMatrixWorld(true);const body=model.getObjectByName('controller_mesh');const bounds=new THREE.Box3().setFromObject(body);const center=bounds.getCenter(new THREE.Vector3());
   const front=new THREE.Vector3(hand==='left'?-.025:.025,.779,-.624).normalize();
   // Same upright face view as the checked atlas: controller handles point down.
   const up=new THREE.Vector3(0,-.624,-.779);up.addScaledVector(front,-up.dot(front)).normalize();
   const right=new THREE.Vector3().crossVectors(up,front).normalize();up.crossVectors(front,right).normalize();
   const rotation=new THREE.Matrix4().makeBasis(right,up,front).invert();
   const rig=new THREE.Group();rig.position.x=hand==='left'?-.055:.055;
   model.position.copy(center).negate().applyMatrix4(rotation);model.quaternion.setFromRotationMatrix(rotation);
   rig.add(model);this.scene.add(rig);this.groups[hand]=rig;this.models[hand]=model;
   model.traverse(o=>{if(o.isMesh){o.material=Array.isArray(o.material)?o.material.map(m=>m.clone()):o.material.clone();for(const material of [].concat(o.material)){material.roughness=.5;this.baseMaterials.push({material,color:material.color.clone(),emissive:material.emissive?.clone()});}}});
   model.traverse(node=>{if(node.name.endsWith('_value'))this.restTransforms.push({node,position:node.position.clone(),quaternion:node.quaternion.clone()})});
   const rest=model.getObjectByName('thumbrest_pressed_value');if(rest)this.addMarker(hand,'thumbrest_pressed_value',rest);
  }
  try{const response=await fetch('assets/controller-spatial-anchors.json');if(response.ok){const data=await response.json();const p=data.left_menu?.gltf_world_position;
   if(p){const anchor=new THREE.Object3D();anchor.name='menu_marker';anchor.position.fromArray(p);this.models.left.add(anchor);this.addMarker('left','menu_marker',anchor);}}}catch{}
  this.ready=true;this.render();
  for(const [token,[hand,name]] of Object.entries(tokens))if(['a','b','x','y','menu'].includes(token)){
   const node=this.models[hand].getObjectByName(name);if(!node)continue;
   const canvas=document.createElement('canvas');canvas.width=canvas.height=128;const context=canvas.getContext('2d');context.fillStyle='#333832';context.font='600 86px sans-serif';context.textAlign='center';context.textBaseline='middle';context.fillText(token==='menu'?'≡':token.toUpperCase(),64,67);
   const sprite=new THREE.Sprite(new THREE.SpriteMaterial({map:new THREE.CanvasTexture(canvas),depthTest:true}));sprite.scale.set(.006,.006,.006);this.scene.add(sprite);this.letterLabels.push({sprite,node,hand});
  }
  for(const hand of ['left','right']){
   const stick=this.models[hand].getObjectByName('thumbstick');const point=new THREE.Box3().setFromObject(stick).getCenter(new THREE.Vector3());point.z+=.012;
   for(const [direction,axis] of Object.entries({up:[0,1,0],down:[0,-1,0],left:[-1,0,0],right:[1,0,0]})){
    const arrow=new THREE.ArrowHelper(new THREE.Vector3(...axis),point,.035,0xb32315,.010,.007);arrow.visible=false;arrow.line.material.depthTest=false;arrow.cone.material.depthTest=false;arrow.renderOrder=11;this.scene.add(arrow);this.directionArrows[`${hand}_stick_${direction}`]=arrow;
   }
  }
 }
 addMarker(hand,name,parent){const geometry=new THREE.SphereGeometry(.004,16,10);const material=new THREE.MeshBasicMaterial({color:0xff3b24,transparent:true,opacity:.88,depthTest:false});const marker=new THREE.Mesh(geometry,material);marker.renderOrder=10;marker.visible=false;parent.add(marker);this.markers[`${hand}:${name}`]=marker;}
 setView(view){this.closeUp=false;this.zoomHand=null;this.manualUntil=performance.now()+5000;this.azimuth=view==='left'?.95:view==='right'?-.95:0;this.pitch=.06;this.distance=.39;this.render()}
 autoPresent(inputs,now=performance.now()){
  // Turn toward the actual surfaces involved in the current mapping. Chords
  // can involve opposite sides; dwell on each relevant view in turn.
  const key=inputs.join('+');if(key!==this.autoKey){this.autoKey=key;this.autoSince=now;}
  const dt=this.lastAuto?Math.min(.1,(now-this.lastAuto)/1000):.016;this.lastAuto=now;
  if(now<this.manualUntil)return;
  this.zoomHand=null;this.closeUp=false;
  const views=[];
  if(inputs.some(t=>/^(a|b|x|y)(?:_touch)?$|^menu$|stick|thumbrest/.test(t)))views.push({azimuth:0,pitch:.06});
  for(const hand of ['left','right']){
   if(inputs.includes(hand+'_grip'))views.push({azimuth:hand==='left'?1.12:-1.12,pitch:.12});
   if(inputs.includes(hand+'_trigger')||inputs.includes(hand+'_trigger_touch'))views.push({azimuth:hand==='left'?2.40:-2.40,pitch:.75});
  }
  if(!views.length)views.push({azimuth:0,pitch:.06});
  this.autoTarget=views[Math.floor((now-this.autoSince)/1800)%views.length];
  const mix=1-Math.exp(-dt*5);this.azimuth=THREE.MathUtils.lerp(this.azimuth,this.autoTarget.azimuth,mix);this.pitch=THREE.MathUtils.lerp(this.pitch,this.autoTarget.pitch,mix);this.distance=THREE.MathUtils.lerp(this.distance,.39,mix);
 }
 setInputs(inputs){
  this.active=inputs;for(const base of this.baseMaterials){base.material.color.copy(base.color);if(base.emissive)base.material.emissive.copy(base.emissive);base.material.emissiveIntensity=1}
  Object.entries(this.markers).forEach(([key,m])=>{m.visible=key.endsWith(':menu_marker');m.material.color.set(0x7c8175);m.material.opacity=.35});
  Object.values(this.directionArrows).forEach(a=>a.visible=false);
  for(const {node,position,quaternion} of this.restTransforms){node.position.copy(position);node.quaternion.copy(quaternion)}
  this.focus=null;const missing=[];
  // Show contact on the physical surface without depressing or tilting it.
  // Draw presses last if a chord contains both contact and press inputs.
  for(const token of [...inputs].sort((a,b)=>Number(b.endsWith('_touch')||b.endsWith('_thumbrest'))-Number(a.endsWith('_touch')||a.endsWith('_thumbrest')))){const target=tokens[token];if(!target){missing.push(token);continue}const [hand,name]=target;const node=this.models[hand]?.getObjectByName(name);
   const contact=token.endsWith('_touch')||token.endsWith('_thumbrest');
   if(!node){missing.push(token);continue}const marker=this.markers[`${hand}:${name}`];if(marker){marker.visible=true;marker.material.color.set(contact?0xe0a92e:0xff3b24);marker.material.opacity=.88}
   if(this.directionArrows[token])this.directionArrows[token].visible=true;
   // Use the asset author's actual pressed/axis endpoint transforms.
   // These illustrate the recorded semantic input, not measured finger motion.
   let stem=contact?null:name.endsWith('_button')?name+'_pressed':name==='squeeze'?'xr_standard_squeeze_pressed':name==='trigger'?'xr_standard_trigger_pressed':token.endsWith('_stick_click')?'xr_standard_thumbstick_pressed':null;
   let endpoint='max';const direction=token.match(/_stick_(up|down|left|right)$/)?.[1];
   if(direction){stem=`xr_standard_thumbstick_${direction==='up'||direction==='down'?'y':'x'}axis_pressed`;endpoint=direction==='up'||direction==='left'?'min':'max'}
   if(stem){const value=this.models[hand].getObjectByName(stem+'_value'),pressed=this.models[hand].getObjectByName(stem+'_'+endpoint);if(value&&pressed){value.position.copy(pressed.position);value.quaternion.copy(pressed.quaternion)}}
   node.traverse(o=>{if(o.isMesh)for(const mat of [].concat(o.material)){mat.color.set(contact?0xe0a92e:0xff5a32);mat.emissive?.set(contact?0xad7814:0xe32f14);mat.emissiveIntensity=.7}});
   if(!this.focus||name==='squeeze'||name==='trigger')this.focus={node,hand,name};
  }
 return missing;
 }
 pick(x,y){
  const box=this.renderer.domElement.getBoundingClientRect(),ray=new THREE.Raycaster();
  ray.setFromCamera(new THREE.Vector2((x-box.left)/box.width*2-1,1-(y-box.top)/box.height*2),this.camera);
  for(const hit of ray.intersectObjects(Object.entries(this.models).filter(([hand])=>this.groups[hand].visible).map(([,model])=>model),true).slice(0,1)){
   for(let node=hit.object;node;node=node.parent){
    const token=Object.entries(tokens).find(([t,[hand,name]])=>!t.match(/_stick_(up|down|left|right)$/)&&node===this.models[hand]?.getObjectByName(name));
    if(token){this.onPick?.(token[0]);return token[0]}
   }
  }
  return null;
 }
 inputScreenPoint(token){
  const [hand,name]=tokens[token]||[];const node=this.models[hand]?.getObjectByName(name);if(!node)return null;
  const box=new THREE.Box3().setFromObject(node),point=box.isEmpty()?node.getWorldPosition(new THREE.Vector3()):box.getCenter(new THREE.Vector3());
  point.project(this.camera);const rect=this.renderer.domElement.getBoundingClientRect();return {x:rect.left+(point.x+1)*rect.width/2,y:rect.top+(1-point.y)*rect.height/2};
 }
 focusControl(closeUp=false){if(!this.focus)return;this.closeUp=closeUp;this.manualUntil=performance.now()+5000;this.zoomHand=this.focus.hand;this.azimuth=this.focus.name==='squeeze'?(this.focus.hand==='left'?1.12:-1.12):this.focus.name==='trigger'?(this.focus.hand==='left'?2.4:-2.4):0;this.pitch=this.focus.name==='trigger'?.75:.06;this.distance=.39;this.render()}
 capture(){this.render();return {controller:this.renderer.domElement.toDataURL('image/png'),detail:this.detailRenderer?.domElement.toDataURL('image/png')||null}}
 render(){
  if(!this.ready)return;
  const canvas=this.renderer.domElement,w=canvas.clientWidth,h=canvas.clientHeight;if(!w||!h)return;
  this.renderer.setSize(w,h,false);this.camera.aspect=w/h;
  const bounds=new THREE.Box3();for(const [hand,model] of Object.entries(this.models)){this.groups[hand].visible=!this.zoomHand||this.zoomHand===hand;if(this.groups[hand].visible)bounds.union(new THREE.Box3().setFromObject(model.getObjectByName('controller_mesh')));}
  for(const {sprite,node,hand} of this.letterLabels){sprite.visible=this.groups[hand].visible;const box=new THREE.Box3().setFromObject(node);if(box.isEmpty()){node.getWorldPosition(sprite.position);sprite.position.z+=.001}else{box.getCenter(sprite.position);sprite.position.z=box.max.z+.0005}}
  const center=bounds.getCenter(new THREE.Vector3()),direction=new THREE.Vector3(Math.sin(this.azimuth),this.pitch,Math.cos(this.azimuth)).normalize();
  const right=new THREE.Vector3().crossVectors(new THREE.Vector3(0,1,0),direction).normalize(),up=new THREE.Vector3().crossVectors(direction,right);
  const tangent=Math.tan(THREE.MathUtils.degToRad(this.camera.fov/2));let fit=0;
  for(const x of [bounds.min.x,bounds.max.x])for(const y of [bounds.min.y,bounds.max.y])for(const z of [bounds.min.z,bounds.max.z]){
   const point=new THREE.Vector3(x,y,z).sub(center);fit=Math.max(fit,Math.abs(point.dot(right))/(tangent*this.camera.aspect)+point.dot(direction),Math.abs(point.dot(up))/tangent+point.dot(direction));
  }
  let distance=fit*1.1*(this.distance/.39);
  if(this.closeUp&&this.focus){const box=new THREE.Box3().setFromObject(this.focus.node);if(box.isEmpty())this.focus.node.getWorldPosition(center);else box.getCenter(center);distance=.115*(this.distance/.39);}
  this.camera.position.copy(center).addScaledVector(direction,distance);this.camera.lookAt(center);this.camera.updateProjectionMatrix();this.renderer.render(this.scene,this.camera);
  if(!this.detailRenderer)return;
  const mini=this.detailRenderer.domElement,mw=mini.clientWidth,mh=mini.clientHeight;if(!mw||!mh)return;this.detailRenderer.setSize(mw,mh,false);this.detailCamera.aspect=mw/mh;
  const focus=this.focus||{node:this.models.left.getObjectByName('y_button'),hand:'left',name:'y_button'};
  const point=focus.node.isMesh?new THREE.Box3().setFromObject(focus.node).getCenter(new THREE.Vector3()):focus.node.getWorldPosition(new THREE.Vector3());
  let offset=new THREE.Vector3(0,.035,.13);
  if(focus.name==='squeeze')offset.set(focus.hand==='left'?.13:-.13,.015,.055);
  if(focus.name==='trigger')offset.set(focus.hand==='left'?-.035:.035,.10,-.09);
  this.detailCamera.position.copy(point).add(offset);this.detailCamera.lookAt(point);this.detailCamera.updateProjectionMatrix();
  const otherHand=focus.hand==='left'?'right':'left',other=this.groups[otherHand],hiddenArrows=Object.entries(this.directionArrows).filter(([token,arrow])=>token.startsWith(otherHand+'_')&&arrow.visible);
  other.visible=false;hiddenArrows.forEach(([,arrow])=>arrow.visible=false);
  this.detailRenderer.render(this.scene,this.detailCamera);
  other.visible=true;hiddenArrows.forEach(([,arrow])=>arrow.visible=true);
 }
}
