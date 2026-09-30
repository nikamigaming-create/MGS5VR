# Release acceptance checkpoint — 27 September 2026

**Updated 28 September:** [RC3](RELEASE_CANDIDATE_2026-09-28.md) fixes the verified
Mission 1 cinematic handoff. The full intro/return measured zero missing layers
in 17,035 frame cycles. All 34 CTest groups, 7,298 native checks and 76 retail Lua
fixtures pass; normal Mission 1 device Back also passes. The ledger now retains
55 reports / 75 claims, adding the specific regression R05-C4. Wider field,
binocular tutorial, Mission 6 bridge, device clipping and headset acceptance
remain open. The older checkpoints below are historical.

The mod is not ready for an all-issues-fixed release. The recovery ledger contains
55 supplied community reports, 35 situations, 26 recovered feature families and
96 effective control actions. No report has complete current-build acceptance.
The 899 equipment entries are definitions, not 899 tested items. Historical
feature recovery is still an explicit open task.

This checkpoint supersedes older “current candidate” paragraphs in the recovery
and bot documents. Keep their dated evidence; do not interpret old build hashes
as the installed build.

## Post-reboot candidate status

The installed RC2 DLL is `cd91d99e5a892cc216bb444e7706670b14a742083214fe90ef97a33b4375c06b`. The Coco camera, shoulder, body and visual hand-continuity adaptations are integrated. All 34 CTest groups, 7,273 native contract checks and 76 retail Lua fixtures passed. Fresh Meta v207 evidence covers ordinary field Pause and both hands, stationary menu navigation, walk/stop, stance changes, the restored physical binocular optic and normal iDroid Back. Personal INIs are unchanged.

An 800.247 ms induced producer delay had zero missing-layer submissions in its bounded window. The full session nevertheless had seven missing-layer frames during cinematic activation changes, so broad flicker acceptance remains failed. Mission 1 progression, Mission 6 bridge, iDroid clipping and physical headset acceptance remain open. See [candidate notes](RELEASE_CANDIDATE_2026-09-27.md). The older checkpoints below are historical, including their then-unfixed findings.

## Prior native regression checkpoint — candidate 853E

Installed DLL: `853e63eb1758561a44f27673c6cde517585e382d6e0f61a541ce019c620016f0`.
The flicker regression passed a 620.978-second simulator run: 49,974 frame cycles,
zero missing-layer submissions, helicopter Continue/loading, Pause return, and
an explicit 801 ms producer delay. Five selected automated suites passed.
See [the investigation](FLICKER_INVESTIGATION_2026-09-27.md) and the hashed
[community ledger](COMMUNITY_VERIFICATION_2026-09-27.json).

Left prosthetic motion during Pause remains a visible failure. Mission 6's
bridge has not been entered, and Mission 1's binocular tutorial remains open.
Title tape labels overlap and the loading quad needs a fit correction. Quest
Link acceptance has not been run. No complete-report or release pass is claimed.

## Earlier batch: historical evidence

- A local field-kit drill-down joins issues, acceptance requirements, historical
  features, effective controls, runtime settings, code locations and actual
  recordings. Its current controller inset and playback presentation are a
  prototype, not the requested launcher composition. The launcher must keep
  gameplay clean and unobstructed beside interactive 3D controllers, with no
  inset or overlay. Semantic action cues must resolve selected actions against
  current bindings at playback time so rebinding needs no new recording. The
  launcher also needs a 3D cassette presentation and appropriate settings
  widgets. All 96 exported bindings have mapped locations; stick directions are
  mapped as well.
- The runtime numeric inventory scans C++, Lua and MASM under `src` and `include`:
  83 files, 7,380 literals and 46 setting definitions. Each file has a hash; each
  literal has a location and surrounding code. These are review candidates, not
  7,380 user-adjustable settings or 7,380 reviewed values.
- `weapon_hud_setback_cm` exposes the compact weapon readout's elbowward offset,
  default 6 cm, range 0–15 cm. It does not reposition the separate popup.
- The audit found `wrist_surface_lift_cm` was published but ignored. Both native
  forearm paths now consume it. The 2 cm default plus the existing 5 mm clearance
  preserves the previous default geometry. Changed-value runtime fit remains
  to be checked.
- The recorder waits for the first encoded source frame before dispatching a
  recorded case. Its AV merge preserves all video frames and explicitly reports
  any appended audio silence. The field kit independently verifies the encoded
  video's hash, dimensions and duration before offering an excerpt.
- The launcher now checks for an actual MGSV process within 30 seconds. A launch
  request accepted by Steam is no longer reported as a verified process start.
- Ten recorded action lessons are available. The main current-build take,
  `source-showcase-06`, passed ten native cases in 27.4 seconds; its AV file
  preserves all 822 encoded frames. A separate two-case `stance-finish-01` take
  supplies the complete final standing action after a bounded neutral recording
  tail was added. The kit withholds any action missing its first or last frame.
- Fresh Continue proof `polish-continue-02` reached field gameplay, released
  controls, and restored the prior right-grip pose after the cassette handoff.
  Pause-hand motion is now visible in both final eyes at sampled poses 0, 2 and
  4 on `pause-hands-after-02`; SHA-bound reviews are stored beside both pause
  runs. The earlier `pause-hands-after-01` passed native palm telemetry while
  the glove pixels remained frozen, and is preserved as a visual failure.
  These reviews cover hand-mesh motion only. They do not pass the separately
  visible right-eye Pause-panel clipping.
- `polish-idroid-01` observed stationary Snake during native menu-stick input
  with 0 m measured displacement, but normal Back exit timed out in the menu.
  The known held-Back route plus exact visible tutorial-suspension acknowledgment
  recovered gameplay in `supervised-02`; this acknowledgment suspends the
  tutorial and is not tutorial completion. Keep normal map, Help and tutorial
  acceptance separate.
- The existing binocular prototype combines actual right source-eye gameplay,
  synchronized 3D button highlights, an enlarged grip inset and a simple
  instruction. It is an equip demonstration, not proof of the whole binocular
  feature or Mission 1 tutorial, and its inset layout is not the requested clean
  launcher composition. Final-compositor eye captures are linked separately;
  the 3D controller orientation is instructional.
- Fourteen selected CTest groups passed; the latest reported Python suite passed
  134 tests. Browser QA exercises every one of the 96 control references and
  checks the recorded binocular/Pause highlights and the visible R17 failure.

The earlier batch above used candidate `13497AEF40B6D36F407D7EE5F770FE4AD35826412FE8A908DB61F63705D523E5`.
It is historical evidence and does not transfer acceptance to the installed DLL.
The personal INIs remain byte-identical. The immediately preceding C61B build is
recoverable from backup `20260927-132657-5714634a9e594eab84f3cfd69f9df7f8`.
Meta Simulator v207 remains process-scoped; the global OpenXR registry is unchanged.

## Earlier execution order (historical)

1. Finish the native fixes first. Trace the live rejecting modular-arm guard,
   correct paused prosthetic movement, and recheck the flicker paths on Quest
   Link and with the other GPU tests isolated.
2. Finish the tutorial/menu route: current native ownership, controllable hands,
   palm-held iDroid, correct screen fit, safe exit/reopen, and stationary Snake
   while its sticks navigate. Recheck the forced-FOB normal-exit blocker. Keep
   normal map, Help and tutorial cases separate.
3. Complete Mission 1's genuine native BINOCLE state bridge and reproduce Mission
   6's bridge/scene handoff. Record the actual mission transitions without Skip,
   save edits or synthetic progression.
4. Work through the remaining report cases: hands/arms, weapon grips and aiming,
   obstruction, optics/NVG, visibility, pickups/context actions and native HUD.
   Then vehicles/mounts, configuration transitions and compatibility. Every
   supplied report stays in scope; priorities determine order, not silent removal.
5. Check the launcher after the native fixes. Use clean gameplay beside
   interactive 3D controllers, resolve cues through current bindings, and verify
   the cassette UI and settings widgets. For every accepted feature, record a
   complete action and outcome, then generate
   its explanation and controller lesson from that run's effective bindings.
   Reconcile earlier versions and artifacts before declaring the historical
   feature inventory complete. Record the full title-to-field tour from the
   accepted candidate and assemble the final field-kit PDF and film.
6. Freeze one candidate. Repeat regression routes and long-session checks, collect
   physical-headset acceptance, exercise clean install/update/rollback, then
   produce the release archive, hashes and notes from exactly that build.

## Bot responsibility and proof rules

The deterministic runner executes reviewed sequences without a model handoff for
each button. The Luna supervisor handles unexpected states and reports current
scene, attempted action, elapsed wait, failure reason and both-eye captures.
Only one process owns game input. Known startup notices have explicit guards;
unknown tutorial/modal screens stop dependent actions and release controls.

A report needs a reproducible scenario, a source/build identity, a native outcome,
both final eyes and a reviewed motion/interaction result where relevant. A
simulator result does not certify physical headset fit. A native predicate pass
does not erase a visible defect. The Pause panel currently clips the right-eye
frustum at the recorded wrist pose; it remains open despite successful open/close
and visible hand motion. Do not interpret that pose's clipping as proof of a
renderer-math defect, and do not detach the panel from the arm to hide it.

Old raw runs are immutable. SHA-bound sidecars attach reviewed coverage or visual
findings. A newer failed or incomplete take cannot fall back to an older green
result without showing that history explicitly.

## Reproducible artifacts

- Preview server: `python tools/field-guide/serve_release_desk.py`
- Local preview: `http://127.0.0.1:8766/artifacts/field-guide-20260927/release-desk/`
- Generated kit: `artifacts/field-guide-20260927/release-desk/index.html`
- Numeric inventory: `artifacts/field-guide-20260927/numeric-audit.json`
- Browser inspection: `artifacts/field-guide-20260927/release-desk-qa/qa.json`
- Exported example: `artifacts/field-guide-20260927/lessons/binoculars-current-04/lesson.mp4`
- Current main run: `artifacts/bot-20260927/source-showcase-06/result.json`
- Complete final posture take: `artifacts/bot-20260927/stance-finish-01/result.json`
- Continue handoff/pose restoration: `artifacts/bot-20260927/polish-continue-02/result.json`
- iDroid stationary-navigation and blocked normal exit: `artifacts/bot-20260927/polish-idroid-01/result.json`
- Paused-hand frozen-pixel review: `artifacts/bot-20260927/pause-hands-after-01/visual-review.json`
- Paused-hand visible-motion review: `artifacts/bot-20260927/pause-hands-after-02/visual-review.json`
- Existing PDF: `output/pdf/MGS5VR-Field-Kit.pdf` — verification edition, not a
  completed all-feature manual.

No public patch or full-feature film is certified or published by this checkpoint.

## Embedded launcher update

The launcher steering is now implemented in `MGS5VR-Launcher.exe` and its embedded
field-terminal host. Ten clean action lessons sit beside interactive controllers;
the controllers turn toward the relevant face, grip or trigger during lessons.
Eight game-mode cards expose current mappings, contextual 3D picking, stick axes
and links to remapping/available lessons. All 96 actions and 46 tuning definitions
come from the native checker; settings use 39 sliders, five switches and two
enumerated choices, plus exact values. Runtime paths use text inputs.

Browser/real-bridge QA verified remapping binoculars Y to X, saving and replaying
the unchanged source video with X highlighted. The native WebView2 host loaded the
installed settings and rendered lessons/controllers/settings without a browser
tab or development server. Eighteen settings-bridge assertions and eight selected
launcher CTests passed. See `docs/LAUNCHER_3D_VERIFICATION.md` and
`artifacts/launcher-3d-qa/`. These checks do not promote unresolved native reports
or the ten historical recordings to full release acceptance.
