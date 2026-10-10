"""Remove recognized obsolete MGS5VR builds, preserving settings and evidence."""
from pathlib import Path


def candidates(workspace, value):
    root = workspace.ROOT
    selected = []

    def add(path, parent=root):
        if path.exists():
            workspace.safe_tree(path, parent)
            selected.append((path, parent))

    for name in ("build/play-previous", "build/installed-previous"):
        add(root / name)
    releases = root / "build/github-release"
    if releases.is_dir():
        for path in releases.rglob("*.zip"):
            add(path)
    # Old standalone packages have both the mod entry point and their README.
    packages = [root / "dist"]
    for parent in (root / "dist", root / "output"):
        if parent.is_dir():
            packages.extend(parent.glob("MGS5VR-*"))
    temporary = root / "tmp"
    if temporary.is_dir():
        for run in temporary.glob("actions-run-*"):
            packages.extend(run.glob("MGS5VR-*"))
    for path in packages:
        if path.is_dir() and (path / "dinput8.dll").is_file() and (path / "README.md").is_file():
            add(path)
        elif path.is_file() and path.name.startswith("MGS5VR-") and path.suffix == ".zip":
            add(path)
    # Historical run receipts and screenshots remain; only saved mod binaries go.
    for baseline in (root / "private").glob("bot-baseline-*"):
        if (baseline / "baseline.json").is_file():
            for name in workspace.BINARIES:
                add(baseline / name)
    retired = root / "artifacts/dev/retired-builds"
    if retired.is_dir():
        for path in retired.glob("*.dll"):
            add(path)
    # Retain personal INIs and receipts in game-local launcher backups.
    if value.get("game_dir"):
        game = Path(value["game_dir"]).resolve()
        record_path = game / "mgs5vr-install.json"
        if record_path.is_file():
            record = workspace.read_json(record_path)
            if (record.get("schema") == 1 and record.get("product") == "MGS5VR theatre preview"
                    and Path(record.get("game_dir", "")).resolve() == game):
                backups = game / "mgs5vr-launcher-backups"
                if backups.is_dir():
                    workspace.safe_tree(backups, game)
                    for backup in backups.iterdir():
                        if backup.is_dir() and (backup / "mgs5vr-install.json").is_file():
                            for name in workspace.BINARIES:
                                add(backup / name, backups)
    # Never return both a package directory and something inside it.
    unique = {}
    for path, parent in sorted(selected, key=lambda row: len(row[0].parts)):
        if not any(path == other or path.is_relative_to(other) for other in unique):
            unique[path] = parent
    return list(unique.items())


def clean(workspace, args):
    value = workspace.settings()
    if not value.get("pinned_build"):
        raise ValueError("Pin and verify play/ before removing older builds")
    workspace.checked_build(workspace.PLAY, value["pinned_build"])
    workspace.checked_build(workspace.DEV)
    targets = candidates(workspace, value)
    rows = []
    for path, parent in targets:
        files = [p for p in path.rglob("*") if p.is_file()] if path.is_dir() else [path]
        rows.append({"path": str(path), "parent": str(parent),
                     "bytes": sum(p.stat().st_size for p in files),
                     "files": [{"path": str(p.relative_to(path)) if path.is_dir() else p.name,
                                "sha256": workspace.digest(p)} for p in files]})
    total = sum(row["bytes"] for row in rows)
    print(f"Obsolete build targets: {len(rows)}; {total / 1024**3:.3f} GiB")
    if not args.apply:
        for row in rows:
            print(f"{row['bytes']:>12}  {row['path']}")
        return
    receipt = {"schema": 1, "started_utc": workspace.utc(), "bytes": total,
               "pinned_dll": value["pinned_build"]["dll_sha256"], "targets": rows, "removed": []}
    report = workspace.ROOT / "artifacts/dev/build-cleanup.json"
    workspace.write_json(report, receipt)
    for path, parent in targets:
        # Revalidate the resolved absolute path immediately before removal.
        workspace.safe_tree(path, parent)
        if path.is_dir():
            workspace.remove_tree(path, parent)
        else:
            path.unlink()
        receipt["removed"].append(str(path))
        workspace.write_json(report, receipt)
    workspace.checked_build(workspace.PLAY, value["pinned_build"])
    workspace.checked_build(workspace.DEV)
    receipt["completed_utc"] = workspace.utc()
    workspace.write_json(report, receipt)
    print(f"Removed {total / 1024**3:.3f} GiB of obsolete builds. Settings and test evidence retained.\nReceipt: {report}")
