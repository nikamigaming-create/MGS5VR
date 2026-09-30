import * as THREE from 'three';

// A small physical cassette rack. Labels identify recordings; playback resolves
// controller inputs separately from the saved bindings.
export class CassetteDeck {
 constructor(canvas,lessons,onSelect) {
  this.canvas=canvas; this.lessons=lessons; this.onSelect=onSelect; this.angle=-.22;
  this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:true});
  this.renderer.setPixelRatio(Math.min(devicePixelRatio,2));
  this.renderer.toneMapping=THREE.ACESFilmicToneMapping; this.renderer.toneMappingExposure=1.05;
  this.scene=new THREE.Scene(); this.camera=new THREE.PerspectiveCamera(34,1,.1,50);
  this.scene.add(new THREE.HemisphereLight(0xfff5de,0x48463d,2.0));
  const key=new THREE.DirectionalLight(0xfff7e8,3.0); key.position.set(-3,5,7); this.scene.add(key);
  const edge=new THREE.DirectionalLight(0xbec9d1,1.7); edge.position.set(4,1,-2); this.scene.add(edge);
  this.stack=new THREE.Group(); this.scene.add(this.stack); this.tapes=[];
  lessons.slice(0,3).reverse().forEach((lesson,i)=>{
   const tape=this.makeTape(lesson,lessons.indexOf(lesson));
   tape.position.set((i-1)*.26,(i-1)*.23,(i-1)*.29);
   tape.rotation.set(-.14,0,(i-1)*-.085);
   this.stack.add(tape); this.tapes.push(tape);
  });
  let start=null,last=null;
  canvas.addEventListener('pointerdown',e=>{start=[e.clientX,e.clientY];last=start;canvas.setPointerCapture(e.pointerId)});
  canvas.addEventListener('pointermove',e=>{if(!last)return;this.angle+=(e.clientX-last[0])*.007;last=[e.clientX,e.clientY];this.render()});
  canvas.addEventListener('pointerup',e=>{
   if(start&&Math.hypot(e.clientX-start[0],e.clientY-start[1])<5){
    const r=canvas.getBoundingClientRect(),ray=new THREE.Raycaster();
    ray.setFromCamera(new THREE.Vector2((e.clientX-r.left)/r.width*2-1,1-(e.clientY-r.top)/r.height*2),this.camera);
    const hit=ray.intersectObjects(this.tapes,true)[0];
    if(hit){let n=hit.object;while(n&&!n.userData.lesson)n=n.parent;if(n)this.onSelect(n.userData.lesson)}
   }start=last=null;
  });
  canvas.addEventListener('keydown',e=>{
   if(e.key==='Enter'&&lessons.length)this.onSelect(lessons[0].id);
   if(e.key==='ArrowLeft'||e.key==='ArrowRight'){e.preventDefault();this.angle+=e.key==='ArrowLeft'?-.15:.15;this.render()}
  });
  canvas.addEventListener('pointercancel',()=>start=last=null);
  new ResizeObserver(()=>this.render()).observe(canvas);this.render();
 }
 makeTape(lesson,index) {
  const group=new THREE.Group();group.userData.lesson=lesson.id;
  const shell=new THREE.MeshStandardMaterial({color:index%2?0x30322e:0x242623,roughness:.52,metalness:.08});
  const dark=new THREE.MeshStandardMaterial({color:0x101211,roughness:.75});
  const steel=new THREE.MeshStandardMaterial({color:0xb1b1a5,metalness:.8,roughness:.35});
  const bone=new THREE.MeshStandardMaterial({color:0xd6cfb9,roughness:.6});
  const tapeMat=new THREE.MeshStandardMaterial({color:0x392e25,roughness:.64,metalness:.1});
  const box=(w,h,d,x,y,z,mat)=>{const m=new THREE.Mesh(new THREE.BoxGeometry(w,h,d),mat);m.position.set(x,y,z);group.add(m);return m};
  const disc=(radius,x,y,z,mat,segments=64)=>{const m=new THREE.Mesh(new THREE.CylinderGeometry(radius,radius,.025,segments),mat);m.rotation.x=Math.PI/2;m.position.set(x,y,z);group.add(m);return m};
  const ring=(outer,inner,x,y,z,mat)=>{const m=new THREE.Mesh(new THREE.RingGeometry(inner,outer,64),mat);m.position.set(x,y,z);group.add(m);return m};
  const outline=new THREE.Shape();outline.moveTo(-1.84,-1.18);outline.lineTo(1.84,-1.18);outline.lineTo(1.97,-1.05);outline.lineTo(1.97,1.05);outline.lineTo(1.84,1.18);outline.lineTo(-1.84,1.18);outline.lineTo(-1.97,1.05);outline.lineTo(-1.97,-1.05);outline.closePath();
  const body=new THREE.Mesh(new THREE.ExtrudeGeometry(outline,{depth:.25,bevelEnabled:true,bevelSize:.035,bevelThickness:.025,bevelSegments:3,steps:1}),shell);body.position.z=-.14;group.add(body);
  // Moulded edge, recessed tape window, wound tape and six-tooth reel hubs.
  box(3.66,2.08,.025,0,0,.135,shell);
  box(2.95,.88,.032,0,-.06,.16,dark);
  for(const x of [-.88,.88]) {
   disc(x<0?.397:.352,x,-.06,.191,tapeMat);
   for(const radius of [.28,.31,.34,.37])if(x<0||radius<.35)ring(radius,radius-.008,x,-.06,.209,new THREE.MeshStandardMaterial({color:0x534134,roughness:.75}));
   disc(.207,x,-.06,.217,bone);disc(.121,x,-.06,.235,dark,24);
   for(let t=0;t<6;t++) {
    const a=t*Math.PI/3,m=box(.052,.067,.024,x+Math.sin(a)*.122,-.06+Math.cos(a)*.122,.254,bone);m.rotation.z=-a;
   }
  }
  box(.65,.39,.008,0,-.06,.199,new THREE.MeshStandardMaterial({color:0x737369,transparent:true,opacity:.38,roughness:.22}));
  for(let i=-4;i<=4;i++)box(.012,i===0?.18:.11,.009,i*.052,-.06,.215,bone);
  box(2.82,.028,.025,0,-.30,.22,tapeMat);
  const label=document.createElement('canvas');label.width=1200;label.height=230;
  const ctx=label.getContext('2d');ctx.fillStyle='#e7dfc9';ctx.fillRect(0,0,1200,230);
  ctx.fillStyle='#a42b26';ctx.fillRect(0,0,1200,13);
  ctx.strokeStyle='#4c4b41';ctx.lineWidth=2;ctx.strokeRect(15,23,1170,194);
  ctx.fillStyle='#282923';ctx.font='22px Consolas,monospace';ctx.fillText('MGS5VR / TRAINING',35,64);
  ctx.font='bold 48px Bahnschrift, Arial, sans-serif';let size=48;const title=lesson.title.toUpperCase();while(ctx.measureText(title).width>1010){ctx.font=`bold ${--size}px Bahnschrift, Arial, sans-serif`}ctx.fillText(title,35,133);
  ctx.font='23px Consolas,monospace';ctx.fillText('SIDE A',35,185);ctx.fillText(String(index+1).padStart(2,'0'),1078,185);
  const texture=new THREE.CanvasTexture(label);texture.colorSpace=THREE.SRGBColorSpace;
  box(3.55,.69,.009,0,.73,.158,new THREE.MeshStandardMaterial({map:texture,roughness:.95}));
  box(3.1,.21,.009,0,-.68,.158,new THREE.MeshStandardMaterial({color:0xe4dcc7,roughness:.9}));
  const lipShape=new THREE.Shape();lipShape.moveTo(-1.50,-1.14);lipShape.lineTo(1.50,-1.14);lipShape.lineTo(1.28,-.85);lipShape.lineTo(-1.28,-.85);lipShape.closePath();
  const lip=new THREE.Mesh(new THREE.ExtrudeGeometry(lipShape,{depth:.06,bevelEnabled:true,bevelThickness:.012,bevelSize:.016,bevelSegments:2,steps:1}),shell);lip.position.z=.14;group.add(lip);
  for(const x of [-1.02,-.70,0,.70,1.02])disc(x===0?.075:.054,x,-1.015,.233,dark,32);
  for(const x of [-1.76,1.76])for(const y of [-.98,.98]){disc(.052,x,y,.178,steel,24);box(.052,.01,.009,x,y,.197,dark)}
  for(const x of [-1.82,1.82])for(let y=-.45;y<.4;y+=.065)box(.095,.019,.009,x,y,.165,dark);
  // Real cassettes also have a labelled rear face, so rotating never reveals an
  // untextured box. The rear shares the recording title.
  const rear=new THREE.Mesh(new THREE.PlaneGeometry(3.55,.69),new THREE.MeshStandardMaterial({map:texture,roughness:.95}));rear.rotation.y=Math.PI;rear.position.set(0,.73,-.172);group.add(rear);
  return group;
 }
 render() {
  const w=this.canvas.clientWidth,h=this.canvas.clientHeight;if(!w||!h)return;
  this.renderer.setSize(w,h,false);this.camera.aspect=w/h;
  this.camera.position.set(0,.95,Math.max(6.0,4.8/(2*Math.tan(17*Math.PI/180)*this.camera.aspect)));
  this.camera.lookAt(0,.05,0);this.camera.updateProjectionMatrix();this.stack.rotation.y=this.angle;
  this.renderer.render(this.scene,this.camera);
 }
}
