"""Build a per-ID SIM worklist from locally extracted, legally owned equipment tables.

No game mutation, save editing, or synthetic success. Runtime IDs stay distinct
even when they share a model, ammunition, or reload animation. Generated JSON is
private working data; do not distribute the source game scripts.
"""
import argparse
import collections
import decimal
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


def tpp_chimera_inventory(document, source, development):
    """Join modern numbered WP definitions that are absent from the legacy enum table.

    Only parse declarative, constant gunBasic rows; never execute owned Lua.
    Development type/name keys are provenance, not proof that an item is usable.
    """
    raw = source.read_bytes()
    text = raw.decode('utf-8-sig')
    headers = list(re.finditer(r'\bgunBasic\s*=\s*\{', text))
    if len(headers) != 1:
        raise ValueError(f'{source}: expected one declarative gunBasic table')
    start = headers[0].end()
    depth = 1
    end = start
    # This bounded table accepts only constants and integer grades below.
    # Quotes/comments are deliberately rejected instead of misreading braces.
    while depth and end < len(text):
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    if depth:
        raise ValueError(f'{source}: unterminated gunBasic table')
    body = text[start:end-1]
    if any(token in body for token in ('"', "'", '--', '[', ']')):
        raise ValueError(f'{source}: gunBasic is not a plain constant table')
    rows = re.findall(r'\{([^{}]*)\}', body)
    if not rows or not re.fullmatch(r'\s*1\s*(?:,\s*)*', re.sub(r'\{[^{}]*\}', '', body)):
        raise ValueError(f'{source}: unsupported gunBasic header or row structure')

    dev_raw = development.read_bytes()
    dev_text = dev_raw.decode('utf-8-sig')
    definitions = {}
    for match in re.finditer(r'\bRegCstDev\s*\{([^{}]*)\}', dev_text):
        record = match.group(1)
        identity = re.search(r'\bp01\s*=\s*TppEquip\.(EQP_WP_\w+)\s*(?:,|$)', record)
        if not identity:
            continue
        kind = re.search(r'\bp02\s*=\s*TppMbDev\.EQP_DEV_TYPE_(\w+)\s*(?:,|$)', record)
        name = re.search(r'\bp06\s*=\s*"([^"\r\n]*)"', record)
        record_id = re.search(r'\bp00\s*=\s*(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*(?:,|$)', record)
        if not kind or not record_id:
            raise ValueError(f'{development}: unsupported development record for {identity[1]}')
        numeric_id = decimal.Decimal(record_id[1])
        if numeric_id != numeric_id.to_integral_value() or not 0 <= numeric_id <= 0x7fffffff:
            raise ValueError(f'{development}: non-integral development identity')
        item = dict(native_type=kind[1], name_key=name[1] if name else None,
                    development_id=int(numeric_id))
        if identity[1] in definitions:
            previous = definitions[identity[1]]
            if item['native_type'] not in previous['native_types']:
                previous['native_types'].append(item['native_type'])
                previous['native_type'] = 'Unknown'
            if item not in previous['records']:
                previous['records'].append(item)
        else:
            definitions[identity[1]] = dict(native_type=kind[1], native_types=[kind[1]], records=[item])
    if not definitions:
        raise ValueError(f'{development}: no modern weapon development records found')

    existing = {entry['id']: entry for entry in document['entries']}
    seen = set()
    part_names = ('receiver', 'barrel', 'ammunition', 'stock', 'muzzle', 'magazine',
                  'rear_sight', 'front_sight', 'underbarrel', 'light', 'light_secondary')
    added = 0
    for row in rows:
        values = [token.strip() for token in row.split(',')]
        if len(values) != 13 or not values[-1].isdigit() or any(
                not re.fullmatch(r'TppEquip\.\w+', value) for value in values[:-1]):
            raise ValueError(f'{source}: unsupported modern weapon row')
        weapon = values[0].removeprefix('TppEquip.')
        if not re.fullmatch(r'WP_\w+', weapon):
            raise ValueError(f'{source}: unexpected gunBasic identity {weapon}')
        identity = 'EQP_' + weapon
        if identity in seen:
            raise ValueError(f'{source}: duplicate modern weapon {identity}')
        seen.add(identity)
        definition = definitions.get(identity)
        kind = definition['native_type'] if definition else 'Unknown'
        native_parts = dict(zip(part_names, (v.removeprefix('TppEquip.') for v in values[1:-1])))
        detail = dict(gun_basic_id=weapon, native_grade=int(values[-1]), native_parts=native_parts,
                      development=definition, source_kind='modern_chimera_gunBasic')
        if identity in existing:
            if existing[identity]['native_type'] != kind and definition:
                # Development UI categories (e.g. Quiet) need not equal the
                # actual equipment type. Keep both; never silently reclassify.
                detail['development_type_difference'] = [existing[identity]['native_type'], kind]
            existing[identity].update(detail)
            continue
        entry = dict(game='tpp', id=identity, native_type=kind,
                     classification='eligibility_to_check', classification_evidence=None,
                     display_name=None, access='not_checked',
                     tests={name: {'result': 'not_run', 'runs': []} for name in cases(kind)}, **detail)
        document['entries'].append(entry)
        added += 1
    document['sources'] = [dict(path=document['source'], sha256=document['source_sha256']),
                           dict(path=str(source.resolve()), sha256=hashlib.sha256(raw).hexdigest()),
                           dict(path=str(development.resolve()), sha256=hashlib.sha256(dev_raw).hexdigest())]
    document['modern_gun_rows'] = len(rows)
    document['modern_ids_added'] = added
    document['development_weapons_without_gunBasic'] = sorted(set(definitions) - seen)
    document['inventory_limits'] = [
        'Modern gunBasic joins preserve individual grades; neither development nor definition proves access.',
        'Development-only IDs are listed separately, not invented as gunBasic weapons.',
        'Patch load order, buddy/contextual equipment and native abilities still require reconciliation.']
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--table', action='append', required=True, metavar='GAME=PATH')
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--tpp-chimera', type=pathlib.Path,
                        help='Owned parts/EquipParameters.lua containing modern gunBasic rows')
    parser.add_argument('--tpp-development', type=pathlib.Path,
                        help='Owned EquipDevelopConstSetting.lua providing native weapon types')
    args = parser.parse_args()
    games = []
    for value in args.table:
        game, path = value.split('=', 1)
        if game not in {'tpp', 'gz'} or any(g['game'] == game for g in games):
            parser.error('exactly one table per supplied game: tpp or gz')
        games.append(inventory(game, pathlib.Path(path)))
    if bool(args.tpp_chimera) != bool(args.tpp_development):
        parser.error('--tpp-chimera and --tpp-development must be supplied together')
    if args.tpp_chimera:
        tpp = next((game for game in games if game['game'] == 'tpp'), None)
        if tpp is None:
            parser.error('--tpp-chimera requires a tpp table')
        tpp_chimera_inventory(tpp, args.tpp_chimera, args.tpp_development)
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
