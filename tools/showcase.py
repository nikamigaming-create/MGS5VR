"""Prepare and run the actual-game showcase through the managed workspace bot.

Requires an active simulator in the reviewed isolated Afghanistan free-play
fixture. The caller owns the save/runtime lease. Every run is retained, including
failures; a completed queue is never an all-features acceptance claim.
"""
import argparse
import copy
import html
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
FAMILIES = {
    'Handgun': 'EQP_WP_West_hg_010', 'Submachinegun': 'EQP_WP_West_sm_010',
    'Assault': 'EQP_WP_West_ar_010', 'Shotgun': 'EQP_WP_Com_sg_010',
    'GrenadeLauncher': 'EQP_WP_50002', 'Sniper': 'EQP_WP_West_sr_010',
    'Machinegun': 'EQP_WP_West_mg_010', 'Missile': 'EQP_WP_East_ms_010'}


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def prepare(fixtures, output):
    from gameplay_bot.live import digest
    catalog = json.loads(fixtures.read_text())['cases']
    jobs = []
    selections = [(family, family, definition) for family, definition in FAMILIES.items()]
    selections += [('DualSight', 'Assault', 'EQP_WP_West_ar_075'),
                   ('ReflexSight', 'Assault', 'EQP_WP_West_ar_042'),
                   ('CustomDualSight', 'Assault', 'EQP_WP_30005')]
    for job_id, family, definition in selections:
        case = copy.deepcopy(next(row for row in catalog if row['definition'] == definition))
        if case['native_type'] != family or digest(ROOT / case['configuration_script']) != case['configuration_sha256']:
            raise ValueError('Fixture family/source mismatch: ' + definition)
        if not case.get('source_package') and definition not in ('EQP_WP_50002', 'EQP_WP_30005'):
            raise ValueError('Unpackaged/prototype weapon cannot be used in a reload fixture: ' + definition)
        if job_id == 'CustomDualSight':
            # Exact saved, live-tested custom rifle. A different personal
            # assembly must never inherit this fixture's two-optic claim.
            case['expected_custom_parts'] = [63, 29, 49, 10, 0, 21, 4, 1, 3, 0, 2, 0]
        if family in ('Sniper', 'Machinegun', 'Missile'):
            # Native mission reload rejects these long guns in PRIMARY_HIP.
            # Use the source-supported back slot and the physical right card.
            source = (ROOT / case['configuration_script']).read_text(encoding='utf-8')
            source = source.replace('PRIMARY_HIP', 'PRIMARY_BACK').replace('primaryHip', 'primaryBack')
            script = output / 'native-loadouts' / (definition.lower() + '-back.lua')
            script.parent.mkdir(parents=True, exist_ok=True)
            script.write_text(source, encoding='utf-8')
            case.update(slot='PRIMARY_BACK', physical_card='right', configuration_script=str(script.resolve()),
                        configuration_sha256=digest(script))
        path = output / (job_id + '.json')
        write(path, {'kind': 'weapon', 'weapon': case})
        jobs.append({'id': job_id, 'weapon_setup': True, 'command': 'showcase', 'suite': str(path.resolve()), 'seconds': 120,
                     'claims': ['selection', 'one_hand', 'two_hand', 'firing', 'reload', 'fitted_optics']})
    write(output / 'idroid.json', {'kind': 'idroid', 'reviewed_security_fixture': True})
    jobs += [{'id': 'idroid', 'command': 'showcase', 'suite': str((output / 'idroid.json').resolve()),
              'seconds': 120, 'claims': ['left_hand_motion', 'stationary_navigation', 'stow', 'immediate_reopen']}]
    for name, suite in [('field-menus', 'field-context-roundtrip'), ('wrist-hud', 'wrist-display')]:
        jobs.append({'id': name, 'command': 'run', 'suite': str(ROOT / 'tools/gameplay_bot/suites' / (suite + '.json')),
                     'seconds': 120, 'claims': ['native_transitions', 'both_eye_visuals']})
    write(output / 'controls-settings.json', {'scene': 'gameplay', 'enter': False, 'settings': True})
    jobs += [{'id': 'controls-settings', 'command': 'controls-editor', 'seconds': 180,
              'suite': str((output / 'controls-settings.json').resolve()),
              'claims': ['settings', 'remapping', 'vr_toggle', 'menu_cleanup']},
             {'id': 'binocular-optics', 'command': 'optics', 'seconds': 90,
              'claims': ['physical_lens', 'hand_aim', 'exposure']}]
    plan = {'schema': 1, 'jobs': jobs, 'scope': 'Eight representative weapon families and named UI/optic checks',
            'not_covered_by_queue': ['every weapon variant', 'all mission-specific menus', 'physical headset'],
            'fixture': {'mission': 30010, 'location': 10, 'sequence': 'Seq_Game_FreePlay'},
            'visual_acceptance': 'Requires final-eye and temporal review of each take'}
    write(output / 'plan.json', plan)
    for kind in ('menu-pages', 'binocular-recon'):
        path = output / (kind + '.json')
        write(path, {'kind': kind, 'reviewed_security_fixture': kind == 'menu-pages'})
        plan['jobs'].append({'id': kind, 'command': 'showcase', 'suite': str(path.resolve()),
                             'seconds': 90, 'claims': ['native_behavior', 'both_eye_visuals']})
    write(output / 'plan.json', plan)
    return output / 'plan.json'


def summarize(path):
    log = path / 'runner.log'
    result = None
    for line in log.read_text(encoding='utf-8').splitlines():
        try:
            value = json.loads(line)
        except ValueError:
            continue
        if 'status' in value:
            result = value
    if result is None:
        return {'status': 'failed', 'error': 'No completed result'}
    summary = {k: result[k] for k in ('status', 'error', 'recording_error', 'cleanup_error', 'family', 'definition') if k in result}
    summary['visual_acceptance'] = 'pending_review'
    summary['checks'] = []
    for card in result.get('cards', []):
        for key in ('one_hand', 'two_hand', 'firing', 'scopes', 'sight_inspection'):
            value = card.get(key)
            summary['checks'].append({'id': key, **({k: value[k] for k in ('status', 'reason', 'rounds') if k in value}
                                      if isinstance(value, dict) else {'status': value or 'not_run'})})
    if 'reload' in result:
        summary['checks'].append({'id': 'reload', **{k: result['reload'][k] for k in ('status', 'error', 'reason', 'magazine_increase') if k in result['reload']}})
    for case in result.get('cases', []):
        summary['checks'].append({k: case[k] for k in ('id', 'status', 'error') if k in case})
    for index, cycle in enumerate(result.get('cycles', [])):
        for case in cycle.get('cases', []):
            summary['checks'].append({'id': 'cycle-'+str(index+1),
                **{k: case[k] for k in ('status', 'error', 'exit_error', 'native_palm_motion', 'stationary_navigation') if k in case}})
    return summary


def execute(plan_path, output, only):
    plan = json.loads(plan_path.read_text())
    output.mkdir(parents=True, exist_ok=False)
    report = {'schema': 1, 'plan': plan, 'jobs': [], 'status': 'running',
              'physical_headset_acceptance': False, 'all_features_accepted': False}
    jobs = [j for j in plan['jobs'] if not only or j['id'] in only]
    if not jobs or (only and set(only) - {j['id'] for j in jobs}):
        raise ValueError('Unknown or empty showcase selection')
    for job in jobs:
        if (output / 'STOP').exists():
            report['status'] = 'stopped'; break
        started = time.time()
        if job.get('weapon_setup') or job['id'] in FAMILIES:
            for phase, command in [('setup', 'showcase-setup'), ('arrival', 'continue')]:
                pre = [sys.executable, str(ROOT / 'tools/workspace.py'), 'bot', '--command', command,
                       '--seconds', '120']
                if command == 'showcase-setup':
                    pre += ['--suite', job['suite']]
                with (output / (job['id'] + '-' + phase + '.log')).open('w') as log:
                    code = subprocess.run(pre, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT).returncode
                latest = json.loads((ROOT / 'artifacts/bot/latest.json').read_text())
                path = Path(latest['path'])
                (path / 'KEEP').write_text('Retained showcase setup evidence; not gameplay acceptance.\n')
                if code:
                    report['jobs'].append({'id': job['id'], 'status': 'blocked', 'phase': phase,
                        'path': str(path), 'exit_code': code, **summarize(path)})
                    report['status'] = 'needs_recovery'
                    write(output / 'results.json', report)
                    return report
            started = time.time()
        args = [sys.executable, str(ROOT / 'tools/workspace.py'), 'bot', '--command', job['command'],
                '--seconds', str(job['seconds']), '--record']
        if job.get('suite'):
            args += ['--suite', job['suite']]
        with (output / (job['id'] + '.log')).open('w') as log:
            code = subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT).returncode
        latest = json.loads((ROOT / 'artifacts/bot/latest.json').read_text())
        path = Path(latest['path'])
        # Never attach stale evidence when the workspace rejected the run before allocation.
        if path.stat().st_mtime < started - 1:
            row = {'id': job['id'], 'status': 'blocked', 'error': 'No fresh bot run', 'exit_code': code}
        else:
            (path / 'KEEP').write_text('Retained showcase investigation evidence.\n')
            row = {'id': job['id'], 'path': str(path), 'exit_code': code, **summarize(path)}
        report['jobs'].append(row)
        write(output / 'results.json', report)
        print(json.dumps(row), flush=True)
        if code:
            # Transport or cleanup failure must be inspected before another input owner starts.
            report['status'] = 'needs_recovery'; break
    if report['status'] == 'running':
        report['status'] = 'queue_completed'
    write(output / 'results.json', report)
    return report


def gallery(manifest_path, output):
    """Render retained real evidence; do not manufacture visual acceptance."""
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    sections = []
    for item in manifest['items']:
        path = Path(item['path']).resolve()
        if not path.is_relative_to(ROOT / 'artifacts') or not (path / 'KEEP').exists():
            raise ValueError('Gallery requires retained workspace evidence')
        summary = summarize(path)
        pictures = sorted(path.glob('*.png'))
        media = ''.join('<a href="'+p.as_uri()+'"><img loading="lazy" src="'+p.as_uri()+'"><small>'+
                        html.escape(p.name)+'</small></a>' for p in pictures)
        videos = ''.join('<p>Native source-eye recording (not final compositor):</p><video controls preload="none" src="'+
                         p.as_uri()+'"></video>' for p in sorted((path / 'recording').glob('*.mp4')))
        sections.append('<section><h2>'+html.escape(item['name'])+'</h2><p>'+html.escape(item['review'])+
                        '</p><details><summary>Machine results</summary><pre>'+html.escape(json.dumps(summary, indent=2))+
                        '</pre></details><div class="eyes">'+media+'</div>'+videos+'</section>')
    body = '<!doctype html><meta charset="utf-8"><title>MGS5VR simulator showcase</title><style>' \
           'body{background:#10171c;color:#e5edf2;font:17px system-ui;max-width:1250px;margin:40px auto;padding:20px}' \
           'h1,h2{color:#8edbd0}section{border-top:1px solid #456;padding:22px 0}.eyes{display:grid;grid-template-columns:1fr 1fr;gap:12px}' \
           'img,video{width:100%;max-height:650px;object-fit:contain}small{display:block;overflow-wrap:anywhere}a{color:#a2c9ed}' \
           'pre{white-space:pre-wrap}summary{cursor:pointer}</style><h1>MGS5VR: actual simulator showcase</h1><p>'+ \
           html.escape(manifest['verdict'])+'</p><p>Click an eye image for its original resolution. Each pair is labeled left/right. '+ \
           'Images are sequential final-compositor captures; source-eye videos do not certify compositor motion or physical-headset comfort.</p>'+''.join(sections)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(body, encoding='utf-8')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    p = sub.add_parser('prepare'); p.add_argument('--fixtures', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    p = sub.add_parser('run'); p.add_argument('--plan', type=Path, required=True); p.add_argument('--output', type=Path, required=True); p.add_argument('--only', nargs='+')
    p = sub.add_parser('gallery'); p.add_argument('--manifest', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.mode == 'prepare':
        print(prepare(args.fixtures, args.output))
    elif args.mode == 'gallery':
        print(gallery(args.manifest, args.output))
    else:
        result = execute(args.plan, args.output, args.only)
        sys.exit(0 if result['status'] == 'queue_completed' else 1)
