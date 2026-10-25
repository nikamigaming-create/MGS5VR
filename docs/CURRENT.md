# Current MGS5VR work — 24 October 2026

**The completed body/arm candidate is ready for physical testing through
[Play-Dev.cmd](../Play-Dev.cmd). Release readiness remains the active goal.**
Work on main only; do not redo implemented repairs or repeat the failed mounted
approach. The full report map is [FIELD_FIXES.md](FIELD_FIXES.md).

## Sustained follow-up: correction awaiting validation

Retained run `20261025T042919220986Z` failed on cycle six after five complete
ordinary iDroid cycles and the sixth opening passed. During a native render stall
(max interval 432.981 ms), the camera published `rig_sequence=0` and
`idroid_device_tracked=false` while raw tracking and iDroid remained active.
The later failure images show recovery, not the transient itself.

The anchored-menu stale-skin fallback retained palms but omitted the handset.
The source correction holds the accepted image transaction for handheld iDroid
until a fresh skin arrives; Pause/Help spatial panels retain their fallback.
The regression checks both rejection and automatic recovery. Build and combined
retest are pending; candidate identity below still names the preceding build.

Current sustained timing is retained in
`artifacts/release-sustained-performance.json`. Fresh stereo frames remain below
runtime submissions even outside video capture. No no-drops claim. Both separate
final-eye clips completed; their roughly 3 fps capture cadence is not game FPS.
Owned session closed, Steam retained, published pin unchanged.

## Checked candidate

- Development DLL: `351d84672f0e1b065212f3503e267940c862d5b18df00d5793d20dfcf3bc3930`.
- Gameplay source: `5ba674fea4d2b1243bf06e5848dd288801bd73f7`; cuff implementation `3cccc52`.
- All 58 groups and installer checks passed. Both retained build slots verify.
- Published play remains `b627c7cacc9a` / October 19.1. No package, publication
  or pin change. Outcome documentation is newer than the checked gameplay build;
  do not rebuild solely for prose. Reconcile package source before publication.

## Delivered in this correction batch

Forearm-only cut rims fit the native elbow plane. Caps use separate normals and
one native fabric sample instead of stretching the shoulder atlas across them.
Dead native vertex slots and exclusive removed triangle slots preserve buffer
sizes, skin weights, distal geometry and overlapping LOD draws. The game creates
107 cap triangles for the admitted owned sna5 geometry.

Combined visual review rejected intermediate `250884f4c156`: full-body-off was
active, but upper sleeves remained visible. The owned model binds normal/tangent,
skinning and UV groups through repeated slots of its attribute buffer; the new
selector incorrectly rejected aliases. The final correction replaces/restores
**every** matching slot, preserving native offsets and 28/32/36-byte attribute
strides. The owned player draw now selects the crop. Full-body and shadow draws
retain original buffers. Existing fixed native thigh/shin lengths, pelvis fit,
sleeve clearance and tracked garment repairs remain intact.

## Combined result

- Forearm-only: retained `20261025T040914930453Z`, all nine cases passed.
- Full body: retained `20261025T040959528958Z`, all nine cases passed.
- Each group covers both-arm reach/return, left iDroid front/turned/stow,
  immediate reopening and restow. Body mode is asserted in the cases.
- Both final eyes reviewed: upper sleeves are absent with full body off; cuffs
  are flat and closed; left bionic forearm/hand and the handheld iDroid remain.
  Full-body geometry returns with the setting on. Both leg scales remain1.
- These are scoped state and endpoint-visual results. They do not establish
  continuous-motion, every-outfit, whole-mission or physical-headset acceptance.
- Earlier failed/interrupted runs remain recorded. Startup autosave/offline
  notices and a loading-transition capture were setup interruptions, not newly
  repaired gameplay defects. The game was offline during the final session.

## Remaining release limits

1. Physical Quest Link: scale/reach, wrist isolation, headset movement, sight and
   handset comfort/readability still need actual feedback on this candidate.
2. Full-body extreme cross-head cloth still crowds part of the view. Other
   outfits remain outside exact forearm geometry admission. Do not call all
   body/arm reports closed.
3. Terrain: descending Afghanistan traversal did not reproduce the reported
   small-rock stop. Exact rock/corridor cases remain undiagnosed. No collision
   change was justified by a route deadline or combat interruption.
4. Mounted: field anti-air gun remained intact, but native entry was not reached.
   Combat and a short-movement bound stopped approach. No live action45/contact
   was observed; the adapter still admits mortar46 and machine-gun47. A new
   attempt needs a changed fixture/prerequisite, not another identical route.
5. Broader forced lessons, other weapons, mission completion and sustained
   performance remain unaccepted. The five conservative catalog gates remain;
   no release-gate waiver or universal playability claim.

## Extreme-reach follow-up — retained evidence only

The two final runs retain requested controller grips and rendered palms. Neutral
return error is below1mm; extreme cross-head error is44–59cm. This is not, by
itself, a new tracking or mesh-penetration defect: the captured native regression
fixture has28.4cm/25.5cm arm segments and asks for a wrist106cm from its shoulder.
Its reach-only lower bound is52.1cm of clamping. An independent200,000-direction
comparison of the existing head-clearance fallback improved target distance by
only0.9–1.2mm, depending on stereo padding. No solver rewrite was justified.

That older captured fixture establishes reach infeasibility, not the current
cloth's exact eye distance. Current endpoint records lack a joined full bone
palette, so mesh penetration is unproven. Keep the extreme-view comfort limit
explicit; require a reachable-pose reproduction or physical feedback before
another clearance change. Candidate351d84672f0e remains unchanged. Analysis is
retained in `artifacts/release-reach-position-audit.json` and
`artifacts/release-reach-feasibility-audit.json`.

## Cleanup and launch readiness

Full_body is restored to1. Owned game, simulator and bounded readers are closed;
no test menu or gameplay harness remains. Steam24696 was retained. Both Play
commands explicitly use Meta Link, the saved HeadsetRuntime is Meta, and the
shared headset launcher's Meta CheckOnly succeeds.

Windows' global 64-bit OpenXR registration currently names Virtual Desktop.
Restoring Meta there was denied by registry permissions. This is distinct from
the verified explicit Meta selection in both MGS5VR Play commands; do not claim
that the global registration was restored or switch either launcher away from Meta.

Review details: `artifacts/release-completion-progress.json`; retained evidence:
[FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json). Continue from
[RELEASE_PLAN.md](RELEASE_PLAN.md), with the current physical-feedback request
pending. Source and published pin are preserved.
