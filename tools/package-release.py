"""Package the checked fixed play tree and its committed source for GitHub."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile

from workspace import ROOT, PLAY, digest, read_json, safe_tree, source_identity


def checked_name(name):
    path = PurePosixPath(name)
    if not name or "\\" in name or path.is_absolute() or ".." in path.parts:
        raise ValueError(f"Unsafe package path: {name}")
    if any(p.lower() in {"private", ".deps", ".git", "captures"} for p in path.parts):
        raise ValueError(f"Private data in package: {name}")
    if path.suffix.lower() in {".fpk", ".fpkd", ".fmdl", ".ftex", ".dds", ".sav", ".dat", ".dmp", ".pdb"}:
        raise ValueError(f"Retail or diagnostic data in package: {name}")
    if path.name.lower() in {"mgsvtpp.exe", "mgsgroundzeroes.exe", ".env"}:
        raise ValueError(f"Private or retail file in package: {name}")
    return path


def package(tag, output):
    if not re.fullmatch(r"experimental-\d{4}-\d{2}-\d{2}(?:\.\d+)?", tag):
        raise ValueError("Use an experimental date tag, e.g. experimental-2026-09-30")
    output = safe_tree(output, ROOT / "build")
    if output.resolve() == (ROOT / "build/Release").resolve():
        raise ValueError("Keep release archives separate from compiler output; use build/github-release")
    safe_tree(PLAY, ROOT)
    build = read_json(PLAY / "BUILD.json")
    source = source_identity()
    if source["changes"] or source != build["source"]:
        raise ValueError("Commit the public source, then run tools/build.ps1 before packaging")
    if build["validation"]["automated"] != "passed":
        raise ValueError("The play build has not passed its automated checks")
    notes = f"docs/RELEASE_{tag.removeprefix('experimental-').split('.')[0]}.md"
    required = {notes, "dinput8.dll", "MGS5VR-Launcher.exe", "MGS5VR-FieldKit.exe", "mgs5vr_import.exe",
                "mgs5vr_controls.exe", "mgs5vr_probe.exe", "Install.cmd", "Launch-Headset.cmd",
                "tools/setup.ps1", "tools/install.ps1", "LICENSE", "licenses/OpenXR.txt",
                "licenses/MinHook.txt", "licenses/WebView2-LICENSE.txt",
                "launcher-ui/vendor/three-r180/LICENSE", "launcher-ui/assets/LICENSE-webxr-input-profiles.md"}
    if not required.issubset(build["files"]):
        raise ValueError("Incomplete checked installation tree")
    for name, expected in build["files"].items():
        checked_name(name)
        if digest(PLAY / name) != expected:
            raise ValueError(f"Checked play file changed: {name}")
    for name in ("mgs5vr.ini", "mgs5vr-controls.ini"):
        if digest(PLAY / name) != digest(ROOT / "config" / name):
            raise ValueError("Release must contain public defaults, not personal INIs")
    actual = {p.relative_to(PLAY).as_posix() for p in PLAY.rglob("*") if p.is_file()}
    if actual != set(build["files"]) | {"BUILD.json"}:
        raise ValueError("Unmanaged files in play tree; review them before packaging")
    output.mkdir(parents=True, exist_ok=True)
    source_zip = output / "MGS5VR-source.zip"
    archive = output / f"MGS5VR-{tag}.zip"
    for path in (source_zip, archive):
        if path.exists():
            raise ValueError(f"Release output already exists: {path}")
    subprocess.run(["git", "archive", "--format=zip", "--prefix=MGS5VR-source/",
                    f"--output={source_zip}", source["commit"]], cwd=ROOT, check=True)
    with zipfile.ZipFile(source_zip) as source_files:
        for item in source_files.infolist():
            checked_name(item.filename)
            if (item.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Linked files are not supported in source archives")
        if source_files.testzip() is not None:
            raise ValueError("Invalid source ZIP")
    release = {"schema": 1, "tag": tag, "prerelease": True,
               "created_utc": datetime.now(timezone.utc).isoformat(),
               "source_commit": source["commit"], "source_sha256": source["source_sha256"],
               "dll_sha256": build["files"]["dinput8.dll"], "source_archive_sha256": digest(source_zip),
               "validation": build["validation"], "notes": notes}
    payload = {"BUILD.json": (PLAY / "BUILD.json").read_bytes(),
               "RELEASE.json": (json.dumps(release, indent=2) + "\n").encode(),
               "SOURCE_REVISION.txt": (source["commit"] + "\n").encode()}
    hashes = dict(build["files"])
    hashes.update({name: hashlib.sha256(data).hexdigest() for name, data in payload.items()})
    hashes["MGS5VR-source.zip"] = digest(source_zip)
    manifest = {"schema": 1, "source_commit": source["commit"], "files": hashes}
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for name in sorted(build["files"]):
            bundle.write(PLAY / name, "MGS5VR/" + name)
        bundle.write(source_zip, "MGS5VR/MGS5VR-source.zip")
        for name, data in payload.items():
            bundle.writestr("MGS5VR/" + name, data)
        bundle.writestr("MGS5VR/manifest.json", json.dumps(manifest, indent=2) + "\n")
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise ValueError("Invalid release ZIP")
        for name, expected in hashes.items():
            with bundle.open("MGS5VR/" + name) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != expected:
                    raise ValueError(f"Release file verification failed: {name}")
    checksum = output / f"SHA256SUMS-{tag.removeprefix('experimental-')}.txt"
    checksum.write_text(f"{digest(archive)}  {archive.name}\n", encoding="ascii")
    (output / "release.json").write_text(json.dumps({**release, "archive": archive.name,
        "archive_sha256": digest(archive), "archive_bytes": archive.stat().st_size,
        "verified_files": len(hashes)}, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"archive": str(archive), "checksum": str(checksum),
                      "source_commit": source["commit"], "verified_files": len(hashes)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/github-release")
    args = parser.parse_args()
    package(args.tag, args.output)
