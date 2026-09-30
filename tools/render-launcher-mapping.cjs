// Export the launcher's saved-mapping tour without opening or controlling a desktop app.
const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),http=require('http'),cp=require('child_process'),crypto=require('crypto'),os=require('os');
const {once}=require('events');
const root=path.resolve(__dirname,'..'),ui=path.join(root,'build/Release/launcher-ui');
const output=path.join(root,'artifacts/dev');fs.mkdirSync(output,{recursive:true});
const config=JSON.parse(fs.readFileSync(path.join(root,'private/workspace.json'),'utf8'));
const gameExe=path.join(config.game_dir,'mgsvtpp.exe'),controls=path.join(config.game_dir,'mgs5vr-controls.ini');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const controlsBefore=sha(controls),request=path.join(os.tmpdir(),'mgs5vr-map-'+crypto.randomUUID()+'.json');
fs.writeFileSync(request,JSON.stringify({method:'load',gameExe}));
let saved;
try{saved=JSON.parse(cp.execFileSync('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',path.join(root,'tools/launcher-bridge.ps1'),'-RequestPath',request],{encoding:'utf8',windowsHide:true}));}
finally{fs.unlinkSync(request);}
const server=http.createServer((req,res)=>{
 try{if(req.method!=='GET'){res.writeHead(405);res.end();return;}
  const pathname=decodeURIComponent(new URL(req.url,'http://localhost').pathname),file=path.resolve(ui,'.'+pathname);
  if(file!==ui&&!file.startsWith(ui+path.sep)){res.writeHead(403);res.end();return;}
  const target=fs.statSync(file).isDirectory()?path.join(file,'index.html'):file;
  const mime={'.html':'text/html','.js':'text/javascript','.json':'application/json','.css':'text/css','.svg':'image/svg+xml','.png':'image/png','.glb':'model/gltf-binary','.mp4':'video/mp4'};
  res.writeHead(200,{'Content-Type':mime[path.extname(target)]||'application/octet-stream','Cache-Control':'no-store'});fs.createReadStream(target).pipe(res);
 }catch{res.writeHead(404);res.end();}
});
function assert(value,message){if(!value)throw Error(message);}
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 let encoder;
 try{
  const page=await browser.newPage({viewport:{width:1440,height:960}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('mappingBridge',method=>{assert(method==='load','Mapping export may only read settings');return saved;});
  await page.addInitScript(()=>window.__testBridge=(method,payload)=>window.mappingBridge(method,payload));
  await page.goto('http://127.0.0.1:'+server.address().port+'/',{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.launcherQA?.ready,{timeout:30000});
  await page.evaluate(()=>window.launcherQA.startTour());
  const steps=await page.evaluate(()=>window.launcherQA.getTour());
  const touches=steps.filter(s=>s.kind==='touch').flatMap(s=>s.inputs);
  assert(new Set(touches).size===10,'The tour must include all ten capacitive contacts');
  for(const token of touches){const value=await page.evaluate(t=>window.launcherQA.inspectInputs([t]),token);assert(!value.missing.length,'Missing physical surface: '+token);assert(!value.changedTransforms.length,'Contact depressed a control: '+token);}
  const press=await page.evaluate(()=>window.launcherQA.inspectInputs(['a']));assert(press.changedTransforms.length>0,'Pressed face button did not depress');
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Mapping tour overflows');
  const duration=steps.at(-1).start+steps.at(-1).seconds,fps=15,frames=Math.ceil(duration*fps);
  const video=path.join(output,'controller-mapping.mp4');
  encoder=cp.spawn('ffmpeg',['-hide_banner','-loglevel','error','-y','-f','image2pipe','-framerate',String(fps),'-vcodec','png','-i','pipe:0','-an','-c:v','libx264','-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',video],{windowsHide:true,stdio:['pipe','ignore','pipe']});
  let encoderError='';encoder.stderr.on('data',chunk=>encoderError=(encoderError+chunk).slice(-4000));
  const encoderDone=once(encoder,'close');
  for(let n=0;n<frames;n++){
   await page.evaluate(t=>window.launcherQA.seekTour(t,true),n/fps);
   const pixels=await page.screenshot({animations:'disabled'});
   if(!encoder.stdin.write(pixels))await once(encoder.stdin,'drain');
   if(n%150===0)console.log('Mapping video '+Math.round(100*n/frames)+'%');
  }
  encoder.stdin.end();const [code]=await encoderDone;assert(code===0,'Video export failed: '+encoderError);
  const reload=steps.find(s=>s.name==='gameplay.reload');await page.evaluate(t=>window.launcherQA.seekTour(t,true),reload.start+2.2);
  const poster=path.join(output,'controller-mapping.png');await page.screenshot({path:poster});
  const probe=JSON.parse(cp.execFileSync('ffprobe',['-v','error','-show_entries','stream=codec_type,duration,width,height,nb_frames:format=duration','-of','json',video],{encoding:'utf8',windowsHide:true}));
  const stream=probe.streams.find(s=>s.codec_type==='video');assert(stream.width===1440&&stream.height===960&&Math.abs(Number(stream.duration)-frames/fps)<.1,'Unexpected video size or duration');
  assert(!errors.length,'Launcher errors: '+errors.join('; '));assert(sha(controls)===controlsBefore,'Mapping export changed personal controls');
  const manifest={schema:1,source:'launcher_saved_mapping_tour',illustrative_controllers:true,gameplay_capture:false,controls_sha256:controlsBefore,video_sha256:sha(video),duration_seconds:frames/fps,fps,steps,touch_inputs:touches,contact_does_not_depress_controls:true,personal_controls_unchanged:true,browser_errors:errors,probe};
  fs.writeFileSync(path.join(output,'controller-mapping.json'),JSON.stringify(manifest,null,2));
  console.log(JSON.stringify({video,poster,duration:frames/fps,contacts:touches.length,personalControlsPreserved:true}));
 }finally{if(encoder&&encoder.exitCode===null)encoder.kill();await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{server.close();console.error(error);process.exitCode=1;});
