"""Maintained MGS5VR observation/action runner. No desktop input or synthetic footage."""
import argparse
import json
import math
import pathlib
import subprocess
import sys
import time
from contextlib import ExitStack

from gameplay_bot.core import Behaviors, BotFault, Events, atomic_json
from gameplay_bot.live import InputLease, Live, ROOT, digest, game_identity, rotate
from gameplay_bot.recording import Recording, after_verified_arrival
from gameplay_bot.locomotion import move_local
from gameplay_bot.session import preserved_controller_pose, preserved_tracking_poses, run_suite
from gameplay_bot.startup import advance_startup, capture_startup_baseline, wait_for_continue_rack, capture_continue_rack
from gameplay_bot.campaign import read_json, run_campaign, validate_campaign, unstarted_campaign
from gameplay_bot.idroid import inspect_idroid
from gameplay_bot.optic_exposure import inspect_optic_exposure
from gameplay_bot.supervisor import run_supervised
from gameplay_bot.state_graph import StateGraph, declared_model
from gameplay_bot.motion import _blend


def continue_game(live, behavior):
    continued_from_title = False
    controller_pose_restore = None
    state = advance_startup(live)
    if state["scene"] == "title":
        state = wait_for_continue_rack(live, state)
        ready_until=time.monotonic()+30.
        while not (state.get('opening_focus',{}).get('ready')
                and state.get('opening_bench',{}).get('layout',{}).get('verified')):
            if time.monotonic()>ready_until or state.get('scene')!='title':
                raise BotFault('The physical native title/player did not finish becoming ready')
            time.sleep(.1);state=live.observe(native=True)
        if state.get('opening_native_menu'):
            live.execute({'op':'action','name':'system.idroid'})
            state=behavior.wait_for({'opening_native_menu':False},5,'optional-cassette-mode')
        origin, orientation = state["opening_position"], state["opening_orientation"]
        from gameplay_bot.combat import controller_pair, inverse, mul, paired_aim_position, pointing
        from gameplay_bot.session import _controller_pose
        from gameplay_bot.cabin_workbench import compose
        flat = [-.7071067812, 0., 0., .7071067812]
        with preserved_tracking_poses(live):
            saved_pose = _controller_pose(live,'right','grip','local')
            pair = controller_pair(saved_pose, _controller_pose(live, 'right', 'aim', 'local'))
            def move_to(local, seconds=.55, rotation=None):
                start_pose = _controller_pose(live, 'right', 'grip', 'local')
                target = {'position': [a+b for a,b in zip(origin,rotate(orientation,local))],
                          'orientation': mul(orientation,rotation or flat)}
                began = time.monotonic()
                for packet in range(1, 11):
                    pose = _blend(start_pose,target,packet/10)
                    aim_q = mul(pose['orientation'],inverse(pair['orientation']))
                    live.call('set_controller_pose',{'hand':'right','pose_type':'grip','base_space':'local',**pose})
                    live.call('set_controller_pose',{'hand':'right','pose_type':'aim','base_space':'local',
                        'position':paired_aim_position(pose['position'],aim_q,pair),'orientation':aim_q})
                    time.sleep(max(0.,began+seconds*packet/10-time.monotonic()))
            live.release()
            bench = state.get('opening_bench') or {}
            tapes = bench.get('tapes') or []
            if len(tapes)!=6 or not state.get('opening_focus',{}).get('ready'):
                raise BotFault('Physical cassette state or native title focus unavailable')
            layout=bench.get('layout') or {}
            if not layout.get('verified') or not isinstance(layout.get('slot'),dict):
                raise BotFault('The cassette player lacks its witnessed native seat layout')
            # Both tape and player must be within the anatomical arm's reach.
            # Approach the witnessed furniture before sending grip movement.
            center=[(a+b)/2 for a,b in zip(layout['seat']['position'],layout['slot']['position'])]
            room={'position':origin,'orientation':orientation}
            contact=compose(room,{'position':center,'orientation':[0.,0.,0.,1.]})['position']
            position=compose(room,{'position':[center[0],center[1]+.55,center[2]+.40],
                'orientation':[0.,0.,0.,1.]})['position']
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            target={'position':position,'orientation':pointing([a-b for a,b in zip(contact,position)])}
            grips={hand:_controller_pose(live,hand,'grip','local') for hand in ('left','right')}
            aims={hand:_controller_pose(live,hand,'aim','local') for hand in ('left','right')}
            for n in range(1,9):
                moved=_blend(head,target,n/8)
                live.call('set_head_pose',{'base_space':'local',**moved})
                delta=[a-b for a,b in zip(moved['position'],head['position'])]
                for hand in ('left','right'):
                    for kind,pose in (('grip',grips[hand]),('aim',aims[hand])):
                        live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',
                            'position':[a+b for a,b in zip(pose['position'],delta)],'orientation':pose['orientation']})
                time.sleep(.03)
            time.sleep(.25)
            state=live.observe()
            origin,orientation=state['opening_position'],state['opening_orientation']
            tapes=state['opening_bench']['tapes'];layout=state['opening_bench']['layout']
            slot=layout['slot'];slot_position=slot['position'];slot_rotation=slot['orientation']
            move_to(tapes[0]['position'],rotation=tapes[0]['orientation'])
            capture_continue_rack(live)
            live.input([{'hand':'right','component':'Grip','value':1.}],.1,lease_seconds=5.)
            time.sleep(.12)
            if live.observe().get('opening_bench',{}).get('tapes',[{}])[0].get('hand')!=1:
                raise BotFault('Continue cassette did not attach to the physical grip')
            move_to([tapes[0]['position'][0],tapes[0]['position'][1]+.18,tapes[0]['position'][2]],rotation=tapes[0]['orientation'])
            for distance in (.10,.05,0.):
                lift=rotate(slot_rotation,[0.,0.,distance])
                move_to([a+b for a,b in zip(slot_position,lift)],rotation=slot_rotation)
            live.release()
            live.events.emit('semantic_action',action='opening.continue',
                             label='GRAB CONTINUE / INSERT INTO PLAYER / RELEASE')
            behavior.wait_for({'title':False},60,'leave-title')
            controller_pose_restore = saved_pose
        continued_from_title = True
    state = live.observe()
    if state["loading"]:
        live.capture("native-loading")
        deadline = time.monotonic() + 90
        while True:
            state = live.observe(native=True)
            if not state["loading"]:
                break
            if state["native"].get("loading_wait_confirm") is True:
                live.capture("native-resume-ready")
                live.execute({"op": "action", "name": "menus.confirm", "native_before": {"loading_wait_confirm": True}})
                behavior.wait_for({"loading": False}, 60, "resume-to-player")
                break
            if time.monotonic() >= deadline:
                raise BotFault("Native loading never became ready; confirm was not sent")
            time.sleep(.25)
    # Cabin is a real native destination but must not be called field gameplay.
    state = live.observe()
    destination = "cabin" if state.get("cabin") else "gameplay"
    behavior.wait_for({"scene": destination}, 90, "accepted-player-camera")
    state = live.observe(native=True)
    if state["native"].get("title") not in (False, 0) or not isinstance(state["native"].get("mission"), int):
        raise BotFault("Native scene does not confirm gameplay arrival")
    captures = live.capture("gameplay-arrival")
    return {"status": "observed_arrival", "state": state, "captures": captures,
            "continued_from_title": continued_from_title,
            "controller_pose_restored": controller_pose_restore is not None,
            "controller_pose_restore": controller_pose_restore,
            "visual_acceptance": "pending"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("observe", "showcase", "showcase-setup", "title", "cold-start", "cinematic", "profile", "bench", "bench-film", "studio-manager", "controls-editor", "body", "studio-field", "studio-board", "studio-hands", "continue", "run", "session", "move", "campaign", "idroid", "optics", "supervise", "state", "cqc", "recon"),
                        help="session advances Continue and runs the whole suite on one connection")
    parser.add_argument("--game-dir", required=True, type=pathlib.Path)
    parser.add_argument("--proxy", type=pathlib.Path)
    parser.add_argument("--elliott-data", type=pathlib.Path, help="Isolated Elliott runtime IPC directory with PID-bound pose readback")
    parser.add_argument("--controls-tool", type=pathlib.Path, default=ROOT / "build/Release/mgs5vr_controls.exe")
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--suite", type=pathlib.Path)
    parser.add_argument("--campaign", type=pathlib.Path, default=ROOT / "tools/gameplay_bot/campaigns/community.json")
    parser.add_argument("--resume", type=pathlib.Path, help="Campaign checkpoint; requires identical executable, DLL and both INIs")
    parser.add_argument("--transition", help="state: explicitly selected graph transition; Pause requires its own named transition")
    parser.add_argument("--seconds", type=float, help="supervise defaults to 900 seconds; other commands default to 1")
    parser.add_argument('--idle-seconds', type=float, default=10., help='Supervisor no-decision handoff window')
    parser.add_argument("--distance", type=float, default=.35, help="move: short forward target in native units, below one unit")
    parser.add_argument("--record", action="store_true", help="rotate native source-eye/audio takes; final compositor stills remain separate")
    parser.add_argument("--trace-menu-input", action="store_true", help="Arm the bounded 15-second native UI input observer for one explicit run/session suite")
    args = parser.parse_args()
    if not args.elliott_data and not args.proxy:
        parser.error("Provide --proxy or --elliott-data")
    for label, candidate in (("--proxy", args.proxy), ("--controls-tool", args.controls_tool)):
        if candidate is None:
            continue
        if not candidate.is_absolute():
            parser.error(f"{label} must be an absolute path; received: {candidate}")
        if not candidate.is_file():
            parser.error(f"{label} executable does not exist: {candidate}")
    args.proxy = args.proxy.resolve() if args.proxy else None
    args.controls_tool = args.controls_tool.resolve()
    if args.seconds is None:
        args.seconds = 0. if args.command == "supervise" else 15. if args.command == "state" else 1.
    if (not math.isfinite(args.seconds) or args.seconds < 0
            or (not args.seconds and args.command != 'supervise')):
        parser.error('--seconds must be positive; supervise also accepts zero for no total timer')
    if args.command in ("run", "session", "cold-start", "cqc", "recon") and not args.suite:
        parser.error(args.command + " requires --suite")
    if args.trace_menu_input and args.command not in ("run", "session"):
        parser.error('--trace-menu-input requires one explicit run/session suite')
    if args.resume and args.command != "campaign":
        parser.error("--resume is only supported by campaign")
    if (args.command == 'state') != bool(args.transition):
        parser.error('state requires --transition; other commands do not accept it')
    campaign = read_json(args.campaign) if args.command == "campaign" else None
    if campaign:
        validate_campaign(campaign, ROOT)
    with InputLease():
        identity = game_identity(args.game_dir)
        bindings = json.loads(subprocess.check_output([str(args.controls_tool), "--bindings-json",
                             str(args.game_dir / "mgs5vr-controls.ini")], text=True, encoding="utf-8"))
        identity["bindings_tool_sha256"] = digest(args.controls_tool)
        events = Events(args.output, identity)
        atomic_json(args.output / "bindings.json", bindings)
        live = recording = pose_cleanup = None
        result = {"status": "failed"}
        try:
            operator = None
            if args.elliott_data:
                from gameplay_bot.elliott import ElliottOperator
                operator = ElliottOperator(args.elliott_data, identity["process"]["ProcessId"])
                identity["simulator_backend"] = "Elliott isolated file IPC"
                identity["simulator_data"] = str(args.elliott_data.resolve())
                atomic_json(args.output / "identity.json", identity)
            live = Live(args.proxy, args.game_dir, bindings, events, operator=operator)
            behavior = Behaviors(live, events)
            def start_recording():
                nonlocal recording
                if not args.record or recording is not None:
                    return
                recording = Recording(args.game_dir, events, identity["process"]["ProcessId"])
                recording.start()
                live.background_check = recording.check
                live.pose_recording = True
                live.pose_snapshot("pre-suite-head-and-hands")
            # Preserve evidence of the simulator's authored/current head and
            # controller transforms before ready() has any chance to seed
            # simulated tracking in a newly relaunched session.
            if args.record:
                live.pose_snapshot("pre-readiness-existing-pose")
            live.ready()
            live.release()
            pose_cleanup = ExitStack()
            pose_cleanup.enter_context(preserved_tracking_poses(live))
            baseline = capture_startup_baseline(live)
            print(json.dumps({"event": "baseline", "captures": baseline}), flush=True)
            if args.trace_menu_input:
                trace, timing = live.native.read('trace-native-menu-input')
                events.emit('menu_input_trace_armed', observation=trace, transport=timing,
                            scope='15-second diagnostic; native query behavior unchanged')
            if args.command == "observe":
                start_recording()
                deadline = time.monotonic() + args.seconds
                last_scene = None
                next_capture = time.monotonic() + 1.
                capture_number = 0
                while True:
                    state = live.observe()
                    if state["scene"] != last_scene:
                        print(json.dumps({"event": "state", "state": state}), flush=True)
                        last_scene = state["scene"]
                    if time.monotonic() >= deadline:
                        break
                    if time.monotonic() >= next_capture:
                        live.capture(f"observe-{capture_number}")
                        capture_number += 1
                        next_capture = time.monotonic() + 1.
                    time.sleep(.25)
                result = {"status": "observed", "state": state, "baseline": baseline}
            elif args.command == "cold-start":
                from gameplay_bot.cold_start import advance_cold_start
                result = advance_cold_start(live, read_json(args.suite), args.seconds)
            elif args.command == "cinematic":
                from gameplay_bot.cinematic import inspect_cinematic
                start_recording()
                result = inspect_cinematic(live, args.seconds, read_json(args.suite) if args.suite else None)
            elif args.command == "profile":
                from gameplay_bot.profile import inspect_profile
                result = inspect_profile(live, args.seconds, read_json(args.suite))
            elif args.command == "title":
                state = advance_startup(live, timeout=args.seconds)
                if state["scene"] != "title" or not state.get("title_menu"):
                    raise BotFault("Title inspection requires the witnessed native title menu")
                state = wait_for_continue_rack(live, state)
                result = {"status": "observed_title", "state": state,
                          "captures": capture_continue_rack(live), "visual_acceptance": "pending"}
            elif args.command in ("bench","bench-film"):
                from gameplay_bot.cabin_workbench import workbench
                result=workbench(live,timeout=args.seconds,film=args.command=='bench-film')
            elif args.command == "studio-board":
                from gameplay_bot.cabin_workbench import inspect_persisted_board
                result=inspect_persisted_board(live,timeout=args.seconds)
            elif args.command == "body":
                from gameplay_bot.body import inspect_body
                policy=read_json(args.suite) if args.suite else {}
                arrival=continue_game(live,behavior) if policy.get('enter',True) else None
                start_recording()
                result=inspect_body(live,args.seconds,policy)
                if arrival:result['arrival']=arrival
            elif args.command == "controls-editor":
                from gameplay_bot.controls_editor import inspect_controls_editor
                policy=read_json(args.suite) if args.suite else {'scene':'cabin','enter':True}
                if policy.get('enter'):
                    arrival=continue_game(live,behavior)
                else:
                    current=live.observe(native=True)
                    arrival=({'status':'current_scene','state':current} if current['scene']==policy['scene']
                        or (policy.get('restore_vr_only') and policy['scene']=='cabin' and current.get('cabin'))
                        else advance_startup(live,timeout=args.seconds))
                result=inspect_controls_editor(live,policy,args.seconds);result['arrival']=arrival
            elif args.command == "studio-manager":
                from gameplay_bot.studio_manager import inspect_camera_manager
                policy=read_json(args.suite) if args.suite else {'scene':'title','mode':'gallery'}
                if policy.get('enter'):
                    arrival=continue_game(live,behavior)
                else:
                    advance_startup(live,timeout=args.seconds)
                    arrival=None
                result=inspect_camera_manager(live,policy,args.seconds)
                if arrival:result['arrival']=arrival
            elif args.command == "studio-field":
                from gameplay_bot.cabin_field import inspect_field_studio
                arrival=continue_game(live,behavior)
                result=inspect_field_studio(live,args.seconds)
                result['arrival']=arrival
            elif args.command == "studio-hands":
                from gameplay_bot.cabin_field import inspect_studio_hands
                arrival=continue_game(live,behavior)
                result=inspect_studio_hands(live,args.seconds)
                result['arrival']=arrival
            elif args.command == "continue":
                result = continue_game(live, behavior)
                if result.get("status") == "observed_arrival":
                    start_recording()
            elif args.command == "move":
                start_recording()
                # Match walk_local's live-calibrated short pulses. The old
                # 0.15 brake value stalls inside the native walking deadzone.
                result = move_local(live, target_distance=args.distance,
                                    brake_magnitude=.35, pulse_seconds=.1)
                result["captures"] = live.capture("movement-after-release")
            elif args.command == "idroid":
                start_recording()
                result = inspect_idroid(live, behavior)
            elif args.command == "cqc":
                from gameplay_bot.cqc import run_named_cqc
                start_recording()
                result = run_named_cqc(live, read_json(args.suite), output=args.output,
                    enemy_script=(ROOT / 'tools/navigation-state.lua').read_text())
            elif args.command == "recon":
                from gameplay_bot.recon import run_named_recon
                start_recording()
                result = run_named_recon(live, read_json(args.suite), output=args.output,
                    enemy_script=(ROOT / 'tools/navigation-state.lua').read_text(), max_seconds=args.seconds)
            elif args.command == "showcase-setup":
                from gameplay_bot.showcase import setup_weapon
                result = setup_weapon(live, read_json(args.suite))
            elif args.command == "showcase":
                from gameplay_bot.showcase import run as run_showcase
                start_recording()
                result = run_showcase(live, behavior, read_json(args.suite), args.seconds)
            elif args.command == "optics":
                start_recording()
                result = inspect_optic_exposure(live,args.seconds)
            elif args.command == "supervise":
                start_recording()
                initial_suite = read_json(args.suite) if args.suite else None
                if initial_suite is not None:
                    initial_suite = {**initial_suite, "source_path": str(args.suite.resolve()), "source_sha256": digest(args.suite)}
                result = run_supervised(behavior, seconds=args.seconds, initial_suite=initial_suite,
                                        idle_seconds=args.idle_seconds)
            elif args.command == 'state':
                graph = StateGraph(ROOT, declared_model(ROOT), identity, bindings)
                suite = graph.probe_suite(args.transition, identity=identity, max_wait=min(args.seconds, 600))
                atomic_json(args.output/'state-plan.json', suite)
                def state_checkpoint(records):
                    atomic_json(args.output/'queue.json', {'cases': records, 'graph_identity': graph.identity})
                    print(json.dumps({'event':'state_transition', 'id':records[-1]['id'],
                                      'status':records[-1]['status']}), flush=True)
                enter_game = lambda: continue_game(live, behavior)
                if args.record:
                    enter_game = after_verified_arrival(enter_game, start_recording)
                result = run_suite(behavior, suite, state_checkpoint, enter_game)
                result.update(graph_identity=graph.identity, selected_transition=args.transition,
                              transition_ids=suite['transition_ids'], neutral_exit=suite['neutral_exit'],
                              scope=suite['scope'], full_game_acceptance=False)
            elif args.command == "campaign":
                enter_game = lambda: continue_game(live, behavior)
                if args.record:
                    enter_game = after_verified_arrival(enter_game, start_recording)
                def campaign_checkpoint(payload):
                    atomic_json(args.output / "campaign.json", payload)
                    if payload["suites"]:
                        latest = payload["suites"][-1]
                        print(json.dumps({"event": "campaign_checkpoint", "id": latest["id"],
                                          "status": latest["status"]}), flush=True)
                result = run_campaign(behavior, campaign, ROOT, campaign_checkpoint, identity,
                                      enter_game=enter_game,
                                      resume=read_json(args.resume) if args.resume else None)
            else:
                suite = json.loads(args.suite.read_text(encoding="utf-8-sig"))
                suite_hash = digest(args.suite)
                def checkpoint(records):
                    atomic_json(args.output / "queue.json", {"suite_sha256": suite_hash, "results": records})
                    latest = records[-1]
                    print(json.dumps({"event": "case_finished", "id": latest["id"], "status": latest["status"]}), flush=True)
                enter_game = None
                if args.command == "session":
                    enter_game = lambda: continue_game(live, behavior)
                    if args.record:
                        enter_game = after_verified_arrival(enter_game, start_recording)
                else:
                    start_recording()
                result = run_suite(behavior, suite, checkpoint,
                                   enter_game)
            if recording and result.get("status") in ("observed_pass", "observed_arrival", "observed"):
                live.release()
                recording.capture_tail()
        except Exception as error:
            if campaign:
                checkpoint_path = args.output / "campaign.json"
                result = read_json(checkpoint_path) if checkpoint_path.is_file() else unstarted_campaign(campaign, ROOT, identity, error)
                result.update(status="failed", error=str(error), release_ready=False)
            elif args.command in ('supervise', 'state'):
                checkpoint_path = args.output / ('queue.json' if args.command == 'state' else 'supervised-cases.json')
                result = read_json(checkpoint_path) if checkpoint_path.is_file() else {"cases": []}
                result.update(status="failed", error=str(error), release_ready=False)
            else:
                result = {"status": "failed", "error": str(error)}
        finally:
            if args.trace_menu_input and live:
                try:
                    trace, timing = live.native.read('inspect-native-menu-input')
                    result['menu_input_trace'] = {'observation':trace, 'transport':timing,
                                                'snapshot_atomic':False}
                except Exception as error:
                    result['menu_input_trace_error'] = str(error)
            if recording:
                try:
                    recording_result = recording.close()
                    result["recording"] = recording_result
                    recording_error = Recording.failure_reason(recording_result)
                    if recording_error:
                        result.update(status="failed", recording_error=recording_error)
                except Exception as error:
                    result.update(status="failed", recording_error=str(error))
            if live:
                try:
                    live.cleanup_menus()
                except Exception as error:
                    result.update(status="failed", cleanup_error=str(error))
                if pose_cleanup:
                    try:
                        pose_cleanup.close()
                    except Exception as error:
                        result.update(status="failed", pose_restore_error=str(error))
                try:
                    # Cleanup has already been attempted above. A failed or
                    # ambiguous unwind must not authorize another input chain.
                    live.close(cleanup=False)
                except Exception as error:
                    result.update(status="failed", cleanup_error=str(error))
            events.emit("run_finished", result=result)
            atomic_json(args.output / "result.json", result)
        if args.command == "supervise":
            print(json.dumps({"status": result["status"], "cases": len(result.get("cases", [])),
                              "error": result.get("error"), "result": str(args.output / "result.json")}), flush=True)
        else:
            print(json.dumps(result), flush=True)
        return 1 if result["status"] == "failed" else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (BotFault, OSError, ValueError, subprocess.SubprocessError) as error:
        print(json.dumps({"status": "failed", "error": str(error)}), file=sys.stderr)
        raise SystemExit(1)
