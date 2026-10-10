// Render the same saved-binding PNG guides offered by the launcher. This is
// offline artifact rendering: no desktop/window automation or game inputs.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const cp=require('node:child_process'),crypto=require('node:crypto');
const {parseArgs}=require('node:util');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'..');
const {values}=parseArgs({options:{controls:{type:'string'},checker:{type:'string',default:path.join(root,'build/dev/mgs5vr_controls.exe')},output:{type:'string',default:path.join(root,'docs/control-guides')},browser:{type:'string',default:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'}},strict:true});
if(!values.controls)throw Error('Pass --controls with the personal mgs5vr-controls.ini to export.');
const controls=path.resolve(values.controls),checker=path.resolve(values.checker),output=path.resolve(values.output);
const sha=data=>crypto.createHash('sha256').update(data).digest('hex');
const controlsBytes=fs.readFileSync(controls),revision=sha(controlsBytes),checkerRevision=sha(fs.readFileSync(checker));
// This read-only export executes the runtime's parser, including defaults,
// personal remaps, disabled actions, alternatives and version migrations.
const bindings=JSON.parse(cp.execFileSync(checker,['--bindings-json',controls],{encoding:'utf8',windowsHide:true}));
if(!Array.isArray(bindings.actions)||!Array.isArray(bindings.axes))throw Error('The checker did not return an effective binding catalog.');
if(sha(fs.readFileSync(controls))!==revision||sha(fs.readFileSync(checker))!==checkerRevision)throw Error('The controls or checker changed during export; run the exporter again after the update.');
const ui=path.join(root,'tools/launcher-ui');
const html=`<!doctype html><meta charset="utf-8"><style>body{margin:0}canvas{display:block;width:1240px;height:1754px}</style><canvas id="guide"></canvas>
<script type="importmap">{"imports":{"three":"/vendor/three-r180/build/three.module.js"}}</script>
<script type="module">
import {controlGuidePages,drawControlGuide} from "/control-guide.js";
window.guideRender=async(bindings,index,metadata)=>{
 const pages=controlGuidePages(bindings);
 const result=await drawControlGuide(document.querySelector("canvas"),pages[index],metadata);
 return {...result,pages:pages.map(p=>{
  const rows=p.columns.flat().flatMap(s=>s.rows);
  return {id:p.id,title:p.title,actions:[...new Set(rows.flatMap(r=>r.actions||[]))],axes:[...new Set(rows.flatMap(r=>r.axes||[]))]};
 })};
};
window.guideReady=true;
</script>`;
const field=path.join(root,'tools/field-guide');
const server=http.createServer((req,res)=>{
 const route=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname);
 if(route==='/'){res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});return res.end(html);}
 let base=ui,relative=route.slice(1);
 if(route==='/controller-player.js'){base=field;relative='controller-player.js';}
 else if(route==='/assets/field-header.svg'){base=path.join(root,'docs/images');relative='field-header.svg';}
 else if(route.startsWith('/assets/')||route.startsWith('/vendor/'))base=field;
 const file=path.resolve(base,relative);
 if(!file.startsWith(path.resolve(base)+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404);return res.end();}
 const mime={'.js':'text/javascript; charset=utf-8','.svg':'image/svg+xml','.json':'application/json','.glb':'model/gltf-binary','.png':'image/png'}[path.extname(file)]||'application/octet-stream';
 res.writeHead(200,{'Content-Type':mime});fs.createReadStream(file).pipe(res);
});
(async()=>{
 let browser;
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 try{
  fs.mkdirSync(output,{recursive:true});
  browser=await chromium.launch({executablePath:values.browser,headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
  const page=await browser.newPage({viewport:{width:1240,height:1754},deviceScaleFactor:1});
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:'+server.address().port+'/',{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.guideReady,null,{timeout:10000});
  const files=[];let pages;
  for(let index=0;index<4;index++){
   const result=await page.evaluate(({bindings,index,revision})=>window.guideRender(bindings,index,{revision}),{bindings,index,revision});
   if(errors.length)throw Error(errors.join('\n'));
   const bytes=Buffer.from(await page.evaluate(()=>document.querySelector('canvas').toDataURL('image/png').split(',')[1]),'base64');
   const name='MGS5VR-'+result.id+'.png';fs.writeFileSync(path.join(output,name),bytes);pages=result.pages;
   files.push({name,width:result.width,height:result.height,content_scale:result.contentScale,sha256:sha(bytes)});
  }
  const bindingJson=JSON.stringify(bindings,null,2)+'\n';fs.writeFileSync(path.join(output,'effective-bindings.json'),bindingJson);
  const artwork=['docs/images/field-header.svg','tools/launcher-ui/paper-grain.svg','tools/field-guide/controller-player.js','tools/field-guide/assets/meta-quest-touch-plus-left.glb','tools/field-guide/assets/meta-quest-touch-plus-right.glb','tools/field-guide/assets/controller-spatial-anchors.json'];
  const manifest={schema:2,source:'native effective bindings export',visual_style:'MGS5VR Field Terminal; original header artwork and actual 3D controller models',controls_file:path.basename(controls),controls_sha256:revision,checker_sha256:checkerRevision,bindings_sha256:sha(bindingJson),renderer_sha256:sha(fs.readFileSync(path.join(ui,'control-guide.js'))),artwork_sha256:Object.fromEntries(artwork.map(name=>[name,sha(fs.readFileSync(path.join(root,name)))])),pages,files};
  fs.writeFileSync(path.join(output,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
  console.log(JSON.stringify({output,controls_sha256:revision,files},null,2));
 }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error.stack||error.message);process.exitCode=1;});
