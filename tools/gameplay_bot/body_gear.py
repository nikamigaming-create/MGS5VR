"""Observe the real mounted loadout without gating on unrelated foot clearance."""
import math
import time

from .core import BotFault, atomic_json
from .combat import pointing
from .session import preserved_tracking_poses, _controller_pose
from .cabin_workbench import compose, shifted


def inspect(live, seconds):
    deadline = time.monotonic() + min(seconds, 90.)
    result = {'status': 'running', 'phases': [], 'visual_acceptance': 'not_reviewed',
              'interaction_acceptance': 'not_tested', 'headset_acceptance': 'not_tested'}
    scene = None

    def observe():
        state = live.observe(native=True)
        if (time.monotonic() >= deadline or state.get('scene') not in ('cabin', 'gameplay')
                or (scene is not None and state['scene'] != scene)
                or any(state.get(k) is not False for k in ('pause', 'idroid', 'loading', 'demo', 'gamepad'))):
            raise BotFault('Body gear observation lost its bounded native scene')
        return state

    with preserved_tracking_poses(live):
        try:
            live.release()
            state = observe()
            scene = state['scene']
            head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
            hands = [_controller_pose(live, side, kind, 'local')
                     for side in ('left', 'right') for kind in ('grip', 'aim')]
            for label, direction in (('front', [0., -.6, -1.]),
                                     ('belt', [0., -1., -.25]),
                                     ('left', [-.7, -.8, -.25]),
                                     ('right', [.7, -.8, -.25])):
                target = compose(head, {'position': [0., 0., 0.], 'orientation': pointing(direction)})
                live.call('set_head_pose', {'base_space': 'local', **target})
                for hand in hands:
                    live.call('set_controller_pose', hand)
                until = min(deadline, time.monotonic() + 6.)
                while True:
                    state = observe()
                    frame = state.get('rendered') or {}
                    pose = frame.get('head_pose') or {}
                    if (state.get('camera_active') and not state.get('camera_suspended')
                            and 0 <= state['now_ms'] - frame.get('sample_ms', -1000) < 150
                            and math.dist(pose.get('position', [999.]*3), target['position']) < .001
                            and abs(sum(a*b for a, b in zip(pose.get('orientation', []), target['orientation']))) > .99999):
                        break
                    if time.monotonic() >= until:
                        raise BotFault('Body gear head pose did not reach the rendered frame')
                    time.sleep(.05)
                result['phases'].append({'label': label, 'state': state,
                                         'eyes': live.capture('body-gear-' + label)})
                atomic_json(live.events.output / 'body-gear-progress.json', result)
            result['status'] = 'observed_body_gear'
        except Exception as error:
            result.update(status='failed', error=str(error))
            raise
        finally:
            live.release()
            atomic_json(live.events.output / 'body-gear-progress.json', result)
    return result


def inspect_camera_grab(live, seconds):
    """Touch/drift/grab/return only; no shutter, settings or inventory writes."""
    deadline = time.monotonic() + min(seconds, 60.)
    result = {'status': 'running', 'phases': [], 'headset_acceptance': 'not_tested'}

    def observe():
        state = live.observe()
        studio = (state.get('rendered') or {}).get('studio') or {}
        if (time.monotonic() >= deadline or state.get('scene') != 'gameplay'
                or any(state.get(k) is not False for k in ('pause', 'idroid', 'loading', 'demo', 'gamepad'))
                or not studio.get('available') or studio.get('cabin')):
            raise BotFault('Mounted camera test lost field ownership')
        return state, studio

    def move(pose):
        for kind in ('grip', 'aim'):
            live.call('set_controller_pose', {'hand': 'right', 'pose_type': kind, 'base_space': 'local', **pose})

    with preserved_tracking_poses(live):
        try:
            live.release()
            _, initial = observe()
            if initial.get('camera_hand') != -1:
                raise BotFault('Mounted camera test requires an unheld camera')
            for label, delay, expected in (('grace', .5, 1), ('expired', 1.2, -1)):
                _, studio = observe()
                mount = studio['camera']
                contact_until=min(deadline,time.monotonic()+4.)
                while True:
                    move(mount)
                    time.sleep(.1)
                    _, studio = observe()
                    if studio.get('camera_candidate_mask',0)&2:
                        mount=studio['camera']
                        break
                    if time.monotonic()>=contact_until:
                        raise BotFault('No confirmed contact with the current body camera mount')
                    mount=studio['camera']
                move(shifted(mount, [.173, 0., 0.]))
                time.sleep(delay)
                before, _ = observe()
                result['phases'].append({'label': label + '-before-grip', 'state': before})
                atomic_json(live.events.output / 'camera-grab-progress.json', result)
                live.input([{'hand': 'right', 'component': 'Grip', 'value': 1.}], .12, lease_seconds=3.)
                # input() starts an expiring lease; it does not wait for XR
                # polling or a new completed native skin publication.
                time.sleep(.2)
                state, studio = observe()
                result['phases'].append({'label': label, 'state': state,
                    'eyes': live.capture('camera-grab-' + label, static_grips=True)})
                if studio.get('camera_hand') != expected:
                    raise BotFault('Mounted camera ' + label + ' did not match expected ownership')
                live.release()
                time.sleep(.12)
                state, studio = observe()
                if studio.get('camera_hand') != -1:
                    raise BotFault('Released camera did not return')
                result['phases'].append({'label': label + '-released', 'state': state})
                move(shifted(mount, [.4, 0., 0.]))
                time.sleep(.1)
            result['status'] = 'observed_camera_grab_window'
        except Exception as error:
            result.update(status='failed', error=str(error))
            raise
        finally:
            live.release()
            atomic_json(live.events.output / 'camera-grab-progress.json', result)
    return result
