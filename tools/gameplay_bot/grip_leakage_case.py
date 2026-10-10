"""Current field, physical-binocular or iDroid grip isolation through VR input.

No fixture/menu/optic is opened. This bounded case measures the current effective
roles and native packets under left, right and two grips; it cannot prove visual
grain absence without the owning runner's continuous player-eye recording.
"""
import math
import time
from pathlib import Path

from .combat import fresh_rig
from .core import BotFault, atomic_json
from .health import needs_recovery
from .live import authoritative_controls, channel, held_input_evidence
from .locomotion import _neutral_audit, _position


def grip_roles(bindings, context):
    expected = {
        'gameplay': {'gameplay.support_grip': 'left_grip', 'gameplay.ready_weapon': 'right_grip'},
        'binoculars': {'binoculars.support_grip': 'left_grip'},
        'menus': {'menus.previous_tab': 'left_grip', 'menus.next_tab': 'right_grip'},
    }.get(context)
    if expected is None: raise BotFault('Grip isolation requires a current field/physical-binocular/iDroid context')
    found = {}
    for row in bindings.get('actions', []):
        if context not in row.get('contexts', []): continue
        for binding in row.get('bindings', []):
            tokens = binding.get('inputs', [])
            if not tokens or not set(tokens) <= {'left_grip', 'right_grip'}: continue
            if (row.get('name') not in expected or tokens != [expected[row['name']]] or
                    binding.get('gesture') != 'level' or row['name'] in found):
                raise BotFault('Effective grip chord has another action owner; no isolation input dispatched')
            found[row['name']] = tokens[0]
    if found != expected:
        raise BotFault('Current grip roles differ from the source-backed physical VR mapping')
    return found


def grip_packet(state, context, tokens, *, after_sample_ms):
    controls = authoritative_controls(state)
    evidence = held_input_evidence(state, tokens, after_sample_ms=after_sample_ms)
    if evidence is None: return None
    expected_buttons = ((0x100 if 'left_grip' in tokens else 0)|
                        (0x200 if 'right_grip' in tokens else 0)) if context == 'menus' else 0
    expected_triggers = [255 if context == 'gameplay' and 'right_grip' in tokens else 0, 0]
    allowed = {7 if t == 'left_grip' else 8 for t in tokens}
    if (controls.get('context') != context or state.get('gamepad') is not False or
            controls.get('native_buttons') != expected_buttons or
            controls.get('native_triggers') != expected_triggers or
            controls.get('native_axes') != [0, 0, 0, 0] or
            any(abs(v) > .09 for i, v in enumerate(controls['physical'][:11]) if i not in allowed) or
            any(abs(v) > .09 for v in controls['sticks'])):
        raise BotFault('Grip input leaked or changed its expected physical/native packet')
    if state.get('native', {}).get('status_BINOCLE') is not False:
        raise BotFault('Native binocular camera mode leaked under the physical grip')
    return evidence


def run_current_grip_leakage(live, *, output, anchor=None, seconds=.60,
                              clock=time.monotonic, sleep=time.sleep):
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'case': 'current-vr-left-right-both-grip-isolation',
              'phases': [], 'captures': [], 'fixture_setup_changed_by_case': False,
              'menu_page_effect_visual_acceptance': 'pending_if_menu_context',
              'visual_grain_absence_proven': False, 'physical_headset_acceptance': False}
    started = clock()
    try:
        if (type(seconds) not in (int, float) or not math.isfinite(seconds) or not .35 <= seconds <= 1.):
            raise BotFault('Grip isolation requires a .35..1 second independent lease')
        if getattr(live, 'held_action_recording', None) is None:
            raise BotFault('Grip isolation requires continuous player-eye action recording')
        live.release(); before, anchor = fresh_rig(live, anchor)
        controls = authoritative_controls(before); _neutral_audit(controls)
        context = controls['context']; roles = grip_roles(live.bindings, context)
        baseline = before['native']['player_life']; origin = _position(before)
        result.update(before=before, anchor=anchor, context=context, effective_grip_roles=roles)

        def checkpoint():
            if clock()-started >= 15. or (output/'STOP').exists() or (live.events.output/'STOP').exists():
                raise BotFault('Current grip isolation stopped or exceeded its 15-second bound')
            current, _ = fresh_rig(live, anchor, clock=clock, sleep=sleep)
            c = authoritative_controls(current); n = current['native']; f = current['rendered']
            if (c['context'] != context or n.get('status_BINOCLE') is not False or
                    n.get('popup') is not False or n.get('saving') is not False or
                    n.get('demo') is not False or n.get('game_over') != 0 or
                    n.get('status_CARRY') is not False or n.get('not_alert') is not True or
                    needs_recovery(n, baseline) or math.dist(_position(current), origin) > .05):
                raise BotFault('Current grip fixture changed, moved, detected danger or entered native binocular mode')
            if context == 'menus':
                if current.get('menu') is not True or current.get('idroid') is not True:
                    raise BotFault('Grip tab probe lost its existing physical iDroid owner')
            elif current.get('menu') is not False or current.get('idroid') is not False:
                raise BotFault('Grip probe unexpectedly opened another menu')
            if context == 'binoculars' and f.get('binocular_held') is not True:
                raise BotFault('Grip probe lost the actual hand-held physical binocular')
            if context == 'gameplay' and n.get('status_NORMAL_ACTION') is not True:
                raise BotFault('Ordinary field grip probe cannot claim an unknown stationary emplacement owner')
            return current

        checkpoint(); result['captures'] += live.capture('grip-isolation-current-before')
        for tokens in (['left_grip'], ['right_grip'], ['left_grip', 'right_grip']):
            pre = checkpoint(); _neutral_audit(pre['controls'])
            floor = pre['controls']['sample_ms']; phase = {'tokens': tokens, 'samples': []}
            end = clock()+seconds
            try:
                live.input([channel(t) for t in tokens], seconds, lease_seconds=seconds)
                while clock() < end:
                    current = checkpoint()
                    observed = grip_packet(current, context, tokens, after_sample_ms=floor)
                    if observed is not None:
                        floor = observed['sample_ms']
                        phase['samples'].append({'state': current, 'packet': observed})
                        live.events.emit('current_grip_native_isolation', context=context,
                                         tokens=tokens, state=current, packet=observed)
                    sleep(.01)
            finally: live.release()
            if len(phase['samples']) < 2:
                raise BotFault('Two distinct held native grip samples were not witnessed; no replay')
            phase['after'] = checkpoint(); _neutral_audit(phase['after']['controls'])
            result['phases'].append(phase)
        result['after'] = checkpoint(); result['captures'] += live.capture('grip-isolation-current-released')
        result['status'] = 'observed_current_grip_packets_without_native_binocular_mode'
    except Exception as error:
        result['status'] = 'failed'; result['error'] = str(error)
    finally:
        try: live.release()
        except Exception as error:
            result['status'] = 'failed'; result['release_error'] = str(error)
        result['seconds'] = clock()-started
        atomic_json(output/'result.json', result)
    return result
