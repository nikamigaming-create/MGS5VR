"""One checkout, one current play build, bounded local bot runs."""
from __future__ import annotations

import argparse
import collections
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / "private/workspace.json"
PLAY = ROOT / "play"
SCRATCH = ROOT / "artifacts/bot"
SUPPORTED_EXE = "085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45"
BINARIES = ("dinput8.dll", "mgs5vr_controls.exe")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def utc():
    return datetime.now(timezone.utc).isoformat()


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], cwd=ROOT, check=True, **kwargs)


def safe_tree(path, parent):
    """Validate the entire tree before a recursive move/removal on Windows."""
    path, parent = Path(path).absolute(), Path(parent).resolve()
    if path.resolve() == parent or not path.resolve().is_relative_to(parent):
        raise ValueError(f"Refusing path outside {parent}: {path}")
    for node in (path, *path.parents):
        if node.exists() and (node.is_symlink() or
                getattr(node.lstat(), "st_file_attributes", 0) & 0x400):
            raise ValueError(f"Linked path is not a managed workspace tree: {node}")
    if path.exists() and path.is_dir():
        for base, dirs, files in os.walk(path, followlinks=False):
            for name in dirs + files:
                node = Path(base) / name
                if node.is_symlink() or getattr(node.lstat(), "st_file_attributes", 0) & 0x400:
                    raise ValueError(f"Refusing recursive operation through link: {node}")
    return path


def remove_tree(path, parent):
    path = safe_tree(path, parent)
    if path.exists():
        shutil.rmtree(path)


def tree_bytes(path):
    return sum(p.stat().st_size for p in Path(path).rglob("*") if p.is_file())


def processes():
    if sys.platform != "win32":
        return []
    script = ("Get-CimInstance Win32_Process -Filter \"Name='mgsvtpp.exe'\" | "
              "Select-Object ProcessId,ExecutablePath,CreationDate | ConvertTo-Json -Compress")
    result = run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                 capture_output=True, text=True, timeout=15,
                 creationflags=subprocess.CREATE_NO_WINDOW)
    rows = json.loads(result.stdout.strip() or "null") or []
    return [rows] if isinstance(rows, dict) else rows


def settings():
    return read_json(SETTINGS) if SETTINGS.exists() else {}


def configure(args):
    value = settings()
    if args.game_dir:
        game = args.game_dir.resolve()
        if digest(game / "mgsvtpp.exe") != SUPPORTED_EXE:
            raise ValueError("Unsupported TPP executable")
        value["game_dir"] = str(game)
        for name in BINARIES:
            build = ROOT / "build/Release" / name
            if build.exists() and (game / name).exists() and digest(build) == digest(game / name):
                value.setdefault("trusted_local_files", {})[name] = digest(build)
    if args.proxy:
        source = args.proxy.resolve()
        local = ROOT / ".deps/meta-xr-operator"
        local.mkdir(parents=True, exist_ok=True)
        names = (source.name, "XrApiLayer_METAX_operator.dll", "XrApiLayer_METAX_operator.json")
        for name in names:
            if not (source.parent / name).is_file():
                raise ValueError(f"Incomplete operator dependency: {name}")
        hashes = {}
        for name in names:
            if (source.parent / name).resolve() != (local / name).resolve():
                shutil.copy2(source.parent / name, local / name)
            hashes[name] = digest(local / name)
        write_json(local / "provenance.json", {"source": str(source.parent), "files": hashes})
        value["operator_proxy"] = str(local / source.name)
    for key in ("runtime", "navigation"):
        candidate = getattr(args, key)
        if candidate:
            if not candidate.is_file():
                raise ValueError(f"Missing {key}: {candidate}")
            value[key] = str(candidate.resolve())
    value.setdefault("keep_runs", 3)
    value.setdefault("scratch_budget_bytes", 2 * 1024**3)
    write_json(SETTINGS, value)
    print(f"Local settings: {SETTINGS}")


def source_identity():
    paths = run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
                capture_output=True).stdout.decode("utf-8").split("\0")
    hasher = hashlib.sha256()
    for name in sorted(set(filter(None, paths))):
        path = ROOT / name
        hasher.update(name.encode("utf-8") + b"\0")
        hasher.update(digest(path).encode() if path.is_file() else b"missing")
    return {"commit": run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip(),
            "source_sha256": hasher.hexdigest(),
            "changes": run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout.splitlines()}


def promote(stage, play=None, previous=None):
    play = play or PLAY
    previous = previous or ROOT / "build/play-previous"
    safe_tree(stage, ROOT / "build")
    safe_tree(play, ROOT)
    if not stage.is_dir():
        raise ValueError("The checked play stage is missing")
    wanted = {p.relative_to(stage).as_posix(): digest(p) for p in stage.rglob("*") if p.is_file()}
    if not play.exists():
        stage.rename(play)
        return
    # Keep the actual directory and unchanged executable files in place. The
    # launcher/WebView can hold them open while a new game DLL is installed.
    # Back up once, replace changed files individually, commit BUILD.json last.
    old_manifest = play / "BUILD.json"
    managed = set(read_json(old_manifest).get("files", {})) if old_manifest.is_file() else set()
    obsolete = sorted(managed - wanted.keys())
    for name in (*wanted, *obsolete):
        safe_tree(play / name, play)
    remove_tree(previous, ROOT / "build")
    shutil.copytree(play, previous)
    replaced = []
    temporary = None
    try:
        for name in obsolete:
            target = play / name
            if target.is_file():
                target.unlink()
                replaced.append(name)
        order = sorted(wanted, key=lambda name: (name == "BUILD.json", name))
        for name in order:
            target = play / name
            if target.is_file() and digest(target) == wanted[name]:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            candidate = target.with_name(target.name + ".mgs5vr-tmp")
            safe_tree(candidate, play)
            if candidate.exists():
                raise ValueError(f"Unfinished play update file preserved: {candidate}")
            temporary = candidate
            shutil.copy2(stage / name, temporary)
            os.replace(temporary, target)
            temporary = None
            replaced.append(name)
        for name, expected in wanted.items():
            if digest(play / name) != expected:
                raise ValueError(f"Play update did not verify: {name}")
    except BaseException:
        for name in reversed(replaced):
            target, saved = play / name, previous / name
            if saved.is_file():
                shutil.copy2(saved, target)
            else:
                target.unlink()
        raise
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    remove_tree(stage, ROOT / "build")


def deploy(value, known_old=None):
    if not value.get("game_dir"):
        print("play/ refreshed; configure the game once to enable synchronization.")
        return
    if processes():
        raise ValueError("Close MGSV before synchronizing; play/ contains the new build.")
    game = Path(value["game_dir"]).resolve()
    safe_tree(game / "dinput8.dll", game)
    if digest(game / "mgsvtpp.exe") != SUPPORTED_EXE:
        raise ValueError("Unsupported TPP executable")
    record_path = game / "mgs5vr-install.json"
    record = read_json(record_path)
    if (record.get("schema") != 1 or record.get("product") != "MGS5VR theatre preview"
            or Path(record.get("game_dir", "")).resolve() != game):
        raise ValueError("Install record does not own this game folder")
    manifest = read_json(PLAY / "BUILD.json")
    for name in (*BINARIES, "mgs5vr.ini", "mgs5vr-controls.ini", "mgs5vr-install.json"):
        safe_tree(game / name, game)
        if not (game / name).is_file():
            raise ValueError(f"Incomplete recorded install: {name}")
    for name in BINARIES:
        if digest(PLAY / name) != manifest["files"][name]:
            raise ValueError(f"Current play build was modified: {name}")
        allowed = {record["files"].get(name)}
        allowed.add(value.get("trusted_local_files", {}).get(name))
        if name == "dinput8.dll":
            allowed.add(value.get("trusted_local_dll"))
        if known_old:
            allowed.add(known_old.get(name))
        for base in (PLAY, ROOT / "build/play-previous", ROOT / "build/Release"):
            if (base / name).exists():
                allowed.add(digest(base / name))
        if digest(game / name).lower() not in {h.lower() for h in allowed if h}:
            raise ValueError(f"Installed {name} has unknown provenance; preserved")
    run([PLAY / "mgs5vr_controls.exe", "--check", game / "mgs5vr-controls.ini"])
    personal = {name: digest(game / name) for name in ("mgs5vr.ini", "mgs5vr-controls.ini")}
    if all(digest(game / name) == digest(PLAY / name) for name in BINARIES) and record.get("development_build") == manifest["source"]:
        print("Installed game already matches play/.")
        return
    rollback = ROOT / "build/installed-previous"
    remove_tree(rollback, ROOT / "build")
    rollback.mkdir(parents=True)
    for name in (*BINARIES, "mgs5vr-install.json"):
        shutil.copy2(game / name, rollback / name)
    try:
        for name in BINARIES:
            temporary = game / (name + ".mgs5vr-tmp")
            safe_tree(temporary, game)
            shutil.copy2(PLAY / name, temporary)
            os.replace(temporary, game / name)
            record["files"][name] = digest(game / name)
        for name, expected in personal.items():
            if digest(game / name) != expected:
                raise ValueError(f"Personal settings changed during sync: {name}")
            record["files"][name] = expected
        record["installed_utc"] = utc()
        record["development_build"] = manifest["source"]
        write_json(record_path, record)
    except BaseException:
        for name in (*BINARIES, "mgs5vr-install.json"):
            shutil.copy2(rollback / name, game / name)
        raise
    finally:
        for name in BINARIES:
            temporary = game / (name + ".mgs5vr-tmp")
            if temporary.exists():
                temporary.unlink()
    value["trusted_local_dll"] = manifest["files"]["dinput8.dll"]
    value["trusted_local_files"] = {name: manifest["files"][name] for name in BINARIES}
    write_json(SETTINGS, value)
    print(f"Installed {manifest['files']['dinput8.dll'][:12]} from {PLAY}; personal settings preserved.")


def refresh(args):
    value = settings()
    if value.get("game_dir") and not args.no_deploy and processes():
        raise ValueError("Close MGSV before building and synchronizing the test installation")
    known_old = {name: digest(ROOT / "build/Release" / name) for name in BINARIES
                 if (ROOT / "build/Release" / name).exists()}
    run(["cmake", "--preset", "windows-x64", f"-DPython3_EXECUTABLE={sys.executable}"])
    run(["cmake", "--build", "--preset", "release", "--parallel"])
    run(["ctest", "--preset", "release", "--output-junit", ROOT / "build/current-tests.xml"])
    stage = ROOT / "build/play-stage"
    remove_tree(stage, ROOT / "build")
    run(["cmake", "--install", "build", "--config", "Release", "--component", "MGS5VR", "--prefix", stage])
    run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
         ROOT / "tests/installer_tests.ps1", "-PackageRoot", stage])
    files = {p.relative_to(stage).as_posix(): digest(p) for p in stage.rglob("*") if p.is_file()}
    write_json(stage / "BUILD.json", {"schema": 1, "built_utc": utc(), "source": source_identity(), "files": files,
               "validation": {"automated": "passed", "test_report": "build/current-tests.xml",
                              "simulator": "unproven_on_this_build", "headset": "unproven", "full_game": "unproven"}})
    promote(stage)
    if not args.no_deploy:
        deploy(value, known_old)
    print(f"Current test build: {PLAY}")


def prune_runs(value, latest=None):
    runs = SCRATCH / "runs"
    if not runs.exists():
        return
    if latest is None and (SCRATCH / "latest.json").exists():
        latest = safe_tree(read_json(SCRATCH / "latest.json")["path"], runs)
    candidates = [p for p in runs.iterdir() if p.is_dir() and (p / "managed-run.json").exists()
                  and (p / "finished.json").exists() and not (p / "KEEP").exists()]
    candidates.sort(key=lambda p: p.name, reverse=True)
    total = sum(tree_bytes(p) for p in candidates)
    for index, path in reversed(list(enumerate(candidates))):
        if path == latest:
            continue
        if index >= value.get("keep_runs", 3) or total > value.get("scratch_budget_bytes", 2 * 1024**3):
            total -= tree_bytes(path)
            remove_tree(path, runs)


def bot(args):
    value = settings()
    game, proxy = Path(value["game_dir"]), Path(value["operator_proxy"])
    manifest = read_json(PLAY / "BUILD.json")
    if digest(game / "dinput8.dll") != manifest["files"]["dinput8.dll"]:
        raise ValueError("Installed game differs from play/; synchronize before testing")
    prune_runs(value)
    output = SCRATCH / "runs" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    write_json(output / "managed-run.json", {"schema": 1, "source": manifest["source"], "started_utc": utc()})
    if args.operation == "navigate":
        command = [sys.executable, ROOT / "tools/gameplay-navigate.py", "--manifest", value["navigation"],
                   "--goal", *args.goal, "--mode", args.mode]
    else:
        command = [sys.executable, ROOT / "tools/gameplay-bot.py", args.command,
                   "--controls-tool", PLAY / "mgs5vr_controls.exe"]
        if args.suite:
            command += ["--suite", args.suite.resolve()]
        if args.record:
            command += ["--record"]
    command += ["--game-dir", game, "--proxy", proxy, "--output", output, "--seconds", args.seconds]
    code = 1
    try:
        print(f"Running {args.operation}; evidence: {output}", flush=True)
        with (output / "runner.log").open("w", encoding="utf-8") as log:
            code = subprocess.run([str(a) for a in command], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT).returncode
    finally:
        write_json(output / "finished.json", {"finished_utc": utc(), "exit_code": code})
        write_json(SCRATCH / "latest.json", {"path": str(output), "exit_code": code})
        prune_runs(value, output)
    if code:
        # A failed owned simulator run must not leave a test menu or stalled
        # game open. stop-sim checks exact process generations and keeps Steam.
        record = ROOT / "artifacts/simulator-steam-session.json"
        if record.exists() and read_json(record).get("launchMethod") == "direct":
            run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
                 ROOT / "tools/stop-simulator.ps1"])
        raise ValueError(f"Bot case needs attention: {output}")
    print(f"Latest bot evidence: {output}")


def launch_sim(args):
    value = settings()
    command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
               ROOT / "tools/launch-test-game.ps1", "-GameDir", value["game_dir"],
               "-RuntimeManifest", value["runtime"], "-OperatorDir", Path(value["operator_proxy"]).parent]
    run(command)


def resolution(args):
    value = settings()
    command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
               PLAY / "tools/launcher-display.ps1", "-Mode", "Apply",
               "-GameExe", Path(value["game_dir"]) / "mgsvtpp.exe"]
    if args.width is not None or args.height is not None:
        if args.width is None or args.height is None:
            raise ValueError("Custom resolution needs both --width and --height")
        command += ["-Preset", "Custom", "-Width", args.width, "-Height", args.height]
    else:
        command += ["-Preset", "Headset", "-Scale", args.scale]
    run(command)


def pin_latest(_):
    latest = read_json(SCRATCH / "latest.json")
    path = safe_tree(latest["path"], SCRATCH / "runs")
    (path / "KEEP").write_text("Retained acceptance/investigation evidence; do not prune.\n", encoding="utf-8")
    print(f"Pinned evidence: {path}")


def status(args):
    value = settings()
    manifest = read_json(PLAY / "BUILD.json") if (PLAY / "BUILD.json").exists() else None
    ledger = read_json(ROOT / "docs/COMMUNITY_VERIFICATION_2026-09-27.json")
    installed = None
    if value.get("game_dir") and (Path(value["game_dir"]) / "dinput8.dll").exists():
        installed = digest(Path(value["game_dir"]) / "dinput8.dll")
    info = {"checkout": str(ROOT), "play": str(PLAY), "build": manifest,
            "installed_dll_sha256": installed,
            "installed_matches_play": bool(manifest and installed == manifest["files"]["dinput8.dll"]),
            "source_matches_play": bool(manifest and source_identity()["source_sha256"] == manifest["source"]["source_sha256"]),
            "reports": dict(collections.Counter(r["status"] for r in ledger["reports"])),
            "claims": dict(collections.Counter(c["status"] for r in ledger["reports"] for c in r["claims"])),
            "ledger_evidence_dll": ledger["candidate"]["dll_sha256"],
            "latest_bot": read_json(SCRATCH / "latest.json") if (SCRATCH / "latest.json").exists() else None}
    if args.json:
        print(json.dumps(info, indent=2))
    else:
        print(f"Checkout: {ROOT}\nPlay: {PLAY}\nInstalled matches play: {info['installed_matches_play']}\nSource matches play: {info['source_matches_play']}")
        if manifest:
            print(f"DLL: {manifest['files']['dinput8.dll']}\nBuilt: {manifest['built_utc']}")
        print(f"Community reports: {info['reports']}\nScoped claims: {info['claims']}")
        print("Full-game and headset acceptance: open. Next cases: docs/CURRENT.md")


def coverage(args):
    """Generate one fixed review view without launching or controlling a game."""
    value = settings()
    command = [sys.executable, ROOT / "tools/gameplay-coverage.py",
               "--game-dir", Path(value["game_dir"]), "--candidate-dll", PLAY / "dinput8.dll",
               "--controls-tool", PLAY / "mgs5vr_controls.exe",
               "--output", ROOT / "artifacts/dev/coverage"]
    if args.menus:
        command += ["--menus-only"]
    for candidate in args.run:
        retained = safe_tree(candidate, SCRATCH / "runs")
        if not (retained / "KEEP").is_file():
            raise ValueError("Pin the reviewed run before using it as coverage evidence: " + str(retained))
        if not all((retained / name).is_file() for name in ("identity.json", "result.json")):
            raise ValueError("Retained coverage run needs identity.json and result.json: " + str(retained))
        command += ["--run", retained]
    run(command)
    print(f"Coverage view: {ROOT / 'artifacts/dev/coverage/index.html'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    setup = commands.add_parser("configure")
    for name in ("game-dir", "proxy", "runtime", "navigation"):
        setup.add_argument("--" + name, type=Path)
    setup.set_defaults(handler=configure)
    build = commands.add_parser("build")
    build.add_argument("--no-deploy", action="store_true")
    build.set_defaults(handler=refresh)
    commands.add_parser("deploy").set_defaults(handler=lambda _: deploy(settings()))
    observe = commands.add_parser("bot")
    observe.add_argument("--command", choices=("observe", "continue", "run", "session", "move", "idroid", "supervise"), default="observe")
    observe.add_argument("--suite", type=Path)
    observe.add_argument("--seconds", type=float, default=15)
    observe.add_argument("--record", action="store_true")
    observe.set_defaults(handler=bot)
    navigate = commands.add_parser("navigate")
    navigate.add_argument("--goal", type=float, nargs=3, required=True)
    navigate.add_argument("--mode", choices=("auto", "walk", "run", "crouch", "crawl"), default="auto")
    navigate.add_argument("--seconds", type=float, default=60)
    navigate.set_defaults(handler=bot)
    launch = commands.add_parser("launch-sim")
    launch.set_defaults(handler=launch_sim)
    sizing = commands.add_parser("resolution", help="Apply the active headset recommendation or custom native size")
    sizing.add_argument("--width", type=int)
    sizing.add_argument("--height", type=int)
    sizing.add_argument("--scale", type=int, default=100)
    sizing.set_defaults(handler=resolution)
    commands.add_parser("stop-sim").set_defaults(handler=lambda _: run([
        "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", ROOT / "tools/stop-simulator.ps1"]))
    commands.add_parser("pin").set_defaults(handler=pin_latest)
    current = commands.add_parser("status")
    current.add_argument("--json", action="store_true")
    current.set_defaults(handler=status)
    report = commands.add_parser("coverage", help="Refresh the fixed read-only coverage view")
    report.add_argument("--menus", action="store_true", help="Show VR menu paths by game state and presentation")
    report.add_argument("--run", action="append", type=Path, default=[], help="Reviewed, pinned bot evidence")
    report.set_defaults(handler=coverage)
    commands.add_parser("prune").set_defaults(handler=lambda _: prune_runs(settings()))
    args = parser.parse_args()
    try:
        args.handler(args)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
