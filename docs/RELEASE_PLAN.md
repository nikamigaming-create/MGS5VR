# Release execution plan — 26 October 2026

### Follow-up contact evidence

Retained motion samples expose a contact regression in the scripted center-grip
poses: the previous authored attachment put the index tip within 3–6 mm but
snapped the sheet 8–9 cm and about 106 degrees; the current attachment avoids
that snap but leaves a measured 30–38 mm fingertip gap during the sampled holds
(`artifacts/opening-paper-contact-comparison-20261026.json`). Those scripted
poses are not naturally aligned pinch trials. A nominal finger-chain search
suggests the existing curl axes alone cannot close that gap in those poses; it
is not a full IK or visual acceptance result
(`artifacts/opening-paper-curl-feasibility-20261026.json`).

Additional leads, not diagnosed failures: camera handle alignment predicts a
33 mm pickup shift for the sampled right grip, while the sampled room-relative
camera shift was 32 mm; the trace lacks a tight stationary before/after bracket
(`artifacts/opening-camera-pickup-lead-20261026.json`). The marker's large
right-hand displacement coincided with a 58 cm input-to-rendered-palm separation;
a left-hand sample had close palm alignment but was not tightly bracketed
(`artifacts/opening-marker-pickup-leads-20261026.json`). Reproduce these in a
future combined pass with reachable, deliberately aligned grasps, inspect both
eyes and preserve typing, drawing, release and throw behavior. Do not infer
physical Quest results or change the existing flow solely from these leads.

## Opening cabin follow-up — 26 October

Checked/deployed development DLL `217df3f4b9be` includes a repair for a
reproduced stationary pickup jump: the pinned note and Polaroid previously
moved 8–9 cm and rotated about 106 degrees when gripped. Pickup now preserves
the selected hand-relative pose, with only a small aligned pinch adjustment;
contact adjustment does not manufacture throw velocity. In the combined native
run `artifacts/opening-paper-lid-validation-20261026`, stationary samples
moved about 1 mm and 0.09 degrees. The run completed cassette pickup, off-axis
rejection, seating, eject/recovery, camera printing with both hands, note
editing, marker writing, and note/photo board placement. All 58 build test
groups and installer checks passed. The user's board was restored byte-for-byte
and existing photos verified (`artifacts/opening-studio-validation-restore.json`).

Eight coincident lid end-face vertices previously stayed behind; the exact
native interior mesh now moves 76 lid-body vertices plus four window vertices
(`artifacts/opening-lid-end-seams-20261026.json`). The second native player bone
influences a small control, not the identified lid vertices; this does not
exclude other native animation mechanisms. Original lid travel remains unverified.
The paired-eye stills are too distant or obstructed by sleeves to establish
clean fingertip contact or the quality of the moving lid. Dynamic lid motion,
wider grab orientations, physical Quest testing and whole-scene acceptance
remain open. The test-owned session is closed and Steam retained.


## Opening cabin cassette and lid — 26 October

The user reports unnatural cassette insertion and questions the tape-player
model and lid motion; Polaroid and Post-it handling are also in scope. The
existing player body is owned native helicopter-interior geometry. A point-set
comparison of all 478 vertices against the separate owned `pcp0` cutscene player
matched within 0.0002 mm after the measured placement; this proves geometry
correspondence, not native animation or material parity. The white instant
camera is a separate mod-authored object. The lid uses reconstructed motion;
the original cutscene animation and exact travel remain unverified.

A live off-axis attempt reproduced an 89.36-degree cassette rotation at the
guide boundary (`artifacts/opening-guide-repro2-20261026/guide-snap.json`).
Source `4945ca9` preserves the hand-driven rotation, rejects a sideways entry,
introduces guide correction continuously and interpolates seating after release,
including reversed aligned tapes. The exact hashed interior mesh had ten lid
triangles mixing animated and stationary vertices; the previously identified lid components
then moved together (68 body vertices plus four window vertices; see follow-up above). Geometry audit:
`artifacts/opening-lid-topology-20261026.json`.

Checked/deployed candidate `d2341d57320d` passed all 58 test groups and
installer checks. Fresh native run `artifacts/opening-guide-verified-20261026`
measured zero rotation at the formerly snapping sideways guide boundary and
completed pickup, seat, eject and supported recovery. Both final-eye seated and
open-player images were reviewed; dynamic motion and physical headset comfort
remain unaccepted. Earlier paired workbench `20261027T011954036566Z` completed
Polaroid and Post-it pickup, writing and board placement on the prior candidate;
this is not proof of smooth contact at every orientation. In-game tests added
four photos and four strokes to an existing note; the pre-test library and
note stroke count were restored, test images retained privately, and other
paper records preserved byte-for-byte (`artifacts/opening-studio-cleanup.json`).

Next: review the reconstructed lid travel against a verified original native
animation or physical reference, and reproduce any remaining paper contact or
placement instability before editing those working paths. Physical Quest and
whole-scene acceptance remain open. Steam retained; owned session closed;
published play pin and personal control settings unchanged.


## Gameplay toggle demonstrations — 26 October

Source `52585ca` adds a 25–150% marked-enemy size control (60% default),
front-loads God Mode and common switches, and groups related settings in the
headset and launcher. A reproduced live-edit bug stowed binoculars when marker
settings changed; display/recon-only changes now preserve the active optic
after neutral release. Binding and other control changes retain the reset.

Checked/deployed dev DLL `4187a681cd4f` passed 58 test groups and installer
checks. `artifacts/toggle-showcase-20261026/field-kit-gameplay-showcase.mp4`
is an edited native right-eye gameplay recording with game audio; paired
final-eye checkpoints and exact scopes are in `coverage.json`. It demonstrates
body/eyepatch (previous candidate), binocular/world marker visibility and size,
close-muzzle firing allowed/blocked, and cover attachment/release/re-attachment.
The sandbag entry needed a lateral approach; lean/aim retained native cover at
night, with visual and physical acceptance still open. A second binocular glow
setup lost the bot's fresh tracked owner; glow remains unverified. God Mode
damage immunity, other switch effects, dual sights, physical Quest behavior,
daytime shifting shadows, and wider mounted interactions remain open. This is
not a complete all-toggle showcase or a whole-game acceptance claim.

Next: finish effect-based demonstrations for the remaining switches and
investigate the tracked-owner interruption; diagnose daytime shadow movement.


## Mounted spatial aim and daytime shadow report — 26 October

The user clarified that the daytime report is **larger light/shadow patches
shifting during head nods**. Trees never disappear. Do not diagnose this as
missing foliage/culling or tiny specular sparkles. The cause remains undiagnosed;
no rendering or graphics-setting change is claimed.

Retained `artifacts/foliage-20261026/baseline-lz` records the initial daylight
head-pitch sweep and paired final eyes. `daylight-lighting` in the same directory
records a noon fixture, exposure observations and both eyes; the original clock
was restored. `camp-lighting` occurred after sunset and is excluded from daytime
acceptance. These captures do not establish a shimmer fix or headset acceptance.

Source now extends the existing mortar `0x46` / machine-gun `0x47` handle adapter:
spatial hand displacement uses the actor's authored forearm length as an input
lever; wrist rotation remains additive; either hand operates and the other
supports/transfers at the live bearing. Right-stick trim adds to the target,
stops at the attained bearing on release, limits pending travel at native stops,
and does not rotate the viewer. Native mechanical limits, action ownership,
tracking-loss release and fresh-grip reacquisition remain in force. This is not
a newly verified physical emplacement pivot or universal mounted-gun adapter.

The retained mortar attempt `20261026T192853043999Z` refused the already destroyed
native fixture before mounting. It is a fixture refusal, not a new input failure
or an acceptance pass. The new spatial/trim behavior has contract coverage;
in-game hand contact, discharge/impact and physical responsiveness remain open.
Helicopter/anti-aircraft handle admission is still unverified. Initial Blast Arm
launch heading, recurring tutorial notices and previous physical limits remain.

Checked/deployed candidate `87186cb55ebe` / source `939d41a` passes all
58 test groups and installer checks. Final review also repaired the one-frame
hand-transfer fallback to camera turning. The initial handoff build failed for a
missing include; the corrected final build passes. New in-game/physical mounted
acceptance remains open. Use `Play-Dev.cmd` for the user's review.

Next: diagnose the reported daytime shadow patches and confirm actual native
helicopter/anti-aircraft contacts before extending admission. Do not substitute
a speculative shader patch or disable shadows and mark the report fixed.
Published play remains pinned; all 424 published and 434 development files,
installed DLL and personal settings verify. Owned simulator/input session is
closed, clock/poses restored, Steam retained and Meta 64-bit Link checked.
Receipt: `artifacts/mounted-spatial-preservation.json`; final build log:
`artifacts/mounted-spatial-final-build.log`. No whole-game/headset sign-off.

## Blast Arm tower destruction with game audio � 26 October

The user rejected take6's 3.3 fps capture and missing game audio. Replacement
`artifacts/rocket-proof-20261026/take7-native-av/field-kit-rocket-arm-game-audio.mp4`
retains a continuous 22.9-second native right-eye source and synchronized MGSV
process audio: 680 unique image transactions, 29.69 unique fps, eight dropped
samples, longest source gap 0.266 seconds. The 30 fps container includes repeated
frames; no interpolation. Full render FOV aspect is corrected, without cropping.
This is in-engine eye footage, not final-compositor video. Separate paired final
eye checkpoints are retained. No synthesized narration. Actual launch, socket
motion, gyro, stick and native return are shown; no demolition in this take.

Crouched A-star approach `20261026T181347204901Z` reached the outpost. The earlier
stance-input failure remains scoped to its own run. Simulator Thumbstick X/Y
writes were observed to replace one another, preventing diagonal radial choices;
this does not establish a physical-controller defect.

User authorized stopping the separate OpenNV test to free Meta XR. Its verified
process and child crash handler were stopped; Steam retained. Fresh MGSV process
34376 loaded the ACC, where Blast Arm was selected through the native sortie
loadout (retained `artifacts/rocket-tower-20261026/blast-equipped`). Supply-drop
waiting was abandoned. Deployment `20261026T184813917645Z`, approach
`20261026T185023101192Z` and flank `20261026T185239276285Z` are retained.

**Delivered:** `artifacts/rocket-tower-20261026/blast-shot2/field-kit-blast-arm-tower-game-audio.mp4`.
32.4 seconds, 970 unique native images (29.94 fps), two repeated CFR frames,
maximum source gap 0.0993 seconds. Synchronized 48 kHz stereo MGSV audio has no
reported capture gaps; mean -18.7 dB, peak -0.5 dB. No narration or interpolation.
Native hand ID 518 and God mode 1 are recorded. Launch, additive socket/gyro/stick
guidance and explosion collapse the wooden watchtower at about 24.5 seconds.
Both final eyes after smoke clears show the tower gone. No enemy casualty claimed.
Review/limits: `blast-shot2/visual-review.json` in the same retained directory.

First take `blast-shot1` missed: launch follows the native camera heading, not
HMD gaze. Existing native-smooth right-stick turning aligned the second shot.
This is an unresolved launch-aim contract gap, not a source repair or a perfect
rocket-arm pass. Next fix: investigate tracked LEFT socket aim at initial launch;
retain recurring tutorial notices and physical-headset acceptance as open.

Owned session closed, input/poses restored, Steam retained, Meta 64-bit Link
checked. All 424 published and 434 development manifest files, installed DLL and
personal settings match the earlier receipt. Published pin unchanged. Receipt:
`artifacts/rocket-tower-20261026/preservation.json`.


## Checked spatial controls and silent video — 26 October

Candidate `9d5e15c0a101` / source `e162ef6` passed all 58 test groups and installer
checks and is deployed to development. The new spatial socket channel, stronger
wrist gyro and joystick are additive. Live screen/socket/gyro/twist and fitting
settings remain available; the oval stays flipped up above the wrist.

Retained `artifacts/rocket-proof-20261026/take6` shows centered arm approach,
palm-down wrist, launch, spatial raise/wave with wrist rotation fixed, wrist
pitch/yaw/twist, all three controls together, downward guidance and native return.
All 77 left and 24 right captured frames reviewed. Capture cadence is 3.30/1.00
fps, so smoothness and physical headset acceptance remain open. Native horse
crosses the foreground. User accepts intentional right-eye patch overlap.
Take5 ended before completing all three inputs; it remains incomplete.

Delivery is `take6/field-kit-rocket-arm-no-voice.mp4`: continuous real left-eye
footage with Field Kit labels outside the image. Robotic narration was rejected
and removed; ffprobe confirms video only. No game audio was captured. Do not
add synthesized narration back to this delivery.

Recurring Choose a Rival Help remains unresolved despite saved FINISH state;
normal Back dismisses it. Latest retained arrival/arm selection route is
`20261026T171614902974Z`. No tower demolition or enemy impact is yet demonstrated.
Tower approach `20261026T173208372900Z` reached the ridge foot. Ascent
`20261026T173417070766Z` failed when requested physical input was not observed
by the native XR sampler; no shot or demolition occurred. Earlier wrong-height
goal refusal `20261026T172703692067Z` is retained separately. Cause of ascent
input failure is undiagnosed; no collision or gameplay repair is claimed.

Owned simulator/game session is closed, inputs released, Steam retained, Meta
64-bit Link runtime restored/checked. Both build slots and installed DLL verify;
personal settings and published pin are unchanged. Receipt:
`artifacts/rocket-spatial-preservation.json`. Next: user Play-Dev headset review;
retain tutorial persistence and tower-ascent input failure as unresolved. Older
sections below retain the historical stage and build they originally describe.

## Socket + gyro + joystick clarification — 26 October

User explicitly requests three concurrent controls: spatial forearm/socket aim,
stronger independent phantom-wrist gyro, and fine joystick correction. Prior
`0451221ea76d` demonstrates wrist gyro and joystick, but moving a socket with
unchanged palm orientation did not itself steer. Its centered take4 is retained
at `artifacts/rocket-proof-20261026/take4`: arm approach/palm-down pose, launch,
wrist rotation, translation, joystick, downward burst and native return; 22 s,
65 left/63 right frames at 2.95/2.86 capture fps. Both complete frame sequences
reviewed; left feed clear. User confirms the noticeable right-eye patch overlap
is intentional and acceptable, not a defect by itself. No physical acceptance.
Take3 timed out waiting for automatic return and is retained as incomplete.

New source adds the solved elbow-to-socket direction as a separate yaw/pitch
channel at gain 1, alongside the existing wrist gyro (default gain 2) and stick.
Socket roll is excluded from this channel to avoid duplicating wrist twist.
All three add before native bounds; none overrides/recenters another. A new
live Rocket Arm socket steering switch defaults on. There are 70 settings and
19 switches. Build/check/deploy and spatial-steering video are pending. Retain
the flipped-up oval, original personal settings and published play pin.

Latest user clarification: flip the oval UP above the socket, facing the pilot.
Source now lifts it 60 degrees from the dorsal wrist plane about its attached
near edge; the complete mount still follows the wrist. This supersedes the flat
plane requested earlier. Flat candidate `eca68f998fb9` passed 58 groups and
installer checks, but take2 ended during yaw before all controls were shown.
The flight/feed returned early; impact cause is unproven. Retain
`artifacts/rocket-proof-20261026/take2` as incomplete. Flipped-up `e56cff6352e9`
also passed 58 groups/installer and deployed. The following build synchronizes
launcher wording and this evidence record; flipped-up visual review is pending.


Next action: build/check/deploy the corrected dorsal OVAL wrist display, then
record the user-requested bounded final-eye demonstration. The earlier upright
rectangle failed visual review. Show actual launch, wrist twist, translation,
additive joystick and native return; inspect both eyes. Preserve the published
pin and saves, restore poses/input/runtime and close the owned simulator after
recording. No universal or physical-headset acceptance from this take.

Current action completed: checked/deployed `fbc50f53c4d3` / source `3421214` of the clarified Rocket Arm batch:
rectangle attached to the left socket, phantom left-hand gyro including optional
wrist twist, additive joystick correction and live tuning. All 58 groups and
installer checks pass; both slots and installed DLL verify, personal settings
and published play are preserved. Next use the user's physical headset feedback.
Do not claim the new interaction is
perfect from contracts or reuse old Rocket Arm images as acceptance of it.

Latest action completed: checked/deployed `c5900adb1a22` / source `72b3636` for the live-settings
usability batch (first-page body/eyepatch, switches before fitting controls,
accurate launcher descriptions). All 58 groups and installer checks pass; both
slots verify, saved preferences and published pin remain intact. Next, use
the user's Play-Dev physical feedback; no repeated
simulator/soak runs. Existing unresolved field scope remains in FIELD_FIXES.
This change does not establish new gameplay or headset acceptance.

This is the durable plan for finishing the user-reported repairs and preparing
the next TPP experimental release. Read [CURRENT.md](CURRENT.md), then execute
this plan. [FIELD_FIXES.md](FIELD_FIXES.md) separates implemented repairs from
remaining code work and acceptance. Do not restart the three-day investigation
from unchecked historical boxes.

The user has now explicitly set release readiness as an active goal: complete
all justified fixes on main, then test the batch and continue through remaining
field/progression checks. Do not stop merely because one cuff case passes.

Current checked/deployed candidate is `27e9108f2a58`, preserving held walking
input across helicopter disembark. The targeted held-stick smoke passes, with
0.4035 units horizontal travel after exit and neutral input restored afterward.
Retain the user's broader physical movement report until its behavior is confirmed.
Previous `1e50576a0e7d` darkens the native
first-person eyepatch to matte black and removing reflection-like atlas detail.
The prior `422594fb5ef8` supplies retained stereo on/off/motion pictures.
The preceding `f58ef4224995` batch remains included. The reported arm,
sandstorm, Rocket Arm, MG and mount-centering batch is implemented and has scoped
combined gameplay results in CURRENT. Actual follow-up defects (MG root-yaw
feedback and Rocket guidance neutral resets) are corrected. Original settings
and Meta Link are restored and Play-Dev is ready. Retained native face captures
confirm eyepatch on/off/on; physical headset review remains pending. Retain the older unresolved field scope;
do not repeat completed narrow cases or certify untested combinations.

Previous checked candidate `62f696613e2b` / `5bb471f`:
It fixes terminal stow routing under camera-suspending Help and retains the whole
physical Menu press as handheld-owned. Forced-Help stow/reopen passes retained
20261025T152037198735Z.
It additionally retires the on-board exemption after disembarking and admits
Challenge Tasks to exact-owner stow-Help cleanup. Open-handset exit/stow smoke
passes; the corrected live Challenge Tasks stow/reopen now passes too. Physical
review remains open.
It includes camera/skin body-visibility continuity and field-pickup iDroid
stow/reopen ownership. Scoped full-body/forearm-only movement and flight/field
handset lifecycle reviews passed. Actual pickup entry remains unaccepted because
the helicopter fixture did not land. Extreme cloth, exact terrain failures and
broader field/headset scope remain open. The previous stock correction and its
retained failures remain recorded in CURRENT and FREE_PLAY_ACCEPTANCE.

The identities below record the original audit baseline.

## Starting point and authority

- Work in the existing checkout on `main`; no new branch or worktree.
- Audited gameplay source: `5622052bd8799603ef8aae66235ec82d3c7bce6f`.
- Checked development DLL: `17b044cba72e33e6d6eefc89be5f00bcb313ba0fee4bb8751da9d43e52b50e4f`.
- Published local play DLL: `b627c7cacc9acd540e04193a811d3ca14bd59c9be542cc587345f1cd5ca2b47c`,
  source `f8cf03f949aeb37203dccf67ad5ce7a306d7fe6d`.
- Before this documentation cleanup, development's complete source identity
  matched the checkout and both slots had zero manifest file mismatches.
  The existing October 23 report records 58 passing test groups, zero failures.
  These are existing results, not a new gameplay or headset pass.
- Documentation changes now make the checkout newer than the packaged docs.
  Do not rebuild merely to refresh prose; include them in the next planned batch.
- The source audit verified production callers and relevant test assertions.
  It did not launch the game, change bindings, or certify physical play.
- `AGENTS.md` contains the effective rules. Historical directions and reports
  retain provenance but cannot override newer rules or reopen a repaired subcase.

## Decisions already made

The fixed-length legs/pelvis solver, reach and wrist-isolation repairs, garment
drivers, reload support retention, tracking recovery, contextual-heading guard,
cover entry/exit fixes, transit body, left iDroid lifecycle/projection, Rocket Arm
steering, cabin paper/printing and ACC rifle grabbing are implemented on main.
Preserve them. FP13's free-roam route and FP14's sortie head are scoped resolved.

Concrete implementation limits: physical mounted aiming admits native actions
`0x46` and `0x47` only. The active source batch expands forearm-only cropping
from one sna5 layout to 20 exact owned outfit profiles; the combined build passed and the selected outfit was visually inspected.
Other outfits remain unreviewed. The previous sna5 cuff has scoped stereo review;
extreme full-body sleeve crowding remains a visible limitation.
The exact rock/corridor reports remain undiagnosed. Missing coverage for another
weapon, outfit or lesson does not establish a new defect.

Historical contribution refs are not outstanding feature branches to merge.
The asynchronous mailbox worker was withheld over native D3D11 synchronization;
the extra movement filter was not layered over the existing stabilizer. Preserve
those decisions and attribution; see [fork review](FORK_REVIEW_2026-09-20.md).

## Execution order

### 0. Documentation preparation — completed in this pass

Consolidate current rules and entry points; preserve old evidence as history;
record code state separately from acceptance; keep the release gate conservative.
No gameplay code, build, deployment, pin or publication is part of this step.

### 1. One bounded baseline investigation for the remaining unknowns

Use the existing checked development before editing working flows. Inspect the
effective personal bindings and current native state once, then select the
following scene groups. Reuse retained passing subcases; do not repeat ordinary
ACC toggles, both-rifle tracking recovery or binocular village marking just to
generate another pass count.

| Order | Scene group | Exact question to resolve | Required outcome before a code change |
| --- | --- | --- | --- |
| 1 | Body/arms, then eligible field cover | Does the current cuff/cross-head crowding obstruct ordinary use? Does full-body fallback retain hands/forearms without upper sleeves for the selected outfit? | Identify exact outfit, vertices/weights or admission failure; preserve native thigh/shin lengths and left bionic arm. Do not restore the startup-failing cuff experiment. |
| 2 | Mounted fixture | Why does native entry fail at the intact Mother Base gun, and what native action/contacts belong to each additional type? | Verify intact status, native interaction eligibility and actual mounting. The latest local handoff already reaches the adjacent platform and records failed rear-entry attempts. Do not repeat approach loops, infer `0x45`, or override Mother Base's lethal-weapon restriction. Use a genuinely usable alternative if the base gun is unavailable. |
| 3 | Reported terrain | Can the exact low-rock interruption and tight passage be repeated? | Record native actor/capsule/floor state versus camera/body in both travel directions and body modes. A bot displacement guard or blocked route is not itself a collision bug. If the location cannot be recovered, record that precise dependency and continue independent work. |
| 4 | Remaining progression/weapon gaps | Do forced iDroid lessons, bolt-action support, Fulton/climb headings or powered-arm impact actually fail on this candidate? | Identify native owner/state and visible failure before altering their working generic paths. No tutorial/save/progression bypass. |

Each case has a bounded deadline and neutral cleanup. A timeout without a new
diagnostic fact ends that attempt; resume only after a changed prerequisite or
specific diagnostic hypothesis. No supervisor loop or repeated destroyed-gun
entry. Missing hardware/scene is recorded as unproven, not a code defect.

### 2. One coherent correction batch

Prioritize body/head/arm presentation, then confirmed mounted/interaction issues
and any terrain or progression failure reproduced in step 1. Use the source
ownership map in FIELD_FIXES; extend existing implementations instead of adding
parallel rigs, optics or menu owners. Additional mounted types require confirmed
native action identity, authored contacts, native limit enforcement, release and
stick override. Broader outfit cropping requires source-bound geometry admission,
not arbitrary model hiding. Do not weaken collision to clear a route.

Keep every selected repair in one reviewed candidate. Run focused contracts
while editing where they test the failure; do not build/deploy after each small
subcase. If step 1 finds no justified gameplay change, use the existing fixes and
proceed to final source/docs packaging checks without inventing work.

### 3. Commit reviewed source/docs, build once, validate the combined candidate

Record actual outcomes in CURRENT/FIELD_FIXES/FREE_PLAY_ACCEPTANCE and reconcile
the five conservative catalog blockers with their exact evidence. Prepare the
release notes before the final source commit/build so the package includes them.
No source push or publication is implied by a local commit.

From the repository root:

```powershell
python tools/workspace.py status
.\tools\build.ps1
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
```

`build.ps1` builds/checks development only. `launch-sim` explicitly deploys it;
do not call `pin-build`. Run the selected bounded cases through `workspace.py bot`
with reviewed existing suites/policies. Do not pretend a named suite exists for
an unimplemented fixture; select its actual guarded case before dispatch.

| Combined review | Required behavior and adjacent transitions |
| --- | --- |
| Body and movement — FP04/05/06/17 | Standing/seated calibration and recenter; crouch/prone/walk; both-arm reach and wrist isolation; cross-head return; cover lean/peek/exit; full-body on/off and fallback; boarding/transit/disembark. Native thigh/shin lengths remain unchanged. |
| Combat and recovery — FP01/02/08 | Moving supported rifle, fire/visible impact, reload, bolt cycle, release/reacquire and weapon swap; valid idle and invalid tracking with no held-trigger discharge on recovery; Rocket Arm flight/impact/return and eligible Blast Arm case. |
| Mounted and contextual actions — FP03/11/15 | Each claimed mounted type: actual entry, either-hand yaw/pitch, limits, contact, operating-hand transfer, stick override, release, discharge/impact, exit. Vehicle entry/exit, deliberate driving turn, Fulton and table/sandbag climb at four starting headings; fixed HMD heading must not inherit forced actor alignment. |
| iDroid/progression — FP07/12/13/14 | Left-hand contact/projector and readable both-eye motion; ordinary and forced stow/immediate reopen; authored-scene return and exit notice; Development and nested dialog navigation without unintended purchases; sortie preview and native free-roam route; physical binocular inspection/zoom/acquisition/explicit marking preserved. Complete a naturally available mission through its objective and return; do not call a lesson a mission pass. |
| Cabin — FP09/10/11 | Both-hand paper/marker contact, write/type/pin, front-slot printing, stacking and cassette presentation; both native rifle assemblies carry/return and reject held-grip reacquisition across interruptions. Recheck already-passed cases only when the changed code affects them. |

Review final composition in both eyes and through transitions, not only endpoint
stills. Preserve exact candidate/run identity and failed cases. For sustained
claims, the existing catalog requires at least 180 seconds/five cycles per
feature and a 600-second integrated soak; a short slice cannot replace these.
Measure fresh XR submissions, repeats, stalls and actual runtime period with
unchanged per-eye recommendation/FOV. Capture FPS is not runtime performance.

Retain only reviewed evidence using `python tools/workspace.py pin` (run
retention, not build promotion). End with `python tools/workspace.py stop-sim`:
release inputs, restore poses/settings, close test-opened menus and owned
processes, restore Meta runtime; keep Steam running. A failed test does not
authorize leaving Pause, a terminal or a background harness open.

### 4. Physical Quest Link review and release decision

Launch the checked candidate through `Play-Dev.cmd` on wired Meta Quest Link.
The user performs physical reach/scale, seated/standing, wrist isolation,
handset readability, sight/contact, tracking recovery and comfort checks during
the combined gameplay flow. Simulator observations cannot close these reports.
Record the actual feedback against the exact candidate; do not repeat old
hardware feedback as acceptance of the new build.

Any new reproducible crash, progression lock, forced view rotation, lost hands,
stretched legs or broken established interaction blocks a claim that it is fixed.
Repair affected code and rerun its affected group; do not restart every unrelated
case. No promise of a same-day full-game certification: the mission ledger still
has no full-mission accepted passes across its 51 required missions/variants.

Present one release decision: delivered fixes, actual remaining defects,
unproven combinations, physical feedback and exact candidate. If unknowns remain,
the experimental release must disclose them. The existing `--allow-known-issues`
packaging option is not a default way to bypass review; a release with those
specific blockers needs an explicit decision on the concrete candidate.

### 5. Authorized packaging, publication and pin

Confirm the requested destinations and unused tag at release time. Proposed tag:
`experimental-2026-10-24`, only if it is still October 24 and that tag is unused.
Do not reuse the date automatically on a later day.

```powershell
python -B tools/regression_catalog.py --release
python tools/package-release.py --tag experimental-2026-10-24
```

Packaging requires clean committed source matching BUILD.json, passing checks,
matching public defaults, notes and complete payload hashes. If post-review docs
change the source identity, refresh/check development once before packaging and
verify whether runtime binaries changed; do not substitute a different DLL for
the reviewed one without appropriate revalidation.

Verify ZIP contents, source archive, launcher, licenses, default settings and
checksum; exclude owned assets, saves and private diagnostics. Publish only when
authorized to the confirmed server/GitHub destinations. Verify uploaded payloads
against the local checksum. A draft or source push is not publication completion.
Only then run `python tools/workspace.py pin-build` for that exact candidate.
Verify both Play paths and retain exactly `play/` and `build/dev/`; cleanup of
recognized obsolete builds uses `tools/clean-local-builds.ps1 -Apply` only after
both slots verify. Never remove settings, saves, source or retained evidence.

## Resume record

- Completed on main: documentation/source audit; previous body/head, garment,
  reach and interaction repairs; flat native cuffs with independent shading and
  correct repeated attribute-slot binding. Gameplay source `5ba674f`.
- Checked candidate `351d84672f0e`: 58 groups and installer checks pass. Both
  slots verify; published play remains October 19.1 (`b627c7cacc9a`).
- Combined affected review completed: nine cases in each body mode, including
  left iDroid stow/immediate reopen. Both final eyes reviewed. The rejected
  intermediate selector regression remains recorded; see CURRENT and the ledger.
- Native Afghanistan travel/fire diagnosis did not establish a terrain repair
  or actual intact anti-air mounting. Do not repeat combat-blocked approaches.
- Remaining: physical Quest feedback, extreme cloth crowding/other outfits,
  exact terrain and mounted/progression cases, sustained performance. Keep
  implemented fixes distinct from missing coverage; five conservative gates remain.
- Cleanup complete: full_body1, owned game/simulator/readers closed, Steam
  retained. Both Play commands/saved headset preference select Meta. Windows'
  global OpenXR registration is Virtual Desktop; an attempted Meta restoration
  was denied by registry permissions. See CURRENT for this precise limitation.
- Extreme-reach follow-up: the captured fixture requests106cm from a53.9cm
  native arm chain; dense comparison does not justify replacing its fallback.
  Current mesh penetration remains unproven. Do not repeat that impossible-pose
  test as evidence of a new tracking defect; see CURRENT.
- Next: process physical feedback on this candidate and diagnose a newly
  reproducible remaining failure before editing. No package/publication/pin or
  release-gate waiver. Outcome documentation may be newer than packaged prose.

- Sustained follow-up found a reproducible handheld stale-skin camera publication
  failure in cycle six. Source correction and stall/recovery regression are in
  this batch. Build completed: all 58 groups and installer checks pass. Retained
  combined run `20261025T044511277442Z` passes all 27 cases, including Pause and
  immediate reopening. The prior run stays failed. Candidate `a6dc5fb91151`
  is ready through Play-Dev. Next: physical Quest feedback and a newly
  reproducible field case; frame repeats remain measured and unresolved.

## Current execution checkpoint — 25 October, completed batch

Steps 1–3 below are complete: `d4b526e24cf5` is installed and ready through
`Play-Dev.cmd`. Continue with the user's play feedback; do not rerun this batch
or resume historical simulator loops. Remaining limits are in FIELD_FIXES.

1. Finish the existing code batch on main: rigid prosthetic shell, retained
   iDroid reopen camera/stereo, and forearm crop cache replacement. Preserve
   fixed leg lengths, native interaction mappings and exact outfit admission.
2. Commit code and this handoff, then run `tools/build.ps1 -SkipBootstrap -Deploy`
   once. Required build checks are part of that batch. Repair a concrete build
   failure if one appears; do not launch repeated gameplay/soak loops.
3. Leave the checked candidate installed and available through `Play-Dev.cmd`
   for the user. Keep Steam running, Meta Link selected, and all test sessions
   closed. Record exact build/deploy outcome in the ledger.
4. The remaining terrain/corridor location, actual additional mounted contacts,
   other outfits/extreme cloth, broader progression and physical acceptance stay
   explicit. Do not invent repairs or reopen already implemented work merely
   because acceptance is incomplete. Use the user's next gameplay feedback.
5. Publishing and changing the published pin remain separate from deployment.

This checkpoint supersedes the repeated simulator workflow in historical plan
sections above. The user wants accumulated code delivered as one candidate.
