"""Inventory runtime numeric literals and exported settings without changing them.

This is a lexical audit, not a claim that every number is a tunable parameter.
Comments and strings are excluded. Engine addresses, signatures and layout
contracts remain visible, with their source locations and source-file hashes.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
NUMBER = r"(?<![\w.])(?:0[xX][\da-fA-F']+(?:\.[\da-fA-F']*)?(?:[pP][+-]?\d+)?|0[bB][01']+|(?:\d[\d']*(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)(?:[uUlLfF]+)?(?!\w)"
CPP_SKIP = r'R"(?P<raw_delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=raw_delimiter)"|//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
LUA_SKIP = r'--\[(?P<comment_eq>=*)\[[\s\S]*?\](?P=comment_eq)\]|--[^\n]*|\[(?P<string_eq>=*)\[[\s\S]*?\](?P=string_eq)\]|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
ASM_SKIP = r';[^\n]*|"(?:""|[^"])*"|\'(?:\'\'|[^\'])*\''
ASM_NUMBER = r'(?<![\w.])(?:[0-9][0-9a-fA-F]*[hH]|[01]+[bB]|[0-7]+[oOqQ]|[0-9]+[dDtT]?)(?!\w)'
DEFINITION = re.compile(r'\{"(settings\.[\w]+)",\s*([\d.+-]+),\s*([\d.+-]+),\s*([\d.+-]+)\}')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def literals(source, lua=False, assembly=False):
    skip = ASM_SKIP if assembly else LUA_SKIP if lua else CPP_SKIP
    pattern = re.compile('(?:' + skip + ')|(?P<number>' + (ASM_NUMBER if assembly else NUMBER) + ')')
    lines = source.splitlines()
    line, previous = 1, 0
    for match in pattern.finditer(source):
        line += source.count('\n', previous, match.start())
        previous = match.start()
        if match.group('number') is None:
            continue
        text = lines[line - 1].strip()
        value = match.group('number')
        category = ('native_contract' if assembly or value.lower().startswith(('0x', '0b')) else
                    'named_or_declared' if re.search(r'\b(?:constexpr|const|enum)\b', text) else
                    'inline_numeric')
        yield {'line': line, 'column': match.start() - source.rfind('\n', 0, match.start()),
               'literal': value, 'category': category, 'context': text,
               'review': 'unreviewed'}


def build(root=ROOT, controls=None, checker=None):
    files, rows = [], []
    for folder in ('src', 'include'):
        for path in sorted((root / folder).rglob('*')):
            if path.suffix not in ('.cpp', '.hpp', '.h', '.lua', '.asm'):
                continue
            relative = path.relative_to(root).as_posix()
            files.append({'path': relative, 'sha256': digest(path), 'scan': 'lexical'})
            for item in literals(path.read_text(encoding='utf-8-sig'), path.suffix == '.lua', path.suffix == '.asm'):
                rows.append({'id': f"{relative}:{item['line']}:{item['column']}", 'file': relative, **item})
    definitions = (root / 'src/controls.cpp').read_text(encoding='utf-8-sig')
    settings = []
    current = {}
    if controls and checker:
        current = {r['name']: r['value'] for r in json.loads(subprocess.check_output(
            [str(checker), '--settings-json', str(controls)], text=True))}
    runtime_texts = {p['path']: (root / p['path']).read_text(encoding='utf-8-sig')
                     for p in files if p['scan'] == 'lexical'}
    for name, default, minimum, maximum in DEFINITION.findall(definitions):
        unit = next((unit for suffix, unit in [('_cm', 'cm'), ('_degrees', 'degrees'),
                    ('_ms', 'ms'), ('_percent', '%')] if name.endswith(suffix)),
                    'toggle' if float(maximum) == 1 else 'mode')
        references = []
        for file, source in runtime_texts.items():
            for line_number, line in enumerate(source.splitlines(), 1):
                if '"' + name + '"' in line:
                    references.append({'file': file, 'line': line_number, 'context': line.strip()})
        settings.append({'name': name, 'default': float(default), 'minimum': float(minimum),
                         'maximum': float(maximum), 'unit': unit, 'effective_value': current.get(name),
                         'references': references, 'review': 'runtime_effect_not_certified'})
    return {'schema': 1, 'scope': ['src', 'include'], 'source_files': files,
            'counts': dict(Counter(r['category'] for r in rows)), 'literal_count': len(rows),
            'settings': settings, 'literals': rows, 'complete_review': False,
            'limits': ['Lexical numeric inventory; not every numeric literal is an adjustable setting.',
                       'Names and units do not prove a setting affects the runtime.',
                       'C++, Lua and MASM assembly literals are inventoried; classification is heuristic and every row still needs review.',
                       'Tools, tests, third-party dependencies and generated files are outside this runtime-code scan.'],
            'known_findings': [{'name': 'settings.wrist_surface_lift_cm',
                'finding': 'Audit found this was published but ignored. Both native forearm paths now consume it; 0.005 m base clearance + 0.02 m default preserves the preceding geometry.',
                'status': 'source_change_pending_runtime_retest'}, {'name': 'settings.weapon_hud_setback_cm',
                'finding': 'New source setting replaces the recent fixed 6 cm compact-readout adjustment. Popup placement is independent.',
                'status': 'source_change_pending_runtime_retest'}]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--controls', type=Path)
    parser.add_argument('--checker', type=Path, default=ROOT / 'build/Release/mgs5vr_controls.exe')
    args = parser.parse_args()
    report = build(controls=args.controls, checker=args.checker)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'files': len(report['source_files']),
                      'literals': report['literal_count'], 'settings': len(report['settings']),
                      'complete_review': False}))
