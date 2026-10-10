"""One current-camp CQC and body action using the existing physical VR handlers.

This case owns no game lifecycle or fixture reset. It uses a fresh complete
soldier roster, native mapped approach and independently observed outcomes.
An isolated proximity target is not proof of an unseen approach or back angle.
The body destination is a mapped placement, not inferred AI concealment.
"""
import math
import time
from collections import deque
from pathlib import Path

from .bodies import actor_identity, guarded_enemies, handle_body, life, point
from .combat import admit
from .core import BotFault, atomic_json
from .cqc import attempt_cqc, exact_close_target
from .health import needs_recovery
from .locomotion import _position
from .perception import read_enemy_snapshot
from .route import navigate, set_posture


def camp_roster(snapshot, camp, anchor):
    if ((snapshot.get('mission'), snapshot.get('location')) != tuple(anchor[:2]) or
            snapshot.get('truncated') is not False or snapshot.get('unavailable_loaded') != 0 or
            life(snapshot.get('normal_life')) != 0 or not isinstance(camp, str) or not camp):
        raise BotFault('Direct camp action requires a complete same-world native roster')
    rows = snapshot.get('soldiers')
    if not isinstance(rows, list):
        raise BotFault('Current native soldiers are unavailable')
    names, ids, result = set(), set(), []
    for row in rows:
        name, object_id = actor_identity(row)
        if name in names or object_id in ids:
            raise BotFault('Current camp actor identity is ambiguous')
        names.add(name); ids.add(object_id)
        validated = {**row, 'position': point(row.get('position'))}
        life(row.get('life'))
        if row.get('camp') == camp:
            result.append(validated)
    if not 1 <= len(result) <= 16:
        raise BotFault('Direct action requires a bounded actual camp roster')
    return result


def select_guard(snapshot, camp, anchor, player, *, radius=45.):
    """Select an awake exact guard; sleeping bodies never become CQC targets."""
    player = point(player)
    roster = camp_roster(snapshot, camp, anchor)
    active = [row for row in roster if life(row['life']) == 0 and
              math.dist(player, row['position']) <= radius]
    isolated = []
    for actor in active:
        a = actor['position']
        if all(row['id'] == actor['id'] or life(row['life']) != 0 or
               math.hypot(row['position'][0]-a[0], row['position'][2]-a[2]) > 2.5 or
               abs(row['position'][1]-a[1]) > 1.5 for row in snapshot['soldiers']):
            isolated.append(actor)
    return min(isolated, key=lambda row: (math.dist(player, row['position']), row['id']), default=None)


def placement_node(nav, body_position, *, check_abort=None):
    """Find one connected nearby floor node. Distance does not imply cover."""
    body_position = point(body_position)
    start = nav.nearest(body_position, max_distance=1.25)
    queue, seen = deque([start]), {start}
    while queue:
        if check_abort is not None: check_abort()
        node = queue.popleft()
        position = nav.positions[node]
        distance = math.dist(body_position, position)
        if 3. <= distance <= 6. and abs(body_position[1]-position[1]) <= 1.5:
            return node, list(position)
        for target in nav.adj.get(node, ()):
            if target not in seen and math.dist(body_position, nav.positions[target]) <= 8.:
                seen.add(target); queue.append(target)
    raise BotFault('No connected nearby mapped body-placement node was found')


def run_camp_cqc_body(live, nav, *, camp, enemy_script, output, anchor=None,
                      placement_goal=None, max_seconds=120.,
                      clock=time.monotonic, sleep=time.sleep):
    """Current field only: one approach, one CQC chain and one body transaction.

    Root owns the live input generation and recording. This helper neither
    loads a save nor starts/stops a process. Existing CQC/body handlers retain
    their native outcome gates and independently expiring input leases.
    """
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'objective': 'current-camp-cqc-pickup-carry-drop',
              'map_identity': nav.identity, 'stages': [],
              'native_player_to_actor_link_verified': False,
              'behind_guard_or_unseen_approach_proven': False,
              'enemy_ai_concealment_proven': False,
              'visual_acceptance': 'pending_recorded_player_eye_review'}
    started = clock()
    try:
        if (type(max_seconds) not in (int, float) or not math.isfinite(max_seconds) or
                not 30. <= max_seconds <= 180.):
            raise BotFault('Direct camp case requires a bounded 30..180 second duration')
        live.release()
        state, anchor = admit(live, anchor)
        healthy_life = state['native']['player_life']
        result['anchor'] = anchor; result['before'] = state

        def abort():
            if (clock()-started >= max_seconds or (output/'STOP').exists() or
                    (live.events.output/'STOP').exists()):
                raise BotFault('Direct camp action stopped or reached its bounded deadline')

        def checkpoint():
            abort()
            current, _ = admit(live, anchor)
            if needs_recovery(current['native'], healthy_life):
                raise BotFault('Health recovery interrupts the direct camp action')
            if current['native'].get('not_alert') is not True:
                raise BotFault('Detection interrupts this stealth CQC/body case')
            return current

        def enemies():
            checkpoint()
            snapshot = read_enemy_snapshot(live.native.read, enemy_script)
            camp_roster(snapshot, camp, anchor)
            checkpoint()
            return snapshot

        snapshot = enemies(); state = checkpoint()
        result['roster_before'] = camp_roster(snapshot, camp, anchor)
        actor = select_guard(snapshot, camp, anchor, _position(state))
        if actor is not None:
            result['cqc_target'] = actor
            end = nav.nearest(actor['position'], max_distance=1.75)
            goal = list(nav.positions[end])
            if (math.hypot(goal[0]-actor['position'][0], goal[2]-actor['position'][2]) > 1. or
                    abs(goal[1]-actor['position'][1]) > 1.5):
                raise BotFault('The native floor cannot support the close CQC destination')
            nav.path(nav.nearest(_position(state)), end, check_abort=abort)
            checkpoint()
            if math.hypot(_position(state)[0]-actor['position'][0],
                          _position(state)[2]-actor['position'][2]) > 1.25:
                route_output = output/'cqc-approach'; route_output.mkdir(exist_ok=True)
                route = navigate(live, nav, goal, output=route_output, enemy_reader=enemies,
                    mode='stealth', max_seconds=min(60., max_seconds-(clock()-started)-10.),
                    expected_location=anchor[1], arrival_radius=.75, recenter=False,
                    interrupt_on_danger=True, known_battle=False)
                result['stages'].append({'kind': 'cqc_approach', 'status': route['status'],
                                         'result': str(route_output/'result.json')})
                if route['status'] != 'observed_native_route':
                    raise BotFault(route.get('error', 'Direct CQC approach did not arrive'))
            set_posture(live, 'crouch', sleep=sleep)
            snapshot = enemies(); state = checkpoint()
            actor = exact_close_target(snapshot, actor, anchor, _position(state))
            action = attempt_cqc(live, actor=actor, anchor=anchor, enemy_script=enemy_script,
                output=output/'cqc', healthy_life=healthy_life, pulses=5, pulse_seconds=.20,
                clock=clock, sleep=sleep)
            result['cqc'] = action
            result['stages'].append({'kind': 'cqc', 'status': action['status'],
                                     'result': str(output/'cqc'/'result.json')})
            if action.get('ordinary_field_resumed') is not True:
                raise BotFault('CQC did not release ordinary field ownership; no body action follows')
            if action.get('needs_health_recovery') is True:
                raise BotFault('Health recovery takes priority after CQC')
            if action['status'] == 'failed':
                raise BotFault(action.get('error', 'Direct CQC dispatch failed'))
        else:
            result['cqc_unavailable'] = 'No isolated awake exact camp guard within 45m'
            live.events.emit('direct_camp_cqc_unavailable', reason=result['cqc_unavailable'],
                             roster=result['roster_before'])

        snapshot = enemies(); state = checkpoint()
        roster = camp_roster(snapshot, camp, anchor)
        bodies = sorted((row for row in roster if life(row['life']) in (3, 4)),
                        key=lambda row: (math.dist(_position(state), row['position']), row['id']))
        # The actual chosen CQC actor has priority if incapacitation was observed.
        if actor is not None:
            bodies.sort(key=lambda row: row['id'] != actor['id'])
        selected = bodies[0] if bodies else None
        if selected is None:
            raise BotFault('No actual sleeping/fainted body is available after the current action')
        selected, _ = guarded_enemies(snapshot, selected, camp, anchor, _position(state))
        if placement_goal is None:
            node, goal = placement_node(nav, selected['position'], check_abort=abort)
        else:
            goal = point(placement_goal); node = nav.nearest(goal)
            if math.dist(goal, nav.positions[node]) > .002:
                raise BotFault('Explicit body destination is not an exact mapped floor node')
        result['body_target'] = selected
        result['placement'] = {'node': node, 'position': goal,
                               'sources': nav.sources, 'concealment_proven': False}
        remaining = max_seconds-(clock()-started)
        if remaining < 5.:
            raise BotFault('The bounded case has no remaining time for body handling')
        action = handle_body(live, nav, actor=selected, camp=camp, enemy_script=enemy_script,
            output=output/'body', cover_goal=goal, anchor=anchor, max_seconds=min(90., remaining))
        result['body'] = action
        result['stages'].append({'kind': 'body', 'status': action['status'],
                                 'result': str(output/'body'/'result.json')})
        if action['status'] == 'observed_native_roster_candidate_pickup_carry_drop':
            result['status'] = ('observed_exact_actor_incapacitation_during_cqc_input_and_body_displacement'
                if result.get('cqc', {}).get('status') ==
                   'observed_exact_actor_incapacitation_during_vr_cqc_input' and
                   result['cqc'].get('pulses') and
                   selected['id'] == result['cqc'].get('target_id')
                else 'observed_current_camp_body_displacement')
        else:
            result['status'] = action['status']
            if action.get('error'): result['error'] = action['error']
    except Exception as error:
        result['status'] = 'failed'; result['error'] = str(error)
    finally:
        try: live.release()
        except Exception as error:
            result['status'] = 'failed'; result['release_error'] = str(error)
        result['seconds'] = clock()-started
        atomic_json(output/'result.json', result)
    return result
