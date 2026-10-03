# Regression coverage and release gates

The [machine-readable catalog](../tests/vr_regression_catalog.json) accounts for
47 implemented behavior families: 36 gameplay, six runtime and five tooling.
These are an explicit audit denominator, not 47 completed features. A native
button route, source file, test fixture or owned item definition is not proof
that its full gameplay interaction works.

The published October 2 release and the current handheld-default, native
animation and performance candidate are different builds. The release's
41 CTest groups and older endpoint captures cannot clear a defect reproduced on
the newer candidate. Exact build, configuration and native evidence identities
remain in the controlled local acceptance ledger; this public catalog contains
source references and honest coverage scopes, not private captures or user data.

## Current gate

**Five coverage families remain blocked.** Candidate `92d1a69368bd` passed
44 CTest groups and installer transactions, including 120 real D3D11 mailbox
checks. These isolated checks do not accept native gameplay. Three linked
hand/arm and iDroid transition families retain unresolved device/animation
failures. The ACC Development report and measured performance failures
also retain blocked status. These statuses remain recorded in the package
manifest; they are not converted into passes by an experimental release.

Candidate `92d1a69368bd` adds the handheld opening defer while the native body
update is within 150 ms, five additional guarded chrome layouts (25 total),
and omission of only the mod-owned spatial-iDroid world pause during verified
native cabin play. Spatial field pausing remains. Retained
`20261003T104542847534Z` passed 13 spatial-mode state cases: Weapons/Items and
Buddy Equipment each returned from its actual child grid to the Development
root after one ordinary 120 ms VR Back, with both eyes reviewed. Helicopter
opened its armament grid; its Back decision was not consumed before the
supervisor deadline. Retained `20261003T112828492997Z` passed eight handheld
state cases, including one 120 ms Back from the settled Weapons/Items grid to
the category root, with both before/after eyes reviewed. Its ninth category
Down check reported `Native menu stick stayed held after release`; the runner
neutralized input and closed the owned game/runtime, retaining Steam. No cause
is established. Handheld Buddy Equipment and both-mode Helicopter/Security
exits stay open.

Retained `20261003T114628779393Z` passed seven handheld field cases: open,
cant, right/left yaw, ordinary Back, immediate reopening and final stow. All
14 outcome PNGs were reviewed: open outcomes retain the full-size right-hand
phone and both arms, and stow clears device UI. Initial/reopen left-eye images
capture the UI fade while later right-eye images are settled. They are
sequential outcomes, not continuous-boundary or simultaneous stereo evidence.
The preceding recording attempt stopped before cases at the 25 GiB reserve.
Retained `20261003T120302596202Z` passed four spatial field cases: open, one
120 ms Back, immediate reopen and final 120 ms Back. All eight outcome PNGs
show the expected Map-open/UI-cleared states and both arms. The final native
handset is mid-stow in the left capture and gone in the later right capture.
This does not establish simultaneous stereo, continuous animation or paused
world motion. Retained `20261003T115540064524Z` expected `pause=true`
incorrectly; all other guards matched 53 open observations. The native flag
describes Pause/Help UI, so the corrected private fixture expects `pause=false`
and retains every other guard.

Retained `20261003T114817998847Z` passed 12 state cases for five independent
injected raw-pose losses after opening, with 5.500–5.891-second measured holds
inside eight-second leases. All ten loss and ten recovery images were reviewed
with both arms, full-size phone, Map and world visible. Raw flags recover,
activation/epoch remain stable and current camera/rig publications advance.
The native player sequence remains fixed while the menu is open, and
compositor still records do not identify an immutable source frame.
This is bounded synthetic tracking evidence, not physical Quest idle testing,
opening with loss, API-stall recovery, continuous animation or proof against
repeated images.

Earlier candidate `1b641438f481` failed the sustained motion fixture in cycle
two on stale rig observations. Candidate `5a7bae2a2864` completed five full
cycles but failed in cycle six when a 205 ms GPU handoff aged the tracking
publication. Its reviewed left-arm opening disappearance and stow fades cleared,
but candidate `787d1828c937` still showed a tiny detached device on first opening.
The later `f694be5341fb` source recording shows a full-size attached handset at
first opening and one missing-both-arms image on reopening. Its exact closing
source pairs were not copied by the recorder. Candidate `7a5b00774fd2` passed
seven ordinary field state cases with both outcome eyes reviewed, but its
recording showed a detached small handset at first opening and left-forearm
Map chrome for one closing source. The current `92d1a69368bd` repairs target
those defects; the newer handheld outcome pass does not clear the exact
continuous boundaries seen in the earlier recording.
No candidate has promoted six-cycle continuous acceptance. Neither endpoint
state cases nor valid segmented source video closes that gate.

The retained capture-off ACC window on `4df840eda253` contains 130.092 seconds
of complete timing bins: 88.24 XR submissions/s, 40.14 fresh pairs/s, native GPU
mean 14.85 ms, and a submission-stage maximum of 653.33 ms. The consumer stage
averaged 0.092 ms and had no cycle over 11.111 ms in that window. A separate
recorded open-menu interval reached 121.168 ms consumption, with a nearby
120.955 ms keyed-mutex acquisition. Recording and scene workloads differ; these
figures are scoped observations, not a controlled performance comparison.
That older submission measurement combines layer preparation and `xrEndFrame`.
The later `f694be5341fb` capture-off stowed field window spans 295.233 seconds:
87.67 XR submissions/s and 54.98 fresh pairs/s, with the exact `xrEndFrame` API
reaching 811.755 ms while layer preparation stays below 0.182 ms. These are
different scenes and workloads, not a matched before/after comparison.
The initial `7a5b00774fd2` pre-image startup timeout remains unexplained; a
subsequent baseline-pacing boot of the same DLL reached native Present and XR.
Those older observations do not establish `92d1a69368bd` performance.
Full-game, continuous-feature and physical Quest/headset coverage remain open.

### Current user reports

The catalog preserves these reports as structured `current_user_reports`, linked
to existing behavior families. They do not inflate the feature count or rewrite
the historical R01–R55 report IDs.

| Report | Current evidence and unknowns | Required regression |
| --- | --- | --- |
| ACC Development freezes selection, navigation and Back | Blocked overall; scoped improvement on `92d1a69368bd`. Weapons/Items exited to Development after one ordinary 120 ms VR Back in both spatial and handheld runs; spatial Buddy Equipment also exited. Both eyes were reviewed for these outcomes. The handheld run's later category Down check failed its stick-release observation and cleaned up safely; cause unknown. Handheld Buddy and both-mode Helicopter/Security exits remain untested. The earlier `7a5b00774fd2` Weapons/Items failure had retail-confirmed B delivery while Up worked; a complete navigation freeze was not established. | Complete all four categories in handheld and spatial modes, enabled/disabled entries, selection/navigation, Back and immediate re-entry through effective VR bindings. Exclude purchases. |
| Supply drops hard to locate during a fight because markers were absent | Open, not reproduced in the reported fight. Fight, supply type, native marker identity and original settings are unknown. Current inspected HUD mode is binoculars-only; the replacement label path covers waypoints and acquired people, with no identified supply-specific replacement. | Reproduce request/landing/location cues while moving in the actual combat state. Keep firearm reticles off, physical optics and the left-arm HUD intact. |
| Intermittent weapon-selection trouble | Open, exact symptom unknown. Opening, category selection, confirmation, cancellation and native equip are distinct possibilities, not diagnosed causes. | Observe visible picker contents during input and resulting native weapon identity; repeat selection/cancel/reuse with movement and neutral releases. |
| Cutscene camera or animation bugs | Open, lower priority. Exact scene, transition and symptom are unknown. Older Mission 6 evidence does not resolve it. | Identify the native scene and review continuous entry, authored shots/animation and natural gameplay recovery in both eyes. |

Historical before/after stills remain useful within their original scope. Repair
must preserve the right-hand native iDroid, native animation, live UI and left-arm
ordinary HUD, then replay complete transitions with retained evidence. Exact
process/configuration identities and private captures remain in the controlled
local ledger. The public candidate ID remains
`october-2-unreleased-handheld-investigation`.

## What the counts mean

| Audit item | Count | Limit |
| --- | ---: | --- |
| Implemented behavior families | 47 | Scope is enumerated below; weapon/item/mission variants are separate obligations. |
| Historical release CTest groups | 41 | Isolated code/tool/GPU contracts, not 41 gameplay passes. |
| Current registered CTest groups | 44 | Adds catalog/reference, release-gate, native recorder timing and attachment lineage checks. |
| Families with a related registered CTest | 44 | Only the catalog's stated automated scope is covered. |
| Auxiliary tests only | 1 | D-Dog has a mocked Lua lifecycle fixture, not an automated pet-contact outcome. |
| No dedicated automated regression identified | 2 | Rat pickup/release and optic exposure/history isolation. |
| Public native fixture files | 17 | Includes the new six-cycle, 180-second iDroid motion suite; it is not a native acceptance pass. |
| Families with one or more initial public native fixtures | 12 | A broad fixture often tests only a small part of the family. |
| Retained scoped native observations | 30 | Earlier identified runs/builds; not current blanket passes. |
| Current bounded native observations/measurements | 1 | Tracking-loss diagnostic; failed native performance is counted as blocked. |
| Current blocked families | 5 | Three linked iDroid transition families, ACC Development report and measured performance failures. |
| Native outcome unverified | 6 | Source implementation or experiments do not prove native behavior. |
| Historical continuous evidence | 3 | Stereo/gameplay, left-arm display and D-Dog; limited builds, scenes and capture cadence. |
| Current accepted continuous feature runs | 0 | Endpoint passes do not close this gate. |
| Current accepted physical-headset regressions | 0 | Six families have older headset feedback, which includes defects. |
| Families missing sustained native regression | 42 | All gameplay/runtime families; the five external tools use their own checks. |
| Current accepted integrated ten-minute soaks | 0 | This remains a required gate, not an achieved result. |

The JSON summary is authoritative as fixtures and tests are added. The original
41-group list stays a historical baseline. New tests belong in the separately
validated current group inventory; they do not rewrite release history.

A complete list of existing CTest names, exact test sources and public native
suite paths is in the catalog. Some tests link several features; counts must not
be summed as independent executions. Auxiliary files are explicitly distinguished
from registered CTest groups.

The attachment tests cover exact owner, generation, activation epoch, palette,
post-native publication and temporal-order rejection. The typed routing tests
exclude shared Mission Orders from outgoing-device suppression. These isolated
contracts do not accept native attachment visuals or clear the gameplay blocker.

The current catalog maps all 110 source/header files. Its CTest validator fails
on new unmapped source, test groups or native suites, deleted paths, duplicate
references, invalid statuses and stale counts. Run it without the game:

```powershell
python tests/regression_catalog_tests.py
python tools/regression_catalog.py
```

The packager calls the same validator with the release gate before reading a
build or writing output. Known blocked behavior rejects packaging; pending and
experimental scopes are preserved explicitly in release metadata. Promotion
requires a reviewed acceptance record with matching candidate ID, an opaque
local-ledger ID and an existing public report. The report must contain matching
`acceptance-id: <id>` and `candidate-id: <id>` markers. These are record IDs,
not private paths or artifact hashes. CI validates the accounting and public
report; it cannot replace the maintainer's review of private native pixels.

Native `accepted_scoped`, continuous `accepted` and headset `accepted` are
separate statuses. Continuous promotion requires at least 180 seconds, five
cycles, both-eye and transition review, and matching sustained metrics. Headset
promotion additionally requires an actual physical session and hardware/runtime
identity. A soak needs at least 600 seconds; a shared reviewed soak ID counts
once. Current catalog values remain zero/missing and all known blockers remain.

## Sustained humanlike regression

For each applicable gameplay/runtime family, require at least **three minutes**
and **five complete reversible cycles**. One run may cover several families
provided each outcome is identified. Do not repeat consumables or progression
steps blindly; use their actual native prerequisites and record one-shot outcomes
separately.

The sequence must include independent head and hand movement, slow and brisk
poses, near/far inspection, neutral releases, natural pauses and appropriate
native locomotion before and after the interaction. Exercise at least one
relevant interruption or menu/context transition, then prove that native
animation, input ownership and gameplay recover. Tracking fault injection is
a bounded software diagnostic; physical occlusion, headset removal and runtime
system menus remain separate hardware tests.

Record activation/opening, steady use, exit/stow and immediate reuse throughout.
Inspect dense source-eye frames as well as final composited motion, and both eyes
where stereo matters. Review intermediate frames, not only screenshots taken
after native state flags settle. A single-eye recording alone cannot certify
stereo or physical comfort. Sparse recording cannot prove the absence of defects
shorter than its sampling interval.

An integrated **ten-minute minimum** soak must mix ordinary movement, wrist UI,
iDroid, weapons/optics and menu transitions with repeated natural re-entry.
Preserve native animation and the established interaction for each feature.
Use the relevant real scene; an unavailable tutorial, vehicle, actor or item is
an explicit unverified result. The soak does not certify the entire campaign.

Keep actual duration, repetition count, native state and input ownership,
source/pose generations, final-eye timestamps, failure cleanup and timing
statistics with the controlled evidence. Report recording cadence separately
from native CPU/GPU times, XR fresh submissions, repeats and over-budget stages.
Do not interpolate missing frames or turn a lower-resolution run into an
unchanged-sharpness performance claim.

## Behavior inventory

Native “retained scope” means a historical bounded observation, not current
acceptance. “Old feedback” is physical tester feedback, not a headset pass.
See each JSON entry for exact source paths, test names, scope and missing checks.

| ID | Implemented behavior | Automated | Public fixtures | Native | Continuous | Headset |
| --- | --- | --- | ---: | --- | --- | --- |
| `tpp-stereo` | TPP native stereo and source-frame ownership | 3 groups | 0 | retained scope | historical partial | old feedback |
| `render-resolution` | Independent desktop mirror and runtime eye resolution | 2 groups | 0 | retained scope | unverified | unverified |
| `proxy-lifecycle` | DirectInput forwarding, OpenXR lifecycle and shutdown | 2 groups | 0 | retained scope | unverified | unverified |
| `tracking-recovery` | Tracking loss, focus loss and session recovery | 3 groups | 0 | current scope | unverified | unverified |
| `presentation-modes` | Immersive and large-screen presentation switching | 3 groups | 0 | retained scope | unverified | unverified |
| `cinematic-transitions` | Authored cameras, interactive sequences and stale-demo recovery | 4 groups | 0 | retained scope | unverified | unverified |
| `cabin-continue` | Physical title cassettes and Continue handoff | 3 groups | 0 | retained scope | unverified | unverified |
| `cabin-movement` | Title cabin movement and bounded collision envelope | 1 groups | 0 | retained scope | unverified | unverified |
| `locomotion-posture` | Walking, running, native stances and contextual traversal routing | 3 groups | 3 | retained scope | unverified | old feedback |
| `turn-recenter` | Smooth turn, snap turn, recenter and height calibration | 3 groups | 0 | retained scope | unverified | old feedback |
| `hand-arm-rig` | Tracked hands, arm IK, fingers and native animation preservation | 3 groups | 3 | **BLOCKED** | **BLOCKED** | old feedback |
| `player-visibility` | First-person body concealment, native shadows and visibility restoration | 3 groups | 0 | retained scope | unverified | unverified |
| `firearm-actions` | One-hand firearm fit, tracked aim, native fire and reload | 3 groups | 1 | retained scope | unverified | old feedback |
| `support-grip` | Physical support-hand contact and release | 2 groups | 1 | retained scope | unverified | unverified |
| `weapon-scopes` | Authored weapon scopes and independent optical zoom | 2 groups | 0 | retained scope | unverified | unverified |
| `optic-stabilization` | Weapon and scope attachment stabilization | 2 groups | 0 | unverified | unverified | unverified |
| `optic-exposure` | Optic exposure and render-history isolation | none | 0 | retained scope | unverified | unverified |
| `physical-binoculars` | Handheld binocular equip, hand aim, zoom and stow | 2 groups | 1 | retained scope | unverified | unverified |
| `binocular-recon` | Binocular acquisition, explicit marking, intel and native tutorial events | 2 groups | 0 | retained scope | unverified | unverified |
| `binocular-material` | Experimental native binocular housing material | 2 groups | 0 | unverified | unverified | unverified |
| `left-wrist-hud` | Left bionic forearm weapon/status display and wrist popups | 2 groups | 1 | retained scope | historical partial | old feedback |
| `equipment-picker` | Wrist equipment categories, selection and cancellation | 3 groups | 2 | retained scope | unverified | unverified |
| `item-actions` | Native throw, place, detonate and item-use routes | 2 groups | 0 | retained scope | unverified | unverified |
| `wrist-commands` | Wrist Commands, native holds and selected buddy orders | 3 groups | 2 | retained scope | unverified | unverified |
| `idroid-mount` | Right-hand iDroid default, native device mount and live screen | 5 groups | 4 | **BLOCKED** | **BLOCKED** | unverified |
| `idroid-lifecycle` | Ordinary iDroid stow and immediate reopening | 3 groups | 4 | **BLOCKED** | **BLOCKED** | unverified |
| `idroid-native-guides` | iDroid native restrictions, completed guide recovery and tutorial escape | 3 groups | 4 | **BLOCKED** | unverified | unverified |
| `idroid-cone` | Experimental iDroid native projection-cone retargeting | 2 groups | 0 | unverified | unverified | unverified |
| `pause-panel` | Pause and explicitly selected off-wrist spatial panels | 4 groups | 3 | retained scope | unverified | unverified |
| `prompt-captions` | Effective-binding captions, Map footer and native popup observations | 4 groups | 0 | retained scope | unverified | unverified |
| `hud-policy` | World markers, recon visibility and physical-reticle policy | 1 groups | 0 | retained scope | unverified | unverified |
| `contextual-native-actions` | Contextual CQC, pickup, carry, interrogation and extraction routes | 2 groups | 0 | unverified | unverified | unverified |
| `motion-melee` | Physical fist and held-weapon motion contact | 1 groups | 0 | retained scope | unverified | unverified |
| `dog-petting` | Native D-Dog cabin lifecycle and physical petting | auxiliary only | 0 | retained scope | historical partial | unverified |
| `rat-pickup` | Native small-animal pickup, held presentation and release path | none | 0 | unverified | unverified | unverified |
| `horse-riding` | Native horse mount, ride, commands and dismount | 2 groups | 0 | retained scope | unverified | unverified |
| `vehicles-mounted` | Physical wheel steering and native mounted weapon/control contexts | 2 groups | 0 | retained scope | unverified | unverified |
| `powered-arm` | Native powered prosthetic-arm action access | 1 groups | 0 | unverified | unverified | unverified |
| `head-audio` | Head-relative native audio listener | 1 groups | 0 | retained scope | unverified | unverified |
| `ground-zeroes` | Separate Ground Zeroes native stereo experiment | 3 groups | 0 | retained scope | unverified | unverified |
| `controls-native` | Effective personal bindings, native-button mode and gamepad ownership | 4 groups | 0 | retained scope | unverified | unverified |
| `configuration-tools` | External controls/settings editing, validation and recovery | 4 groups | 0 | n/a | n/a | n/a |
| `launcher-fieldkit` | Launcher, field terminal, controller lessons and fitting previews | 7 groups | 0 | n/a | n/a | n/a |
| `owned-asset-import` | Local import of legally owned presentation assets | 1 groups | 0 | n/a | n/a | n/a |
| `install-update` | Install, update, uninstall, backups and fixed-workspace transactions | 3 groups | 0 | n/a | n/a | n/a |
| `performance-pacing` | Native frame pacing, GPU timing and fresh XR submission | 2 groups | 0 | **BLOCKED** | unverified | unverified |
| `regression-tooling` | Bounded native bot, supervision, capture and evidence accounting | 12 groups | 0 | n/a | n/a | n/a |

## Source and community accountability

The catalog links all R01–R55 supplied historical report IDs to an implemented
family or an explicitly excluded request. R49 (stance indicator), R50 (Infinite
Heaven world-feature expansion) and R51 (complete left-handed rig) are excluded
requests, not closed defects. The [community verification ledger](COMMUNITY_VERIFICATION_2026-09-27.json)
and [recovery plan](COMMUNITY_RECOVERY_PLAN.md) retain their dated scope; this
audit is not a fresh GitHub/Discord census.

The earlier RC5 physical Mission 1 binocular lessons and Mission 6 bridge/return
are retained as historical native evidence. The newer fixed-pose binocular
zoom replay does not reaccept acquisition, explicit marking or a complete mission.
D-Dog has an older reviewed continuous native response; the rat path has no
accepted current pickup/release. Optic material insertion and iDroid cone fitting
remain unproven experiments. Complete weapons, items, buddies, vehicles and
Ground Zeroes first-person VR remain open.

Catalog validation checks unique IDs, allowed statuses, existing public paths,
actual CTest names, every current source/header, and every tracked native suite.
New or deleted source/test/fixture references require an explicit mapping change.
Release packaging must reject known `blocked` features. Broader `pending`
coverage and `experimental` features must remain visible in release metadata;
a package must not convert their absence of evidence into a pass.

To update acceptance, repair and reproduce the exact issue, retain the identified
native evidence, inspect the entire relevant transition and update only the
supported scope. Keep newer failures ahead of older passes. The catalog cannot
manufacture visual or headset acceptance from test counts.
