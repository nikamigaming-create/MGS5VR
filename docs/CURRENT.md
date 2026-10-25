# Current MGS5VR work — 25 October 2026

## Current development candidate — arm/pickup batch

**`a33bfd559b9e` / source `f5dfb37` is built, checked and deployed for
Play-Dev.** All 58 groups and installer checks passed in
`artifacts/release-arm-pickup-build.log`. Published play is unchanged.

The C++ visibility change retains an accepted full-body policy across the
camera-to-skin publication gap, bounded by owner/model, activation, reference,
presentation/menu generation and 150 ms age. Failed solves still clear it.
Native leg lengths and collision are unchanged. Four full-body forward/back
segments completed in retained `20261025T143642333671Z`; review of all 596
encoded native-source-eye frames at the left arm found no visibility-mode pop.
Four forearm-only segments completed in `20261025T143743590555Z`; both final
compositor eyes and 5 fps source-eye review preserve hands/forearms without
upper sleeves while disabled. Settings restored. This is one outfit/posture,
not every reach or physical Quest acceptance. Extreme cloth crowding remains open.

C++/Lua now stows the field-owned iDroid on verified native helicopter passenger
entry, releases its old borrow/open ownership, and permits a deliberate fresh
on-board terminal with UI-only stow. The fresh flight open/turn/stow/reopen/restow
and disembark cases passed in `20261025T143046727537Z`; both-eye checkpoints were
reviewed. Ground open/turn/stow/reopen/restow and back-rifle draw/stow passed in
`20261025T143543140256Z`, with both-eye checkpoints reviewed. Native Challenge
Tasks Help and its Exit Tutorial notice interrupted earlier field cases; they
were acknowledged through VR controls, without changing progress flags.

**Actual field pickup entry remains unaccepted.** The native pickup call returned
but the helicopter hovered above the LZ; no boarding was reached. Setup movement
in `20261025T144059390254Z` stopped after a released segment exceeded its bounded
envelope. This is neither boarding acceptance nor a reproduced terrain hang.
The Lua ownership cases test the handoff contract; they do not replace that
missing gameplay observation. Earlier pre-input fixture rejects and interrupted
cases remain retained and enumerated in FREE_PLAY_ACCEPTANCE.

Source-eye recordings precede compositor crop/composition. Final-eye images
were inspected separately. Do not repeat the completed motion/handset cases
just for pass counts. Continue diagnosed gameplay changes; remaining terrain,
extreme cloth, pickup entry and broader FIELD_FIXES coverage stay explicit.

## Previous stock candidate and retained results

**`da4ae29de73b` / source `7273c25` is built, checked and deployed for
`Play-Dev.cmd`.** The moving back-stock intrusion has scoped visual acceptance.
The visibility policy survives camera-to-rig publication gaps and updates at
the accepted rig publication; drawing restores the native back assembly.

- Retained `20261025T134726758222Z`: four forward/back crouched segments completed.
  All 430 recorded native-source-eye frames were reviewed at the affected
  upper-left edge; no stock flash was visible.
- Retained `20261025T134807665938Z`: physical wrist selection chose native back
  equipment 873, then draw/release completed. Both final eyes show the held
  rifle, including its stock, receiver and optic.
- Retained `20261025T134844370944Z`: all 374 recorded post-stow frames at that
  edge are clear, and released final eyes show both arms without the back stock.
  Three segments completed; the next stopped because native position did not
  settle promptly after neutral input. This is not a traversal pass or a
  reproduced terrain hang. All physical inputs were released.
- Source-eye recordings are not final-compositor stereo video. Physical Quest,
  other back weapons, extended handling and the broader FIELD_FIXES scope remain
  open. Do not turn this scoped visual repair into whole-game acceptance.

The build passed 58 groups and installer checks; log:
`artifacts/release-back-publication-build.log`. The transport run
`20261025T134415368151Z` passed loading but timed out at the simulator input
connection on disembark. After confirming neutral controls and unchanged flight,
`20261025T134703812123Z` completed disembark without replaying the route.
That transport failure remains retained. Game/simulator were closed through
`stop-sim`; Steam remained running. Published `play/` was not changed.

Next: physical review through Play-Dev, localize the exact rock/corridor failure,
and continue only diagnosed remaining gameplay repairs under FIELD_FIXES. Do not
repeat the stock walk, ACC soak or ordinary iDroid route to accumulate passes.

## Active combined correction batch

Candidate `971dfff38c29`, source `c8a7a29`, built, checked and deployed the following batch:

- Paused modular-arm cache covers all 32 admitted owned parts, retires replaced
  models and invalidates frozen poses on resource changes.
- Forearm-only rendering has 20 exact owned outfit buffer profiles instead of
  one sna5 profile. Each profile supplies native partitions, LODs, attribute
  layout and elbow plane; female elbows remain at 0.432 m, male at 0.472 m.
  Bilateral partitions reserve both arms' retained vertices when closing cuffs.
  Index and vertex variants are joined as one resource transaction. No native
  geometry is distributed. Other outfits and visual acceptance remain open.
- Body/camera mounted admission uses the actual native action ID, independently
  of input focus. Generic NORMAL_ACTION transitions cannot select a mounted eye
  or skip the on-foot floor fit merely because they use native action controls.
- Mounted grip leases include action identity; the input consumer rejects aim
  left over from another weapon action. Mortar/MG admission remains 0x46/0x47;
  no guessed gatling action has been added.

User clarification: terrain worked before the first-person rig transition.
The exact small-rock/corridor failure remains unlocalized; the action-admission
correction is not yet established as its cause. Native movement still owns
actor collision, stairs and slopes; IK changes render channels and restores
animation inputs after publication. Do not replace that controller speculatively.

Controller design references reviewed: [Godot CharacterBody3D](https://docs.godotengine.org/en/stable/classes/class_characterbody3d.html),
[Jolt CharacterVirtual](https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/class_character_virtual.html),
and [Unity XR Body Transformer](https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.1/manual/xr-body-transformer.html).
They distinguish collision-constrained movement from grounding/presentation.
Applying that separation here is an engineering decision, not evidence that
MGSV has a particular collision defect. Keep native locomotion authoritative.

Candidate `1933aa81fc8e` / `4538bc7` built and deployed, but its attempted
hip-stock safeguard did **not** remove the reported intrusion. Retained moving
run `20261025T131456983791Z` still shows it; adjacent weapon draw/release in
`20261025T131546151339Z` passed. Do not claim that candidate fixes this report.

Native slot isolation `20261025T132017913994Z` hides only PRIMARY_BACK during
four forward/backward segments: the intrusion disappears. Native visibility was
restored in cleanup. This diagnostic is not acceptance. The back assembly is
already attached to the fitted body; its stowed stock crosses the first-person
eye during crouched gait. The earlier hip-stock hypothesis is reverted.

The correction now in source uses native main-view group concealment for the
owned stowed custom back assemblies (1/4), preserving their shadows. Drawing or
manipulating the selected back weapon restores its groups. Cabin inspection,
scripted presentation, other actors and other weapon slots retain their existing
ownership. Typed player/service/interface/resource joins and the verified native
part resolver identify every assembly part; no arbitrary nearby mesh is hidden.
Candidate `77a2fe7c9dc2` / `bc0984d` built and deployed; all 58 groups and
installer checks passed. Back-rifle selection/draw/release in retained run
`20261025T133824547414Z` shows native equipment 873 and the held rifle in both
final eyes. Moving review **failed**: 5 fps upper-left crops in retained runs
`20261025T133744460062Z` and `20261025T133919295859Z` reveal stock flashes that
the initial 2 fps full-frame review missed. Do not claim this candidate fixes it.

The camera callback publishes a new native source before asking for the last
rig; movement retires that rig until its matching skin arrives. The correction
now preserves owned concealment across that gap and applies visibility again
immediately after the accepted rig publication. Building and reviewing this
timing correction together with moving stow and back-weapon draw/stow was the
next step; its completed result is recorded at the top of this document.
Transport run `20261025T133444697660Z` retained a blank right compositor frame
during loading; native loading continued and arrival resumed from flight without
replaying the menu route. This remains a recorded failure, not a transition pass.
Published `play/` remains unchanged.

The earlier combined route `20261025T125553142316Z` reported ten completed
cases: forearm fallback poses, left iDroid, Rival Help dismissal, free-roam
selection, loading and disembark. Selected arm/iDroid eyes were inspected.
That scratch run expired before retention, so it is a work observation rather
than retained acceptance evidence; do not repeat it just to replenish a count.

Retained traversal `20261025T125843318333Z` completed six segments before native
displacement exceeded the bot bound after release. This was excess movement,
not a terrain hang. Four forward/backward segments in the retained moving-stock
recording completed with valid full-body fitting. The exact terrain hang remains
unlocalized. The peripheral fixture `20261025T130109341945Z` dispatched no motion
because its outcome was already satisfied; it remains a failed fixture.

The moving/held-weapon review is now recorded above. Continue remaining
diagnosed work without ACC soak repeats.
Do not publish or advance the play pin from a build alone.

## Previously checked candidate

**Candidate `d4b526e24cf5` is built and deployed from source `32f2ab0`.
All 58 groups and installer checks passed. Use `Play-Dev.cmd` for the user's
physical test. No new simulator/game session was launched. Full-game and
physical-headset acceptance remain open.**
Work on main only. `build/dev/BUILD.json` identifies the checked candidate;
`play/` remains the last published release. [FIELD_FIXES.md](FIELD_FIXES.md)
separates implemented repairs from unresolved reports and acceptance gaps.

## Batch contents — 25 October

- The owned rigid bionic shell follows the same paused forearm palette as the
  articulated hand, fixing the reported apparent third arm.
- iDroid reopening preserves the last accepted hand/device camera publication
  until its new skin arrives. The compositor now recognizes that wait as an
  expected handoff: retain the accepted stereo image, reject new unjoined pixels,
  and resume on the fresh rig. Camera/reference/presentation identities must match.
  Native Help/Pause continues to use its existing spatial fallback.
- Forearm-only cropping previously retained the first sixteen native object
  buffers forever. Later replacement buffers could never register, leaving
  shoulder geometry uncropped. The bounded cache now recycles the least recently
  used completed entry and never evicts a mapped buffer. This is a source-level
  lifecycle correction; it does not expand the admitted outfit geometry.

Build/check/deploy completed with `tools/build.ps1 -SkipBootstrap -Deploy`.
The exact outcome is in `artifacts/release-batched-candidate-outcome.json`,
FREE_PLAY_ACCEPTANCE and the development manifest. The first contract run
exposed fixtures missing the required new skin publication; their sequencing
was corrected before the successful combined checks. No live loop was run.
Physical testing belongs to the user through `Play-Dev.cmd`; no pin or release
publication is part of this candidate deployment.

## Duplicate left forearm — 25 October

The user's screenshot is reproduced in both final eyes during the native
"Choose a Rival" iDroid Help state. The owned prosthetic contains an articulated
34-bone hand and a separate rigid shell (`84a051ef13b1e379`, one LFOREARM bone,
root parent -1). The refresh required both LFOREARM and LHAND, so it updated the
hand but rejected the shell. The sampled shell was about 20 cm behind the
body's forearm matrix, creating the apparent third arm.

The correction admits that exact owned shell/rig and joins it to the same
native body palette. It does not hide or delete the shell. The original model
ownership, matching root, arm group and native bone-length checks remain.
Regression coverage includes admission, rejection of foreign single-bone parts,
and rigid/idempotent pose transfer. All 58 groups and installer checks pass.
Current both-eye endpoints cover hand raise/return and head pitch in the exact
forced Help state and ordinary Pause. All 135 frames of the 45-second, 3 fps
left-eye motion capture were inspected chronologically. No stationary duplicate
shell remains in those samples. Sparse single-eye video and stereo endpoints
do not establish full-rate stereo or physical-headset acceptance.

Evidence: `artifacts/duplicate-left-arm-report/` contains the user image,
current both-eye captures and read-only native model/pose inventory. Retained
run `20261025T112301566248Z` failed when the first opening entered forced Help.
The reviewed native Close and stow then passed in `20261025T113207144511Z`.
Current run `20261025T114234180520Z` passed 13 cases before an optional legacy
Help fixture rejected its entry context; it dispatched no input for that case.
Keep the overall run failed. The exact forced-Help motion cases already passed.
Run `20261025T114651540109Z` passed eight sustained cases before a native
Daily Bonus notice consumed Back during cycle three; cleanup closed iDroid.
Run `20261025T115331588265Z` also passed eight cases before Daily Bonus
consumed Back in cycle six. Final run `20261025T115918951584Z` passed the
seventh cycle and first immediate reopen/stow, then caught an unjoined camera
publication at the second reopening. All three overall runs remain failed.
Visual record: `artifacts/release-rigid-shell-visual-review.json`.

## Reopen failure and source correction

Run `20261025T115918951584Z` reports `menu_idroid=true`, `rig_sequence=0`
and no device transform at the second immediate reopening. Stereo already
rejected that unjoined frame, but camera resolution overwrote the accepted
hand/device anchor. The original empty-rig correction also used a mismatch
status that rejected retained stereo. This batch keeps the anchor and explicitly
permits only the previously accepted stereo image during the expected wait.
The focused regression covers retained pixels, rejection of the old stowed skin,
fresh-skin recovery and the existing native Help/Pause fallback. These contracts
passed in the completed build; no new native gameplay pass is claimed.

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

## Frame-handoff comparison on the corrected candidate

The shared producer fence lets XR admit a completed copy without waiting for
another native Present. Ownership transitions remain atomic; EVENT fallback,
GPU completion checks and the three-slot bound remain. GPU regressions cover
future markers, no-more-Present delivery and concurrent pixel/lineage reuse.

Current partial settled-window medians are 88.49 fresh pairs/sec front, 88.68
turned and 87.30 stowed at about 90 submissions/sec, versus 75.83/76.12/62.42
on the a6dc baseline. Config, effective bindings, game executable, eye FOVs and
body mode match. The current online page/notice state differs from the earlier
session, so the stowed comparison is the stronger scene match. These window
statistics support reduced repeats in this ACC sample, not universal frame
pacing or physical display acceptance. The later completed windows retain the improvement (about 89 fresh pairs/sec);
the interrupted runs do not certify a clean integrated soak.
Details: `artifacts/release-rigid-shell-performance.json`.

Microsoft API contracts: [shared fence creation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/nf-d3d11_3-id3d11fence-createsharedhandle)
and [opening on the receiving device](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nf-d3d11_4-id3d11device5-opensharedfence).

## Earlier sustained gameplay review (preceding candidate)

- Reviewed DLL: `a6dc5fb91151ed40c58a4ea4a67543112bf62ebc14cd5f45ea29bfef708128f6`.
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

Full_body is1. The owned game/simulator and bounded bots are closed; no test
menu remains. Steam30812 is retained. Both Play
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
