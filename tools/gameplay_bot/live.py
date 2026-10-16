"""Local Windows adapters: native read-only observation and semantic OpenXR input."""
from __future__ import annotations

import base64
import hashlib
import importlib.util
import io
import json
import math
import os
import pathlib
import re
import subprocess
import time

from .core import ActionPrerequisiteChanged, BlankCompositorFrame, BotFault, atomic_json, matches, scene, reviewed_action_contract, REVIEWED_MENU_BUTTONS
from .operator_recovery import CapturePairInvalidated, CaptureRecoveryResult, recover_capture_once
from .startup_evidence import StartupEvidence

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_tool(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path):
    with pathlib.Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def game_identity(game):
    game = pathlib.Path(game).resolve()
    # Exact executable path and process creation time prevent PID reuse. This
    # is read-only process inventory, never Windows keyboard/focus automation.
    script = ("Get-CimInstance Win32_Process -Filter \"Name='mgsvtpp.exe'\" | "
              "Where-Object { $mgsProcess=Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue; $mgsProcess -and !$mgsProcess.HasExited } | "
              "Select-Object ProcessId,ExecutablePath,CreationDate | ConvertTo-Json -Compress")
    result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                            capture_output=True, text=True, check=True, timeout=10,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    rows = json.loads(result.stdout or "null") or []
    if isinstance(rows, dict):
        rows = [rows]
    rows = [r for r in rows if r.get("ExecutablePath") and
            pathlib.Path(r["ExecutablePath"]).resolve() == game / "mgsvtpp.exe"]
    if len(rows) != 1:
        raise BotFault("Expected one live MGSV process at the configured game directory")
    return {"game": "tpp", "process": rows[0],
            "exe_sha256": digest(game / "mgsvtpp.exe"),
            "dll_sha256": digest(game / "dinput8.dll"),
            "controls_sha256": digest(game / "mgs5vr-controls.ini"),
            "config_sha256": digest(game / "mgs5vr.ini")}


class InputLease:
    def __init__(self):
        self.stream = None

    def __enter__(self):
        import msvcrt
        path = pathlib.Path(os.environ["LOCALAPPDATA"]) / "MGS5VR" / "gameplay-bot.lock"
        path.parent.mkdir(parents=True, exist_ok=True)
        self.stream = path.open("a+b")
        self.stream.seek(0, 2)
        if self.stream.tell() == 0:
            self.stream.write(b"0")
            self.stream.flush()
        self.stream.seek(0)
        try:
            msvcrt.locking(self.stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            self.stream.close()
            self.stream = None
            raise BotFault("Another maintained bot owns the input lease") from error
        # Older private tools do not participate in this lease. Refuse known
        # legacy input owners rather than pretending they have been excluded.
        script = "Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | Where-Object { $_.CommandLine -match 'gameplay-runner\\.py' } | Select-Object -ExpandProperty ProcessId"
        try:
            result = subprocess.run(["powershell.exe", "-NoProfile", "-Command", script],
                                    capture_output=True, text=True, check=True, timeout=10,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            if result.stdout.strip():
                raise BotFault("Legacy gameplay runner is active; release that owner first")
        except BaseException:
            self.__exit__(None, None, None)
            raise
        return self

    def __exit__(self, *_):
        if self.stream:
            import msvcrt
            self.stream.seek(0)
            msvcrt.locking(self.stream.fileno(), msvcrt.LK_UNLCK, 1)
            self.stream.close()
            self.stream = None


class NativeReader:
    def __init__(self, timeout=2.):
        self.module = load_tool("mgs5vr_native_reader", "native-actions.py")
        self.timeout = timeout
        self._readonly_scripts = frozenset(("inspect-bot-state",
            (ROOT / "tools/scenario-state.lua").read_text(encoding="utf-8")))

    def read(self, script):
        # Each request is fully correlated; an ambiguous response is never
        # replayed as an action. Callers submit authored observations or named
        # render diagnostics from an explicitly selected bounded test case.
        return self._read(script, self.timeout)

    def read_startup_observation(self, script):
        # A retail title load can stall the Lua queue beyond its ordinary 2 s
        # budget. Extend only the response deadline of this ONE correlated
        # authored observation; never resubmit a timed-out request. Unrecognized
        # Lua, input:* and diagnostics mutations cannot enter this path.
        if script not in self._readonly_scripts:
            raise BotFault("Startup observation requires an exact known read-only script")
        return self._read(script, 8., readonly_startup=True)

    def _read(self, script, response_timeout, *, readonly_startup=False):
        native = self.module
        request = time.time_ns()
        payload = script.encode("utf-8")
        started = time.monotonic()
        with native.connect(self.timeout) as pipe:
            pipe.write(native.REQUEST.pack(request, len(payload)) + payload)
            deadline = time.monotonic() + response_timeout
            try:
                response, status, length, queued, execution = native.RESPONSE.unpack(
                    native.read_exact(pipe, native.RESPONSE.size, deadline))
                if response != request or length > native.MAX_SCRIPT:
                    raise BotFault("Native response identity/size mismatch")
                body = native.read_exact(pipe, length, deadline).decode("utf-8")
            except RuntimeError as error:
                if readonly_startup and str(error) == "native action response timed out; action completion is unknown":
                    raise BotFault("Native read-only startup observation timed out after 8 seconds; request not replayed") from error
                raise
        if status:
            raise BotFault(f"Native observation rejected: {status}: {body}")
        return json.loads(body), {"seconds": time.monotonic()-started,
                                  "queued_ms": queued, "execution_us": execution,
                                  "response_timeout_seconds": response_timeout,
                                  "readonly_startup": readonly_startup}


def rotate(q, v):
    x, y, z, w = q
    a, b, c = v
    t = [2*(y*c-z*b), 2*(z*a-x*c), 2*(x*b-y*a)]
    return [a+w*t[0]+y*t[2]-z*t[1], b+w*t[1]+z*t[0]-x*t[2], c+w*t[2]+x*t[1]-y*t[0]]


def scripted_horse_fire_owner(state, native_before):
    """Own a longer diagnostic grip/trigger hold only in the retail chase."""
    required = {'mission':10010, 'location':30, 'story':0,
                'sequence':'Seq_Game_VolginRide', 'game_over':0, 'popup':False,
                'hospital_volgin_ride':True, 'weapon_PRIMARY_HIP':70}
    if not isinstance(native_before, dict) or not matches(native_before, required):
        return False
    return (matches(state.get('native', {}), required)
            and state.get('scene') == 'gameplay' and state.get('camera_active') is True
            and all(state.get(k) is False for k in ('menu','pause','idroid','gamepad'))
            and state.get('controls', {}).get('context') == 'gameplay'
            and state.get('controls', {}).get('rig_input') is True)


def channel(token):
    if token.endswith("_touch") or token in ("left_thumbrest", "right_thumbrest"):
        raise BotFault("The installed simulator operator cannot synthesize capacitive touch inputs: " + token)
    faces = {"a": ("right", "A"), "b": ("right", "B"), "x": ("left", "X"),
             "y": ("left", "Y"), "menu": ("left", "Menu")}
    if token in faces:
        hand, component = faces[token]
        return {"hand": hand, "component": component, "value": 1.}
    match = re.fullmatch(r"(left|right)_(grip|trigger|stick_click|stick_up|stick_down|stick_left|stick_right)", token)
    if not match:
        raise BotFault("Unsupported physical input token: " + token)
    hand, kind = match.groups()
    components = {"grip": "Grip", "trigger": "Trigger", "stick_click": "ThumbstickClick"}
    if kind in components:
        return {"hand": hand, "component": components[kind], "value": 1.}
    direction = kind.removeprefix("stick_")
    return {"hand": hand, "component": "Thumbstick", "sub_component": "Y" if direction in ("up", "down") else "X",
            "value": 1. if direction in ("up", "right") else -1.}


class ControlSampleUnavailable(BotFault):
    """A control publication can be reacquired before dispatching any input."""


def authoritative_controls(state):
    """Require the current, fresh sample from the native XR control resolver."""
    controls = state.get("controls")
    if not isinstance(controls, dict):
        raise ControlSampleUnavailable("Native control context is unavailable or stale")
    age = controls.get("age_ms")
    sample_ms = controls.get("sample_ms")
    context = controls.get("context")
    if type(age) is not int or not 0 <= age <= 250 or type(sample_ms) is not int or sample_ms < 0:
        raise ControlSampleUnavailable("Native control context is unavailable or stale")
    if context not in {"gameplay", "equipment", "commands", "binoculars", "menus", "horse", "vehicle", "nativeButtons"}:
        raise BotFault("Native control context is unknown")
    now_ms = state.get("now_ms")
    if now_ms is not None:
        if type(now_ms) is not int or now_ms < sample_ms:
            raise BotFault("Native control sample timestamp is invalid")
        if now_ms - sample_ms > 250 or now_ms - sample_ms != age:
            raise BotFault("Native control sample age is inconsistent")
    return controls


def vehicle_weapon_test_owner(state, native_before, *, steering=False):
    """Exact, stationary driver owner for the bounded semantic firearm probe."""
    required = ('mission', 'location', 'sequence', 'player_vehicle_id', 'status_ON_VEHICLE', 'popup', 'saving')
    if (not isinstance(native_before, dict) or any(key not in native_before for key in required)
            or type(native_before['player_vehicle_id']) is not int
            or not 0 <= native_before['player_vehicle_id'] < 65535
            or native_before['status_ON_VEHICLE'] is not True
            or native_before['popup'] is not False or native_before['saving'] is not False):
        return None
    controls = authoritative_controls(state)
    axes = controls.get('native_axes')
    axes_valid = (isinstance(axes, list) and len(axes) == 4 and axes[1:] == [0, 0, 0]
                  and type(axes[0]) is int and -32767 <= axes[0] <= 32767
                  and (steering or axes[0] == 0))
    frame, native = state.get('rendered') or {}, state.get('native') or {}
    guarded_native = {key: native_before[key] for key in required}
    guarded_native.update({key: value for key, value in native_before.items()
                           if key == 'not_alert' or key.startswith('weapon_')})
    if (state.get('scene') != 'gameplay' or state.get('menu') is not False
            or state.get('idroid') is not False or state.get('gamepad') is not False
            or state.get('camera_active') is not True or state.get('camera_suspended') is not False
            or controls.get('context') != 'vehicle' or controls.get('travel_mode') != 3
            or controls.get('rig_input') is not True or controls.get('native_triggers') != [0, 0]
            or controls.get('native_buttons') != 0 or not axes_valid
            or not matches(native, guarded_native) or frame.get('presentation_focused') is not True
            or any(frame.get('raw_right_' + kind + '_tracked') is not True for kind in ('grip', 'aim'))):
        return None
    owner, activation = frame.get('player_owner'), frame.get('activation')
    stamp, now = frame.get('sample_ms'), state.get('now_ms')
    if (type(owner) is not int or owner <= 0 or type(activation) is not int or activation <= 0
            or state.get('activation') != activation or type(stamp) is not int or type(now) is not int
            or not 0 <= now - stamp <= 150):
        return None
    return owner, activation, native['mission'], native['location'], native['sequence'], native['player_vehicle_id']


def vehicle_weapon_test_ready(state, owner):
    """Native acknowledgement stays authoritative during a bounded shot."""
    frame = state.get('rendered') or {}
    weapon = frame.get('vehicle_weapon') or {}
    stamp, now = weapon.get('time'), state.get('now_ms')
    return (owner is not None
            and all(weapon.get(key) is True for key in ('available', 'requested', 'ready', 'eligible'))
            and (weapon.get('player'), weapon.get('activation')) == owner[:2]
            and type(stamp) is int and type(now) is int and 0 <= now-stamp <= 150)


def vehicle_weapon_test_drawn(state, owner):
    """A native ready request alone does not establish completed weapon draw."""
    observer = (state.get('rendered') or {}).get('weapon_native_observer') or {}
    stamp, now = observer.get('sample_ms'), state.get('now_ms')
    mode, resource = observer.get('mode'), observer.get('resource_handle')
    return (vehicle_weapon_test_ready(state, owner)
            and (state.get('rendered') or {}).get('native_firearm') is True
            and observer.get('source') == 'retained_native_skin_sampling'
            and all(observer.get(key) is True for key in ('sampled', 'component_accepted',
                'instance_index_matched', 'resource_accepted', 'muzzle_matrix_read',
                'attachment_valid', 'muzzle_socket_valid', 'muzzle_solved'))
            # Retail exposes the accepted resource and authored attachment;
            # weapon_model is deliberately null in this observer's schema.
            and type(resource) is int and 0 < resource < 0xffff0000
            and type(mode) is int and mode & 0x04000000 == 0
            and type(stamp) is int and type(now) is int and 0 <= now-stamp <= 150)


def vehicle_wheel_test_owner(state, native_before, *, root=None, grips=False, steering=False):
    """Two-grip inspection owns only a stationary, native wheeled driver."""
    owner = vehicle_weapon_test_owner(state, native_before, steering=steering)
    controls, frame, native = state.get('controls') or {}, state.get('rendered') or {}, state.get('native') or {}
    weapon = frame.get('vehicle_weapon') or {}
    position = tuple(native.get('player_' + axis) for axis in 'xyz')
    if (owner is None or controls.get('native_packet_source') != 'xr_runtime_final_mapped_packet'
            or native.get('player_vehicle_is_vehicle') is not True
            or native.get('player_vehicle_is_helicopter') is not False
            or frame.get('driver_wheel_tracked') is not True
            or any(frame.get('raw_left_' + kind + '_tracked') is not True for kind in ('grip', 'aim'))
            or weapon.get('requested') is not False or weapon.get('ready') is not False
            or any(not finite_number(value) for value in position)
            or (root is not None and math.dist(position, root) > .05)):
        return None
    for key, length in (('physical', 11), ('sticks', 4)):
        values = controls.get(key)
        if not isinstance(values, list) or len(values) != length:
            return None
        for index, value in enumerate(values):
            allowed_grip = grips and key == 'physical' and index in (7, 8)
            if not finite_number(value) or (not 0 <= value <= 1 if allowed_grip else abs(value) > .001):
                return None
    return owner


def fresh_control_observation(observe, *, native=False, timeout=2., clock=None, sleep=None):
    """Wait only for a fresh publication; never replay a gameplay action."""
    clock, sleep = clock or time.monotonic, sleep or time.sleep
    started = clock()
    if not finite_number(started) or not finite_number(timeout) or timeout < 0:
        raise BotFault("Control observation clock or timeout is invalid")
    deadline = started + timeout
    if not finite_number(deadline):
        raise BotFault("Control observation clock or timeout is invalid")
    prior_read_finished = started
    while True:
        read_started = clock()
        if not finite_number(read_started) or read_started < prior_read_finished:
            raise BotFault("Control observation clock is invalid")
        state = observe(native=native)
        read_finished = clock()
        if not finite_number(read_finished) or read_finished < read_started:
            raise BotFault("Control observation clock is invalid")
        prior_read_finished = read_finished
        try:
            controls = authoritative_controls(state)
            # inspect-bot-state precedes the synchronous native Lua query.
            # Include all read latency conservatively, without rewriting the
            # native now/sample/age fields into a different clock domain.
            if controls["age_ms"] + (read_finished - read_started) * 1000 > 250:
                raise ControlSampleUnavailable("Native control publication aged during observation")
            return state, controls
        except ControlSampleUnavailable:
            remaining = deadline - read_finished
            if remaining <= 0:
                raise
            sleep(min(.025, remaining))


def authoritative_context(state):
    """Require the current, fresh context from the native XR control resolver."""
    controls = authoritative_controls(state)
    return controls["context"]


def reviewed_action_packet(state, action, held_token=None):
    """Exact action-mapped A/B or complete sampled release; no RPC-only proof."""
    if action not in REVIEWED_MENU_BUTTONS:
        return False
    controls = authoritative_controls(state)
    if (controls["context"] != "menus"
            or controls.get("native_packet_source") != "xr_runtime_final_mapped_packet"
            or type(controls.get("native_buttons")) is not int
            or controls["native_buttons"] != (REVIEWED_MENU_BUTTONS[action] if held_token else 0)):
        return False
    faces = {"a":0, "b":1, "x":2, "y":3}
    if held_token is not None and held_token not in faces:
        return False
    for name, count in (("physical", 11), ("sticks", 4), ("native_axes", 4), ("native_triggers", 2)):
        values = controls.get(name)
        if not isinstance(values, list) or len(values) != count:
            return False
        for index, value in enumerate(values):
            target = 1. if name == "physical" and held_token and index == faces[held_token] else 0.
            if not finite_number(value) or abs(value-target) > .08:
                return False
    return True


def released_startup_observation(state, held):
    """Admit a longer observation at the reproduced, released title splash."""
    if (held or state.get("scene") != "title" or state.get("title") is not True
            or state.get("title_menu") is not False or state.get("camera_active") is not False
            or any(state.get(key) is not False for key in ("menu", "idroid", "pause"))):
        return False
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return False
    fast_seconds = state.get("transport", {}).get("seconds")
    if (controls["context"] not in ("menus", "nativeButtons")
            or not finite_number(fast_seconds) or fast_seconds < 0
            or controls["age_ms"] + fast_seconds * 1000 > 250
            or type(controls.get("native_buttons")) is not int or controls["native_buttons"] != 0):
        return False
    for key, count in (("physical", 11), ("sticks", 4), ("native_axes", 4), ("native_triggers", 2)):
        values = controls.get(key)
        if (not isinstance(values, list) or len(values) != count
                or any(not finite_number(value) or abs(value) > .001 for value in values)):
            return False
    touches = controls.get("touches")
    return (isinstance(touches, dict) and len(touches) == 10
            and all(finite_number(value) and abs(value) <= .001 for value in touches.values()))


def static_grip_capture_allowed(held, state, bindings):
    """Permit sampled input consumed by an inspection or mapping owner.

    Grips must resolve only to the established weapon-ready/support modifiers
    in the effective layout. The actual XR sample must contain no face button,
    trigger or stick input. The mapping editor instead owns all controls and
    must deliver a neutral native packet. Input expires independently of RPCs.
    """
    editor=state.get('rendered',{}).get('controls_editor') or {}
    if held and editor.get('visible') is True and editor.get('owns_input') is True:
        try:controls=authoritative_controls(state)
        except BotFault:return False
        # The mapping panel consumes every sampled input. Held inspection is
        # allowed only while no native button, stick or trigger is delivered;
        # capture rechecks this owner separately before each compositor eye.
        if (state.get('camera_active') is True and state.get('loading') is False
                and state.get('gamepad') is False and controls.get('native_buttons')==0
                and controls.get('native_axes')==[0,0,0,0] and controls.get('native_triggers')==[0,0]
                and editor.get('live_inputs',0) and controls.get('age_ms',1000)<150):
            return True
    studio=state.get('rendered',{}).get('studio') or {}
    if held and studio.get('available') and state.get('camera_active') is True and state.get('menu') is False \
            and state.get('loading') is False and state.get('gamepad') is False:
        try:controls=authoritative_controls(state)
        except BotFault:return False
        mask=studio.get('input_mask',0);owned=set()
        for side,hand in enumerate(('left','right')):
            if mask&(1<<side):owned.add(hand)
        if owned and all(v.get('hand') in owned and v.get('component')=='Grip' and not v.get('sub_component')
                and finite_number(v.get('value')) and 0<v['value']<=1 for v in held.values()):
            physical=controls['physical'];sticks=controls['sticks']
            if (all(abs(v)<=.01 for i,v in enumerate(physical[:11]) if i not in (7,8))
                    and all(abs(v)<=.01 for v in sticks) and controls.get('native_buttons')==0
                    and controls.get('native_triggers')==[0,0]
                    and held_input_evidence(state,[v['hand']+'_grip' for v in held.values()])):
                return True
    if state.get('controls',{}).get('context')=='binoculars':
        from .binocular_presentation import static_support_capture_allowed
        return static_support_capture_allowed(held,state,bindings)
    handheld_free = (state.get('menu') is True and state.get('idroid') is True
        and state.get('idroid_ui_only_owner') is True and state.get('idroid_handheld') is True
        and state.get('idroid_menu_control') == 1 and state.get('idroid_menu_input_ready') is True
        and state.get('idroid_tutorial_mode') == 0 and state.get('pause') is False
        and state.get('native', {}).get('popup') is False
        and state.get('gamepad') is False and state.get('loading') is False)
    if not held or state.get("camera_active") is not True or (state.get("menu") is not False and not handheld_free):
        return False
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return False
    if controls["context"] != "gameplay" or controls.get("rig_input") is not True:
        return False
    tokens = set()
    for value in held.values():
        if value.get("hand") not in ("left", "right") or value.get("component") != "Grip" or value.get("sub_component"):
            return False
        if not finite_number(value.get("value")) or not 0 < value["value"] <= 1:
            return False
        tokens.add(value["hand"] + "_grip")
    if "right_grip" not in tokens or not held_input_evidence(state, sorted(tokens)):
        return False
    support_borrowed=(handheld_free and tokens=={'left_grip','right_grip'}
            and state.get('rendered',{}).get('idroid_stowed') is True
            and state.get('rendered',{}).get('weapon_support_attached') is True)
    support_without_terminal=(state.get('menu') is False and state.get('idroid') is False
            and tokens=={'left_grip','right_grip'}
            and state.get('rendered',{}).get('weapon_support_attached') is True)
    if handheld_free:
        if (tokens!={'right_grip'} and not support_borrowed) or controls.get('native_triggers')!=[255,0]:
            return False
    physical, sticks = controls["physical"], controls["sticks"]
    if any(abs(value) > .01 for index, value in enumerate(physical[:11]) if index not in (7, 8)) or any(abs(value) > .01 for value in sticks):
        return False
    if controls.get("native_buttons") != 0:
        return False
    for index, token in ((7, "left_grip"), (8, "right_grip")):
        if token not in tokens and abs(physical[index]) > .01:
            return False
    allowed = {"gameplay.ready_weapon", "gameplay.support_grip"}
    matched = set()
    for action in bindings.get("actions", []):
        if "gameplay" not in action.get("contexts", []):
            continue
        for binding in action.get("bindings", []):
            inputs = set(binding.get("inputs", []))
            mapped_touches = [token for token in inputs if token.endswith("_touch") or token.endswith("_thumbrest")]
            if any(not finite_number(controls.get("touches", {}).get(token))
                   or abs(controls["touches"][token]) > .09 for token in mapped_touches):
                return False
            if inputs and inputs <= tokens:
                # The same LEFT level modifier focuses the terminal away
                # from a gun. The sampled native support owner explicitly
                # stows that terminal, so this one inactive focus mapping
                # does not make a stationary support capture a menu action.
                if ((support_borrowed or support_without_terminal)
                        and action.get('name') == 'system.idroid_focus'
                        and action.get('modifier') is True and binding.get('gesture') == 'level'):
                    continue
                if action.get("name") not in allowed or action.get("modifier") is not True:
                    return False
                matched.add(action["name"])
    return "gameplay.ready_weapon" in matched


def require_presentation_transition(state, target):
    """A missing player camera is not a request to turn immersive mode off."""
    if target not in ("immersive", "quad"):
        raise BotFault("system.toggle_vr requires target_presentation=immersive or quad")
    if state.get("camera_available") is not True:
        raise BotFault("VR presentation is unavailable")
    requested = any(state.get(key) is True for key in
                    ("camera_active", "camera_pending", "camera_awaiting_player"))
    if target == "immersive":
        if requested:
            raise BotFault("Immersive mode is already requested; a missing player camera must not trigger a toggle")
        # HeadCameraStop::manual=1; tracking/camera faults are not manual quad.
        if state.get("camera_reason") != 1 or any(state.get(key) is not False for key in
                ("camera_active", "camera_pending", "camera_awaiting_player")):
            raise BotFault("Cannot restore immersive from an unidentified presentation state")
    elif state.get("camera_active") is not True:
        raise BotFault("A deliberate quad transition requires an active immersive camera")


def held_input_evidence(state, inputs, *, after_sample_ms=None):
    """Return an audit only when the requested physical channels were sampled held.

    `native_buttons` is the XR layer's gamepad sample published to the mailbox;
    it does not establish which input source the game ultimately consumed.
    """
    controls = state.get("controls")
    if not isinstance(controls, dict) or not isinstance(inputs, (list, tuple)) or not inputs:
        return None
    try:
        authoritative_controls(state)
    except BotFault:
        return None
    sample_ms = controls["sample_ms"]
    if after_sample_ms is not None and (type(after_sample_ms) is not int or sample_ms <= after_sample_ms):
        return None
    physical = controls.get("physical")
    sticks = controls.get("sticks")
    if (not isinstance(physical, list) or len(physical) < 11
            or not isinstance(sticks, list) or len(sticks) != 4):
        return None
    sampled_values = physical[:11] + sticks
    if any(not finite_number(value) for value in sampled_values):
        return None
    buttons = {"a": 0, "b": 1, "x": 2, "y": 3, "menu": 4,
               "left_stick_click": 5, "right_stick_click": 6,
               "left_grip": 7, "right_grip": 8,
               "left_trigger": 9, "right_trigger": 10}
    axes = {"left_stick_up": (1, 1.), "left_stick_down": (1, -1.),
            "left_stick_left": (0, -1.), "left_stick_right": (0, 1.),
            "right_stick_up": (3, 1.), "right_stick_down": (3, -1.),
            "right_stick_left": (2, -1.), "right_stick_right": (2, 1.)}
    for token in inputs:
        if token in buttons:
            value = physical[buttons[token]]
            if isinstance(value, bool) or not isinstance(value, (float, int)) or value < .5:
                return None
        elif token in axes:
            index, expected = axes[token]
            value = sticks[index]
            if isinstance(value, bool) or not isinstance(value, (float, int)) or value * expected < .5:
                return None
        else:
            return None
    published_buttons = controls.get("native_buttons")
    if type(published_buttons) is not int or not 0 <= published_buttons <= 0xffff:
        return None
    return {
        "sampled_context": controls["context"],
        "sample_ms": sample_ms,
        "age_ms": controls["age_ms"],
        "physical": {"buttons": physical[:11], "sticks": sticks},
        "xr_published_gamepad": {"buttons": published_buttons},
        "requested_inputs": list(inputs),
    }


def finite_number(value):
    if isinstance(value, bool) or not isinstance(value, (float, int)):
        return False
    try:
        return math.isfinite(value)
    except (OverflowError, TypeError):
        return False


def sampled_release_keys(state):
    """Channels needing release in a fresh, complete native XR input sample.

    None means the audit cannot replace the initial full neutralization. Held
    leases are always released separately, even if their last sample was zero.
    """
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return None
    latency = state.get('transport', {}).get('seconds')
    # Native Help/iDroid takes menu ownership while the XR sampler stays live.
    # Its rig_input=False disables gameplay rig controls, not physical input
    # observation. This exception only acknowledges release; it admits no action.
    xr_menu_owner = controls.get('rig_input') is False and controls['context'] == 'menus'
    if ((controls.get('rig_input') is not True and not xr_menu_owner)
            or controls.get('native_packet_source') != 'xr_runtime_final_mapped_packet'
            or not finite_number(latency) or latency < 0
            or controls['age_ms'] + latency * 1000 > 250):
        return None
    tokens = ('a', 'b', 'x', 'y', 'menu', 'left_stick_click', 'right_stick_click',
              'left_grip', 'right_grip', 'left_trigger', 'right_trigger',
              'left_stick_left', 'left_stick_up', 'right_stick_left', 'right_stick_up')
    physical, sticks = controls.get('physical'), controls.get('sticks')
    if (not isinstance(physical, list) or len(physical) != 11
            or not isinstance(sticks, list) or len(sticks) != 4
            or any(not finite_number(v) for v in physical + sticks)):
        return None
    result = set()
    for token, value in zip(tokens, physical + sticks):
        if value != 0:
            item = channel(token)
            result.add((item['hand'], item['component'], item.get('sub_component')))
    return result


def released_field_state(state):
    """A released live field can be handed to the next bounded case."""
    if not isinstance(state, dict):
        return False
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return False
    native = state.get('native', {})
    frame = state.get('rendered', {})
    if (state.get('scene') != 'gameplay' or state.get('camera_active') is not True
            or state.get('camera_available') is not True
            or any(state.get(key) is not False for key in ('menu', 'pause', 'idroid', 'loading', 'title', 'demo'))
            or controls.get('context') != 'gameplay' or controls.get('rig_input') is not True
            or native.get('popup') is not False or native.get('demo') is not False
            or native.get('game_over') != 0 or native.get('saving') is not False
            or not finite_number(native.get('player_life')) or native['player_life'] <= 0
            or not isinstance(native.get('sequence'), str) or not native['sequence'].startswith('Seq_Game_')
            or frame.get('presentation_focused') is not True or not frame.get('player_owner')
            or controls.get('native_buttons') != 0):
        return False
    transport = state.get('transport', {}).get('seconds')
    if (not finite_number(transport) or transport < 0
            or controls['age_ms'] + transport * 1000 > 250):
        return False
    for key, count in (('physical', 11), ('sticks', 4), ('native_axes', 4), ('native_triggers', 2)):
        values = controls.get(key)
        if (not isinstance(values, list) or len(values) != count
                or any(not finite_number(value) or abs(value) > .001 for value in values)):
            return False
    return True


class Live:
    def __init__(self, proxy, game, bindings, events, *, operator=None):
        self.events, self.game, self.bindings = events, pathlib.Path(game), bindings
        self.native = NativeReader()
        self._operator_factory = None
        if operator is None:
            operator_type = load_tool("mgs5vr_operator", "record-simulator.py").Operator
            proxy_path = pathlib.Path(proxy)
            self._operator_factory = lambda: operator_type(proxy_path)
            self.operator = self._operator_factory()
        else:
            self.operator = operator
        self._capture_recovery_used = False
        self.held = {}
        self.transport_error = None
        self.last_tick = None
        self.lua_script = (ROOT / "tools/scenario-state.lua").read_text(encoding="utf-8")
        self.identity = game_identity(game)
        self.startup_evidence = StartupEvidence(self.game / "mgs5vr.log", self.identity["process"]["CreationDate"])
        self.background_check = None
        self.pose_recording = False
        self.opened_menu = None
        from .presentation import PresentationGuard
        self.presentation_guard = PresentationGuard()
        self.process_handle = None
        import ctypes
        from ctypes import wintypes
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.process_handle = self.kernel.OpenProcess(0x100000, False, self.identity["process"]["ProcessId"])
        if not self.process_handle:
            self.operator.close()
            raise BotFault("Cannot retain the game process generation")

    @staticmethod
    def text_result(result):
        return json.loads(next(block["text"] for block in result.get("content", []) if block.get("type") == "text"))

    def ready(self, timeout=60.):
        deadline = time.monotonic() + timeout
        last_error = None
        while True:
            try:
                state = self.text_result(self.call("get_session_info", {}))
                if state.get("state", {}).get("name") == "XR_SESSION_STATE_FOCUSED":
                    break
            except (RuntimeError, TimeoutError, json.JSONDecodeError) as error:
                # An exited owner and an uncertain transport cannot become
                # ready by sleeping. Do not spend sixty seconds rediscovering it.
                if (getattr(self, 'transport_error', None)
                        or (getattr(self, 'process_handle', None)
                            and self.kernel.WaitForSingleObject(self.process_handle, 0) != 258)):
                    raise
                last_error = str(error)
            if time.monotonic() >= deadline:
                raise BotFault("XR readiness deadline: " + str(last_error))
            time.sleep(.25)
        head = self.text_result(self.call("get_head_pose", {"base_space": "local"}))
        flags = head.get("flags", {})
        if not (flags.get("position_valid") and flags.get("orientation_valid")):
            # A newly installed simulator can start without tracked devices.
            # Explicit simulated poses establish tracking; never label an
            # invalid-pose black compositor capture a successful baseline.
            self.call("set_head_pose", {"base_space": "local", "position": [0., 1.6, 0.], "orientation": [0., 0., 0., 1.]})
            for hand, x in (("left", -.2), ("right", .2)):
                for kind in ("grip", "aim"):
                    self.call("set_controller_pose", {"hand": hand, "pose_type": kind, "base_space": "view",
                                                       "position": [x, -.25, -.45], "orientation": [0., 0., 0., 1.]})
        self.events.emit("runtime_ready", state=state, initialized_simulated_tracking=not flags.get("position_valid"))
        # XR focus can precede the retail game's Lua initialization. Only the
        # native fast read is safe here; a full scenario query would queue on
        # Lua and consume the short observation deadline during normal startup.
        while True:
            startup, timing = self.native.read("inspect-bot-state")
            if startup.get("lua_ready", True) is True:
                self.events.emit("native_ready", state=startup, transport=timing)
                break
            if time.monotonic() >= deadline:
                raise BotFault("Native Lua readiness deadline; no gameplay input sent")
            time.sleep(min(.25, max(0., deadline-time.monotonic())))

    def call(self, suffix, args):
        if self.transport_error:
            raise BotFault("Operator transport needs recovery after: " + self.transport_error)
        if self.kernel.WaitForSingleObject(self.process_handle, 0) != 258:
            raise BotFault("Game process exited; refuse input in another session")
        started = time.monotonic()
        self.events.emit("rpc_started", tool=suffix, arguments=args)
        try:
            # Loading can delay an otherwise neutral release acknowledgement.
            # Give release-only RPCs the normal transport budget. Nonzero
            # actions retain their short deadline and independent input expiry;
            # no timed-out request is replayed or followed by queued actions.
            input_action = suffix == "set_controller_input" and args.get("value") != 0
            latest = getattr(self, 'latest_state', {})
            fast_field = latest.get('scene') == 'gameplay' and latest.get('menu') is False
            result = self.operator.call("openxr_" + suffix, args,
                                        timeout=2. if input_action or fast_field else 15.)
        except TimeoutError as error:
            # A timed-out capture can leave the serialized transport blocked.
            # Never queue a chain of releases/actions behind unknown work. Each
            # nonzero channel also has its own operator-side expiry.
            self.transport_error = str(error)
            self.events.emit("operator_transport_failed", tool=suffix, error=self.transport_error)
            factory = getattr(self, "_operator_factory", None)
            if factory is None:
                raise
            result = recover_capture_once(self, suffix, args, error, make_operator=factory)
        # Binary captures are stored separately, never duplicated in JSONL.
        self.events.emit("rpc_completed", tool=suffix, seconds=time.monotonic()-started)
        return result

    def observe(self, native=False, *, fast_only=False):
        if native and fast_only:
            raise BotFault("Fast-only observation cannot request a Lua snapshot")
        if self.background_check:
            self.background_check()
        if self.kernel.WaitForSingleObject(self.process_handle, 0) != 258:
            raise BotFault("Original game process exited")
        state, timing = self.native.read("inspect-bot-state")
        tick = state["now_ms"]
        if self.last_tick is not None and tick <= self.last_tick:
            raise BotFault("Native state is stale or the session clock changed")
        self.last_tick = tick
        state["scene"] = scene(state)
        state["transport"] = timing
        if state.get("title") is True or state.get("title_cabin") is True:
            state.update(self.startup_evidence.read())
        controls = state.get("controls") or {}
        native_presentation = state.get("camera_active") is False and all(
            state.get(key) is False for key in ("loading", "title", "menu", "demo"))
        if (native or native_presentation) and not fast_only:
            if released_startup_observation(state, getattr(self, "held", {})):
                state["native"], state["lua_transport"] = self.native.read_startup_observation(self.lua_script)
                if state["lua_transport"]["seconds"] > self.native.timeout:
                    self.events.emit("startup_observation_wait", native_tick=tick,
                                     transport=state["lua_transport"], request_replayed=False)
            else:
                state["native"], state["lua_transport"] = self.native.read(self.lua_script)
            state["scene"] = scene(state)
        editor=state.get("controls_editor")
        if isinstance(editor,dict) and editor.get("visible"):
            # VR off has no native first-person publication. The editor still
            # has its own current spatial surface; expose that diagnostic to
            # UI inspections without inventing a skin, head or hand sample.
            state["rendered"]=state.get("rendered") or {}
            state["rendered"]["controls_editor"]=editor
        self.events.emit("observation", state=state)
        self.latest_state = state
        observer = getattr(self, 'state_observer', None)
        if observer is not None:
            observer(state)
        return state

    def capture(self, label, *, static_grips=False):
        if self.held and not static_grips:
            raise BotFault("Release input before a blocking compositor capture")
        recording = getattr(self, "held_action_recording", None)
        if self.held and static_grips and recording is not None:
            state = self.observe(native=True)
            if not static_grip_capture_allowed(self.held, state, self.bindings):
                raise BotFault("Recorded held inspection still requires sampled ready/support grips only")
            recording.check()
            # Actual run 163607 blocked the compositor backend for 15 seconds
            # during a held inspection. The continuous native eye take records
            # this action without that RPC. This checkpoint is NOT a still,
            # stereo pass, or final-compositor observation.
            self.events.emit("action_video_checkpoint", label=label, state=state,
                             source="native_source_eye_before_runtime_composition",
                             video=str(recording.segments[-1]["video"]),
                             final_compositor_captured=False, visual_acceptance="pending_recorded_frame_review")
            return []
        deadline = time.monotonic() + 2.
        while True:
            state = self.observe(native=True)
            if self.held and not static_grip_capture_allowed(self.held, state, self.bindings):
                raise BotFault("Only sampled stationary weapon-ready/support grips permit an optic inspection capture")
            try:
                captures = self._capture_once(label, static_grip_state=state if static_grips else None)
            except CapturePairInvalidated as error:
                self.events.emit("capture_pair_restart", label=label,
                                 partial_captures=error.captures,
                                 native_tick=error.state.get("now_ms"))
                # Reobserve both the native state and both eyes. Keep the
                # existing presentation deadline and guard unchanged.
                continue
            except BlankCompositorFrame as error:
                error.state = state
                raise
            if self.presentation_guard.admit(captures, state):
                return captures
            if time.monotonic() >= deadline:
                self.events.emit("presentation_failed", label=label, captures=captures,
                                 reason="A compositor eye froze during live gameplay or across a native scene/menu/hand change")
                raise BotFault("Stale compositor view: an eye stopped updating; stop the test loop")
            time.sleep(.1)

    def _capture_once(self, label, *, static_grip_state=None):
        if self.held and (static_grip_state is None or not static_grip_capture_allowed(self.held, static_grip_state, self.bindings)):
            raise BotFault("Release input before a blocking compositor capture")
        if not re.fullmatch(r"[a-zA-Z0-9_-]{1,100}", label):
            raise BotFault("Invalid evidence label")
        if self.pose_recording:
            self.pose_snapshot(label)
        captures = []
        for eye in ("left", "right"):
            if self.held and static_grip_state is not None:
                sampled = self.observe(native=True)
                if not static_grip_capture_allowed(self.held, sampled, self.bindings):
                    raise BotFault("Static grip expired or gameplay input changed during optic capture")
            result = self.call("capture_composited_image", {"eye": eye})
            recovered = result if isinstance(result, CaptureRecoveryResult) else None
            if recovered is not None:
                result = recovered.mcp_result
            block = next((b for b in result.get("content", []) if b.get("type") == "image"), None)
            if not block:
                raise BotFault("Compositor returned no image")
            pixels = base64.b64decode(block["data"], validate=True)
            stamp = time.time_ns()
            while True:
                path = self.events.output / f"{label}-{stamp}-{eye}.png"
                try:
                    output = path.open("xb")
                except FileExistsError:
                    # Wall-clock resolution/recovery can repeat a timestamp.
                    # Keep every prior capture immutable, including rejected pairs.
                    stamp += 1
                    continue
                with output:
                    output.write(pixels)
                break
            captures.append(str(path))
            from PIL import Image, ImageStat
            with Image.open(io.BytesIO(pixels)) as frame:
                gray = frame.convert("L")
                maximum = gray.getextrema()[1]
                deviation = ImageStat.Stat(gray).stddev[0]
                metrics = {"size": list(frame.size), "maximum": maximum, "luma_stddev": deviation}
            self.events.emit("capture", eye=eye, path=str(path), sha256=digest(path), source="final_compositor", metrics=metrics,
                             recovered_partial=recovered is not None)
            if maximum <= 3 or deviation < .5:
                raise BlankCompositorFrame(f"Blank/uniform {eye} compositor frame retained at {path}; readiness not proven")
            if recovered is not None:
                self.events.emit("capture_pair_invalidated", label=label, eye=eye,
                                 partial_captures=captures, reason="operator_capture_recovery")
                raise CapturePairInvalidated(captures, recovered.state)
        return captures

    def pose_snapshot(self, label):
        """Save readback for teaching diagrams, with each query timed separately.

        These are operator pose observations, not one atomic rendered-frame
        snapshot. Gameplay zooms must still come from the matching real footage.
        """
        values = []
        queries = [("get_head_pose", {"base_space": "local"})]
        queries += [("get_controller_pose", {"hand": hand, "pose_type": kind, "base_space": "view"})
                    for hand in ("left", "right") for kind in ("grip", "aim")]
        for tool, arguments in queries:
            started = self.events.emit("pose_query_started", label=label, tool=tool, arguments=arguments)
            result = self.text_result(self.call(tool, arguments))
            event = self.events.emit("tracked_pose", label=label, tool=tool, arguments=arguments,
                                     result=result, query_started_qpc_100ns=started.get("qpc_100ns"),
                                     source="operator_readback")
            values.append(event)
        return values

    def input(self, values, duration, *, lease_seconds=None):
        if not .03 <= duration <= 5:
            raise BotFault("Input must have a bounded 0.03..5 second hold")
        lease = duration if lease_seconds is None else lease_seconds
        if not .03 <= lease <= 5:
            raise BotFault("Input expiry must be within 0.03..5 seconds")
        # The operator accepts one channel per RPC. Canonical binding order
        # puts face buttons before their modifiers, leaking an interaction
        # edge while a chord is being formed. Establish Menu, then grip
        # modifiers, before face/stick controls. This is not an atomic API.
        priority = {"Menu": 0, "Grip": 1}
        ordered = sorted(values, key=lambda item: priority.get(item["component"], 2))
        deadline = time.monotonic() + lease
        for value in ordered:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise BotFault("Input expiry elapsed while forming chord")
            key = (value["hand"], value["component"], value.get("sub_component"))
            self.held[key] = value
            # The backend timer is independent of a blocked capture or a dead
            # runner. Chord children expire before their modifiers. Ordinary
            # completion still explicitly releases all channels first.
            modifier_tail = .15 if len(ordered) > 1 and value["component"] in priority else 0.
            self.call("set_controller_input", {**value, "auto_release": True,
                                                "hold_duration": min(5., remaining + modifier_tail)})

    def release(self, *, keep=()):
        # The first release clears every channel retained by an earlier runner.
        # Subsequently a fresh native sample can avoid redundant idle RPCs;
        # owned leases and every observed nonzero channel must still be cleared.
        # A missing/stale audit falls back to full neutralization.
        errors = []
        values = [channel(t) for t in ("a", "b", "x", "y", "left_grip", "right_grip",
                   "left_trigger", "right_trigger", "left_stick_click", "right_stick_click",
                   "left_stick_up", "left_stick_left", "right_stick_up", "right_stick_left", "menu")]
        # Release held face/axis channels before clearing idle channels.
        # Otherwise a moving stick keeps driving Snake while twelve unrelated
        # face/trigger RPCs finish. Still clear every chord child
        # before grip/Menu modifiers to avoid exposing an unmodified action.
        priority = {"Menu": 2, "Grip": 1}
        held = set(self.held)
        required = None
        sampled_tick = None
        released_count = 0
        if getattr(self, '_released_once', False) and not getattr(self, 'transport_error', None):
            try:
                sampled = self.observe()
                active = sampled_release_keys(sampled)
                if active is not None:
                    required = held | active
                    sampled_tick = sampled['controls']['sample_ms']
            except Exception as error:
                self.events.emit('release_audit_unavailable', error=str(error))
        def release_order(item):
            key = (item["hand"], item["component"], item.get("sub_component"))
            return (priority.get(item["component"], 0), key not in held)
        for value in sorted(values, key=release_order):
            key = (value["hand"], value["component"], value.get("sub_component"))
            if key in keep or (required is not None and key not in required):
                continue
            try:
                self._release_channel(value)
                self.held.pop((value["hand"], value["component"], value.get("sub_component")), None)
                released_count += 1
            except Exception as error:
                errors.append(str(error))
        if not errors and required is not None and released_count:
            # Removing idle RPCs also removes their accidental settling delay.
            # Wait for an XR sample made AFTER the releases before another
            # action can resolve its context (e.g. equipment -> gameplay).
            try:
                deadline = time.monotonic() + 1.
                after_release = self.observe()['now_ms']
                while True:
                    sampled = self.observe()
                    active = sampled_release_keys(sampled)
                    if (active is not None and sampled['controls']['sample_ms'] > after_release
                            and not active.difference(keep)):
                        self.events.emit('release_sample_verified', state=sampled)
                        break
                    if time.monotonic() >= deadline:
                        raise BotFault('Released channels did not reach a fresh neutral XR sample')
                    time.sleep(.01)
            except Exception as error:
                errors.append(str(error))
        self.events.emit("neutralized", errors=errors, sampled_tick=sampled_tick,
                         release_scope='full' if required is None else 'held_and_sampled_active')
        if errors:
            self._released_once = False
            raise BotFault("Input release failed: " + errors[0])
        self._released_once = True

    def _release_channel(self, value):
        # v207.1 can explicitly reject a neutral write while the previous
        # independently expiring gesture finishes. That is a known rejection,
        # not an ambiguous completion. Retry only this idempotent zero; never
        # replay a nonzero action or a timed-out/otherwise failed request.
        arguments = {**value, "value": 0, "auto_release": False}
        deadline = time.monotonic() + .5
        attempts = 0
        while True:
            try:
                self.call("set_controller_input", arguments)
                return
            except RuntimeError as error:
                if (str(error) != "Error: Another input gesture is already in progress for this session."
                        or time.monotonic() >= deadline):
                    raise
                attempts += 1
                self.events.emit('release_gesture_busy', channel=arguments, attempt=attempts,
                                 action_replayed=False)
                time.sleep(min(.025, max(0., deadline-time.monotonic())))

    def equipment_select(self, step):
        """Use the real wrist picker, with one independently expiring modifier."""
        category = step.get("category")
        card = step.get("card")
        if category not in ("primary", "secondary", "support", "items") or card not in ("up", "down", "left", "right"):
            raise BotFault("Equipment selection needs a named category and cardinal card")
        actions = {a["name"]: a for a in self.bindings["actions"]}
        modifier, action = actions.get("equipment.open"), actions.get("equipment." + category)
        if not modifier or not modifier.get("modifier") or not modifier.get("bindings") or not action or not action.get("bindings"):
            raise BotFault("The effective wrist picker bindings are unavailable")
        opening, selecting = modifier["bindings"][0], action["bindings"][0]
        if opening["gesture"] != "level" or selecting["gesture"] != "level":
            raise BotFault("Wrist selection requires the established held modifier and category bindings")
        axis = next((a for a in self.bindings["axes"] if a["name"] == "axes.equipment"), None)
        if not axis or axis["source"] != "right_stick":
            raise BotFault("This bounded picker inspection requires the right-stick equipment axis")
        # Resolve every input before dispatch. Unsupported capacitive simulation
        # or an invalid binding cannot partially open the picker.
        held = [channel(t) for t in opening["inputs"]]
        choose = [channel(t) for t in selecting["inputs"]]
        browse = channel(axis["source"] + "_" + card)
        state, controls = fresh_control_observation(self.observe)
        if controls["context"] != "gameplay" or state.get("menu") is not False or state.get("camera_active") is not True:
            raise BotFault("Wrist inspection requires fresh ordinary gameplay")
        self.release()
        deadline = time.monotonic() + 5.
        keep = {(v["hand"], v["component"], v.get("sub_component")) for v in held}
        def sampled(tokens, seconds, context):
            first = None
            while time.monotonic() < deadline:
                current = self.observe()
                sample = held_input_evidence(current, tokens)
                if sample and sample["sampled_context"] == context:
                    first = sample["sample_ms"] if first is None else first
                    if sample["sample_ms"] - first >= math.ceil(seconds * 1000):
                        return current
                else:
                    first = None
                time.sleep(.025)
            raise BotFault("Wrist picker did not establish its sampled input/context before expiry")
        try:
            self.events.emit("semantic_equipment_selection", category=category, card=card,
                             opening=opening, selecting=selecting, axis=axis)
            self.input(held, .2, lease_seconds=5.)
            sampled(opening["inputs"], .2, "equipment")
            self.input(choose, .3, lease_seconds=max(.03, deadline-time.monotonic()))
            sampled(opening["inputs"] + selecting["inputs"], .3, "equipment")
            self.release(keep=keep)
            sampled(opening["inputs"], .3, "equipment")
            self.input([browse], .3, lease_seconds=max(.03, deadline-time.monotonic()))
            sampled(opening["inputs"] + [axis["source"] + "_" + card], .3, "equipment")
        finally:
            self.release()
        return self.observe()

    def execute(self, step):
        if step.get('op') == 'commands_select':
            from .commands import select
            return select(self, step)
        if step.get('op') == 'menu_focus_sequence':
            from .menus import focus_sequence
            return focus_sequence(self, step)
        name = step.get('name', '')
        menu_action = step.get('op') == 'action' and (name.startswith('menus.') or name.startswith('idroid.'))
        menu_navigation = step.get('op') == 'menu_navigate' and step.get('owner', 'idroid') == 'idroid'
        if not menu_action and not menu_navigation:
            return self._execute(step)
        state, controls = fresh_control_observation(self.observe, native=True)
        route = (state.get('idroid') is True and state.get('idroid_handheld') is True
                 and state.get('pause') is False
                 and state.get('idroid_menu_routing_available') is True)
        if not route:
            return self._execute(step)
        if step.get('reviewed_action_only') is True:
            # This contract admits one physical face channel. A new modifier
            # requires a separately authored/reviewed case, not an implicit edge.
            raise BotFault('Reviewed handheld menu case requires explicit grip-focus ownership')
        focus = next((a for a in self.bindings['actions'] if a['name'] == 'system.idroid_focus'), None)
        if (not focus or not focus.get('modifier') or len(focus.get('bindings', [])) != 1
                or focus['bindings'][0]['gesture'] != 'level'):
            raise BotFault('Handheld menu action needs one effective held focus modifier')
        native = state.get('native', {})
        if (state.get('gamepad') is not False or native.get('popup') is not False
                or native.get('saving') is not False or state.get('loading') is not False):
            raise BotFault('Handheld menu focus lost its native input owner')
        if float(step.get('seconds', .12)) > 3.:
            raise BotFault('Handheld menu action exceeds its independently bounded focus lease')
        recipe = ({**step, 'name': 'idroid.previous_tab' if name == 'menus.previous_tab' else name}
                  if menu_action else dict(step))
        try:
            focus_channels = [channel(t) for t in focus['bindings'][0]['inputs']]
            self.input(focus_channels, .2, lease_seconds=5.)
            deadline = time.monotonic() + .8
            while True:
                focused, current = fresh_control_observation(self.observe, native=True)
                if (focused.get('idroid') is not True or focused.get('gamepad') is not False
                        or focused['native'].get('popup') is not False):
                    raise BotFault('Handheld focus owner changed before menu action')
                # A forced guide is already in menus before the simulated
                # grip arrives. Its preceding native acknowledgement must
                # not stand in for the newly requested physical focus.
                if (current['context'] == 'menus' and focused.get('idroid_menu_input_ready') is True
                        and focused.get('idroid_menu_control') == 2
                        and held_input_evidence(focused, focus['bindings'][0]['inputs'],
                                                after_sample_ms=controls['sample_ms'])):
                    self.events.emit('handheld_menu_focus_acquired', state=focused)
                    break
                if time.monotonic() >= deadline:
                    raise BotFault('Native handheld grip focus was not acknowledged; menu action not sent')
                time.sleep(.025)
            self._menu_focus_keep = tuple((c['hand'], c['component'], c.get('sub_component'))
                                          for c in focus_channels)
            return self._execute(recipe)
        finally:
            self._menu_focus_keep = ()
            self.release()

    def _execute(self, step):
        if step.get('op') == 'await_state':
            from .core import Behaviors
            seconds,predicate=step.get('seconds',8.),step.get('state')
            if (self.held or type(seconds) not in (int,float) or not math.isfinite(seconds)
                    or not 0<seconds<=60 or not isinstance(predicate,dict) or not predicate):
                raise BotFault('Phase wait requires released input, explicit state and a bounded deadline')
            return Behaviors(self,self.events).wait_for(predicate,seconds,'explicit-phase-wait',stable=2)
        if step.get('op') == 'advance_startup':
            from .startup import advance_startup
            seconds=step.get('seconds',60.)
            if type(seconds) not in (int,float) or not math.isfinite(seconds) or not 0<seconds<=90:
                raise BotFault('Startup observation requires a bounded 0..90 second window')
            return advance_startup(self,timeout=seconds)
        if step.get('op') == 'observe_interval':
            from .interval import observe_interval
            return observe_interval(self, step)
        if step.get('op') == 'hospital_follow':
            from .hospital import follow_rail
            return follow_rail(self,step.get('seconds',30.))
        if step.get('op') == 'hospital_follow_ishmael':
            from .hospital import follow_rail
            return follow_rail(self,step.get('seconds',30.),ishmael=True,waypoints=step.get('waypoints'),
                               sequence=step.get('sequence','Seq_Game_EnterSmokeRoom'),
                               stop_at_last_waypoint=step.get('stop_at_last_waypoint',False))
        if step.get('op') == 'hospital_navigate_event':
            from .hospital import follow_rail
            return follow_rail(self,step.get('seconds',45.),ishmael=True,
                               sequence=step['sequence'],event_manifest=step['manifest'],event=step['event'])
        if step.get('op') == 'hospital_stick_probe':
            from .hospital import probe_stick
            return probe_stick(self,step['axis'],step['value'],step.get('seconds',.6))
        if step.get('op') == 'hospital_shoot_extinguisher':
            from .hospital import shoot_extinguisher
            return shoot_extinguisher(self)
        if step.get('op') == 'hospital_horse_chase':
            from .hospital import horse_chase
            return horse_chase(self,step.get('seconds',90.))
        if step.get('op') == 'hospital_corridor_cue':
            from .hospital import corridor_cue
            return corridor_cue(self,step['sequence'],step.get('seconds',.6))
        if step.get('op') == 'hospital_crawl':
            from .locomotion import crawl_hospital
            return crawl_hospital(self,step.get('seconds',2.),step.get('direction','forward'),
                                  witnessed_smoke_corridor=step.get('witnessed_smoke_corridor',False),
                                  witnessed_under_bed=step.get('witnessed_under_bed',False))
        if step.get('op') == 'action' and 'wheel_sweep_degrees' in step:
            from .vehicle_wheel import sweep
            return sweep(self, step)
        if step["op"] == "walk_local":
            from .locomotion import move_local
            return move_local(self, target_distance=step.get("distance", .35),
                              max_displacement=1., max_seconds=5.,
                              brake_magnitude=.35, pulse_seconds=.1,
                              direction=step.get("direction", "up"))
        if step["op"] == "motion":
            from .motion import move
            return move(self, step)
        if step["op"] in ("tracking_loss", "head_tracking_loss"):
            head_loss = step["op"] == "head_tracking_loss"
            mask, milliseconds = step.get("mask"), step.get("milliseconds")
            if (type(mask) is not int or not 0 <= mask <= (3 if head_loss else 15)
                    or type(milliseconds) is not int or not 100 <= milliseconds <= 8000 or self.held):
                raise BotFault("Tracking-loss fixture requires released input and a bounded valid mask")
            state = self.observe()
            playable = state.get("scene") in ("gameplay", "cabin") or (
                state.get("idroid") and not any(state.get(k) for k in ("pause", "title", "loading", "demo")))
            if (not playable or not state.get("camera_active")
                    or state.get("camera_suspended") or not state.get("rendered", {}).get("rig_sequence")):
                raise BotFault("Tracking-loss fixture requires a current accepted playable rig")
            target = "head" if head_loss else "controller"
            response, timing = self.native.read(f"diagnostics:{target}-tracking-loss:{mask}:{milliseconds}")
            if not response.get("armed") or not response.get("enabled") or response.get("mask") != mask:
                raise BotFault("Explicit local tracking-loss diagnostic was not armed")
            if head_loss and mask:
                self.presentation_guard.expect_head_tracking_loss(mask, milliseconds)
            self.events.emit("synthetic_tracking_loss", target=target, mask=mask, milliseconds=milliseconds,
                             response=response, transport=timing,
                             evidence_limit="Synthetic raw tracking loss in the actual game; not physical Quest 3 inactivity")
            return
        if step["op"] == "menu_observe":
            from .menus import observe
            return observe(self, step)
        if step["op"] == "menu_navigate":
            from .menus import navigate
            return navigate(self, step)
        if step["op"] == "equipment_select":
            return self.equipment_select(step)
        if step["op"] == "pose":
            hand, position, orientation = step["hand"], step["position"], step["orientation"]
            if hand not in ("left", "right"):
                raise BotFault("Invalid hand pose")
            poses = (("grip", position, orientation),
                     ("aim", step.get("aim_position", position), step.get("aim_orientation", orientation)))
            # Validate BOTH poses before changing either one. Real controllers
            # have different grip and pointing bases; equating them concealed
            # the reported ergonomic iDroid failure in previous simulator runs.
            for kind, point, attitude in poses:
                if not isinstance(point, list) or not isinstance(attitude, list) or len(point) != 3 or len(attitude) != 4:
                    raise BotFault("Invalid " + kind + " pose")
                if not all(type(v) in (int, float) and math.isfinite(v) for v in point + attitude) or max(abs(v) for v in point) > 2:
                    raise BotFault("Pose exceeds the local demonstration envelope")
                length = math.sqrt(sum(v*v for v in attitude))
                if not .99 < length < 1.01:
                    raise BotFault("Pose quaternion is not normalized")
            for kind, point, attitude in poses:
                self.call("set_controller_pose", {"hand": hand, "pose_type": kind, "base_space": "view",
                                                    "position": point, "orientation": attitude})
            self.events.emit("controller_pose_set", hand=hand, base_space="view",
                             position=position, orientation=orientation,
                             aim_position=poses[1][1], aim_orientation=poses[1][2])
            if self.pose_recording:
                self.pose_snapshot("pose-after")
            return
        if step["op"] != "action":
            raise BotFault("Only effective-binding semantic actions are accepted in cases")
        name = step["name"]
        action = next((a for a in self.bindings["actions"] if a["name"] == name), None)
        if not action or not action["bindings"]:
            raise BotFault("Action unavailable/disabled: " + name)
        # Captures and loading transitions can briefly interrupt the XR
        # publication. Reacquire it before input instead of terminating the
        # whole queue on one stale sample. All eligibility guards below use
        # this fresh state; the 250 ms freshness limit is unchanged.
        state, control_sample = fresh_control_observation(self.observe, native=bool(step.get("native_before")))
        if step.get("state_before") and not matches(state, step["state_before"]):
            raise ActionPrerequisiteChanged("Observed action prerequisite changed")
        if name == "system.toggle_vr":
            require_presentation_transition(state, step.get("target_presentation"))
        # Scene labels do not distinguish equipment, Commands, optics, vehicles,
        # and native button routing. Use the fresh context published by the
        # compiled resolver, and fail closed when it is missing.
        context = control_sample["context"]
        admission_sample_ms = state.get("now_ms", control_sample["sample_ms"])
        if step.get("native_before"):
            if not matches(state["native"], step["native_before"]):
                raise ActionPrerequisiteChanged("Native action prerequisite changed")
        if context not in action["contexts"]:
            raise BotFault(f"{name} is not available in observed {context}")
        binding = action["bindings"][0]
        ready_binding = None
        vehicle_fire = name == 'vehicle.fire'
        horse_fire = step.get('scripted_horse_burst') is True
        if horse_fire and (name != 'gameplay.fire_or_cqc' or step.get('include_ready') is not True
                           or not scripted_horse_fire_owner(state, step.get('native_before'))):
            raise BotFault('Long scripted fire requires the exact native horse chase and physical ready binding')
        vehicle_owner = None
        wheel_pair = step.get('include_left_wheel') is True
        wheel_owner = wheel_root = None
        if vehicle_fire and step.get('include_ready') is not True:
            raise BotFault('Vehicle fire requires include_ready; a lone trigger owns the accelerator')
        if step.get("include_ready") is True:
            scope_zoom = name == 'gameplay.zoom'
            ready_name = 'vehicle.ready_weapon' if vehicle_fire else 'gameplay.ready_weapon'
            expected_context = 'vehicle' if vehicle_fire else 'gameplay'
            ready = next((a for a in self.bindings["actions"] if a["name"] == ready_name), None)
            if (name not in ("gameplay.fire_or_cqc", "gameplay.zoom", "vehicle.fire") or context != expected_context
                    or not ready or not ready.get("modifier") or context not in ready["contexts"]
                    or not ready.get("bindings") or step.get("include_support")
                    or state.get("scene") != "gameplay" or state.get("camera_active") is not True
                    or state.get("menu") is not False or state.get("idroid") is not False
                    or state.get("gamepad") is not False or control_sample.get("rig_input") is not True):
                raise BotFault("Ready/fire inspection requires the current matching VR weapon owner")
            if vehicle_fire:
                vehicle_owner = vehicle_weapon_test_owner(state, step.get('native_before'))
                if vehicle_owner is None:
                    raise BotFault('Vehicle fire requires the exact stationary native driver and released pedals')
            ready_binding = ready["bindings"][0]
            if (binding["gesture"] != ('press' if scope_zoom else 'level') or ready_binding["gesture"] != "level"
                    or not ready_binding["inputs"]
                    or not set(ready_binding["inputs"]).issubset({"left_grip", "right_grip"})
                    or set(binding["inputs"]) & set(ready_binding["inputs"])
                    or binding["inputs"] != (['right_stick_up'] if scope_zoom else ['right_trigger'])):
                raise BotFault("Ready/action inspection requires the effective grip-ready and physical action bindings")
            binding = {**binding, "inputs": list(dict.fromkeys(ready_binding["inputs"] + binding["inputs"]))}
        if step.get("include_support") is True:
            support = next((a for a in self.bindings["actions"] if a["name"] == "gameplay.support_grip"), None)
            if name != "gameplay.ready_weapon" or not support or not support.get("modifier") or context not in support["contexts"] or not support["bindings"]:
                raise BotFault("Support inspection requires both established weapon-ready and support modifiers")
            extra = support["bindings"][0]
            if binding["gesture"] != "level" or extra["gesture"] != "level":
                raise BotFault("Support inspection requires level grip modifiers")
            inputs = list(dict.fromkeys(binding["inputs"] + extra["inputs"]))
            if set(inputs) != {"left_grip", "right_grip"}:
                raise BotFault("Static support inspection requires grip-only effective bindings")
            binding = {**binding, "inputs": inputs}
        if wheel_pair:
            left = next((a for a in self.bindings['actions'] if a['name'] == 'vehicle.wheel_grip'), None)
            if (name != 'vehicle.right_wheel_grip' or context != 'vehicle'
                    or step.get('include_ready') or step.get('include_support') or step.get('static_grip_capture')
                    or self.held or not action.get('modifier') or len(action['bindings']) != 1
                    or not left or not left.get('modifier') or context not in left['contexts']
                    or len(left.get('bindings', [])) != 1):
                raise BotFault('Two-hand wheel inspection requires the exact effective wheel modifiers and released input')
            extra = left['bindings'][0]
            if (binding['gesture'] != 'level' or binding['inputs'] != ['right_grip']
                    or extra['gesture'] != 'level' or extra['inputs'] != ['left_grip']):
                raise BotFault('Two-hand wheel inspection requires LG/RG-only effective bindings')
            wheel_owner = vehicle_wheel_test_owner(state, step.get('native_before'))
            if wheel_owner is None:
                raise BotFault('Two-hand wheel inspection requires a tracked native driver with all controls neutral')
            wheel_root = tuple(state['native']['player_' + axis] for axis in 'xyz')
            # Form the left wheel contact before RG, whose off-wheel meaning
            # is weapon-ready. The game still decides actual contact ownership.
            binding = {**binding, 'inputs': extra['inputs'] + binding['inputs']}
        threshold = binding["milliseconds"] / 1000
        default_duration = threshold + .1 if binding["gesture"] == "hold" else .12
        if name == "system.idroid" and binding["gesture"] == "tap":
            # A real 120 ms Start pulse was sampled by XR/native XInput but
            # missed ACC entry on a warm run. Keep a bounded, longer tap below
            # the effective personal hold threshold; never retry implicitly.
            default_duration = min(.30, threshold * .6)
        duration = float(step.get("seconds", default_duration))
        if horse_fire and not .03 <= duration <= 2.:
            raise BotFault('Scripted horse ready/fire requires a bounded 0.03..2 second hold')
        if ready_binding and not horse_fire and not .03 <= duration <= .35:
            raise BotFault("Ready/fire inspection requires a bounded 0.03..0.35 second burst")
        if wheel_pair and not .03 <= duration <= 2.5:
            raise BotFault('Two-hand wheel inspection requires a bounded 0.03..2.5 second hold')
        reviewed_only = step.get("reviewed_action_only") is True
        if reviewed_only:
            reviewed_step = {key:value for key,value in step.items() if key != "reviewed_action_only"}
            guard = {**step.get("state_before", {}),
                     **{"native."+key:value for key,value in step.get("native_before", {}).items()}}
            reviewed_action_contract({"reviewed_action_only":True, "before":guard, "after":guard,
                                      "steps":[reviewed_step]})
            if (getattr(self, "supervised", False) is not True or binding["gesture"] != "level"
                    or len(binding["inputs"]) != 1 or binding["inputs"][0] not in ("a", "b", "x", "y")):
                raise BotFault("Reviewed action requires one effective level face button in supervised mode")
            if not reviewed_action_packet(state, name):
                raise ActionPrerequisiteChanged("Reviewed action requires a sampled fully neutral native packet")
        if binding["gesture"] == "tap" and duration >= threshold:
            raise BotFault("Tap duration crosses the configured hold boundary")
        if binding["gesture"] == "hold" and duration <= threshold:
            raise BotFault("Hold duration does not reach configured threshold")
        self.events.emit("semantic_action", action=name, label=action["label"], binding=binding,
                         context=context, scene=state["scene"], duration=duration)
        evidence = None
        sampled_hold = binding["gesture"] == "hold" or (binding["gesture"] == "level" and duration >= .5)
        hold_since = None
        during = step.get("while_held")
        if during is not None and (not isinstance(during, dict) or not during):
            raise BotFault("A while-held outcome needs a nonempty predicate")
        if during and binding["gesture"] == "tap":
            raise BotFault("A tap cannot be extended to wait for a held outcome")
        during_count = 0
        observed = None
        native_during = bool(during and any(key.startswith("native.") for key in during))
        try:
            if wheel_pair:
                # Neutral pedals alone do not prove that the vehicle stopped.
                # Observe an advancing native sample before dispatch; any root
                # drift over 5 cm or owner/input change aborts without gripping.
                stationary_deadline = time.monotonic() + 1.
                stationary_since = control_sample['sample_ms']
                while True:
                    stationary = self.observe(native=True)
                    if vehicle_wheel_test_owner(stationary, step['native_before'], root=wheel_root) != wheel_owner:
                        raise BotFault('Two-hand wheel driver moved or lost neutral ownership before dispatch')
                    stamp = stationary['controls']['sample_ms']
                    if stamp - stationary_since >= 100:
                        admission_sample_ms = stationary['now_ms']
                        self.events.emit('vehicle_wheel_stationary_before_grip', state=stationary,
                                         sampled_milliseconds=stamp-stationary_since)
                        break
                    if time.monotonic() >= stationary_deadline:
                        raise BotFault('Two-hand wheel inspection did not observe a stationary native driver')
                    time.sleep(.025)
            if ready_binding:
                # Establish the physical ready modifier before the trigger.
                # Simultaneous RPC dispatch can otherwise produce CQC while
                # the native weapon is still being raised. Both leases remain
                # independently bounded and the finally block clears both.
                self.input([channel(t) for t in ready_binding["inputs"]], .25,
                           lease_seconds=2.5 if vehicle_fire else 2.)
                ready_deadline = time.monotonic() + (1.8 if vehicle_fire else 1.)
                ready_since = None
                while True:
                    raised = self.observe(native=bool(step.get("native_before")))
                    audit = held_input_evidence(raised, ready_binding["inputs"], after_sample_ms=admission_sample_ms)
                    owner = (raised.get("scene") == "gameplay" and raised.get("menu") is False
                             and raised.get("idroid") is False and raised.get("gamepad") is False
                             and raised.get("camera_active") is True
                             and raised.get("controls", {}).get("context") == expected_context
                             and raised.get("controls", {}).get("rig_input") is True)
                    if vehicle_fire:
                        owner = owner and vehicle_weapon_test_owner(raised, step['native_before']) == vehicle_owner
                    if not owner or (step.get("native_before") and not matches(raised.get("native", {}), step["native_before"])):
                        raise BotFault("Weapon owner changed before trigger dispatch")
                    scope_ready = (not scope_zoom or raised.get('rendered', {}).get('scope_tracked') is True)
                    native_ready = (vehicle_weapon_test_drawn(raised, vehicle_owner) if vehicle_fire else
                        raised.get("rendered", {}).get("native_firearm") is True
                        and raised.get("controls", {}).get("native_triggers") == [255, 0])
                    if audit and scope_ready and native_ready:
                        ready_since = audit["sample_ms"] if ready_since is None else ready_since
                        if audit["sample_ms"] - ready_since >= 200:
                            self.events.emit('vehicle_weapon_ready_before_fire' if vehicle_fire else
                                             'weapon_ready_before_zoom' if scope_zoom else "weapon_ready_before_fire", state=raised,
                                             sampled_milliseconds=audit["sample_ms"] - ready_since)
                            admission_sample_ms = raised.get("now_ms", audit["sample_ms"])
                            break
                    else:
                        ready_since = None
                    if time.monotonic() >= ready_deadline:
                        raise BotFault("Native weapon readiness was not sampled before trigger dispatch")
                    time.sleep(.025)
            if (name in ("system.pause", "system.idroid") and state.get("menu") is False
                    and state.get("scene") != "title"):
                # Record ownership before dispatch so an interrupted opening
                # still has cleanup. Never close a user's pre-existing menu.
                self.opened_menu = "idroid" if name == "system.idroid" else "pause"
            lease_seconds = min(5., duration + 2.) if sampled_hold or during else duration
            if ready_binding:
                lease_seconds = min(2.85, duration + .8) if horse_fire else min(.6, duration + .2)
            if wheel_pair:
                # input() adds a 150 ms modifier tail; actual backend timers
                # therefore remain <= 3 s even if this runner stops responding.
                lease_seconds = min(2.85, duration + 1.)
            edge_started = time.monotonic()
            self.input([channel(t) for t in binding["inputs"]], duration, lease_seconds=lease_seconds)
            self.events.emit("action_channels_held", action=name, duration=duration)
            # Slow xrEndFrame can delay action sampling by several hundred ms.
            # A host-side .7s sleep may never produce a native 550ms hold. Long
            # gestures use the resolver's sampled clock, with a hard wall limit.
            dispatched = edge_started if reviewed_only else time.monotonic()
            deadline = dispatched + (min(5., duration + 2.) if sampled_hold or during else duration)
            if ready_binding or wheel_pair:
                deadline = dispatched + lease_seconds
            while True:
                held_state = self.observe(native=native_during or vehicle_fire or wheel_pair or horse_fire)
                if horse_fire and not scripted_horse_fire_owner(held_state, step['native_before']):
                    raise BotFault('Scripted chase owner changed during the bounded trigger hold')
                if wheel_pair and vehicle_wheel_test_owner(held_state, step['native_before'],
                        root=wheel_root, grips=True) != wheel_owner:
                    raise BotFault('Two-hand wheel owner, stationary root, or neutral pedals changed during grip lease')
                if vehicle_fire and (vehicle_weapon_test_owner(held_state, step['native_before']) != vehicle_owner
                                     or not vehicle_weapon_test_ready(held_state, vehicle_owner)):
                    raise BotFault('Vehicle weapon owner or native readiness changed during trigger lease')
                current = held_input_evidence(held_state, binding["inputs"],
                                              after_sample_ms=admission_sample_ms)
                if reviewed_only:
                    held_guard = {**step["state_before"], "controls.native_buttons":REVIEWED_MENU_BUTTONS[name]}
                    if (time.monotonic() > deadline or not matches(held_state, held_guard)
                            or not reviewed_action_packet(held_state, name, binding["inputs"][0])):
                        current = None
                if evidence is None and current is not None:
                    evidence = current
                    self.events.emit("action_input_audit", action=name, admitted_context=context,
                                     admission_sample_ms=admission_sample_ms, **evidence)
                duration_complete = time.monotonic() - dispatched >= duration
                if sampled_hold:
                    duration_complete = False
                    if current is None:
                        # An absent/stale publication is not a sampled release.
                        # Keep the first held timestamp, but credit no elapsed
                        # time until another fresh held sample arrives. A fresh
                        # contradictory physical sample does break the hold.
                        try:
                            sample = authoritative_controls(held_state)
                            if sample["sample_ms"] > admission_sample_ms:
                                hold_since = None
                        except BotFault:
                            pass
                    else:
                        if hold_since is None:
                            hold_since = current["sample_ms"]
                        elapsed = current["sample_ms"] - hold_since
                        if elapsed >= math.ceil(duration * 1000):
                            duration_complete = True
                if during:
                    during_count = during_count + 1 if current and matches(held_state, during) else 0
                if duration_complete and (not during or during_count >= 2):
                    if sampled_hold:
                        self.events.emit("action_hold_completed", action=name,
                                         first_sample_ms=hold_since, last_sample_ms=current["sample_ms"],
                                         sampled_milliseconds=elapsed)
                    if during:
                        self.events.emit("action_outcome_while_held", action=name,
                                         predicate=during, state=held_state)
                        # Explicit stationary grip inspections permit bounded
                        # final-eye captures. Movement, buttons, triggers and
                        # arbitrary remaps still use a state-only checkpoint.
                        captures = []
                        if step.get("static_grip_capture") is True:
                            if not static_grip_capture_allowed(self.held, held_state, self.bindings):
                                raise BotFault("This action is not a stationary weapon/support grip inspection")
                            captures = self.capture(step["capture_while_held"], static_grips=True)
                            held_state = self.observe(native=native_during)
                            if not static_grip_capture_allowed(self.held, held_state, self.bindings) or not matches(held_state, during):
                                raise BotFault("Grip inspection expired or its native outcome changed during capture")
                        source = ("native_source_eye_recording" if self.background_check
                                  else "state_only_no_held_video")
                        if captures:source="final_compositor_static_grips"
                        checkpoint = self.events.emit("held_visual_checkpoint", action=name,
                                                      label=step.get("capture_while_held"), source=source,
                                                      final_compositor_capture=bool(captures))
                        observed = {"state": held_state, "captures": captures,
                                    "visual_checkpoint": checkpoint}
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    if during:
                        raise BotFault("Requested while-held native outcome was not observed before release")
                    if sampled_hold:
                        raise BotFault("XR samples did not establish the requested hold duration before its deadline")
                    break
                time.sleep(min(.025, remaining))
        finally:
            if getattr(self, '_menu_focus_sequence_active', False):
                self.release(keep=self._menu_focus_keep)
            else:
                self.release()
        if evidence is None:
            raise BotFault("Requested physical input was never observed held by the native XR control sampler")
        if reviewed_only:
            # The requested single-channel lease has already expired/released.
            # Wait only for a fresh neutral native packet, never another edge.
            deadline = time.monotonic()+1.
            neutral_since = None
            released_guard = {key:value for key,value in step["state_before"].items()
                              if not key.startswith("controls.")}
            while time.monotonic() < deadline:
                released, controls = fresh_control_observation(self.observe, native=True,
                    timeout=max(0., deadline-time.monotonic()))
                if (controls["context"] != "menus" or not matches(released, released_guard)
                        or not matches(released.get("native", {}), step["native_before"])):
                    raise BotFault("Reviewed action lost its released native menu owner")
                if controls["sample_ms"] > evidence["sample_ms"] and reviewed_action_packet(released, name):
                    neutral_since = controls["sample_ms"] if neutral_since is None else neutral_since
                    if controls["sample_ms"]-neutral_since >= 100:
                        result = {"scope":"physical_input_and_release_only", "held_evidence":evidence,
                                  "released_state":released, "requested_lease_seconds":duration,
                                  "released_neutral_milliseconds":controls["sample_ms"]-neutral_since,
                                  "page_outcome_proven":False, "page_outcome_pending":True, "visual_review_required":True}
                        self.events.emit("reviewed_action_released", action=name, **result)
                        return result
                else:
                    neutral_since = None
                time.sleep(.025)
            raise BotFault("Reviewed action did not establish sampled neutral input after release")
        return observed

    def _finish_menu_cleanup(self, state):
        # A cleared terminal bit can coexist with a retained tutorial pause.
        # Require the live camera/control owner to return, not just menu=false.
        deadline = time.monotonic() + 3
        stable = 0
        while True:
            controls = state.get("controls")
            native = state.get("native")
            # An audit can expire during the native stow transition. Missing
            # observations are pending evidence, never a playable state.
            playable = (state.get("menu") is False
                        and state.get("scene") in ("gameplay", "cabin")
                        and state.get("camera_active") is True
                        and state.get("camera_suspended") is False
                        and state.get("camera_awaiting_player") is False
                        and isinstance(controls, dict) and controls.get("rig_input") is True
                        and isinstance(native, dict) and native.get("popup") is False)
            stable = stable + 1 if playable else 0
            if stable >= 3:
                self.opened_menu = None
                self.events.emit("test_menu_cleanup", status="closed", camera_restored=True)
                return
            if time.monotonic() >= deadline:
                raise BotFault("Test menu closed without restoring the live VR camera; session cleanup required")
            time.sleep(.1)
            state = self.observe(native=True)

    def cleanup_menus(self):
        if not getattr(self, "opened_menu", None):
            return
        self.release()
        # ACC Development and field Pause -> Options -> Control Type can have
        # several levels to unwind. Keep ordinary Back bounded, with fresh ownership before each
        # edge; never force terminal closure or acknowledge an unknown popup.
        for _ in range(4):
            state = self.observe(native=True)
            if state.get("menu") is False:
                self._finish_menu_cleanup(state)
                return
            native = state.get("native", {})
            if (state.get("menu") is not True or native.get("popup") is not False
                    or native.get("tutorial_pause") is not False
                    or state.get("idroid") is not (self.opened_menu == "idroid")
                    or (self.opened_menu == "idroid" and state.get("pause") is not False)):
                raise BotFault("Test menu cleanup needs review; refusing an unknown prompt or menu")
            self.execute({"op": "action", "name": "menus.back",
                          "state_before": {"menu": True, "idroid": self.opened_menu == "idroid",
                                           "pause": self.opened_menu != "idroid"},
                          "native_before": {"popup": False, "tutorial_pause": False}})
            time.sleep(.3)
        state = self.observe(native=True)
        if state.get("menu") is not False:
            raise BotFault("Test-opened menu did not close")
        self._finish_menu_cleanup(state)

    def close(self, *, cleanup=True):
        try:
            self.release()
            if cleanup:
                self.cleanup_menus()
            # Preserve field progress after an unsuccessful action. This is a
            # fresh release receipt, never an action pass or a menu bypass.
            output = getattr(self.events, 'output', None)
            if cleanup and isinstance(output, pathlib.Path):
                try:
                    until = time.monotonic() + .75
                    while True:
                        state = self.observe(native=True)
                        if released_field_state(state) or time.monotonic() >= until:
                            break
                        time.sleep(.025)
                    atomic_json(output / 'released-session.json', {
                        'schema': 1, 'unix_ns': time.time_ns(), 'identity': self.identity,
                        'status': 'released_field' if cleanup and not self.held and released_field_state(state)
                                  else 'needs_cleanup', 'state': state})
                except Exception as error:
                    atomic_json(output / 'released-session.json', {
                        'schema': 1, 'unix_ns': time.time_ns(), 'status': 'needs_cleanup',
                        'error': str(error)})
        finally:
            self.operator.close()
            if self.process_handle:
                self.kernel.CloseHandle(self.process_handle)
                self.process_handle = None
