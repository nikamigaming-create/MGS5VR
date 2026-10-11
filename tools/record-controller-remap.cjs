// Record real browser interactions with the shipping launcher HTML and native
// settings bridge. A disposable profile owns all edits. No desktop automation.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const cp=require('node:child_process'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'..'),out=path.join(root,'artifacts/dev/controller-mapping');
const fixture=path.join(out,'editor-demonstration'),ui=path.join(root,'build/Release/launcher-ui');
const personal=path.join(JSON.parse(fs.readFileSync(path.join(root,'private/workspace.json'),'utf8')).game_dir,'mgs5vr-controls.ini');
const sha=f=>crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex'),before=sha(personal);
fs.mkdirSync(fixture,{recursive:true});
fs.copyFileSync(path.join(out,'original-controls.ini'),path.join(fixture,'mgs5vr-controls.ini'));
fs.copyFileSync(path.join(root,'config/mgs5vr.ini'),path.join(fixture,'mgs5vr.ini'));
fs.writeFileSync(path.join(fixture,'mgsvtpp.exe'),'not a game; never launched');fs.writeFileSync(path.join(fixture,'mgs5vr-install.json'),'{}');
const server=http.createServer((req,res)=>{
 try{const file=path.resolve(ui,'.'+new URL(req.url,'http://localhost').pathname),target=fs.statSync(file).isDirectory()?path.join(file,'index.html'):file;
 if(req.method!=='GET'||!target.startsWith(ui+path.sep)){res.writeHead(403);return res.end();}
 res.writeHead(200,{'Content-Type':({'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.png':'image/png','.svg':'image/svg+xml','.glb':'model/gltf-binary'})[path.extname(target)]||'application/octet-stream'});fs.createReadStream(target).pipe(res);
 }catch{res.writeHead(404);res.end();}
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 let page,video;
 try{
  const context=await browser.newContext({viewport:{width:1440,height:960},recordVideo:{dir:path.join(fixture,'recorded'),size:{width:1440,height:960}}});
  page=await context.newPage();video=page.video();
  await page.exposeFunction('demoBridge',(method,payload)=>{
   if(!['load','saveControls'].includes(method))throw Error('Recording cannot launch a game or edit runtime settings');
   const request=path.join(fixture,'request.json');fs.writeFileSync(request,JSON.stringify({...payload,method,gameExe:path.join(fixture,'mgsvtpp.exe')}));
   try{return {...JSON.parse(cp.execFileSync('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',path.join(root,'tools/launcher-bridge.ps1'),'-RequestPath',request],{encoding:'utf8',windowsHide:true})),gameRunning:false};}finally{fs.unlinkSync(request);}
  });
  await page.addInitScript(()=>window.__testBridge=(m,p)=>window.demoBridge(m,p));
  await page.goto('http://127.0.0.1:'+server.address().port+'/',{waitUntil:'networkidle'});await page.waitForFunction(()=>window.launcherQA?.ready);
  await page.evaluate(()=>{
   const ring=document.createElement('div');ring.id='tutorial-pointer';ring.style.cssText='position:fixed;pointer-events:none;z-index:99999;width:28px;height:28px;margin:-14px;border:3px solid #d9422b;border-radius:50%;background:#d9422b22;box-shadow:0 0 0 5px #fff8;';document.body.append(ring);
   addEventListener('pointermove',e=>{ring.style.left=e.clientX+'px';ring.style.top=e.clientY+'px'});
  });
  const steps=[],started=Date.now();
  const dwell=async label=>{steps.push({seconds:(Date.now()-started)/1000,label});await page.waitForTimeout(1800)};
  const click=async selector=>{const node=page.locator(selector).first();await node.scrollIntoViewIfNeeded();const box=await node.boundingBox();await page.mouse.move(box.x+box.width/2,box.y+box.height/2,{steps:14});await node.click();};
  await click('nav [data-tab="controls"]');await dwell('Open Controller bindings');
  await page.locator('#action-search').fill('ready');await page.locator('#action-select').selectOption('gameplay.ready_weapon');await click('[data-view="right"]');await dwell('Select weapon ready; keep its right-grip option');
  await click('#add-alternative');await dwell('Add an alternative');
  await page.locator('#contact-kind').selectOption('touch');await dwell('Choose Light touch');
  await click('[data-row="1"][data-token="right_trigger_touch"]');await dwell('Add right trigger touch to the new option');
  await click('[data-row="1"][data-token="a"]');await dwell('Remove placeholder A');
  await page.locator('#gesture-1').selectOption('level');await dwell('Choose While held');
  await click('#save');await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);await dwell('Save through the actual validator');
  if(!fs.readFileSync(path.join(fixture,'mgs5vr-controls.ini'),'utf8').includes('right_grip | right_trigger_touch'))throw Error('Recorded remap did not save');
  await click('#refresh-bindings');await dwell('Reload the saved mapping');
  await click('#training [data-quick-tour]');await page.waitForFunction(()=>document.querySelector('#tour-view').hidden===false);
  const touch=await page.evaluate(()=>window.launcherQA.getTour().find(s=>s.name==='gameplay.ready_weapon'&&s.inputs.includes('right_trigger_touch')));
  await page.evaluate(t=>window.launcherQA.seekTour(t,true),touch.start+.6);await dwell('The saved tour resolves the new touch alternative');
  if(sha(personal)!==before)throw Error('Recording changed personal settings');
  await page.close();await context.close();await video.saveAs(path.join(out,'controller-remap-browser.webm'));
  cp.execFileSync('ffmpeg',['-hide_banner','-loglevel','error','-y','-i',path.join(out,'controller-remap-browser.webm'),'-c:v','libx264','-preset','fast','-crf','19','-pix_fmt','yuv420p','-movflags','+faststart',path.join(out,'controller-remap-browser.mp4')],{windowsHide:true});
  fs.writeFileSync(path.join(out,'controller-remap-browser.json'),JSON.stringify({schema:1,source:'actual headless browser rendering of launcher HTML, with an added pointer ring',desktop_capture:false,gameplay_capture:false,fixture_only:true,personal_controls_unchanged:true,steps,video_sha256:sha(path.join(out,'controller-remap-browser.mp4'))},null,2));
  console.log(JSON.stringify({video:path.join(out,'controller-remap-browser.mp4'),steps:steps.length}));
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{server.close();console.error(e);process.exitCode=1});
