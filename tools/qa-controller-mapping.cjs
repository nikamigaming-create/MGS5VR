// Exercise the shipping HTML and PowerShell bridge on an exact copy of the
// installed controls. This never launches or controls a desktop app or game.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const cp=require('node:child_process'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'..');
const output=path.resolve(process.argv[2]||path.join(root,'artifacts/dev/controller-mapping'));
const ui=path.join(root,'build/Release/launcher-ui');
const checker=path.join(root,'build/dev/mgs5vr_controls.exe');
const workspace=JSON.parse(fs.readFileSync(path.join(root,'private/workspace.json'),'utf8'));
const personal=path.join(workspace.game_dir,'mgs5vr-controls.ini');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const personalBefore=sha(personal),pinBefore=sha(path.join(root,'play/BUILD.json'));
const fixture=path.join(output,'fixture'),shots=path.join(output,'launcher');
fs.mkdirSync(fixture,{recursive:true});fs.mkdirSync(shots,{recursive:true});
fs.copyFileSync(personal,path.join(fixture,'mgs5vr-controls.ini'));
fs.copyFileSync(path.join(workspace.game_dir,'mgs5vr.ini'),path.join(fixture,'mgs5vr.ini'));
fs.writeFileSync(path.join(fixture,'mgsvtpp.exe'),'disposable settings fixture; never launched');
fs.writeFileSync(path.join(fixture,'mgs5vr-install.json'),'{}');
let requests=0;
const transactions=[],captures=[],checks=[];
function assert(value,message){if(!value)throw Error(message);checks.push(message);}
function bridge(method,payload={}){
 if(!['load','saveControls','saveRuntime'].includes(method))throw Error('Fixture bridge rejects game/installation actions');
 const request=path.join(fixture,'request.json');
 fs.writeFileSync(request,JSON.stringify({...payload,method,gameExe:path.join(fixture,'mgsvtpp.exe')}));
 const before=sha(path.join(fixture,'mgs5vr-controls.ini'));requests++;
 try{
  const state=JSON.parse(cp.execFileSync('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',path.join(root,'tools/launcher-bridge.ps1'),'-RequestPath',request,'-CheckerPath',checker],{encoding:'utf8',windowsHide:true,stdio:['ignore','pipe','pipe']}));
  transactions.push({method,changes:payload.changes,before,after:sha(path.join(fixture,'mgs5vr-controls.ini')),accepted:true});
  return {...state,gameRunning:false};
 }catch(error){
  const message=String(error.stderr||error.stdout||error.message);
  transactions.push({method,changes:payload.changes,before,after:sha(path.join(fixture,'mgs5vr-controls.ini')),accepted:false,error:message});
  throw Error(message);
 }finally{fs.unlinkSync(request);}
}
const server=http.createServer((req,res)=>{
 try{
  const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
  const file=path.resolve(ui,'.'+name),target=fs.statSync(file).isDirectory()?path.join(file,'index.html'):file;
  if(!target.startsWith(ui+path.sep)||req.method!=='GET'){res.writeHead(403);return res.end();}
  const type={'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.svg':'image/svg+xml','.png':'image/png','.glb':'model/gltf-binary','.mp4':'video/mp4'};
  res.writeHead(200,{'Content-Type':type[path.extname(target)]||'application/octet-stream','Cache-Control':'no-store'});fs.createReadStream(target).pipe(res);
 }catch{res.writeHead(404);res.end();}
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const errors=[],expectedErrors=[];
 let page;
 try{
  page=await browser.newPage({viewport:{width:1440,height:960}});
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('settingsFixtureBridge',bridge);
  await page.addInitScript(()=>window.__testBridge=(method,payload)=>window.settingsFixtureBridge(method,payload));
  await page.goto('http://127.0.0.1:'+server.address().port+'/',{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.launcherQA?.ready,{timeout:30000});
  const saved=await page.evaluate(()=>window.launcherQA.state.bindings);
  fs.writeFileSync(path.join(output,'effective-bindings.json'),JSON.stringify(saved,null,2));
  const snap=async(name,selectors=[])=>{
   const boxes=[];
   for(const selector of selectors){const node=page.locator(selector).first();if(await node.count()&&await node.isVisible()){const box=await node.boundingBox();if(box)boxes.push({selector,...box});}}
   await page.screenshot({path:path.join(shots,name+'.png'),animations:'disabled'});
   captures.push({name,file:'launcher/'+name+'.png',boxes,snapshot:await page.evaluate(()=>window.launcherQA.snapshot())});
  };
  const select=async name=>{await page.locator('#action-search').fill('');await page.locator('#action-select').selectOption(name);await page.locator('#action-select').scrollIntoViewIfNeeded();};
  const save=async()=>{await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save-bar').hidden,{timeout:30000});};
  const expression=async value=>{await page.locator('.advanced').evaluate(e=>e.open=true);await page.locator('#binding-expression').fill(value);await page.locator('#apply-expression').click();};
  await snap('01-home',['nav [data-tab="controls"]','nav [data-tab="modes"]']);
  await page.evaluate(()=>window.launcherQA.selectTab('controls'));
  const names=await page.locator('#action-select option').evaluateAll(nodes=>nodes.map(n=>n.value));
  assert(names.length===saved.actions.length&&new Set(names).size===saved.actions.length,'Every native-parser action appears once in the launcher editor');
  const locations=[];
  for(const action of saved.actions){
   await select(action.name);
   const value=await page.locator('#binding-expression').inputValue();
   const parsed=await page.evaluate(async text=>(await import('./binding-model.js')).parseBinding(text),value);
   assert(parsed.length===action.bindings.length,'Editor preserves alternatives/unbound state: '+action.name);
   const options=await page.locator('#gesture-0 option').evaluateAll(nodes=>nodes.map(n=>n.value));
   if(action.name==='system.idroid_focus')assert(options.join()==='level','iDroid focus permits a sustained binding only');
   for(let i=0;i<Math.max(1,action.bindings.length);i++)locations.push(await page.evaluate(({name,i})=>window.launcherQA.inspectAction(name,i),{name:action.name,i}));
  }
  assert(locations.every(a=>!a.missing.length),'Every saved action/alternative has its physical 3D control locations');
  const inputs=await page.evaluate(async()=> (await import('./binding-model.js')).inputNames);
  const contacts=inputs.filter(t=>t.endsWith('_touch')||t.endsWith('_thumbrest'));
  for(const token of inputs){const result=await page.evaluate(t=>window.launcherQA.inspectInputs([t]),token);assert(!result.missing.length,'3D surface available: '+token);if(contacts.includes(token))assert(!result.changedTransforms.length,'Contact does not press physical control: '+token);}
  assert(contacts.length===10,'All ten capacitive contacts are present independently');
  await select('gameplay.reload');await snap('02-reload',['#action-select','#gesture-0','[data-row="0"][data-token="left_grip"]','[data-row="0"][data-token="b"]']);
  await page.locator('#action-search').fill('idroid');await snap('03-search',['#action-search','#action-select']);
  await select('gameplay.ready_weapon');await snap('04-ready',['#action-select','#binding-rows']);
  await page.locator('#add-alternative').click();await page.locator('#contact-kind').selectOption('touch');
  await page.evaluate(()=>window.launcherQA.pickInput('right_trigger'));
  await page.locator('[data-row="1"][data-token="a"]').click();await page.locator('#gesture-1').selectOption('level');
  await snap('05-touch-alternative',['#contact-kind','#gesture-1','[data-row="1"][data-token="right_trigger_touch"]','#save']);
  await save();
  assert((await page.evaluate(()=>window.launcherQA.state.values['gameplay.ready_weapon'])).includes('right_trigger_touch'),'Trigger touch alternative saves through the actual native parser');
  await snap('06-saved',['#status','#binding-label']);
  await page.reload({waitUntil:'networkidle'});await page.waitForFunction(()=>window.launcherQA?.ready);
  await page.evaluate(()=>window.launcherQA.selectTab('controls'));await select('gameplay.ready_weapon');
  assert((await page.locator('#binding-expression').inputValue()).includes('right_trigger_touch'),'Saved mapping survives a complete editor reload');
  // Every supported contact/button/direction and each gesture is actually
  // persisted and re-read, including contacts the simulator cannot synthesize.
  for(const token of inputs){await expression(`right_grip | ${token==='right_trigger_touch'?token:'right_trigger_touch + '+token}`);await save();const value=await page.evaluate(()=>window.launcherQA.state.bindings.actions.find(a=>a.name==='gameplay.ready_weapon').bindings);assert(value.some(b=>b.inputs.includes(token)),'Input survives native save/read: '+token);}
  for(const gesture of ['level','press','release','tap','hold']){
   await expression(gesture==='level'?'right_grip | right_trigger_touch':`right_grip | ${gesture}(right_trigger_touch${['tap','hold'].includes(gesture)?',350':''})`);await save();
   assert((await page.evaluate(()=>window.launcherQA.state.bindings.actions.find(a=>a.name==='gameplay.ready_weapon').bindings)).some(b=>b.gesture===gesture&&b.inputs.includes('right_trigger_touch')),'Gesture survives native save/read: '+gesture);
   if(gesture==='hold')await snap('07-gesture',['#gesture-1','#threshold-1','#binding-label']);
  }
  await page.locator('#default-action').click();await save();
  await select('equipment.open');await page.locator('#add-alternative').click();await page.locator('#contact-kind').selectOption('touch');await page.evaluate(()=>window.launcherQA.pickInput('left_thumbrest'));
  await page.locator('[data-row="1"][data-token="a"]').click();await page.locator('#gesture-1').selectOption('level');
  await snap('08-thumbrest',['#contact-kind','[data-row="1"][data-token="left_thumbrest"]','#binding-label']);await save();
  assert((await page.evaluate(()=>window.launcherQA.state.values['equipment.open'])).includes('left_trigger | left_thumbrest'),'Thumb-rest alternative saves and retains the original equipment binding');
  await page.locator('#default-action').click();await save();
  const axisNames=await page.locator('[data-axis]').evaluateAll(nodes=>nodes.map(n=>n.dataset.axis));
  assert(axisNames.length===saved.axes.length&&new Set(axisNames).size===saved.axes.length,'Every stick assignment is editable');
  for(const source of ['left_stick','right_stick','disabled']){
   for(const axis of saved.axes)await page.locator(`[data-axis="${axis.name}"]`).selectOption(source);
   await save();assert((await page.evaluate(()=>window.launcherQA.state.bindings.axes)).every(a=>a.source===source),'All ten axes persist '+source);
  }
  for(const axis of saved.axes)await page.locator(`[data-axis="${axis.name}"]`).selectOption(axis.source);await save();
  await page.locator('.axis-section').scrollIntoViewIfNeeded();await snap('09-sticks',['[data-axis="axes.menu"]','[data-axis="axes.map"]']);
  await select('gameplay.native_dpad_up');await page.locator('#disable-action').click();await snap('10-unbound',['#action-select','#add-alternative','#default-action']);
  if(await page.locator('#save').isVisible())await page.locator('#discard').click();
  // Conflict/stale-save rejection must leave the actual fixture bytes intact.
  await select('system.idroid');await expression('press(a_touch)');
  await select('system.pause');await expression('press(a_touch)');
  const conflictBefore=sha(path.join(fixture,'mgs5vr-controls.ini'));await page.locator('#save').click();
  await page.waitForFunction(()=>document.querySelector('#status').classList.contains('error'));
  assert(sha(path.join(fixture,'mgs5vr-controls.ini'))===conflictBefore,'Conflicting edits cannot alter the saved file');
  assert((await page.locator('#status').innerText()).includes('conflict'),'Conflict names are visible in the launcher');
  expectedErrors.push('conflict');await snap('11-conflict',['#status','#save-bar']);await page.locator('#discard').click();
  await select('system.idroid');await expression('tap(menu,600)');
  const staleFile=path.join(fixture,'mgs5vr-controls.ini');fs.appendFileSync(staleFile,'\r\n; externally edited test fixture\r\n');const staleBefore=sha(staleFile);
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#status').classList.contains('error'));
  assert(sha(staleFile)===staleBefore&&(await page.locator('#status').innerText()).includes('changed outside'),'Stale saves retain another editor’s changes');
  expectedErrors.push('changed outside');await page.locator('#discard').click();
  await page.evaluate(()=>window.launcherQA.startTour());const tour=await page.evaluate(()=>window.launcherQA.getTour());
  for(const name of ['system.idroid','system.idroid_focus','menus.next_tab','idroid.previous_tab','idroid.next_tab','gameplay.equip_binoculars','gameplay.reload','system.pause']){
   const step=tour.find(s=>s.name===name);assert(!!step,'Saved tour covers '+name);await page.evaluate(t=>window.launcherQA.seekTour(t,true),step.start+.6);await snap('tour-'+name.replaceAll('.','-'),['#tour-binding','#tour-description']);
  }
  for(const [i,step] of tour.filter(s=>s.kind==='touch').entries()){await page.evaluate(t=>window.launcherQA.seekTour(t,true),step.start+.6);await snap('touch-'+i,['#tour-title','#tour-description']);}
  await page.evaluate(()=>window.launcherQA.selectTab('modes'));const modes=await page.locator('[data-mode]').evaluateAll(nodes=>nodes.map(n=>n.dataset.mode));
  const modeCoverage=[];
  for(const mode of modes){
   await page.locator(`[data-mode="${mode}"]`).click();const actual=await page.locator('[data-inspect]').evaluateAll(nodes=>nodes.map(n=>n.dataset.inspect));
   const expected=saved.actions.filter(a=>a.contexts.includes(mode)).map(a=>a.name);
   assert(expected.every(n=>actual.includes(n))&&actual.every(n=>expected.includes(n)),'Mode reference exactly matches parser context: '+mode);
   await snap('mode-'+mode,['#mode-title','#mode-description']);modeCoverage.push({mode,actions:actual});
  }
  await page.evaluate(()=>window.launcherQA.selectTab('settings'));await page.locator('#setting-search').fill('handheld');
  await snap('12-handheld-setting',['[data-setting="settings.handheld_menus"]']);
  await page.locator('#setting-search').fill('idroid');await snap('13-idroid-fit',['#fit-panel','#settings-grid']);
  await page.locator('#setting-search').fill('');
  const settingNames=await page.locator('[data-setting]').evaluateAll(nodes=>nodes.map(n=>n.dataset.setting));
  const expectedSettings=await page.evaluate(()=>window.launcherQA.state.settings.map(row=>row.name));
  assert(settingNames.length===expectedSettings.length&&expectedSettings.every(name=>settingNames.includes(name)),'Every native-parser setting appears in the launcher');
  for(const name of ['settings.block_firing_near_cover','settings.enemy_world_markers','settings.god_mode']){
   await page.locator('#setting-search').fill(name.split('.')[1]);
   const toggle=page.locator(`[data-setting="${name}"]`);await toggle.scrollIntoViewIfNeeded();
   assert(await toggle.getAttribute('type')==='checkbox','Launcher exposes a checkbox: '+name);
   const original=await toggle.isChecked();await toggle.setChecked(!original);await save();
   assert(Number((await page.evaluate(()=>window.launcherQA.state.settings)).find(row=>row.name===name).value)===Number(!original),'Launcher toggle persists On/Off: '+name);
   await snap(name.split('.')[1]+'-toggled',[`[data-setting="${name}"]`]);
   await page.locator(`[data-setting="${name}"]`).setChecked(original);await save();
  }
  await page.setViewportSize({width:1000,height:740});await page.evaluate(()=>window.launcherQA.selectTab('controls'));
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Editor fits the minimum supported launcher width');
  const unexpected=errors.filter(e=>!expectedErrors.some(s=>e.includes(s)));
  assert(!unexpected.length,'No unexpected JavaScript exceptions');
  assert(sha(personal)===personalBefore&&sha(path.join(root,'play/BUILD.json'))===pinBefore,'Personal controls and pinned build remain byte-identical');
  const result={schema:1,status:'passed',source:'real_launcher_html_and_native_settings_bridge',gameplay_acceptance:false,physical_headset_acceptance:false,personal_controls_sha256:personalBefore,pinned_build_sha256:pinBefore,checker_sha256:sha(checker),action_count:saved.actions.length,axis_count:saved.axes.length,actions:locations,inputs,contacts,unbound:saved.actions.filter(a=>!a.bindings.length).map(a=>a.name),modeCoverage,checks,transactions,captures,errors,requests};
  fs.writeFileSync(path.join(output,'launcher-regression.json'),JSON.stringify(result,null,2));
  console.log(JSON.stringify({status:result.status,actions:result.action_count,axes:result.axis_count,inputs:inputs.length,contacts:contacts.length,checks:checks.length,captures:captures.length,output}));
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(error=>{server.close();console.error(error);process.exitCode=1});
