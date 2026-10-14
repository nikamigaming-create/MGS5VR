"""Measured controller pairs and continuous bot poses for the physical optic.

This changes simulated inspection motion only. The production optic, anatomical
palms, eye origins, acquisition, and explicit marking keep their existing owners.
LOCAL and VIEW poses use metres and xyzw; composition is parent * child.
"""
import math
import time

from .combat import controller_pair, inverse, looking_head, mul, normalized, paired_aim_position
from .core import BotFault
from .live import authoritative_controls, channel, held_input_evidence, rotate
from .motion import _read
from .session import _controller_pose


# Imported housing sockets from include/mgs5vr/optic_rig.hpp. These are body
# coordinates, not controller calibration or native world coordinates.
OCULAR = (-.032788, -.000562, .05512)
PRIMARY = (.085, -.008, 0.)
SUPPORT = (-.078, -.008, -.018)
PALM = [1., 0., 0., 0.]
# Bot inspection target used in the recorded 191402/185316 right-eye aperture
# views. This is an authored VIEW pose, not a measurement of the user's IPD.
INSPECTION_OCULAR_VIEW = (.032, 0., -.06)


def _pose(value):
    if not isinstance(value, dict):
        raise BotFault('Binocular pose publication is unavailable')
    p, q = value.get('position'), value.get('orientation')
    if (not isinstance(p, (list, tuple)) or len(p) != 3
            or not isinstance(q, (list, tuple)) or len(q) != 4
            or any(type(v) not in (int, float) or not math.isfinite(v) for v in list(p)+list(q))
            or max(abs(v) for v in p) > 3
            or not .99 < math.sqrt(sum(v*v for v in q)) < 1.01):
        raise BotFault('Binocular poses require valid tracked metres and normalized xyzw rotations')
    return {'position': list(p), 'orientation': normalized(q)}


def _compose(parent, child):
    return {'position': [a+b for a, b in zip(parent['position'], rotate(parent['orientation'], child['position']))],
            'orientation': normalized(mul(parent['orientation'], child['orientation']))}


def _relative(parent, child):
    q = inverse(parent['orientation'])
    return {'position': rotate(q, [b-a for a, b in zip(parent['position'], child['position'])]),
            'orientation': normalized(mul(q, child['orientation']))}


def _paired(grip, pair):
    grip = _pose(grip)
    pair = _pose(pair)
    attitude = normalized(mul(grip['orientation'], inverse(pair['orientation'])))
    return grip, {'position': paired_aim_position(grip['position'], attitude, pair), 'orientation': attitude}


def grip_rotation(bindings):
    """Read the effective fit; native order is yaw * pitch * roll."""
    angles = []
    for axis in ('pitch', 'yaw', 'roll'):
        rows = [row.get('value') for row in bindings.get('settings', [])
                if row.get('name') == 'settings.binocular_'+axis+'_degrees']
        if (len(rows) != 1 or type(rows[0]) not in (int, float)
                or not math.isfinite(rows[0]) or abs(rows[0]) > 180):
            raise BotFault('Effective physical-binocular fit is unavailable: '+axis)
        angles.append(math.radians(rows[0])/2)
    pitch, yaw, roll = angles
    return normalized(mul([0., math.sin(yaw), 0., math.cos(yaw)],
                          mul([math.sin(pitch), 0., 0., math.cos(pitch)],
                              [0., 0., math.sin(roll), math.cos(roll)])))


def inspection_poses(head, right_pair, left_pair, fit, ocular_view=INSPECTION_OCULAR_VIEW):
    """Place the real authored aperture and both palms without faking aim joins."""
    head = _pose(head)
    ocular_view = _pose({'position': list(ocular_view), 'orientation': [0., 0., 0., 1.]})['position']
    body_q = head['orientation']
    ocular = _compose(head, {'position': ocular_view, 'orientation': [0., 0., 0., 1.]})['position']
    body = {'position': [p-d for p, d in zip(ocular, rotate(body_q, OCULAR))], 'orientation': body_q}
    right_aim_q = normalized(mul(body_q, inverse(fit)))
    right = {'position': _compose(body, {'position': PRIMARY, 'orientation': PALM})['position'],
             'orientation': normalized(mul(right_aim_q, _pose(right_pair)['orientation']))}
    left = _compose(body, {'position': SUPPORT, 'orientation': PALM})
    return {'right': _paired(right, right_pair), 'left': _paired(left, left_pair)}


def support_poses(frame, pair):
    """Derive the side cup from the same rendered LOCAL ocular publication."""
    if frame.get('binocular_held') is not True:
        raise BotFault('An authored binocular support contact requires the held physical optic')
    ocular = _pose(frame.get('binocular_ocular'))
    socket = {'position': [s-o for s, o in zip(SUPPORT, OCULAR)], 'orientation': PALM}
    return _paired(_compose(ocular, socket), pair)


def _angle(a, b):
    return math.degrees(2*math.acos(min(1., abs(sum(x*y for x, y in zip(a, b))))))


def _blend(a, b, fraction):
    # SLERP keeps per-packet angular speed bounded even for a 162 degree turn.
    first, last = a['orientation'], b['orientation']
    dot = sum(x*y for x, y in zip(first, last))
    if dot < 0:
        last = [-v for v in last]; dot = -dot
    dot = min(1., dot)
    if dot > .9995:
        q = normalized([x+(y-x)*fraction for x, y in zip(first, last)])
    else:
        theta = math.acos(dot); denominator = math.sin(theta)
        x, y = math.sin((1-fraction)*theta)/denominator, math.sin(fraction*theta)/denominator
        q = normalized([u*x+v*y for u, v in zip(first, last)])
    return {'position': [x+(y-x)*fraction for x, y in zip(a['position'], b['position'])], 'orientation': q}


def static_support_capture_allowed(held, state, bindings):
    """A narrow integration predicate for a stationary binocular side cup.

    Live.capture checks this predicate before each compositor eye. A recording
    checkpoint remains source-eye evidence unless compositor pixels are saved.
    """
    if (set(held) != {('left', 'Grip', None)} or state.get('camera_active') is not True
            or state.get('scene') != 'gameplay' or state.get('gamepad') is not False
            or state.get('menu') is not False or state.get('idroid') is not False
            or state.get('rendered', {}).get('binocular_held') is not True):
        return False
    item = held[('left', 'Grip', None)]
    if (item.get('hand') != 'left' or item.get('component') != 'Grip'
            or item.get('sub_component') or type(item.get('value')) not in (int, float)
            or not math.isfinite(item['value']) or not 0 < item['value'] <= 1):
        return False
    try:
        controls = authoritative_controls(state)
    except BotFault:
        return False
    if (controls['context'] != 'binoculars' or controls.get('rig_input') is not True
            or not held_input_evidence(state, ['left_grip']) or controls.get('native_buttons') != 0
            or controls.get('native_triggers') != [0, 0]
            or any(abs(v) > .01 for i, v in enumerate(controls['physical']) if i != 7)
            or any(abs(v) > .01 for v in controls['sticks'])):
        return False
    axes = controls.get('native_axes'); frame = state.get('rendered') or {}
    if (not isinstance(axes, list) or len(axes) != 4
            or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > .01 for v in axes)
            or len(controls['physical']) != 11 or frame.get('presentation_focused') is not True
            or any(frame.get('raw_'+hand+'_'+kind+'_tracked') is not True
                   for hand in ('left', 'right') for kind in ('grip', 'aim'))
            or type(frame.get('sample_ms')) is not int or type(state.get('now_ms')) is not int
            or not 0 <= state['now_ms']-frame['sample_ms'] <= 150):
        return False
    try:
        ocular = _pose(frame.get('binocular_ocular')); left = _pose(frame.get('left_grip'))
        socket = _compose(ocular, {'position': [s-o for s, o in zip(SUPPORT, OCULAR)], 'orientation': PALM})
        if math.dist(left['position'], socket['position']) > .12:
            return False
    except BotFault:
        return False
    matched = False
    for action in bindings.get('actions', []):
        if 'binoculars' not in action.get('contexts', []):
            continue
        for binding in action.get('bindings', []):
            inputs = set(binding.get('inputs', []))
            if inputs and inputs <= {'left_grip'}:
                name=action.get('name')
                # The established iDroid focus modifier shares LG in normal
                # contexts, but xr_runtime gates it on idroidRouting. This
                # predicate already requires the handset closed and a neutral
                # native packet. Admit that exact inert modifier only.
                if (name not in ('binoculars.support_grip','system.idroid_focus')
                        or action.get('modifier') is not True or binding.get('gesture') != 'level'
                        or binding.get('inputs') != ['left_grip']):
                    return False
                if name=='binoculars.support_grip':matched = True
    return matched


class BinocularPresentation:
    """One recon's sampled poses; caller owns equip, acquisition and stow.

    ``observe`` must retain the recon's exact world/player/activation/epoch and
    preempt health loss or detection. No native camera, stick aim, or game state
    write is performed here. Calls are serialized simulator RPCs, not atomic XR.
    """
    def __init__(self, live, observe, *, clock=time.monotonic, sleep=time.sleep):
        self.live, self.observe, self.clock, self.sleep = live, observe, clock, sleep
        self.fit = grip_rotation(live.bindings)
        self.initial_head = _read(live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
        self.initial = {}
        self.pairs = {}
        for hand in ('left', 'right'):
            grip = _pose(_controller_pose(live, hand, 'grip', 'local'))
            aim = _pose(_controller_pose(live, hand, 'aim', 'local'))
            self.initial[hand] = (grip, aim)
            self.pairs[hand] = controller_pair(grip, aim)
        self.identity = None
        self.support_until = 0.
        self.state = self.sample()

    def sample(self):
        state = self.observe()
        frame, controls = state.get('rendered') or {}, state.get('controls') or {}
        native = state.get('native') or {}
        identity = (native.get('mission'), native.get('location'), state.get('activation'),
                    frame.get('player_owner'), frame.get('presentation_epoch'))
        if (any(type(v) is not int or v <= 0 for v in identity)
                or (self.identity is not None and identity != self.identity)
                or controls.get('context') not in ('gameplay', 'binoculars')
                or controls.get('rig_input') is not True
                or frame.get('presentation_focused') is not True
                or any(frame.get('raw_'+hand+'_'+kind+'_tracked') is not True
                       for hand in ('left', 'right') for kind in ('grip', 'aim'))
                or type(state.get('now_ms')) is not int or type(frame.get('sample_ms')) is not int
                or not 0 <= state['now_ms']-frame['sample_ms'] <= 150):
            raise BotFault('Binocular motion lost its fresh tracked field owner')
        self.identity = identity
        self.state = state
        return state

    def _published_after(self, stamp):
        until = self.clock()+.75
        while True:
            state = self.sample()
            if state['rendered']['sample_ms'] > stamp:
                return state
            if self.clock() >= until:
                raise BotFault('Binocular rig did not publish after the pose packet; no motion replay')
            self.sleep(.015)

    def _motion(self, end_head, end_grips, *, follow_head, seconds=.4):
        if type(seconds) not in (int, float) or not .1 <= seconds <= 3:
            raise BotFault('Binocular motion needs a bounded 0.1..3 second duration')
        self.sample()
        head = _read(self.live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
        end_head = _pose(end_head)
        starts = {hand: _pose(_controller_pose(self.live, hand, 'grip', 'local')) for hand in ('left', 'right')}
        ends = {hand: _pose(end_grips[hand]) for hand in ('left', 'right')}
        if follow_head:
            starts = {h: _relative(head, p) for h, p in starts.items()}
            ends = starts
        travel = max(math.dist(starts[h]['position'], ends[h]['position']) for h in starts)
        angle = max([_angle(head['orientation'], end_head['orientation'])]+
                    [_angle(starts[h]['orientation'], ends[h]['orientation']) for h in starts])
        packets = max(1, math.ceil(angle/6.), math.ceil(travel/.015), math.ceil(seconds/.05))
        if follow_head:
            reach = max(math.sqrt(sum(v*v for v in starts[h]['position'])) for h in starts)
            packets = max(packets, math.ceil(math.radians(angle)*reach/.015))
        # Never skip intermediate poses after a slow RPC; large absolute turns
        # must not become a single late correction or lose the measured pairs.
        begun = self.clock(); largest_gap = 0.; last = begun
        try:
            for index in range(1, packets+1):
                before = self.sample()
                if self.support_until:
                    self.hold_support()
                if self.clock()-begun > 5.:
                    raise BotFault('Binocular motion exceeded five seconds; no pose replay')
                weight = index/packets
                moved_head = _blend(head, end_head, weight)
                poses = {}
                for hand in ('left', 'right'):
                    grip = _blend(starts[hand], ends[hand], weight)
                    if follow_head:
                        grip = _compose(moved_head, grip)
                    poses[hand] = _paired(grip, self.pairs[hand])
                # Validate the entire packet before its first mutation.
                for pair in poses.values():
                    for pose in pair:
                        _pose(pose)
                if head != end_head:
                    self.live.call('set_head_pose', {'base_space': 'local', **moved_head})
                for hand in ('right', 'left'):
                    for kind, pose in zip(('grip', 'aim'), poses[hand]):
                        self.live.call('set_controller_pose', {'hand': hand, 'pose_type': kind, 'base_space': 'local', **pose})
                state = self._published_after(before['rendered']['sample_ms'])
                now = self.clock(); largest_gap = max(largest_gap, now-last); last = now
                if largest_gap > .5:
                    raise BotFault('Binocular motion stalled for more than 500 ms; no pose replay')
                self.live.events.emit('binocular_motion_sample', fraction=weight, packets=packets,
                                      sample_ms=state['rendered']['sample_ms'],
                                      rig_sequence=state['rendered'].get('rig_sequence'),
                                      head=moved_head, pose_rpc_atomic=False)
                self.sleep(max(0., seconds*weight-(self.clock()-begun)))
            actual_head = _read(self.live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
            if math.dist(actual_head['position'], end_head['position']) > .001 or _angle(actual_head['orientation'], end_head['orientation']) > .1:
                raise BotFault('Binocular head endpoint did not match fresh operator readback')
            for hand in ('left', 'right'):
                for kind, target in zip(('grip', 'aim'), poses[hand]):
                    actual = _pose(_controller_pose(self.live, hand, kind, 'local'))
                    if math.dist(actual['position'], target['position']) > .001 or _angle(actual['orientation'], target['orientation']) > .1:
                        raise BotFault('Binocular paired hand endpoint did not match fresh operator readback')
            self.sample()
        except Exception:
            self.live.release(); self.support_until = 0.
            raise
        self.live.events.emit('binocular_motion_completed', packets=packets, seconds=self.clock()-begun,
                              largest_packet_gap_seconds=largest_gap, max_angular_step_degrees=6.,
                              max_hand_step_metres=.015, visual_acceptance='pending_recorded_motion_review')
        return self.state

    def raise_to_eye(self, *, seconds=.45):
        head = _read(self.live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
        poses = inspection_poses(head, self.pairs['right'], self.pairs['left'], self.fit)
        return self._motion(head, {h: p[0] for h, p in poses.items()}, follow_head=False, seconds=seconds)

    def look_at(self, target, *, seconds=.35):
        state = self.sample()
        target = list(target)
        if len(target) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) for v in target):
            raise BotFault('Binocular look target is invalid')
        head = _read(self.live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
        end = {**head, 'orientation': looking_head(head['orientation'], state['rendered']['native_pose'], target)}
        grips = {h: self.initial[h][0] for h in self.initial}  # ignored for a following-head path
        return self._motion(end, grips, follow_head=True, seconds=seconds)

    def seat_support(self, *, seconds=.2):
        state = self.sample()
        left, _ = support_poses(state['rendered'], self.pairs['left'])
        right = _pose(_controller_pose(self.live, 'right', 'grip', 'local'))
        head = _read(self.live, 'get_head_pose', {'base_space': 'local'}, local_head=True)
        return self._motion(head, {'right': right, 'left': left}, follow_head=False, seconds=seconds)

    def align_to_ocular(self, *, seconds=.2):
        """Use the rendered optic's attachment correction, without resetting it."""
        state = self.sample(); frame = state['rendered']
        if frame.get('binocular_held') is not True:
            raise BotFault('Ocular feedback requires the held physical binoculars')
        head = _pose(frame.get('head_pose')); ocular = _pose(frame.get('binocular_ocular'))
        target = _compose(head, {'position': list(INSPECTION_OCULAR_VIEW), 'orientation': [0., 0., 0., 1.]})['position']
        delta = [a-b for a, b in zip(target, ocular['position'])]
        distance = math.sqrt(sum(v*v for v in delta))
        if distance > .06:
            raise BotFault('Rendered ocular differs by over six centimetres; inspect the actual rig before correcting')
        right = _pose(_controller_pose(self.live, 'right', 'grip', 'local'))
        left, _ = support_poses(frame, self.pairs['left'])
        right['position'] = [p+d for p, d in zip(right['position'], delta)]
        left['position'] = [p+d for p, d in zip(left['position'], delta)]
        self.live.events.emit('binocular_ocular_feedback', distance_metres=distance,
                              source_sample_ms=frame['sample_ms'], ocular_target_source='authored_bot_inspection_view_pose',
                              measured_eye_origins=False)
        return self._motion(head, {'right': right, 'left': left}, follow_head=False, seconds=seconds)

    def hold_support(self):
        state = self.sample()
        rows = [a for a in self.live.bindings.get('actions', []) if a.get('name') == 'binoculars.support_grip']
        if (state['controls']['context'] != 'binoculars' or state['rendered'].get('binocular_held') is not True
                or len(rows) != 1 or rows[0].get('modifier') is not True
                or 'binoculars' not in rows[0].get('contexts', [])
                or not rows[0].get('bindings') or rows[0]['bindings'][0].get('gesture') != 'level'
                or rows[0]['bindings'][0].get('inputs') != ['left_grip']):
            raise BotFault('Binocular support needs the effective tracked left-grip modifier')
        contact, _ = support_poses(state['rendered'], self.pairs['left'])
        if math.dist(_pose(state['rendered'].get('left_grip'))['position'], contact['position']) > .12:
            raise BotFault('Tracked left hand has not reached the authored binocular side cup')
        if self.clock()+.65 >= self.support_until:
            self.live.input([channel('left_grip')], 2., lease_seconds=2.)
            self.support_until = self.clock()+2.
            self.live.events.emit('binocular_support_lease', action='binoculars.support_grip', seconds=2.)
        return state

    def press(self, name, *, acquired=False, seconds=.25):
        """Keep support while using an existing explicit mark or zoom binding."""
        if name not in ('binoculars.mark', 'binoculars.zoom') or (name == 'binoculars.mark' and acquired is not True):
            raise BotFault('Explicit binocular mark requires the caller\'s fresh native acquisition evidence')
        if type(seconds) not in (int, float) or not .03 <= seconds <= .5:
            raise BotFault('Binocular mark/zoom needs a bounded 0.03..0.5 second press')
        rows = [a for a in self.live.bindings.get('actions', []) if a.get('name') == name]
        expected = ['right_trigger'] if name == 'binoculars.mark' else ['left_stick_click']
        if (len(rows) != 1 or 'binoculars' not in rows[0].get('contexts', [])
                or not rows[0].get('bindings') or rows[0]['bindings'][0].get('gesture') != 'press'
                or rows[0]['bindings'][0].get('inputs') != expected):
            raise BotFault('Physical binocular inspection binding differs: '+name)
        before = self.hold_support(); begun = self.clock(); deadline = begun+seconds; audit = None
        try:
            self.live.input([channel(t) for t in expected], seconds, lease_seconds=seconds)
            while True:
                state = self.sample()
                if audit is None:
                    audit = held_input_evidence(state, expected, after_sample_ms=before['controls']['sample_ms'])
                if self.clock() >= deadline:
                    break
                self.sleep(min(.025, max(0., deadline-self.clock())))
            if audit is None:
                raise BotFault('Binocular action input was not sampled; no action replay')
            self.live.events.emit('binocular_supported_action', action=name, audit=audit,
                                  native_outcome_acceptance=False, acquired_by_caller=acquired)
        except Exception:
            self.release()
            raise
        finally:
            self.live.release(keep=(('left', 'Grip', None),) if self.support_until else ())
        return self.sample()

    def support_observation(self):
        """Input/socket evidence, not a substitute for actual palm pixels."""
        state = self.hold_support()
        state = self._published_after(state['rendered']['sample_ms'])
        frame = state['rendered']
        contact, _ = support_poses(frame, self.pairs['left'])
        left = _pose(frame.get('left_grip'))
        evidence = {'sample_ms': frame['sample_ms'], 'rig_sequence': frame.get('rig_sequence'),
                    'sampled_left_grip': bool(held_input_evidence(state, ['left_grip'])),
                    'authored_contact_distance': math.dist(left['position'], contact['position']),
                    'actual_contact_flag_published': False,
                    'visual_acceptance': 'pending_both_eye_and_motion_review'}
        self.live.events.emit('binocular_support_observation', **evidence)
        return evidence

    def release(self):
        self.live.release(); self.support_until = 0.

    def restore(self, *, seconds=.45):
        """Call after the caller stows; avoid the current instantaneous return."""
        self.release()
        state = self.sample()
        if state['rendered'].get('binocular_held') is not False or state['controls']['context'] != 'gameplay':
            raise BotFault('Stow the physical binoculars before restoring the inspection pose')
        return self._motion(self.initial_head, {h: p[0] for h, p in self.initial.items()},
                            follow_head=False, seconds=seconds)
