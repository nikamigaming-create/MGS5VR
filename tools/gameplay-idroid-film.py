"""Record a guarded current-state iDroid film probe through existing VR input.

Run only after the current runner's input lease has ended. This script never
launches the game, changes saves/settings or supplies native actor mutations.
"""
import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path

from gameplay_bot.core import Events, atomic_json
from gameplay_bot.idroid_film import bindings_contract, film_idroid
from gameplay_bot.live import InputLease, Live, ROOT, game_identity
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.recording import Recording


def validate_output(output):
    """Admit a fresh output or only the workspace runner's unopened envelope."""
    output=Path(output).absolute()
    for node in (output,*output.parents):
        if node.is_symlink() or (node.exists() and getattr(node.lstat(),'st_file_attributes',0)&0x400):
            raise ValueError('Linked output paths cannot own film evidence')
    if not output.exists():return
    if not output.is_dir():raise ValueError('Film output must be a fresh directory')
    entries=list(output.iterdir())
    if any(p.name not in {'managed-run.json','runner.log'} or not p.is_file() or p.is_symlink() or
           getattr(p.lstat(),'st_file_attributes',0)&0x400 for p in entries):
        raise ValueError('Film output already has evidence or unowned files; it cannot be resumed or overwritten')
    managed=output/'managed-run.json'
    if not managed.is_file():raise ValueError('Existing film output requires workspace-created managed-run.json')
    envelope=json.loads(managed.read_text(encoding='utf-8-sig'))
    if not isinstance(envelope,dict) or envelope.get('schema')!=1 or not isinstance(envelope.get('source'),dict) or not envelope.get('started_utc'):
        raise ValueError('Film output has an invalid managed-run envelope')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('game-dir','proxy','output','navigation-manifest'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--goal-node',type=int,required=True,help='Reviewed nearby connected native floor node')
    parser.add_argument('--expected-mission',type=int,required=True)
    parser.add_argument('--expected-location',type=int,required=True)
    parser.add_argument('--expected-sequence',required=True)
    parser.add_argument('--seconds',type=float,default=90.)
    parser.add_argument('--record',action='store_true',required=True)
    args=parser.parse_args()
    try:
        validate_output(args.output)
        if args.goal_node<0 or not math.isfinite(args.seconds) or not 15.<=args.seconds<=90.:
            raise ValueError('Use a nonnegative reviewed goal node and 15..90 seconds')
        if (args.expected_mission,args.expected_location,args.expected_sequence)!=(30010,10,'Seq_Game_FreePlay'):
            raise ValueError('This current-state film case is limited to 30010/10/Seq_Game_FreePlay')
    except (ValueError,OSError) as error:parser.error(str(error))
    manifest=json.loads(args.navigation_manifest.read_text(encoding='utf-8-sig'))
    atlas=NavigationAtlas(manifest,json.loads((ROOT/'tools/gameplay_bot/catalogs/native-worlds.json').read_text()),base=args.navigation_manifest.parent)
    world=(args.expected_mission,args.expected_location,args.expected_sequence)
    nav=atlas.select(args.expected_location)
    with InputLease():
        validate_output(args.output)
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),'--bindings-json',
                          str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        bindings_contract(bindings)
        events=Events(args.output,identity);atomic_json(args.output/'bindings.json',bindings)
        sources=[ROOT/'tools/gameplay-idroid-film.py',ROOT/'tools/gameplay_bot/idroid_film.py']
        atomic_json(args.output/'film-case-source.json',{'schema':1,'source_hashes':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
                    'capture_scope':'native source-eye AV during action; neutral final compositor stills separately',
                    'requested_behavior_or_physical_headset_accepted':False})
        live=Live(args.proxy,args.game_dir,bindings,events);recording=None
        try:
            live.ready();live.release()
            recording=Recording(args.game_dir,events,identity['process']['ProcessId'],segment_seconds=12.)
            recording.start();live.background_check=recording.check;live.held_action_recording=recording
            result=film_idroid(live,nav,enemy_script=(ROOT/'tools/navigation-state.lua').read_text(),world=world,
                         goal_node=args.goal_node,output=args.output,max_seconds=args.seconds)
        finally:
            live.background_check=None;live.held_action_recording=None
            try:
                if recording is not None:recording.close()
            finally:live.close()
    print(json.dumps({k:result.get(k) for k in ('status','error','requested_walk_while_open_native_forward_displacement_observed',
          'requested_release_to_walk_native_forward_displacement_observed','visual_acceptance')}))
    return 1 if result['status']=='failed' else 0


if __name__=='__main__':raise SystemExit(main())
