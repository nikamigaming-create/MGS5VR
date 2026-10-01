"""Refresh the fixed game map/state model without controlling a game."""
import argparse
import json
from pathlib import Path
import subprocess

from gameplay_bot.campaign import file_hash, fingerprint, read_json
from gameplay_bot.core import BotFault, atomic_json, matches
from gameplay_bot.navigation import NavigationAtlas
from gameplay_bot.state_graph import StateGraph, declared_model, observation_key

ROOT = Path(__file__).resolve().parents[1]


def render_html(report):
    data = json.dumps(report).replace('</', '<\\/')
    return '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MGS5VR game model</title>
<style>body{margin:0;background:#eee9df;color:#282827;font:16px/1.5 Arial,sans-serif}main{max-width:1440px;margin:auto;padding:28px}h1{font-size:44px;line-height:1.05}select,input,button{font:inherit;padding:9px;margin:4px 5px 4px 0;max-width:100%}.note{border-left:4px solid #b6382f;padding:12px;background:#faf7f0}table{border-collapse:collapse;width:100%}td,th{padding:8px;text-align:left;border-bottom:1px solid #aaa397}svg{width:100%;min-width:760px}#diagram{overflow:auto}pre{white-space:pre-wrap;overflow-wrap:anywhere}details{border-top:1px solid #aaa397;padding:12px 0}.edge{stroke:#a3312b;stroke-dasharray:5 4}.ready{stroke:#396f57;stroke-dasharray:none}rect{fill:#fbf8f0;stroke:#8c897f}text{fill:#282827;font-size:12px}[data-node]{cursor:pointer}.selected rect{stroke:#b6382f;stroke-width:3}</style>
<main><h1>Maps, states and transitions.</h1><p>One model for spatial routing and VR test routing. Select a state to inspect its immediate connections and missing adapters.</p>
<p class="note">Discovery and full-game acceptance remain open. Green edges are authored probes with guards, not passed game paths. Plans require fresh final-eye review and an observed native outcome at every step.</p>
<p class="note">Open report: another player's helicopter upgrade lock-up occurred in the ACC. Field list/Back evidence does not cover it. Search ACC helicopter paths to inspect the remaining work.</p>
<h2>Navigation atlas</h2><table><thead><tr><th>Location</th><th>Tiles</th><th>Nodes</th><th>Coverage</th></tr></thead><tbody id="worlds"></tbody></table>
<h2>State graph</h2><p id="counts"></p><input id="search" placeholder="Search states, mission codes or menus" aria-label="Search states"><select id="kind" aria-label="State family"><option value="">All state families</option></select><select id="state" aria-label="Selected state"></select><div id="diagram"></div><pre id="details"></pre><div id="edges"></div>
<h2>Observed owner probes</h2><p>Retained native transitions below cover coarse control ownership. Exact menu pages, modes and headset acceptance remain open.</p><div id="evidence"></div>
<h2>Next guarded test</h2><pre id="plan"></pre><p>Runtime execution uses the existing supervised VR runner with fresh entry/outcome checks and owned-session cleanup. This page dispatches no input.</p></main>
<script>const report=''' + data + ''';const graph=report.graph;const states=graph.states;
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const select=document.querySelector('#state'),search=document.querySelector('#search'),kind=document.querySelector('#kind');
document.querySelector('#worlds').innerHTML=report.atlas.locations.map(w=>'<tr><td>'+esc(w.key)+' ('+w.code+')</td><td>'+w.tiles+'</td><td>'+esc(w.nodes??'—')+'</td><td>'+esc(w.coverage)+'</td></tr>').join('');
document.querySelector('#counts').textContent=Object.keys(states).length+' declared states/obligations · '+graph.transitions.length+' transition obligations · '+graph.transitions.filter(e=>e.ready).length+' authored probes. These counts are not a percentage of the game.';
document.querySelector('#evidence').innerHTML=report.native_owner_evidence.length?report.native_owner_evidence.map(e=>'<p>'+esc(e.transition)+' · '+esc(e.case)+'<br>'+esc(e.run)+'</p>').join(''):'<p>No retained matching native owner evidence selected.</p>';
[...new Set(Object.values(states).map(s=>s.kind))].sort().forEach(k=>{const o=document.createElement('option');o.value=k;o.textContent=k.replaceAll('_',' ');kind.append(o)});
function choices(){const current=select.value,q=search.value.toLowerCase();select.innerHTML='';Object.entries(states).filter(([id,s])=>(!kind.value||s.kind===kind.value)&&(id+' '+s.title).toLowerCase().includes(q)).forEach(([id,s])=>{const o=document.createElement('option');o.value=id;o.textContent=id+' · '+s.title;select.append(o)});if([...select.options].some(o=>o.value===current))select.value=current;draw()}
function draw(){const id=select.value;if(!id){document.querySelector('#diagram').innerHTML='';document.querySelector('#details').textContent='No matching states';document.querySelector('#edges').innerHTML='';return}const linked=graph.transitions.filter(e=>e.from===id||e.to===id);const visible=linked.slice(0,12);const height=Math.max(160,visible.length*64+40),centerY=height/2;let svg='<svg viewBox="0 0 1100 '+height+'"><defs><marker id="arrow" markerWidth="9" markerHeight="9" refX="8" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8" fill="#8c897f"/></marker></defs>';
const box=(key,x,y,selected=false)=>'<g data-node="'+esc(key)+'" class="'+(selected?'selected':'')+'"><rect x="'+x+'" y="'+y+'" width="320" height="48" rx="5"/><text x="'+(x+10)+'" y="'+(y+20)+'"><title>'+esc(states[key].title)+'</title>'+esc(key.length>43?key.slice(0,40)+'…':key)+'</text><text x="'+(x+10)+'" y="'+(y+37)+'">'+esc(states[key].kind.replaceAll('_',' '))+'</text></g>';
visible.forEach((e,i)=>{const outgoing=e.from===id,key=outgoing?e.to:e.from,x=outgoing?760:10,y=i*64+20;svg+='<path class="edge '+(e.ready?'ready':'')+'" d="M'+(outgoing?710:330)+','+(outgoing?centerY:y+24)+' L'+(outgoing?760:390)+','+(outgoing?y+24:centerY)+'" fill="none" marker-end="url(#arrow)"><title>'+esc(e.id+' · '+(e.reason||'Authored probe; acceptance unproven'))+'</title></path>'+box(key,x,y)});svg+=box(id,390,centerY-24,true)+'</svg>';document.querySelector('#diagram').innerHTML=svg;document.querySelectorAll('[data-node]').forEach(el=>el.onclick=()=>{kind.value='';search.value='';const target=el.dataset.node;choices();select.value=target;draw()});
document.querySelector('#details').textContent=JSON.stringify({id,...states[id]},null,2);document.querySelector('#edges').innerHTML=(linked.length>12?'<p>Showing 12 of '+linked.length+' connections above; all are listed below.</p>':'')+linked.map(e=>'<details><summary>'+esc(e.id)+' · '+(e.ready?'authored probe':'blocked')+'</summary><pre>'+esc(JSON.stringify(e,null,2))+'</pre></details>').join('')}
[kind,search].forEach(el=>el.addEventListener('input',choices));select.addEventListener('change',draw);choices();select.value='owner.on_foot';draw();document.querySelector('#plan').textContent=JSON.stringify(report.next_test,null,2);
</script>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--candidate-dll', type=Path, required=True)
    parser.add_argument('--controls-tool', type=Path, required=True)
    parser.add_argument('--navigation', type=Path)
    parser.add_argument('--run', action='append', type=Path, default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    identity = {'exe_sha256': file_hash(args.game_dir/'mgsvtpp.exe'), 'dll_sha256': file_hash(args.candidate_dll),
                'controls_sha256': file_hash(args.game_dir/'mgs5vr-controls.ini'), 'config_sha256': file_hash(args.game_dir/'mgs5vr.ini')}
    bindings = json.loads(subprocess.check_output([str(args.controls_tool), '--bindings-json',
        str(args.game_dir/'mgs5vr-controls.ini')], text=True, encoding='utf-8'))
    graph = StateGraph(ROOT, declared_model(ROOT), identity, bindings)
    worlds = read_json(ROOT/'tools/gameplay_bot/catalogs/native-worlds.json')
    atlas = NavigationAtlas(read_json(args.navigation) if args.navigation else [], worlds,
                            base=args.navigation.parent if args.navigation else ROOT)
    summary = atlas.summary()
    for world in summary['locations']:
        if world['tiles']:
            try:
                nav = atlas.select(world['code'])
                world.update(nodes=len(nav.positions), directed_edges=sum(len(v) for v in nav.adj.values()),
                             content_identity=nav.identity, unresolved_portals=len(nav.unresolved_portals))
            except (BotFault, OSError) as error:
                world.update(coverage='blocked_import', error=str(error))
    observations, native_edges, covered = [], [], set()
    for run in args.run:
        if not (run/'KEEP').is_file():
            raise BotFault('Pin the reviewed run before using model evidence')
        recorded, result = read_json(run/'identity.json'), read_json(run/'result.json')
        arrival = result.get('arrival', {}).get('state')
        if arrival:
            observations.append({'run': str(run), 'phase': 'arrival', 'identity': recorded,
                                 'observed_state': observation_key(arrival), 'acceptance': 'unproven'})
        if (result.get('graph_identity') == graph.identity and fingerprint(recorded) == fingerprint(identity)
                and result.get('status') == 'observed_pass'):
            transitions = result.get('transition_ids', [])
            cases = result.get('cases', [])
            if len(transitions) == len(cases):
                edges = {edge['id']: edge for edge in graph.edges}
                for name, case in zip(transitions, cases):
                    edge = edges.get(name)
                    if (edge and edge.get('ready') and case.get('status') == 'observed_pass' and not case.get('release_error')
                            and not case.get('capture_error')
                            and matches(case.get('before', {}), edge['case']['before'])
                            and matches(case.get('after', {}), edge['case']['after'])):
                        covered.add(name)
                        native_edges.append({'transition': name, 'run': str(run), 'case': case['id'],
                            'scope': 'Observed coarse control-owner transition only; exact native mode/page and headset acceptance remain open'})
    report = {'schema': 1, 'graph': graph.export(), 'atlas': summary, 'observations': observations,
              'next_test': graph.next_test('owner.on_foot', identity=identity, covered=covered),
              'native_owner_evidence': native_edges,
              'discovery_complete': False, 'full_game_acceptance': False,
              'limits': ['Owner probes do not certify exact menus or native modes',
                         'All authored sequences, legal choices and failure/recovery branches require discovery',
                         'NPC navigation remains a prior; player clearance and actual arrival require native movement',
                         'Paired final-eye temporal review and physical-headset acceptance remain separate']}
    atomic_json(args.output/'game-model.json', report)
    (args.output/'game-model.html').write_text(render_html(report), encoding='utf-8')
    print(json.dumps({'states': len(graph.nodes), 'transitions': len(graph.edges),
        'authored_probes': sum(e['ready'] for e in graph.edges), 'blocked': len(graph.blockers()),
        'navigation': summary, 'full_game_acceptance': False}))


if __name__ == '__main__':
    main()
