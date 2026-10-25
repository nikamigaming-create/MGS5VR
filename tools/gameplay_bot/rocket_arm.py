"""One bounded flight of the already equipped native Rocket/Blast Arm."""
import math
import time

from .core import BotFault, atomic_json
from .live import channel, rotate
from .combat import mul
from .session import preserved_tracking_poses, _controller_pose


def inspect_flight(live):
    result = {'status': 'running', 'samples': [], 'captures': []}
    deadline = time.monotonic() + 60.
    ready = next(a for a in live.bindings['actions'] if a['name'] == 'gameplay.ready_weapon')
    fire = next(a for a in live.bindings['actions'] if a['name'] == 'gameplay.fire_or_cqc')
    if (ready['bindings'][0]['inputs'] != ['right_grip']
            or fire['bindings'][0]['inputs'] != ['right_trigger']):
        raise BotFault('Rocket inspection requires the audited grip-ready and trigger bindings')
    native, _ = live.native.read('return string.format("{\\"hand\\":%d}",vars.handEquip or -1)')
    if native['hand'] not in (517, 518):
        raise BotFault('The equipped native hand is not Rocket/Blast Arm')
    result['hand'] = native['hand']

    def sample(label):
        state = live.observe()
        if (time.monotonic() > deadline or state.get('scene') != 'gameplay'
                or any(state.get(k) is not False for k in ('menu', 'loading', 'demo', 'gamepad'))):
            raise BotFault('Rocket inspection lost its bounded gameplay owner')
        frame = state.get('rendered') or {}
        result['samples'].append({'phase': label, 'time': time.monotonic(),
            'guidance': frame.get('rocket_arm_guidance'), 'camera': frame.get('rocket_arm_camera'),
            'view': frame.get('native_pose'), 'controls': state.get('controls'),
            'left_grip': frame.get('left_grip')})
        return frame.get('rocket_arm_guidance') is True

    def hold(label, seconds, flight=False):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            live.input([channel('right_grip')], .1, lease_seconds=1.)
            active = sample(label)
            if flight and not active:
                return False
            time.sleep(.04)
        return True

    with preserved_tracking_poses(live):
        try:
            live.release()
            sample('before')
            live.execute({'op': 'pose', 'hand': 'left', 'position': [0., -.25, -.55],
                'orientation': [.59974526894386, -.32601793907784127, .5887929828136983, .43282876425700023]})
            left = {kind: _controller_pose(live, 'left', kind, 'local') for kind in ('grip', 'aim')}
            hold('ready', 1.)
            live.input([channel('right_grip'), channel('right_trigger')], .3, lease_seconds=.5)
            time.sleep(.3)
            live.release(keep=[('right', 'Grip', None)])
            until = time.monotonic() + 4.
            while not sample('launch'):
                if time.monotonic() > until:
                    raise BotFault('Native rocket flight did not begin')
                live.input([channel('right_grip')], .1, lease_seconds=1.)
                time.sleep(.04)
            if not hold('neutral-flight', .5, True):
                raise BotFault('Flight ended before hand steering')
            pitch = math.radians(20.) / 2
            raised = [math.sin(pitch), 0., 0., math.cos(pitch)]
            for kind, pose in left.items():
                live.call('set_controller_pose', {'hand': 'left', 'pose_type': kind, 'base_space': 'local',
                    **pose, 'position': [pose['position'][0],pose['position'][1]+.12,pose['position'][2]+.02],
                    'orientation': mul(raised, pose['orientation'])})
            if not hold('left-hand-pitch', .7, True):
                raise BotFault('Flight ended during hand elevation')
            yaw = math.radians(30.) / 2
            for kind, pose in left.items():
                live.call('set_controller_pose', {'hand': 'left', 'pose_type': kind, 'base_space': 'local',
                    **pose, 'position': [pose['position'][0]-.14,pose['position'][1]+.12,pose['position'][2]+.04],
                    'orientation': mul([0., math.sin(yaw), 0., math.cos(yaw)], mul(raised, pose['orientation']))})
            if not hold('left-hand-yaw', 1., True):
                raise BotFault('Flight ended during hand steering')
            def bearing(phase):
                pose=next(s['camera'] for s in reversed(result['samples']) if s['phase']==phase)
                forward=rotate(pose['orientation'],[0.,0.,1.])
                return [math.degrees(math.atan2(forward[0],forward[2])),
                        math.degrees(math.atan2(forward[1],math.hypot(forward[0],forward[2])))]
            neutral,elevated,turned=[bearing(p) for p in ('neutral-flight','left-hand-pitch','left-hand-yaw')]
            result['native_hand_response']={'elevation_degrees':elevated[1]-neutral[1],
                'left_yaw_degrees':(turned[0]-elevated[0]+180.)%360.-180.}
            if min(result['native_hand_response'].values())<2.:
                raise BotFault('Socket movement did not steer the native projectile in both requested directions: '
                               +str(result['native_hand_response']))
            for kind, pose in left.items():
                live.call('set_controller_pose', {'hand': 'left', 'pose_type': kind, 'base_space': 'local', **pose})
            hold('hand-return', .3, True)
            live.input([channel('right_grip'), {**channel('left_stick_right'), 'value': .6}], 1.)
            until = time.monotonic() + 1.
            while time.monotonic() < until and sample('native-left-stick'):
                time.sleep(.04)
            live.release(keep=[('right', 'Grip', None)])
            hold('stick-settling', .8, True)
            if sample('after-stick'):
                live.input([channel('right_grip')], .1, lease_seconds=3.)
                result['captures'] = live.capture('rocket-flight-screen', static_grips=True)
            live.release()
            while sample('return'):
                time.sleep(.1)
            result['status'] = 'observed_flight_return'
            return result
        finally:
            live.release()
            atomic_json(live.events.output / 'rocket-flight.json', result)
