"""Compile an executable iDroid case from the actual current presentation.

Compilation never changes settings, world, save or tutorial restrictions. The
owning runner supplies a fresh state and invokes one current fixture at a time.
Other fixtures remain explicit, source-backed recipes rather than fake passes.
"""
from .core import BotFault
from .idroid_film import bindings_contract, film_idroid


MENU_BUTTONS = {
    'a': ('menus.confirm', 0x1000), 'b': ('menus.back', 0x2000),
    'x': ('menus.action_x', 0x4000), 'y': ('menus.action_y', 0x8000),
    'left_grip': ('menus.previous_tab', 0x100), 'right_grip': ('menus.next_tab', 0x200),
    'left_stick_click': ('menus.left_click', 0x40), 'right_stick_click': ('menus.right_click', 0x80),
    'left_trigger': ('menus.left_trigger', None), 'right_trigger': ('menus.right_trigger', None),
}


def compile_current_idroid(state, bindings, *, goal_node=None,
                           navigation_grip='left_grip', own_existing_menu=False):
    native = state.get('native') or {}
    frame = state.get('rendered') or {}
    world = (native.get('mission'), native.get('location'), native.get('sequence'))
    if (any(type(v) is not int or v <= 0 for v in world[:2]) or
            not isinstance(world[2], str) or not world[2].startswith('Seq_Game_') or
            any(type(state.get(k)) is not bool for k in ('idroid', 'idroid_handheld')) or
            type(native.get('helicopter_space')) is not bool or
            type(state.get('idroid_tutorial_mode')) is not int or
            not 0 <= state['idroid_tutorial_mode'] <= 255 or
            navigation_grip not in ('left_grip', 'right_grip') or
            type(own_existing_menu) is not bool or
            (state['idroid'] and not own_existing_menu)):
        raise BotFault('Current iDroid case needs typed world/presentation/tutorial and menu ownership')
    owner = (state.get('activation'), frame.get('player_owner'), frame.get('presentation_epoch'))
    if any(type(v) is not int or v <= 0 for v in owner):
        raise BotFault('Current iDroid case lacks an exact VR owner')
    handheld = state['idroid_handheld']; cabin = native['helicopter_space']
    bindings_contract(bindings, handheld=handheld)
    if not cabin and (type(goal_node) is not int or goal_node < 0):
        raise BotFault('Field walking film requires an actual nearby connected NAV node')
    mode = state['idroid_tutorial_mode'] if state['idroid'] else 0
    case_id = ('acc' if cabin else 'field')+'-'+('handheld' if handheld else 'spatial')
    case_id += '-tutorial'+str(mode)+'-'+navigation_grip
    return {'schema': 1, 'id': case_id, 'executor': 'film_idroid', 'world': world, 'owner': owner,
        'fixture': {'idroid': state['idroid'], 'handheld': handheld, 'tutorial_mode': mode,
                    'helicopter_space': cabin},
        'kwargs': {'world': world, 'goal_node': goal_node, 'handheld': handheld,
            'navigation_grip': navigation_grip, 'tutorial_mode': mode,
            'helicopter_space': cabin, 'exercise_walk': not cabin,
            'exercise_both_grips': True, 'start_open': state['idroid'],
            'own_existing_menu': own_existing_menu},
        'physical_actions_exercised': ['menu', 'b', 'left_grip', 'right_grip']+
            ([] if cabin else ['left_stick_up']),
        'native_binocular_leakage': 'fresh native BINOCLE=false read under each observed grip/stick lease',
        'other_button_recipes': {token: {'action': action, 'native_button_mask': mask,
            'executor': 'menu_packet_case.run_menu_button_case',
            'kwargs': {'token': token, 'fixture_label': case_id, 'own_existing_idroid': True, 'seconds': .12},
            'prerequisite': 'root prepares and owns the current iDroid page; recapture after each button',
            'page_operation_success_inferred_from_packet': False, 'executed': False}
            for token, (action, mask) in MENU_BUTTONS.items()
            if token not in ('b', 'left_grip', 'right_grip')},
        'source_refs': ['src/native_controls.cpp:nativeMenuGamepad',
                        'src/native_actions.cpp:MGS5VR_iDroid named pad and pause registrations'],
        'field_walking_expected_from_source': False,
        'spatial_native_pixels_or_full_game_acceptance': False}


def run_current_idroid_case(live, nav, *, state, enemy_script, output, goal_node=None,
                            navigation_grip='left_grip', own_existing_menu=False,
                            max_seconds=90., **timing):
    """Execute the compiled source-bound case; film_idroid rechecks its fixture."""
    case = compile_current_idroid(state, live.bindings, goal_node=goal_node,
        navigation_grip=navigation_grip, own_existing_menu=own_existing_menu)
    # A stale caller snapshot cannot authorize a changed owner at invocation.
    from .combat import fresh_rig
    current, identity = fresh_rig(live)
    if tuple(identity[:2]) != tuple(case['world'][:2]) or tuple(identity[2:]) != tuple(case['owner']):
        raise BotFault('Compiled iDroid case lost the current world or VR owner')
    fresh = compile_current_idroid(current, live.bindings, goal_node=goal_node,
        navigation_grip=navigation_grip, own_existing_menu=own_existing_menu)
    if fresh['fixture'] != case['fixture'] or tuple(fresh['world']) != tuple(case['world']):
        raise BotFault('Compiled iDroid presentation/tutorial fixture changed before input')
    result = film_idroid(live, nav, enemy_script=enemy_script, output=output,
                         max_seconds=max_seconds, **case['kwargs'], **timing)
    result['compiled_case'] = case
    from pathlib import Path
    from .core import atomic_json
    atomic_json(Path(output)/'result.json', result)
    return result
