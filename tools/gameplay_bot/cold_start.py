"""Bounded first-launch navigation, enabled only by explicit agreement policy."""
import time

from .core import BlankCompositorFrame, BotFault, matches
from .startup import capture_released_startup_transition, logged_in_notice


def advance_cold_start(live, policy, timeout):
    if policy.get('accept_first_launch_agreements') is not True:
        raise BotFault('Cold-start agreement navigation requires explicit authorized policy')
    if not 0 < timeout <= 600:
        raise BotFault('Cold-start observation must be bounded to ten minutes')
    deadline = time.monotonic() + timeout
    notices, confirmations, phases = 0, 0, []
    previous, next_input = None, 0.
    while time.monotonic() < deadline:
        state = live.observe(native=True)
        native, terms = state.get('native', {}), state.get('terms_menu', {})
        sequence = native.get('sequence')
        key = (sequence, terms.get('mode'), terms.get('vertical'), terms.get('horizontal'))
        if key != previous:
            live.events.emit('cold_start_stage', state=state)
            phases.append({'sequence': sequence, 'terms_mode': terms.get('mode'),
                           'terms_row': terms.get('vertical')})
            previous = key
        if (native.get('mission') == 10010 and sequence == 'Seq_Game_TitleMenu'
                and state.get('title_menu') is True):
            return {'status': 'observed_cold_title', 'state': state, 'phases': phases,
                    'captures': live.capture('cold-title-menu'), 'agreements_confirmed': confirmations,
                    'owned_content_notices': notices, 'prologue_complete': False}
        # A real opening camera is the first milestone, not prologue completion.
        if (native.get('mission') == 10010 and isinstance(sequence, str)
                and (sequence.startswith('Seq_Game_') or state.get('opening_cinematic') is True)
                and state.get('camera_active') is True and not policy.get('wait_for_title_menu')):
            try:
                captures = live.capture('cold-opening')
            except BlankCompositorFrame:
                # The observed authored opening starts with a retail fade.
                # Keep observing this exact owner; never replay an input or
                # count a blank image as scene readiness.
                current = live.observe(native=True)
                if (current.get('opening_cinematic') is not True
                        or current.get('camera_active') is not True
                        or current.get('native', {}).get('mission') != 10010
                        or current.get('native', {}).get('demo') is not True):
                    raise
                time.sleep(.25)
                continue
            return {'status': 'observed_cold_opening', 'state': state, 'phases': phases,
                    'captures': captures, 'agreements_confirmed': confirmations,
                    'owned_content_notices': notices, 'prologue_complete': False}
        if native.get('mission') not in (1, 10010) or native.get('story') != 0:
            raise BotFault('Cold-start navigation lost its fresh campaign owner')
        if time.monotonic() < next_input:
            time.sleep(.1)
            continue
        step = None
        guard = {'mission': native['mission'], 'story': 0, 'sequence': sequence,
                 'popup': native.get('popup'), 'saving': False}
        popup = state.get('popup_observer') or {}
        if (sequence == 'Seq_Demo_ConfirmAutoSave' and native.get('popup') is True
                and matches(popup, {'reader_verified': True, 'owner_verified': True,
                    'active': True, 'numeric_id': 3, 'string_id': '0xaef1c53b3767'})):
            step = {'op': 'action', 'name': 'menus.confirm', 'seconds': .3}
        elif (sequence == 'Seq_Demo_ShowDlcAnnouncePopup' and native.get('popup') is True
                and matches(popup, {'reader_verified': True, 'owner_verified': True,
                    'active': True, 'numeric_id': 3, 'string_id': '0x0',
                    'choice_owner_verified': True, 'current_choice_index': 1,
                    'current_choice_negative': False})):
            if notices >= 64:
                raise BotFault('Owned-content notice bound reached without leaving native announcements')
            step = {'op': 'action', 'name': 'menus.confirm', 'seconds': .3}
            step['state_before'] = {'popup_observer.' + k: popup[k] for k in (
                'reader_verified', 'owner_verified', 'active', 'numeric_id', 'string_id',
                'choice_owner_verified', 'current_choice_index', 'current_choice_negative')}
            notices += 1
        elif sequence == 'Seq_Demo_LogInKonamiServer' and logged_in_notice(state):
            step = {'op': 'action', 'name': 'menus.confirm', 'seconds': .3}
        elif (sequence == 'Seq_Demo_StartHasTitleMission' and native.get('mission') == 10010
                and native.get('title') is True and native.get('title_shown') is True
                and native.get('title_started') is True and native.get('demo') is False
                and state.get('opening_cinematic') is False and state.get('title_menu') is False):
            # The first-launch hospital prompt bypasses the ordinary title
            # Show hook. Its two eyes and native ShowTitle state were reviewed.
            guard.update(title=True, title_shown=True, title_started=True, demo=False)
            step = {'op': 'action', 'name': 'system.idroid', 'seconds': .15,
                    'state_before': {'title': True, 'title_menu': False, 'opening_cinematic': False}}
        elif (sequence == 'Seq_Demo_LogInKonamiServer' and native.get('popup') is False
                and state.get('controls', {}).get('context') == 'menus'
                and matches(terms, {'verified': True, 'active': True, 'ready': True, 'phase': 5})
                and type(terms.get('owner')) is int and terms['owner'] > 0
                and type(terms.get('sample_ms')) is int
                and 0 <= state['now_ms'] - terms['sample_ms'] <= 250):
            mode, row, action = terms.get('mode'), terms.get('vertical'), terms.get('action')
            if mode in (3, 4, 8):
                final_row = 2 if mode == 8 else 1
                if type(row) is not int or not 0 <= row <= final_row:
                    raise BotFault('Unknown native agreement selection')
                if row < final_row and action in (2, 3):
                    step = {'op': 'menu_navigate', 'owner': 'terms', 'direction': 'down', 'seconds': .15}
                elif row == final_row and action == 1 and terms.get('horizontal') == 1:
                    step = {'op': 'menu_navigate', 'owner': 'terms', 'direction': 'left', 'seconds': .15}
                elif row == final_row and action == 0 and terms.get('horizontal') == 0:
                    step = {'op': 'action', 'name': 'menus.confirm', 'seconds': .3}
                    confirmations += 1
            elif mode == 2 and row == 0 and terms.get('horizontal') == 1 and action == 1:
                step = {'op': 'action', 'name': 'menus.confirm', 'seconds': .3}
            if step:
                step['state_before'] = {'terms_menu.' + k: terms[k] for k in (
                    'verified', 'active', 'ready', 'phase', 'mode', 'horizontal', 'vertical', 'action')}
        if step:
            live.capture('cold-before-' + str(len(phases)) + '-' + str(notices))
            live.execute({**step, 'native_before': guard})
            live.release()
            next_input = time.monotonic() + .5
        else:
            time.sleep(.1)
    capture_released_startup_transition(live, 'cold-start-deadline', timeout=10.)
    raise BotFault('Cold-start deadline at ' + str(sequence))
