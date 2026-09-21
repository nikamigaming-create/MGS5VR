"""Run isolated Lua lifecycle fixtures using the live game's Lua 5.1 VM.

All game state and mutating APIs are mocked; no live actors/save state are used.
"""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/native_cabin.lua').read_text(encoding='utf-8')
tests = (root / 'tests/native_cabin_tests.lua').read_text(encoding='utf-8')
assert ']====]' not in source and ']====]' not in tests
script = 'return assert(loadstring([====[' + tests + ']====]))()([====[' + source + ']====])'
raise SystemExit(subprocess.call([sys.executable, str(root / 'tools/native-actions.py'), '--script', script]))
