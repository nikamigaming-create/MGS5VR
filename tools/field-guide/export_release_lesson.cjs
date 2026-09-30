/* Export the same audited lesson as the interactive kit: real source video,
   authored 3D controller endpoints, and a control close-up inside the game view. */
const {chromium}=require('playwright');
const fs=require('fs');
const path=require('path');
const crypto=require('crypto');
const {spawnSync}=require('child_process');
const digest=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const run=(command,args,cwd)=>{const r=spawnSync(command,args,{cwd,encoding:'utf8',windowsHide:true,maxBuffer:8*1024*1024});if(r.status!==0)throw Error(`${command} failed: ${r.stderr||r.error}`);return r.stdout};
(async()=>{
 const id=process.argv[2],output=path.resolve(process.argv[3]||'');
 if(!id||!process.argv[3])throw Error('Usage: export_release_lesson.cjs <lesson-id> <new-output-directory>');
 if(fs.existsSync(output))throw Error('Output already exists; preserve old proof and choose a fresh directory');
 fs.mkdirSync(output,{recursive:true});fs.mkdirSync(path.join(output,'frames'));
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 let lesson,title,instruction;const fps=24;
 try{
  const page=await browser.newPage({viewport:{width:1500,height:1080}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:8766/artifacts/field-guide-20260927/release-desk/',{waitUntil:'networkidle'});
  await page.evaluate(id=>window.fieldKit.openLesson(id),id);
  await page.waitForFunction(()=>window.fieldKit.ready&&window.fieldKit.lesson);
  lesson=await page.evaluate(()=>window.fieldKit.lesson);title=await page.locator('#lesson-title').textContent();instruction=await page.locator('#instruction').textContent();
  await page.evaluate(()=>{document.querySelector('#controller-canvas').style.cssText='width:820px;height:710px;min-height:710px;max-height:710px;flex:none';document.querySelector('#detail-canvas').style.cssText='width:250px;height:225px';});
  const frames=Math.floor(lesson.duration*fps);if(frames<1)throw Error('No complete output frame');
  for(let n=0;n<frames;n++){
   const t=n/fps,inputs=lesson.intervals.filter(i=>i.start<=t&&t<i.end).map(i=>i.input);
   const images=await page.evaluate(({inputs,angle})=>window.fieldKit.renderControls(inputs,angle),{inputs,angle:-.10+.20*n/Math.max(1,frames-1)});
   for(const [name,url] of Object.entries(images)){
    if(!url.startsWith('data:image/png;base64,'))throw Error('Controller frame was not rendered');
    fs.writeFileSync(path.join(output,'frames',`${name}-${String(n).padStart(5,'0')}.png`),Buffer.from(url.split(',')[1],'base64'));
   }
  }
  if(errors.length)throw Error(errors.join('\n'));
  fs.writeFileSync(path.join(output,'render.json'),JSON.stringify({fps,frames,lesson,title,instruction,page_errors:errors},null,2));
 }finally{await browser.close()}
 const source=path.resolve('artifacts/field-guide-20260927/release-desk',lesson.media);
 if(digest(source)!==lesson.source_sha256)throw Error('Source video changed after the field-kit manifest was generated');
 const duration=Math.floor(lesson.duration*fps)/fps,c=lesson.crop;
 fs.writeFileSync(path.join(output,'heading.txt'),title);fs.writeFileSync(path.join(output,'instruction.txt'),instruction);
 fs.writeFileSync(path.join(output,'footer.txt'),`Recorded source-eye gameplay | Build ${lesson.identity.dll_sha256.slice(0,12)} | 3D control orientation is instructional`);
 const font="fontfile='C\\:/Windows/Fonts/arial.ttf'";
 const graph=[`color=c=0xf1ecdf:s=1920x1080:r=${fps}:d=${duration}[paper]`,
  `[0:v]trim=start=${lesson.start}:duration=${duration},setpts=PTS-STARTPTS,crop=${c.width}:${c.height}:${c.x}:${c.y},scale=924:800[game]`,
  '[paper][game]overlay=40:190[base]',
  '[base]drawbox=x=1004:y=190:w=876:h=800:color=0xe3dece:t=fill[panel]',
  '[panel][1:v]overlay=1032:204[controllers]',
  '[controllers]drawbox=x=688:y=675:w=260:h=262:color=0xf1ecdf:t=fill[inset]',
  '[inset][2:v]overlay=693:705[shown]',
  `[shown]drawtext=${font}:text='MGS5VR / FIELD INSTRUCTION':x=40:y=35:fontsize=22:fontcolor=0xa6322a,drawtext=${font}:textfile=heading.txt:x=40:y=74:fontsize=48:fontcolor=0x292b28,drawtext=${font}:textfile=instruction.txt:x=40:y=143:fontsize=24:fontcolor=0x292b28,drawtext=${font}:text='CONTROL CLOSE-UP':x=710:y=684:fontsize=18:fontcolor=0x292b28,drawtext=${font}:text='LEFT':x=1207:y=950:fontsize=22:fontcolor=0x292b28,drawtext=${font}:text='RIGHT':x=1612:y=950:fontsize=22:fontcolor=0x292b28,drawtext=${font}:textfile=footer.txt:x=40:y=1022:fontsize=20:fontcolor=0x67695f[out]`].join(';');
 fs.writeFileSync(path.join(output,'composite.filter'),graph);
 run('ffmpeg',['-hide_banner','-loglevel','error','-n','-threads','2','-i',source,'-framerate',String(fps),'-i','frames/controller-%05d.png','-framerate',String(fps),'-i','frames/detail-%05d.png','-filter_complex_threads','1','-filter_complex_script','composite.filter','-map','[out]','-map','0:a:0','-af',`atrim=start=${lesson.start}:duration=${duration},asetpts=PTS-STARTPTS`,'-r',String(fps),'-t',String(duration),'-c:v','libx264','-threads','2','-crf','19','-preset','fast','-c:a','aac','-b:a','192k','-movflags','+faststart','lesson.mp4'],output);
 const probe=JSON.parse(run('ffprobe',['-v','error','-show_entries','stream=codec_type,duration,width,height,nb_frames:format=duration','-of','json','lesson.mp4'],output));
 const video=probe.streams.find(s=>s.codec_type==='video');if(video.width!==1920||video.height!==1080||Math.abs(Number(video.duration)-duration)>.05)throw Error('Export size/duration mismatch');
 const firstCue=lesson.intervals[0],poster=Math.min(duration-.05,(firstCue.start+firstCue.end)/2);
 run('ffmpeg',['-hide_banner','-loglevel','error','-n','-threads','2','-ss',String(poster),'-i','lesson.mp4','-frames:v','1','poster.png'],output);
 const provenance={schema:1,lesson,output_sha256:digest(path.join(output,'lesson.mp4')),probe,fps,source_interval_seconds:[lesson.start,lesson.start+duration],controller_basis:'Authored GLB pressed/axis endpoint transforms. Highlight intervals come from audited controller acknowledgments; orientation is an instructional camera orbit.',limits:['Edited instructional excerpt, not an uninterrupted full-feature or final-compositor proof.','Source dropped frames and controller acknowledgment timing limit exact rendered-frame alignment.'],title,instruction};
 fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify(provenance,null,2));
 console.log(JSON.stringify({video:path.join(output,'lesson.mp4'),poster:path.join(output,'poster.png'),duration,source_build:lesson.identity.dll_sha256}));
})().catch(e=>{console.error(e);process.exitCode=1});
