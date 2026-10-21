"""One reviewed cardinal Commands choice through the effective VR bindings.

Native RigCommands still owns menu readiness, selection and confirmation. This
executor records a delivered choice; callers must assert the actual response.
"""
import math
import time

from .core import BotFault, matches
from .live import channel, fresh_control_observation, held_input_evidence, sampled_release_keys
from .locomotion import _axis_source


def selection_inputs(bindings, direction):
    if direction not in ('up', 'down', 'left', 'right'):
        raise BotFault('Commands selection requires one reviewed cardinal choice')
    actions = {a['name']: a for a in bindings.get('actions', [])}
    opening, confirming = actions.get('commands.keep_open'), actions.get('commands.confirm')
    if (not opening or opening.get('modifier') is not True or not confirming
            or len(opening.get('bindings', [])) != 1 or len(confirming.get('bindings', [])) != 1
            or 'gameplay' not in opening.get('contexts', [])
            or 'commands' not in confirming.get('contexts', [])):
        raise BotFault('Effective on-foot Commands bindings are unavailable')
    hold, confirm = opening['bindings'][0], confirming['bindings'][0]
    if hold.get('gesture') != 'level' or confirm.get('gesture') != 'press':
        raise BotFault('Commands requires its held opener and edge confirmation')
    tokens = hold.get('inputs', [])
    confirming_tokens = confirm.get('inputs', [])
    axis = _axis_source(bindings, 'axes.commands')
    choice = axis + '_' + direction
    if (not tokens or not confirming_tokens or set(tokens) & set(confirming_tokens)
            or any(t.startswith(axis + '_') for t in tokens + confirming_tokens)):
        raise BotFault('Commands opener, confirmation and selection must be independent')
    return tokens, confirming_tokens, choice, [channel(t) for t in tokens], [channel(t) for t in confirming_tokens], channel(choice)


def select(live, step, *, clock=time.monotonic, sleep=time.sleep):
    opening, confirming, choice, hold_channels, confirm_channels, choice_channel = selection_inputs(live.bindings, step.get('direction'))
    guard = step.get('native_before')
    if (not isinstance(guard, dict) or type(guard.get('mission')) is not int
            or not isinstance(guard.get('sequence'), str) or not guard['sequence'].startswith('Seq_Game_')
            or guard.get('popup') is not False or guard.get('saving') is not False):
        raise BotFault('Commands choice requires explicit native mission, sequence, popup and save prerequisites')
    state, _ = fresh_control_observation(live.observe, native=True)
    if (live.held or sampled_release_keys(state) != set() or not matches(state.get('native', {}), guard)
            or state.get('scene') != 'gameplay' or state.get('camera_active') is not True
            or state.get('camera_available') is not True or state.get('gamepad') is not False
            or state.get('camera_suspended') is not False or state.get('menu') is not False
            or state.get('idroid') is not False or state['controls'].get('context') != 'gameplay'
            or state['controls'].get('travel_mode') != 1):
        raise BotFault('Commands choice requires released on-foot gameplay in the reviewed native scene')
    activation = state.get('activation')
    frame = state.get('rendered', {})
    owner = (frame.get('player_owner'), frame.get('presentation_epoch'))
    if any(type(v) is not int or v <= 0 for v in (activation, *owner)):
        raise BotFault('Commands camera generation is unavailable')
    deadline = clock() + 4.5

    def sample(tokens, seconds, *, confirmation=False):
        first = None
        while clock() < deadline:
            current = live.observe()
            if (current.get('activation') != activation or current.get('scene') != 'gameplay'
                    or current.get('camera_active') is not True or current.get('camera_suspended') is not False
                    or current.get('gamepad') is not False
                    or tuple(current.get('rendered', {}).get(k) for k in ('player_owner','presentation_epoch')) != owner
                    or current.get('menu') is not False or current.get('idroid') is not False):
                raise BotFault('Commands owner changed; choice was not retried')
            evidence = held_input_evidence(current, tokens)
            if evidence and evidence['sampled_context'] == 'commands':
                first = evidence['sample_ms'] if first is None else first
                if confirmation:
                    pad = current['controls']
                    expected = (-1 if step['direction'] in ('left', 'down') else 1)
                    axis = 2 if step['direction'] in ('left', 'right') else 3
                    if (pad.get('native_buttons', 0) & 0x0080
                            and pad.get('native_axes', [0]*4)[axis] * expected > 6000):
                        return current
                elif evidence['sample_ms'] - first >= math.ceil(seconds * 1000):
                    return current
            else:
                first = None
            sleep(.025)
        raise BotFault('Commands did not acknowledge its bounded choice; no input was retried')

    try:
        live.events.emit('semantic_commands_selection', direction=step['direction'], opening=opening,
                         confirming=confirming, axis_token=choice, native_guard=guard,
                         outcome_scope='input_delivery_only')
        live.input(hold_channels, .2, lease_seconds=4.5)
        sample(opening, .5)
        live.input([choice_channel], .2, lease_seconds=max(.03, deadline-clock()))
        sample(opening + [choice], .15)
        live.input(confirm_channels, .15, lease_seconds=.35)
        delivered = sample(opening + [choice] + confirming, 0, confirmation=True)
        live.events.emit('commands_choice_delivered', state=delivered, direction=step['direction'],
                         native_response_pending=True)
    finally:
        live.release()
    return live.observe()
