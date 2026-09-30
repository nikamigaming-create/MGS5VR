// End-to-end UI exercises use the real settings bridge on a disposable INI.
const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),output=path.resolve(process.argv[2]||'artifacts/launcher-3d-qa/browser');fs.mkdirSync(output,{recursive:true});
const fixture=path.join(output,'fixture');fs.mkdirSync(fixture,{recursive:true});
for(const name of ['mgs5vr-controls.ini','mgs5vr.ini'])fs.copyFileSync(path.join(root,'config',name),path.join(fixture,name));
fs.writeFileSync(path.join(fixture,'mgsvtpp.exe'),'fixture — never launched');fs.writeFileSync(path.join(fixture,'mgs5vr-install.json'),'{}');
let sequence=0;const bridge=async(method,payload)=>{
 if(!['load','saveControls','saveRuntime'].includes(method))throw Error('Test cannot launch or mutate a real installation');
 const request=path.join(fixture,`request-${++sequence}.json`);fs.writeFileSync(request,JSON.stringify({...payload,method,gameExe:path.join(fixture,'mgsvtpp.exe')}));
 try{const value=cp.execFileSync('C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',path.join(root,'tools/launcher-bridge.ps1'),'-RequestPath',request],{encoding:'utf8',windowsHide:true});return {...JSON.parse(value),gameRunning:false};}catch(e){throw Error(String(e.stderr||e.stdout||e.message))}finally{fs.unlinkSync(request)}
};
function assert(value,message){if(!value)throw Error(message)}
(async()=>{
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:960}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('fixtureBridge',bridge);await page.addInitScript(()=>window.__testBridge=(method,payload)=>window.fixtureBridge(method,payload));
  await page.goto('http://127.0.0.1:8766/build/Release/launcher-ui/',{waitUntil:'networkidle'});await page.waitForFunction(()=>window.launcherQA?.ready,{timeout:30000});
  await page.screenshot({path:path.join(output,'01-deploy.png'),fullPage:true});
  await page.evaluate(()=>window.launcherQA.openLesson('vr-binocular-latch-enter'));await page.waitForFunction(()=>document.querySelector('video').readyState>=2);
  const cueTime=await page.evaluate(()=>{const c=window.launcherQA.lesson.cues[0];return(c.start+c.end)/2});
  const before=await page.evaluate(t=>window.launcherQA.seek(t),cueTime);assert(before.inputs.includes('y')&&before.inputs.includes('left_grip'),'Initial binocular chord');
  await page.waitForTimeout(1000);await page.screenshot({path:path.join(output,'02-lesson.png'),fullPage:true});
  const cameraBefore=await page.evaluate(()=>window.launcherQA.snapshot());
  await page.waitForFunction(()=>Math.abs(window.launcherQA.snapshot().camera.azimuth)>.85);const autoSide=await page.evaluate(()=>window.launcherQA.snapshot().camera);
  await page.screenshot({path:path.join(output,'02b-auto-grip-view.png'),fullPage:true});
  await page.locator('#remap-lesson').click();await page.locator('[data-token="y"]').click();await page.locator('[data-token="x"]').click();
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);assert(fs.readFileSync(path.join(fixture,'mgs5vr-controls.ini'),'utf8').includes('equip_binoculars = hold(left_grip + x,300)'),'Actual file saved');
  await page.evaluate(()=>window.launcherQA.openLesson('vr-binocular-latch-enter'));const after=await page.evaluate(t=>window.launcherQA.seek(t),cueTime);
  assert(after.inputs.includes('x')&&!after.inputs.includes('y')&&after.inputs.includes('left_grip'),'Replay resolves changed binding');assert(after.media===before.media,'Video identity unchanged');
  await page.screenshot({path:path.join(output,'03-remapped-lesson.png'),fullPage:true});
  await page.evaluate(()=>window.launcherQA.startTour());
  const tour=await page.evaluate(()=>window.launcherQA.getTour());
  const binocularMapping=tour.find(s=>s.name==='gameplay.equip_binoculars');
  assert(binocularMapping.inputs.includes('x')&&!binocularMapping.inputs.includes('y'),'Quick tour uses the actually saved remap');
  const contacts=tour.filter(s=>s.kind==='touch').flatMap(s=>s.inputs);assert(new Set(contacts).size===10,'All ten contact surfaces shown');
  await page.evaluate(()=>window.launcherQA.seekTour(0));
  for(const input of contacts){const shown=await page.evaluate(t=>window.launcherQA.inspectInputs([t]),input);assert(!shown.missing.length&&!shown.changedTransforms.length,'Touch highlights its surface without pressing: '+input);}
  await page.evaluate(()=>window.launcherQA.selectTab('controls'));
  await page.locator('#action-select').selectOption('gameplay.ready_weapon');
  await page.locator('#add-alternative').click();
  await page.locator('#contact-kind').selectOption('touch');
  await page.evaluate(()=>window.launcherQA.pickInput('right_trigger'));
  await page.locator('[data-row="1"][data-token="a"]').click();
  await page.locator('#gesture-1').selectOption('level');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);
  assert(fs.readFileSync(path.join(fixture,'mgs5vr-controls.ini'),'utf8').includes('right_grip | right_trigger_touch'),'Touch alternative saved through the real parser');
  await page.evaluate(()=>window.launcherQA.startTour());
  const updatedTour=await page.evaluate(()=>window.launcherQA.getTour());
  assert(updatedTour.some(s=>s.name==='gameplay.ready_weapon'&&s.inputs.join()==='right_trigger_touch'),'Quick tour includes the saved touch alternative');
  await page.evaluate(()=>window.launcherQA.seekTour(0));
  await page.evaluate(()=>window.launcherQA.selectTab('modes'));await page.locator('#contact-kind').selectOption('press');
  await page.getByRole('button',{name:'Game modes',exact:false}).first().click();const modes=await page.locator('[data-mode]').count();assert(modes===8,'Every native context listed');
  await page.locator('[data-mode="menus"]').click();await page.locator('[data-inspect="menus.confirm"]').first().click();assert((await page.locator('#binding-label').innerText()).includes('A'),'Menus action resolves');
  await page.locator('[data-view="front"]').click();const aPoint=await page.evaluate(()=>window.launcherQA.controlPoint('a'));await page.mouse.click(aPoint.x,aPoint.y);assert((await page.locator('#picked-control').innerText()).includes('CONTROL / A'),'Actual 3D A click shows its contextual actions');assert(await page.locator('[data-inspect="menus.confirm"]').count()===1,'3D click resolves confirm action in menus');
  await page.locator('[data-inspect="menus.confirm"]').first().click();
  await page.screenshot({path:path.join(output,'04-mode-menus.png'),fullPage:true});
  const contexts=await page.evaluate(()=>[...document.querySelectorAll('[data-mode]')].map(e=>e.dataset.mode));for(const mode of contexts){await page.locator(`[data-mode="${mode}"]`).click();assert(await page.locator('[data-inspect]').count()>0,'Mode has controls: '+mode)}
  const actionLocations=await page.evaluate(()=>window.launcherQA.state.bindings.actions.flatMap(a=>Array.from({length:Math.max(1,a.bindings.length)},(_,i)=>window.launcherQA.inspectAction(a.name,i))));assert(actionLocations.length>=96&&actionLocations.every(a=>!a.missing.length),'Every action resolves its 3D locations');
  await page.getByRole('button',{name:'VR settings',exact:false}).first().click();const sliders=await page.locator('#settings-grid input[type=range]').count(),toggles=await page.locator('#settings-grid input[type=checkbox]').count(),selects=await page.locator('#settings-grid select').count();assert(sliders>30&&toggles>=5&&selects===2,'Appropriate live settings widgets');
  await page.locator('#setting-search').fill('idroid');await page.screenshot({path:path.join(output,'05-idroid-settings.png'),fullPage:true});
  await page.locator('#setting-search').fill('weapon_hud');await page.locator('[data-number="settings.weapon_hud_setback_cm"]').fill('7.5');await page.locator('[data-number="settings.weapon_hud_setback_cm"]').dispatchEvent('change');await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden);assert(fs.readFileSync(path.join(fixture,'mgs5vr-controls.ini'),'utf8').includes('weapon_hud_setback_cm = 7.5'),'Fractional slider exact value round trip');
  await page.locator('#setting-search').fill('');await page.locator('[data-settings="runtime"]').click();assert(await page.locator('#settings-grid input[type=text]').count()>=5,'Runtime asset paths are text inputs');
  await page.setViewportSize({width:1000,height:740});await page.evaluate(()=>window.launcherQA.selectTab('modes'));await page.screenshot({path:path.join(output,'06-compact.png'),fullPage:true});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth),'No horizontal overflow at minimum width');
  await page.evaluate(()=>window.launcherQA.selectTab('notes'));await page.locator('[data-catalog="numbers"]').click();await page.waitForFunction(()=>document.querySelector('#note-list').textContent.includes('numbers found in'));await page.locator('#note-search').fill('weapon_hud_setback_cm');assert(await page.locator('#note-list .note-row').count()>0,'Numeric inventory source lookup');await page.screenshot({path:path.join(output,'07-numeric-inventory.png'),fullPage:true});
  await page.locator('#note-search').fill('');await page.locator('[data-catalog="report"]').click();await page.waitForFunction(()=>document.querySelectorAll('[data-report]').length===55);
  await page.locator('[data-report="R20"] > summary').click();assert((await page.locator('[data-report="R20"]').innerText()).includes('Automated check passed'),'Editor report has individual verified checks');
  await page.locator('[data-report="R20"] .claim-proof').first().locator('summary').click();await page.locator('[data-report="R20"] [data-proof]').first().click();await page.waitForFunction(()=>document.querySelector('#proof-content pre'));assert((await page.locator('#proof-content').innerText()).includes('exit_code'),'Actual editor test evidence opens');await page.locator('#close-proof').click();
  await page.locator('#report-filter').selectOption('failed');assert(await page.locator('[data-report="R18"]').count()===1&&await page.locator('[data-report="R20"]').count()===0,'Failures stay separate from passed editor checks');
  await page.locator('[data-report="R18"] > summary').click();await page.locator('[data-report="R18"] .claim-proof').first().locator('summary').click();const picture=page.locator('[data-report="R18"] [data-proof]').filter({hasText:'.png'}).first();await picture.click();await page.waitForFunction(()=>{const img=document.querySelector('#proof-content img');return img?.complete&&img.naturalWidth>0});await page.screenshot({path:path.join(output,'08-community-evidence.png'),fullPage:true});await page.locator('#close-proof').click();
  await page.locator('#report-filter').selectOption('');await page.locator('#note-search').fill('R17');await page.locator('[data-report="R17"] > summary').click();await page.screenshot({path:path.join(output,'09-pause-results.png'),fullPage:true});
  const summary=await page.evaluate(()=>window.launcherQA.snapshot());assert(summary.overlays===0,'No video overlays');assert(errors.length===0,'Browser errors: '+errors.join('\n'));
  fs.writeFileSync(path.join(output,'result.json'),JSON.stringify({before,after,videoUnchanged:true,modes,sliders,toggles,selects,summary,cameraBefore,autoSide,click3D:true,actionLocations,errors},null,2));console.log(JSON.stringify({output,modes,sliders,toggles,selects,remap:true,videoUnchanged:true,automaticRotation:true,click3D:true,actions:actionLocations.length,errors}));
  await page.evaluate(()=>{window.__testBridge=async()=>({needsGame:true,gameExe:'',gameRunning:false});});await page.evaluate(()=>window.launcherQA.refresh(true));await page.evaluate(()=>window.launcherQA.selectTab('modes'));assert(await page.locator('[data-mode]').count()===8,'Default mode guide available without an installed configuration');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
