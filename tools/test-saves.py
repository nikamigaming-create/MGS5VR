"""Local MGSV test profiles: immutable snapshots, verified switches and rollback.

The paired 0/1 files are retained together; they are not treated as independent
campaign slots. Only the six TPP campaign/personal files below are ever changed.
Graphics, controls, GZ saves, cloud metadata and game assets are untouched.
"""
import argparse
import csv
import datetime
import hashlib
import io
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
STORE = ROOT / "artifacts" / "test-profiles"
FILES = tuple(
    [f"287700/local/{name}{slot}" for name in ("TPP_GAME_DATA", "PERSONAL_DATA") for slot in (0, 1)]
    + [f"311340/remote/{name}" for name in ("TPP_GAME_DATA", "PERSONAL_DATA")]
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def profile_path(store, name):
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,95}", name):
        raise ValueError("Profile names use lowercase letters, digits and hyphens")
    result = (store.resolve() / name).resolve()
    if result.parent != store.resolve():
        raise ValueError("Profile escapes the store")
    return result


def source_file(source, relative):
    parts = pathlib.PurePosixPath(relative).parts
    choices = (source / relative, source / (parts[0] + "-" + parts[1]) / parts[2])
    matches = [p for p in choices if p.is_file()]
    if not matches:
        raise ValueError(f"Incomplete profile: missing {relative}")
    if len(matches) > 1 and digest(matches[0]) != digest(matches[1]):
        raise ValueError(f"Ambiguous source copies: {relative}")
    return matches[0]


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def snapshot(source, store, name, note="", state=None):
    target = profile_path(store, name)
    if target.exists():
        raise ValueError(f"Snapshot already exists: {name}")
    sources = [(relative, source_file(source, relative)) for relative in FILES]
    before = {relative: digest(path) for relative, path in sources}
    target.mkdir(parents=True)
    records = []
    for relative, path in sources:
        copied = target / relative
        copied.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, copied)
        if digest(copied) != before[relative] or digest(path) != before[relative]:
            raise RuntimeError(f"Save changed during snapshot: {relative}; snapshot has no valid manifest")
        records.append({"path": relative, "bytes": copied.stat().st_size,
                        "sha256": before[relative], "source": str(path.resolve())})
    manifest = {"schema": 1, "name": name, "created_utc": utc(), "note": note,
                "native_state": state, "files": records}
    write_json(target / "manifest.json", manifest)
    return manifest


def verify(store, name):
    directory = profile_path(store, name)
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    paths = [item["path"] for item in manifest["files"]]
    if len(paths) != len(FILES) or set(paths) != set(FILES):
        raise ValueError("Manifest must contain exactly the six allowed save files")
    for item in manifest["files"]:
        path = directory / item["path"]
        if path.resolve().is_relative_to(directory) is False:
            raise ValueError("Save path escapes profile")
        if not path.is_file() or path.stat().st_size != item["bytes"] or digest(path) != item["sha256"]:
            raise ValueError(f"Save integrity check failed: {item['path']}")
    return manifest


def require_stopped():
    if os.name != "nt":
        raise RuntimeError("Live save switching is supported only on Windows")
    result = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True,
                            text=True, check=True, creationflags=subprocess.CREATE_NO_WINDOW)
    running = {row[0].lower() for row in csv.reader(io.StringIO(result.stdout)) if row}
    blockers = running & {"mgsvtpp.exe", "mgsgroundzeroes.exe", "steam.exe"}
    if blockers:
        raise RuntimeError("Close normally before snapshot/switch: " + ", ".join(sorted(blockers)))


def require_cloud_disabled(account):
    path = account / "7/remote/sharedconfig.vdf"
    tokens = iter(re.findall(r'"(?:\\.|[^"\\])*"|[{}]', path.read_text(encoding="utf-8")))
    def parse():
        result = {}
        for token in tokens:
            if token == "}":
                return result
            key = token[1:-1]
            value = next(tokens)
            if key in result:
                raise ValueError("Ambiguous Steam Cloud configuration")
            result[key] = parse() if value == "{" else value[1:-1]
        return result
    try:
        apps = parse()["UserRoamingConfigStore"]["Software"]["Valve"]["Steam"]["apps"]
        if any(apps.get(app, {}).get("cloudenabled") != "0" for app in ("287700", "311340")):
            raise ValueError("Disable Steam Cloud for TPP and its 311340 mirror before switching test saves")
    except (KeyError, StopIteration) as error:
        raise ValueError("Cannot establish that Steam Cloud is disabled for test saves") from error


def activate(account, store, name):
    manifest = verify(store, name)
    # Verify all destinations and preserve their exact state before staging.
    rollback_name = "before-" + datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:6]
    snapshot(account, store, rollback_name, f"Automatic rollback before activating {name}")
    rollback = profile_path(store, rollback_name)
    source = profile_path(store, name)
    staged, applied = [], []
    transaction = uuid.uuid4().hex
    try:
        for relative in FILES:
            destination = account / relative
            if not destination.resolve().is_relative_to(account.resolve()):
                raise ValueError("Destination escapes account")
            pending = destination.with_name(destination.name + ".mgs5vr-" + transaction)
            shutil.copyfile(source / relative, pending)
            staged.append(pending)
        for relative, pending in zip(FILES, staged):
            os.replace(pending, account / relative)
            applied.append(relative)
        for item in manifest["files"]:
            if digest(account / item["path"]) != item["sha256"]:
                raise RuntimeError(f"Installed save mismatch: {item['path']}")
    except BaseException:
        for relative in applied:
            shutil.copyfile(rollback / relative, account / relative)
        raise
    finally:
        for pending in staged:
            if pending.exists():
                pending.unlink()
    result = {"profile": name, "account": str(account.resolve()), "activated_utc": utc(),
              "rollback": rollback_name, "files_verified": len(FILES),
              "native_state_verified": False}
    write_json(store / "active.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--store", type=pathlib.Path, default=STORE)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list")
    check = commands.add_parser("verify")
    check.add_argument("name")
    capture = commands.add_parser("snapshot")
    capture.add_argument("name")
    capture.add_argument("--source", required=True, type=pathlib.Path)
    capture.add_argument("--archive", action="store_true", help="Import a closed, offline archive")
    capture.add_argument("--note", default="")
    capture.add_argument("--state", type=pathlib.Path, help="Fresh native state recorded before closing MGSV")
    switch = commands.add_parser("activate")
    switch.add_argument("name")
    switch.add_argument("--account", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if args.command == "list":
        results = []
        for path in sorted(args.store.glob("*/manifest.json")):
            try:
                value = verify(args.store, path.parent.name)
                results.append({k: value[k] for k in ("name", "created_utc", "note", "native_state")})
            except (OSError, ValueError, KeyError) as error:
                results.append({"name": path.parent.name, "error": str(error)})
        print(json.dumps(results, indent=2))
    elif args.command == "verify":
        print(json.dumps(verify(args.store, args.name), indent=2))
    elif args.command == "snapshot":
        if not args.archive:
            require_stopped()
        elif "userdata" in [part.lower() for part in args.source.resolve().parts]:
            raise ValueError("--archive cannot bypass live Steam account protection")
        state = json.loads(args.state.read_text(encoding="utf-8-sig")) if args.state else None
        print(json.dumps(snapshot(args.source, args.store, args.name, args.note, state), indent=2))
    elif args.command == "activate":
        require_stopped()
        if not args.account.is_dir() or args.account.parent.name.lower() != "userdata" or not args.account.name.isdigit():
            raise ValueError("--account must be an existing Steam userdata/<account-id> directory")
        require_cloud_disabled(args.account)
        print(json.dumps(activate(args.account, args.store, args.name), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f"test-saves: {error}", file=sys.stderr)
        raise SystemExit(1)
