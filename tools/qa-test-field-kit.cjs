// Browser QA and a real UI demonstration. No OS/game mouse or keyboard injection.
const {chromium}=require('playwright');
const fs=require('fs'),path=require('path');
const args=process.argv.slice(2), demonstrate=args.includes('--demo');
const root=path.resolve(__dirname,'..'), output=path.join(root,'artifacts/test-field-kit',demonstrate?'demo':'browser-qa');
fs.mkdirSync(output,{recursive:true});
const connection=JSON.parse(fs.readFileSync(path.join(root,'artifacts/test-field-kit/connection.json')));
const assert=(value,message)=>{if(!value)throw Error(message)};
(async()=>{
  const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
  const context=await browser.newContext({viewport:{width:1600,height:1200},...(demonstrate?{recordVideo:{dir:output,size:{width:1600,height:1200}}}:{})});
  const page=await context.newPage(), errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  const getState=async role=>JSON.parse(await (await context.request.get(connection.url+'/api/state',{headers:{Authorization:'Bearer '+connection.tokens[role||'human']}})).text());
  async function commandDone(){await page.waitForFunction(()=>!document.querySelector('#observe').disabled,{timeout:180000});}
  try{
    await page.goto(connection.human_url);await page.waitForSelector('.scenario-row');
    assert(await page.locator('.scenario-row').count()===4,'Four executable scenarios');
    assert(!await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),'Desktop has no horizontal overflow');
    await page.screenshot({path:path.join(output,'01-operations.png'),fullPage:true});
    if(demonstrate){
      const initial=await getState();assert(initial.connected&&!initial.busy,'Requires a connected idle runner');
      assert(initial.telemetry.scene==='gameplay','Live field scene required');
      await page.locator('#observe').click();await page.waitForTimeout(700);await commandDone();
      await page.waitForFunction(()=>document.querySelector('#left-eye').naturalWidth>0);
      await page.waitForTimeout(2000);
      await page.locator('#run').scrollIntoViewIfNeeded();
      await page.locator('#run').click();
      const initialCount=initial.results.length;
      await page.waitForTimeout(700);
      await page.waitForFunction(()=>!document.querySelector('#observe').disabled,{timeout:180000});
      const final=await getState();
      const added=final.results.slice(initialCount);
      assert(added.length===3&&added.every(result=>result.status==='observed_pass'),'Equipment, Commands and physical binocular round trips must pass');
      fs.writeFileSync(path.join(output,'native-results.json'),JSON.stringify(added,null,2));
      await page.waitForTimeout(2200);
      await page.locator('#llm').scrollIntoViewIfNeeded();await page.locator('#llm').click();
      await page.waitForFunction(()=>document.querySelector('#llm').classList.contains('selected'));
      // Compare a stable observation, not time-varying telemetry samples.
      const human=await getState('human'),llm=await getState('llm');
      assert(JSON.stringify(human.observation)===JSON.stringify(llm.observation),'Both supervisors receive the identical captured observation');
      await page.waitForTimeout(3000);await page.locator('#human').click();
      await page.waitForFunction(()=>document.querySelector('#human').classList.contains('selected'));
    }
    await page.locator('[data-page="library"]').click();await page.waitForTimeout(demonstrate?3000:100);
    assert(await page.locator('.library-card').count()===4,'All scenarios described');
    await page.screenshot({path:path.join(output,'02-library.png'),fullPage:true});
    await page.locator('[data-page="coverage"]').click();await page.waitForSelector('.report');
    assert(await page.locator('.report').count()===55,'Entire 55-report ledger visible');
    await page.locator('#coverage-filter').fill('R03');assert(await page.locator('.report').count()>=1,'Coverage search works');
    await page.locator('.report summary').first().click();await page.waitForTimeout(demonstrate?3000:100);
    await page.screenshot({path:path.join(output,'03-coverage.png'),fullPage:true});
    await page.locator('[data-page="archive"]').click();await page.waitForTimeout(demonstrate?3000:100);
    await page.screenshot({path:path.join(output,'04-archive.png'),fullPage:true});
    if(demonstrate){await page.locator('[data-review]').first().click();await page.waitForTimeout(3500);}
    if(!demonstrate){await page.setViewportSize({width:650,height:950});await page.locator('[data-page="operations"]').click();assert(!await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),'Mobile has no horizontal overflow');await page.screenshot({path:path.join(output,'05-mobile.png'),fullPage:true});}
    assert(!errors.length,'No browser errors: '+errors.join('; '));
    fs.writeFileSync(path.join(output,'qa.json'),JSON.stringify({passed:true,errors,realSession:demonstrate,capturedAt:new Date().toISOString(),videoKind:'Browser UI recording; gameplay panels are timestamped sequential compositor stills'},null,2));
    console.log(JSON.stringify({passed:true,output,errors}));
  }finally{
    const video=page.video();await context.close();if(video)await video.saveAs(path.join(output,'test-field-kit.webm'));await browser.close();
  }
})().catch(error=>{console.error(error.stack);process.exit(1)});
