# Current MGS5VR work — 24 October 2026

**The completed body/arm candidate is ready for physical testing through
[Play-Dev.cmd](../Play-Dev.cmd). Release readiness remains the active goal.**
Work on main only; do not redo implemented repairs or repeat the failed mounted
approach. The full report map is [FIELD_FIXES.md](FIELD_FIXES.md).

## Sustained iDroid correction and result

The first extended run (`20261025T042919220986Z`, candidate `351d84672f0e`)
failed in cycle six: a 432.981 ms native frame interval let the anchored-menu
fallback publish new eyes with retained palms but no tracked handset transform.
The later failure images show recovery, not the transient itself. Keep that run
failed. The correction holds the accepted image transaction until a matching
fresh handheld skin arrives; ordinary Pause/Help panels keep their fallback.
A deterministic regression checks rejection and automatic recovery.

Corrected candidate `a6dc5fb91151` passes retained run `20261025T044511277442Z`:
27 of 27 cases, seven sustained left-iDroid cycles, Pause open/close and two
immediate open/stow round trips. Bounded observation totals 667.83 seconds.
Both final eyes reviewed for Pause, turned iDroid and final reopening. Valid body
samples retain leg scales 1, 1. This is ACC state/endpoint acceptance, not every
forced guide, continuous stereo, whole mission or physical-headset acceptance.

Current timing now exists: settled window medians are about 90 submissions/sec,
with 75.83 fresh stereo pairs/sec open/front, 76.12 turned and 62.42 stowed.
Frame repeats remain an explicit performance limit. These are logged window
statistics, not per-frame percentiles or physical display measurements.
Details: `artifacts/release-handheld-stall-outcome.json` and
`artifacts/release-handheld-stall-performance.json`.

## Frame-handoff correction awaiting build and comparison

Retained current-candidate logs show producer completion waiting for the next
native Present before XR can see an already completed image. The source change
exports the producer completion fence and opens it on the XR device. XR can
admit completed copies without using the native immediate context. Atomic
readiness promotion preserves a slot already claimed by the consumer; EVENT
compatibility and the three-slot bound remain. GPU regressions cover a future
completion marker, no-more-Present delivery and concurrent stereo pixel/lineage
reuse. A combined candidate run must establish whether this reduces repeats.

Microsoft API contracts: [shared fence creation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/nf-d3d11_3-id3d11fence-createsharedhandle)
and [opening on the receiving device](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nf-d3d11_4-id3d11device5-opensharedfence).

## Checked candidate

- Development DLL: `a6dc5fb91151ed40c58a4ea4a67543112bf62ebc14cd5f45ea29bfef708128f6`.
- Gameplay source: `f11d0d0878268a59e4c06d5c334078610a5f21ef`; body/cuff batch `5ba674f`.
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

## Prior body/arm review (body code unchanged)

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
   performance remain unaccepted; current repeats are measured above. The five conservative catalog gates remain;
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
another clearance change. The reach solver remains unchanged in the current candidate. Analysis is
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
