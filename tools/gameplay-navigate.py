"""Drive the installed game over its owned NAV2 graph with ordinary VR inputs."""
import argparse
import json
import subprocess
from pathlib import Path

from gameplay_bot.core import Events
from gameplay_bot.live import Live, InputLease, game_identity
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.perception import read_enemy_snapshot
from gameplay_bot.recording import Recording
from gameplay_bot.route import navigate, set_posture

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--proxy', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--goal', type=float, nargs=3, required=True)
    parser.add_argument('--mode', choices=('auto','stealth','walk','run','crouch','crawl'), default='auto')
    parser.add_argument('--interrupt-on-danger', action='store_true')
    parser.add_argument('--no-recenter', action='store_true')
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--seconds', type=float, default=0, help='Optional total limit; zero uses native progress guards')
    parser.add_argument('--arrival-radius', type=float, default=3.5,
                        help='Bounded final approach radius; does not change native floor/collision tolerances')
    parser.add_argument('--continue-through-bridge', action='store_true',
                        help='Continue ordinary traversal during the playable bridge event')
    args=parser.parse_args()
    manifest=json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    worlds=json.loads((Path(__file__).parent/'gameplay_bot/catalogs/native-worlds.json').read_text())
    atlas=NavigationAtlas(manifest,worlds,base=args.manifest.parent)
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),
            '--bindings-json',str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        live=Live(args.proxy,args.game_dir,bindings,Events(args.output,identity))
        recording=None
        try:
            live.ready()
            if args.record:
                recording=Recording(args.game_dir,live.events,identity['process']['ProcessId'],segment_seconds=12.)
                recording.start();live.background_check=recording.check;live.held_action_recording=recording
            if args.mode == 'stealth':
                set_posture(live,'crouch')
                live.events.emit('navigation_stealth_entry',state=live.observe(native=True))
            observed=live.observe(native=True)
            location=observed.get('native',{}).get('location')
            nav=atlas.select(location)
            live.events.emit('navigation_world_selected', location=location, map_identity=nav.identity,
                             tiles=len(nav.sources))
            lua=(Path(__file__).parent/'navigation-state.lua').read_text()
            def enemies():
                return read_enemy_snapshot(live.native.read,lua)
            result=navigate(live,nav,args.goal,output=args.output,enemy_reader=enemies,mode=args.mode,max_seconds=args.seconds,
                            obstacle_file=args.manifest.parent/'blocked-edges.json',stop_at_bridge=not args.continue_through_bridge,
                            expected_location=location,arrival_radius=args.arrival_radius,
                            interrupt_on_danger=args.interrupt_on_danger,recenter=not args.no_recenter)
            print(json.dumps({'status':result['status'],'error':result.get('error')}))
        finally:
            # close() releases inside a finally-protected transport shutdown.
            # A release failure must never leave the owned proxy running.
            try:
                live.background_check=None;live.held_action_recording=None
                try:
                    if recording is not None:recording.close()
                finally:live.close()
            except Exception as error:
                live.events.emit('navigation_close_failed', error=str(error))
        return 0 if result['status'] in ('observed_native_route', 'observed_native_bridge_trigger') else 1

if __name__=='__main__':
    raise SystemExit(main())
