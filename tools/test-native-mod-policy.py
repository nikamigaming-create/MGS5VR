"""Run isolated mod-policy contracts in the retail Lua 5.1 VM."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
quote = lambda path: '[====[' + (root / path).read_text(encoding='utf-8') + ']====]'
script = ('return assert(loadstring(' + quote('tests/native_mod_policy_tests.lua') + '))()('
          + quote('src/native_god_mode.lua') + ',' + quote('src/native_idroid_interruption.lua') + ')')
raise SystemExit(subprocess.call([sys.executable, str(root / 'tools/native-actions.py'), '--script', script]))
