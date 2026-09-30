# Community recovery plan

For the latest installed build, field-kit drill-down and execution order, see the
[27 September release acceptance checkpoint](RELEASE_ACCEPTANCE_2026-09-27.md).
The dated implementation evidence below is preserved as history.

Updated 2026-09-26. This is the current engineering plan for the supplied community
reports. It supersedes the **planning priorities**, but preserves the historical
results, in [September 20 triage](COMMUNITY_ISSUES_2026-09-20.md).
Status: **initial defect repairs implemented; runtime acceptance still open**.
The game was closed by the user after lag; game input and media rendering are
stopped. Six field-control cases passed in a saved 16.7-second take, and iDroid
menu navigation held native player position fixed. An additional loading-gate
repair builds and passes 7,209 core checks, but is not installed/live-verified.
The maintained bot has 76 passing Python contracts; controlled walking, prone,
remaining community reports and the full instructional film remain open. See
the current checkpoint below for exact build identities and limitations.
The current candidate removes the pinned cutscene camera, restores cutscene Pause
navigation and the hospital's two native look lessons, adds thumb-rest bindings,
and corrects the controls editor's validation feedback. Camera/core/community
regressions and isolated Lua fixtures pass; this is not a physical-headset result.
The [implementation checkpoint](BOT_AND_SHOWCASE_PLAN.md#implementation-checkpoint-september-26)
records the evidence and current failures. No physical-headset test or complete
community-report closure is claimed.

## First repair candidate — September 26

This section records the earlier package, not the newest offline build.

Local package: `dist/MGS5VR-community-20260926.zip`, with `CANDIDATE_NOTES.md`
and a file-hash manifest inside. Built DLL SHA-256:
`5CA9E7332E9565271FACFB4266655EDCCCD4D488C7488B89553B9D6634AE68CF`.
This identifies an uncommitted candidate based on `932d786`, not a new tagged
release. The candidate was installed through the package updater after preserving
the previous DLL, checker, settings and install record. Installed DLL/checker
hashes match the candidate; both personal INIs remain byte-identical.

Implemented: current authored cutscene cameras (R01), cutscene Pause input
precedence (part of R04), the two mission-10010 look lessons, bindable thumb-rest
touches (R21), and accurate editor pending/error/recovery feedback (R20).
No downloaded full-file replacement was used. The camera repair adopts the
contribution's current-shot intent without its unrelated offset/posture changes.

Validation: all eight selected CTest suites pass, including 7,200 core checks,
281 controls checks, 28 bot tests, camera observer, community regressions, binding
export and the hidden editor lifecycle. The isolated Lua 5.1 fixtures pass 46
assertions. Installer/update/rollback fixtures pass against the staged package.
These checks do not establish final-eye, first-playthrough or physical-headset
acceptance. Logs are retained locally in `artifacts/community-fixes-20260926/`.

Next defect work: join the VR binoculars to the native event/status required by
Mission 1 (R02), then resolve tutorial input reachability and Mission 6's scene
transition from native evidence. The private-desktop launch attempt did not
produce a working game session. The normal Steam/simulator session now reaches
outdoor gameplay in 18.359 seconds without changing foreground focus. A fresh
configured A press changed native standing to crouching, with final-eye footage.
The initial capture runs at 3.02 FPS and is diagnostic material; capture quality,
continued gameplay coverage and first-playthrough acceptance remain open. The
runner's missed Press Start transition and a combined Continue-to-suite command
are covered by eight added regressions (36 bot tests now pass; the package-time
count above remains historical).
The user confirmed closing the game and subsequently the simulator; stopped
camera/render counters in that interval do not establish a new game defect.
A bot recovery nevertheless mistook awaiting-player for VR-off and toggled VR
off; its recovery guard is now corrected. The user subsequently authorized normal
game/simulator windows for recording; computer-use, focus manipulation and OS
input remain prohibited. The current runner has 56 passing contract tests.
The continuous field suite observed five passes, and a corrected held-Commands
case subsequently passed. The user-left-open tutorial was closed through its
visible A prompt, returning to freeplay without moving Snake. See the latest
checkpoint for capture quality, precise action limits and the current wrist/
iDroid ergonomic work. These are not complete gameplay or headset acceptance.
Bot coverage, all remaining report rows and the finished showcase remain open.

## Decision and scope

The full deliverable is the corrected mod **plus a sustained gameplay bot,
verified controls for every supported context, and the finished instructional
showcase**. The [bot and showcase specification](BOT_AND_SHOWCASE_PLAN.md) records
the audited tooling, missing A*/state/action infrastructure, coverage rules,
continuous operation requirements and B0-B7 milestones. This is part of the same
program, not an optional final editing task.

The [native context coverage inventory](NATIVE_CONTEXT_COVERAGE.md) is the initial
machine-readable bridge from this plan to per-mode bot cases and teaching clips.
Its 35 obligations are an explicit incomplete denominator, with missing native
identity and outcomes retained rather than inferred from the eight input modes.

Repair the shared decisions that connect native game state, VR presentation,
player actions and visible feedback. Organize implementation by those decisions
and the failing gameplay routes. The six contributed patches are reference
implementations and experimental evidence to evaluate within that work.

Start from current source `932d7863b7a808e28cf7d6734d3a2ab79f6a67f0`
(`experimental-2026-09-24`). Establish one identified candidate while investigating
the reported failures. Tester build identification runs alongside that work; it
is required to attribute regressions and close hardware reports, not to begin
source investigation or fix demonstrated source defects.

The next stability candidate prioritizes progression, visible and escapable menus,
reliable combat, and camera/tracking continuity. Every other supplied defect stays
in the ledger with an owner area and acceptance case. Feature requests have an
explicit later stage. Shipping a bounded candidate does not mean this whole plan
is complete.

Scope is the supplied Discord issue log, supplied follow-up text, contribution
bundle, repository report history, and the user's added bot/complete-controls/video
requirements. This review is not a fresh census of live
Discord/GitHub. Images represented only by `Image`, uninspected linked media,
and an unspecified "my issue persists" do not establish additional diagnoses.

## Evidence and baseline

| Source | What it establishes | Limit |
| --- | --- | --- |
| `mgs5vr_discord_issue_log_24_09_2026.txt` (`D`) | Distinct complaints and requests; explicitly covers builds through September 20. | Aggregate reports, no per-report DLL identity or complete reproductions. |
| Supplied `Pasted text.txt` (`F`) | September 24 posts, subsequent `Yesterday` posts, and a final `1:48 AM` marker follow-up. | Preserve relative timestamps. The checkpoint dates the main follow-ups to September 25; the last post has no absolute date. "New build" is not a hash. |
| `PATCH NOTES.txt`, six patches and three full-file copies (`C`) | Proposed stale-demo recovery, cinematic following, tracking cache, lower-body adjustment, visibility recovery and fit changes. | Author reports improvement; application/round-trip claims prove file reproduction, not current gameplay acceptance. Author identity/test environment are not established by the supplied files. |
| [Community triage](COMMUNITY_ISSUES_2026-09-20.md), [test notes](COMMUNITY_TEST_2026-09-20.md), [Quest feedback](QUEST3_TESTER_FEEDBACK.md), [fork review](FORK_REVIEW_2026-09-20.md) (`H`) | Earlier symptoms, accepted source work, bounded SIM observations and retained limitations. | Historical evidence; statuses and GitHub issue states have not been revalidated here. |
| Current checkout (`S`) | Exact implementations and existing test expectations described below. | Source mechanisms do not prove the causes of a tester's session. |

All six patch preimage hashes match the September 20 tag
`14849f11d901c3a86c2cf8c51edd6327ec421d87`; this was rechecked. Current source
is the September 24 tag above, although its commit timestamp is September 20.
Release names, commit timestamps and installed DLL identities are distinct.
The incoming checkpoint records five focused patches passing a path-corrected
apply check and the controller patch failing. No patch was applied in this review.

Unmodified inputs and SHA-256 hashes are retained locally in the ignored directory
`private/community-review-20260926/`, with `manifest.json`. This includes the older
`MGS5VR_Complete_Fix_Plan_2026-09-22.md`, which targets the old commit and is
historical analysis, not the current implementation specification. Raw chats and
personal runtime evidence stay private; this document is the portable summary.

Primary text fingerprints:

- D: `5B39FA028095D14057A9BCDFA3F49CE3F10ED185E37CC9E8A109026AD2D503C2`
- F: `E8225BD26591227961842B27DB29669699FDDFA7631B2BD316CFF215F48020EF`
- C notes: `E213BAB1A10C4039AD62B6343F26C735A19F7FB000E3FB47A9A3E61A7689E199`

## What the source explains

### 1. Scene classification controls several unrelated decisions

The [native classifier](../src/native_presentation.lua) uses a `Seq_Demo_`
sequence or `DemoDaemon.IsDemoPlaying()` for ordinary cinematics. Inspection of
the owned prologue script showed that `s10010_sequence.IsDemoPlaying` requires a
demo-name list; the previous no-argument fallback always returned false. It was
removed. It was not evidence of stale demo flags in other missions.

The current candidate distinguishes the exact mission-10010 sequences
`Seq_Game_FewDaysLater0` and `Seq_Game_FewDaysLater1`. Their native script reads
`PlayerVars.rightStickX/Y`, so they retain authored camera presentation and
right-stick look without enabling locomotion or combat. Ordinary cinematic input
still suppresses sticks/triggers. An open native menu takes precedence and keeps
its navigation. The native game still owns each lesson's input masks and progress.
This is a bounded repair, not a complete inventory of interactive cinematics or a
new unknown/query-failed classification policy.

The baseline [head camera](../src/head_camera.cpp) deliberately held the first
scripted pose across animation and camera-object cuts. The candidate removes that
pin: physical head motion is relative to the current accepted authored shot.
Regression checks cover translation/rotation, tracking recovery, camera handoff,
paused-menu anchoring and return to a fresh player publication. Camera ownership,
gameplay offsets and posture smoothing are preserved. Full-sequence visibility
and comfort still require runtime review; the proposed screen mode below remains
future work.

The owned Mission 1 script supplies a more specific R02 mechanism: its binocular
lesson advances on `Player.OnBinocularsMode`, and later steps require
`PlayerStatus.BINOCLE`. The custom VR optic does not establish that native state.
Its visible activation therefore cannot by itself complete the lesson. Repair
requires a native binocular enter/exit and progression path joined to the VR
presentation, not just reclassifying the scene. Mission 6 remains unreproduced.

The September 26 field assay adds a useful limit: native-button mode forwarded
right shoulder (`512`) through the game's XInput poll for about 1.4 seconds,
but a concurrent read-only trace never observed `PlayerStatus.BINOCLE` during
that hold. This was mission 30010 free play, not the Mission 1 lesson. The first
attempt observed only after release and a mode toggle, so it could not establish
entry; the corrected attempt sampled throughout the hold while retaining native
input mode. The missing outcome is downstream of XR delivery. Current retail
control layout, action masks and native-camera interaction must be resolved
before choosing the bridge implementation. Konami's
[PC control manual](https://mgstpp-app.konamionline.com/manual/pc/eu/en/pc_03.html)
specifies holding the binocular command and distinguishes Action/Shooter layouts;
an expected latched state after release would be the wrong test.

### 2. The rendered body is not the native gameplay body

The [controller rig](../src/controller_rig.cpp) repositions the upper-body skin
and solves tracked arms. `PoseRestore` restores original joint arrays after native
skin processing. `shot()` separately supplies a rendered-muzzle target for
supported firearm shots. These are scoped presentation/shot adaptations, not
evidence that actor heading, placement logic or pre-fire obstruction queries use
the same frame.

The contributed stationary pelvis adjustment also changes skin joints. It does
not demonstrate a native facing or collision correction. Therefore C4 placed
behind the user, a rock behind the user blocking fire, visible torso/leg mismatch,
and camera drift should be investigated together through native/action/rendered
transforms. They still need separate reproductions: ordinary obstruction and a
stuck input context can produce superficially similar firing failures.

### 3. Validity, visibility and attachment are separate

Current rig code already keeps publishing the native skin/camera when the right
controller is untracked, while clearing tracked weapon/optic state. Its support
solver already has compact-weapon handling, contact retention and weapon-change
resets. The small-gun report persists despite those mechanisms; replacing them
with an older implementation would discard useful work without identifying the
remaining failure.

[Player visibility](../src/player_visibility.cpp) already suppresses one verified
native proximity-fade path. A disappearing arm can still originate in tracking,
skin publication, draw membership, clipping or another fade path. A body-restore
watchdog cannot establish a fix for a black compositor/world image.

The contribution's hand cache has no age expiry and is used beyond visual
continuity, including shot eligibility. Its 250 ms body watchdog treats a missed
publication as a reason to restore hidden groups. Both ideas require explicit
state/owner/age rules, especially through pause, genuine cutscenes and tracking
loss. A remembered pose must not become indefinitely valid tracking.

### 4. Several reports concern settings or presentation, with conflicting outcomes

The [HUD policy](../include/mgs5vr/hud.hpp) intentionally shows world recon in
`full`, restricts it to an aligned binocular pass in `binoculars_only`, and hides
it in `off`. Follow-up reports say `full` stabilizes highlights but makes icons
too large. Track policy/discoverability, intermittent loss and marker size as
different questions; changing every default to `full` would not resolve all three.

Menus are projected onto 16:9 panels in [UI rendering](../src/ui_renderer.cpp),
while [canvas correction](../src/stereo.cpp) derives an aspect correction from
native and eye projections. The desktop-window-dependent stretch report needs a
matched aspect-ratio test of that entire mapping. Width/depth sliders alone do
not prove correct text proportions or clipping.

Hand pitch, player height, scope eye relief, binocular distance, support distance
and smoothing already have consumers in current source. The G2 and Quest users'
different successful settings are useful calibration evidence, not universal
replacement defaults. Preserve those controls and test their interactions.

### 5. Controls reachability and editor behavior need distinct fixes

Thumb-rest touch still feeds finger animation and is now separately published as
`left_thumbrest` and `right_thumbrest` in [OpenXR input](../src/xr_runtime.cpp)
for profiles exposing the path. The tokens are available to bindings and the
compiled binding export. Runtime activity and headset behavior still need
profile-aware acceptance.

`menus.dpad_*` actions exist and are consumed by native menu routing, but all four
default to `disabled`. An iDroid lesson requiring native D-pad input needs a
reachable binding and correct tutorial context. The existing held-Back recovery
is an escape route; it does not prove tutorial completion.

[The text controls editor](../tools/edit-controls.ps1) now replaces old VALID
feedback immediately with CHECKING after an edit, explains invalid/checker-error
states, and enables Save for a valid dirty draft. Its new hidden
[lifecycle test](../tests/edit_controls_lifecycle_tests.ps1) exercises the actual
timer and handlers through edit/save, invalid input, correction, backups and an
external-edit conflict. The original tester's attempted edit is still unknown;
this fixes misleading feedback and establishes recovery, not the cause of every
possible validation rejection. Invalid layouts remain unsaveable.

Current gamepad ownership preserves native XInput and switches the rig to native
animated hands. That does not establish correct Xbox aim/binocular presentation.
The pad-only report needs its own end-to-end presentation policy and tests.

## Implementation contracts and ownership

These are intended changes and acceptance requirements, not newly available
settings or verified behavior. Reuse the existing frame, input, native-action
and rendering infrastructure; extract a small shared decision only where the
failing case needs it. Avoid an unrelated engine rewrite.

| Workstream | Owning code area | Required result |
| --- | --- | --- |
| W0: build identity and reproducible cases | Package/launcher/logging; existing scenario and capture tools | One source revision, package/DLL/checker identity, effective settings and recorded reproduction for each claim. |
| W1: scene, input and camera transitions | Native presentation Lua/pump, XR routing, head camera, menu routing/rendering | Distinguish native scene facts, permitted interactions and chosen display mode. Camera, input, menu and visibility consume a coherent decision with a generation/reason. |
| W2: avatar and native action alignment | Native movement/action adapters, controller rig, head origin, shot/throw/placement paths | Native intended facing/target, visible body and accepted action agree for the action at issue. Correct the first divergent transform or native gate. |
| W3: tracked rig, visibility and optics | Controller/arm/optic rigs, player visibility, final frame publication | Independent head/hand validity, bounded visual recovery, stable support contact and shared final hand/weapon/lens poses. |
| W4: readable menus, HUD and feedback | UI canvas/layer routing, menu surfaces, optic markers/recon | Correct proportions, useful scale, reachable menus, truthful native feedback and documented view roles. |
| W5: usable controls and configuration | Controls schema/parser, XR actions, native input, editor/launcher, docs | Supported actions are reachable in their context; valid edits persist and apply; failures explain how to recover. Gamepad input and presentation agree. |
| W6: mounted gameplay | Travel mode, native camera/look/actions, vehicle-specific view and UI | Each named vehicle/emplacement supports entry, visibility, both aiming axes, its reticle/range control, firing and exit. |
| W7: rendering/performance and installation | Stereo/render/capture, pacing, package/installer | Resolve measured visual/performance faults on identified setups; preserve settings, saves and foreign mods through recovery. |
| W8: feature/compatibility extensions | Feature-specific owner after W0-W7 stability | Explicitly scoped new capabilities, each with its own acceptance and supported configurations. |

W1 should retain separate facts for title/loading, an authored shot, an active
interactive sequence, menu state, native input owner and camera/player publication
validity. Unknown or expired evidence must remain distinguishable from confirmed
gameplay. A fresh player skeleton alone does not prove a demo ended. Menus need
priority for visible navigation even when a cinematic is paused. Require neutral
input on ownership/context changes so a held action cannot leak into play.

Proposed cinematic default: show the **live native cinematic on a stable,
head-tracked screen**, with visible Pause/Back/allowed Skip. Return to tracked
gameplay when the native interaction state allows it. Mission 1's interactive
lesson must retain the actual required actions; it cannot be classified as a
noninteractive movie. Evaluate an immersive following-camera option separately
for comfort. Never preserve the first shot as if it showed the entire movie, or
make skip/reload a requirement to progress on a first playthrough.

W2 must declare the intended on-foot body/action heading and how physical turns,
stick turns and roomscale affect it while retaining independent looking/aiming.
Use verified native movement/action paths; merely rotating the visible pelvis
does not change gameplay intent. C4/mine preview, accepted placement and final
object position must agree. Trace firing from physical input through mapped
native input, ready state, obstruction decision, shot invocation and muzzle.
Keep legitimate wall/cover rejection and enemy detection semantics. Prone roll,
CQC, mounts, vehicles and scripted animation keep their necessary native control.

W3 may retain a recent pose briefly for presentation only, with a measured
time limit, explicit invalid action state and resets on tracking-space epoch,
owner, model, weapon and scene changes. Use elapsed time, not frame-count blends.
Do not let hand loss invalidate valid HMD/world publication. Visibility changes
must restore only owned state on a verified safe thread/lifecycle; a mutex and
timer alone are not a complete design. Fit offsets are evaluated after transform
ownership is correct and against posture/mount transitions.

## Complete report ledger

P0 blocks the stability candidate's core play/progression route. P1 is remaining
reported functionality/usability work. P2 is polish, performance or compatibility
unless reproduction reveals a blocker. Feature means an explicit later extension.
These are planning priorities, not assertions of incidence. Every row remains
open; "improved" means a supplied tester or historical note reported improvement.
Each row can be split into independently closable cases without losing its ID.

| ID | Report and source | Priority / owner | Current assessment and required closure case |
| --- | --- | --- | --- |
| R01 | Stuck/distant cutscenes; OKB Zero helipad; prologue/jeep presentation (D/F/H) | P0 W1 | First-shot pin removed; current authored camera, head motion, cuts, Pause and return-to-player regressions pass. Complete multi-shot sequences and comfort still need runtime review. |
| R02 | Mission 1, Ocelot binocular lesson cannot continue (D) | P0 W1/W5 | Native script requires OnBinocularsMode/BINOCLE, which the custom VR optic does not establish. Implement the native state bridge, then complete on first playthrough without Skip or progression edits. |
| R03 | Mission 6 bridge: displaced view, changed controls, cannot move (D) | P0 W1 | Capture before/after voice/script trigger and checkpoint reload; native controls and correct player camera survive. |
| R04 | Pause navigates invisibly during cutscenes (D) | P0 W1/W4 | Open menu now takes input precedence over cinematic stick/trigger suppression. Both-eye menu visibility, return and allowed skip through cuts remain runtime acceptance cases. |
| R05 | Black/third-person view near HMD, below waist, after idle/system menu; removal/resume crash (H/C) | P0 W1/W3 | Prior fixes and contributor claims are partial evidence. Separate hand occlusion, HMD loss, focus/session loss and missing stereo publication. |
| R06 | Changing game options drops VR until checkpoint reload (D) | P0 W1/W7 | Change options/resolution and resume without losing chosen presentation or camera owner. |
| R07 | C4 follows Snake's facing instead of player; mine/placement feedback (F) | P0 W2/W4 | Native placement authority unverified. Physical/stick turn, recenter, tank placement, crouch/prone: preview and placed object agree. |
| R08 | Clear-looking aim blocked near cover/prone; UI sound; rock behind player blocks shooting (D/F/H) | P0 W2/W5 | Distinguish input rejection from native obstruction. Clear muzzle fires without stance reset; genuinely obstructed muzzle still blocks. |
| R09 | Upper/lower body split, shadow mismatch, horse drift, box/toilet view after CQC/mounts/long play (D/F/H) | P1 W2/W3 | Compare native root, displayed pelvis/torso and HMD across repeated transitions and a sustained session. |
| R10 | Hands/weapons/items fade near surfaces; arms disappear near ground/prone (D/H) | P1 W3 | One fade hook exists. Identify first missing stage; verify both-eye visibility on slopes, walls and affected costume. |
| R11 | Left hand jumps/detaches on pistol/revolver/Uzi, especially left roll (D/F) | P1 W3 | Current compact support logic is insufficient evidence. Sweep roll/close contacts, reload, release, reacquire and change weapons. |
| R12 | Short arms, restricted reach, exposed/deformed sleeves (H/C) | P1 W3 | Measure shoulder/wrist/reach across users and costumes. No universal shoulder-offset fix is established. |
| R13 | Low viewpoint, walking bob/sway, nausea; requested better height (D/F/H/C) | P1 W2/W3 | Height setting exists and one user likes +20 cm. Check calibration, walk/run, stance/mount response and no accumulated offset. |
| R14 | One-hand pitch; Quest -20 and G2 -16 fit reports; weapon-only pitch request (F/H) | P1 W3/W5 | Aim correction exists. Verify controller/grip/aim, hand contact, sights and shot remain coherent; retain personal fit. |
| R15 | Binocular/scope narrow eye range and intermittent lens disappearance; 15/16 cm boundary; scope jitter/smoothing request (D/F/H) | P1 W3 | New settings helped. Test physical tracking-safe distances, threshold hysteresis, smoothing/latency, both eyes, scopes and surrounding geometry. |
| R16 | Weapon seems to switch near the face (F) | P1 W3/W5 | Cause unknown: trace physical/chord/context/equipment events before attributing to optics distance. |
| R17 | Pause at hip/bottom-left, unreachable; frozen arm while paused (D/H) | P1 W1/W4 | Default quad helps some reports. Pause for >5 s; close/reopen; separately test opt-in handheld behavior and tracking. |
| R18 | iDroid wide/squashed/tiny and desktop-aspect dependent; requests position/rotation/size (D/F) | P1 W4 | Partial tuning exists. Compare native canvas at 16:9, 16:10, ultrawide and non-16:9 custom render sizes; pan/zoom/tabs/clipping. |
| R19 | iDroid tutorial needs D-pad; player stuck (F/H) | P0 W1/W5 | Default menu D-pad actions are disabled. Complete requested tutorial inputs; independently verify safe escape/reopen. |
| R20 | Controls editor Save becomes unavailable after remapping (F) | P1 W5 | Pending/invalid/error feedback repaired; hidden real-handler test passes valid save, rejected edit, correction, backups and external-edit preservation. Original attempted edit/checker identity still unknown. |
| R21 | Thumb-rest mapping missing (F) | P1 W5 | Source plumbing now exposes separate left/right Touch-profile thumb-rest tokens through runtime sampling, parsing and binding export. Physical headset/profile acceptance remains open. |
| R22 | Missing/conflicting controls; binocular stance differs; unclear controls/manual (D/H) | P1 W5 | Inventory effective actions by context/profile; test transitions, holds/chords, remapped controls and neutral live reload. |
| R23 | Cannot toggle suppressor/flashlight in quick menu (D/H) | P1 W5 | Identify native action and provide usable conflict-free route with visible native state change. |
| R24 | Carry/drop bodies, plants/diamonds/context actions and interrogation unclear/unusable (F/H) | P1 W4/W5 | Instructions conflict across layouts. Test the active config's native hold/context actions and visible prompts; verify interrogation intel. |
| R25 | Missing/intermittent world markers/highlights; unclear `full` (D/F/H) | P1 W4 | Mode-dependent visibility is intentional in source. Test mark/unmark and stow in each mode; trace any loss inconsistent with policy. |
| R26 | Markers obscure enemy in `full` (F/H) | P1 W4 | Separate scale defect from R25. Bound apparent size near/far, world/binocular views and crowded scenes. |
| R27 | Misaligned gun reticle; keep throw/placement reticle; laser too thin (D/F) | P1 W2/W4 | Preserve useful aim feedback per action. Turning off all native reticles is not an acceptable workaround for placement. |
| R28 | Missing ammo/flower/diamond icons, captions/prompts/notifications; blank loadout or detached HUD (D/H) | P1 W4 | Catalog missing native layers individually; prove each relevant pickup/message/loadout event in both eyes. |
| R29 | No soldier stats through binoculars (D/H) | P1 W4/W5 | Separate native analysis/progression availability from suppressed UI and from ordinary marking. |
| R30 | NVG mesh blocks view (D) | P1 W3/W4 | Hide only the offending first-person mesh where appropriate; retain NVG effect and equipment state. |
| R31 | Tank/APC view fully obstructed (D) | P1 W6 | Choose vehicle-specific view/mesh treatment; test entry, motion, aiming, firing and exit without a global height hack. |
| R32 | Mortar fires but cannot aim (D/H) | P1 W6 | Verify native targeting/range axes and reticle, with visible impact at intended target. |
| R33 | Machine-gun turret lacks reticle/elevation (D/H) | P1 W6 | Both aim axes, firing, usable view and reticle, then exit. |
| R34 | Helicopter/Pequod turret inaccessible (D/H) | P1 W5/W6 | Verify native access/context action, traverse, elevation, fire and return. |
| R35 | Combat-vehicle reticle absent; ZHUK RS-ZO range cannot change (D) | P1 W6/W4 | Test this vehicle separately from ordinary tank/Jeep steering, including range and rocket impact. |
| R36 | Cabin/cockpit bindings mixed; Continue selection/hidden-menu focus (D/H) | P1 W1/W5 | Newer cabin/iDroid feedback is positive. Preserve cold launch, physical Continue, Resume and cabin controls without hidden menu movement. |
| R37 | Vehicle auto-center on entry; view/steering/horse direction disagreement (D/H) | P1 W2/W6 | Declare native mounted steering policy; no forced HMD turn or inherited on-foot snap offset on entry/exit. |
| R38 | Xbox LT + shoot freezes into 2D presentation (F) | P1 W1/W5 | Trace native aim/menu flags and owner; gamepad-only aiming/firing keeps live head tracking and intended display. |
| R39 | Xbox binocular view floats at lower left (F) | P1 W4/W5 | Provide centered native optic presentation when pad owns input; no requirement for tracked controllers. |
| R40 | Resolution/sharpness, >4K startup, headset vs mirror FPS, optic/directional lag (F/H) | P2 W7 | Match runtime/resolution/refresh/route; measure fresh submitted frames and latency. Prior config/SIM results do not establish headset performance. |
| R41 | Horizon artifacts/tree pop-in (D/H) | P2 W7 | Matched left/right motion capture; isolate culling/projection from resolution and frame cadence. |
| R42 | Rectangular light/shadow mismatch; displaced flares/outline tails (D/H) | P2 W7 | Separate layers with same-frame stereo/projection evidence; do not group all artifacts as a pacing bug. |
| R43 | Infinite Heaven/IHHook/SnakeBite coexistence (D/H) | P2 W7/W8 | Versioned loader strategy and reversible install tests; preserve foreign loader. No claim of general mod compatibility. |
| R44 | DLL/INI swaps start flat; folder/update/removal confusion (H) | P1 W0/W7 | Show active game/config/build; complete-package install/update/rollback preserve personal settings, saves and unrelated files. |
| R45 | ReShade/DLSS/other SnakeBite or graphics mods (H) | P2 W7/W8 | Separate versioned combinations; do not infer support from one user's successful mod. |
| R46 | Snap turn/angle/faster turning requested; recenter tilt history (D/H) | P1 W2/W5 | Existing settings/source, not a missing implementation claim. Verify native smooth vs snap, angle, neutral latch and leaned pivot. |
| R47 | Physical stand/crouch/prone (D/H) | Feature W8 | Explicit opt-in stance policy after height/native posture work; no double crouch or broken prone roll. |
| R48 | NVG hand-to-head toggle (D) | Feature W8 | Deliberate gesture with hysteresis and no accidental equipment switch; R30 is a separate defect. |
| R49 | Visible stance indicator (D/H) | Feature W4/W8 | Native stance-derived, readable, consistent through scripted/mounted states. |
| R50 | Enemy senses, subsistence, time/weather features from IH (D) | Feature W8 | Separate scope from loader coexistence and core VR recovery; preserve progression semantics. |
| R51 | True left-handed rig (H: issue #8 at earlier review) | Feature W8 | Mirrored grip/weapon/interaction ownership and complete controls; not merely swapped button labels. |
| R52 | Ground Zeroes first-person completion (H: issue #7 at earlier review) | Feature W8 | Separate camera/player/rig/HUD adapters and game-specific acceptance; TPP success does not close it. |
| R53 | Quiet/rat/buddy expansion and universal mod installer (H) | Feature W8 | Separate native interaction/compatibility work. Retain existing working buddy behavior during regression checks. |
| R54 | In-headset configuration instead of external editing (H) | Feature W8 | External editor already exists; an in-game editor needs safe input ownership, schema, persistence and readable UI. |
| R55 | Roomscale/body movement and rotation follow the player (D/H) | Feature W2/W8 | R07/R09 action-heading correctness comes first. Broader body-following needs native collision-aware movement, independent looking, stance/mount rules and observed arrival. |

Positive follow-ups remain regression requirements: improved binocular reach and
scope eye relief, usable central iDroid, corrected cabin controls, height/hand
tuning, marker visibility with `full`, interrogation, native equipment preview,
smooth turning and limited driving/riding. Success for one setup never silently
closes a conflicting report on another. R20, R38/R39 and R25/R26 are distinct
from the older bugs despite partial related work in current source.

## How the community contribution is used

Evaluate its behavior against the owning workstream and failing case. Preserve
the original bundle and author attribution for reused ideas/code. Record
`adopted`, `adapted`, `superseded by current implementation`, or `deferred with
reason` for each idea as implementation proceeds. Do not claim authorship or
hardware coverage that the supplied material does not establish.

| Contributed idea | Decision for implementation |
| --- | --- |
| Shared stale-demo override and diagnostics | Useful W1 hypothesis and trace points. Rework around explicit interaction/camera evidence and coherent transitions; two seconds of matching head publication is not sufficient proof of gameplay by itself. |
| Follow native cinematic camera | Evidence that frozen-shot presentation needs reconsideration. Test separately from stale-state recovery; compare live-screen default and optional immersive following. Smoothing does not by itself establish comfort. |
| Lock horizontal head offset; larger smoothing/deadband | Candidate W2/W3 comfort experiment. Compare walking against crouch/prone, climbing, mounts and discontinuities; keep one stabilization owner, with explicit reset rules. |
| Higher/backward camera and changed shoulders | Calibration candidates after W2/W3 correctness. Preserve user offsets; measure reach, sleeves, body, weapons and scale before changing defaults. |
| Cache untracked hands | Preserve the visual-continuity goal, redesign validity/expiry and action eligibility. Reject indefinite `effectiveTracked` reuse and frame-count-dependent recovery. |
| Stationary pelvis realignment | Optional W2/W3 visual treatment after native/action-heading correctness. It does not close R07/R08. |
| Visibility watchdog and locking | Investigate W1/W3 restoration ownership, threading and pause/transition behavior. A blind 250 ms restore is insufficient, especially if native publication pauses legitimately. |

The supplied full files predate newer menu, `nativeIdroidClosing()`, height and
gamepad-ownership work. Do not install them wholesale. Reconcile focused behavior
with current symbols and tests; omit unrelated formatting rewrites. The previous
[fork review](FORK_REVIEW_2026-09-20.md) remains relevant: bounded adaptive pacing
already exists, the unsynchronized D3D11 worker is not accepted, and an additional
head filter must not be layered over the current stabilizer without comparison.

## Execution order and exit gates

Execute this sequence together with [B0-B7](BOT_AND_SHOWCASE_PLAN.md): B0/B1
session observation and reliable startup begin with W0/W1; B2 navigation and B3
contextual actions turn reproductions into repeatable tests; B4 proves one
uninterrupted route before B5 scales the suite. B6 produces synchronized teaching
clips during implementation. B7 delivers the guide and complete final film on the
accepted candidate. A bot command completing is never a substitute for the
feature-specific gates below.

No dates are promised before reproductions establish the native work required.
Keep changes reviewable by behavior and gate; a cross-file scene fix is one
coherent change, while an unrelated camera-offset experiment is separate.

| Stage | Concrete work | Exit gate |
| --- | --- | --- |
| 0. Identify and reproduce | W0: build current revision, record identities, verify existing suites, add bounded transition/action traces where existing logs are insufficient. Create failing cases for R01-R04, R07/R08 and R19; capture R20's actual validation result. | A known baseline and at least one observed failing transition and action case, or a specifically recorded missing scene/hardware requirement. Independent deterministic fixes continue. |
| 1. Restore progression and recovery | W1 plus blocking W5/W4 pieces: interactive sequence routing, cinematic display, visible Pause, tutorial D-pad, options/loading return and neutral input transitions. Editor/diagnostic usability can proceed independently once its baseline is known. | Mission 1 first-playthrough lesson, Mission 6 bridge, multi-shot cutscene/Pause and tutorial completion/escape pass on the candidate. No blind navigation or required checkpoint workaround. |
| 2. Make body, combat and tracking agree | W2/W3: identify native facing/placement/obstruction owners; repair alignment and firing recovery; bounded hand validity, support contact and visibility. | C4/placement and clear-vs-obstructed firing agree; compact support/occlusion/posture/costume tests pass; drift/comfort claims have the required motion evidence. |
| 3. Complete readable interactions | Remaining W4/W5/W3: aspect/scale, markers, missing feedback, profile fit/optic thresholds, thumb-rest/config/editor reachability and attachments. | Each affected UI/action row has a visible usable result with tested mode/config; personal settings survive. |
| 4. Cover native special modes | W6 and pad presentation: named turrets/vehicles and Xbox aim/binoculars, with explicit ownership handoff. | Each advertised mode completes its own access/view/aim/fire/exit route. Xbox ownership tests alone are insufficient. |
| 5. Validate and package | W7 targeted rendering/performance/installation work; regression route and physical affected-device checks on one final artifact. | Candidate gate below passes; supported modes and every remaining P1/P2 limitation are explicit. |
| 6. Extend deliberately | W8 requests and versioned compatibility projects. | Feature-specific acceptance, without reopening working core paths. |

Dependency: W0 enables comparison everywhere; W1 establishes reliable contexts;
W2 establishes action/pose ownership; W3 fit and W4 presentation use those results.
W5 editor/schema work and W7 installation diagnostics can proceed independently.
W6 builds on W1/W2. Do not delay small demonstrated fixes behind a full-system
rewrite, or close cross-system reports with only a visual improvement.

## Verification and release discipline

For each case record separately: **reported**, **reproduced**, **cause confirmed**,
**implemented**, **automated pass**, **SIM/final-eye pass**, and **affected-headset
pass**. Absence of one layer remains explicit. `Not reproduced` is not `fixed`.
An existing implementation with historical SIM coverage is not an open request
for a duplicate implementation, nor a current hardware pass.

Use the current entry point `./tools/build.ps1` (or `-SkipBootstrap` when pinned
dependencies already exist). Its CMake/CTest Release presets are the baseline.
Extend the relevant existing contracts, configurable-controls/community tests,
GPU UI-clipping tests and installer tests. Add actual text-editor lifecycle
coverage for R20. The read-only `tools/test-native-presentation.py` fixture runs
in the game's Lua VM and requires a verified live session; it is separate from
CTest and does not reproduce real missions. Update tests that encode the old
fixed-shot policy when replacing that policy; do not merely preserve green tests
for behavior the user cannot use.

Required acceptance cases include:

| Gate | Minimum sequence and evidence |
| --- | --- |
| Scene/ownership | Title/Continue/loading; avatar setup; real noninteractive demo; interactive Mission 1; Mission 6 bridge voice trigger; camera cuts; Pause/Options; return. Log raw native state, classified state/reason, input owner and camera/player generation. Test a genuine demo with a still-publishing skeleton as a negative case for stale recovery. |
| Combat/body | Stand/crouch/prone and roll; physical vs stick turn; C4/mine placement; clear space and wall/cover rejection; CQC and mount/dismount. Record native root/heading, HMD frame, rendered wrist/muzzle, native acceptance/rejection and visible result. |
| Hands/optics | Pistol/revolver/Uzi and a long-gun control; slow/fast left/right roll; support acquisition/release/reload; near/far lens boundaries; each hand occluded briefly and beyond any cache limit; reacquire; no-controller operation. Both eyes/world stay correct. |
| UI/input | Pause >5 s; iDroid pan/zoom/tabs/tutorial/close/reopen; supported window aspects; full/binoculars_only/off; preserve placement feedback with firearm-reticle preferences. Valid and invalid remaps, corrected remap, stale/missing checker, file changes, backup and neutral live apply. |
| Mounted/pad | Test each named emplacement/vehicle independently; pad-only with motion controllers absent or idle; LT/fire, bino zoom/mark/stow, menus and ownership changes. No unintended mode switch or stale screen. |
| Tracking/session | HMD and hands independently lost/returned; runtime focus/system menu, idle, removal/resume and reference-space reset. Distinguish continuity from stale-image display using frame generations and final-eye motion. |

For motion-sensitive fixes, target at least 30 seconds of continuous affected
motion and 10 transition/reacquisition cycles; extend if the reported failure
takes longer. Use a 15-minute combined play route and at least 30 minutes for
body/mount drift acceptance. These are proposed minima, not observations made
in this review. Long-latency reports require their actual triggering duration.

Capture final submitted imagery for both eyes; inspect the full sequence and
transitions, not selected stills. Pair capture with runtime/process, timestamp,
request/result identity, configuration and transaction/pose generations. Record
wall-clock duration, cadence, drops and audio when relevant. Negative fixtures
must expose frozen camera output, stale tracked actions, invisible menus or
broken hand/weapon joins to show that each corresponding gate can fail.
Use native/OpenXR semantic actions for gameplay tests. SIM plumbing cannot
certify physical fit, comfort, headset-only occlusion or Xbox hardware behavior.

At minimum rerun affected checks on Quest 3/Touch and the reporting G2 setup
before making claims about those profiles; record the actual runtime/connection.
Xbox claims need a physical pad. Preserve successful handheld opt-in, central
quad, equipment, interrogation, cabin, optics and native movement behavior.

The candidate manifest must bind source SHA plus dirty state, package ID,
DLL/checker/launcher/config-schema hashes or versions, game executable profile,
build settings and test evidence. On the installed game, record the actual DLL
and effective config hash/path, runtime, interaction profile, headset/controllers,
connection, render dimensions/refresh, mods, mission/checkpoint and repro steps.
Collect only the fields needed for the failure; do not block known source work
waiting for every tester. Build IDs must be visible enough to avoid another
ambiguous "latest build" report. Hash the packaged and installed copies.

Release requires all P0 cases passing on the exact candidate, relevant automated
and final-eye gates, and affected hardware evidence for the claims made. Any
unresolved P0 keeps the stability candidate in development/testing. A bounded
release may list remaining P1/P2 cases explicitly; an "all community defects
fixed" claim requires every defect row to be closed or demonstrably superseded
with evidence. Feature deferral is explicit, never counted as a defect fix.
Test update/rollback against the known package while retaining custom settings,
saves, owned assets and foreign loaders. Keep the previous artifact available.

## Resume and maintain this plan

The initial identified baseline, persistent observer and checked launch-to-field
route (B0/B1) are implemented with one successful live Continue route. The current
work is to **resolve input authority and observed gameplay outcomes**, including
the failed posture case and nested tutorial menu, then capture the scene-to-input
decision for the Mission 1/Mission 6 failures, while checking default D-pad
reachability and the exact controls-editor rejection. Implement the smallest
shared transition correction and verify it across the real cutscene, interactive
lesson and Pause cases. Add native-query-backed navigation and a complete
interrogation/combat route before scaling equipment tests. Do not start by applying
the six patches or assembling another unverified montage.

For every implementation change, update its report IDs with:

```text
Report ID(s), original source and reproduction:
Baseline source / installed DLL / config / runtime-profile:
Confirmed cause, or remaining hypothesis:
Owning workstream, changed behavior and contribution attribution:
Fix commit / package / DLL identity:
Automated command and result:
SIM / final-eye artifact, duration and result:
Physical device / runtime / tester observation and result:
Regressions checked, limitations, next action:
```

Retain contradictory reports as separate setup records. Add new community
evidence to these IDs or append new IDs; do not replace this plan with another
unconnected dated checklist. Public issue/PR posting and tester messages are
separate actions; none were sent as part of this planning work.
