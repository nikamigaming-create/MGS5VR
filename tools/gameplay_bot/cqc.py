"""Bounded CQC through the existing lowered-weapon physical VR trigger.

This is not physical hand acquisition. Retail owns the contextual action. A
player CQC bit does not identify a held soldier; exact-actor changes are retained
as candidate observations, with buddy attribution and the direct link unresolved.
"""
import math
import re
import time
from pathlib import Path

from .combat import fresh_rig, looking_head
from .core import BotFault, atomic_json
from .health import HealthRecoveryNeeded, needs_recovery
from .live import authoritative_controls, channel, held_input_evidence, rotate
from .locomotion import _position, _validate_xr_field_state, _neutral_audit
from .perception import read_enemy_snapshot
from .session import preserved_tracking_poses


SOURCE_CONTRACT = {
    'vr_route': 'src/input_bridge.cpp: lowered weapon keeps right trigger as native attack/CQC',
    'effective_binding': 'gameplay.fire_or_cqc=level(right_trigger); ready/support grips released',
    'native_status': 'registered PlayerStatus.CQC_HOLD=87 and CQC_CONTINUOUS=88',
    'native_actor_reads': 'TppEnemy GetLifeStatus/GetActionStatus/GetStatus and native soldier GetPosition',
    'manual': 'https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html',
    'native_player_to_actor_link_verified': False,
    'physical_hand_grab_implemented': False,
    'pulse_durations': 'bounded experimental simulator inputs; not a discovered native timing contract',
}


def actor_identity(actor):
    if (not isinstance(actor, dict) or not isinstance(actor.get('name'), str) or
            not re.fullmatch(r'sol_[A-Za-z0-9_]{1,120}', actor['name']) or
            type(actor.get('id')) is not int or not 0 < actor['id'] < 65535):
        raise BotFault('CQC requires an exact owned soldier name and ID')
    return actor['name'], actor['id']


def cqc_binding(bindings):
    actions = bindings.get('actions', [])
    matches = [r for r in actions if r.get('name') == 'gameplay.fire_or_cqc']
    if (len(matches) != 1 or 'gameplay' not in matches[0].get('contexts', []) or
            len(matches[0].get('bindings', [])) != 1 or
            matches[0]['bindings'][0].get('gesture') != 'level' or
            matches[0]['bindings'][0].get('inputs') != ['right_trigger']):
        raise BotFault('CQC requires the effective plain physical right-trigger VR binding')
    return ['right_trigger']


def native_cqc_script(actor, anchor):
    name, object_id = actor_identity(actor)
    if (not isinstance(anchor, (list, tuple)) or len(anchor) != 5 or
            any(type(v) is not int or v <= 0 for v in anchor)):
        raise BotFault('CQC requires an exact numeric world/VR owner')
    return f'''-- Bounded read-only CQC/one exact native soldier. No native writes.
assert(vars.missionCode=={anchor[0]} and vars.locationCode=={anchor[1]},'CQC world changed')
assert(PlayerStatus.CQC_HOLD==87 and PlayerStatus.CQC_CONTINUOUS==88,'CQC status registration differs')
assert(TppEnemy.LIFE_STATUS.NORMAL==0 and TppEnemy.LIFE_STATUS.SLEEP==3 and TppEnemy.LIFE_STATUS.FAINT==4,'life enum differs')
local id=GameObject.GetGameObjectId('TppSoldier2','{name}')
assert(id=={object_id} and id~=GameObject.NULL_ID,'exact CQC actor changed')
assert(GameObject.GetTypeIndex(id)==TppGameObject.GAME_OBJECT_TYPE_SOLDIER2,'CQC actor type differs')
local function scalar(v)
 if type(v)=='number' then assert(v==v and v~=math.huge and v~=-math.huge);return tostring(v) end
 if v==nil then return 'null' end
 assert(type(v)=='string' and #v<=128,'native state is not a bounded scalar')
 return '"'..v:gsub('[%z\\1-\\31\\\\"]',function(c)return string.format('\\\\u%04x',string.byte(c))end)..'"'
end
local position=TppMath.Vector3toTable(GameObject.SendCommand(id,{{id='GetPosition'}}))
assert(type(position)=='table' and #position==3,'CQC actor point unavailable')
local life=GameObject.SendCommand(id,{{id='GetLifeStatus'}})
assert(type(life)=='number' and life%1==0,'native life unavailable')
local action=GameObject.SendCommand(id,{{id='GetActionStatus'}})
local status=GameObject.SendCommand(id,{{id='GetStatus'}})
local hold=PlayerInfo.AndCheckStatus{{PlayerStatus.CQC_HOLD}}
local continuous=PlayerInfo.AndCheckStatus{{PlayerStatus.CQC_CONTINUOUS}}
assert(type(hold)=='boolean' and type(continuous)=='boolean','native CQC status unavailable')
local playerIndex=PlayerInfo.GetLocalPlayerIndex()
assert(type(playerIndex)=='number' and playerIndex%1==0,'native local player unavailable')
return '{{"mission":'..vars.missionCode..',"location":'..vars.locationCode..
 ',"player_index":'..playerIndex..',"cqc_hold":'..tostring(hold)..',"cqc_continuous":'..tostring(continuous)..
 ',"id":'..id..',"life":'..life..',"action":'..scalar(action)..',"status":'..scalar(status)..
 ',"position":['..scalar(position[1])..','..scalar(position[2])..','..scalar(position[3])..']}}'
'''


def validate_native(data, actor, anchor, *, player_index=None):
    actor_identity(actor)
    if (data.get('mission'), data.get('location'), data.get('id')) != (anchor[0], anchor[1], actor['id']):
        raise BotFault('Exact native CQC world/actor changed')
    if (type(data.get('player_index')) is not int or data['player_index'] < 0 or
            (player_index is not None and data['player_index'] != player_index) or
            any(type(data.get(k)) is not bool for k in ('cqc_hold', 'cqc_continuous')) or
            type(data.get('life')) is not int):
        raise BotFault('Native CQC player/status/life is unavailable')
    point = data.get('position')
    if (not isinstance(point, list) or len(point) != 3 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in point)):
        raise BotFault('Exact native CQC actor point is unavailable')
    for key in ('action', 'status'):
        value = data.get(key)
        if (value is not None and (type(value) not in (int, float, str) or
                (isinstance(value, str) and len(value) > 128) or
                (type(value) in (int, float) and not math.isfinite(value)))):
            raise BotFault('Native actor state is not a bounded scalar')
    return data


def cqc_state(live, anchor, healthy_life=None, *, transition=False):
    state, identity = fresh_rig(live, anchor)
    controls, _ = _validate_xr_field_state(state)
    native, frame = state['native'], state['rendered']
    if (native.get('player_vehicle_id') != 65535 or native.get('popup') is not False or
            native.get('saving') is not False or native.get('demo') is not False or
            native.get('demo_nonplayable') is not False or native.get('game_over') != 0 or
            native.get('title') is not False or native.get('status_CARRY') is not False or
            native.get('status_CURE') is not False or
            (not transition and native.get('status_NORMAL_ACTION') is not True) or
            frame.get('binocular_held') is not False or
            any(frame.get('raw_right_'+kind+'_tracked') is not True for kind in ('grip', 'aim'))):
        raise BotFault('CQC lost its on-foot lower-weapon VR action owner')
    if healthy_life is not None and needs_recovery(native, healthy_life):
        raise HealthRecoveryNeeded('Health recovery interrupts the bounded CQC experiment')
    return state, identity


def cqc_packet(state, *, after_sample_ms):
    audit = held_input_evidence(state, ['right_trigger'], after_sample_ms=after_sample_ms)
    if audit is None:
        return None
    controls = authoritative_controls(state)
    if (state.get('gamepad') is not False or controls.get('context') != 'gameplay' or
            controls.get('rig_input') is not True or controls.get('travel_mode') != 1 or
            controls.get('native_buttons') != 0 or controls.get('native_triggers') != [0, 255] or
            controls.get('native_axes') != [0, 0, 0, 0] or
            any(abs(v) > .09 for i, v in enumerate(controls['physical'][:11]) if i != 10) or
            any(abs(v) > .09 for v in controls['sticks'])):
        raise BotFault('CQC was not the witnessed lowered LT0/RT-only VR packet')
    return audit


def exact_close_target(snapshot, actor, anchor, player, *, max_range=1.75):
    """A proximity admission bound is an experiment, not native CQC eligibility."""
    name, object_id = actor_identity(actor)
    if ((snapshot.get('mission'), snapshot.get('location')) != tuple(anchor[:2]) or
            snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded') != 0 or
            snapshot.get('normal_life') != '0'):
        raise BotFault('CQC requires a complete current native enemy roster')
    soldiers = snapshot.get('soldiers')
    if not isinstance(soldiers, list):
        raise BotFault('CQC enemy roster is unavailable')
    names, ids = set(), set()
    for row in soldiers:
        row_name, row_id = actor_identity(row)
        if row_name in names or row_id in ids:
            raise BotFault('CQC enemy identity is ambiguous')
        names.add(row_name); ids.add(row_id)
    matching = [r for r in soldiers if r.get('name') == name and r.get('id') == object_id]
    if len(matching) != 1 or matching[0].get('life') != '0':
        raise BotFault('The chosen exact CQC guard is absent or already incapacitated')
    chosen = matching[0]
    position = chosen.get('position')
    if (not isinstance(position, list) or len(position) != 3 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in position)):
        raise BotFault('CQC guard point is unavailable')
    distance = math.hypot(position[0]-player[0], position[2]-player[2])
    if distance > max_range or abs(position[1]-player[1]) > 1.5:
        raise BotFault('The actual CQC guard is outside the bounded close-approach range')
    for row in soldiers:
        if row.get('id') == object_id or row.get('life') != '0':
            continue
        point = row.get('position')
        if (not isinstance(point, list) or len(point) != 3 or
                any(type(v) not in (int, float) or not math.isfinite(v) for v in point)):
            raise BotFault('Another native guard has an unavailable point')
        if math.hypot(point[0]-player[0], point[2]-player[2]) <= 2.5 and abs(point[1]-player[1]) <= 1.5:
            raise BotFault('Another active close guard makes the CQC target ambiguous')
    return chosen


def _close_position(position, player):
    if (math.hypot(position[0]-player[0], position[2]-player[2]) > 1.75 or
            abs(position[1]-player[1]) > 1.5):
        raise BotFault('Fresh exact CQC actor moved out of the close-approach bound')


def _facing(camera, position):
    forward = rotate(camera['orientation'], [0., 0., 1.])
    delta = [position[0]-camera['position'][0], position[2]-camera['position'][2]]
    length = math.hypot(*delta); facing = math.hypot(forward[0], forward[2])
    if min(length, facing) <= .01:
        raise BotFault('CQC horizontal facing is degenerate')
    cosine = (forward[0]*delta[0]+forward[2]*delta[1])/(length*facing)
    angle = math.degrees(math.acos(max(-1., min(1., cosine))))
    if angle > 30.:
        raise BotFault('Fresh actual player view still faces away from the CQC guard')
    return angle


def _checkpoint(live, label, state):
    recording = getattr(live, 'held_action_recording', None)
    if recording is not None:
        recording.check()
        live.events.emit('cqc_video_checkpoint', label=label, state=state,
            source='native_source_eye_before_runtime_crop_and_composition',
            final_compositor_captured=False, visual_acceptance='pending_video_review')
        return []
    return live.capture(label)


def wait_released(live, script, actor, anchor, player_index, *, max_seconds=1.5,
                  clock=time.monotonic, sleep=time.sleep):
    """Observe native release/animation completion; never invent a release chord."""
    started = clock(); attempts = 0; state = observation = None
    while clock()-started < max_seconds:
        # Damage during CQC is retained and handed back to the caller. It
        # cannot prevent observation of an already released trigger.
        state, _ = cqc_state(live, anchor, None, transition=True)
        controls = state['controls']; _neutral_audit(controls)
        if (controls.get('native_buttons') != 0 or controls.get('native_triggers') != [0, 0] or
                controls.get('native_axes') != [0, 0, 0, 0]):
            raise BotFault('CQC release observation requires an actual neutral native packet')
        observation, timing = live.native.read(script)
        validate_native(observation, actor, anchor, player_index=player_index)
        state, _ = cqc_state(live, anchor, None, transition=True)
        attempts += 1
        finished = (not observation['cqc_hold'] and not observation['cqc_continuous'] and
                    state['native'].get('status_CQC_HOLD') is False and
                    state['native'].get('status_CQC_CONTINUOUS') is False and
                    state['native'].get('status_NORMAL_ACTION') is True)
        elapsed = clock()-started
        live.events.emit('cqc_release_observation', native=observation, state=state,
                         seconds=elapsed, ordinary_field_resumed=finished,
                         input_replayed=False, transport=timing)
        if finished and elapsed <= max_seconds:
            return {'released': True, 'seconds': elapsed, 'observations': attempts,
                    'native': observation, 'state': state}
        sleep(min(.035, max(0., max_seconds-(clock()-started))))
    return {'released': False, 'seconds': clock()-started, 'observations': attempts,
            'native': observation, 'state': state,
            'reason': 'native restraint or animation still owns field; no guessed release action'}


def classify(before, during, after, *, ammo_unchanged):
    if not ammo_unchanged:
        raise BotFault('Lowered CQC input changed firearm ammunition; preserve the dispatch failure')
    same = all(row['id'] == before['id'] and row['player_index'] == before['player_index'] for row in during+[after])
    if not same:
        raise BotFault('Native CQC outcome changed exact actor or player')
    witnessed_cqc = any(row['cqc_hold'] or row['cqc_continuous'] for row in during)
    state_change = any((row['action'], row['status']) != (before['action'], before['status']) for row in during+[after])
    if not during:
        status = 'exact_actor_changed_before_cqc_dispatch' if before['life'] != after['life'] else 'no_cqc_input_dispatched'
    elif before['life'] == 0 and after['life'] in (3, 4):
        status = 'observed_exact_actor_incapacitation_during_vr_cqc_input'
    elif witnessed_cqc and state_change:
        status = 'observed_native_cqc_and_exact_actor_state_change'
    elif witnessed_cqc:
        status = 'observed_player_cqc_state_only'
    else:
        status = 'no_native_cqc_outcome_observed'
    return {'status': status, 'target_id': before['id'], 'target_life_after': after['life'],
            'native_cqc_state_observed': witnessed_cqc, 'exact_actor_state_change': state_change,
            'restraint_released_observed': witnessed_cqc and not after['cqc_hold'] and not after['cqc_continuous'],
            'native_player_to_actor_link_verified': False, 'buddy_attribution_excluded': False,
            'choke_throw_knife_or_physical_hand_contact_proven': False}


def attempt_cqc(live, *, actor, anchor, enemy_script, output, healthy_life=None,
                pulses=1, pulse_seconds=.65, gap_seconds=.08, clock=time.monotonic, sleep=time.sleep):
    """One single hold, or an explicitly selected chain of at most five presses.

    Caller first approaches within 1.75m using native NAV and crouches/stands.
    This executor never teleports, moves, writes actor state, opens a menu or
    retries an uncertain input. Each independently expiring pulse is sampled.
    """
    cqc_binding(live.bindings)
    if (type(pulses) is not int or not 1 <= pulses <= 5 or
            type(pulse_seconds) not in (int, float) or not .12 <= pulse_seconds <= 1. or
            type(gap_seconds) not in (int, float) or not .06 <= gap_seconds <= .20 or
            (pulses > 1 and pulse_seconds > .25)):
        raise BotFault('CQC needs one bounded hold or 2..5 short sampled presses')
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'source_contract': SOURCE_CONTRACT, 'target': actor,
              'pulses': [], 'captures': [], 'visual_acceptance': 'pending_both_eye_review',
              'native_player_to_actor_link_verified': False, 'physical_hand_grab_implemented': False}
    try:
        with preserved_tracking_poses(live):
            live.release(); state, _ = cqc_state(live, anchor, healthy_life)
            _neutral_audit(state['controls'])
            if state['native'].get('status_CRAWL') is True:
                raise BotFault('CQC approach must be crouched or standing, not crawling')
            snapshot = read_enemy_snapshot(live.native.read, enemy_script)
            state, _ = cqc_state(live, anchor, healthy_life)
            exact_close_target(snapshot, actor, anchor, _position(state))
            script = native_cqc_script(actor, anchor)
            before, timing = live.native.read(script)
            validate_native(before, actor, anchor)
            if before['life'] != 0 or before['cqc_hold'] or before['cqc_continuous']:
                raise BotFault('CQC starts only with a live chosen guard and no existing restraint')
            result['before'] = before
            baseline_ammo = {k: state['native'].get('ammo_in_weapon_'+k) for k in ('PRIMARY_HIP', 'PRIMARY_BACK', 'SECONDARY')}
            if any(type(v) not in (int, float) or not math.isfinite(v) or v < 0 for v in baseline_ammo.values()):
                raise BotFault('CQC ammunition counters are unavailable')
            # Face the observed root horizontally; no guessed head/chest socket.
            head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
            camera = state['rendered']['native_pose']
            goal = [before['position'][0], camera['position'][1], before['position'][2]]
            head['orientation'] = looking_head(head['orientation'], camera, goal)
            live.call('set_head_pose', {'base_space': 'local', **head})
            state, _ = cqc_state(live, anchor, healthy_life)
            result['captures'] += _checkpoint(live, 'cqc-before', state)
            observed = []
            for index in range(pulses):
                if (output/'STOP').exists(): raise BotFault('CQC stopped before the next pulse')
                state, _ = cqc_state(live, anchor, healthy_life, transition=index > 0)
                _neutral_audit(state['controls'])
                if (state['controls'].get('native_triggers') != [0, 0] or
                        state['controls'].get('native_buttons') != 0 or
                        state['controls'].get('native_axes') != [0, 0, 0, 0]):
                    raise BotFault('CQC requires native aim/attack released before each new press')
                current_actor, _ = live.native.read(script)
                validate_native(current_actor, actor, anchor, player_index=before['player_index'])
                if current_actor['life'] != 0:
                    result['status'] = 'exact_actor_incapacitated_before_cqc_dispatch'
                    break
                state, _ = cqc_state(live, anchor, healthy_life, transition=index > 0)
                _close_position(current_actor['position'], _position(state))
                result['view_target_angle_degrees'] = _facing(state['rendered']['native_pose'], current_actor['position'])
                floor = state['controls']['sample_ms']; sampled = []
                started = clock()
                try:
                    live.input([channel('right_trigger')], pulse_seconds, lease_seconds=pulse_seconds)
                    while clock()-started < pulse_seconds:
                        current = live.observe(native=False)
                        controls, _ = _validate_xr_field_state(current)
                        frame = current.get('rendered') or {}
                        if (current.get('activation'), frame.get('player_owner'), frame.get('presentation_epoch')) != tuple(anchor[2:]):
                            raise BotFault('CQC VR owner changed during trigger hold')
                        if (type(frame.get('sample_ms')) is not int or not 0 <= current['now_ms']-frame['sample_ms'] <= 150 or
                                any(frame.get('raw_right_'+kind+'_tracked') is not True for kind in ('grip', 'aim'))):
                            raise BotFault('CQC tracking publication changed during trigger hold')
                        audit = cqc_packet(current, after_sample_ms=floor)
                        if audit is not None:
                            floor = audit['sample_ms']; sampled.append(audit)
                        if sampled: break
                        sleep(.015)
                    if not sampled:
                        raise BotFault('CQC trigger had no native XR packet; no ambiguous replay')
                    # The native read may outlive the pulse: the independent
                    # short lease limits input; no capture RPC occurs held.
                    during, timing = live.native.read(script)
                    validate_native(during, actor, anchor, player_index=before['player_index'])
                    observed.append(during)
                    remainder = pulse_seconds-(clock()-started)
                    if remainder > 0: sleep(remainder)
                finally:
                    live.release()
                result['pulses'].append({'index': index+1, 'seconds': pulse_seconds,
                    'experimental_duration': True, 'sampled_packets': sampled, 'native': during, 'transport': timing})
                live.events.emit('cqc_pulse_observed', target_id=actor['id'], **result['pulses'][-1])
                if during['life'] != 0: break
                if index+1 < pulses: sleep(gap_seconds)
            release = wait_released(live, script, actor, anchor, before['player_index'], clock=clock, sleep=sleep)
            state, after = release['state'], release['native']
            if state is None or after is None:
                raise BotFault('Native CQC release could not be observed')
            result['release'] = release
            after_ammo = {k: state['native'].get('ammo_in_weapon_'+k) for k in baseline_ammo}
            result.update(classify(before, observed, after, ammo_unchanged=baseline_ammo == after_ammo))
            result['ordinary_field_resumed'] = release['released']
            if not release['released']:
                result['observed_status_before_release_timeout'] = result['status']
                result['status'] = 'native_cqc_release_pending'
            if healthy_life is not None:
                result['needs_health_recovery'] = needs_recovery(state['native'], healthy_life)
            result['after'] = after; result['ammo_before'] = baseline_ammo; result['ammo_after'] = after_ammo
            result['captures'] += _checkpoint(live, 'cqc-released', state)
            live.events.emit('cqc_outcome', **{k: v for k, v in result.items() if k not in ('captures', 'pulses')})
    except Exception as error:
        result['status'] = 'failed'; result['error'] = str(error)
    finally:
        try: live.release()
        except Exception as error:
            result['status'] = 'failed'; result['release_error'] = str(error)
        atomic_json(output/'result.json', result)
    return result
