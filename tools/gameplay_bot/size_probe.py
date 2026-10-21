"""Repeat head-height/recenter measurements on a stationary native actor."""
import json
import math
import subprocess
import time
from .core import BotFault, atomic_json
from .live import ROOT
from .session import preserved_tracking_poses, _controller_pose
from .combat import pointing


def apply_controls(live, changes):
    request = live.events.output/'probe-settings-request.json'
    def bridge(method, **values):
        atomic_json(request, {'method': method, 'gameExe': str(live.game/'mgsvtpp.exe'), **values})
        return json.loads(subprocess.check_output(['powershell.exe', '-NoProfile', '-NonInteractive',
            '-File', str(ROOT/'tools/launcher-bridge.ps1'), '-CheckerPath',
            str(ROOT/'build/dev/mgs5vr_controls.exe'), '-RequestPath', str(request)],
            text=True, encoding='utf-8-sig'))
    loaded = bridge('load')
    response = bridge('saveControls', revision=loaded['controlsRevision'],
                      changes={k: str(v) for k,v in changes.items()})
    if not response.get('ok', True):raise BotFault('Probe preferences rejected')
    # Runtime requires two stable file observations at 750 ms intervals.
    time.sleep(2.)


def inspect(live, seconds, policy):
    deadline = time.monotonic() + min(seconds, 180.)
    result = {'status': 'running', 'phases': [], 'scope': policy,
              'headset_acceptance': 'not_run'}
    previous = {r['name']: r['value'] for r in live.bindings['settings']
                if r['name'] in ('settings.seated_mode', 'settings.full_body')}
    if len(previous) != 2:raise BotFault('Body preferences unavailable')

    def settings(changes):
        apply_controls(live, changes)

    def observe():
        s = live.observe(native=True)
        if (time.monotonic() > deadline or s['scene'] != 'gameplay'
                or s['native']['mission'] != policy.get('mission', 30050)
                or any(s.get(k) for k in ('loading', 'menu', 'demo', 'gamepad'))):
            raise BotFault('Size inspection lost its native field owner')
        return s

    try:
        live.release();result['before'] = observe();result['preferences_before'] = previous
        if not result['before']['native'].get('status_STAND'):
            raise BotFault('Height sequence requires native standing at entry')
        with preserved_tracking_poses(live):
            standing = policy.get('standing', True)
            settings({'settings.seated_mode': 0 if standing else 1, 'settings.full_body': 1})
            time.sleep(2.)
            s = observe();f = s['rendered']
            until = min(deadline, time.monotonic()+5.)
            while standing and not f['physical_floor_tracked'] and time.monotonic()<until:
                time.sleep(.15);s=observe();f=s['rendered']
            result['after_settings'] = s
            if standing and not f['physical_floor_tracked']:
                raise BotFault('Standing mode has no runtime floor; no standing pass is possible')
            head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
            hands = [_controller_pose(live,h,k,'local') for h in ('left','right') for k in ('grip','aim')]
            base_y = f['physical_floor_y'] + 1.65 if standing else head['position'][1]
            # Keep hands fixed while inspecting feet. Every native actor pose
            # remains produced by the game; only simulated HMD tracking moves.
            cases = [('neutral',0,True),('raised',.25,False),
                    ('return',0,False),('high',.6,False),('return-after-high',0,False),
                    ('recenter',0,True),('raised-again',.25,False),('final-return',0,True)]
            if policy.get('transitions'):
                cases = [('neutral',0,True),('lowered',-.5,False),('lowered-recenter',-.5,True),
                         ('rise-after-low-recenter',0,False),('home-recenter',0,True),
                         ('lean-right',0,False),('lean-recenter',0,True),('lean-return',0,False),
                         ('home-again',0,True),('native-crouch',0,False),('native-prone',0,False),
                         ('native-stand',0,False),('final-return',0,True)]
            if policy.get('rotations'):
                cases=[('neutral',0,True)]+[('yaw-'+str(a),0,False) for a in (45,90,135,179,181,225,270,315,360)]+[('final-return',0,True)]
            for label, delta, recenter in cases:
                if label.startswith('native-'):
                    actions = {'native-crouch': [('status_SQUAT',.15)],
                               'native-prone': [('status_CRAWL',.85)],
                               'native-stand': [('status_SQUAT',.15),('status_STAND',.15)]}
                    for flag, duration in actions[label]:
                        live.execute({'op':'action','name':'gameplay.stance','seconds':duration});live.release()
                        until = min(deadline,time.monotonic()+6.)
                        while not observe()['native'].get(flag):
                            if time.monotonic() >= until:raise BotFault('Missing native posture '+flag)
                            time.sleep(.1)
                lateral = .5 if label in ('lean-right','lean-recenter') else 0.
                yaw=math.radians(float(label[4:])) if label.startswith('yaw-') else 0.
                target = {'position': [head['position'][0]+lateral,base_y+delta,head['position'][2]],
                          'orientation': pointing([-.15*math.sin(yaw),-1.,-.15*math.cos(yaw)])}
                live.call('set_head_pose', {'base_space':'local', **target})
                for hand in hands:live.call('set_controller_pose', hand)
                if recenter:live.execute({'op':'action','name':'system.recenter','seconds':.15});live.release()
                time.sleep(1.2)
                state = observe();r = state['rendered'];root = r['native_body_source_root']['position']
                body = r['full_body'];eye = r['body_eye']['position'];pelvis = r['body_pelvis']['position']
                row = {'label':label, 'state':state, 'metrics':{
                    'eye_above_root':eye[1]-root[1], 'pelvis_above_root':pelvis[1]-root[1],
                    'pelvis_to_feet':[math.dist(pelvis,r[k]['position']) for k in ('left_foot','right_foot')],
                    'floor_clearance':body['floor_clearance'], 'body_valid':body['valid']}}
                row['metrics']['floor_queries'] = body['floor_queries']
                row['metrics']['leg_scale'] = body.get('leg_scale')
                row['metrics']['leg_calibration'] = body.get('leg_calibration')
                row['metrics']['eye_root_horizontal'] = math.hypot(eye[0]-root[0],eye[2]-root[2])
                result['phases'].append(row)
                if label.startswith('yaw-') or label in ('neutral','high','return-after-high','final-return','lowered-recenter',
                             'rise-after-low-recenter','lean-return','native-crouch','native-prone','native-stand'):
                    row['eyes'] = live.capture('size-'+label)
                atomic_json(live.events.output/'size-probe.json',result)
                if policy.get('require_grounded'):
                    expected=[max(.09,v) for v in body['native_floor_clearance']]
                    if (not body['valid'] or body['floor_queries']!=2
                            or any(abs(a-b)>.02 for a,b in zip(body['floor_clearance'],expected))):
                        result['status']='failed_grounding';result['failed_phase']=label
                        result['failure_eyes']=live.capture('grounding-failed-'+label)
                        raise BotFault('Grounded body regression failed at '+label)
            if policy.get('require_grounded'):
                baseline=result['phases'][0]['metrics']
                for phase in result['phases']:
                    if phase['label'] in ('return-after-high','rise-after-low-recenter','final-return'):
                        metrics=phase['metrics']
                        if abs(metrics['eye_above_root']-baseline['eye_above_root'])>.025:
                            result['status']='failed_height_recovery'
                            raise BotFault('Body height did not recover: '+phase['label'])
                        if abs(metrics['leg_calibration']-baseline['leg_calibration'])>.01:
                            result['status']='failed_size_recovery'
                            raise BotFault('Leg calibration grew: '+phase['label'])
            result['status'] = 'height_sequence_observed'
        return result
    except Exception as error:
        if result['status']=='running':result['status']='failed'
        result['error']=str(error)
        raise
    finally:
        live.release()
        try:
            if policy.get('transitions') and result.get('before',{}).get('native',{}).get('status_STAND'):
                # A failed posture phase must not strand a formerly standing
                # player prone. Only act while this same ordinary field owns input.
                for _ in range(2):
                    state=live.observe(native=True)
                    if (state.get('scene')!='gameplay' or state.get('menu')
                            or state.get('native',{}).get('mission')!=policy.get('mission',30050)):
                        break
                    if state['native'].get('status_STAND'):break
                    if not (state['native'].get('status_CRAWL') or state['native'].get('status_SQUAT')):break
                    live.execute({'op':'action','name':'gameplay.stance','seconds':.15});live.release()
                    time.sleep(1.2)
        finally:
            settings(previous)
            live.execute({'op':'action','name':'system.recenter','seconds':.15});live.release()
            result['preferences_restored'] = True
            atomic_json(live.events.output/'size-probe.json',result)
