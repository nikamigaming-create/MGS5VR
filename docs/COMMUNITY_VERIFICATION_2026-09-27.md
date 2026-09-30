# Community issue verification - updated 28 September 2026

Current RC5 DLL SHA-256: `c69b75c6099d77d773b6503caee339c51ae33411d3c65062c2b0ba28c51ca79e`. Meta XR Simulator v207. Physical headset acceptance is unrun.

All 55 reports and 77 scoped claims are retained. Historical evidence keeps the binary actually tested.

RC5 passes the actual physical Mission 1 binocular lessons and Miller explanation/return, plus Mission 6 bridge activation, player-view continuity, natural event end and subsequent VR movement. All historical evidence retains its actual DLL identity. Full-mission completion, remaining issue coverage and headset acceptance are not claimed.

See [RC5 candidate notes](RELEASE_CANDIDATE_2026-09-28.md).

## R01 - Stuck/distant cutscenes; OKB Zero helipad; prologue/jeep presentation (D/F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. RC3 retains the Coco cinematic follow and passes the complete native Mission 1 intro and return to player control. This is one real scene; OKB Zero, prologue, jeep and full headset comfort remain unaccepted. Next: Verify the other reported cinematics and physical comfort; retain the separate Mission 1 evidence.

## R02 - Mission 1, Ocelot binocular lesson cannot continue (D)

Status: **verified_simulator**

- **Acceptance evidence** - `verified_simulator`. On c69b75c6, real physical binocular raise, zoom, hand-aimed VR intel request and waypoint advance the actual Mission 1 lessons, play the complete Miller explanation, and return to RescueMiller. No Skip, timer expiry, sequence writes, or native binocular camera was used. Three separate 45-second left-eye takes contain 477 nonblack frames; both final eyes were reviewed. Earlier RC4 failures remain retained. Next: Retain the VR behavior; complete physical-headset acceptance separately. This scoped tutorial result does not certify the entire mission.
- **Existing physical binoculars usable while riding** - `verified_simulator`. The existing equip chord was rejected in horse mode before the fix. RC4 allows that same gesture while riding; both free gameplay and the actual lesson equip, zoom 2x/4x, place a physical-ray waypoint and stow. Existing personal controls were preserved. Next: Retain this behavior while fixing mission recognition; separately test automatic acquisition against a visible enemy and physical headset fit.

## R03 - Mission 6 bridge: displaced view, changed controls, cannot move (D)

Status: **not_tested**

- **Native Mission 6 bridge event and return to VR movement** - `verified_simulator`. On c69b75c6, ordinary native-map traversal triggers p31_020020_000 at full health without alert. The actual playable bridge event keeps the VR camera active and gameplay controls throughout 190 observations, ends normally at event sequence 2, and subsequent VR stick input moves the player 6.717 native units. All 699 frames in five separate 45-second left-eye takes are nonblack; both trigger and return eyes were reviewed. Next: Retain this scoped bridge result. Full Mission 6 completion, actual movement during the active event, checkpoint recovery and headset comfort remain separate.
- **Conservative stale-camera recovery guards** - `verified_automated`. Eight selected native test groups and 64 native Lua presentation checks pass. HeadCamera integration accepts fresh matching ownerless-shot publications after two seconds; wrong-camera and active-demo cases remain blocked. Next: Complete Mission 6 bridge trigger/reload with final-eye proof; these contract tests do not close R03-C1.
- **Bridge checkpoint reload recovery** - `not_tested`. The real bridge encounter and control return passed; no checkpoint reload at this encounter was exercised in this run. Next: Repeat the bridge checkpoint through the normal VR menu and verify camera, controls and scene continuity.

## R04 - Pause navigates invisibly during cutscenes (D)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Open menu now takes input precedence over cinematic stick/trigger suppression. Both-eye menu visibility, return and allowed skip through cuts remain runtime acceptance cases.

## R05 - Black/third-person view near HMD, below waist, after idle/system menu; removal/resume crash (H/C)

Status: **failed**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Prior fixes and contributor claims are partial evidence. Separate hand occlusion, HMD loss, focus/session loss and missing stereo publication.
- **Repeated stereo dropouts during field gameplay** - `failed`. Earlier live runs failed: C61B recorded 19 missing-layer frames under runtime/GPU stalls. Candidate 853e fixes startup/Pause handoffs, accepted-image expiry and delayed matching-rig classification. Its bounded 620.978-second run recorded 0 missing layers across 49,974 frame cycles; see the separately scoped regression claim. Whole-report headset/idle/resume acceptance remains open. Next: Repeat sustained natural field/dropout routes and headset playback; the specific Mission 1 activation handoff is now verified in R05-C4.
- **No empty submission through startup, Continue/loading, Pause and producer delay** - `not_tested`. The RC2 cinematic failure is corrected in RC3 and accepted separately in R05-C4. The complete combined startup/Continue/Pause/producer-delay case has not been rerun on RC3. Earlier field and induced-stall evidence retains its original DLL identity. Next: Repeat the combined field Pause and producer-delay route on RC3, then physical headset acceptance.
- **Native Mission 1 cinematic cuts and return to player control retain a submitted scene** - `verified_simulator`. RC3 DC49 completed the real 128.516-second intro through Seq_Game_IDroidTutorial. The startup/Continue/intro window measured 17,035 frame cycles, zero missing layers, zero invalid tracking and seven explicitly retained camera-handoff frames. One startup cycle was requested non-rendering by the runtime. Both-eye samples and two sampled compositor takes show the scene without an added quad. Final c69b75c6 additionally completes the actual Miller explanation and return after fixing the reproduced retired-image handoff deadlock: 477 nonblack frames across three separate 45-second takes, both return eyes reviewed. Capture cadence is 3.2-3.8 fps; this does not certify headset smoothness. Next: Physical-headset acceptance and other cinematic/field routes remain separate.

## R06 - Changing game options drops VR until checkpoint reload (D)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Change options/resolution and resume without losing chosen presentation or camera owner.

## R07 - C4 follows Snake's facing instead of player; mine/placement feedback (F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Native placement authority unverified. Physical/stick turn, recenter, tank placement, crouch/prone: preview and placed object agree.

## R08 - Clear-looking aim blocked near cover/prone; UI sound; rock behind player blocks shooting (D/F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Distinguish input rejection from native obstruction. Clear muzzle fires without stance reset; genuinely obstructed muzzle still blocks.

## R09 - Upper/lower body split, shadow mismatch, horse drift, box/toilet view after CQC/mounts/long play (D/F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. Coco stationary lower-body correction is integrated with bounded horizontal shift, no vertical shift and native actor/collision unchanged. Native geometry/state contracts pass; the full visible-body, horse and sustained-session report is not accepted. Next: Compare native root, displayed pelvis/torso and HMD across repeated transitions and a sustained session.

## R10 - Hands/weapons/items fade near surfaces; arms disappear near ground/prone (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: One fade hook exists. Identify first missing stage; verify both-eye visibility on slopes, walls and affected costume.

## R11 - Left hand jumps/detaches on pistol/revolver/Uzi, especially left roll (D/F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Current compact support logic is insufficient evidence. Sweep roll/close contacts, reload, release, reacquire and change weapons.

## R12 - Short arms, restricted reach, exposed/deformed sleeves (H/C)

Status: **needs_headset**

- **Acceptance evidence** - `needs_headset`. Coco shoulder offset {0,-0.23,-0.04} is integrated and contract-tested. Sampled arms remain visible; user-specific reach, costumes and comfort require headset checks. Next: Measure shoulder/wrist/reach across users and costumes. No universal shoulder-offset fix is established.

## R13 - Low viewpoint, walking bob/sway, nausea; requested better height (D/F/H/C)

Status: **not_tested**

- **Height adjustment exists** - `not_tested`. Coco bob suppression and camera offsets are integrated. Native tests cover gait oscillation, posture re-anchoring and unfiltered physical-head motion. Fresh field walk/stop and stance controls pass; physical comfort remains unverified. Next: Verify calibrated height through walk/run, stance and mount transitions with different users.

## R14 - One-hand pitch; Quest -20 and G2 -16 fit reports; weapon-only pitch request (F/H)

Status: **needs_headset**

- **Acceptance evidence** - `needs_headset`. The one-hand pitch correction exists in source, but physical controller/grip/aim, contact, sights and shot coherence need headset acceptance. Next: Aim correction exists. Verify controller/grip/aim, hand contact, sights and shot remain coherent; retain personal fit.

## R15 - Binocular/scope narrow eye range and intermittent lens disappearance; 15/16 cm boundary; scope jitter/smoothing request (D/F/H)

Status: **needs_headset**

- **Acceptance evidence** - `needs_headset`. Tuning changes are present, but physical tracking-safe distance, hysteresis, latency and both-eye scope behavior have not been accepted on headset. Next: New settings helped. Test physical tracking-safe distances, threshold hysteresis, smoothing/latency, both eyes, scopes and surrounding geometry.

## R16 - Weapon seems to switch near the face (F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Cause unknown: trace physical/chord/context/equipment events before attributing to optics distance.

## R17 - Pause at hip/bottom-left, unreachable; frozen arm while paused (D/H)

Status: **needs_headset**

- **Right glove/forearm follows sampled paused controller poses** - `verified_simulator`. Right glove/forearm changes pose in both sampled eyes with the actual field Pause page on CD91. Next: Repeat on physical headset after the left prosthetic correction.
- **Centered Pause canvas fits both eyes** - `verified_simulator`. The ordinary field Pause page fits both CD91 eye views. A contaminated tutorial-menu test was rejected and rerun after documented cleanup. Next: Check readability, scale and placement on Quest Link.
- **Pause hand/panel acceptance on a physical headset** - `needs_headset`. The candidate evidence is from simulator final eyes; no physical headset fit or hand tracking acceptance was run. Next: Verify tracking, reach and full panel visibility in both eyes on headset for representative poses.
- **Left prosthetic follows controller while paused** - `verified_simulator`. The modular prosthetic moves in both sampled eyes while the ordinary field Pause menu is open on CD91. Next: Physical-headset left prosthetic and Pause motion acceptance.
- **Pause navigation keeps Snake stationary** - `verified_simulator`. Fresh ordinary field Pause navigation on CD91 produces 0 m player displacement and 0 degrees yaw. Next: Physical headset acceptance remains open.

## R18 - iDroid wide/squashed/tiny and desktop-aspect dependent; requests position/rotation/size (D/F)

Status: **failed**

- **Native hand motion and stationary stick navigation in the iDroid** - `verified_simulator`. Current-build native telemetry records six sampled palm poses and stationary navigation as observed_pass; paired visual review confirms the Mother Base screen content renders. The left-eye screen clipping is tracked separately in R18-C5. Next: Review the paired final-eye pose sequence for hologram orientation, placement, scale and hand attachment; then repeat in headset.
- **iDroid hologram aspect, placement and clipping across displays** - `not_tested`. No current display-size matrix or visual review for aspect, clipping, pan/zoom and tabs was located. Next: Compare native canvas at 16:9, 16:10, ultrawide and custom render sizes; exercise pan, zoom and tabs in both eyes.
- **Back closes iDroid in the tested current session** - `verified_simulator`. Normal Back closes iDroid in the RC3 Mission 1 device lesson after Ocelot releases the native input restriction, and the real mission advances to Seq_Game_BinocularsTutorial. The harness previously used the wrong close action; this is fresh validation, not a new device runtime fix. Earlier normal-field Back evidence is CD91. Next: Verify physical headset close/reopen and the separate forced FOB tutorial in R19.
- **iDroid hand tracking and hologram fit on headset** - `needs_headset`. All current proof is simulator/native telemetry; physical hand tracking, reach and readability remain open. Next: Repeat pose, navigation and exit tests on headset with a matched native and both-eye capture.
- **Native iDroid screen content and stereo fit during hand pose** - `failed`. RC3 still crops the right edge of the held map in the left-eye sample at the tested palm pose. Map content and normal Back work; screen-fit acceptance remains failed. Next: Correct placement while retaining hand/device attachment, then repeat scale, rotation, aspect and tab checks in both eyes.

## R19 - iDroid tutorial needs D-pad; player stuck (F/H)

Status: **failed**

- **Complete tutorial-required D-pad inputs** - `not_tested`. No current-build completion of the requested iDroid tutorial sequence was located. The default D-pad actions are disabled in the known binding configuration. Next: Run the full tutorial from a clean authorized save path, resolve every visible prompt, and prove tutorial completion without progression edits.
- **Safe escape and reopen from the tutorial/forced menu** - `failed`. In the forced-tutorial path, held Back reached a visible “Exit Tutorial / Suspending the tutorial for now / A Close” page. That is evidence for a suspension escape route, not tutorial completion. A different current normal-map close timed out as recorded in R18-C3. Next: Separate normal-map close from forced tutorial recovery; add a guarded path only for a freshly captured known prompt, then verify reopen and tutorial completion independently.
- **Guarded A Close on the exact tutorial-suspension page** - `verified_simulator`. The exact visible A Close acknowledgment was accepted; the menu UI closed and native state returned to gameplay. This is a guarded escape from the tutorial-suspension page, not tutorial completion. Next: Verify the native handset lifecycle and safe reopen after acknowledgment; this action does not complete the tutorial.
- **Handset stows after tutorial-suspension acknowledgment** - `failed`. After A Close, the UI closed but the handset remained held in the right-eye review, exposing a separate device lifecycle defect. Next: Resolve native stowed/held state after the acknowledgment, then prove safe close and reopen.

## R20 - Controls editor Save becomes unavailable after remapping (F)

Status: **verified_automated**

- **Save remains available and reports valid remaps in the settings UI** - `verified_automated`. The hidden real-handler lifecycle test verifies valid edit/save, invalid diagnostic, correction, backup, and external-edit protection; embedded browser QA separately exercises remapping. Bridge fixture tests also pass. Scope is the tested editor/launcher build, not the original unknown personal edit. Next: Repeat with a copy of a user configuration and verify restart/reload preserves the selected bindings.
- **Valid save, backup, invalid/conflicting edit and stale revision handling** - `verified_automated`. Disposable-fixture regression checks cover valid fractional save, backup, rejected invalid/conflicting edits without mutation, stale revision and runtime rejection. This verifies bridge behavior, not the original unknown personal edit. Next: Retain the fixture regression and verify one clean external-config save/reload on the packaged launcher.

## R21 - Thumb-rest mapping missing (F)

Status: **not_tested**

- **Separate left/right thumb-rest tokens flow through profile sampling and binding export** - `not_tested`. Source plumbing is present, but no hashed run/test artifact proving the separate profile tokens was found in this audit. Next: Add a durable automated profile test covering left/right sampling, parse and binding export.
- **Physical headset profile emits distinct thumb-rest inputs** - `not_tested`. Source plumbing is present, but no hashed run/test artifact proving the separate profile tokens was found in this audit. Next: Verify each thumb-rest independently on the supported Touch profile and confirm no unintended action fires.

## R22 - Missing/conflicting controls; binocular stance differs; unclear controls/manual (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Inventory effective actions by context/profile; test transitions, holds/chords, remapped controls and neutral live reload.

## R23 - Cannot toggle suppressor/flashlight in quick menu (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Identify native action and provide usable conflict-free route with visible native state change.

## R24 - Carry/drop bodies, plants/diamonds/context actions and interrogation unclear/unusable (F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Instructions conflict across layouts. Test the active config's native hold/context actions and visible prompts; verify interrogation intel.

## R25 - Missing/intermittent world markers/highlights; unclear `full` (D/F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Mode-dependent visibility is intentional in source. Test mark/unmark and stow in each mode; trace any loss inconsistent with policy.

## R26 - Markers obscure enemy in `full` (F/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Separate scale defect from R25. Bound apparent size near/far, world/binocular views and crowded scenes.

## R27 - Misaligned gun reticle; keep throw/placement reticle; laser too thin (D/F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Preserve useful aim feedback per action. Turning off all native reticles is not an acceptable workaround for placement.

## R28 - Missing ammo/flower/diamond icons, captions/prompts/notifications; blank loadout or detached HUD (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Catalog missing native layers individually; prove each relevant pickup/message/loadout event in both eyes.
- **Helicopter title tape labels and loading-quad fit** - `verified_simulator`. Fresh CD91 title/Continue/loading samples fit both eyes and preserve separate cassette columns. Captions are small; props remain beneath loading. Next: Physical-headset readability, then loading transition prop cleanup.

## R29 - No soldier stats through binoculars (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Separate native analysis/progression availability from suppressed UI and from ordinary marking.

## R30 - NVG mesh blocks view (D)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Hide only the offending first-person mesh where appropriate; retain NVG effect and equipment state.

## R31 - Tank/APC view fully obstructed (D)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Choose vehicle-specific view/mesh treatment; test entry, motion, aiming, firing and exit without a global height hack.

## R32 - Mortar fires but cannot aim (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Verify native targeting/range axes and reticle, with visible impact at intended target.

## R33 - Machine-gun turret lacks reticle/elevation (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Both aim axes, firing, usable view and reticle, then exit.

## R34 - Helicopter/Pequod turret inaccessible (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Verify native access/context action, traverse, elevation, fire and return.

## R35 - Combat-vehicle reticle absent; ZHUK RS-ZO range cannot change (D)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Test this vehicle separately from ordinary tank/Jeep steering, including range and rocket impact.

## R36 - Cabin/cockpit bindings mixed; Continue selection/hidden-menu focus (D/H)

Status: **not_tested**

- **Physical Continue from the title rack reaches gameplay** - `verified_simulator`. Fresh physical Continue reaches native field gameplay and both final eyes show world/hands on DLL 75efffffbbf5. Controller pose restored. This run's short recording failed cadence validation and is excluded from video acceptance. Next: Repeat cold launch and Resume from saved gameplay; capture both-eye rack focus and headset behavior.
- **Cabin controls and hidden-menu focus across cold launch and Resume** - `not_tested`. The fresh run proves Continue arrival but does not cover the full cabin binding set or Resume/cold-launch focus matrix. Next: Exercise each cabin action and Resume across a cold launch; verify no hidden menu navigation in both eyes.
- **Cabin usability and Continue on headset** - `needs_headset`. The fresh title-to-gameplay handoff was performed in the simulator; physical rack reach, focus and menu discoverability are not established. Next: Verify title rack reach, focus and Resume behavior on headset.

## R37 - Vehicle auto-center on entry; view/steering/horse direction disagreement (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Declare native mounted steering policy; no forced HMD turn or inherited on-foot snap offset on entry/exit.

## R38 - Xbox LT + shoot freezes into 2D presentation (F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Trace native aim/menu flags and owner; gamepad-only aiming/firing keeps live head tracking and intended display.

## R39 - Xbox binocular view floats at lower left (F)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Provide centered native optic presentation when pad owns input; no requirement for tracked controllers.

## R40 - Resolution/sharpness, >4K startup, headset vs mirror FPS, optic/directional lag (F/H)

Status: **needs_headset**

- **Acceptance evidence** - `needs_headset`. No matched headset runtime/resolution/refresh/route performance measurement was located; simulator and configuration results are insufficient. Next: Match runtime/resolution/refresh/route; measure fresh submitted frames and latency. Prior config/SIM results do not establish headset performance.

## R41 - Horizon artifacts/tree pop-in (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Matched left/right motion capture; isolate culling/projection from resolution and frame cadence.

## R42 - Rectangular light/shadow mismatch; displaced flares/outline tails (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Separate layers with same-frame stereo/projection evidence; do not group all artifacts as a pacing bug.

## R43 - Infinite Heaven/IHHook/SnakeBite coexistence (D/H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Versioned loader strategy and reversible install tests; preserve foreign loader. No claim of general mod compatibility.

## R44 - DLL/INI swaps start flat; folder/update/removal confusion (H)

Status: **verified_automated**

- **Launcher identifies build and provides configuration controls** - `verified_automated`. The packaged launcher QA records build/config identification and tested settings editing. This covers the launcher flow, not complete install, update and rollback preservation. Next: Run complete-package install/update/rollback fixtures and verify personal settings, saves and unrelated files survive.

## R45 - ReShade/DLSS/other SnakeBite or graphics mods (H)

Status: **not_tested**

- **Acceptance evidence** - `not_tested`. No qualifying current-build evidence for this acceptance slice was located during this audit. Next: Separate versioned combinations; do not infer support from one user's successful mod.

## R46 - Snap turn/angle/faster turning requested; recenter tilt history (D/H)

Status: **not_tested**

- **Snap-turn settings and source behavior** - `not_tested`. Current immutable source snapshot and the completed core suite cover neutral latch and leaned snap-pivot contracts. Simulator and headset behavior has not been accepted; this runtime claim stays open. Next: In simulator and headset, test smooth vs snap, angle, neutral latch and leaned pivot.

## R47 - Physical stand/crouch/prone (D/H)

Status: **not_implemented**

- **Opt-in physical body movement drives native standing, crouch and prone** - `not_implemented`. No implementation or acceptance evidence for physical head-height stance policy was located; existing controller-driven stance roundtrips do not prove this feature. Next: Implement opt-in posture inference after height calibration, then verify no double crouch or broken prone roll.

## R48 - NVG hand-to-head toggle (D)

Status: **not_implemented**

- **Deliberate hand-to-head NVG gesture with hysteresis** - `not_implemented`. No hand-to-head NVG gesture implementation or test was located. Next: Implement a deliberate gesture with hysteresis and prove it never switches equipment accidentally; keep R30 mesh handling separate.

## R49 - Visible stance indicator (D/H)

Status: **not_implemented**

- **Readable native stance indicator** - `not_implemented`. No stance indicator implementation or runtime evidence was located. Next: Add an indicator driven by native stance state and verify readability through scripted and mounted states.

## R50 - Enemy senses, subsistence, time/weather features from IH (D)

Status: **not_implemented**

- **Infinite Heaven enemy-sense, subsistence and time/weather feature set** - `not_implemented`. These requested gameplay features are outside the verified VR runtime and no implementation evidence was located. Next: Scope each feature separately, preserving native progression semantics and existing loader compatibility.

## R51 - True left-handed rig (H: issue #8 at earlier review)

Status: **not_implemented**

- **Complete left-handed rig and interaction ownership** - `not_implemented`. No complete mirrored weapon, grip and interaction implementation was located; button-label swapping would not satisfy the request. Next: Implement a mirrored hand/weapon/interaction profile and verify controls and native contacts on headset.

## R52 - Ground Zeroes first-person completion (H: issue #7 at earlier review)

Status: **not_implemented**

- **Ground Zeroes first-person game adapter and completion** - `not_implemented`. Current evidence and candidate target TPP; no GZ-specific camera/player/rig/HUD adapter or completion proof was located. Next: Build separate GZ adapters and acceptance; do not infer completion from TPP results.

## R53 - Quiet/rat/buddy expansion and universal mod installer (H)

Status: **not_implemented**

- **Quiet, rat and buddy expansion plus universal mod installer** - `not_implemented`. No complete new actor-interaction set or universal mod-installer support was proven by this audit. Next: Implement and verify each native interaction and installer compatibility separately while regression-testing current buddy behavior.

## R54 - In-headset configuration instead of external editing (H)

Status: **not_implemented**

- **In-headset settings editor** - `not_implemented`. An external/desktop 3D launcher settings editor is verified, but no in-headset editor was located. Next: Design an in-game editor with safe input ownership, schema-backed values, persistent save and readable headset UI.

## R55 - Roomscale/body movement and rotation follow the player (D/H)

Status: **not_implemented**

- **Roomscale body movement and rotation follows the player** - `not_implemented`. No collision-aware roomscale/body-follow implementation or arrival proof was located. Next: First prove R07 action heading and R09 root/pelvis alignment, then implement collision-aware body movement with independent look, stance/mount rules and observed arrival.
