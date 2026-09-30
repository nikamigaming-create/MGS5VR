"""Build the field-kit verification ledger from declared coverage and real runs."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import html
import json
from pathlib import Path
import subprocess

from gameplay_bot.campaign import file_hash, inventory, summarize_runs, validate_campaign, read_json
from gameplay_bot.core import atomic_json

ROOT = Path(__file__).resolve().parents[1]


def render_html(report):
    counts = Counter(row["kind"] for row in report["rows"])
    data = json.dumps(report, ensure_ascii=False).replace("</", "<\\/")
    counts_html = "".join(f'<span><b>{count}</b>{html.escape(kind.replace("_", " "))}</span>' for kind, count in counts.items())
    return '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MGS5VR · Field verification</title><style>
:root{color-scheme:light;background:#eee9df;color:#282827;font:16px/1.5 Arial,sans-serif}*{box-sizing:border-box}
body{margin:0}header{border-bottom:4px solid #b6382f;padding:20px 5vw;font-weight:bold;letter-spacing:.13em}
main{max-width:1440px;margin:auto;padding:40px 5vw}h1{font:800 clamp(36px,6vw,80px)/1.0 'Arial Narrow',Arial,sans-serif;letter-spacing:-.045em;margin:15px 0}
.eyebrow{color:#a3312b;letter-spacing:.18em;font-size:12px;font-weight:bold}p{max-width:920px}.counts{display:flex;flex-wrap:wrap;gap:28px;border-block:1px solid #aaa397;margin:28px 0;padding:18px 0}.counts span{font-size:12px;text-transform:uppercase}.counts b{display:block;font-size:34px}
.filters{display:flex;gap:12px;flex-wrap:wrap;position:sticky;top:0;background:#eee9df;padding:14px 0}input,select{font:inherit;padding:10px;border:1px solid #8c897f;background:#fbf8f0}input{flex:1;min-width:240px}
details{border-top:1px solid #b9b2a6;padding:16px 0}summary{cursor:pointer;display:grid;grid-template-columns:200px 1fr 190px;gap:16px;align-items:start}.id{font:12px monospace;overflow-wrap:anywhere}.state{font-size:12px;text-align:right;text-transform:uppercase;color:#a3312b}.body{padding:10px 0 8px 216px}.body p{margin:8px 0}.body li{margin:8px 0}a{color:#9b2a25}code{font-size:12px;overflow-wrap:anywhere}.note{border-left:4px solid #b6382f;padding:8px 18px;background:#f7f3ea}footer{margin:35px 0;color:#67645e;font-size:13px}
@media(max-width:760px){summary{grid-template-columns:1fr}.state{text-align:left}.body{padding-left:0}.filters{position:static}}
</style><header>MGS5VR / FIELD KIT / VERIFICATION</header><main><div class="eyebrow">CURRENT CANDIDATE · EVIDENCE INDEX</div>
<h1>Show the work.<br>Prove the result.</h1><p>Every report, known situation, mapped action and recovered feature stays visible. A native state change is evidence for that action; it does not by itself close the report. Open a row for acceptance requirements and run records.</p>
<div class="note">Coverage is incomplete. No public release or finished all-feature film is certified by this ledger. Historical recordings retain their own build identity.</div>
<div class="counts">''' + counts_html + '''</div><div class="filters"><input id="search" type="search" aria-label="Search coverage" placeholder="Mission 6, iDroid, R03, scope, controller…"><select id="kind" aria-label="Coverage type"><option value="">All coverage</option></select><select id="status" aria-label="Evidence status"><option value="">All evidence states</option><option>unproven</option><option>partial_native_evidence</option><option>failure_observed</option></select></div><p id="count"></p><section id="rows"></section><footer id="identity"></footer></main>
<script>const report=''' + data + ''';
const escape=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const kind=document.querySelector('#kind'),status=document.querySelector('#status'),search=document.querySelector('#search');
[...new Set(report.rows.map(r=>r.kind))].forEach(k=>{const o=document.createElement('option');o.value=k;o.textContent=k.replaceAll('_',' ');kind.append(o)});
function paint(){const q=search.value.toLowerCase();const rows=report.rows.filter(r=>(!kind.value||r.kind===kind.value)&&(!status.value||r.status===status.value)&&JSON.stringify(r).toLowerCase().includes(q));
document.querySelector('#count').textContent=rows.length+' of '+report.rows.length+' obligations shown';
document.querySelector('#rows').innerHTML=rows.map(r=>'<details><summary><span class="id">'+escape(r.id)+'</span><strong>'+escape(r.title)+'</strong><span class="state">'+escape(r.status.replaceAll('_',' '))+'</span></summary><div class="body">'+
(r.latest_report?'<p class="note">'+escape(r.latest_report)+'</p>':'')+'<ul>'+(r.acceptance||[]).map(a=>'<li>'+escape(a)+'</li>').join('')+'</ul><p>Source: <code>'+escape(r.source)+'</code></p>'+
(r.effective_binding?'<p>Effective binding: <code>'+escape(JSON.stringify(r.effective_binding))+'</code></p>':'')+
(r.evidence.length?r.evidence.map(e=>'<p><b>'+escape(e.current?'Current candidate':'Historical build')+'</b> · '+escape(e.native_status)+' · '+escape(e.suite)+'<br><code>'+escape(e.run)+'</code>'+(e.reason?'<br>'+escape(e.reason):'')+'</p>').join(''):'<p>No matching campaign evidence yet.</p>')+'</div></details>').join('');}
[search,kind,status].forEach(e=>e.addEventListener('input',paint));paint();document.querySelector('#identity').textContent='Candidate DLL '+report.target_identity.dll_sha256+' · generated '+report.generated_utc;
</script>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--candidate-dll", type=Path, default=ROOT / "build/Release/dinput8.dll")
    parser.add_argument("--controls-tool", type=Path, default=ROOT / "build/Release/mgs5vr_controls.exe")
    parser.add_argument("--run", action="append", type=Path, default=[])
    parser.add_argument("--equipment-inventory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    campaign = read_json(ROOT / "tools/gameplay_bot/campaigns/community.json")
    validate_campaign(campaign, ROOT)
    game = args.game_dir.resolve()
    bindings = json.loads(subprocess.check_output([str(args.controls_tool), "--bindings-json", str(game / "mgs5vr-controls.ini")], text=True, encoding="utf-8"))
    identity = {"exe_sha256": file_hash(game / "mgsvtpp.exe"), "dll_sha256": file_hash(args.candidate_dll),
                "controls_sha256": file_hash(game / "mgs5vr-controls.ini"), "config_sha256": file_hash(game / "mgs5vr.ini")}
    rows = inventory(ROOT, bindings, args.equipment_inventory)
    report = summarize_runs(ROOT, rows, args.run, identity)
    report.update(generated_utc=datetime.now(timezone.utc).isoformat(), counts=dict(Counter(row["kind"] for row in rows)),
                  campaign=campaign, historical_inventory_complete=False)
    args.output.mkdir(parents=True, exist_ok=True)
    atomic_json(args.output / "coverage.json", report)
    atomic_json(args.output / "effective-bindings.json", bindings)
    (args.output / "index.html").write_text(render_html(report), encoding="utf-8")
    print(json.dumps({"output": str(args.output.resolve()), "counts": report["counts"], "release_ready": False}))


if __name__ == "__main__":
    main()
