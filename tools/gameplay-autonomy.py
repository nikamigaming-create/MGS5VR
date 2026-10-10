"""Run a bounded autonomous field policy with native knowledge and VR input."""
import argparse
import json
import math
import subprocess
from pathlib import Path

from gameplay_bot.autonomy import secure_camp
from gameplay_bot.core import Events, atomic_json
from gameplay_bot.live import InputLease, Live, game_identity
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.recording import Recording


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('game-dir','proxy','output','manifest'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--camp',required=True)
    parser.add_argument('--seconds',type=float,default=180.)
    parser.add_argument('--record',action='store_true',help='Record native source-eye video; final compositor stills remain separate')
    parser.add_argument('--stealth-loadout',type=Path,required=True,help='Reviewed fixture-specific nonlethal wrist selection policy')
    parser.add_argument('--emplacement',type=Path,help='Use this authored mortar after verified guard incapacitation')
    parser.add_argument('--body-handling',action='store_true',help='Attempt one native pickup, mapped carry and drop after securing the camp')
    args=parser.parse_args()
    manifest=json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    worlds=json.loads((Path(__file__).parent/'gameplay_bot/catalogs/native-worlds.json').read_text())
    atlas=NavigationAtlas(manifest,worlds,base=args.manifest.parent)
    stealth_loadout=json.loads(args.stealth_loadout.read_text(encoding='utf-8'))
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),
            '--bindings-json',str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        events=Events(args.output,identity);atomic_json(args.output/'bindings.json',bindings)
        live=Live(args.proxy,args.game_dir,bindings,events)
        recording=None
        try:
            live.ready()
            observed=live.observe(native=True);nav=atlas.select(observed['native']['location'])
            if args.record:
                recording=Recording(args.game_dir,events,identity['process']['ProcessId'],segment_seconds=12.)
                recording.start();live.background_check=recording.check
                live.held_action_recording=recording
                events.emit('action_visual_policy',held_actions='continuous_native_source_eye_video',
                            neutral_checkpoints='final_compositor',final_compositor_acceptance='pending')
            result=secure_camp(live,nav,output=args.output,
                              enemy_script=(Path(__file__).parent/'navigation-state.lua').read_text(),camp=args.camp,
                              max_seconds=args.seconds,obstacle_file=args.manifest.parent/'blocked-edges.json',
                              stealth_loadout=stealth_loadout)
            secured=result['status']=='observed_native_camp_incapacitated'
            if secured and args.body_handling:
                from gameplay_bot.bodies import handle_body, life
                from gameplay_bot.combat import admit
                from gameplay_bot.core import BotFault
                from gameplay_bot.locomotion import _position
                from gameplay_bot.perception import read_enemy_snapshot
                state,anchor=admit(live)
                enemies=read_enemy_snapshot(live.native.read,(Path(__file__).parent/'navigation-state.lua').read_text())
                bodies=sorted((r for r in enemies['soldiers'] if r.get('camp')==args.camp and life(r['life']) in (3,4)),
                              key=lambda r:math.dist(_position(state),r['position']))
                candidates=[result.get('last_cover_goal')]
                candidates += [s.get('goal') for s in reversed(result['stages']) if s.get('kind')=='approach']
                selected=None
                for actor in bodies:
                    for goal in candidates:
                        if goal is None or not 1.5<=math.dist(goal,actor['position'])<=40.:continue
                        try:
                            start=nav.nearest(actor['position'],max_distance=1.25);end=nav.nearest(goal)
                            nav.path(start,end)
                        except BotFault:continue
                        selected=(actor,list(nav.positions[end]));break
                    if selected:break
                if selected is None:
                    # A camp can already be incapacitated on arrival, leaving
                    # no earlier approach/cover destination. Select a genuine
                    # connected local floor rather than abandoning body work.
                    from gameplay_bot.cqc_case import placement_node
                    for actor in bodies:
                        try:
                            _, goal = placement_node(nav, actor['position'])
                        except BotFault:
                            continue
                        selected = (actor, goal)
                        break
                if selected is None:
                    followup={'status':'failed','error':'No exact sleeping body with a connected nearby mapped placement destination'}
                else:
                    followup=handle_body(live,nav,actor=selected[0],cover_goal=selected[1],anchor=anchor,
                        camp=args.camp,enemy_script=(Path(__file__).parent/'navigation-state.lua').read_text(),
                        output=args.output/'body',max_seconds=120.)
                result['body_handling']=followup
                if followup['status']=='failed':result['status']='failed';result['error']=followup['error']
                atomic_json(args.output/'result.json',result)
            if secured and args.emplacement:
                from gameplay_bot.emplacement import use_emplacement
                policy=json.loads(args.emplacement.read_text(encoding='utf-8'))
                followup=use_emplacement(live,nav,policy=policy,camp=args.camp,
                    enemy_script=(Path(__file__).parent/'navigation-state.lua').read_text(),
                    output=args.output/'emplacement',max_seconds=120.,
                    minimum_life=result['before']['native']['player_life'])
                result['emplacement']=followup
                if followup['status']=='failed':
                    result['status']='failed';result.setdefault('error',followup['error'])
                atomic_json(args.output/'result.json',result)
            print(json.dumps({key:result.get(key) for key in ('status','stages','error')}))
        finally:
            live.background_check=None
            live.held_action_recording=None
            try:
                if recording is not None:recording.close()
            finally:live.close()
    return 1 if result['status']=='failed' else 0


if __name__=='__main__':raise SystemExit(main())
