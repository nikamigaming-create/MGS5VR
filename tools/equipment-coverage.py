"""Build a per-ID SIM worklist from locally extracted, legally owned equipment tables.

No game mutation, save editing, or synthetic success. Runtime IDs stay distinct
even when they share a model, ammunition, or reload animation. Generated JSON is
private working data; do not distribute the source game scripts.
"""
import argparse
import collections
import hashlib
import json
import pathlib
import re


ENTRY = re.compile(
    r'\{\s*TppEquip\.(EQP_\w+)\s*,\s*TppEquip\.(EQP_TYPE_\w+)\s*,'
    r'\s*([^,{}]+)\s*,\s*TppEquip\.(EQP_BLOCK_\w+)\s*,'
    r'\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}'
)
FIREARMS = {'Handgun', 'Submachinegun', 'Assault', 'Shotgun', 'Sniper',
            'Machinegun', 'GrenadeLauncher', 'Missile'}
RESOURCES = {'Ammo', 'Bullet', 'AmmoBox', 'WeaponBox'}


def cases(kind):
    common = ['equip_and_stow', 'both_eye_presentation', 'head_and_hand_motion',
              'switch_away_and_back', 'controls_do_not_double_trigger']
    if kind in FIREARMS:
        return common + ['aim_and_impact', 'partial_reload', 'empty_reload',
                         'scope_and_zoom_if_present', 'alternate_mode_if_present',
                         'muzzle_obstruction']
    if kind == 'Throwing':
        return common + ['aim_and_throw', 'effect_at_target', 'depletion', 'cancel_ready']
    if kind == 'Placed':
        return common + ['place', 'activate_or_detonate', 'effect_at_target', 'depletion']
    if kind == 'AttackArm':
        return common + ['charge_and_activate', 'effect_at_target', 'recharge',
                         'remote_camera_and_return_if_present']
    if kind == 'Shield':
        return common + ['block_incoming_attack', 'sidearm_use', 'movement_and_stance']
    return common + ['native_use', 'native_effect', 'cancel_or_remove',
                     'item_specific_actions']


def inventory(game, source):
    if game == 'gz':
        return gz_inventory(source)
    raw = source.read_bytes()
    text = raw.decode('utf-8-sig')
    matches = list(ENTRY.finditer(text))
    if not matches:
        raise ValueError(f'{source}: no recognized equipment table; no inventory guessed')
    # Fail if an equipment row has a structure our reader has not handled.
    starts = re.findall(r'\{\s*TppEquip\.EQP_\w+\s*,', text)
    if len(matches) != len(starts):
        raise ValueError(f'{source}: parsed {len(matches)} of {len(starts)} rows')
    ids = set()
    entries = []
    for match in matches:
        equip, kind, parameter, block, model, package = match.groups()
        if equip in ids:
            raise ValueError(f'{source}: duplicate ID {equip}')
        ids.add(equip)
        kind = kind.removeprefix('EQP_TYPE_')
        resource = kind in RESOURCES
        entries.append(dict(
            game=game, id=equip, native_type=kind, parameter=parameter.strip(),
            block=block, model=model, package=package,
            classification='resource' if resource else 'eligibility_to_check',
            # Native types alone cannot decide whether an entry belongs to
            # Snake, a buddy, a scripted enemy, or a non-playable demo.
            classification_evidence=None, display_name=None, access='not_checked',
            tests={} if resource else {name: {'result': 'not_run', 'runs': []}
                                      for name in cases(kind)}))
    return dict(game=game, source=str(source.resolve()),
                source_sha256=hashlib.sha256(raw).hexdigest(), entries=entries)


def gz_inventory(directory):
    """GZ uses independent gun/support JSON tables, not TPP's EQP enum table."""
    sources, entries = [], []
    for filename, key in [('GunBasicParameter.json', 'gunBasicParameterTable'),
                          ('SupportWeaponParameter.json', 'supportWeaponParameterTable')]:
        path = directory / filename
        raw = path.read_bytes()
        sources.append({'path': str(path.resolve()), 'sha256': hashlib.sha256(raw).hexdigest()})
        for row in json.loads(raw.decode('utf-8-sig'))[key]:
            if row[0] is not True:
                continue
            identity = row[1]
            if key == 'gunBasicParameterTable':
                family = {'ar': 'Assault', 'hg': 'Handgun', 'sg': 'Shotgun',
                          'sm': 'Submachinegun', 'sr': 'Sniper', 'ms': 'Missile',
                          'Gl': 'GrenadeLauncher'}[identity[3:5]]
                scope, alternate = row[5], row[6]
            else:
                family = 'Placed' if row[4].startswith('TYPE_PLACE_') else 'Throwing'
                scope, alternate = '', ''
            entries.append(dict(game='gz', id=identity, native_type=family,
                                classification='eligibility_to_check', classification_evidence=None,
                                display_name=None, access='not_checked',
                                native_scope=scope, native_alternate=alternate,
                                tests={name: {'result': 'not_run', 'runs': []} for name in cases(family)}))
    return dict(game='gz', sources=sources, entries=entries,
                inventory_limits=['Gun/support tables do not enumerate binoculars, iDroid or NVG.',
                                  'A native definition does not establish player access.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--table', action='append', required=True, metavar='GAME=PATH')
    parser.add_argument('--output', required=True, type=pathlib.Path)
    args = parser.parse_args()
    games = []
    for value in args.table:
        game, path = value.split('=', 1)
        if game not in {'tpp', 'gz'} or any(g['game'] == game for g in games):
            parser.error('exactly one table per supplied game: tpp or gz')
        games.append(inventory(game, pathlib.Path(path)))
    if args.output.exists():
        parser.error('output already exists; preserve its recorded results and choose a new path')
    document = dict(schema=1, scope='per-native-equipment-ID, not family sampling',
                    physical_headset='not_requested_or_tested',
                    coverage_complete=False, games=games,
                    inventory_limits=['The table may include non-player/demo entries.',
                                      'Mounted, buddy and contextual systems need a separate action list.',
                                      'Each excluded or inapplicable action requires a recorded reason.',
                                      'Inventory is not equip/use or visual evidence.'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
    for game in games:
        counts = collections.Counter(e['native_type'] for e in game['entries'])
        print(json.dumps({'game': game['game'], 'native_entries': len(game['entries']),
                          'types': counts, 'SIM_passes': 0}))


if __name__ == '__main__':
    main()
