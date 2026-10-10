"""Run isolated read-only navigation-query fixtures in MGSV's Lua 5.1 VM."""
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'tools/navigation-state.lua').read_text(encoding='utf-8')
tests = (root / 'tests/native_navigation_tests.lua').read_text(encoding='utf-8')
assert ']====]' not in source and ']====]' not in tests
script = 'return assert(loadstring([====[' + tests + ']====]))()([====[' + source + ']====])'
result = subprocess.run([sys.executable, str(root / 'tools/native-actions.py'), '--script', script],
                        capture_output=True, text=True)
if result.returncode:
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)
record = json.loads(result.stdout)
value = json.loads(record['result'])  # Prove that even the overflow case is valid JSON.
print(json.dumps({'status': value['status'], 'checks': value['checks'],
                  'large_snapshot_bytes': len(json.dumps(value['large_snapshot'])),
                  'queued_ms': record['queued_ms'], 'execution_us': record['execution_us']}))
