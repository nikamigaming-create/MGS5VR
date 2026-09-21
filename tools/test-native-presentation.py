"""Run read-only native presentation fixtures in the game's Lua 5.1 VM."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/native_presentation.lua').read_text(encoding='utf-8')
tests = (root / 'tests/native_presentation_tests.lua').read_text(encoding='utf-8')
assert ']====]' not in source and ']====]' not in tests
script = 'return assert(loadstring([====[' + tests + ']====]))()([====[' + source + ']====])'
raise SystemExit(subprocess.call([sys.executable, str(root / 'tools/native-actions.py'), '--script', script]))
