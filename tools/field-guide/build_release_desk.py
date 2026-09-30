"""Join the issue ledger, numeric audit and real action recordings into a field kit."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
from functools import lru_cache
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('lesson', ROOT / 'tools/render-gameplay-lesson.py')
lesson = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lesson)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def relative(path, output):
    path = Path(path).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError('Field-kit media must be inside the workspace: ' + str(path))
    return Path(os.path.relpath(path, output)).as_posix()


@lru_cache(maxsize=32)
def inspect_media(path, expected_sha):
    actual_sha = sha(path)
    if not expected_sha or actual_sha.lower() != expected_sha.lower():
        raise ValueError('Source video does not match its recorder index SHA-256')
    probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error',
        '-show_entries', 'stream=codec_type,start_time,duration,width,height:format=duration',
        '-of', 'json', str(path)], text=True, encoding='utf-8'))
    video = next((s for s in probe.get('streams', []) if s.get('codec_type') == 'video'), None)
    if video is None:
        raise ValueError('Source media has no video stream')
    duration = float(video.get('duration', 0))
    start = float(video.get('start_time', 0))
    if not math.isfinite(duration) or duration <= 0 or not math.isfinite(start) or abs(start) > .002:
        raise ValueError('Source media must have a finite video duration starting at zero')
    return {'sha256': actual_sha, 'video_duration': duration,
            'width': video['width'], 'height': video['height']}


def clip_window(first, last, begin, end, media_duration):
    # Native capture metadata cannot detect an AV mux that truncated the video.
    # Require the action itself to fit both clocks; trim only optional tail padding.
    media_end = first + int(media_duration * 1e7)
    available_end = min(last, media_end)
    if begin < first or end > available_end:
        raise ValueError('Encoded video does not cover the whole action; native timestamps alone are insufficient')
    return max(first, begin - 2000000), min(available_end, end + 6000000)


def lessons_from_run(run, output):
    run = run.resolve()
    result = read(run / 'result.json')
    identity = read(run / 'identity.json')
    cases = result.get('cases', []) + [c for s in result.get('suites', []) for c in s.get('cases', [])]
    events = [json.loads(line) for line in (run / 'events.jsonl').read_text().splitlines() if line]
    segments = read(run / 'recording/index.json')['segments'] if (run / 'recording/index.json').exists() else []
    review_path = run / 'visual-review.json'
    review = read(review_path) if review_path.exists() else None
    if review:
        if review.get('result_sha256') != sha(run / 'result.json'):
            raise ValueError('Visual review does not match the exact run result: ' + str(run))
        for capture in review.get('captures', {}).values():
            image = (run / capture['path']).resolve()
            if not image.is_file() and not Path(capture['path']).is_absolute():
                image = (ROOT / capture['path']).resolve()
            if not image.is_relative_to(run) or sha(image) != capture.get('sha256'):
                raise ValueError('Visual review capture is missing or changed: ' + str(image))
    records = []
    for case in cases:
        record = {'id': case['id'], 'native_status': case['status'], 'visual_status': 'unreviewed',
                  'run': relative(run / 'result.json', output), 'identity': identity,
                  'title': case['id'].replace('-', ' ').capitalize(), 'available': False}
        records.append(record)
        if review and review.get('case_id') == case['id']:
            record.update(visual_status=review['status'], visual_finding=review['finding'],
                          visual_scope=review['scope'], visual_review=relative(review_path, output))
        if case['status'] != 'observed_pass':
            record['reason'] = case.get('error', 'Native outcome did not pass')
            continue
        try:
            window, cues = lesson.compile_action_cues(events, case['id'])
            begin = next(e for e in window if e['event'] == 'case_started')['qpc_100ns']
            end = next(e for e in reversed(window) if e['event'] == 'case_finished')['qpc_100ns']
            segment = next((s for s in segments if s.get('status') == 'captured'
                and s['video_stats'].get('complete') and s['video_stats']['first_qpc_100ns'] <= begin
                and end <= s['video_stats']['last_qpc_100ns']), None)
            if segment is None:
                raise ValueError('No single complete source segment covers this action')
            stats = segment['video_stats']
            source = Path(segment['output']).resolve()
            if not source.is_relative_to(run) or not source.is_file():
                raise ValueError('Missing or out-of-run source recording')
            media = inspect_media(source, segment.get('sha256'))
            if (media['width'], media['height']) != (stats['width'], stats['height']):
                raise ValueError('Encoded video dimensions disagree with capture metadata')
            first, last = stats['first_qpc_100ns'], stats['last_qpc_100ns']
            clip_begin, clip_end = clip_window(first, last, begin, end, media['video_duration'])
            intervals = []
            for cue in cues:
                for channel in cue['channel_intervals']:
                    a, b = channel['start_qpc_100ns'], channel['end_qpc_100ns']
                    if not clip_begin <= a < b <= clip_end:
                        raise ValueError('Control interval is outside the captured excerpt')
                    intervals.append({'input': channel['input'], 'start': (a - clip_begin) / 1e7,
                                      'end': (b - clip_begin) / 1e7, 'label': channel['label']})
            f = stats['render_fov']
            aspect = (math.tan(f[1])-math.tan(f[0]))/(math.tan(f[2])-math.tan(f[3]))
            crop = lesson.source_crop_rect(stats['width'], stats['height'], aspect)
            images = [e for e in window if e['event'] == 'capture']
            record.update(available=True, action=cues[0]['action'], label=cues[0]['label'],
                media=relative(source, output), source_sha256=media['sha256'],
                encoded_video_duration=media['video_duration'],
                source_kind=segment.get('source'), source_eye={0: 'left', 1: 'right'}.get(stats.get('eye'), 'unknown'),
                source_width=stats['width'], source_height=stats['height'],
                crop={'width': crop[0], 'height': crop[1], 'x': crop[2], 'y': crop[3]},
                start=(clip_begin-first)/1e7, end=(clip_end-first)/1e7,
                duration=(clip_end-clip_begin)/1e7, intervals=intervals,
                before=case.get('before'), after=case.get('after'),
                source_dropped_frames=stats.get('dropped'),
                timing='Controller command acknowledgments corroborated by native input samples; source dropped frames limit exact pixel alignment.',
                poses='Illustrative controller orientation; button timing follows this recorded action.',
                eyes=[{'eye': e['eye'], 'url': relative(e['path'], output), 'sha256': e['sha256']} for e in images],
                result_sha256=sha(run / 'result.json'), events_sha256=sha(run / 'events.jsonl'))
        except (ValueError, KeyError, StopIteration, subprocess.CalledProcessError) as error:
            record['reason'] = str(error)
    return records


def select_lessons(runs, output):
    # Later explicit runs supersede the same case, while old raw results remain.
    # Never silently fall back to a historic pass if the latest action failed.
    selected = {}
    for run in runs:
        for record in lessons_from_run(run, output):
            selected[record['id']] = record
    return list(selected.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--coverage', type=Path, required=True)
    parser.add_argument('--numbers', type=Path, required=True)
    parser.add_argument('--bindings', type=Path, required=True)
    parser.add_argument('--run', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = read(args.coverage)
    numbers = read(args.numbers)
    for source in numbers['source_files']:
        source_path = ROOT / source['path']
        if sha(source_path) != source['sha256']:
            raise ValueError('Numeric audit is stale; regenerate it before publishing the kit: ' + source['path'])
    for row in numbers['literals']:
        row['source_url'] = relative(ROOT / row['file'], output)
    for setting in numbers['settings']:
        for reference in setting['references']:
            reference['source_url'] = relative(ROOT / reference['file'], output)
    selected_lessons = select_lessons(args.run, output)
    for row in report['rows']:
        for evidence in row['evidence']:
            evidence['url'] = relative(Path(evidence['run']) / 'result.json', output)
            visual = next((record for record in selected_lessons if record.get('visual_review')
                           and record['run'] == evidence['url']
                           and record['id'] in evidence['case_ids']), None)
            if visual:
                evidence.update(visual_status=visual['visual_status'], visual_finding=visual['visual_finding'],
                                visual_review_url=visual['visual_review'])
                if evidence['current'] and visual['visual_status'] == 'issue_open':
                    row['status'] = 'failure_observed'
    data = {'generated_utc': datetime.now(timezone.utc).isoformat(), 'coverage': report,
            'numbers': numbers, 'bindings': read(args.bindings),
            'lessons': selected_lessons,
            'release_ready': False, 'source_review_complete': False,
            'plan': [
                ['1', 'Tutorials and iDroid', 'Fix paused hands, menu exit and display fit; prove each state separately.'],
                ['2', 'Mission blockers', 'Mission 1 native binocular lesson and Mission 6 camera/control recovery.'],
                ['3', 'Rig, weapons and optics', 'Support hands, near-surface visibility, placement, aiming and NVG.'],
                ['4', 'Vehicles and compatibility', 'Mounted weapons, cabin roles, gamepad transitions, settings and mod coexistence.'],
                ['5', 'Complete feature tour', 'One recorded lesson per accepted feature, with synchronized 3D control cues.'],
                ['6', 'Release candidate', 'One frozen build; full regressions, headset checks, clean install/update/rollback and package hashes.']]}
    (output / 'data.js').write_text('window.RELEASE_DESK_DATA=' + json.dumps(data).replace('</', '<\\/') + ';', encoding='utf-8')
    (output / 'manifest.json').write_text(json.dumps({k: v for k, v in data.items() if k not in ('numbers', 'coverage', 'bindings')}, indent=2), encoding='utf-8')
    for name in ('release-desk.html', 'release-desk.css', 'release-desk.js', 'controller-player.js'):
        shutil.copy2(HERE / name, output / ('index.html' if name == 'release-desk.html' else name))
    shutil.copytree(HERE / 'vendor', output / 'vendor', dirs_exist_ok=True)
    (output / 'assets').mkdir(exist_ok=True)
    for name in ('meta-quest-touch-plus-left.glb', 'meta-quest-touch-plus-right.glb', 'LICENSE-webxr-input-profiles.md'):
        shutil.copy2(HERE / 'assets' / name, output / 'assets' / name)
    anchors = HERE / 'assets/controller-spatial-anchors.json'
    if anchors.exists():
        shutil.copy2(anchors, output / 'assets' / anchors.name)
    print(json.dumps({'output': str(output), 'rows': len(report['rows']),
                      'lessons_available': sum(r['available'] for r in data['lessons']),
                      'lessons_missing': sum(not r['available'] for r in data['lessons']), 'release_ready': False}))


if __name__ == '__main__':
    main()
