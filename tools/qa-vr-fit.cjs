// Headless launcher QA, using the real validator and a disposable game fixture.
// No game, desktop window, controller or user's settings are operated.
const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),output=path.resolve(process.argv[2]||'artifacts/dev/vr-fit-qa');
const fixture=path.join(output,'fixture');fs.mkdirSync(fixture,{recursive:true});
for(const file of ['mgs5vr-controls.ini','mgs5vr.ini'])fs.copyFileSync(path.join(root,'config',file),path.join(fixture,file));
fs.writeFileSync(path.join(fixture,'mgsvtpp.exe'),'settings fixture, never launched');fs.writeFileSync(path.join(fixture,'mgs5vr-install.json'),'{}');
let requestNumber=0;const checks=[];
function assert(value,label){if(!value)throw Error(label);checks.push(label);}
function near(a,b,tolerance=1e-6){return a.length===b.length&&a.every((v,i)=>Math.abs(v-b[i])<=tolerance);}
function hash(buffer){return crypto.createHash('sha256').update(buffer).digest('hex');}
async function bridge(method,payload){
 if(!['load','saveControls','saveRuntime'].includes(method))throw Error('Fixture refuses game or installation operations');
 const request=path.join(fixture,`request-${++requestNumber}.json`);fs.writeFileSync(request,JSON.stringify({...payload,method,gameExe:path.join(fixture,'mgsvtpp.exe')}));
 try{return {...JSON.parse(cp.execFileSync('powershell.exe',['-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',path.join(root,'tools/launcher-bridge.ps1'),'-RequestPath',request],{encoding:'utf8',windowsHide:true})),gameRunning:false};}
 finally{fs.unlinkSync(request);}
}
(async()=>{
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:960}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('fixtureBridge',bridge);await page.addInitScript(()=>window.__testBridge=(method,payload)=>window.fixtureBridge(method,payload));
  await page.goto('http://127.0.0.1:8766/build/Release/launcher-ui/',{waitUntil:'networkidle'});await page.waitForFunction(()=>window.launcherQA?.ready);
  await page.evaluate(()=>window.launcherQA.selectTab('settings'));const canvas=page.locator('#fit-canvas');
  const snap=()=>page.evaluate(()=>window.launcherQA.fitSnapshot());
  const edit=async(key,value)=>{const input=page.locator(`[data-number="settings.${key}"]`);await input.fill(String(value));await input.dispatchEvent('input');};
  const baseline=await snap();assert(baseline.context==='idroid','3D fitting view opens on right-hand iDroid');
  assert(Number(await page.locator('[data-setting="settings.wrist_text_scale"]').inputValue())===1.5,'Fractional arm text scale is shown accurately by its slider');
  assert(await page.evaluate(()=>window.launcherQA.state.settings.every(r=>window.launcherQA.fitContext(r.name))),'All 53 VR adjustment rows belong to a 3D preview');
  await page.locator('[data-fit-context="idroid"]').click();await page.locator('#fit-filter').click();
  assert(await page.locator('[data-number*="idroid_grip_"]').count()===6,'All grip positions and rotations are surfaced');
  await page.locator('[data-setting="settings.handheld_menus"]').check();
  const imageBefore=hash(await canvas.screenshot());await edit('idroid_screen_width_cm',60);const wider=await snap();
  assert(near(wider.lowerEdge,baseline.lowerEdge)&&Math.abs(wider.screenSize[0]-.6)<1e-6,'Resizing preserves emitter-side lower edge');
  assert(Math.abs(wider.screen[1]-wider.lowerEdge[1]-.6*9/32)<1e-6,'16:9 screen rises above its lower edge');
  assert(hash(await canvas.screenshot())!==imageBefore,'Unsaved width adjustment redraws actual 3D pixels');
  await edit('idroid_screen_pitch_degrees',20);await edit('idroid_screen_yaw_degrees',15);await edit('idroid_screen_roll_degrees',-12);const angled=await snap();
  assert(near(angled.lowerEdge,baseline.lowerEdge)&&!near(angled.screenRotation,baseline.screenRotation),'All screen rotations pivot at the lower edge');
  await edit('idroid_grip_x_cm',3.5);await edit('idroid_grip_y_cm',-2);await edit('idroid_grip_z_cm',4);const shifted=await snap();
  assert(near(shifted.device,[.035,-.02,.04]),'Grip translations use metres and the documented controller axes');
  assert(near(shifted.hand.map((v,i)=>v-angled.hand[i]),[.035,-.02,.04]),'Hand and device translate together');
  await edit('idroid_grip_pitch_degrees',10);await edit('idroid_grip_yaw_degrees',-25);await edit('idroid_grip_roll_degrees',30);
  const fitted=await snap();assert(!near(fitted.emitter,shifted.emitter)&&!near(fitted.hand,shifted.hand),'Grip rotations move hand, emitter and projection together');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);await page.evaluate(()=>window.launcherQA.refresh(true));
  assert(near((await snap()).screen,fitted.screen),'Saved fit survives a real parser and bridge reload');
  const ini=fs.readFileSync(path.join(fixture,'mgs5vr-controls.ini'),'utf8');assert(ini.includes('idroid_grip_x_cm = 3.5')&&ini.includes('idroid_screen_roll_degrees = -12'),'Fractional position and rotation persist in INI');
  await edit('idroid_screen_depth_cm',15);await page.locator('#discard').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  assert(near((await snap()).screen,fitted.screen),'Discard restores both saved controls and preview');
  await page.locator('#fit-reset').click();const reset=await snap();assert(near(reset.screen,baseline.screen)&&near(reset.hand,baseline.hand),'Reset fit restores complete grip and projection defaults');
  assert(await page.locator('[data-setting="settings.handheld_menus"]').isChecked(),'Reset fit preserves the existing handheld interaction mode');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  const cameraBefore=await snap(),box=await canvas.boundingBox();await page.mouse.move(box.x+box.width*.4,box.y+box.height*.5);await page.mouse.down();await page.mouse.move(box.x+box.width*.7,box.y+box.height*.65,{steps:8});await page.mouse.up();
  assert(Math.abs((await snap()).azimuth-cameraBefore.azimuth)>.3,'Pointer drag rotates the 3D fitting camera');await page.mouse.wheel(0,-180);assert((await snap()).zoom<1,'Wheel zooms the fitting view');
  await page.locator('[data-fit-view="side"]').click();assert(Math.abs((await snap()).azimuth-Math.PI/2)<1e-6,'Side view exposes projector depth and intersection');
  await page.screenshot({path:path.join(output,'idroid-side.png'),fullPage:false});
  await page.locator('[data-fit-view="front"]').first().click();await page.screenshot({path:path.join(output,'idroid-front.png'),fullPage:false});
  const contexts=['wrist','hands','optics','menus','movement'];const views={};
  for(const context of contexts){await page.locator(`[data-fit-context="${context}"]`).click();assert(await page.locator('#settings-grid .setting-card').count()>0,context+' has fitting controls');views[context]=await snap();await page.screenshot({path:path.join(output,context+'.png'),fullPage:false});}
  await page.locator('[data-fit-context="wrist"]').click();const left=await snap();await edit('weapon_hud_setback_cm',9);const arm=await snap();assert(!near(arm.status,left.status)&&near(arm.device,left.device),'Weapon info adjustment affects LEFT forearm and preserves iDroid fit');
  await page.locator('#discard').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  await page.locator('[data-fit-context="optics"]').click();const optic=await snap();await edit('scope_eye_relief_cm',15);assert(Math.abs((await snap()).scopeEye[2]-optic.scopeEye[2]-.05)<1e-6,'Scope eye distance updates physical clearance preview');
  await page.locator('#discard').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  await page.locator('[data-fit-context="movement"]').click();await edit('player_height_offset_cm',25);assert(Math.abs((await snap()).eyeHeight-1.65)<1e-6,'Height adjustment moves the eye without scaling the world');
  await page.locator('#discard').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  await page.locator('#fit-filter').click();assert(await page.locator('#settings-grid .setting-card').count()===53,'Show all restores every adjustment');
  await page.locator('[data-settings="runtime"]').click();await page.locator('[data-fit-context="menus"]').click();assert(await page.locator('[data-number="theatre.width_cm"]').count()===1,'Runtime theatre fit is surfaced');
  await page.locator('[data-setting="diagnostics.wrist_hud_experiment"]').uncheck();assert((await snap()).offWrist,'Explicit off-wrist preference previews a spatial panel');
  await page.locator('#discard').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);assert(!(await snap()).offWrist,'Discard restores left-arm presentation');
  await page.locator('[data-settings="controls"]').click();await page.locator('[data-fit-context="idroid"]').click();await page.locator('#fit-filter').click();await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:path.join(output,'overview.png'),fullPage:false});
  await page.setViewportSize({width:1000,height:740});await page.screenshot({path:path.join(output,'compact.png'),fullPage:false});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Fitting view has no horizontal overflow at launcher minimum width');
  assert(errors.length===0,'3D fitting UI has no browser errors');fs.writeFileSync(path.join(output,'result.json'),JSON.stringify({checks,baseline,fitted,reset,views,errors},null,2));console.log(JSON.stringify({output,checks:checks.length,errors}));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
