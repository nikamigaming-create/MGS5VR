const escape=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const labels={verified_automated:'Automated check passed',verified_simulator:'Simulator check passed',failed:'Failed',needs_headset:'Headset test needed',not_tested:'Not tested',not_implemented:'Not built'};
let ledger=null,loading=null,proofs=[];
export async function communityLedger(){
 if(!loading)loading=fetch('community-verification.json').then(r=>{if(!r.ok)throw Error('Community test results could not be loaded.');return r.json()}).then(d=>ledger=d);
 return loading;
}
function counts(claims){const n={};for(const c of claims)n[c.status]=(n[c.status]||0)+1;return n}
function summary(claims){const n=counts(claims);return [
 [(n.verified_automated||0)+(n.verified_simulator||0),'passed'],[n.failed,'failed'],
 [n.not_tested,'not tested'],[n.needs_headset,'need headset'],[n.not_implemented,'not built']
 ].filter(([count])=>count).map(([count,label])=>`${count} ${label}`).join(' · ')}
function evidenceLink(e){const index=proofs.push(e)-1;return `<button class="proof-link" data-proof="${index}">${escape(e.path.split(/[\\/]/).at(-1))}</button>`}
export async function paintCommunity(query,filter){
 const d=await communityLedger();proofs=[];
 const all=d.reports.flatMap(r=>r.claims),n=counts(all);
 document.getElementById('notes-counts').innerHTML=[
 [d.reports.filter(r=>r.type==='defect').length,'reported problems'],
 [d.reports.filter(r=>r.type==='feature').length,'feature requests'],
 [(n.verified_automated||0)+(n.verified_simulator||0),'checks passed'],
 [n.failed||0,'checks failed'],[n.not_tested||0,'checks not run']
 ].map(([v,label])=>`<div><b>${v}</b><span>${label}</span></div>`).join('');
 const rows=d.reports.filter(r=>[r.id,r.title,...r.claims.flatMap(c=>[c.title,c.finding,c.next_action])].join(' ').toLowerCase().includes(query))
  .filter(r=>!filter||(filter==='feature'?r.type==='feature':r.claims.some(c=>filter==='passed'?c.status.startsWith('verified_'):c.status===filter)));
 document.getElementById('note-list').innerHTML=`<p class="verification-scope">${all.length} individual checks across 55 reports. Each result applies to the test and build shown below. Physical headset acceptance: <b>not completed</b>.</p><p class="help">Candidate: <code>${escape(d.candidate.dll_sha256.slice(0,16))}</code> · ${rows.length} matching reports</p>`+rows.map(r=>`<details class="note-row" data-report="${escape(r.id)}"><summary><span>${escape(r.id)} / ${escape(r.title)}</span><small>${summary(r.claims)}</small></summary><p class="report-kind">${r.type==='feature'?'Feature request':'Reported problem'} · ${escape(r.priority)}</p>${r.claims.map(c=>{
 const builds=[...new Set(c.evidence.map(e=>e.build_sha256).filter(Boolean))];
 return `<article class="claim" data-result="${escape(c.status)}"><div class="claim-title"><h3>${escape(c.title)}</h3><span class="result-badge">${labels[c.status]}</span></div><p>${escape(c.finding)}</p>${c.next_action?`<p class="next-check"><b>Next check:</b> ${escape(c.next_action)}</p>`:''}${builds.length?`<p class="help">Tested build${builds.length>1?'s':''}: ${builds.map(b=>`<code>${escape(b.slice(0,16))}</code>${b!==d.candidate.dll_sha256?' (earlier build)':''}`).join(', ')}</p>`:''}${c.evidence.length?`<details class="claim-proof"><summary>Evidence (${c.evidence.length} records)</summary><div class="proof-list">${c.evidence.map(e=>evidenceLink(e)+(e.related?.length?`<div class="related-proof">${e.related.map(evidenceLink).join('')}</div>`:'')).join('')}</div></details>`:''}</article>`}).join('')}</details>`).join('');
}
export async function openCommunityProof(index){
 const e=proofs[index];if(!e)throw Error('Evidence selection is no longer available.');
 const dialog=document.getElementById('proof-dialog'),body=document.getElementById('proof-content');
 document.getElementById('proof-title').textContent=e.path.split(/[\\/]/).at(-1);
 document.getElementById('proof-identity').textContent=`${e.path}\nSHA-256: ${e.sha256}${e.build_sha256?'\nBuild: '+e.build_sha256:''}`;
 body.replaceChildren();dialog.showModal();
 if(e.kind==='image'){const img=document.createElement('img');img.src=e.url;img.alt=e.path;body.append(img);}
 else {body.textContent='Reading evidence…';const response=await fetch(e.url);if(!response.ok)throw Error('Evidence file is missing.');const value=await response.text();const pre=document.createElement('pre');pre.textContent=value;body.replaceChildren(pre);}
}
