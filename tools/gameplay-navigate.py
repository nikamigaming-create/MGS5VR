"""Drive the installed game over its owned NAV2 graph with ordinary VR inputs."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from gameplay_bot.core import Events
from gameplay_bot.live import Live, InputLease, game_identity
from gameplay_bot.navigation import NativeMap
from gameplay_bot.route import navigate

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--proxy', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--goal', type=float, nargs=3, required=True)
    parser.add_argument('--mode', choices=('auto','walk','run','crouch','crawl'), default='auto')
    parser.add_argument('--seconds', type=float, default=600)
    parser.add_argument('--continue-through-bridge', action='store_true',
                        help='Continue ordinary traversal during the playable bridge event')
    args=parser.parse_args()
    manifest=json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    for entry in manifest:
        if entry['path'].endswith('.nav2'):
            if hashlib.sha256(Path(entry['file']).read_bytes()).hexdigest() != entry['sha256']:
                raise ValueError('Owned navigation asset changed: '+entry['file'])
    nav=NativeMap.from_files([e['file'] for e in manifest if e['path'].endswith('.nav2')])
    with InputLease():
        identity=game_identity(args.game_dir)
        bindings=json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),
            '--bindings-json',str(args.game_dir/'mgs5vr-controls.ini')],text=True))
        live=Live(args.proxy,args.game_dir,bindings,Events(args.output,identity))
        try:
            live.ready()
            lua=(Path(__file__).parent/'navigation-state.lua').read_text()
            def enemies():
                value,_=live.native.read(lua)
                return value
            result=navigate(live,nav,args.goal,output=args.output,enemy_reader=enemies,mode=args.mode,max_seconds=args.seconds,
                            obstacle_file=args.manifest.parent/'blocked-edges.json',stop_at_bridge=not args.continue_through_bridge)
            print(json.dumps({'status':result['status'],'error':result.get('error')}))
        finally:
            # close() releases inside a finally-protected transport shutdown.
            # A release failure must never leave the owned proxy running.
            try:
                live.close()
            except Exception as error:
                live.events.emit('navigation_close_failed', error=str(error))
        return 0 if result['status'] in ('observed_native_route', 'observed_native_bridge_trigger') else 1

if __name__=='__main__':
    raise SystemExit(main())
