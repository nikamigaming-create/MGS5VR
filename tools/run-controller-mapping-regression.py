"""Bounded fresh retail session for the controller mapping review.

Launch development, reach Continue through the maintained guarded runner, then
allow finite observation-bound plans. Always close the test session and restore
the exact controls bytes. No desktop input, Steam restart or pinned-build change.
"""
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/dev/controller-mapping'
config=json.loads((ROOT/'private/workspace.json').read_text())
controls=Path(config['game_dir'])/'mgs5vr-controls.ini'
original=controls.read_bytes()
temporary=original.replace(b'handheld_menus = 0',b'handheld_menus = 1')
if temporary==original or b'handheld_menus = 0' not in original:
    raise SystemExit('Expected the reviewed spatial preference; controls were not changed')
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'original-controls.ini').write_bytes(original)
receipt={'schema':1,'controls_sha256':hashlib.sha256(original).hexdigest(),'temporary':'handheld_menus=1','restored':False,'test_session_closed':False,'runs':[]}
def write(): (OUT/'session-lease.json').write_text(json.dumps(receipt,indent=2))
write()
def run(args):
    previous={p.name for p in (ROOT/'artifacts/bot/runs').iterdir() if p.is_dir()}
    child=subprocess.Popen([sys.executable,str(ROOT/'tools/workspace.py'),'bot',*args],cwd=ROOT)
    deadline=time.monotonic()+40
    while time.monotonic()<deadline:
        candidates=[p for p in (ROOT/'artifacts/bot/runs').iterdir() if p.is_dir() and p.name not in previous]
        if candidates:
            path=max(candidates,key=lambda p:p.name)
            (OUT/'active-run.txt').write_text(str(path))
            # Compatibility with the existing explicit two-eye review helper.
            (ROOT/'artifacts/dev/music-stop-current/active-run.txt').write_text(str(path))
            receipt['runs'].append(str(path));write()
            print('CONTROLLER REGRESSION: '+str(path),flush=True)
            break
        if child.poll() is not None:break
        time.sleep(.1)
    code=child.wait()
    return code
try:
    controls.write_bytes(temporary)
    subprocess.check_call([sys.executable,str(ROOT/'tools/workspace.py'),'launch-sim'],cwd=ROOT)
    # Session advances only known, natively observed startup / Continue states.
    entry={'schema':1,'required_settings':{'settings.handheld_menus':1},'cases':[{
        'id':'mapping-handheld-open','before':{'scene':'cabin','idroid':False,'menu':False,'native.title':False,'native.popup':False},
        'setup':[{'op':'motion','seconds':.3,'hands':[{'hand':'left','base_space':'view','position':[-.08,-.15,-.32],
            'orientation':[.2588190451,0,0,.9659258263],'aim_position':[-.08,-.15,-.32],'aim_orientation':[-.2588190451,0,0,.9659258263]}]}],
        'steps':[{'op':'action','name':'system.idroid','seconds':.25}],
        'after':{'idroid':True,'idroid_handheld':True,'camera_active':True},'timeout':10,'after_stable_samples':12},{
        'id':'mapping-reviewed-rival-help-close',
        'before':{'idroid':True,'pause':True,'idroid_tutorial_mode':10,
                  'native.mission':40050,'native.sequence':'Seq_Game_MainGame','native.popup':False},
        'steps':[{'op':'action','name':'menus.back','seconds':.30,
                  'state_before':{'pause':True,'idroid_tutorial_mode':10},
                  'native_before':{'mission':40050,'sequence':'Seq_Game_MainGame','popup':False}}],
        'after':{'pause':False},'timeout':8,'after_stable_samples':3}]}
    (OUT/'handheld-entry.json').write_text(json.dumps(entry,indent=2))
    code=run(['--command','session','--suite',str(OUT/'handheld-entry.json'),'--seconds','240','--record'])
    # A recognized arrival can be inspected even if an opening card needs review.
    print('Entry runner exit: '+str(code),flush=True)
    # A native source-eye movie cannot start on a static startup/help page.
    # Supervision uses real paired compositor captures; the separately bounded
    # final-eye recorder supplies the teaching clip after actual page review.
    run(['--command','supervise','--seconds','600','--idle-seconds','180'])
finally:
    receipt['test_session_closed']=subprocess.call([sys.executable,str(ROOT/'tools/workspace.py'),'stop-sim'],cwd=ROOT)==0
    if controls.read_bytes() in (original,temporary):controls.write_bytes(original)
    receipt['restored']=controls.read_bytes()==original
    write()
    print(json.dumps({'controls_restored':receipt['restored'],'test_session_closed':receipt['test_session_closed']}),flush=True)
