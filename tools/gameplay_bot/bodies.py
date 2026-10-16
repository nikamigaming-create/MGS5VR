"""One source-bound pickup, mapped carry and drop experiment through VR B.

Local CARRY plus one exact roster actor's native carried state is a candidate
join, not a direct carrier-to-body getter. Captures remain pending human review.
No transform/status/world writes, CQC requests or concealed-body claims occur.
"""
import inspect
import json
import math
import re
import time
from pathlib import Path

from .combat import fresh_rig, looking_head
from .core import BotFault, atomic_json
from .health import HealthRecoveryNeeded, needs_recovery
from .live import authoritative_controls, channel, held_input_evidence
from .locomotion import _neutral_audit, _position, _validate_xr_field_state
from .navigation import arrived
from .perception import read_enemy_snapshot
from .route import navigate
from .session import preserved_tracking_poses


SOURCE_CONTRACT = {
    'archive_entry': '/Assets/tpp/script/lib/TppEnemy.lua',
    'sha256': '47a40c641ef1184ffeb073dc99ce5da85a47f1f68a33e94f56dde78de3084e43',
    'life_enums_line': 25, 'life_enums': {'NORMAL': 0, 'DEAD': 1, 'DYING': 2, 'SLEEP': 3, 'FAINT': 4},
    'status_wrappers_lines': [898, 919], 'soldier_status_caller_line': 2178,
    'carry_status': {'index': 134, 'registration_call': '0x140554b1f'},
    'native_enum_exe_sha256': '3fdc4ea24d616f7d05965c8ea8dda1e25a7872fbc94733c00ba4b65490255203',
    'native_carried_constant': 'TppGameObject.NPC_STATE_CARRIED; retain actual value',
    'owned_carried_equality': 's10040_radio.lua:346',
    'packet_source': 'input_bridge.cpp:23-33; xr_runtime.cpp:985-994',
}
PICKUP_SECONDS = .85  # One bounded experiment; not a sourced native hold duration.
# The exact injured hostage stayed carried after .12/.85-second native B
# packets; retained 20261016T160701943862Z observed the real put-down with
# this sustained hold. The caller still requires the actual actor outcome.
DROP_SECONDS = 2.2  # Bounded experiment, not a recovered native threshold.
MAX_ROSTER = 16


def point(value):
    if (not isinstance(value, (list, tuple)) or len(value) != 3 or
            any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1e6 for v in value)):
        raise BotFault('Body handling requires a finite actual native point')
    return list(value)


def life(value):
    # navigation-state.lua serializes native numeric life as a quoted scalar.
    if type(value) is str and value in ('0', '1', '2', '3', '4'):
        value = int(value)
    if type(value) is not int or value not in range(5):
        raise BotFault('Native soldier life is unavailable')
    return value


def actor_identity(actor):
    if (not isinstance(actor, dict) or not isinstance(actor.get('name'), str) or
            not re.fullmatch(r'sol_[A-Za-z0-9_]{1,120}', actor['name']) or
            type(actor.get('id')) is not int or not 0 < actor['id'] < 65535):
        raise BotFault('Body handling needs an exact owned soldier name/ID')
    return actor['name'], actor['id']


def pickup_binding(bindings):
    rows = [r for r in bindings.get('actions', []) if r.get('name') == 'gameplay.pickup_carry']
    if (len(rows) != 1 or rows[0].get('contexts') != ['gameplay'] or
            len(rows[0].get('bindings', [])) != 1 or
            rows[0]['bindings'][0].get('gesture') != 'level' or
            rows[0]['bindings'][0].get('inputs') != ['b']):
        raise BotFault('Body case requires the effective plain gameplay B pickup binding')
    return ['b']


def guarded_enemies(snapshot, actor, camp, anchor, player, expected_roster=None):
    if (snapshot.get('mission'), snapshot.get('location')) != tuple(anchor[:2]):
        raise BotFault('Body/enemy world changed')
    if snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded') != 0:
        raise BotFault('Body handling cannot use incomplete loaded enemy telemetry')
    soldiers = snapshot.get('soldiers')
    if not isinstance(soldiers, list) or not soldiers or life(snapshot.get('normal_life')) != 0:
        raise BotFault('Complete native soldier roster is unavailable')
    expected_name, expected_id = actor_identity(actor)
    expected_life = life(actor.get('life'))
    if expected_life not in (3, 4):
        raise BotFault('This bounded case admits a sleeping or fainted body only')
    selected = [r for r in soldiers if r.get('name') == expected_name and r.get('id') == expected_id]
    if len(selected) != 1:
        raise BotFault('Exact body is absent or ambiguous in the current roster')
    actual_body_position = point(selected[0].get('position'))
    names, ids, roster, chosen = set(), set(), [], None
    for row in soldiers:
        name, object_id = actor_identity(row)
        if name in names or object_id in ids:
            raise BotFault('Native roster identity is ambiguous')
        names.add(name); ids.add(object_id)
        position = point(row.get('position')); observed_life = life(row.get('life'))
        if observed_life == 0 and (row.get('camp') == camp or
                math.dist(position, player) <= 45. or math.dist(position, actual_body_position) <= 45.):
            raise BotFault('An active native guard remains in camp or within 45m of body handling')
        if row.get('camp') == camp:
            roster.append({'name': name, 'id': object_id, 'life': observed_life})
        if name == expected_name or object_id == expected_id:
            if (name, object_id, row.get('camp'), observed_life) != (
                    expected_name, expected_id, camp, expected_life):
                raise BotFault('Exact body life/name/ID/camp changed')
            chosen = {**row, 'position': position}
    if chosen is None or not 1 <= len(roster) <= MAX_ROSTER:
        raise BotFault('Exact body or bounded camp roster is unavailable')
    roster.sort(key=lambda r: r['id'])
    if expected_roster is not None and roster != expected_roster:
        raise BotFault('The witnessed camp body roster changed')
    return chosen, roster


def native_body_script(roster, anchor):
    if not 1 <= len(roster) <= MAX_ROSTER:
        raise BotFault('Body roster exceeds 1..16 native actors')
    if (not isinstance(anchor, (list, tuple)) or len(anchor) != 5 or
            any(type(v) is not int or v <= 0 for v in anchor)):
        raise BotFault('Body query requires an exact numeric world/VR owner')
    rows = []
    seen_names, seen_ids = set(), set()
    for row in roster:
        name, object_id = actor_identity(row)
        if name in seen_names or object_id in seen_ids:
            raise BotFault('Body query roster has duplicate identities')
        seen_names.add(name); seen_ids.add(object_id)
        rows.append("{name=" + json.dumps(name) + ",expected_id=" + str(object_id) + "}")
    return '''-- Bounded read-only CARRY / soldier status observer; no mutations.
local requested={''' + ','.join(rows) + '''}
assert(vars.missionCode==''' + str(anchor[0]) + ''' and vars.locationCode==''' + str(anchor[1]) + ''','body world changed')
assert(PlayerStatus.CARRY==134,'native CARRY enum changed')
assert(TppEnemy.LIFE_STATUS.SLEEP==3 and TppEnemy.LIFE_STATUS.FAINT==4,'owned life enums changed')
assert(type(TppGameObject.GAME_OBJECT_TYPE_SOLDIER2)=='number','native soldier type unavailable')
local carrying=PlayerInfo.AndCheckStatus{PlayerStatus.CARRY}
assert(type(carrying)=='boolean','native player CARRY unavailable')
local playerIndex=PlayerInfo.GetLocalPlayerIndex()
assert(type(playerIndex)=='number' and playerIndex%1==0,'native player index unavailable')
local carried=TppGameObject.NPC_STATE_CARRIED
local function scalar(v)
 if type(v)=='number' then assert(v==v and v~=math.huge and v~=-math.huge);return tostring(v) end
 assert(type(v)=='string' and #v<=128,'native status scalar unavailable')
 return '"'..v:gsub('[%z\\1-\\31\\\\"]',function(c)return string.format('\\\\u%04x',string.byte(c))end)..'"'
end
local constant=scalar(carried)
local output={}
for _,row in ipairs(requested) do
 local id=GameObject.GetGameObjectId('TppSoldier2',row.name)
 assert(id==row.expected_id and id~=GameObject.NULL_ID,'exact body ID changed')
 assert(GameObject.GetTypeIndex(id)==TppGameObject.GAME_OBJECT_TYPE_SOLDIER2,'native body type changed')
 local l=GameObject.SendCommand(id,{id='GetLifeStatus'})
 local a=GameObject.SendCommand(id,{id='GetActionStatus'})
 local s=GameObject.SendCommand(id,{id='GetStatus'})
 assert(type(l)=='number' and l%1==0,'native body life unavailable')
 output[#output+1]='{"id":'..id..',"life":'..l..',"action":'..scalar(a)..',"status":'..scalar(s)..',"carried":'..tostring(s==carried)..'}'
end
return '{"mission":'..vars.missionCode..',"location":'..vars.locationCode..',"player_index":'..playerIndex..',"player_carry":'..tostring(carrying)..',"carried_constant":'..constant..',"soldiers":['..table.concat(output,',')..']}'
'''


def carry_candidate(data, roster, actor, anchor):
    if (data.get('mission'), data.get('location')) != tuple(anchor[:2]) or type(data.get('player_carry')) is not bool:
        raise BotFault('Native carry observation lost its world or boolean state')
    if type(data.get('player_index')) is not int or data['player_index'] < 0:
        raise BotFault('Native carry local player is unavailable')
    constant = data.get('carried_constant')
    if type(constant) not in (int, str):
        raise BotFault('Exact native carried constant is unavailable')
    rows = data.get('soldiers')
    if not isinstance(rows, list) or len(rows) != len(roster):
        raise BotFault('Native carried roster is incomplete')
    expected = {r['id']: r for r in roster}; seen, matches = set(), []
    for row in rows:
        object_id = row.get('id')
        if type(object_id) is not int or object_id not in expected or object_id in seen:
            raise BotFault('Native carried actor identity changed')
        seen.add(object_id)
        if type(row.get('life')) is not int or row['life'] != expected[object_id]['life']:
            raise BotFault('Native body life changed during carry inspection')
        status = row.get('status')
        if type(status) not in (int, str) or (type(status) is str and len(status) > 128):
            raise BotFault('Native body status scalar is unavailable')
        match = type(status) is type(constant) and status == constant
        if type(row.get('carried')) is not bool or row['carried'] is not match:
            raise BotFault('Native carried classification disagrees with raw status')
        if match:
            matches.append(object_id)
    return {'chosen_candidate_carried': data['player_carry'] and matches == [actor['id']],
            'local_player_carry': data['player_carry'], 'matching_carried_ids': matches,
            'player_index': data['player_index'], 'native_player_to_actor_link_verified': False,
            'scope': 'unique exact native-carried candidate in owned camp roster; carrier link unverified'}


def body_state(live, anchor, healthy_life=None, *, transition=False):
    state, identity = fresh_rig(live, anchor)
    controls, _ = _validate_xr_field_state(state)
    native, frame = state['native'], state['rendered']
    if (native.get('player_vehicle_id') != 65535 or native.get('popup') is not False or
            native.get('saving') is not False or native.get('demo') is not False or
            native.get('demo_nonplayable') is not False or native.get('game_over') != 0 or
            native.get('title') is not False or type(native.get('status_CARRY')) is not bool or
            (not transition and native.get('status_NORMAL_ACTION') is not True and native['status_CARRY'] is not True) or
            frame.get('binocular_held') is not False):
        raise BotFault('Body interaction lost ordinary on-foot gameplay/optic ownership')
    if healthy_life is not None and needs_recovery(native, healthy_life):
        raise HealthRecoveryNeeded('Health recovery takes priority over body handling')
    return state, identity


def pickup_packet(state, *, after_sample_ms):
    audit = held_input_evidence(state, ['b'], after_sample_ms=after_sample_ms)
    if audit is None:
        return None
    controls = authoritative_controls(state)
    if (state.get('gamepad') is not False or controls['context'] != 'gameplay' or
            controls.get('rig_input') is not True or controls.get('travel_mode') != 1 or
            controls.get('native_buttons') != 0x2000 or controls.get('native_triggers') != [0, 0] or
            controls.get('native_axes') != [0, 0, 0, 0] or
            any(abs(v) > .09 for i, v in enumerate(controls['physical'][:11]) if i != 1) or
            any(abs(v) > .09 for v in controls['sticks'])):
        raise BotFault('Pickup pulse was not the witnessed B-only native 0x2000 packet')
    return audit


def pulse(live, anchor, seconds, label, *, clock=time.monotonic, sleep=time.sleep):
    live.release()
    state, _ = body_state(live, anchor, transition=True)
    _neutral_audit(state['controls'])
    floor, samples = state['controls']['sample_ms'], []
    end = clock() + seconds
    try:
        live.input([channel('b')], seconds, lease_seconds=seconds)
        while clock() < end:
            current = live.observe(native=False)
            _validate_xr_field_state(current)
            frame = current.get('rendered') or {}
            if (current.get('activation'), frame.get('player_owner'), frame.get('presentation_epoch')) != tuple(anchor[2:]):
                raise BotFault('Body pulse VR owner changed')
            observed = pickup_packet(current, after_sample_ms=floor)
            if observed is not None:
                floor = observed['sample_ms']; samples.append(observed)
            sleep(.02)
    finally:
        live.release()
    if not samples:
        raise BotFault('One B pulse had no sampled native packet; no ambiguous replay')
    result = {'seconds': seconds, 'experimental_duration': True, 'sampled_packets': samples,
              'input_acknowledgement_is_not_body_outcome': True}
    live.events.emit(label, **result)
    return result


def handle_body(live, nav, *, actor, camp, enemy_script, output, cover_goal, anchor, max_seconds=90):
    """One pickup/drop each. Root owns invocation, input lease and live cleanup."""
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'objective': 'one-native-body-pickup-mapped-carry-drop',
              'actor': actor, 'map_identity': nav.identity, 'source_contract': SOURCE_CONTRACT,
              'captures': [], 'observations': [], 'native_player_to_actor_link_verified': False,
              'visual_acceptance': 'pending_both_eye_hand_body_and_placement_review',
              'enemy_ai_concealment_proven': False, 'full_body_handling_acceptance': False}
    result['pickup_attempts'] = result['drop_attempts'] = 0
    started = time.monotonic(); initial_anchor = anchor; player_index = None; roster = None
    try:
        if type(max_seconds) not in (int, float) or not math.isfinite(max_seconds) or not 5 <= max_seconds <= 120:
            raise BotFault('Body case needs a bounded 5..120 second deadline')
        if 'carry_reader' not in inspect.signature(navigate).parameters:
            raise BotFault('Carry-safe walk route contract is not installed; no pickup dispatched')
        pickup_binding(live.bindings); actor_identity(actor); point(actor.get('position'))
        if not isinstance(camp, str) or actor.get('camp') != camp:
            raise BotFault('Exact owned body camp is required')
        goal = point(cover_goal)
        goal_node = nav.nearest(goal)
        if math.dist(goal, nav.positions[goal_node]) > .002 or not 1.5 <= math.dist(goal, actor['position']) <= 40:
            raise BotFault('Body destination must be an actual mapped node 1.5..40m from the body')
        result['placement_goal'] = goal; result['placement_node'] = goal_node
        with preserved_tracking_poses(live):
            live.release(); state, anchor = body_state(live, anchor)
            healthy_life = state['native']['player_life']; result['before'] = state
            if state['native']['status_CARRY'] is not False:
                raise BotFault('Already carrying an unwitnessed actor; no pickup dispatched')

            def checkpoint(*, transition=False):
                nonlocal player_index, roster
                if time.monotonic() - started >= max_seconds or (output/'STOP').exists() or (live.events.output/'STOP').exists():
                    raise BotFault('Body case stopped or reached its bounded deadline')
                current, _ = body_state(live, anchor, healthy_life, transition=transition)
                snapshot = read_enemy_snapshot(live.native.read, enemy_script)
                body, observed_roster = guarded_enemies(snapshot, actor, camp, anchor, _position(current), roster)
                if roster is None:
                    roster = observed_roster
                data, timing = live.native.read(native_body_script(roster, anchor))
                observed = carry_candidate(data, roster, actor, anchor)
                if player_index is not None and observed['player_index'] != player_index:
                    raise BotFault('Native local player identity changed during body handling')
                player_index = observed['player_index']
                record = {'native': data, 'candidate': observed, 'body': body, 'state': current,
                          'transport': timing, 'map_identity': nav.identity}
                result['observations'].append(record)
                live.events.emit('body_native_observation', **record)
                return current, body, observed

            state, body, _ = checkpoint()
            result['body_before'] = body
            nav.path(nav.nearest(_position(state)), nav.nearest(body['position'], max_distance=1.25))
            nav.path(nav.nearest(body['position'], max_distance=1.25), goal_node)

            def enemy_reader():
                snapshot = read_enemy_snapshot(live.native.read, enemy_script)
                current, _ = body_state(live, None, healthy_life, transition=True)
                if tuple((current['native']['mission'], current['native']['location'])) != tuple(anchor[:2]):
                    raise BotFault('Body route world changed')
                guarded_enemies(snapshot, actor, camp, anchor, _position(current), roster)
                return snapshot

            if math.dist(_position(state), body['position']) > 1.25:
                route_output = output/'approach'; route_output.mkdir()
                route = navigate(live, nav, body['position'], output=route_output, enemy_reader=enemy_reader,
                                 mode='walk', max_seconds=max(.001, max_seconds-(time.monotonic()-started)),
                                 expected_location=anchor[1], arrival_radius=1.0, recenter=False)
                result['approach_result'] = str(route_output/'result.json')
                if route['status'] != 'observed_native_route':
                    raise BotFault(route.get('error', 'Body approach did not arrive'))
                _, refreshed = body_state(live, None, healthy_life)
                if refreshed[:2] != anchor[:2] or refreshed[3:] != anchor[3:]:
                    raise BotFault('Body approach changed world/player owner/presentation epoch')
                anchor = refreshed
            state, body, candidate = checkpoint()
            if candidate['local_player_carry'] or math.dist(_position(state), body['position']) > 1.25:
                raise BotFault('Exact body is not within witnessed pickup distance or carry is already active')
            head = live.text_result(live.call('get_head_pose', {'base_space': 'local'}))['pose']
            head['orientation'] = looking_head(head['orientation'], state['rendered']['native_pose'], body['position'])
            live.call('set_head_pose', {'base_space': 'local', **head})
            state, body, candidate = checkpoint()
            result['captures'] += live.capture('body-before-pickup')
            state, body, candidate = checkpoint()
            if candidate['local_player_carry'] or math.dist(_position(state), body['position']) > 1.25:
                raise BotFault('Body changed during pre-pickup eye capture')
            result['pickup_attempts'] = 1
            result['pickup_pulse'] = pulse(live, anchor, PICKUP_SECONDS, 'body_pickup_pulse')

            def wait_transition(wanted):
                end = min(started+max_seconds, time.monotonic()+4.)
                while time.monotonic() < end:
                    current, chosen, witnessed = checkpoint(transition=True)
                    complete = (witnessed['chosen_candidate_carried'] if wanted else
                                not witnessed['local_player_carry'] and actor['id'] not in witnessed['matching_carried_ids'])
                    if complete:
                        return current, chosen, witnessed
                    time.sleep(.06)
                raise BotFault('One B pulse did not produce the required native body transition; no replay')

            state, body, candidate = wait_transition(True)
            result['pickup'] = {'state': state, 'body': body, 'candidate': candidate}
            result['captures'] += live.capture('body-native-carry-observed')

            def carry_reader():
                _, _, witnessed = checkpoint(transition=True)
                return witnessed['chosen_candidate_carried'] is True

            if not carry_reader():
                raise BotFault('Exact carried candidate was lost before movement')
            carry_output = output/'carry'; carry_output.mkdir()
            route = navigate(live, nav, goal, output=carry_output, enemy_reader=enemy_reader,
                             carry_reader=carry_reader, mode='walk',
                             max_seconds=max(.001, max_seconds-(time.monotonic()-started)),
                             expected_location=anchor[1], arrival_radius=1.0)
            result['carry_route_result'] = str(carry_output/'result.json')
            if route['status'] != 'observed_native_route' or not carry_reader():
                raise BotFault(route.get('error', 'Exact native carried candidate route did not arrive'))
            state, body, candidate = checkpoint(transition=True)
            player = _position(state)
            result['placement_arrival'] = {
                'player_root': player, 'nav_floor_goal': goal,
                'xz_distance_m': math.hypot(player[0]-goal[0], player[2]-goal[2]),
                'root_minus_floor_y_m': player[1]-goal[1],
                'distance_3d_m': math.dist(player, goal),
                'xz_radius_m': 1.25, 'max_height_m': 2.5,
                'scope': 'native player root to mapped supporting floor; not body pickup reach',
            }
            if not arrived(player, goal, radius=1.25, max_height=2.5):
                raise BotFault('Mapped body placement goal was not actually reached')
            result['before_drop'] = {'state': state, 'body': body, 'candidate': candidate}
            result['captures'] += live.capture('body-before-drop')
            if not carry_reader():
                raise BotFault('Exact carried candidate changed during drop eye capture')
            result['drop_attempts'] = 1
            result['drop_pulse'] = pulse(live, anchor, DROP_SECONDS, 'body_drop_pulse')
            state, body, candidate = wait_transition(False)
            result['drop'] = {'state': state, 'body': body, 'candidate': candidate}
            result['body_position_displacement_m'] = math.dist(result['body_before']['position'], body['position'])
            result['placement_to_player_distance_m'] = math.dist(_position(state), body['position'])
            result['captures'] += live.capture('body-after-drop')
            result['status'] = 'observed_native_roster_candidate_pickup_carry_drop'
            if result['body_position_displacement_m'] < 1.:
                result['status'] = 'observed_pickup_drop_without_proven_body_displacement'
    except Exception as error:
        result['status'] = 'failed'; result['error'] = str(error)
    finally:
        try:
            live.release()
        except Exception as error:
            result['cleanup_error'] = str(error); result['status'] = 'failed'
        result['initial_anchor'] = initial_anchor; result['final_anchor'] = anchor
        result['seconds'] = time.monotonic()-started
        atomic_json(output/'result.json', result)
    return result
