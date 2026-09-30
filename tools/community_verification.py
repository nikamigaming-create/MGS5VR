"""Package reviewed community evidence without silently accepting changed files."""
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil

STATUSES = {'verified_automated', 'verified_simulator', 'failed', 'needs_headset',
            'not_tested', 'not_implemented'}
SUFFIXES = {'.json', '.png', '.jpg', '.txt', '.log', '.md', '.ps1', '.cpp', '.hpp', '.py', '.lua'}


def bundle(root, source, output):
    root, output = Path(root).resolve(), Path(output).resolve()
    data = json.loads(Path(source).read_text(encoding='utf-8-sig'))
    data = copy.deepcopy(data)
    ids = [r['id'] for r in data['reports']]
    if sorted(ids) != [f'R{i:02}' for i in range(1, 56)]:
        raise ValueError('Community ledger must retain each of R01–R55 exactly once')
    folder = output / 'evidence'
    folder.mkdir(parents=True, exist_ok=True)
    copied = {}

    def evidence(item, related=True):
        path = (root / item['path']).resolve()
        expected = item['sha256'].lower()
        if (not path.is_relative_to(root) or path.suffix.lower() not in SUFFIXES
                or not re.fullmatch('[a-f0-9]{64}', expected)):
            raise ValueError('Invalid evidence path or hash: ' + item['path'])
        if path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError('Evidence file is too large: ' + item['path'])
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError('Reviewed evidence has changed: ' + item['path'])
        name = expected + (path.suffix.lower() if path.suffix.lower() in {'.png', '.jpg', '.json'} else '.txt')
        if name not in copied:
            shutil.copy2(path, folder / name)
            copied[name] = len(raw)
        result = {**item, 'url': 'evidence/' + name,
                  'kind': 'image' if path.suffix.lower() in {'.png', '.jpg'} else 'text'}
        # A review binds exactly these captures. Include them for direct viewing,
        # never discover arbitrary sibling screenshots and call them reviewed.
        if related and path.name == 'visual-review.json':
            review = json.loads(raw)
            result['related'] = [evidence(x, False) for x in review.get('captures', [])
                                 if isinstance(x, dict) and 'path' in x and 'sha256' in x]
        return result

    for report in data['reports']:
        if not report['claims']:
            raise ValueError('Report has no testable claims: ' + report['id'])
        for claim in report['claims']:
            if claim['status'] not in STATUSES:
                raise ValueError('Unknown verification status: ' + claim['status'])
            if claim['status'].startswith('verified_') and not claim['evidence']:
                raise ValueError('Verified claim has no evidence: ' + claim['id'])
            claim['evidence'] = [evidence(x) for x in claim['evidence']]
    data['bundled_evidence'] = {'files': len(copied), 'bytes': sum(copied.values())}
    (output / 'community-verification.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    return data
