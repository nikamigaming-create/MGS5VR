"""GZ operator entry: bounded effective controls, without TPP native addresses."""
import base64
import ctypes
from ctypes import wintypes
import json
import pathlib
import subprocess
import time
from contextlib import ExitStack
from types import SimpleNamespace

from .core import BotFault, atomic_json
from .live import channel, digest, load_tool


def game_identity(game):
    executable = game / 'MgsGroundZeroes.exe'
    expected = '7460d9dba9b6fe34893b5d330aca201983bb1734844f3688acffcc0ab22a1815'
    if digest(executable) != expected:
        raise BotFault('Unsupported Ground Zeroes executable')
    script = ("Get-CimInstance Win32_Process -Filter \"Name='MgsGroundZeroes.exe'\" | "
              "Select-Object ProcessId,ParentProcessId,ExecutablePath,CreationDate | ConvertTo-Json -Compress")
    result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script],
                            capture_output=True, text=True, check=True, timeout=10,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    rows = json.loads(result.stdout or 'null') or []
    if isinstance(rows, dict):
        rows = [rows]
    rows = [r for r in rows if r.get('ExecutablePath') and pathlib.Path(r['ExecutablePath']).resolve() == executable]
    pids = {r['ProcessId'] for r in rows}
    children = [r for r in rows if r['ParentProcessId'] in pids]
    if len(children) != 1:
        raise BotFault('Expected the exact Ground Zeroes launcher and game child')
    return {'game': 'gz', 'process': children[0], 'exe_sha256': expected,
            'dll_sha256': digest(game / 'dinput8.dll'),
            'controls_sha256': digest(game / 'mgs5vr-controls.ini'),
            'config_sha256': digest(game / 'mgs5vr.ini')}


def run(args, identity, bindings, events):
    # One deliberate action between compositor observations. GZ does not
    # borrow TPP's Lua state reader, mission predicates, or scripted routes.
    if args.command not in ('observe', 'run') or args.elliott_data or args.trace_menu_input:
        raise BotFault('GZ currently supports observe or one selected effective-control action')
    if not 0 < args.seconds <= 15:
        raise BotFault('GZ observation must be bounded to 15 seconds')
    selected = None
    pose_case = False
    held_capture = False
    fire_binding = None
    if args.command == 'run':
        case = json.loads(args.suite.read_text(encoding='utf-8'))
        name = case.get('ground_zeroes_action')
        pose_case = case.get('ground_zeroes_pose') in ('hands', 'wrist', 'idroid', 'binoculars')
        held_capture = case.get('ground_zeroes_held_capture') is True
        allowed = {'native.a', 'native.b', 'native.x', 'native.y', 'menus.left_trigger', 'gameplay.ready_weapon', 'gameplay.equip_binoculars', 'binoculars.stow', 'binoculars.zoom', 'system.pause', 'system.idroid', 'system.toggle_vr', 'system.native_buttons'}
        if held_capture and name not in ('menus.left_trigger', 'gameplay.ready_weapon'):
            raise BotFault('Held GZ capture requires a selected weapon-ready control')
        if name not in allowed and not (pose_case and name is None):
            raise BotFault('GZ case must select an available presentation or face-button action')
        selected = next((a for a in bindings['actions'] if a['name'] == name), None)
        if name is not None and (not selected or not selected['bindings']):
            raise BotFault('Selected GZ control is disabled')
        if case.get('ground_zeroes_fire') is True:
            if name != 'gameplay.ready_weapon' or not held_capture:
                raise BotFault('GZ firing requires the selected held weapon-ready action')
            action = next((a for a in bindings['actions'] if a['name'] == 'gameplay.fire_or_cqc'), None)
            if not action or not action['bindings']:
                raise BotFault('GZ firing control is disabled')
            fire_binding = action['bindings'][0]
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k.OpenProcess.restype = wintypes.HANDLE
    k.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = k.OpenProcess(0x100000, False, identity['process']['ProcessId'])
    if not handle:
        raise BotFault('GZ game process exited')
    operator = None
    recording = None
    held = []
    result = {'status': 'failed', 'native_gameplay_state': 'not_inferred'}
    try:
        operator = load_tool('gz_operator', 'record-simulator.py').Operator(args.proxy)
        def call(name, values):
            if k.WaitForSingleObject(handle, 0) != 258:
                raise BotFault('Original GZ process exited')
            return operator.call('openxr_' + name, values, timeout=2. if name == 'set_controller_input' and values.get('value') else 15.)
        def release():
            failed = []
            while held:
                value = held.pop()
                try:
                    call('set_controller_input', {**value, 'value': 0., 'auto_release': False})
                except Exception as error:
                    failed.append((value, error))
            if failed:
                held.extend(value for value, _ in failed)
                raise failed[0][1]
        def capture(label):
            if recording:
                recording.check()
            for eye in ('left', 'right'):
                frame = call('capture_composited_image', {'eye': eye})
                block = next((b for b in frame.get('content', []) if b.get('type') == 'image'), None)
                if block is None:
                    raise BotFault('GZ compositor returned no image')
                path = events.output / f'{label}-{eye}.png'
                path.write_bytes(base64.b64decode(block['data'], validate=True))
                events.emit('capture', eye=eye, path=str(path), sha256=digest(path), source='final_compositor')
        capture('before')
        if args.record:
            from .recording import Recording
            recording = Recording(args.game_dir, events, identity['process']['ProcessId'])
            recording.start()
        if pose_case:
            from .session import preserved_controller_pose, preserved_head_pose, _controller_pose
            from .combat import mul, inverse
            from .live import rotate
            adapter = SimpleNamespace(call=call, release=release, events=events,
                text_result=lambda result: json.loads(next(b['text'] for b in result['content'] if b['type'] == 'text')))
            with ExitStack() as poses:
                for hand in ('left', 'right'):
                    for kind in ('grip', 'aim'):
                        poses.enter_context(preserved_controller_pose(adapter, hand, kind, 'local'))
                if case.get('ground_zeroes_head_orientation') is not None:
                    from .binocular_presentation import _pose
                    saved_head = poses.enter_context(preserved_head_pose(adapter))
                    target_head = _pose({**saved_head, 'orientation': case['ground_zeroes_head_orientation']})
                    call('set_head_pose', {'base_space': 'local', **target_head, 'duration_seconds': .8})
                    time.sleep(.8)
                optic_poses = None
                if case.get('ground_zeroes_pose') == 'binoculars':
                    from .binocular_presentation import inspection_poses, grip_rotation, _relative
                    pairs = {hand: _relative(_controller_pose(adapter, hand, 'aim', 'view'),
                                             _controller_pose(adapter, hand, 'grip', 'view'))
                             for hand in ('left', 'right')}
                    optic_poses = inspection_poses({'position': [0., 0., 0.], 'orientation': [0., 0., 0., 1.]},
                                                  pairs['right'], pairs['left'], grip_rotation(bindings))
                for hand, x in (('left', -.25), ('right', .25)):
                    pair = {kind: _controller_pose(adapter, hand, kind, 'view') for kind in ('grip', 'aim')}
                    if optic_poses:
                        if hand == 'left' and not case.get('ground_zeroes_binocular_support'):
                            continue
                        for kind, target_pose in zip(('grip', 'aim'), optic_poses[hand]):
                            call('set_controller_pose', {**pair[kind], **target_pose, 'duration_seconds': .6})
                        continue
                    wrist = case.get('ground_zeroes_pose') == 'wrist' and hand == 'left'
                    target = [-.1, -.15, -.45] if wrist else [x, -.25, -.4]
                    rotation = [0., 2**-.5, 0., 2**-.5] if wrist else [0., 0., 0., 1.]
                    if case.get('ground_zeroes_pose') == 'idroid' and hand == 'left':
                        # Turn the existing native left-hand cup toward the
                        # viewer. This stages a controller, never the handset.
                        # inverse(wristFromPalm * nativeIdroidLeftBodyInWrist)
                        target = [-.05, -.28, -.65]
                        grip = [.5997454573, -.3260184535, .5887929104, .4328282143]
                        rotation = mul(grip, inverse(pair['grip']['orientation']))
                    for kind, pose in pair.items():
                        offset = rotate(rotation, [a-b for a,b in zip(pose['position'], pair['grip']['position'])])
                        call('set_controller_pose', {**pose,
                            'position': [a+b for a,b in zip(target, offset)],
                            'orientation': mul(rotation, pose['orientation']), 'duration_seconds': .6})
                if optic_poses and case.get('ground_zeroes_binocular_support') is True:
                    support = next((a for a in bindings['actions'] if a['name'] == 'binoculars.support_grip'), None)
                    if not support or not support['bindings']:
                        raise BotFault('Physical binocular supporting grip is disabled')
                    for token in support['bindings'][0]['inputs']:
                        value = channel(token)
                        held.append(value)
                        call('set_controller_input', {**value, 'auto_release': True, 'hold_duration': 15.})
                    events.emit('effective_control_action', name=support['name'], binding=support['bindings'][0])
                time.sleep(args.seconds)
                capture('posed')
                optic_actions = case.get('ground_zeroes_binocular_actions')
                if optic_actions is None:
                    optic_action = case.get('ground_zeroes_binocular_action')
                    if optic_action is None and case.get('ground_zeroes_binocular_zoom') is True:
                        optic_action = 'binoculars.zoom'
                    optic_actions = [optic_action] if optic_action else []
                if not isinstance(optic_actions, list) or len(optic_actions) > 4:
                    raise BotFault('Binocular presentation allows at most four selected actions')
                for optic_action in optic_actions if optic_poses else []:
                    if optic_action not in ('binoculars.zoom', 'binoculars.mark', 'binoculars.clear_mark', 'binoculars.intel'):
                        raise BotFault('Unsupported physical binocular action')
                    action = next((a for a in bindings['actions'] if a['name'] == optic_action), None)
                    if not action or not action['bindings']:
                        raise BotFault('Physical binocular action is disabled')
                    binding = action['bindings'][0]
                    duration = binding['milliseconds'] / 1000. + .15 if binding['gesture'] == 'hold' else .12
                    if duration > 3:
                        raise BotFault('Binocular zoom hold exceeds bounded action duration')
                    try:
                        for token in binding['inputs']:
                            value = channel(token)
                            held.append(value)
                            call('set_controller_input', {**value, 'auto_release': True, 'hold_duration': duration + 1.})
                        time.sleep(duration)
                    finally:
                        release()
                    events.emit('effective_control_action', name=action['name'], binding=binding)
                    time.sleep(1.)
                    capture('zoomed' if optic_action == 'binoculars.zoom' else 'cleared' if optic_action == 'binoculars.clear_mark' else 'action')
                release()
            capture('restored')
        if selected:
            binding = selected['bindings'][0]
            duration = binding['milliseconds'] / 1000. + .15 if binding['gesture'] == 'hold' else .12
            if held_capture:
                duration = 3.
            if duration > 3:
                raise BotFault('Selected GZ hold exceeds the bounded action duration')
            values = [channel(token) for token in binding['inputs']]
            values.sort(key=lambda v: v['component'] not in ('Menu', 'Grip'))
            try:
                for value in values:
                    held.append(value)
                    call('set_controller_input', {**value, 'auto_release': True, 'hold_duration': duration + 1.})
                if held_capture:
                    # A stowed GZ rifle has a native draw transition. Keep
                    # the held control through it before sampling or firing.
                    time.sleep(1.75)
                    if fire_binding:
                        fire_values = [channel(token) for token in fire_binding['inputs']]
                        for value in fire_values:
                            held.append(value)
                            call('set_controller_input', {**value, 'auto_release': True, 'hold_duration': .15})
                        time.sleep(.15)
                        for value in fire_values:
                            call('set_controller_input', {**value, 'value': 0., 'auto_release': False})
                            held.remove(value)
                        events.emit('effective_control_action', name='gameplay.fire_or_cqc', binding=fire_binding, seconds=.15)
                    capture('held')
                else:
                    time.sleep(duration)
            finally:
                release()
            events.emit('effective_control_action', name=selected['name'], binding=binding)
        if not pose_case:
            time.sleep(args.seconds)
        capture('after')
        if recording:
            recording.check()
        result['status'] = 'completed'
    finally:
        try:
            if operator:
                try:
                    if held:
                        release()
                finally:
                    operator.close()
        finally:
            try:
                if recording:
                    result['recording'] = recording.close()
                    if recording.failure_reason(result['recording']):
                        result['status'] = 'failed'
                        raise BotFault(recording.failure_reason(result['recording']))
            finally:
                k.CloseHandle(handle)
                atomic_json(events.output / 'result.json', result)
