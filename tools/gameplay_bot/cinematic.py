"""Inspect real authored scenes using tracked head motion and viewer movement."""
import math
import time

from .core import BotFault
from .combat import mul
from .session import _controller_pose, preserved_tracking_poses


def inspect_cinematic(live, timeout, policy=None):
    if not 0 < timeout <= 180:
        raise BotFault('Cinematic inspection requires a bounded three-minute session')
    deadline = time.monotonic() + timeout
    result = {'status': 'running', 'phases': [], 'physical_headset_acceptance': 'not_run'}
    policy = policy or {}

    def observe():
        state = live.observe(native=True)
        frame = state.get('rendered') or {}
        native = state.get('native') or {}
        lesson = (state.get('demo_interactive_look') is True and native.get('mission') == 10010
                  and native.get('sequence') in ('Seq_Game_FewDaysLater0', 'Seq_Game_FewDaysLater1'))
        if (time.monotonic() > deadline or state.get('demo') is not True
                or state.get('camera_active') is not True or state.get('camera_suspended') is not False
                or state.get('title_menu') is True or state.get('pause') is True
                or state.get('idroid') is True or state.get('loading') is True
                or (native.get('demo') is not True and not lesson)
                or state.get('native', {}).get('popup') is True
                or frame.get('presentation_focused') is not True):
            raise BotFault('Cinematic inspection lost its active native authored scene')
        return state

    def capture(label):
        state = observe()
        result['phases'].append({'label': label, 'state': state, 'eyes': live.capture(label)})
        return state

    with preserved_tracking_poses(live):
        live.release()
        observe()
        head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
        hands = [_controller_pose(live, hand, kind, 'local')
                 for hand in ('left', 'right') for kind in ('grip', 'aim')]

        def set_head(pose):
            observe()
            live.call('set_head_pose', {'base_space': 'local', **pose})
            if policy.get('hospital_look') is True:
                sample = observe()
                result.setdefault('head_motion_samples', []).append({
                    'native': sample.get('native'), 'controls': sample.get('controls'),
                    'head_pose': sample.get('rendered', {}).get('head_pose')})
            # The simulator moves hands with set_head_pose; head-only motion
            # restores their measured absolute LOCAL transforms immediately.
            for hand in hands:
                live.call('set_controller_pose', hand)
            end = min(deadline, time.monotonic() + 4.)
            while time.monotonic() < end:
                frame = observe().get('rendered') or {}
                tracked = frame.get('head_pose') or {}
                if (abs(sum(a*b for a, b in zip(tracked.get('orientation', []), pose['orientation']))) > .99999
                        and max((abs(a-b) for a, b in zip(tracked.get('position', []), pose['position'])), default=1.) < .001):
                    return
                time.sleep(.025)
            raise BotFault('Cinematic head motion was not published by the native camera')

        if policy.get('wait_for_look_lesson') is True:
            # Watch real progression after START GAME; do not start a pose
            # challenge halfway through an adjacent failure/retry demo.
            while time.monotonic() < deadline:
                state = live.observe(native=True)
                native = state.get('native') or {}
                if native.get('mission') != 10010 or native.get('story') != 0:
                    raise BotFault('Hospital look observation lost its fresh campaign owner')
                if (native.get('sequence') == 'Seq_Game_FewDaysLater1'
                        and state.get('demo_interactive_look') is True):
                    break
                time.sleep(.1)
            else:
                raise BotFault('Hospital look lesson was not reached before its observation deadline')

        if policy.get('hospital_look') is not True:
            capture('cinematic-forward')
            for degrees in (26, -26):
                target = {'position': head['position'], 'orientation': mul(head['orientation'],
                    [0., math.sin(math.radians(degrees)/2), 0., math.cos(math.radians(degrees)/2)])}
                set_head(target)
                time.sleep(.35)
                capture('cinematic-look-' + str(degrees))
            target = {'position': [head['position'][0] + .12, *head['position'][1:]],
                      'orientation': head['orientation']}
            set_head(target)
            time.sleep(.35)
            capture('cinematic-physical-lean')
            set_head(head)
        state = capture('cinematic-head-restored')
        if policy.get('hospital_look') is True:
            if (state.get('native', {}).get('mission') != 10010
                    or state.get('native', {}).get('sequence') not in (
                        'Seq_Game_FewDaysLater0', 'Seq_Game_FewDaysLater1')
                    or state.get('demo_interactive_look') is not True):
                raise BotFault('Physical look lesson requires the current native hospital lesson')
            initial_sequence = state['native']['sequence']
            for degrees in range(8, 33, 8):
                target = {'position': head['position'], 'orientation': mul(head['orientation'],
                    [math.sin(math.radians(degrees)/2), 0., 0., math.cos(math.radians(degrees)/2)])}
                set_head(target)
                time.sleep(.08)
            capture('hospital-head-looking-up')
            samples = []
            until = min(deadline, time.monotonic()+3.)
            while time.monotonic() < until:
                state = observe()
                samples.append({'native': state.get('native'), 'controls': state.get('controls')})
                time.sleep(.1)
            result['head_look_samples'] = samples
            result['lesson_progressed'] = (state['native']['sequence'] in (
                'Seq_Demo_FewDaysLater2', 'Seq_Game_FewDaysLater2')
                or state['native'].get('look_lesson_succeeded') is True)
            result['phases'].append({'label': 'hospital-head-look-outcome', 'state': state,
                                    'eyes': live.capture('hospital-head-look-outcome')})
            set_head(head)
        if not state.get('demo_interactive_look') and state.get('rendered', {}).get('cinematic_free_move'):
            axis = next((a for a in live.bindings.get('axes', []) if a.get('name') == 'axes.move'), None)
            if not axis or axis.get('source') not in ('left_stick', 'right_stick'):
                raise BotFault('Cinematic viewer movement lacks its effective personal axis')
            arguments = {'hand': axis['source'].split('_')[0], 'component': 'Thumbstick',
                         'sub_component': 'Y', 'value': .55}
            live.input([arguments], 1.2, lease_seconds=2.)
            try:
                end = time.monotonic() + .8
                while time.monotonic() < end:
                    observe()
                    time.sleep(.025)
            finally:
                live.release()
            state = capture('cinematic-viewer-moved')
            if max(abs(v) for v in state.get('controls', {}).get('native_axes', [1.])) != 0:
                raise BotFault('Cinematic viewer movement leaked to native actor controls')
            result['viewer_offset'] = state.get('rendered', {}).get('cinematic_free_offset')
        result['status'] = 'observed_cinematic_viewing'
        result['visual_acceptance'] = 'pending'
    return result
