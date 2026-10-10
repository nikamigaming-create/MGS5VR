"""Stationary wheel range checks through the maintained physical input lease."""
import math
import time
from contextlib import ExitStack

from .core import BotFault, matches
from .live import channel, held_input_evidence, rotate, vehicle_wheel_test_owner
from .motion import _pose
from .session import preserved_controller_pose
from .combat import controller_pair, inverse, mul, normalized, paired_aim_position


def _response(state, expected, after):
    """Join physical wheel output with the packet actually returned to retail."""
    wheel = state.get('rendered', {}).get('driver_wheel_axis')
    delivery = state.get('xinput_delivery', {}).get('index0') or {}
    stamp, now = delivery.get('sample_ms'), state.get('now_ms')
    axes = delivery.get('axes')
    if (type(wheel) not in (int, float) or not math.isfinite(wheel)
            or delivery.get('caller_in_game') is not True or delivery.get('packet_valid') is not True
            or delivery.get('source') != 'xr_mailbox' or delivery.get('result_code') != 0
            or type(stamp) is not int or type(now) is not int or not after <= stamp <= now
            or now-stamp > 150 or delivery.get('buttons') != 0 or delivery.get('triggers') != [0, 0]
            or not isinstance(axes, list) or len(axes) != 4 or axes[1:] != [0, 0, 0]
            or type(axes[0]) is not int or not -32767 <= axes[0] <= 32767):
        return False
    mapped = state['controls']['native_axes'][0]
    return ((abs(wheel) <= .05 and abs(mapped) <= 1639 and abs(axes[0]) <= 1639)
            if expected == 0 else
            (wheel*expected >= .9 and mapped*expected >= 29490 and axes[0]*expected >= 29490))


def sweep(live, step, *, clock=time.monotonic, sleep=time.sleep):
    degrees = step.get('wheel_sweep_degrees')
    both = step.get('include_left_wheel') is True
    name = 'vehicle.right_wheel_grip' if both else 'vehicle.wheel_grip'
    if (degrees not in (-35, 35) or type(degrees) not in (int, float) or live.held
            or step.get('name') != name or any(step.get(k) for k in
                ('include_ready', 'include_support', 'static_grip_capture'))):
        raise BotFault('Wheel sweep requires released input and an exact left/both-hand signed35-degree case')
    tokens = ['left_grip', 'right_grip'] if both else ['left_grip']
    for token, action_name in zip(tokens, ['vehicle.wheel_grip', 'vehicle.right_wheel_grip']):
        action = next((a for a in live.bindings['actions'] if a['name'] == action_name), None)
        if (not action or action.get('modifier') is not True or 'vehicle' not in action.get('contexts', [])
                or len(action.get('bindings', [])) != 1 or action['bindings'][0].get('gesture') != 'level'
                or action['bindings'][0].get('inputs') != [token]):
            raise BotFault('Wheel sweep requires the exact effective grip-only wheel bindings')
    initial = live.observe(native=True)
    owner = vehicle_wheel_test_owner(initial, step.get('native_before'))
    if owner is None or (step.get('state_before') and not matches(initial, step['state_before'])):
        raise BotFault('Wheel sweep requires a neutral, tracked native driver')
    root = tuple(initial['native']['player_'+axis] for axis in 'xyz')
    contacts = {side: _pose(**initial['rendered']['driver_'+side+'_wheel_grip']) for side in ('left', 'right')}
    center = [(a+b)*.5 for a, b in zip(contacts['left']['position'], contacts['right']['position'])]
    head = _pose(**initial['rendered']['head_pose'], local_head=True)
    axis = normalized(rotate(head['orientation'], [0., 0., -1.]))
    mask = 3 if both else 1
    stationary_since = initial['controls']['sample_ms']
    before_deadline = clock()+1.
    while True:
        current = live.observe(native=True)
        if vehicle_wheel_test_owner(current, step['native_before'], root=root) != owner:
            raise BotFault('Wheel sweep lost stationary ownership before gripping')
        if current['controls']['sample_ms']-stationary_since >= 100:
            break
        if clock() >= before_deadline:
            raise BotFault('Stationary driver samples did not advance')
        sleep(.025)
    with ExitStack() as saved:
        pairs = {}
        for side in ('left', 'right'):
            grip = saved.enter_context(preserved_controller_pose(live, side, 'grip', 'local'))
            aim = saved.enter_context(preserved_controller_pose(live, side, 'aim', 'local'))
            pairs[side] = controller_pair(grip, aim)

        def pose(angle):
            half = math.radians(angle)/2
            rotation = [v*math.sin(half) for v in axis]+[math.cos(half)]
            for side in ('left', 'right') if both else ('left',):
                grip = contacts[side]
                attitude = normalized(mul(rotation, grip['orientation']))
                point = [a+b for a, b in zip(center, rotate(rotation,
                    [p-c for p, c in zip(grip['position'], center)]))]
                aim = normalized(mul(attitude, inverse(pairs[side]['orientation'])))
                for kind, pos, quat in (('grip', point, attitude), ('aim',
                        paired_aim_position(point, aim, pairs[side]), aim)):
                    live.call('set_controller_pose', {'hand':side, 'pose_type':kind,
                        'base_space':'local', 'position':pos, 'orientation':quat})

        pose(0)
        current = live.observe(native=True)
        if vehicle_wheel_test_owner(current, step['native_before'], root=root) != owner:
            raise BotFault('Wheel sweep owner changed while reaching the wheel')
        admitted = current['now_ms']
        lease_deadline = clock()+2.65

        def observe():
            if clock() >= lease_deadline:
                raise BotFault('Wheel sweep reached its independent grip expiry')
            value = live.observe(native=True)
            if vehicle_wheel_test_owner(value, step['native_before'], root=root,
                    grips=True, steering=True) != owner:
                raise BotFault('Wheel sweep lost its driver, stationary root, tracking, or neutral pedals')
            return value

        def wait_response(expected, timeout):
            deadline = min(lease_deadline, clock()+timeout)
            since = None
            while True:
                value = observe()
                audit = held_input_evidence(value, tokens, after_sample_ms=admitted)
                valid = (audit and value['rendered'].get('driver_wheel_hands') == mask
                         and _response(value, expected, admitted))
                if valid:
                    since = audit['sample_ms'] if since is None else since
                    if audit['sample_ms']-since >= 50:
                        return value
                else:
                    since = None
                if clock() >= deadline:
                    raise BotFault('Native wheel steering response did not match the signed physical sweep')
                sleep(.025)
        try:
            live.input([channel(t) for t in tokens], .3, lease_seconds=2.65)
            wait_response(0, .5)
            for fraction in (.25, .5, .75, 1.):
                pose(degrees*fraction)
                observe()
                sleep(.04)
            endpoint = wait_response(1 if degrees > 0 else -1, .5)
            if step.get('while_held') and not matches(endpoint, step['while_held']):
                raise BotFault('Wheel sweep did not satisfy the authored held outcome')
            live.events.emit('vehicle_wheel_sweep_response', degrees=degrees, hands=mask, state=endpoint,
                evidence_limit='Stationary physical steering and retail XInput delivery; vehicle travel and visual review are separate')
            for fraction in (.75, .5, .25, 0.):
                pose(degrees*fraction)
                observe()
                sleep(.04)
            neutral = wait_response(0, .4)
            live.events.emit('vehicle_wheel_sweep_returned_neutral', state=neutral)
        finally:
            live.release()
    return {'state':endpoint, 'captures':[], 'visual_checkpoint':{
        'source':'state_only_no_held_video', 'grip_lease_seconds':2.65}}
