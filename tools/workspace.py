"""One pinned play build, one development build, bounded local bot runs."""
from __future__ import annotations

import argparse
import collections
from datetime import datetime, timezone
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / "private/workspace.json"
PLAY = ROOT / "play"
DEV = ROOT / "build/dev"
SCRATCH = ROOT / "artifacts/bot"
SUPPORTED_EXE = "085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45"
BINARIES = ("dinput8.dll", "mgs5vr_controls.exe")
CONTROLLER_ASSETS = tuple('assets/controls/' + name for name in
    ('left.mesh', 'right.mesh', 'left.png', 'right.png', 'LICENSE.txt', 'provenance.json'))
RETAINED_CONTROL = '; MGS5VR newer-build preference '


def project_controls(text, supported):
    """Retain unavailable settings as comments; restore them for a newer slot."""
    result = []
    section = ''
    for line in text.splitlines(keepends=True):
        retained = re.fullmatch(r'; MGS5VR newer-build preference \[([a-z0-9_]+)\]: (.*?)(\r?\n)?', line)
        if retained:
            owner, original, ending = retained.groups()
            if owner != section:
                raise ValueError('Retained control preference moved outside its section')
            candidate = original + (ending or '')
        else:
            candidate = line
        header = re.fullmatch(r'\s*\[([^]]+)\]\s*', candidate.lstrip('\ufeff'))
        if header:
            section = header[1].strip().lower()
        entry = re.match(r'\s*([^;#=]+?)\s*=', candidate)
        if entry:
            name = section + '.' + entry[1].strip().lower()
            if name not in supported:
                candidate = RETAINED_CONTROL + '[' + section + ']: ' + candidate
        result.append(candidate)
    return ''.join(result)


def control_catalog(checker):
    bindings = json.loads(run([checker, '--bindings-json'], capture_output=True, text=True).stdout)
    settings_rows = json.loads(run([checker, '--settings-json'], capture_output=True, text=True).stdout)
    return {row['name'] for row in (*bindings['actions'], *bindings['axes'], *settings_rows)}


def compatible_controls(build, path):
    original = path.read_bytes()
    text = original.decode('utf-8-sig')
    checked = False
    try:
        run([build / 'mgs5vr_controls.exe', '--check', path], capture_output=True, text=True)
        checked = True
    except subprocess.CalledProcessError:
        pass
    if checked and RETAINED_CONTROL not in text:
        return original
    # The checked development parser validates every preference before the
    # published parser sees a projection. Typos cannot silently become comments.
    checked_build(DEV)
    full = project_controls(text, control_catalog(DEV / 'mgs5vr_controls.exe'))
    if RETAINED_CONTROL in full:
        raise ValueError('Personal controls contain a preference unknown to checked development')
    projected = project_controls(full, control_catalog(build / 'mgs5vr_controls.exe'))
    with __import__('tempfile').TemporaryDirectory(prefix='mgs5vr-controls-') as temporary:
        full_path, projected_path = Path(temporary) / 'full.ini', Path(temporary) / 'slot.ini'
        full_path.write_text(full, encoding='utf-8', newline='')
        projected_path.write_text(projected, encoding='utf-8', newline='')
        run([DEV / 'mgs5vr_controls.exe', '--check', full_path])
        run([build / 'mgs5vr_controls.exe', '--check', projected_path])
    return projected.encode('utf-8')


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
              "Where-Object { $mgsProcess=Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue; $mgsProcess -and !$mgsProcess.HasExited } | "
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


def checked_build(path, pin=None):
    """Verify a complete slot before installing it or discarding older copies."""
    safe_tree(path, ROOT)
    manifest = read_json(path / "BUILD.json")
    if pin and digest(path / "BUILD.json") != pin["manifest_sha256"]:
        raise ValueError("Pinned play manifest changed; refusing to install an unrecognized build")
    if manifest.get("validation", {}).get("automated") != "passed":
        raise ValueError(f"Build has not passed automated checks: {path}")
    if not set(BINARIES).issubset(manifest.get("files", {})):
        raise ValueError(f"Incomplete build manifest: {path}")
    for name, expected in manifest["files"].items():
        target = safe_tree(path / name, path)
        if not target.is_file() or digest(target) != expected:
            raise ValueError(f"Checked build file changed: {target}")
    return manifest


def promote(stage, play=None, previous=None):
    # Low-level replacement defaults to development. Only pin_build selects PLAY.
    play = play or DEV
    previous = previous or ROOT / "build/slot-rollback"
    safe_tree(stage, ROOT / "build")
    safe_tree(play, ROOT)
    safe_tree(previous, ROOT / "build")
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
    if previous.exists():
        raise ValueError(f"Unfinished build transaction preserved: {previous}")
    shutil.copytree(play, previous)
    replaced = []
    temporary = None
    recovered = False
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
        recovered = True
    except BaseException:
        for name in reversed(replaced):
            target, saved = play / name, previous / name
            if saved.is_file():
                shutil.copy2(saved, target)
            else:
                target.unlink()
        recovered = True
        raise
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
        if recovered:
            remove_tree(previous, ROOT / "build")
    remove_tree(stage, ROOT / "build")


def deploy(value, known_old=None, build=None):
    build = build or PLAY
    manifest = checked_build(build, value.get("pinned_build") if build == PLAY else None)
    if not value.get("game_dir"):
        raise ValueError("Configure the game once before installing a build")
    running = bool(processes())
    game = Path(value["game_dir"]).resolve()
    safe_tree(game / "dinput8.dll", game)
    if digest(game / "mgsvtpp.exe") != SUPPORTED_EXE:
        raise ValueError("Unsupported TPP executable")
    record_path = game / "mgs5vr-install.json"
    record = read_json(record_path)
    if (record.get("schema") != 1 or record.get("product") != "MGS5VR theatre preview"
            or Path(record.get("game_dir", "")).resolve() != game):
        raise ValueError("Install record does not own this game folder")
    if running and any(not (game / name).is_file() or digest(game / name) != digest(build / name)
                       for name in BINARIES):
        raise ValueError("Close MGSV before switching builds; both retained builds are unchanged")
    for name in (*BINARIES, "mgs5vr.ini", "mgs5vr-controls.ini", "mgs5vr-install.json"):
        safe_tree(game / name, game)
        if not (game / name).is_file():
            raise ValueError(f"Incomplete recorded install: {name}")
    for name in BINARIES:
        allowed = {record["files"].get(name)}
        allowed.add(value.get("trusted_local_files", {}).get(name))
        if name == "dinput8.dll":
            allowed.add(value.get("trusted_local_dll"))
        if known_old:
            allowed.add(known_old.get(name))
        for base in (PLAY, DEV, ROOT / "build/Release"):
            if (base / name).exists():
                allowed.add(digest(base / name))
        if digest(game / name).lower() not in {h.lower() for h in allowed if h}:
            raise ValueError(f"Installed {name} has unknown provenance; preserved")
    controls = compatible_controls(build, game / "mgs5vr-controls.ini")
    asset_files = tuple(name for name in CONTROLLER_ASSETS
                        if name in manifest['files'] or name in record['files'] or name.replace('/', '\\') in record['files'])
    for name in asset_files:
        safe_tree(game / name, game)
        if (game / name).exists():
            allowed = {record['files'].get(name), record['files'].get(name.replace('/', '\\'))}
            allowed.update(digest(base / name) for base in (PLAY, DEV) if (base / name).is_file())
            if digest(game / name) not in {h.lower() for h in allowed if h}:
                raise ValueError(f'Installed controller asset has unknown provenance; preserved: {name}')
    assets_changed = any((not (game / name).is_file() or digest(game / name) != manifest['files'][name])
                         if name in manifest['files'] else (game / name).exists() for name in asset_files)
    if running and assets_changed:
        raise ValueError('Close MGSV before changing the controller models')
    controls_changed = controls != (game / "mgs5vr-controls.ini").read_bytes()
    if running and controls_changed:
        raise ValueError("Close MGSV before changing the build's control-settings compatibility")
    personal = {name: digest(game / name) for name in ("mgs5vr.ini", "mgs5vr-controls.ini")}
    slot = "play" if build == PLAY else "dev"
    if (all(digest(game / name) == digest(build / name) for name in BINARIES)
            and record.get("development_build") == manifest["source"] and record.get("build_slot") == slot and not controls_changed and not assets_changed):
        print(f"Installed game already matches {slot}.")
        return
    rollback = ROOT / "build/install-rollback"
    if rollback.exists():
        raise ValueError(f"Unfinished install transaction preserved: {rollback}")
    rollback.mkdir(parents=True)
    transaction_files = (*BINARIES, "mgs5vr-install.json", "mgs5vr-controls.ini", *asset_files)
    for name in transaction_files:
        if (game / name).is_file():
            (rollback / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(game / name, rollback / name)
    recovered = False
    try:
        for name in BINARIES:
            # Tool/documentation changes can refresh the build record while
            # the unchanged DLL is loaded by a live game. Do not replace an
            # already matching binary just to update that record.
            if digest(game / name) != digest(build / name):
                temporary = game / (name + ".mgs5vr-tmp")
                safe_tree(temporary, game)
                shutil.copy2(build / name, temporary)
                os.replace(temporary, game / name)
            record["files"][name] = digest(game / name)
        for name, expected in personal.items():
            if digest(game / name) != expected:
                raise ValueError(f"Personal settings changed during sync: {name}")
            record["files"][name] = expected
        for name in asset_files:
            if name in manifest['files']:
                (game / name).parent.mkdir(parents=True, exist_ok=True)
                temporary = game / (name + '.mgs5vr-tmp')
                safe_tree(temporary, game)
                shutil.copy2(build / name, temporary)
                os.replace(temporary, game / name)
                record['files'].pop(name, None)
                record['files'][name.replace('/', '\\')] = digest(game / name)
            else:
                if (game / name).exists():
                    (game / name).unlink()
                record['files'].pop(name, None)
                record['files'].pop(name.replace('/', '\\'), None)
        if controls_changed:
            temporary = game / 'mgs5vr-controls.ini.mgs5vr-tmp'
            safe_tree(temporary, game)
            temporary.write_bytes(controls)
            os.replace(temporary, game / 'mgs5vr-controls.ini')
            record['files']['mgs5vr-controls.ini'] = digest(game / 'mgs5vr-controls.ini')
        record["installed_utc"] = utc()
        record["development_build"] = manifest["source"]
        record["build_slot"] = slot
        write_json(record_path, record)
        recovered = True
    except BaseException:
        for name in transaction_files:
            if (rollback / name).is_file() and (not (game / name).is_file() or digest(rollback / name) != digest(game / name)):
                shutil.copy2(rollback / name, game / name)
            elif not (rollback / name).exists() and (game / name).exists():
                (game / name).unlink()
        recovered = True
        raise
    finally:
        for name in (*BINARIES, 'mgs5vr-controls.ini', *asset_files):
            temporary = game / (name + ".mgs5vr-tmp")
            if temporary.exists():
                temporary.unlink()
        if recovered:
            remove_tree(rollback, ROOT / "build")
    value["trusted_local_dll"] = manifest["files"]["dinput8.dll"]
    value["trusted_local_files"] = {name: manifest["files"][name] for name in BINARIES}
    write_json(SETTINGS, value)
    print(f"Installed {manifest['files']['dinput8.dll'][:12]} from {build}; personal settings preserved.")


def pin_build(args):
    value = settings()
    if args.current:
        manifest = checked_build(PLAY, value.get("pinned_build"))
    else:
        if processes():
            raise ValueError("Close MGSV before promoting the development build")
        manifest = checked_build(DEV)
        if PLAY.exists():
            checked_build(PLAY, value.get("pinned_build"))
        stage = ROOT / "build/pin-stage"
        if stage.exists():
            raise ValueError(f"Unfinished pin transaction preserved: {stage}")
        shutil.copytree(DEV, stage)
        promote(stage, PLAY)
    value["pinned_build"] = {"pinned_utc": utc(), "manifest_sha256": digest(PLAY / "BUILD.json"),
                             "dll_sha256": manifest["files"]["dinput8.dll"],
                             "validation": manifest["validation"]}
    write_json(SETTINGS, value)
    print(f"Pinned play build: {manifest['files']['dinput8.dll'][:12]}. Headset/full-game acceptance is unchanged.")
    if not args.current:
        deploy(value)


def refresh(args):
    value = settings()
    install = getattr(args, "deploy", False)
    if value.get("game_dir") and install and processes():
        raise ValueError("Close MGSV before building and synchronizing the test installation")
    known_old = {name: digest(ROOT / "build/Release" / name) for name in BINARIES
                 if (ROOT / "build/Release" / name).exists()}
    run(["cmake", "--preset", "windows-x64", f"-DPython3_EXECUTABLE={sys.executable}"])
    run(["cmake", "--build", "--preset", "release", "--parallel"])
    run(["ctest", "--preset", "release", "--output-junit", ROOT / "build/current-tests.xml"])
    stage = ROOT / "build/dev-stage"
    if (ROOT / "build/slot-rollback").exists():
        raise ValueError("Recover the unfinished build/slot-rollback transaction before building again")
    remove_tree(stage, ROOT / "build")
    try:
        run(["cmake", "--install", "build", "--config", "Release", "--component", "MGS5VR", "--prefix", stage])
        run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
             ROOT / "tests/installer_tests.ps1", "-PackageRoot", stage])
        files = {p.relative_to(stage).as_posix(): digest(p) for p in stage.rglob("*") if p.is_file()}
        write_json(stage / "BUILD.json", {"schema": 1, "built_utc": utc(), "source": source_identity(), "files": files,
                   "validation": {"automated": "passed", "test_report": "build/current-tests.xml",
                                  "simulator": "unproven_on_this_build", "headset": "unproven", "full_game": "unproven"}})
        checked_build(stage)
        promote(stage)
    finally:
        if not (ROOT / "build/slot-rollback").exists():
            remove_tree(stage, ROOT / "build")
    if install:
        deploy(value, known_old, DEV)
    print(f"Development build: {DEV}\nPinned play build unchanged: {PLAY}")


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


def retained_field_session(output, game, manifest):
    """Keep the same live field after a failed case with verified cleanup."""
    from gameplay_bot.live import released_field_state
    try:
        receipt = read_json(output / 'released-session.json')
        identity = receipt['identity']
        age = time.time_ns() - receipt['unix_ns']
        if (receipt.get('schema') != 1 or receipt.get('status') != 'released_field'
                or not 0 <= age <= 15_000_000_000 or not released_field_state(receipt['state'])
                or identity.get('dll_sha256') != manifest['files']['dinput8.dll']):
            return False
        current = [p for p in processes() if p.get('ExecutablePath')
                   and Path(p['ExecutablePath']).resolve() == game.resolve() / 'mgsvtpp.exe']
        return (len(current) == 1 and all(current[0].get(key) == identity['process'].get(key)
                for key in ('ProcessId', 'CreationDate')))
    except (OSError, ValueError, KeyError, TypeError):
        return False


def bot(args):
    if args.operation == 'bot' and args.command != 'mortar' and getattr(args, 'policy', None):
        raise ValueError('--policy belongs to mortar; use --suite for body and other bot case policies')
    idle_seconds = getattr(args, 'idle_seconds', None)
    if idle_seconds is not None and (args.operation != 'bot' or args.command != 'supervise'
            or not math.isfinite(idle_seconds) or not 0. < idle_seconds <= 180.):
        raise ValueError('--idle-seconds requires bot --command supervise and 0..180 seconds')
    if args.operation == 'bot' and args.command == 'mortar':
        if (not args.record or not args.policy or not args.policy.is_file()
                or not math.isfinite(args.seconds) or not 15. <= args.seconds <= 120.):
            raise ValueError('bot mortar requires --record, an existing --policy and 15..120 seconds')
    if args.operation == 'motherpuffer' and (not math.isfinite(args.seconds) or not 15. <= args.seconds <= 120.):
        raise ValueError('motherpuffer requires 15..120 seconds')
    selected_transition = getattr(args, 'transition', None)
    if args.operation == 'bot' and ((args.command == 'state') != bool(selected_transition)):
        raise ValueError('state requires --transition; other commands do not accept it')
    if args.operation in ('idroid-film', 'emplacement'):
        valid_seconds = (15. <= args.seconds <= 90. if args.operation == 'idroid-film' else args.seconds >= 0.)
        if not args.record or not math.isfinite(args.seconds) or not valid_seconds:
            raise ValueError(f'{args.operation} requires --record and a valid finite duration')
        if args.operation == 'idroid-film' and args.goal_node < 0:
            raise ValueError('idroid-film requires a nonnegative reviewed --goal-node')
        if args.operation == 'emplacement' and not args.policy.is_file():
            raise ValueError('emplacement requires an existing reviewed --policy file')
    value = settings()
    game, proxy = Path(value["game_dir"]), Path(value["operator_proxy"])
    selected_build = PLAY if getattr(args, 'slot', 'dev') == 'play' else DEV
    manifest = checked_build(selected_build)
    if digest(game / "dinput8.dll") != manifest["files"]["dinput8.dll"]:
        raise ValueError(f"Installed game differs from {selected_build}; deploy the selected slot before testing")
    prune_runs(value)
    output = SCRATCH / "runs" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    write_json(output / "managed-run.json", {"schema": 1, "source": manifest["source"], "started_utc": utc()})
    if args.operation == "navigate":
        command = [sys.executable, ROOT / "tools/gameplay-navigate.py", "--manifest", value["navigation"],
                   "--goal", *args.goal, "--mode", args.mode, "--arrival-radius",args.arrival_radius]
        if args.interrupt_on_danger:
            command += ["--interrupt-on-danger"]
        if args.no_recenter:
            command += ["--no-recenter"]
        if args.record:
            command += ["--record"]
    elif args.operation == "combat":
        command = [sys.executable, ROOT / "tools/gameplay-combat.py", "--max-rounds", args.max_rounds, "--radius", args.radius]
        if args.camp:
            command += ["--camp", args.camp]
        if getattr(args, 'target', None):
            command += ['--target', args.target]
        if getattr(args, 'enemy_helicopter', False):
            command += ['--enemy-helicopter']
        if getattr(args, 'one_handed', False):
            command += ['--one-handed']
    elif args.operation == "autonomy":
        command = [sys.executable, ROOT / "tools/gameplay-autonomy.py", "--manifest", value["navigation"], "--camp", args.camp,
                   "--stealth-loadout", args.stealth_loadout.resolve()]
        if args.record:
            command += ["--record"]
        if args.emplacement:
            command += ["--emplacement", args.emplacement.resolve()]
        if args.body_handling:
            command += ["--body-handling"]
    elif args.operation == "idroid-film":
        command = [sys.executable, ROOT / "tools/gameplay-idroid-film.py",
                   "--navigation-manifest", value["navigation"], "--goal-node", args.goal_node,
                   "--expected-mission", 30010, "--expected-location", 10,
                   "--expected-sequence", "Seq_Game_FreePlay", "--record"]
    elif args.operation == "emplacement":
        command = [sys.executable, ROOT / "tools/gameplay-emplacement.py",
                   "--manifest", value["navigation"], "--policy", args.policy.resolve(), "--record"]
        if args.diagnostic_active_camp:
            command += ["--diagnostic-active-camp"]
    elif args.operation == "motherpuffer":
        command = [sys.executable, ROOT / 'tools/gameplay-motherpuffer.py', args.mode,
                   '--vehicle-id', args.vehicle_id, '--record']
    elif args.operation == "mortar-response":
        if not args.policy.is_file() or not math.isfinite(args.seconds) or not 15. <= args.seconds <= 60.:
            raise ValueError('mortar-response requires an existing --policy and 15..60 seconds')
        command = [sys.executable, ROOT / 'private/mortar-native-response-probe.py',
                   '--policy', args.policy.resolve(), '--record']
        if args.enter:
            command += ['--enter']
    elif args.operation == 'bot' and args.command == 'mortar':
        command = [sys.executable, ROOT / 'tools/gameplay-mortar.py', '--manifest', value['navigation'],
                   '--policy', args.policy.resolve(), '--record']
    else:
        command = [sys.executable, ROOT / "tools/gameplay-bot.py", args.command,
                   "--controls-tool", DEV / "mgs5vr_controls.exe"]
        if args.suite:
            command += ["--suite", args.suite.resolve()]
        if args.record:
            command += ["--record"]
        if selected_transition:
            command += ['--transition', selected_transition]
        if getattr(args, 'trace_menu_input', False):
            command += ['--trace-menu-input']
        if getattr(args, 'idle_seconds', None) is not None:
            command += ['--idle-seconds', args.idle_seconds]
    command += ["--game-dir", game, "--proxy", proxy, "--output", output, "--seconds", args.seconds]
    code = 1
    try:
        print(f"Running {args.operation}; evidence: {output}", flush=True)
        with (output / "runner.log").open("w", encoding="utf-8") as log:
            environment = dict(os.environ)
            environment["MGS5VR_OPERATOR_URL"] = f"http://127.0.0.1:{value.get('operator_port', 8720)}"
            code = subprocess.run([str(a) for a in command], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                  env=environment).returncode
    finally:
        write_json(output / "finished.json", {"finished_utc": utc(), "exit_code": code})
        write_json(SCRATCH / "latest.json", {"path": str(output), "exit_code": code})
        prune_runs(value, output)
    if code:
        retained = retained_field_session(output, game, manifest)
        write_json(output / 'session-handoff.json', {'schema': 1,
                   'status': 'retained_released_field' if retained else 'needs_recovery',
                   'utc': utc(), 'case_exit_code': code, 'action_acceptance': False,
                   'automatic_shutdown': False})
        # A case result is not authority to terminate the user's live session.
        # Inputs still expire and unwind; unresolved cleanup is handed to the
        # operator for recovery rather than destroying the camp position.
        detail = 'live field retained with inputs released' if retained else 'live session kept for recovery'
        raise ValueError(f'Case needs correction; {detail}: {output}')
    print(f"Latest bot evidence: {output}")


def launch_sim(args):
    value = settings()
    deploy(value, build=PLAY if getattr(args, 'slot', 'dev') == 'play' else DEV)
    command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
               ROOT / "tools/launch-test-game.ps1", "-GameDir", value["game_dir"],
               "-RuntimeManifest", value["runtime"], "-OperatorDir", Path(value["operator_proxy"]).parent,
               "-OperatorPort", value.get("operator_port", 8720)]
    if args.tracking_fault_test:
        command += ["-TrackingFaultTest"]
    if getattr(args, "idroid_ui_boundary_trace", False):
        command += ["-IdroidUiBoundaryTrace"]
    if getattr(args, "camera_evidence_dir", None):
        command += ["-CameraEvidenceDir", args.camera_evidence_dir.resolve()]
    if getattr(args, "producer_pacing_test", "baseline") == "display":
        command += ["-ProducerPacingTest", "display", "-ProducerPacingTestSeconds", args.producer_pacing_test_seconds]
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
    development = read_json(DEV / "BUILD.json") if (DEV / "BUILD.json").exists() else None
    source = source_identity()
    ledger = read_json(ROOT / "docs/COMMUNITY_VERIFICATION_2026-09-27.json")
    installed = None
    if value.get("game_dir") and (Path(value["game_dir"]) / "dinput8.dll").exists():
        installed = digest(Path(value["game_dir"]) / "dinput8.dll")
    info = {"checkout": str(ROOT), "play": str(PLAY), "build": manifest,
            "dev": str(DEV), "development_build": development, "pinned_build": value.get("pinned_build"),
            "pin_matches_play": bool(value.get("pinned_build") and manifest
                                     and digest(PLAY / "BUILD.json") == value["pinned_build"]["manifest_sha256"]),
            "installed_dll_sha256": installed,
            "installed_matches_play": bool(manifest and installed == manifest["files"]["dinput8.dll"]),
            "installed_matches_dev": bool(development and installed == development["files"]["dinput8.dll"]),
            "source_matches_play": bool(manifest and source["source_sha256"] == manifest["source"]["source_sha256"]),
            "source_matches_dev": bool(development and source["source_sha256"] == development["source"]["source_sha256"]),
            "reports": dict(collections.Counter(r["status"] for r in ledger["reports"])),
            "claims": dict(collections.Counter(c["status"] for r in ledger["reports"] for c in r["claims"])),
            "ledger_evidence_dll": ledger["candidate"]["dll_sha256"],
            "latest_bot": read_json(SCRATCH / "latest.json") if (SCRATCH / "latest.json").exists() else None}
    if args.json:
        print(json.dumps(info, indent=2))
    else:
        print(f"Checkout: {ROOT}\nPinned play: {PLAY}\nDevelopment: {DEV}")
        print(f"Pin matches play: {info['pin_matches_play']}\nInstalled matches play: {info['installed_matches_play']}\nInstalled matches dev: {info['installed_matches_dev']}\nSource matches dev: {info['source_matches_dev']}")
        if manifest:
            print(f"Pinned DLL: {manifest['files']['dinput8.dll']}\nPinned built: {manifest['built_utc']}")
        if development:
            print(f"Development DLL: {development['files']['dinput8.dll']}\nDevelopment built: {development['built_utc']}")
        print(f"Community reports: {info['reports']}\nScoped claims: {info['claims']}")
        print("Full-game and headset acceptance: open. Next cases: docs/CURRENT.md")


def coverage(args):
    """Generate one fixed review view without launching or controlling a game."""
    value = settings()
    command = [sys.executable, ROOT / "tools/gameplay-coverage.py",
               "--game-dir", Path(value["game_dir"]), "--candidate-dll", DEV / "dinput8.dll",
               "--controls-tool", DEV / "mgs5vr_controls.exe",
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


def game_model(args):
    value = settings()
    command = [sys.executable, ROOT / 'tools/game-model.py', '--game-dir', Path(value['game_dir']),
               '--candidate-dll', DEV / 'dinput8.dll', '--controls-tool', DEV / 'mgs5vr_controls.exe',
               '--output', ROOT / 'artifacts/dev/coverage']
    if value.get('navigation'):
        command += ['--navigation', Path(value['navigation'])]
    for candidate in args.run:
        retained = safe_tree(candidate, SCRATCH / 'runs')
        if not (retained / 'KEEP').is_file():
            raise ValueError('Pin the reviewed run before using it as model evidence: ' + str(retained))
        command += ['--run', retained]
    run(command)
    print(f"Game model: {ROOT / 'artifacts/dev/coverage/game-model.html'}")


def items(args):
    if args.action == 'refresh':
        run(['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
             '-File', ROOT / 'tools/catalog-owned-assets.ps1', '-Game', settings()['game_dir']])
        run([sys.executable, ROOT / 'tools/item-catalog.py', 'build'])
        run([sys.executable, ROOT / 'tools/item-catalog.py', 'view'])
    else:
        command = [sys.executable, ROOT / 'tools/item-catalog.py', args.action]
        if args.action == 'search':
            if not args.query:
                raise ValueError('items search requires a query')
            command += [args.query, '--category', args.category, '--limit', str(args.limit)]
        elif args.action == 'show':
            if not args.query:
                raise ValueError('items show requires an exact equipment ID or asset path')
            command.append(args.query)
        run(command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    setup = commands.add_parser("configure")
    for name in ("game-dir", "proxy", "runtime", "navigation"):
        setup.add_argument("--" + name, type=Path)
    setup.set_defaults(handler=configure)
    build = commands.add_parser("build")
    deployment = build.add_mutually_exclusive_group()
    deployment.add_argument("--no-deploy", action="store_true", help="Compatibility flag; builds already leave the installed game alone")
    deployment.add_argument("--deploy", action="store_true", help="Explicitly install the checked development build")
    build.set_defaults(handler=refresh)
    install = commands.add_parser("deploy", help="Switch the installed game without changing either retained build")
    install.add_argument("--slot", choices=("play", "dev"), default="play")
    install.set_defaults(handler=lambda args: deploy(settings(), build=PLAY if args.slot == "play" else DEV))
    pinned = commands.add_parser("pin-build", help="Explicitly replace pinned play with the checked development build")
    pinned.add_argument("--current", action="store_true", help="Record the existing play build as pinned without replacing it")
    pinned.set_defaults(handler=pin_build)
    cleanup = commands.add_parser("clean-builds", help="Inventory or remove recognized old builds; retain the two verified slots")
    cleanup.add_argument("--apply", action="store_true")
    def clean_builds(args):
        from build_retention import clean
        clean(sys.modules[__name__], args)
    cleanup.set_defaults(handler=clean_builds)
    observe = commands.add_parser("bot")
    observe.add_argument('--slot', choices=('dev','play'), default='dev', help='Verify the selected retained build before testing')
    observe.add_argument("--command", choices=("observe", "showcase", "showcase-setup", "title", "cold-start", "cinematic", "profile", "bench", "bench-film", "studio-manager", "controls-editor", "body", "studio-field", "studio-board", "studio-hands", "continue", "run", "session", "move", "idroid", "optics", "supervise", "state", "mortar", "cqc", "recon"), default="observe")
    observe.add_argument('--policy', type=Path, help='mortar: reviewed authored fixture and operator position')
    observe.add_argument("--suite", type=Path)
    observe.add_argument("--seconds", type=float, default=15)
    observe.add_argument('--idle-seconds', type=float, help='Supervisor review window before neutral cleanup')
    observe.add_argument("--record", action="store_true")
    observe.add_argument("--trace-menu-input", action="store_true", help="Explicit bounded native UI query trace for a selected run/session suite")
    observe.add_argument('--transition', help='state: explicitly selected guarded transition')
    observe.set_defaults(handler=bot)
    navigate = commands.add_parser("navigate")
    navigate.add_argument("--goal", type=float, nargs=3, required=True)
    navigate.add_argument("--mode", choices=("auto", "stealth", "walk", "run", "crouch", "crawl"), default="auto")
    navigate.add_argument("--interrupt-on-danger", action="store_true")
    navigate.add_argument("--no-recenter", action="store_true")
    navigate.add_argument("--record", action="store_true")
    navigate.add_argument("--seconds", type=float, default=0, help='Optional total limit; zero uses progress guards')
    navigate.add_argument("--arrival-radius", type=float, default=3.5)
    navigate.set_defaults(handler=bot)
    combat = commands.add_parser("combat", help="Bounded native-observed combat through physical VR hand inputs")
    combat.add_argument("--seconds", type=float, default=30.)
    combat.add_argument("--camp")
    combat.add_argument('--target', help='Exact native soldier name for a mission objective; no visibility override')
    combat.add_argument('--enemy-helicopter',action='store_true',help='Explicit bounded Mission18 helicopter test')
    combat.add_argument('--one-handed',action='store_true',help='Explicit helicopter one-hand firing case')
    combat.add_argument("--max-rounds", type=int, default=24)
    combat.add_argument("--radius", type=float, default=80.)
    combat.set_defaults(handler=bot)
    autonomy = commands.add_parser("autonomy", help="Approach, engage and verify a named native camp without agent intervention")
    autonomy.add_argument("--camp", required=True)
    autonomy.add_argument("--seconds", type=float, default=180.)
    autonomy.add_argument("--record", action="store_true")
    autonomy.add_argument("--stealth-loadout",type=Path,required=True)
    autonomy.add_argument("--emplacement",type=Path)
    autonomy.add_argument("--body-handling",action="store_true")
    autonomy.set_defaults(handler=bot)
    idroid_film = commands.add_parser("idroid-film", help="Record a bounded current-state physical iDroid control probe")
    idroid_film.add_argument("--goal-node", type=int, required=True)
    idroid_film.add_argument("--seconds", type=float, default=90.)
    idroid_film.add_argument("--record", action="store_true", required=True)
    idroid_film.set_defaults(handler=bot)
    emplacement = commands.add_parser("emplacement", help="Record a bounded native-observed emplacement interaction")
    emplacement.add_argument("--policy", type=Path, required=True)
    emplacement.add_argument("--seconds", type=float, default=0., help='Optional total limit; zero uses progress guards')
    emplacement.add_argument("--record", action="store_true", required=True)
    emplacement.add_argument("--diagnostic-active-camp", action="store_true",
                             help="Explicit diagnostic mount/axes/fire/dismount probe; does not certify camp security")
    emplacement.set_defaults(handler=bot)
    mortar_response = commands.add_parser('mortar-response', help='Explicit recorded stationary mortar trigger response diagnostic')
    mortar_response.add_argument('--policy', type=Path, required=True)
    mortar_response.add_argument('--seconds', type=float, default=30.)
    mortar_response.add_argument('--enter', action='store_true')
    mortar_response.set_defaults(handler=bot)
    field_operator = commands.add_parser('motherpuffer', help='Perform direct bounded live VR field actions')
    field_operator.add_argument('--mode',choices=('vehicle','firearm'),required=True)
    field_operator.add_argument('--vehicle-id',type=int,default=25603)
    field_operator.add_argument('--seconds',type=float,default=60.)
    field_operator.set_defaults(handler=bot)
    launch = commands.add_parser("launch-sim")
    launch.add_argument('--slot', choices=('dev','play'), default='dev', help='Deploy the selected retained build for simulator comparison')
    launch.add_argument("--tracking-fault-test", action="store_true",
                        help="Explicitly enable bounded simulated controller occlusion diagnostics")
    launch.add_argument("--idroid-ui-boundary-trace", action="store_true",
                        help="Collect bounded source-owned native UI transition diagnostics")
    launch.add_argument("--camera-evidence-dir", type=Path, help="Existing local directory for owned source-frame diagnostics")
    launch.add_argument("--producer-pacing-test", choices=("baseline", "display"), default="baseline")
    launch.add_argument("--producer-pacing-test-seconds", type=int, default=900)
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
    model = commands.add_parser('model', help='Refresh the fixed map/state graph without game control')
    model.add_argument('--run', action='append', type=Path, default=[], help='Reviewed, pinned bot observations')
    model.set_defaults(handler=game_model)
    item_index = commands.add_parser('items', help='Read-only owned equipment, prop and animation catalog')
    item_index.add_argument('action', choices=('summary', 'search', 'show', 'view', 'refresh'))
    item_index.add_argument('query', nargs='?')
    item_index.add_argument('--category', choices=('all', 'equipment', 'models', 'animations'), default='all')
    item_index.add_argument('--limit', type=int, default=30)
    item_index.set_defaults(handler=items)
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
