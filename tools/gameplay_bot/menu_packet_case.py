"""One exact physical VR menu button on the owning runner's prepared fixture.

This executes an input experiment, not a guessed menu operation. Page/cursor
effects remain recorded player-eye evidence. An unexpected transition releases
the button immediately and can never authorize a second confirm or replay.
"""
import math
import time
from pathlib import Path

from .combat import fresh_rig
from .core import BotFault, atomic_json
from .idroid_cases import MENU_BUTTONS
from .live import authoritative_controls, channel, held_input_evidence
from .locomotion import _neutral_audit


PHYSICAL_INDEX = {'a': 0, 'b': 1, 'x': 2, 'y': 3, 'left_stick_click': 5,
                  'right_stick_click': 6, 'left_grip': 7, 'right_grip': 8,
                  'left_trigger': 9, 'right_trigger': 10}


def menu_button_binding(bindings, token):
    if token not in MENU_BUTTONS:
        raise BotFault('The bounded menu case requires one named physical VR button')
    action, mask = MENU_BUTTONS[token]
    rows = [r for r in bindings.get('actions', []) if r.get('name') == action]
    if (len(rows) != 1 or rows[0].get('contexts') != ['menus'] or
            len(rows[0].get('bindings', [])) != 1 or
            rows[0]['bindings'][0].get('inputs') != [token] or
            rows[0]['bindings'][0].get('gesture') != 'level'):
        raise BotFault('Menu packet case differs from the exact effective physical binding')
    return action, mask


def menu_button_packet(state, token, mask, *, after_sample_ms):
    controls = authoritative_controls(state)
    evidence = held_input_evidence(state, [token], after_sample_ms=after_sample_ms)
    if evidence is None: return None
    triggers = [255 if token == 'left_trigger' else 0, 255 if token == 'right_trigger' else 0]
    if (controls.get('context') != 'menus' or state.get('gamepad') is not False or
            controls.get('native_buttons') != (mask or 0) or
            controls.get('native_triggers') != triggers or controls.get('native_axes') != [0, 0, 0, 0] or
            any(abs(v) > .09 for i, v in enumerate(controls['physical'][:11]) if i != PHYSICAL_INDEX[token]) or
            any(abs(v) > .09 for v in controls['sticks'])):
        raise BotFault('Menu button produced a different physical/native packet')
    return evidence


def _surface(state):
    n = state.get('native') or {}
    return {'scene': state.get('scene'), 'menu': state.get('menu'), 'idroid': state.get('idroid'),
            'pause': state.get('pause'), 'loading': state.get('loading'), 'popup': n.get('popup'),
            'tutorial_pause': n.get('tutorial_pause'), 'tutorial_mode': state.get('idroid_tutorial_mode'),
            'native_binocular': n.get('status_BINOCLE')}


def run_menu_button_case(live, *, token, fixture_label, output, own_existing_idroid=False,
                         seconds=.12, clock=time.monotonic, sleep=time.sleep):
    """No lifecycle/reset. Root supplies and owns a current recorded menu fixture."""
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    result = {'status': 'running', 'case': 'current-menu-one-physical-button', 'token': token,
              'fixture_label': fixture_label, 'input_attempts': 0, 'samples': [], 'captures': [],
              'page_or_cursor_effect_proven': False, 'full_menu_acceptance': False,
              'visual_acceptance': 'pending_recorded_player_eye_effect_review'}
    owns_menu = False
    try:
        action, mask = menu_button_binding(live.bindings, token)
        if (not isinstance(fixture_label, str) or not 1 <= len(fixture_label) <= 120 or
                type(own_existing_idroid) is not bool or
                type(seconds) not in (int, float) or not math.isfinite(seconds) or not .08 <= seconds <= .20):
            raise BotFault('Menu packet case needs an explicit prepared fixture and one .08..0.20s pulse')
        if getattr(live, 'held_action_recording', None) is None:
            raise BotFault('Menu button case requires continuous player-eye action recording')
        live.release(); before, anchor = fresh_rig(live)
        controls = authoritative_controls(before); _neutral_audit(controls)
        native = before['native']
        if (controls.get('context') != 'menus' or before.get('menu') is not True or
                before.get('idroid') is not True or before.get('pause') is not False or
                before.get('loading') is not False or before.get('gamepad') is not False or
                native.get('popup') is not False or native.get('saving') is not False or
                native.get('demo') is not False or native.get('game_over') != 0 or
                native.get('status_BINOCLE') is not False or not own_existing_idroid):
            raise BotFault('One-button case requires a root-owned current iDroid menu without another popup')
        owns_menu = True; live.opened_menu = 'idroid'
        result['before'] = before; result['anchor'] = anchor; result['action'] = action
        result['captures'] += live.capture('menu-'+token+'-before')
        current, _ = fresh_rig(live, anchor); _neutral_audit(current['controls'])
        if _surface(current) != _surface(before):
            raise BotFault('Prepared menu fixture changed during its before-eye capture')
        floor = authoritative_controls(current)['sample_ms']; until = clock()+seconds
        result['input_attempts'] = 1
        try:
            live.input([channel(token)], seconds, lease_seconds=seconds)
            while clock() < until:
                if (output/'STOP').exists() or (live.events.output/'STOP').exists():
                    raise BotFault('Menu button case stopped before sampling')
                sampled, _ = fresh_rig(live, anchor, clock=clock, sleep=sleep)
                if authoritative_controls(sampled).get('context') != 'menus':
                    result['observed_transition_while_sampled'] = _surface(sampled)
                    break
                packet = menu_button_packet(sampled, token, mask, after_sample_ms=floor)
                if sampled['native'].get('status_BINOCLE') is not False:
                    raise BotFault('Native binocular mode leaked during the physical menu button')
                if packet is not None:
                    result['samples'].append({'state': sampled, 'packet': packet})
                    live.events.emit('menu_one_button_sample', token=token, action=action,
                                     state=sampled, packet=packet)
                    floor = packet['sample_ms']
                if _surface(sampled) != _surface(before):
                    result['observed_transition_while_sampled'] = _surface(sampled)
                    break
                sleep(.01)
        finally: live.release()
        if not result['samples']:
            raise BotFault('The one menu button had no witnessed native packet; no replay')
        after, _ = fresh_rig(live, anchor)
        result['after'] = after
        result['surface_before'] = _surface(before); result['surface_after'] = _surface(after)
        result['captures'] += live.capture('menu-'+token+'-released')
        result['status'] = ('observed_native_menu_button_and_surface_transition'
            if result['surface_before'] != result['surface_after']
            else 'observed_native_menu_button_same_surface')
        result['native_binocular_mode_during_sampled_button'] = False
    except Exception as error:
        result['status'] = 'failed'; result['error'] = str(error)
    finally:
        try:
            live.release()
            if owns_menu:
                live.cleanup_menus(); result['owned_idroid_cleanup_requested'] = True
        except Exception as error:
            result['status'] = 'failed'; result['cleanup_error'] = str(error)
        atomic_json(output/'result.json', result)
    return result
