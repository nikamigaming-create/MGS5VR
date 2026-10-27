"""Bounded real compositor takes coordinated with the owned workbench bot."""
import json
import pathlib
import subprocess
import sys
import time

from .core import BotFault,atomic_json
from .live import ROOT


class CabinFilm:
    def __init__(self,live):
        self.live=live;self.output=live.events.output/'film';self.output.mkdir()
        self.active=None;self.takes=[]
        self.proxy=json.loads((ROOT/'private/workspace.json').read_text())['operator_proxy']

    def keep_grips(self):
        # Encoding a chapter must not release a paper held by the input owner.
        # Renew only already-owned grip leases; never extend a shutter/action.
        grips=[value for key,value in self.live.held.items() if key[1]=='Grip']
        if grips:self.live.input(grips,.1,lease_seconds=5.)

    def begin(self,name):
        self.keep_grips()
        self.end()
        number=len(self.takes);path=self.output/f'{number+1:02d}-{name}'
        ready=self.output/f'{number+1:02d}.ready.json';stop=self.output/f'{number+1:02d}.stop'
        log=(self.output/f'{number+1:02d}.log').open('wb')
        process=subprocess.Popen([sys.executable,str(ROOT/'tools/record-simulator.py'),
            '--proxy',self.proxy,'--mp4','--seconds','45','--eye','right','--max-fps','30',
            '--output',str(path),'--ready-file',str(ready),'--stop-file',str(stop)],
            stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
        self.active={'name':name,'path':str(path),'ready':ready,'stop':stop,'process':process,'log':log,'markers':[]}
        deadline=time.monotonic()+12.;renew=time.monotonic()
        while not ready.is_file():
            if process.poll() is not None or time.monotonic()>deadline:
                raise BotFault('Composited movie did not receive its first real eye frame: '+name)
            if time.monotonic()>=renew:self.keep_grips();renew=time.monotonic()+2.
            time.sleep(.05)
        self.active['started_monotonic_seconds']=json.loads(ready.read_text())['started_monotonic_seconds']
        self.mark('chapter-start')

    def mark(self,label):
        if not self.active:raise BotFault('Film marker has no owned capture')
        if self.active['process'].poll() is not None:raise BotFault('Compositor movie ended before '+label)
        row={'label':label,'seconds':time.monotonic()-self.active['started_monotonic_seconds']}
        self.active['markers'].append(row);self.live.events.emit('compositor_film_marker',chapter=self.active['name'],**row)

    def end(self):
        if not self.active:return
        take=self.active;self.active=None;take['stop'].touch()
        try:
            deadline=time.monotonic()+15.
            while take['process'].poll() is None:
                self.keep_grips()
                try:take['process'].wait(timeout=min(2.,max(.01,deadline-time.monotonic())))
                except subprocess.TimeoutExpired:
                    if time.monotonic()>=deadline:raise
            code=take['process'].returncode
        except subprocess.TimeoutExpired as error:
            take['process'].terminate();take['process'].wait(timeout=3.)
            raise BotFault('Owned compositor take did not finalize') from error
        finally:take['log'].close()
        metadata_path=pathlib.Path(take['path'])/'capture.json'
        metadata=json.loads(metadata_path.read_text()) if metadata_path.is_file() else {}
        if code or metadata.get('error') or metadata.get('encoder_exit')!=0 or len(metadata.get('frames',[]))<3:
            raise BotFault('Compositor capture failed: '+take['name'])
        row={k:v for k,v in take.items() if k not in ('ready','stop','process','log')}
        row.update(metadata=metadata,source='Meta OpenXR composited right eye')
        self.takes.append(row)
        atomic_json(self.output/'takes.json',{'source':'actual compositor; separate labeled chapters','takes':self.takes})
