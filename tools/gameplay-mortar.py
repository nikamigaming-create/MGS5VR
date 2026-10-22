"""Reach a reviewed mortar, inspect aim, fire once and exit under recording."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import time

from gameplay_bot.core import BotFault, Events, atomic_json
from gameplay_bot.live import InputLease, Live, ROOT, game_identity, channel, rotate
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.route import navigate, set_posture
from gameplay_bot.perception import read_enemy_snapshot
from gameplay_bot.combat import fresh_rig, mul, inverse, controller_pair, paired_aim_position
from gameplay_bot.emplacement import bind_fixture, fixture, look_at_mount
from gameplay_bot.locomotion import move_local
from gameplay_bot.recording import Recording
from gameplay_bot.session import preserved_tracking_poses


def stationary_mortar(state, policy):
    """Mortars use a stationary action; the ridden-vehicle getter stays empty.

    This acknowledges the action beside the reviewed fixture, not an exact
    native mounted-object getter. Pixels must independently prove discharge.
    """
    native = state.get('native', {})
    controls = state.get('controls', {})
    point = [native.get('player_' + axis) for axis in ('x', 'y', 'z')]
    return (all(type(v) in (int, float) and math.isfinite(v) for v in point)
            and math.dist(point, policy['goal']) <= 2.5
            and native.get('mission') == policy['mission']
            and native.get('location') == policy['location']
            and native.get('status_NORMAL_ACTION') is False
            and native.get('status_PARTS_ACTIVE') is True
            and native.get('status_CARRY') is False
            and native.get('status_ON_VEHICLE') is False
            and native.get('status_ON_HORSE') is False
            and native.get('popup') is False and native.get('saving') is False
            and state.get('scene') == 'gameplay' and state.get('menu') is False
            and controls.get('context') == 'gameplay'
            and controls.get('rig_input') is True)


def aim_adjustment(yaw_error, pitch_error):
    """One human-paced stick axis per pulse, above the observed native dead zone."""
    if any(type(v) not in (int, float) or not math.isfinite(v) for v in (yaw_error, pitch_error)):
        raise BotFault('Mortar aiming errors must be finite degrees')
    if abs(yaw_error) >= 3.:
        axis, error = 'X', yaw_error
    elif abs(pitch_error) >= 2.:
        axis, error = 'Y', pitch_error
    else:
        return None
    # The operator publishes sub-components serially. Sending X and Y in the
    # same short chord reproduced an X pulse followed by Y with native X=0.
    # A .30 stick was also delivered without moving the native mortar pitch.
    return axis, -.6 if error > 0 else .6, .35 if axis == 'Y' else min(.35, max(.20, abs(error)*.012))


def inspect_physical_grips(live, policy, anchor, output, result):
    """One selected physical-handle check inside the existing native mortar run."""
    for side, name in (('left','gameplay.support_grip'),('right','gameplay.ready_weapon')):
        action=next(a for a in live.bindings['actions'] if a['name']==name)
        if (action.get('modifier') is not True or len(action['bindings'])!=1
                or action['bindings'][0]['gesture']!='level'
                or action['bindings'][0]['inputs']!=[side+'_grip']):
            raise BotFault('Physical mortar check requires the audited effective grip modifiers')
    checks=result.setdefault('physical_grips',[])
    def sample(mask=None):
        state,_=fresh_rig(live,anchor)
        if not stationary_mortar(state,policy) or state['rendered'].get('mounted_weapon_action')!=0x46:
            raise BotFault('Physical grip lost the observed native mortar action')
        if mask is not None and state['rendered'].get('mounted_weapon_hands')!=mask:
            raise BotFault('Native handle grip did not remain acquired')
        return state
    def wait_grip(mask):
        until=time.monotonic()+1.5
        while True:
            state=sample()
            if state['rendered'].get('mounted_weapon_hands')==mask:return state
            if time.monotonic()>=until:raise BotFault('Physical grip did not acquire the native handle')
            time.sleep(.025)
    def place(side,contact,degrees=0.):
        grip=live.text_result(live.call('get_controller_pose',{'hand':side,'pose_type':'grip','base_space':'local'}))['pose']
        aim=live.text_result(live.call('get_controller_pose',{'hand':side,'pose_type':'aim','base_space':'local'}))['pose']
        pair=controller_pair(grip,aim);half=math.radians(degrees)/2
        attitude=mul([0.,math.sin(half),0.,math.cos(half)],contact['orientation'])
        aim_attitude=mul(attitude,inverse(pair['orientation']))
        for kind,point,q in (('grip',contact['position'],attitude),
                            ('aim',paired_aim_position(contact['position'],aim_attitude,pair),aim_attitude)):
            live.call('set_controller_pose',{'hand':side,'pose_type':kind,'base_space':'local',
                'position':point,'orientation':q})
    def yaw(pose):
        f=rotate(pose['orientation'],[0.,0.,1.]);return math.degrees(math.atan2(f[0],f[2]))
    for side,mask in (('right',2),('left',1)):
        live.release();state=sample();contact=state['rendered']['mounted_'+side+'_grip']
        place(side,contact);time.sleep(.12)
        try:
            live.input([channel(side+'_grip')],5.,lease_seconds=5.)
            before=wait_grip(mask);place(side,contact,12.)
            samples=[];until=time.monotonic()+1.6
            while time.monotonic()<until:
                state=sample(mask);samples.append(state);time.sleep(.025)
            change=(state['native']['player_camera_yaw']-before['native']['player_camera_yaw']+180.)%360.-180.
            viewer=(yaw(state['rendered']['native_pose'])-yaw(before['rendered']['native_pose'])+180.)%360.-180.
            row={'hand':side,'before':before,'after':state,'samples':samples,
                 'native_yaw_delta':change,'viewer_yaw_delta':viewer};checks.append(row)
            atomic_json(output/'progress.json',result)
            if not 5.<change<20. or abs(viewer)>1.:
                raise BotFault('Physical handle aim did not turn the native weapon independently of the viewer')
            if any(abs(s['rendered']['mounted_weapon_axes'][1])>.001 for s in samples):
                raise BotFault('Pure physical yaw chased native pitch animation')
            if state['rendered']['mounted_weapon_axes']!=[0,0]:
                raise BotFault('Physical handle continued steering after the hand settled')
            row['eyes']=live.capture('mortar-physical-'+side,static_grips=True)
        finally:live.release()
        released=wait_grip(0)
        if released['controls']['native_axes'][2:]!=[0,0]:
            raise BotFault('Released physical handle retained native aiming input')
        row['released']=released
    live.release();state=sample()
    for side in ('left','right'):place(side,state['rendered']['mounted_'+side+'_grip'])
    try:
        live.input([channel('left_grip'),channel('right_grip')],5.,lease_seconds=5.)
        before=wait_grip(3)
        live.input([{'hand':'right','component':'Thumbstick','sub_component':'X','value':.6}],.3,lease_seconds=.3)
        time.sleep(.3);live.release(keep={('left','Grip',None),('right','Grip',None)})
        time.sleep(.5);state=sample(3)
        if state['controls']['native_axes'][2:]!=[0,0]:
            raise BotFault('Held handles fought the released native aiming stick')
        checks.append({'hands':'both','before':before,'after':state,
            'eyes':live.capture('mortar-physical-both-after-stick',static_grips=True)})
    finally:live.release()
    wait_grip(0);atomic_json(output/'progress.json',result)


def run(live, nav, policy, output, seconds):
    result = {'status': 'running', 'scope': 'reviewed_mortar_entry_aim_fire_exit',
              'shot_proven': False, 'exact_mounted_id_proven': False,
              'aim_proven': False, 'arc_proven': False,
              'visual_acceptance': 'pending_discharge_video_review',
              'all_emplacements_acceptance': False, 'policy': policy}
    recorder = None
    entered = False
    interaction_requested = False
    started = time.monotonic()
    baseline = None

    def check():
        if time.monotonic() - started >= seconds:
            raise BotFault('Mortar test reached its selected deadline')
        if recorder is not None:
            recorder.check()

    def health(state):
        native = state.get('native')
        if native and baseline is not None:
            life = native.get('player_life', 0)
            allowance = policy.get('diagnostic_damage_budget', 0)
            if (type(allowance) not in (int, float) or not math.isfinite(allowance)
                    or not 0 <= allowance <= 20000
                    or (allowance and policy.get('active_camp_diagnostic') is not True)):
                raise BotFault('Damage allowance requires a bounded active-camp diagnostic policy')
            allowance = min(allowance, baseline * .95)
            if life <= 0 or baseline - life > allowance:
                raise BotFault('Native damage interrupted mortar test')
            if life < baseline:
                result['damage_observed'] = {'baseline': baseline, 'minimum_life':
                    min(life, result.get('damage_observed', {}).get('minimum_life', baseline)),
                    'diagnostic_budget': allowance}

    try:
        live.release()
        state, _ = fresh_rig(live)
        native = state['native']
        if (policy.get('type') != 81 or policy.get('map_identity') != nav.identity
                or native['mission'] != policy['mission'] or native['location'] != policy['location']):
            raise BotFault('Mortar test requires its reviewed native field fixture')
        operator = policy.get('operator_position')
        if (not isinstance(operator, list) or len(operator) != 3
                or not all(type(v) in (int, float) and math.isfinite(v) for v in operator)
                or math.dist(operator, policy['goal']) > 2.5):
            raise BotFault('Mortar test requires a reviewed nearby operator position')
        baseline = native['player_life']
        live.state_observer = health
        live.background_check = check
        policy = bind_fixture(live, policy)
        result['policy'] = policy
        fixture(live, policy)
        active_camp = policy.get('active_camp_diagnostic') is True
        result['approach'] = navigate(live, nav, operator, output=output/'approach', mode='run',
            max_seconds=max(.1, seconds - (time.monotonic() - started)), recenter=False,
            interrupt_on_danger=not active_camp, known_battle=active_camp,
            avoid_threats=not active_camp,
            expected_location=policy['location'], arrival_radius=2.,
            enemy_reader=lambda: read_enemy_snapshot(live.native.read, (ROOT/'tools/navigation-state.lua').read_text()))
        if result['approach']['status'] != 'observed_native_route':
            raise BotFault(result['approach'].get('error', 'Mortar approach failed'))
        set_posture(live, 'stand')
        state, anchor = fresh_rig(live)
        result['positioning'] = []
        for _ in range(7):
            check()
            n = state['native']
            distance = math.hypot(n['player_x'] - operator[0], n['player_z'] - operator[2])
            if distance < .2:
                break
            if distance > 2.5:
                raise BotFault('Arrival drifted outside the local mortar approach')
            look_at_mount(live, anchor, [operator[0], state['rendered']['native_pose']['position'][1], operator[2]])
            try:
                result['positioning'].append(move_local(live, target_distance=min(.18, max(.07, distance-.08)),
                    max_displacement=.65, max_seconds=min(5., max(.1, seconds-(time.monotonic()-started))),
                    # Live retail sampling: .30 is delivered but produces no
                    # movement after resolver/native deadzones; .40 advances.
                    cruise_magnitude=.45, brake_magnitude=.4, pulse_seconds=.08))
            except BotFault as error:
                state, _ = fresh_rig(live, anchor)
                n = state['native']
                if math.hypot(n['player_x'] - operator[0], n['player_z'] - operator[2]) >= .2:
                    raise
                result['positioning'].append({'status': 'reached_native_interaction_neighborhood',
                    'segment_stop': str(error), 'state': state})
                break
            state, _ = fresh_rig(live, anchor)
        else:
            raise BotFault('Mortar operator position was not reached')
        look_at_mount(live, anchor, [policy['goal'][0], state['rendered']['native_pose']['position'][1], policy['goal'][2]])
        result['native_facing'] = []
        for _ in range(3):
            state, _ = fresh_rig(live, anchor)
            n = state['native']
            desired = math.degrees(math.atan2(policy['goal'][0] - n['player_x'],
                                             policy['goal'][2] - n['player_z']))
            error = (desired - n['player_yaw'] + 180.) % 360. - 180.
            if abs(error) < 25.:
                break
            if n.get('status_NORMAL_ACTION') is not True:
                raise BotFault('Native action changed while facing the fixture')
            try:
                live.input([{'hand': 'right', 'component': 'Grip', 'value': 1.},
                            {'hand': 'right', 'component': 'Thumbstick', 'sub_component': 'X',
                             'value': 1. if error < 0 else -1.}], 3., lease_seconds=3.)
                until = time.monotonic() + 3.
                while time.monotonic() < until:
                    state, _ = fresh_rig(live, anchor)
                    n = state['native']
                    if n.get('status_NORMAL_ACTION') is not True:
                        raise BotFault('Native action changed while facing the fixture')
                    current_error = (desired - n['player_yaw'] + 180.) % 360. - 180.
                    if abs(current_error) < 25.:
                        break
                    time.sleep(.025)
            finally:
                live.release()
            result['native_facing'].append(state)
            if abs((desired - state['native']['player_yaw'] + 180.) % 360. - 180.) < 25.:
                break
        else:
            raise BotFault('Native body did not face the mortar within its bounded turn')
        look_at_mount(live, anchor, [policy['goal'][0], state['rendered']['native_pose']['position'][1], policy['goal'][2]])
        result['fixture'] = fixture(live, policy)
        recorder = Recording(live.game, live.events, live.identity['process']['ProcessId'], segment_seconds=12.)
        recorder.start()
        live.held_action_recording = recorder
        result['before'] = live.observe(native=True)
        result['before_eyes'] = live.capture('mortar-use-before')
        interaction_requested = True
        result['enter_action'] = live.execute({'op': 'action', 'name': 'gameplay.interact', 'seconds': .18,
            'native_before': {'mission': policy['mission'], 'location': policy['location'], 'popup': False,
                              'saving': False, 'status_NORMAL_ACTION': True, 'status_CARRY': False}})
        deadline = time.monotonic() + 2.
        while True:
            state, _ = fresh_rig(live, anchor)
            if stationary_mortar(state, policy):
                entered = True
                break
            if time.monotonic() >= deadline:
                raise BotFault('Mortar entry was not acknowledged; no trigger dispatched')
            time.sleep(.025)
        result['entered'] = state
        # Y preserves the viewer's heading; LOCAL identity is not necessarily
        # the emplacement's forward direction. Look along the native aiming
        # bearing through the existing physical-head transform, including its
        # retained origin/snap offset, instead of looking into Snake's pack.
        point = state['rendered']['native_pose']['position']
        yaw = math.radians(state['native']['player_camera_yaw'])
        look_at_mount(live, anchor, [point[0]+math.sin(yaw), point[1], point[2]+math.cos(yaw)])
        head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
        forward_head = list(head['orientation'])
        state, _ = fresh_rig(live, anchor)
        result['aim_forward'] = state
        result['entered_eyes'] = live.capture('mortar-entered')
        if policy.get('physical_grips') is True:
            inspect_physical_grips(live,policy,anchor,output,result)
        result['aim_ready'] = live.execute({'op': 'action', 'name': 'gameplay.ready_weapon', 'seconds': 1.5,
            'native_before': {'mission': policy['mission'], 'location': policy['location'],
                              'popup': False, 'status_NORMAL_ACTION': False},
            'while_held': {'controls.native_triggers': [255, 0]},
            'static_grip_capture': True,
            'capture_while_held': 'mortar-native-aim-arc'})
        result['aim_axes'] = []
        for axis, sign in (('X', 1), ('X', -1), ('Y', 1), ('Y', -1)):
            before, _ = fresh_rig(live, anchor)
            if not stationary_mortar(before, policy):
                raise BotFault('Stationary mortar ownership changed before aim')
            samples = []
            duration = .65
            try:
                live.input([{'hand': 'right', 'component': 'Grip', 'value': 1.},
                            {'hand': 'right', 'component': 'Thumbstick', 'sub_component': axis,
                             'value': sign * .6}], duration, lease_seconds=duration)
                deadline = time.monotonic() + duration
                while time.monotonic() < deadline:
                    current, _ = fresh_rig(live, anchor)
                    if not stationary_mortar(current, policy):
                        raise BotFault('Stationary mortar ownership changed during aim')
                    samples.append(current)
                    time.sleep(.025)
            finally:
                live.release()
            after, _ = fresh_rig(live, anchor)
            result['aim_axes'].append({'axis': axis, 'sign': sign, 'before': before,
                                       'after': after, 'samples': samples,
                                       'eyes': live.capture('mortar-aim-' + axis.lower() +
                                                            ('plus' if sign > 0 else 'minus'))})
            atomic_json(output/'progress.json', result)
        head['orientation'] = mul(forward_head,
            [math.sin(math.radians(12.5)), 0., 0., math.cos(math.radians(12.5))])
        live.execute({'op':'motion','seconds':.75,'head':{'offset':[0.,0.,0.],'orientation':head['orientation']}})
        result['arc_look'] = live.execute({'op': 'action', 'name': 'gameplay.ready_weapon', 'seconds': 1.2,
            'native_before': {'mission': policy['mission'], 'location': policy['location'],
                              'popup': False, 'status_NORMAL_ACTION': False},
            'while_held': {'controls.native_triggers': [255, 0]},
            'static_grip_capture': True,
            'capture_while_held': 'mortar-native-arc-looking-up'})
        head['orientation'] = forward_head
        live.execute({'op':'motion','seconds':.75,'head':{'offset':[0.,0.,0.],'orientation':head['orientation']}})
        target = policy.get('aim_target')
        if (not isinstance(target, list) or len(target) != 3
                or not all(type(v) in (int, float) and math.isfinite(v) for v in target)
                or not 50. <= math.dist(target, policy['goal']) <= 300.):
            raise BotFault('Mortar firing requires a reviewed downrange aim point 50..300 units away')
        result['downrange_aim'] = {'target': target, 'samples': [], 'impact_proven': False}
        grip = {'hand': 'right', 'component': 'Grip', 'value': 1.}
        keep_grip = {('right', 'Grip', None)}
        # The last pulse must get a final observation. A for/else immediately
        # after twelve pulses rejected a live run that reached tolerance on
        # pulse twelve, without inspecting its result.
        for attempt in range(13):
            state, _ = fresh_rig(live, anchor)
            if not stationary_mortar(state, policy):
                raise BotFault('Mortar owner changed before downrange aim')
            position = state['rendered']['native_pose']['position']
            delta = [b-a for a,b in zip(position,target)]
            yaw = math.degrees(math.atan2(delta[0], delta[2]))
            pitch = -math.degrees(math.atan2(delta[1], math.hypot(delta[0], delta[2])))
            n = state['native']
            yaw_error = (yaw - n['player_camera_yaw'] + 180.) % 360. - 180.
            pitch_error = pitch - n['player_camera_pitch']
            adjustment = aim_adjustment(yaw_error, pitch_error)
            if adjustment is None:
                result['downrange_aim']['settled'] = {'state':state,
                    'yaw_error':yaw_error, 'pitch_error':pitch_error}
                break
            if attempt == 12:
                raise BotFault('Native mortar aim did not reach its downrange bearing; no shot dispatched')
            axis, value, duration = adjustment
            live.input([grip], 5., lease_seconds=5.)
            values = [{'hand': 'right', 'component': 'Thumbstick', 'sub_component': axis, 'value': value}]
            result['downrange_aim'].setdefault('adjustments',[]).append({
                'yaw_error':yaw_error, 'pitch_error':pitch_error, 'axis':axis, 'value':value, 'seconds':duration})
            try:
                live.input(values, duration, lease_seconds=duration)
                end = time.monotonic() + duration
                while time.monotonic() < end:
                    state, _ = fresh_rig(live, anchor)
                    if not stationary_mortar(state, policy):
                        raise BotFault('Mortar owner changed during downrange aim')
                    result['downrange_aim']['samples'].append(state)
                    time.sleep(.02)
            finally:
                live.release(keep=keep_grip)
        # Keep the actual native aim modifier through firing. Releasing it
        # between aiming and RT returns to the close unready camera solution.
        live.input([grip], 5., lease_seconds=5.)
        end = time.monotonic() + .8
        while True:
            state, _ = fresh_rig(live, anchor)
            if not stationary_mortar(state, policy):
                raise BotFault('Mortar owner changed before aimed trigger')
            if state['controls']['native_triggers'] == [255, 0]:
                break
            if time.monotonic() >= end:
                raise BotFault('Mortar ready modifier was not delivered; no shot dispatched')
        result['downrange_aim']['held_state'] = state
        result['downrange_aim']['eyes'] = live.capture('mortar-downrange-aim-held', static_grips=True)
        live.events.emit('mortar_downrange_aim_held', state=state, target=target,
                         impact_proven=False, visual_source='native_source_eye_recording')
        result['trigger'] = {'samples': [], 'seconds': .18, 'ready_retained': True}
        trigger = {'hand': 'right', 'component': 'Trigger', 'value': 1.}
        try:
            live.input([trigger], .18, lease_seconds=.18)
            end = time.monotonic() + .18
            while time.monotonic() < end:
                state, _ = fresh_rig(live, anchor)
                if not stationary_mortar(state, policy):
                    raise BotFault('Mortar owner changed during aimed trigger')
                result['trigger']['samples'].append(state)
                time.sleep(.02)
        finally:
            live.call('set_controller_input', {**trigger, 'value': 0, 'auto_release': False})
            live.held.pop(('right', 'Trigger', None), None)
        if not any(s['controls']['native_triggers'] == [255, 255] for s in result['trigger']['samples']):
            raise BotFault('Aimed mortar RT was not sampled; no ambiguous replay')
        # The native shell-load/fire animation discharges after the short RT.
        # Keep recording its actual response before dismounting; do not replay RT.
        result['response'] = []
        deadline = time.monotonic() + 5.
        while time.monotonic() < deadline:
            state, _ = fresh_rig(live, anchor)
            if not stationary_mortar(state, policy):
                raise BotFault('Stationary mortar ownership changed after trigger')
            result['response'].append(state)
            time.sleep(.05)
        live.release()
        result['fired_eyes'] = live.capture('mortar-trigger-response')
        result['exit_action'] = live.execute({'op': 'action', 'name': 'gameplay.interact', 'seconds': .18,
            'native_before': {'mission': policy['mission'], 'location': policy['location'], 'popup': False,
                              'status_NORMAL_ACTION': False}})
        deadline = time.monotonic() + 2.
        while True:
            state = live.observe(native=True)
            if state['native']['status_NORMAL_ACTION'] is True:
                entered = False
                break
            if time.monotonic() >= deadline:
                raise BotFault('Mortar exit was not acknowledged')
            time.sleep(.025)
        result['after'] = state
        result['after_eyes'] = live.capture('mortar-exited')
        result['status'] = 'observed_mortar_entry_trigger_exit'
    except Exception as error:
        result.update(status='failed', error=str(error))
    finally:
        live.state_observer = None
        live.background_check = None
        live.held_action_recording = None
        try:
            live.release()
            state = live.observe(native=True)
            if interaction_requested and stationary_mortar(state, policy):
                live.execute({'op': 'action', 'name': 'gameplay.interact', 'seconds': .18,
                    'native_before': {'mission': policy['mission'], 'location': policy['location'],
                                      'popup': False, 'status_NORMAL_ACTION': False}})
            result['released'] = live.observe(native=True)
        except Exception as error:
            result.update(status='failed', cleanup_error=str(error))
        if recorder is not None:
            result['recording'] = recorder.close()
            if Recording.failure_reason(result['recording']):
                result.update(status='failed', recording_error=Recording.failure_reason(result['recording']))
        result['seconds'] = time.monotonic() - started
        atomic_json(output/'result.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('game-dir', 'proxy', 'output', 'policy', 'manifest'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=60.)
    parser.add_argument('--record', action='store_true', required=True)
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 15. <= args.seconds <= 120.:
        parser.error('Mortar test requires 15..120 seconds')
    policy = json.loads(args.policy.read_text(encoding='utf-8-sig'))
    atlas = NavigationAtlas(json.loads(args.manifest.read_text(encoding='utf-8-sig')),
        json.loads((ROOT/'tools/gameplay_bot/catalogs/native-worlds.json').read_text()), base=args.manifest.parent)
    with InputLease():
        identity = game_identity(args.game_dir)
        bindings = json.loads(subprocess.check_output([str(args.game_dir/'mgs5vr_controls.exe'),
            '--bindings-json', str(args.game_dir/'mgs5vr-controls.ini')], text=True))
        live = Live(args.proxy, args.game_dir, bindings, Events(args.output, identity))
        try:
            live.ready()
            with preserved_tracking_poses(live):
                result = run(live, atlas.select(policy['location']), policy, args.output, args.seconds)
        finally:
            live.close()
    print(json.dumps({k: result.get(k) for k in ('status', 'error', 'seconds', 'shot_proven')}))
    return int(result['status'] == 'failed')


if __name__ == '__main__':
    raise SystemExit(main())
