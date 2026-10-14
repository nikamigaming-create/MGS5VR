"""One bounded retail posture/IK/camera inspection; final-eye pixels stay native."""
import math
import time
from .core import BotFault,atomic_json,matches
from .combat import pointing
from .session import preserved_tracking_poses,bring_hands_into_view,_controller_pose
from .cabin_workbench import compose,shifted
from .locomotion import move_local

def inspect_mounted_body(live, seconds, policy):
    deadline = time.monotonic() + min(seconds, 90.)
    result = {'status': 'running', 'phases': [], 'physical_headset_acceptance': 'not_run',
              'visual_acceptance': 'pending'}

    def observe():
        s = live.observe(native=True)
        n = s.get('native') or {}
        if (time.monotonic() > deadline or s['scene'] != 'gameplay'
                or any(s.get(k) is not False for k in ('pause', 'idroid', 'loading', 'demo', 'gamepad'))
                or not (n.get('status_ON_HORSE') is True or n.get('status_ON_VEHICLE') is True)
                or not matches(n, policy.get('native', {}))):
            raise BotFault('Mounted body inspection lost its native seat/ride owner')
        return s

    with preserved_tracking_poses(live):
        live.release()
        observe()
        head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
        hands = [_controller_pose(live, hand, kind, 'local')
                 for hand in ('left', 'right') for kind in ('grip', 'aim')]
        for label, direction, offset in (
                ('mounted-body-down', [0., -1., -.35], [0., 0., 0.]),
                ('mounted-body-left', [-.6, -.75, -.35], [0., 0., 0.]),
                ('mounted-body-right', [.6, -.75, -.35], [0., 0., 0.]),
                ('mounted-body-small-lean', [0., -.8, -.35], [.08, 0., 0.])):
            observe()
            target = shifted(head, offset)
            target['orientation'] = compose(head, {'position': [0., 0., 0.],
                                                   'orientation': pointing(direction)})['orientation']
            live.call('set_head_pose', {'base_space': 'local', **target})
            for hand in hands:
                live.call('set_controller_pose', hand)
            end = min(deadline, time.monotonic() + 6.)
            while True:
                state = observe()
                f = state.get('rendered') or {}
                h = f.get('head_pose') or {}
                if (f.get('left_palm_tracked') is True and f.get('right_palm_tracked') is True
                        and 0 <= state['now_ms'] - f.get('sample_ms', 0) <= 150
                        and abs(sum(a*b for a,b in zip(h.get('orientation', []), target['orientation']))) > .99999
                        and max((abs(a-b) for a,b in zip(h.get('position', []), target['position'])), default=1.) < .001):
                    break
                if time.monotonic() >= end:
                    raise BotFault('Mounted head and hands did not publish together')
                time.sleep(.06)
            eyes = live.capture(label)
            result['phases'].append({'label': label, 'state': state, 'eyes': eyes})
            atomic_json(live.events.output/'body-progress.json', result)
    result['status'] = 'observed_mounted_body_presentation'
    result['full_body_admitted_every_phase'] = all(
        (p['state'].get('rendered') or {}).get('full_body', {}).get('valid') is True
        for p in result['phases'])
    return result


def inspect_body(live,seconds=120.,policy=None):
    if (policy or {}).get('mounted') is True:
        return inspect_mounted_body(live, seconds, policy)
    deadline=time.monotonic()+min(seconds,180.)
    result={'status':'running','phases':[],'physical_headset_acceptance':'not_run'}
    def observe(native=False):
        if time.monotonic()>deadline:raise BotFault('Body inspection deadline')
        s=live.observe(native=native)
        if s['scene']!='gameplay' or s.get('loading') or s.get('demo') or s.get('menu'):
            raise BotFault('Body inspection lost ordinary native field ownership')
        return s
    def wait(predicate,label,timeout=8.,native=False):
        end=min(deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            s=observe(native)
            if predicate(s):
                result['phases'].append({'label':label,'state':s});return s
            time.sleep(.06)
        raise BotFault('Body inspection missing '+label)
    def capture(label):
        s=wait(lambda s:s.get('camera_active') and not s.get('camera_suspended')
            and s.get('rendered',{}).get('full_body',{}).get('valid')
            and 0<=s['now_ms']-s['rendered']['sample_ms']<=150
            and s['rendered'].get('left_palm_tracked') and s['rendered'].get('right_palm_tracked'),label+'-skin')
        body=s['rendered']['full_body']
        if body.get('floor_queries')!=2 or min(body.get('floor_clearance',[0,0]))<.075:
            raise BotFault('Body feet lacked complete native floor clearance')
        eyes=live.capture(label)
        result['phases'].append({'label':label,'state':s,'eyes':eyes});atomic_json(live.events.output/'body-progress.json',result)
    def posture(name,flag,seconds):
        live.execute({'op':'action','name':name,'seconds':seconds});live.release()
        wait(lambda s:s.get('native',{}).get(flag) is True,flag,native=True)
        time.sleep(1.2)
    with preserved_tracking_poses(live):
        try:
            live.release();start=observe(native=True)
            if start.get('native',{}).get('status_CRAWL'):posture('gameplay.stance','status_SQUAT',.15)
            if observe(native=True).get('native',{}).get('status_SQUAT'):posture('gameplay.stance','status_STAND',.15)
            bring_hands_into_view(live,observe(native=True)['native'])
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            hands=[_controller_pose(live,hand,kind,'local')
                for hand in ('left','right') for kind in ('grip','aim')]
            def set_head(view):
                live.call('set_head_pose',{'base_space':'local',**view})
                # The simulator moves controllers with its head command.
                # A real player looking down does not also move both hands.
                for pose in hands:live.call('set_controller_pose',pose)
            def look(direction,offset=None):
                view=shifted(head,offset or [0.,0.,0.]);view['orientation']=compose(head,{'position':[0.,0.,0.],'orientation':pointing(direction)})['orientation']
                set_head(view)
                def joined(s):
                    f=s.get('rendered') or {};pose=f.get('head_pose') or {}
                    return (s.get('camera_active') and not s.get('camera_suspended')
                        and 0<=s['now_ms']-f.get('sample_ms',-1000)<=150
                        and math.dist(pose.get('position',[999.]*3),view['position'])<=.001
                        and abs(sum(a*b for a,b in zip(pose.get('orientation',[]),view['orientation'])))>=.999999)
                wait(joined,'requested-head-pose-published')
            capture('standing-world-and-hands')
            look([0.,-1.,-.35]);capture('standing-full-body-and-boots')
            look([.8,-.65,-.3],[.1,0.,0.]);capture('standing-body-side-lean')
            set_head(head)
            posture('gameplay.stance','status_SQUAT',.15);look([0.,-.7,-.45]);capture('crouched-body-and-floor')
            set_head(head)
            posture('gameplay.stance','status_CRAWL',.85);capture('prone-hands-world-and-floor')
            look([.75,-.55,-.25]);capture('prone-body-and-floor')
            set_head(head)
            posture('gameplay.stance','status_SQUAT',.15);posture('gameplay.stance','status_STAND',.15)
            look([0.,-1.,-.35]);capture('standing-body-restored')
            set_head(head)
            samples=[]
            old_observer=getattr(live,'state_observer',None)
            def sample(s):
                if old_observer:old_observer(s)
                if s.get('scene')=='gameplay' and s.get('rendered'):
                    samples.append({'time':s['now_ms'],'sticks':s.get('controls',{}).get('sticks'),'rendered':s['rendered']})
            live.state_observer=sample
            try:result['walk']=move_local(live,target_distance=.35)
            finally:live.state_observer=old_observer
            result['motion_samples']=samples
            capture('walk-released-body-and-world')
            result['status']='observed_body_posture_and_motion'
            return result
        finally:
            live.release();atomic_json(live.events.output/'body-progress.json',result)
