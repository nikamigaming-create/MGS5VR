"""Run one bounded native-observed combat policy through VR controls."""
import argparse
import json
import subprocess
from pathlib import Path

from gameplay_bot.combat import engage
from gameplay_bot.core import Events, atomic_json
from gameplay_bot.live import InputLease, Live, game_identity


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('game-dir','proxy','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--seconds',type=float,default=30.)
    parser.add_argument('--camp')
    parser.add_argument('--target', help='One exact native soldier name; visibility and range guards still apply')
    parser.add_argument('--enemy-helicopter',action='store_true',help='Explicit Mission18 typed helicopter test; existing physical VR controls')
    parser.add_argument('--one-handed',action='store_true',help='Explicit helicopter firing-grip-only case; support acceptance remains separate')
    parser.add_argument('--max-rounds',type=int,default=24)
    parser.add_argument('--radius',type=float,default=80.)
    args=parser.parse_args()
    if args.one_handed and not args.enemy_helicopter:parser.error('One-hand case requires --enemy-helicopter')
    if args.enemy_helicopter and (args.camp or args.target):parser.error('Helicopter test cannot also select soldiers')
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),
            '--bindings-json',str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        events=Events(args.output,identity);atomic_json(args.output/'bindings.json',bindings)
        live=Live(args.proxy,args.game_dir,bindings,events)
        try:
            live.ready()
            if args.enemy_helicopter:
                from gameplay_bot.helicopter_combat import engage as engage_helicopter
                result=engage_helicopter(live,output=args.output,max_seconds=args.seconds,max_rounds=args.max_rounds,one_handed=args.one_handed)
            else:
                result=engage(live,output=args.output,enemy_script=(Path(__file__).parent/'navigation-state.lua').read_text(),
                              camp=args.camp,max_seconds=args.seconds,max_rounds=args.max_rounds,radius=args.radius,
                              target_name=args.target)
            print(json.dumps({key:result.get(key) for key in ('status','rounds','outcomes','error')}))
        finally:live.close()
    return 1 if result['status']=='failed' else 0


if __name__=='__main__':raise SystemExit(main())
