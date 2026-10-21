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
    if (policy or {}).get('right_wrist_isolation') is True:
        from .hand_isolation import inspect
        return inspect(live,seconds,policy)
    if (policy or {}).get('cover_inspect') is True:
        from .live import rotate
        live.release()
        baseline=live.observe(native=True)
        if not matches(baseline['native'],policy['world']) or not baseline['native']['status_BEHIND']:
            raise BotFault('Cover inspection requires current native wall cover')
        result={'status':'running','phases':[],'physical_headset_acceptance':False}
        with preserved_tracking_poses(live):
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            hands=[_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
            for label,offset in [('center',[0,0,0]),('lean-right',[.22,0,0]),
                                 ('peek-up',[0,.25,0]),('return',[0,0,0])]:
                delta=rotate(head['orientation'],offset)
                live.call('set_head_pose',{'base_space':'local',**head,
                    'position':[a+b for a,b in zip(head['position'],delta)]})
                for hand in hands:live.call('set_controller_pose',hand)
                time.sleep(.3)
                state=live.observe(native=True)
                if not matches(state['native'],policy['world']) or not state['camera_active']:
                    raise BotFault('Cover view owner changed')
                row={'label':label,'state':state,'eyes':live.capture('cover-'+label)}
                if label==policy.get('cover_aim_at','peek-up'):
                    row['aim']=live.execute({'op':'action','name':'gameplay.ready_weapon','seconds':.7,
                        'static_grip_capture':True,'capture_while_held':'cover-aim',
                        'while_held':{'camera_active':True,'rendered.native_firearm':True}})
                    live.release()
                result['phases'].append(row)
                atomic_json(live.events.output/'cover-inspect.json',result)
        result['status']='observed_cover_head_and_aim'
        atomic_json(live.events.output/'cover-inspect.json',result)
        return result
    if (policy or {}).get('cover_exit') is True or (policy or {}).get('cover_enter') is True:
        from .locomotion import _axis_source, _validate_xr_scene_state
        live.release()
        before=live.observe(native=True)
        _validate_xr_scene_state(before)
        entering=policy.get('cover_enter') is True
        if not matches(before['native'],policy['world']) or before['native']['status_BEHIND'] is entering:
            raise BotFault('Native cover entry/exit prerequisite changed')
        source=_axis_source(live.bindings,'axes.move')
        direction=policy.get('direction','up' if entering else 'down')
        if direction not in ('up','down','left','right'):raise BotFault('Invalid cover movement')
        axis='Y' if direction in ('up','down') else 'X'
        value=.8 if direction in ('up','right') else -.8
        samples=[]
        try:
            live.input([{'hand':source.split('_')[0],'component':'Thumbstick',
                'sub_component':axis,'value':value}],1.5,lease_seconds=1.6)
            until=time.monotonic()+1.5
            while time.monotonic()<until:
                state=live.observe(native=True)
                _validate_xr_scene_state(state)
                samples.append(state)
                if state['native']['status_BEHIND'] is entering:break
                time.sleep(.03)
        finally:live.release()
        after=live.observe(native=True)
        result={'status':('observed_cover_entry' if entering else 'observed_cover_exit')
                if after['native']['status_BEHIND'] is entering else 'cover_state_unchanged',
            'before':before,'samples':samples,'after':after,'eyes':live.capture('cover-transition')}
        atomic_json(live.events.output/'cover-exit.json',result)
        return result
    if (policy or {}).get('navigate') is True:
        import json
        from pathlib import Path
        from .navigation import NavigationAtlas
        from .route import navigate
        from .perception import read_enemy_snapshot
        root=Path(__file__).resolve().parents[2]
        observed=live.observe(native=True)
        if not matches(observed['native'],policy['world']):
            raise BotFault('Navigation native entry changed')
        manifest=root/policy['manifest']
        atlas=NavigationAtlas(json.loads(manifest.read_text(encoding='utf-8-sig')),
            json.loads((root/'tools/gameplay_bot/catalogs/native-worlds.json').read_text()),base=manifest.parent)
        nav=atlas.select(observed['native']['location'])
        lua=(root/'tools/navigation-state.lua').read_text()
        if policy.get('cqc_target'):
            from .cqc import run_named_cqc
            approach_range=float(policy.get('cqc_approach_range',1.65))
            if not .75<=approach_range<=1.65:raise BotFault('CQC approach range is outside bounded proximity')
            until=time.monotonic()+min(seconds,90.)
            approaches=[]
            for attempt in range(5):
                from .combat import admit
                state,_=admit(live)
                if not matches(state['native'],policy['world']):raise BotFault('CQC world changed')
                enemies=read_enemy_snapshot(live.native.read,lua)
                actor=next(x for x in enemies['soldiers'] if x['name']==policy['cqc_target'])
                if actor['life']!='0':raise BotFault('CQC target is no longer active')
                n=state['native']; p=[n['player_'+k] for k in 'xyz']
                if math.hypot(p[0]-actor['position'][0],p[2]-actor['position'][2])<=approach_range:
                    return run_named_cqc(live,{'native':policy['world'],
                        'target':policy['cqc_target'],'pulses':policy.get('cqc_pulses',3),
                        'pulse_seconds':policy.get('cqc_pulse_seconds',.2),
                        'contextual':policy.get('contextual',False)},
                        enemy_script=lua,output=live.events.output)
                remaining=until-time.monotonic()
                if remaining<=0:raise BotFault('CQC approach deadline')
                step=navigate(live,nav,actor['position'],output=live.events.output,
                    enemy_reader=lambda:read_enemy_snapshot(live.native.read,lua),mode='run',
                    max_seconds=min(remaining,8.),expected_location=n['location'],
                    interrupt_on_danger=True,known_battle=True,arrival_radius=approach_range-.1,recenter=False,
                    capture=False,avoid_threats=False)
                approaches.append(step)
                atomic_json(live.events.output/'cqc-approaches.json',approaches)
                if step['status']=='needs_battle_response':return step
                if step['status']=='failed' and step.get('error')!='Native navigation deadline reached':return step
            raise BotFault('Moving CQC guard not reached within bounded pursuit')
        result=navigate(live,nav,policy['goal'],output=live.events.output,
            enemy_reader=lambda:read_enemy_snapshot(live.native.read,lua),mode=policy.get('mode','walk'),
            max_seconds=min(seconds,90.),expected_location=observed['native']['location'],
            interrupt_on_danger=True,known_battle=policy.get('known_battle',False),
            arrival_radius=policy.get('arrival_radius',3.5),recenter=False)
        return result
    if (policy or {}).get('acc_menu_probe') is True:
        from .acc_menu_probe import inspect
        return inspect(live,seconds,policy)
    if (policy or {}).get('weapon_probe') is True:
        from .weapon_probe import inspect
        return inspect(live,seconds,policy)
    if (policy or {}).get('controller_probe') is True:
        from .controller_probe import inspect
        return inspect(live,seconds,policy)
    if (policy or {}).get('size_probe') is True:
        from .size_probe import inspect
        return inspect(live, seconds, policy)
    if (policy or {}).get('traversal') is True:
        from .traversal_probe import inspect
        return inspect(live, seconds, policy)
    if (policy or {}).get('camera_grab') is True:
        from .body_gear import inspect_camera_grab
        return inspect_camera_grab(live, seconds)
    if (policy or {}).get('gear') is True:
        from .body_gear import inspect
        return inspect(live, seconds)
    if (policy or {}).get('cabin') is True:
        from .cabin_body import inspect
        return inspect(live,seconds)
    if (policy or {}).get('prone_turn') is True:
        from .prone_turn import inspect
        return inspect(live,seconds,policy)
    if (policy or {}).get("roomscale") is True:
        from .roomscale import inspect
        return inspect(live,seconds,policy)
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
