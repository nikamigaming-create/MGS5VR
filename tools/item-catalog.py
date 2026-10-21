"""Searchable local equipment/prop/animation index from an owned archive scan.

Archive membership, declared references, observations and VR implementation are
kept distinct. Finding a rig or a nearby animation never marks an item usable.
The generated database contains private game metadata and is not release data.
"""
import argparse
import hashlib
import json
import pathlib
import re
import sqlite3
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "private/item-catalog/catalog.sqlite"
ANIMATIONS = {".gani", ".fgan", ".mtar", ".mtard", ".caar", ".ani", ".canm"}
SCHEMA = """
CREATE TABLE meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE archives(name TEXT PRIMARY KEY,bytes INTEGER,mtime_utc TEXT,entries INTEGER);
CREATE TABLE media(name TEXT PRIMARY KEY,format TEXT,bytes INTEGER,mtime_utc TEXT);
CREATE TABLE assets(id TEXT PRIMARY KEY,path TEXT,ext TEXT,name_source TEXT);
CREATE INDEX asset_paths ON assets(path COLLATE NOCASE);
CREATE INDEX asset_types ON assets(ext);
CREATE TABLE occurrences(asset TEXT,archive TEXT,package TEXT,offset INTEGER,bytes INTEGER,sha256 TEXT,model TEXT,model_error TEXT);
CREATE INDEX occurrences_asset ON occurrences(asset);
CREATE INDEX occurrences_package ON occurrences(package);
CREATE TABLE package_references(archive TEXT,package TEXT,path TEXT);
CREATE TABLE errors(archive TEXT,package TEXT,message TEXT);
CREATE TABLE equipment(id TEXT PRIMARY KEY,native_type TEXT,parameter TEXT,block TEXT,model TEXT,package TEXT,source TEXT,details TEXT);
CREATE TABLE declared_references(source TEXT,line INTEGER,owner TEXT,target TEXT,kind TEXT);
CREATE INDEX reference_targets ON declared_references(target);
CREATE TABLE observations(id TEXT PRIMARY KEY,target TEXT,label TEXT,status TEXT,record TEXT);
"""


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def putmeta(db, key, value):
    db.execute("INSERT OR REPLACE INTO meta VALUES(?,?)", (key, json.dumps(value)))


def add_equipment(db, directory):
    # The retained extraction captures declarative registration tables. Validate
    # their source bytes instead of executing game Lua while building this index.
    capture = ROOT / "private/arsenal-owned-table-extract.json"
    if capture.is_file():
        document = json.loads(capture.read_text(encoding="utf-8-sig"))
        for source in document.get("sources", []):
            path = ROOT / source["file"].replace("\\", "/")
            if not path.is_file() or digest(path) != source["sha256"]:
                raise ValueError(f"Retained equipment extraction source changed: {path}")
            for table in source["tables"]:
                if table["registration"] != "TppEquip.ReloadEquipIdTable":
                    continue
                for row in table["table"]:
                    if not isinstance(row, list) or len(row) != 6:
                        raise ValueError("Unsupported equipment registration row")
                    identity, kind, parameter, block, model, package = row
                    identity = identity.removeprefix("TppEquip.")
                    db.execute("INSERT INTO equipment VALUES(?,?,?,?,?,?,?,?)", (
                        identity, kind.removeprefix("TppEquip.EQP_TYPE_"), str(parameter),
                        block, model, package, str(path.relative_to(ROOT)),
                        json.dumps({"access": "not_checked", "animation_binding": "not_checked"})))
        putmeta(db, "equipment_capture", {"path": str(capture), "sha256": digest(capture)})
    audit = ROOT / "private/weapon-fit-audit.json"
    if audit.is_file():
        entries = json.loads(audit.read_text(encoding="utf-8-sig"))["entries"]
        for entry in entries:
            # Preserve individual native IDs and their existing check results.
            row = db.execute("SELECT details FROM equipment WHERE id=?", (entry["id"],)).fetchone()
            detail = json.loads(row[0]) if row else {"access": "not_checked", "animation_binding": "not_checked"}
            detail["weapon_fit_record"] = entry
            if row:
                db.execute("UPDATE equipment SET details=? WHERE id=?", (json.dumps(detail), entry["id"]))
            else:
                db.execute("INSERT INTO equipment VALUES(?,?,?,?,?,?,?,?)", (
                    entry["id"], entry.get("native_type"), None, None,
                    entry.get("model"), None, str(audit.relative_to(ROOT)), json.dumps(detail)))
        putmeta(db, "weapon_fit_source", {"path": str(audit), "sha256": digest(audit)})
    if not directory.is_dir():
        return
    sources = []
    for path in sorted(directory.rglob("*.lua")):
        sources.append({"path": str(path), "sha256": digest(path)})
        text = path.read_text(encoding="utf-8-sig")
        motion = "motion" in path.name.lower()
        for line, raw in enumerate(text.splitlines(), 1):
            if raw.lstrip().startswith("--"):
                continue
            # These are lexical declared references, not executable bindings.
            owner = None
            for record in re.finditer(r"\{([^{}]*)\}", raw):
                body = record[1]
                identifiers = re.findall(r"TppEquip\.([A-Za-z_]\w*)", body)
                if not identifiers:
                    continue
                owner = identifiers[0]
                for target in dict.fromkeys(identifiers[1:]):
                    db.execute("INSERT INTO declared_references VALUES(?,?,?,?,?)", (
                        str(path.relative_to(ROOT)), line, owner, target,
                        "motion_table_symbol" if motion else "equipment_table_symbol"))
            for target in dict.fromkeys(re.findall(r'["\'](/Assets/[^"\'\r\n]+)["\']', raw)):
                db.execute("INSERT INTO declared_references VALUES(?,?,?,?,?)", (
                    str(path.relative_to(ROOT)), line, owner, target, "literal_asset_path"))
    putmeta(db, "owned_parameter_sources", sources)


def build(scan, output, parameters, observations):
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".building.sqlite")
    if temporary.exists():
        temporary.unlink()  # Only this tool's workspace metadata staging file.
    db = sqlite3.connect(temporary)
    try:
        db.executescript(SCHEMA)
        db.execute("PRAGMA journal_mode=OFF")
        start = finish = None
        for line in scan.open(encoding="utf-8-sig"):
            row = json.loads(line)
            kind = row["kind"]
            if kind == "scan":
                if start is not None or row.get("schema") != 1:
                    raise ValueError("Unsupported or concatenated asset scan")
                start = row
            elif kind == "complete":
                if finish is not None:
                    raise ValueError("Repeated asset scan completion")
                finish = row
            elif kind == "archive":
                db.execute("INSERT INTO archives VALUES(?,?,?,?)", (
                    row["name"], row["bytes"], row["mtime_utc"], row["entries"]))
            elif kind == "media":
                db.execute("INSERT INTO media VALUES(?,?,?,?)", (
                    row["name"], row["format"], row["bytes"], row["mtime_utc"]))
            elif kind in {"qar", "asset"}:
                identity = ("Q:" if kind == "qar" else "F:") + row["id"]
                db.execute("INSERT OR IGNORE INTO assets VALUES(?,?,?,?)", (
                    identity, row["path"], row["ext"], row.get("name_source", "dictionary")))
                db.execute("INSERT INTO occurrences VALUES(?,?,?,?,?,?,?,?)", (
                    identity, row["archive"], row.get("package"), row["offset"], row["bytes"],
                    row.get("sha256"), json.dumps(row["model"]) if "model" in row else None,
                    row.get("model_error")))
            elif kind == "package_reference":
                db.execute("INSERT INTO package_references VALUES(?,?,?)", (
                    row["archive"], row["package"], row["path"]))
            elif kind == "error":
                db.execute("INSERT INTO errors VALUES(?,?,?)", (row["archive"], row.get("package"), row["message"]))
            else:
                raise ValueError(f"Unrecognized scan record {kind}")
        if not start or not finish:
            raise ValueError("Scan was interrupted; do not replace a completed catalog")
        actual = db.execute("SELECT count(*) FROM occurrences WHERE asset LIKE 'F:%'").fetchone()[0]
        if actual != finish["asset_occurrences"]:
            raise ValueError("Asset occurrence total differs from scan receipt")
        if (db.execute('SELECT count(*) FROM archives').fetchone()[0] != finish['archives']
                or db.execute('SELECT count(*) FROM media').fetchone()[0] != finish.get('media_files', 0)):
            raise ValueError('Archive/media coverage differs from scan receipt')
        putmeta(db, "scan", start)
        putmeta(db, "completion", finish)
        putmeta(db, "scan_sha256", digest(scan))
        putmeta(db, "built_utc", datetime.now(timezone.utc).isoformat())
        add_equipment(db, parameters)
        if observations.is_file():
            records = json.loads(observations.read_text(encoding="utf-8-sig"))
            if records.get("schema") != 1:
                raise ValueError("Unsupported observation ledger")
            for row in records["observations"]:
                if not row.get("evidence"):
                    raise ValueError("An item observation requires evidence")
                db.execute("INSERT INTO observations VALUES(?,?,?,?,?)", (
                    row["id"], row["target"], row["label"], row["status"], json.dumps(row)))
            putmeta(db, "observation_source", {"path": str(observations), "sha256": digest(observations)})
        db.commit()
    except BaseException:
        db.close()
        temporary.unlink(missing_ok=True)
        raise
    db.close()
    temporary.replace(output)
    with sqlite3.connect(output) as final:
        summary(final)


def summary(db):
    values = dict(db.execute("SELECT key,value FROM meta"))
    finish = json.loads(values["completion"])
    counts = {"equipment_definitions": db.execute("SELECT count(*) FROM equipment").fetchone()[0],
              "unique_asset_identities": db.execute("SELECT count(*) FROM assets").fetchone()[0],
              "known_model_paths": db.execute("SELECT count(*) FROM assets WHERE ext='.fmdl'").fetchone()[0],
              "native_item_definitions": db.execute("SELECT count(*) FROM equipment WHERE native_type='Item'").fetchone()[0],
              "unknown_asset_names": db.execute("SELECT count(*) FROM assets WHERE path IS NULL").fetchone()[0],
              "declared_references": db.execute("SELECT count(*) FROM declared_references").fetchone()[0],
              "recorded_observations": db.execute("SELECT count(*) FROM observations").fetchone()[0]}
    counts["animation_asset_identities"] = sum(db.execute("SELECT count(*) FROM assets WHERE ext=?", (ext,)).fetchone()[0] for ext in ANIMATIONS)
    counts["item_prop_model_paths"] = db.execute("SELECT count(*) FROM assets WHERE ext='.fmdl' AND path LIKE '%/item/%'").fetchone()[0]
    counts['equipment_types'] = dict(db.execute('SELECT native_type,count(*) FROM equipment GROUP BY native_type ORDER BY native_type'))
    print(json.dumps({**finish, **counts, "coverage": "installed archive metadata; native use and VR readiness require per-item evidence"}, indent=2))


def search(db, text, limit, category):
    pattern = "%" + text.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
    if category in {"all", "equipment"}:
        rows = db.execute("SELECT id,native_type,model,package FROM equipment WHERE id LIKE ? ESCAPE '\\' OR native_type LIKE ? ESCAPE '\\' OR model LIKE ? ESCAPE '\\' LIMIT ?", (pattern, pattern, pattern, limit))
        for identity, native_type, model, package in rows:
            print(json.dumps({"kind": "equipment", "id": identity, "type": native_type, "model": model, "package": package}))
    if category != "equipment":
        query = "SELECT id,path,ext FROM assets WHERE (path LIKE ? ESCAPE '\\' OR id LIKE ? ESCAPE '\\')"
        arguments = [pattern, pattern]
        if category == "models":
            query += " AND ext='.fmdl'"
        elif category == "animations":
            query += " AND ext IN (" + ",".join("?" for _ in ANIMATIONS) + ")"
            arguments.extend(sorted(ANIMATIONS))
        query += " ORDER BY path LIMIT ?"
        for identity, path, ext in db.execute(query, [*arguments, limit]):
            print(json.dumps({"kind": "asset", "id": identity, "path": path, "ext": ext}))
    for label, status, record in db.execute("SELECT label,status,record FROM observations WHERE label LIKE ? ESCAPE '\\' OR target LIKE ? ESCAPE '\\'", (pattern, pattern)):
        print(json.dumps({"kind": "observation", "label": label, "status": status, "record": json.loads(record)}))


def show(db, identity):
    db.row_factory = sqlite3.Row
    equipment = db.execute("SELECT * FROM equipment WHERE id=?", (identity,)).fetchone()
    if equipment:
        record = dict(equipment)
        record["details"] = json.loads(record["details"])
        record["declared_references"] = [dict(row) for row in db.execute("SELECT * FROM declared_references WHERE owner=? OR target=?", (identity, identity))]
        print(json.dumps(record, indent=2))
        return
    asset = db.execute("SELECT * FROM assets WHERE id=? OR path=?", (identity, identity)).fetchone()
    if not asset:
        raise ValueError("No matching asset or native equipment ID")
    record = dict(asset)
    record["occurrences"] = []
    packages = set()
    for row in db.execute("SELECT * FROM occurrences WHERE asset=?", (asset["id"],)):
        row = dict(row)
        if row["model"]:
            row["model"] = json.loads(row["model"])
        record["occurrences"].append(row)
        if row["package"]:
            packages.add(row["package"])
    record["co_packaged_animation_assets"] = []
    if packages:
        placeholders = ",".join("?" for _ in packages)
        rows = db.execute("SELECT DISTINCT a.id,a.path,a.ext FROM occurrences o JOIN assets a ON a.id=o.asset WHERE o.package IN (" + placeholders + ") AND a.ext IN (" + ",".join("?" for _ in ANIMATIONS) + ")", [*sorted(packages), *sorted(ANIMATIONS)])
        record["co_packaged_animation_assets"] = [dict(row) for row in rows]
    record["observations"] = [json.loads(row[0]) for row in db.execute("SELECT record FROM observations WHERE target=?", (asset["path"],))]
    record["animation_binding"] = "not_checked; archive proximity and bones do not establish a playable animation"
    print(json.dumps(record, indent=2))


def view(db, output):
    observations = collections_by_target(db)
    completion = json.loads(db.execute("SELECT value FROM meta WHERE key='completion'").fetchone()[0])
    unresolved = db.execute('SELECT count(*) FROM assets WHERE path IS NULL').fetchone()[0]
    records = []
    previews_path = output.parent / 'preview-index.json'
    previews = json.loads(previews_path.read_text()) if previews_path.is_file() else {}
    for identity, path, metadata, occurrences, content_hash in db.execute("SELECT a.id,a.path,o.model,count(*),o.sha256 FROM assets a JOIN occurrences o ON a.id=o.asset WHERE a.ext='.fmdl' GROUP BY a.id ORDER BY a.path"):
        model = json.loads(metadata) if metadata else {}
        notes = observations.get(path, [])
        record = {'id': identity, 'name': notes[0]['label'] if notes else pathlib.PurePosixPath(path).stem,
                  'kind': 'Prop' if '/item/' in path else 'Model', 'path': path, 'occurrences': occurrences,
                  'variant_sha256': content_hash,
                  'model': model, 'observations': notes, 'status': notes[0]['status'] if notes else 'Indexed; behavior not checked'}
        if identity in previews and previews[identity].get('sha256') == content_hash:
            preview = previews[identity]
            record['preview'] = {**preview, 'image': (output.parent / ('prop-preview-' + str(preview['page']) + '.png')).as_uri()}
        records.append(record)
    for identity, kind, model, package, detail in db.execute('SELECT id,native_type,model,package,details FROM equipment ORDER BY id'):
        notes = observations.get(identity, [])
        records.append({'id': identity, 'name': notes[0]['label'] if notes else identity, 'kind': 'Equipment',
                        'path': model or '', 'package': package, 'type': kind, 'details': json.loads(detail),
                        'observations': notes, 'status': notes[0]['status'] if notes else 'Defined; player access not checked'})
    for identity, path, ext in db.execute('SELECT id,path,ext FROM assets WHERE ext IN (' + ','.join('?' for _ in ANIMATIONS) + ') ORDER BY path', sorted(ANIMATIONS)):
        records.append({'id': identity, 'name': pathlib.PurePosixPath(path).name, 'kind': 'Animation resource',
                        'path': path, 'status': 'Indexed; individual clips and prop bindings not checked'})
    data = json.dumps(records, separators=(',', ':')).replace('<', '\\u003c')
    coverage = (f"{completion['archives']:,} asset archives scanned · "
                f"{completion.get('media_files', 0)} media files identified · "
                f"{unresolved:,} asset names unresolved · {completion['errors']} scan errors. "
                "Animation resources are indexed; individual clips and bindings still need review.")
    html = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MGS5VR owned item catalog</title>
<style>body{margin:0;background:#14181e;color:#e8e9ec;font:16px system-ui}main{max-width:1300px;margin:auto;padding:28px}h1{font-size:28px;margin:0 0 8px}p{color:#b7bfc9;line-height:1.5}input,select{font:inherit;padding:12px;border:1px solid #536175;border-radius:6px;background:#242c38;color:white}input{width:min(60%,600px)}.filters{display:flex;gap:12px;flex-wrap:wrap}#list{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:14px;margin-top:20px}article{border:1px solid #394353;border-radius:9px;background:#1d2530;padding:16px;overflow:hidden}h2{font-size:17px;overflow-wrap:anywhere}small{color:#b4c1ce}.thumb{width:100%;aspect-ratio:1;background-repeat:no-repeat}code{overflow-wrap:anywhere;font-size:12px}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:11px;color:#c7d1dd}details{margin-top:15px}button{font:inherit;padding:8px 14px;background:#ab342b;color:white;border:0;border-radius:5px;margin-top:18px}</style>
<main><h1>MGS5VR owned item catalog</h1><p>Equipment, native props, models and animation resources. Geometry previews show owned bind geometry; materials and animation are not reproduced. Evidence distinguishes native use from VR behavior.</p>
<p>COVERAGE</p><div class="filters"><input id="query" type="search" placeholder="Search cassette, binoculars, an equipment ID or an asset path" aria-label="Search catalog"><select id="category" aria-label="Category"><option>Prop</option><option>Equipment</option><option>Model</option><option>Animation resource</option><option>All</option></select><select id="review" aria-label="Recorded evidence"><option value="all">Any evidence status</option><option value="recorded">Recorded observations</option><option value="unreviewed">No recorded observations</option></select></div><p id="count" aria-live="polite"></p><button id="export">Export results</button><div id="list"></div><button id="more">Show more</button></main>
<script>const records=DATA;const query=document.querySelector('#query'),category=document.querySelector('#category'),review=document.querySelector('#review'),list=document.querySelector('#list');let limit=60;
function element(tag,value){const e=document.createElement(tag);if(value!==undefined)e.textContent=value;return e}
function selected(){const tokens=query.value.toLowerCase().split(/\\s+/).filter(Boolean);return records.filter(r=>(category.value==='All'||r.kind===category.value)&&(review.value==='all'||(review.value==='recorded')===Boolean(r.observations&&r.observations.length))&&tokens.every(t=>JSON.stringify(r).toLowerCase().includes(t)))}
function render(){list.replaceChildren();const filtered=selected();document.querySelector('#count').textContent=filtered.length+' matching entries · '+Math.min(limit,filtered.length)+' shown';for(const r of filtered.slice(0,limit)){const a=element('article');if(r.preview){const p=r.preview,thumb=element('div');thumb.className='thumb';thumb.style.backgroundImage='url("'+p.image+'")';thumb.style.backgroundSize=(p.columns*100)+'% '+(p.rows*100)+'%';thumb.style.backgroundPosition=((p.cell%p.columns)/(p.columns-1)*100)+'% '+(Math.floor(p.cell/p.columns)/(p.rows-1)*100)+'%';a.append(thumb)}a.append(element('small',r.kind),element('h2',r.name),element('p',r.status),element('code',r.path||r.id));if(r.model){const b=r.model.bounds_native;let text=(r.model.bones||0)+' model bones · '+(r.model.triangles||0)+' triangles';if(b)text+=' · '+b.min.map((v,i)=>Math.round((b.max[i]-v)*1000)).join(' × ')+' mm';a.append(element('p',text))}const d=element('details');d.append(element('summary','Source details and evidence'),element('pre',JSON.stringify(r,null,2)));a.append(d);list.append(a)}document.querySelector('#more').hidden=limit>=filtered.length}
for(const e of [query,category,review])e.addEventListener('input',()=>{limit=60;render()});document.querySelector('#more').addEventListener('click',()=>{limit+=60;render()});document.querySelector('#export').addEventListener('click',()=>{const url=URL.createObjectURL(new Blob([JSON.stringify(selected(),null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='mgs5vr-item-selection.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)});render();</script></html>'''.replace('COVERAGE', coverage).replace('DATA', data)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(html, encoding='utf-8')
    print(f'{len(records)} searchable entries -> {output.resolve()}')


def collections_by_target(db):
    result = {}
    for target, raw in db.execute('SELECT target,record FROM observations ORDER BY id'):
        result.setdefault(target, []).append(json.loads(raw))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=pathlib.Path, default=DEFAULT)
    commands = parser.add_subparsers(dest="command", required=True)
    make = commands.add_parser("build")
    make.add_argument("--scan", type=pathlib.Path, default=DEFAULT.parent / "scan.jsonl")
    make.add_argument("--parameters", type=pathlib.Path, default=ROOT / "private/arsenal-owned-parameters")
    make.add_argument("--observations", type=pathlib.Path, default=ROOT / "tools/item-observations.json")
    commands.add_parser("summary")
    find = commands.add_parser("search")
    find.add_argument("text")
    find.add_argument("--limit", type=int, default=30)
    find.add_argument("--category", choices=["all", "equipment", "models", "animations"], default="all")
    detail = commands.add_parser("show")
    detail.add_argument("identity")
    browser = commands.add_parser('view')
    browser.add_argument('--output', type=pathlib.Path, default=DEFAULT.parent / 'index.html')
    args = parser.parse_args()
    if args.command == "build":
        build(args.scan, args.db, args.parameters, args.observations)
        return
    # Read-only queries cannot create a misleading empty database.
    with sqlite3.connect(args.db.resolve().as_uri() + "?mode=ro", uri=True) as db:
        if args.command == "summary":
            summary(db)
        elif args.command == "search":
            if not 1 <= args.limit <= 1000:
                parser.error("--limit must be 1..1000")
            search(db, args.text, args.limit, args.category)
        elif args.command == 'show':
            show(db, args.identity)
        else:
            view(db, args.output.resolve())


if __name__ == "__main__":
    main()
