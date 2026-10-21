"""Prone stick-turn regression using effective VR bindings and native skin."""
import time
from .core import BotFault, atomic_json
from .session import preserved_tracking_poses, bring_hands_into_view, _controller_pose
from .cabin_workbench import compose
from .combat import pointing


def inspect(live, seconds=180, policy=None):
    if (policy or {}).get('weapon') is True:
        return inspect_weapon(live, seconds)
    result = {'status': 'running', 'phases': [], 'recenter_count': 0}
    deadline = time.monotonic() + min(seconds, 180)

    def observe():
        s = live.observe(native=True)
        if time.monotonic() > deadline or s['scene'] != 'gameplay' or s.get('menu'):
            raise BotFault('Prone-turn lost ordinary gameplay or deadline')
        return s

    def posture(flag, duration):
        live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': duration})
        live.release()
        end = min(deadline, time.monotonic() + 8)
        while time.monotonic() < end:
            if observe()['native'].get(flag):
                time.sleep(1.2)
                return
        raise BotFault('Prone-turn missing posture ' + flag)

    def sample(label):
        s = observe()
        row = {'label': label, 'state': s, 'eyes': live.capture('prone-turn-' + label)}
        result['phases'].append(row)
        atomic_json(live.events.output / 'prone-turn.json', result)

    with preserved_tracking_poses(live):
        try:
            live.release()
            s = observe()
            if s['native'].get('status_CRAWL'):
                posture('status_SQUAT', .15)
            if observe()['native'].get('status_SQUAT'):
                posture('status_STAND', .15)
            bring_hands_into_view(live, observe()['native'])
            head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
            hands = [_controller_pose(live, h, k, 'local') for h in ('left', 'right') for k in ('grip', 'aim')]

            def view(direction=None):
                target = head if direction is None else compose(head, {'position': [0, 0, 0], 'orientation': pointing(direction)})
                live.call('set_head_pose', {'base_space': 'local', **target})
                for hand in hands:
                    live.call('set_controller_pose', hand)
                time.sleep(.2)

            sample('standing-before')
            posture('status_CRAWL', .85)
            sample('prone-before')
            # Look back/down at the prone torso, independently of stick yaw.
            view([.75, -.55, -.25])
            sample('prone-look')
            for direction in ('right', 'left'):
                for i in range(12):
                    live.execute({'op': 'action', 'name': 'turn.' + direction, 'seconds': .3})
                    live.release()
                    time.sleep(.2)
                    sample(direction + '-' + str(i + 1))
            view()
            sample('prone-forward-restored')
            posture('status_SQUAT', .15)
            posture('status_STAND', .15)
            sample('standing-restored')
            result['status'] = 'completed'
        finally:
            live.release()
            atomic_json(live.events.output / 'prone-turn.json', result)
    return result


def inspect_weapon(live, seconds):
    """Exercise the currently loaded rifle in prone; never replace inventory."""
    from . import arsenal
    from .showcase import reload_outcome
    result = {'status': 'running'}
    with preserved_tracking_poses(live):
        try:
            live.release()
            start = live.observe(native=True)
            if start['scene'] != 'gameplay' or not start['native'].get('status_STAND'):
                raise BotFault('Prone weapon check requires an ordinary standing start')
            plan = {'world': {k: start['native'][k] for k in ('mission', 'location', 'sequence')},
                    'scope': 'Existing primary rifle in prone after stick turning',
                    'catalog_queue': [], 'policy': {'burst_seconds': .2},
                    'cards': [{'case_id': 'prone-existing-rifle', 'category': 'primary',
                               'card': 'left', 'expected_slot': 'PRIMARY_HIP'}]}
            live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': .85})
            live.release()
            time.sleep(1.5)
            if not live.observe(native=True)['native'].get('status_CRAWL'):
                raise BotFault('Prone weapon posture not observed')
            result['weapon'] = arsenal.run_sweep(live, plan, seconds=min(seconds, 120), max_cards=1)
            before, anchor = arsenal.guard(live, plan)
            ammo = arsenal.inventory(before['native'])
            live.execute({'op': 'action', 'name': 'gameplay.reload', **arsenal.action_guard(before, plan)})
            end = time.monotonic() + 10
            while True:
                after, _ = arsenal.guard(live, plan, anchor)
                result['reload'] = reload_outcome(ammo, arsenal.inventory(after['native']), 'PRIMARY_HIP')
                if result['reload']['status'] == 'observed_pass' or time.monotonic() >= end:
                    break
                time.sleep(.1)
            result['reload']['eyes'] = live.capture('prone-rifle-reloaded')
            result['status'] = 'completed'
        finally:
            live.release()
            state = live.observe(native=True)
            if state['scene'] == 'gameplay' and state['native'].get('status_CRAWL'):
                live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': .15})
                live.release(); time.sleep(1.2)
            state = live.observe(native=True)
            if state['scene'] == 'gameplay' and state['native'].get('status_SQUAT'):
                live.execute({'op': 'action', 'name': 'gameplay.stance', 'seconds': .15})
                live.release(); time.sleep(1.2)
            result['exit'] = live.observe(native=True)
            atomic_json(live.events.output / 'prone-weapon.json', result)
    return result
