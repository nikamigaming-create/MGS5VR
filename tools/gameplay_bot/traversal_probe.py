"""Bounded observed native walking; no actor transform or collision edits."""
import time
from contextlib import ExitStack
from .core import BotFault, atomic_json
from .locomotion import move_local


def inspect(live, seconds, policy):
    deadline = time.monotonic() + min(seconds, 180.)
    result = {'status': 'running', 'segments': [], 'scope': policy,
              'headset_acceptance': 'not_run'}
    live.release()
    start = live.observe(native=True)
    mission = policy.get('mission', 30050)
    if start['scene'] != 'gameplay' or start['native']['mission'] != mission:
        raise BotFault('Traversal requires the selected live native mission')
    result['before'] = start
    names = {'full_body':'settings.full_body','seated':'settings.seated_mode'}
    changes = {setting:int(policy[key]) for key,setting in names.items() if key in policy}
    previous = {r['name']:r['value'] for r in live.bindings['settings'] if r['name'] in changes}
    poses = ExitStack()
    try:
        if changes:
            from .size_probe import apply_controls
            if len(previous)!=len(changes):raise BotFault('Traversal preferences unavailable')
            apply_controls(live, changes)
            admitted=live.observe(native=True)
            if 'full_body' in policy and admitted['rendered']['full_body_requested'] != bool(policy['full_body']):
                raise BotFault('Requested body setting did not reach the live rig')
            result['after_settings']=admitted
        if 'head_offset' in policy:
            from .session import preserved_tracking_poses, _controller_pose
            poses.enter_context(preserved_tracking_poses(live))
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            hands=[_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
            offset=policy['head_offset']
            if len(offset)!=3 or any(abs(x)>.6 for x in offset):raise BotFault('Invalid bounded head offset')
            head['position']=[a+b for a,b in zip(head['position'],offset)]
            live.call('set_head_pose',{'base_space':'local',**head})
            for hand in hands:live.call('set_controller_pose',hand)
            time.sleep(.5)
            result['after_head_offset']=live.observe(native=True)
        for turn in policy.get('turns', []):
            if turn not in ('left','right'):raise BotFault('Invalid turn')
            live.execute({'op':'action','name':'turn.'+turn,'seconds':.15});live.release();time.sleep(.5)
            result.setdefault('turns', []).append(live.observe(native=True))
        for direction in policy.get('directions', []):
            if time.monotonic() > deadline:
                raise BotFault('Traversal wall-clock deadline')
            state = live.observe(native=True)
            if state['native']['mission'] != mission:
                raise BotFault('Traversal mission changed')
            walked = move_local(live, target_distance=policy.get('target_distance',.55), max_displacement=1.,
                               brake_magnitude=.35, pulse_seconds=policy.get('pulse_seconds',.1), direction=direction)
            state = live.observe(native=True)
            row = {'direction': direction, 'walk': walked, 'state': state}
            result['segments'].append(row)
            if policy.get('capture_each') or len(result['segments']) % 4 == 0:
                row['eyes'] = live.capture('traversal-' + str(len(result['segments'])))
            atomic_json(live.events.output/'traversal.json', result)
            if not walked.get('target_reached', False):
                result['status'] = 'stopped_for_review'
                break
        else:
            result['status'] = 'segments_completed'
        result['after'] = live.observe(native=True)
        result['eyes'] = live.capture('traversal-end')
        return result
    except BotFault as error:
        result['status'] = 'stopped_for_review'
        result['error'] = str(error)
        result['after'] = live.observe(native=True)
        result['eyes'] = live.capture('traversal-stopped')
        return result
    finally:
        live.release()
        try:
            poses.close()
        finally:
            if changes:
                from .size_probe import apply_controls
                apply_controls(live, previous)
                result['preferences_restored']=True
            atomic_json(live.events.output/'traversal.json', result)
