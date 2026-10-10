"""Record one native emplacement diagnostic through the existing VR controls."""
import argparse
import json
import math
import subprocess
from pathlib import Path
from gameplay_bot.core import Events, atomic_json
from gameplay_bot.emplacement import use_emplacement
from gameplay_bot.live import InputLease, Live, ROOT, game_identity
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.recording import Recording

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('game-dir','proxy','output','manifest','policy'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--seconds',type=float,default=0.,help='Optional total limit; zero keeps progressing routes live')
    parser.add_argument('--record',action='store_true')
    parser.add_argument('--diagnostic-active-camp',action='store_true')
    args=parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds < 0:parser.error('Emplacement duration must be finite and nonnegative')
    policy=json.loads(args.policy.read_text())
    manifest=json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    atlas=NavigationAtlas(manifest,json.loads((ROOT/'tools/gameplay_bot/catalogs/native-worlds.json').read_text()),base=args.manifest.parent)
    nav=atlas.select(policy['location'])
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),'--bindings-json',
            str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        events=Events(args.output,identity);atomic_json(args.output/'bindings.json',bindings)
        live=Live(args.proxy,args.game_dir,bindings,events);recording=None
        try:
            live.ready()
            if args.record:
                recording=Recording(args.game_dir,events,identity['process']['ProcessId'],segment_seconds=12.)
                recording.start();live.background_check=recording.check;live.held_action_recording=recording
            result=use_emplacement(live,nav,policy=policy,camp='afgh_slopedWest_ob',
                enemy_script=(ROOT/'tools/navigation-state.lua').read_text(),output=args.output,
                max_seconds=args.seconds,diagnostic_active_camp=args.diagnostic_active_camp)
        finally:
            live.background_check=None;live.held_action_recording=None
            try:
                if recording is not None:recording.close()
            finally:live.close()
    print(json.dumps({k:result.get(k) for k in ('status','error','seconds','diagnostic_active_camp')}))
    return 1 if result['status']=='failed' else 0

if __name__=='__main__':raise SystemExit(main())
