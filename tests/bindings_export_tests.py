"""Exercise the compiled parser/export, including overrides, disabled axes and conflicts."""
import json
import pathlib
import subprocess
import sys
import tempfile

tool = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as temporary:
    path = pathlib.Path(temporary) / "controls.ini"
    path.write_text("[gameplay]\nreload=tap(left_grip + b,240)\n[axes]\nmove=right_stick\nmap=disabled\n", encoding="utf-8")
    data = json.loads(subprocess.check_output([str(tool), "--bindings-json", str(path)], text=True))
    actions = {a["name"]: a for a in data["actions"]}
    axes = {a["name"]: a["source"] for a in data["axes"]}
    assert actions["gameplay.reload"]["bindings"] == [{"gesture": "tap", "milliseconds": 240, "inputs": ["b", "left_grip"]}]
    assert actions["menus.dpad_up"]["bindings"] == []
    assert "gameplay" in actions["system.pause"]["contexts"]
    assert axes["axes.move"] == "right_stick" and axes["axes.map"] == "disabled"
    assert axes["axes.menu"] == "left_stick"
    path.write_text("[gameplay]\nreload=a\n", encoding="utf-8")
    failed = subprocess.run([str(tool), "--bindings-json", str(path)], capture_output=True, text=True)
    assert failed.returncode == 1 and failed.stdout == "" and "conflict" in failed.stderr
    print("Effective binding overrides, context, timing, disabled/default axes and atomic invalid-file rejection passed")
