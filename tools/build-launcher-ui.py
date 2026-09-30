"""Bundle local launcher assets and hash-verified, unobstructed source recordings."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from functools import lru_cache
from community_verification import bundle as bundle_community

ROOT=Path(__file__).resolve().parents[1]
UI=ROOT/'tools/launcher-ui'
TITLES={'equipment-picker-open-release': 'Open the equipment picker', 'commands-held-open-release': 'Open buddy commands', 'vr-binocular-latch-enter': 'Equip binoculars', 'vr-binocular-latch-stow': 'Put binoculars away', 'pause-open': 'Open Pause', 'pause-close': 'Close Pause', 'standing-to-crouched': 'Crouch', 'crouched-to-prone': 'Go prone', 'prone-to-crouched': 'Rise to a crouch', 'crouched-to-standing': 'Stand up'}
DESCRIPTIONS={'equipment-picker-open-release': 'Hold the equipment button to open the picker. Release it to close.', 'commands-held-open-release': 'Hold the command button to open the orders menu. Release it to close.', 'vr-binocular-latch-enter': 'Equip the binoculars, then raise them to your eyes.', 'vr-binocular-latch-stow': 'Put the binoculars away with the controls shown here.', 'pause-open': 'Open the Pause menu. You can still look around and move your hands.', 'pause-close': 'Close the Pause menu to resume the game.', 'standing-to-crouched': 'Change from standing to a crouch.', 'crouched-to-prone': 'Go prone from a crouch.', 'prone-to-crouched': 'Rise from prone into a crouch.', 'crouched-to-standing': 'Stand up from a crouch.'}

@lru_cache(maxsize=64)
def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def export_source(desk):
    text=desk.read_text(encoding='utf-8')
    data=json.loads(text.removeprefix('window.RELEASE_DESK_DATA=').removesuffix(';'))
    lessons=[]
    for old in data['lessons']:
        if not old['available']:continue
        item={key:old[key] for key in ['id','action','source_sha256','source_width','source_height','crop','start','end','duration','source_eye','native_status','visual_status']}
        item['title']=TITLES.get(old['id'],old['title'])
        item['description']=DESCRIPTIONS.get(old['id'],'Use the controls shown beside the video.')
        item['visual_finding']=old.get('visual_finding','')
        item['dll_sha256']=old['identity']['dll_sha256']
        item['source']=(desk.parent/old['media']).resolve().relative_to(ROOT).as_posix()
        # Preserve the demonstrated semantic action window. Never put the
        # recorder's physical input names or labels into the playback contract.
        item['cues']=[{'action':old['action'],'start':min(i['start'] for i in old['intervals']),'end':max(i['end'] for i in old['intervals'])}]
        lessons.append(item)
    rows=lambda kind:[{k:r[k] for k in ['id','title','status','acceptance','latest_report'] if k in r} for r in data['coverage']['rows'] if r['kind']==kind]
    (UI/'catalog-source.json').write_text(json.dumps({'lessons':lessons,'features':rows('historical_feature'),'reports':rows('report')},indent=2),encoding='utf-8')

def ini(path):
    result={};section=''
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line=line.strip()
        if line.startswith('[') and line.endswith(']'):section=line[1:-1]
        elif '=' in line and not line.startswith((';','#')):
            key,value=line.split('=',1);result[section+'.'+key.strip()]=value.strip()
    return result

def build(output,checker=None):
    output.mkdir(parents=True,exist_ok=True)
    for path in UI.iterdir():
        if path.suffix in ('.html','.css','.js','.svg'):shutil.copy2(path,output/path.name)
    guide=ROOT/'tools/field-guide'
    shutil.copy2(guide/'controller-player.js',output/'controller-player.js')
    shutil.copytree(guide/'vendor',output/'vendor',dirs_exist_ok=True)
    (output/'assets').mkdir(exist_ok=True)
    for name in ['meta-quest-touch-plus-left.glb','meta-quest-touch-plus-right.glb','controller-spatial-anchors.json','LICENSE-webxr-input-profiles.md']:
        shutil.copy2(guide/'assets'/name,output/'assets'/name)
    # Reuse the original artwork, unchanged. UI viewports show illustrations only;
    # the historical card's printed bindings must not become today's guide.
    shutil.copy2(ROOT/'docs/images/controls.png',output/'assets/field-card-original.png')
    shutil.copy2(ROOT/'docs/images/field-header.svg',output/'assets/field-header.svg')
    shutil.copy2(ROOT/'docs/images/ARTWORK.md',output/'assets/ARTWORK.md')
    catalog=json.loads((UI/'catalog-source.json').read_text(encoding='utf-8'))
    available=[];missing=[];(output/'media').mkdir(exist_ok=True)
    for lesson in catalog['lessons']:
        source=(ROOT/lesson.pop('source')).resolve()
        if not source.is_relative_to(ROOT):raise ValueError('Recording must be inside workspace')
        if not source.is_file():missing.append(lesson['id']);continue
        if digest(source)!=lesson['source_sha256']:raise ValueError('Recording hash changed: '+str(source))
        name=lesson['source_sha256'][:20]+source.suffix
        target=output/'media'/name
        if not target.is_file() or digest(target)!=lesson['source_sha256']:shutil.copy2(source,target)
        lesson['media']='media/'+name;available.append(lesson)
    catalog['lessons']=available;catalog['missingRecordings']=missing
    bundle_community(ROOT,ROOT/'docs/COMMUNITY_VERIFICATION_2026-09-27.json',output)
    catalog['defaultValues']=ini(ROOT/'config/mgs5vr-controls.ini')
    catalog['runtimeDefaults']=ini(ROOT/'config/mgs5vr.ini')
    checker=checker or ROOT/'build/Release/mgs5vr_controls.exe'
    catalog['previewState']={'bindings':json.loads(subprocess.check_output([str(checker),'--bindings-json'],text=True)),
        'settings':json.loads(subprocess.check_output([str(checker),'--settings-json'],text=True)),
        'values':catalog['defaultValues'],'runtime':catalog['runtimeDefaults'],'writable':False}
    subprocess.run([sys.executable,str(ROOT/'tools/field-guide/audit_numeric_tuning.py'),
        '--output',str(output/'numeric-audit.json'),'--controls',str(ROOT/'config/mgs5vr-controls.ini'),'--checker',str(checker)],check=True)
    (output/'catalog.json').write_text(json.dumps(catalog,indent=2),encoding='utf-8')
    print(json.dumps({'output':str(output),'lessons':len(available),'missing':missing,'uniqueVideos':len({l['media'] for l in available})}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--export-catalog-from',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--checker',type=Path)
    args=parser.parse_args()
    if args.export_catalog_from:export_source(args.export_catalog_from)
    build(args.output.resolve(),args.checker)
