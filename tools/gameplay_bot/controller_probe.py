"""Bounded right-hand inactivity and raw validity-loss reproduction in retail VR."""
import copy
import time
from .core import BotFault, atomic_json
from .session import preserved_tracking_poses, _controller_pose
from .size_probe import apply_controls


def inspect(live, seconds, policy):
    deadline=time.monotonic()+min(seconds,180.)
    result={'status':'running','phases':[], 'scope':policy,
            'physical_quest_inactivity':'unproven','visual_acceptance':'pending_both_eye_review'}
    previous={r['name']:r['value'] for r in live.bindings['settings']
              if r['name']=='settings.seated_mode'}
    if len(previous)!=1:raise BotFault('Seated preference unavailable')
    def observe():
        state=live.observe(native=True)
        if (time.monotonic()>deadline or state['scene'] not in ('cabin','gameplay')
            or state['native']['mission']!=policy['mission']
            or any(state.get(k) for k in ('menu','loading','demo','gamepad'))):
            raise BotFault('Controller probe lost its selected native owner')
        return state
    def record(label, expected_mask):
        state=observe();frame=state.get('rendered') or {}
        row={'label':label,'state':state,'eyes':live.capture(label)}
        result['phases'].append(row)
        atomic_json(live.events.output/'controller-probe.json',result)
        if state.get('tracking_fault_mask')!=expected_mask:
            raise BotFault('Tracking lease changed before observation: '+label)
        if (not state.get('camera_active') or state.get('camera_suspended')
            or not 0<=state['now_ms']-frame.get('sample_ms',-10000)<200):
            raise BotFault('First-person frame stopped publishing: '+label)
        if expected_mask&4 and frame.get('raw_right_grip_tracked') is not False:
            raise BotFault('Right grip loss did not reach the rendered frame')
        if expected_mask&8 and frame.get('raw_right_aim_tracked') is not False:
            raise BotFault('Right aim loss did not reach the rendered frame')
        return state
    try:
        live.release();result['before']=observe();result['preferences_before']=previous
        if 'standing' in policy:
            apply_controls(live,{'settings.seated_mode':0 if policy['standing'] else 1})
        with preserved_tracking_poses(live):
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            hands=[_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
            for mask in policy.get('masks',[0,4,8,12]):
                live.release()
                if mask:live.execute({'op':'tracking_loss','mask':mask,'milliseconds':8000})
                start=time.monotonic()
                for index in range(4):
                    target=start+index*2.35
                    if time.monotonic()<target:time.sleep(target-time.monotonic())
                    moved=copy.deepcopy(head);moved['position'][0]+=.04 if index%2 else -.04
                    live.call('set_head_pose',{'base_space':'local',**moved})
                    # HMD API transports controllers; restore the right exactly,
                    # then move only the left. The right stays stationary.
                    for saved in hands:
                        pose=copy.deepcopy(saved)
                        if pose['hand']=='left':pose['position'][1]+=.12 if index%2 else -.08
                        live.call('set_controller_pose',pose)
                    time.sleep(.15)
                    record('controller-mask-%s-phase-%s'%(mask,index),mask)
                if not mask:
                    target=start+6.5
                    if time.monotonic()<target:time.sleep(target-time.monotonic())
                    record('controller-idle-over-six-seconds',0)
                else:
                    # Wait for the real bounded lease to expire. No forced
                    # camera toggle/recenter is allowed to manufacture recovery.
                    until=time.monotonic()+4.
                    while observe().get('tracking_fault_mask'):
                        if time.monotonic()>until:raise BotFault('Tracking lease did not expire')
                        time.sleep(.1)
                    time.sleep(.4);record('controller-recovered-'+str(mask),0)
            result['status']='observed_camera_continuity'
    except Exception as error:
        result.update(status='failed',error=str(error))
        raise
    finally:
        live.release()
        response,timing=live.native.read('diagnostics:controller-tracking-loss:off')
        result['fault_cleanup']=response
        if 'standing' in policy:apply_controls(live,previous)
        atomic_json(live.events.output/'controller-probe.json',result)
    return result
