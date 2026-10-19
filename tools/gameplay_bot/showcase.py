"""Repeatable, bounded feature checks; never equate input delivery with acceptance."""
import time

from . import arsenal
from .core import BotFault, atomic_json, matches
from .live import digest
from .idroid import inspect_idroid, device_orientation

FAMILIES = ('Handgun', 'Submachinegun', 'Assault', 'Shotgun', 'GrenadeLauncher',
            'Sniper', 'Machinegun', 'Missile')


def reload_outcome(before, after, slot, ammo_field='ammo'):
    if any(before[s]['custom_or_equipped_id'] != after[s]['custom_or_equipped_id'] for s in arsenal.SLOTS):
        return {'status': 'fail', 'reason': 'loadout_changed'}
    change = after[slot][ammo_field] - before[slot][ammo_field]
    return {'status': 'observed_pass' if change > 0 else 'unproven',
            'magazine_increase': change, 'before': before, 'after': after, 'ammo_field': ammo_field,
            'reason': None if change > 0 else 'no_magazine_increase_observed',
            'visual_acceptance': 'pending'}


def weapon_plan(case):
    return {'world': {'mission': 30010, 'location': 10, 'sequence': 'Seq_Game_FreePlay'},
            'scope': 'Representative family ' + case['native_type'], 'catalog_queue': [],
            'policy': {'burst_seconds': .20},
            'cards': [{'case_id': case['case_id'], 'category': case['physical_category'],
                       'card': case['physical_card'], 'expected_slot': case['slot']}]}


def setup_weapon(live, policy):
    case = policy['weapon']
    if case['native_type'] not in FAMILIES:
        raise BotFault('Unknown weapon family')
    script = (arsenal.ROOT / case['configuration_script']).resolve()
    if not script.is_relative_to(arsenal.ROOT / 'private') or digest(script) != case['configuration_sha256']:
        raise BotFault('Owned fixture source changed')
    plan = weapon_plan(case)
    live.release()
    before, anchor = arsenal.guard(live, plan)
    receipt, timing = live.native.read(script.read_text(encoding='utf-8'))
    expected = {'definition_1': case['definition'], 'runtime_id_1': case['retained_runtime_id'],
                'after_weapon': case['retained_runtime_id'], 'target_slot_name': case['slot'],
                'controlled_fixture': True, 'configuration_calls': True}
    if not matches(receipt, expected):
        raise BotFault('Native configuration did not match the requested exact weapon')
    atomic_json(live.events.output / 'configuration.json', {'receipt': receipt, 'transport': timing,
                'script': str(script), 'sha256': digest(script), 'acquisition_tested': False})
    # SetWeapons is a mission-configuration API, not a live gun replacement.
    # Retail SavePlayerCurrentWeapons + Reload rebuilds native equipment from
    # the configured slots (owned TppPlayer.lua:429 and TppMission.lua:2278).
    # This is isolated fixture setup, never evidence of normal acquisition.
    reload_receipt, _ = live.native.read("assert(vars.missionCode==30010 and "
        "TppSequence.GetCurrentSequenceName()=='Seq_Game_FreePlay' and not TppSave.IsSaving()); "
        "TppPlayer.SavePlayerCurrentWeapons(); TppMission.Reload{isNoFade=true,showLoadingTips=false}; "
        "return '{\"fixture_reload_requested\":true}'")
    return {'status': 'fixture_reload_requested', 'receipt': receipt, 'reload_request': reload_receipt}


def weapon(live, policy, seconds):
    case = policy['weapon']
    plan = weapon_plan(case)
    initial, anchor = arsenal.guard(live, plan)
    for index, part in enumerate(case.get('expected_custom_parts', [])):
        if initial['native'].get('chimera_part_' + str(index)) != part:
            raise BotFault('Saved custom sight fixture differs from the reviewed assembly')
    plan['configured_definition'] = {'definition': case['definition'],
        'runtime_id': case['retained_runtime_id'], 'slot': case['slot']}
    actual = initial['native']['weapon_' + case['slot']]
    index = arsenal.SLOTS.index(case['slot'])
    if actual != case['retained_runtime_id']:
        if initial['native'].get('customized_weapon_'+str(index)) != case['retained_runtime_id']:
            raise BotFault('Neither direct slot nor native customized base matches the configured weapon')
        plan['configured_definition'].update(runtime_id=actual, customized_index=index,
            base_id=case['retained_runtime_id'], binding='native_customized_base_and_stable_equipped_slot')
    base = case['retained_runtime_id']
    if type(base) is not int or not 0 < base < 65535:
        raise BotFault('Invalid native definition')
    capacities, _ = live.native.read('local b,c,r,s,d,t=TppEquip.GetAmmoInfo('+str(base)+
        '); return string.format(\'{"capacity":%d,"sub_capacity":%d}\',c,d)')
    plan['ammo_field'] = 'sub_ammo' if capacities['capacity'] == 0 and capacities['sub_capacity'] > 0 else 'ammo'
    before, anchor = arsenal.guard(live, plan)
    result = arsenal.run_sweep(live, plan, seconds=min(seconds, 120), max_cards=1)
    result['family'] = case['native_type']
    result['definition'] = case['definition']
    result['setup_scope'] = 'Native mission reload of isolated configured loadout; acquisition not tested'
    result['reload'] = {'status': 'not_run'}
    if result.get('cards') and isinstance(result['cards'][0].get('firing'), dict):
        try:
            state, _ = arsenal.guard(live, plan, anchor)
            ammo = arsenal.inventory(state['native'])
            fired = result['cards'][0]['firing']
            automatic = reload_outcome(fired['after'], ammo, case['slot'], plan['ammo_field']) if fired.get('status') == 'pass' else None
            live.execute({'op': 'action', 'name': 'gameplay.reload',
                          **arsenal.action_guard(state, plan)})
            deadline = time.monotonic() + 10
            while True:
                state, _ = arsenal.guard(live, plan, anchor)
                result['reload'] = reload_outcome(ammo, arsenal.inventory(state['native']), case['slot'], plan['ammo_field'])
                if automatic and automatic['status'] == 'observed_pass':
                    result['reload'] = {**automatic, 'mechanism': 'native_automatic_reload_before_manual_input',
                                        'manual_reload': 'not_exercised_magazine_already_refilled'}
                else:
                    result['reload']['mechanism'] = 'physical_reload_binding'
                if result['reload']['status'] == 'observed_pass' or time.monotonic() >= deadline:
                    break
                time.sleep(.1)
            result['reload']['eyes'] = live.capture(case['case_id'] + '-reload')
        except Exception as error:
            result['reload'] = {'status': 'fail', 'error': str(error)}
        finally:
            live.release()
    atomic_json(live.events.output / 'showcase.json', result)
    return result


def run(live, behavior, policy, seconds):
    if policy.get('reviewed_security_fixture') is True and policy.get('kind') in ('idroid', 'menu-pages'):
        # This isolated, reviewed free-play save opens the native FOB help once.
        # Acknowledge only its identified owner; it is not ordinary navigation.
        live.execute({'op': 'pose', 'hand': 'left', 'position': [-.12, -.18, -.50],
                      'orientation': device_orientation()})
        live.execute({'op': 'action', 'name': 'system.idroid'})
        state = behavior.wait_for({'idroid': True, 'idroid_menu_input_ready': True}, 8, 'fixture-idroid-open')
        state = live.observe(native=True)
        if state.get('idroid_tutorial_mode') == 11:
            if not matches(state, {'native.mission': 30010, 'native.sequence': 'Seq_Game_FreePlay',
                                   'native.popup': False, 'native.fob_tutorial_state': 127}):
                raise BotFault('Unreviewed tutorial owner; no acknowledgement sent')
            if not state.get('pause'):
                behavior.wait_for({'pause': True, 'idroid_tutorial_mode': 11}, 5, 'reviewed-security-help')
            live.capture('reviewed-security-help')
            live.execute({'op': 'action', 'name': 'menus.back', 'seconds': .15})
            behavior.wait_for({'pause': False}, 6, 'reviewed-security-help-closed')
            live.execute({'op': 'action', 'name': 'menus.back', 'seconds': 1.7})
        elif state.get('idroid_tutorial_mode') == 0 and state.get('pause') is False:
            live.execute({'op': 'action', 'name': 'menus.back'})
        else:
            raise BotFault('Unexpected native tutorial; fixture preparation stopped')
        behavior.wait_for({'idroid': False, 'menu': False}, 8, 'fixture-idroid-stowed')
    if policy.get('kind') == 'binocular-recon':
        from .combat import fresh_rig
        from .perception import read_enemy_snapshot
        from .recon import inspect
        import math
        state, anchor = fresh_rig(live)
        enemies = read_enemy_snapshot(live.native.read, (arsenal.ROOT / 'tools/navigation-state.lua').read_text())
        position = [state['native']['player_'+axis] for axis in 'xyz']
        targets = [r for r in enemies['soldiers'] if r['life'] == enemies['normal_life'] and r.get('camp')]
        if not targets:
            raise BotFault('No living native actor available for acquisition/mark showcase')
        target = min(targets, key=lambda r: math.dist(position, r['position']))
        return inspect(live, enemies, output=live.events.output, anchor=anchor, camp=target['camp'],
                       healthy_life=state['native']['player_life'], max_seconds=min(seconds, 60))
    if policy.get('kind') == 'menu-pages':
        result = {'status': 'running', 'pages': [], 'visual_acceptance': 'pending'}
        live.execute({'op': 'pose', 'hand': 'left', 'position': [-.12, -.18, -.50],
                      'orientation': device_orientation()})
        live.execute({'op': 'action', 'name': 'system.idroid'})
        behavior.wait_for({'idroid': True, 'idroid_menu_input_ready': True}, 8, 'showcase-idroid-pages')
        # Exercise previous away from the native first-tab boundary, then
        # exercise the established next-tab wrap separately.
        for index, action in enumerate((None, 'idroid.next_tab', 'idroid.previous_tab',
                                        'idroid.next_tab', 'idroid.next_tab', 'idroid.next_tab')):
            if action:
                live.execute({'op': 'action', 'name': action})
            time.sleep(.7)
            result['pages'].append({'action': action, 'state': live.observe(native=True),
                                    'eyes': live.capture('idroid-tab-'+str(index))})
        live.execute({'op': 'action', 'name': 'menus.back'})
        behavior.wait_for({'idroid': False, 'menu': False}, 8, 'showcase-pages-stowed')
        result['status'] = 'observed_page_inputs'
        return result
    if policy.get('kind') == 'weapon':
        return weapon(live, policy, seconds)
    if policy.get('kind') == 'idroid':
        results = []
        for index in range(2):
            results.append(inspect_idroid(live, behavior))
            atomic_json(live.events.output / 'showcase.json', {'cycles': results})
            if results[-1]['status'] != 'observed_pass':
                break
        return {'status': 'observed_pass' if len(results) == 2 and all(r['status'] == 'observed_pass' for r in results)
                else 'failed', 'cycles': results, 'visual_acceptance': 'pending'}
    raise BotFault('Showcase requires an explicit weapon or iDroid policy')
