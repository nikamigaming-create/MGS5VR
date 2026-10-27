# Field fixes — implementation and acceptance

### Cabin showcase attempts — 26 October

Multiple actual-game takes were attempted after the user requested a clear, human
showcase. A later side-angle take (`artifacts/opening-showcase-side-retry-20261026`)
completed at 30 fps with game audio but still obscured insertion behind the arm;
it was rejected as a finished demonstration. The source-eye recorder captured 30 fps with game-process audio;
this is pre-compositor footage, not final-headset video. The last complete
workbench take `artifacts/opening-showcase-close-20261026` observed cassette
seat/eject, camera printing with each hand, note editing and marker writing,
and photo/note board placement. Review rejected it as a finished showcase:
the player's insertion is obscured by hands/props and the motions do not read
clearly. The earlier reframe attempt stopped when an approaching left hand
grabbed a loose photo instead of the camera; it is a failed take, not proof of
a universal camera defect.

A separate native ACC take `artifacts/opening-showcase-body-20261026` recorded
full-body requested off/on/off with both final eyes sampled at each state. The
on state admitted the body; the off state showed arms. Both-eye review found
prominent forearm cut ends and incomplete body framing, so this does not certify
a polished headset presentation. The two recordings are separate takes and
must not be represented as one continuous scene or as a completed showcase.
The studio board and existing images matched their pre-test inventory exactly;
temporary photos were archived privately. Both user INIs matched their original
hashes after restoration, the owned simulator/game session closed and Steam was
retained. The checked development DLL did not change.

Next: improve the actual hand/prop view and visually demonstrate a clear
cassette insertion and eject, camera/photo/note handling and body switch with
sound; inspect the completed takes and both eyes before delivery. Physical Quest
and original native lid motion remain open.


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


26 October video follow-up: `fbc50f53c4d3` launched and accepted phantom-hand
steering, but the upright rectangle failed the user's requested wrist plane.
Retained take1 is a visual failure, not presentation acceptance. Revised source
mounts an oval, shallow-beveled display flat on the dorsal wrist and retains
real camera pixels and both-eye source ownership. Build and corrected video
pending; physical feel/impact remain open. Recurring Choose a Rival Help was
closed with native Back; its saved tutorial state was already FINISH. No broad
tutorial persistence repair is claimed. See CURRENT for retained runs.

26 October Rocket Arm clarification: the requested phantom LEFT hand gyro,
socket-fixed rectangular camera and additive joystick correction differ from
the previously accepted forearm-axis/head-facing presentation. The new batch
implements those controls and live screen/gyro/twist/width/sensitivity settings;
focused contract/editor checks pass. Checked/deployed `fbc50f53c4d3` / `3421214`
passes all 58 groups and installer checks; preservation outcome is in CURRENT.
Physical feel, final-eye motion/readability, impact and return on this revision
remain unaccepted; the earlier flight evidence is not relabeled as this revision.

26 October live-settings audit: all 64 gameplay settings are exposed, including
15 on/off options. Gear now places body/eyepatch visibility on its first page
and all switches ahead of numeric tuning; launcher descriptions match the dark
translucent patch. This is a usability change, not closure of any field report.
Checked/deployed `c5900adb1a22` / `72b3636` passes all 58 groups and installer
checks, including edit/save/live-apply for every switch in both directions.
See CURRENT for preservation checks and the personal headset shortcut. Actual
headset review, existing frame repeats and unresolved field reports remain open.

Updated 25 October 2026 with the incoming headset batch and scoped gameplay
results. The original production-path audit baseline was `5622052` (DLL `17b044cba72e`).

**Implemented is not accepted everywhere; acceptance pending is not missing code.**
The previous checklist mixed these states. Its exact run narratives and checked
subcases remain in [FIELD_FIXES_HISTORY_2026-10-23.md](FIELD_FIXES_HISTORY_2026-10-23.md)
and [FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json). No evidence was erased
and no broad report was closed by source inspection.

[CURRENT.md](CURRENT.md) is the handoff. [RELEASE_PLAN.md](RELEASE_PLAN.md) is the
ordered execution plan. All supplied community reports remain in scope.

## Latest incoming reports — 25 October

**Deployment movement:** user confirmed movement stopped after helicopter exit.
Centered-stick departure moved native player and camera on `1e50576a0e7d`; that
does not reproduce the user's complete lock. The source did latch a stick held
through exit because cabin transit changed axis ownership. Checked/deployed
`27e9108f2a58` preserves locomotion ownership across that transition, retaining
the passenger's native-walk mask and menu/focus protections. Held-stick smoke
passes: forward axes survive exit and Snake moves 0.4035 units horizontally
without centering. Retained `20261025T225945448637Z/held-stick-exit`; broader
physical confirmation remains open. See CURRENT for exact runs.

First-person eyepatch follow-up: `422594fb5ef8` renders Snake's
owned native patch and front strap close to the right pupil with gray transparency.
Actual both-eye on/off/on and head-motion pictures are retained under
`20261025T211153118499Z/on-off-motion`; see CURRENT for placement corrections and
the arrival fixture's no-op assertion. Physical headset fit remains user review.
Current checked/deployed `1e50576a0e7d` removes the reflection-like face-atlas
detail from that near-eye draw and changes gray 48% opacity to matte black 72%.

The new physical-play batch (sandstorm spray, arm twist/reach, Rocket Arm socket
display, mounted machine-gun response, helicopter/D-Walker centering) is implemented
together on main; `f58ef4224995` was the preceding checked candidate. Scoped gameplay
checks pass D-Walker center/facing, helicopter exit, responsive MG grip aiming,
actual Rocket socket steering/screen, arm rotations and world-preserving storm
suppression. See CURRENT for retained runs, repaired failures and exact limits.
The optional character-model eyepatch toggle is in this batch; retained native
face captures confirm on/off/on appearance for the selected Snake face. Physical
Quest acceptance and other face variants remain pending. Native prone-entry eligibility,
all mount types and actual field-pickup boarding are not certified.


Previous playtest candidate: `62f696613e2b` / `5bb471f`. It adds native-terminal
stow routing while Help suspends camera presentation, retained exact-owner Help
acknowledgement, and consumption of the entire handheld Menu press. Forced-Help
stow/reopen passes retained 20261025T152037198735Z. It includes the on-board-to-field ownership
reset and Challenge Tasks mode16 post-stow Help cleanup.

- Sleeve pop during motion: a bounded same-owner camera/skin visibility continuity
  repair is deployed as a33bfd559b9e; scoped moving review passed. This does not close extreme
  cloth crowding or every outfit.
  The retained synthetic cross-head case asks for 1.069 m of shoulder-to-wrist
  reach from 0.539 m of native arm length. It cannot serve as a reachable-arm
  regression baseline; keep the current physical ordinary-motion report separate.
- Open iDroid while boarding field pickup: native passenger stow and deliberate
  on-board reopen are deployed with passing lifecycle cases and flight/field
  open/stow/reopen observations. Actual pickup boarding
  remains unverified because the current helicopter fixture did not land.

## Current candidate — 25 October

The active source batch expands exact forearm profiles to 20 owned outfits,
repairs paused-part cache lifecycle, separates mounted body/eye admission from
generic action input, and invalidates mounted aiming across action changes.
The moving-only intrusion is isolated to the stowed back weapon. Candidate
`1933aa81fc8e` did not fix it; the hip-stock hypothesis is reverted. The source
now conceals the owned stowed back assembly from main view and restores it on
draw/manipulation or leaving field presentation. Candidate `da4ae29de73b` /
`7273c25` also preserves concealment through camera-to-rig publication gaps.
Moving source-eye review and both-eye back-rifle draw review pass their scoped
visual checks; see CURRENT for retained runs and movement guard failures.
The terrain report now has
a regression clue (it began after the first-person rig change), not an exact
location or confirmed cause. These edits do not close FP04/FP16 or whole-game
and physical-headset acceptance.

Earlier candidate `d4b526e24cf5` / source `32f2ab0` included
the apparent third-arm correction, complete iDroid reopen pose/stereo handoff,
and recovery of forearm cropping after native object-buffer replacement. All
58 groups and installer checks passed; the user tests through `Play-Dev.cmd`. The cache fix
preserves exact outfit admission and is not all-outfit acceptance. Earlier
failed/interrupted runs remain failed; no repeated simulator soak is planned.

Candidate `a6dc5fb91151` / source `f11d0d0` adds the reproduced handheld
stale-skin correction. It passes 27 combined sustained/menu/reopen cases and
58 test groups plus installer checks. The earlier extended run failed and is
retained. Current frame repeats, physical Quest and wider field coverage remain
open; see CURRENT for measurements and exact scope.

## Body/arm candidate update — 24 October

The body/cuff batch `351d84672f0e` / source `5ba674f` flattens cuff rims,
separates cap shading and handles repeated native attribute-slot bindings.
Both body modes pass nine combined reach/return/left-iDroid/stow/reopen cases.
Both final eyes show the forearm-only crop with flat closed caps; the previous
`250884f4c156` selector regression is retained as a rejected intermediate result.
Extreme full-body cloth crowding, other outfits and physical Quest remain open.
See CURRENT and FREE_PLAY_ACCEPTANCE for exact scope and cleanup.

Candidate `1a64bb965bc6` / source `4392536` adds upper-sleeve clearance along the
whole upper arm and closes supported forearm-only cut loops using existing
native indexed geometry. Full-body reach/return and all seven forearm-only
reach/iDroid smoke cases have scoped results; native help/Daily Bonus interrupted
two earlier menu suites, whose failures remain recorded. See CURRENT.
That intermediate build's rough cuff rim and shading were corrected by the
later body/cuff batch above. Other outfits and physical Quest remain open;
there is no blanket FP17/body closure.

## Code audit and remaining scope

| Report | Implemented on main | Actual remaining work |
| --- | --- | --- |
| FP01 — idle/tracking camera | Runtime-valid inferred controller poses, complete visual hand-pose retention and owner/reference resets in [xr_runtime](../src/xr_runtime.cpp) and [arm_ik](../src/arm_ik.cpp). | Original physical idle-camera fallback was not reproduced in SIM. Verify physical idle/occlusion/recovery; do not invent a new camera fix. |
| FP02 — weapon/support loss | [controller_rig](../src/controller_rig.cpp) retains acquired weapon-relative contact and same-weapon muzzle calibration through reload; support fingers cup outside native manipulation. [WeaponTrackingContinuity](../include/mgs5vr/input_bridge.hpp) retains held readiness and requires fresh trigger release after tracking recovery. | Bolt cycle, continuous stereo reload and physical handling. General rifle movement/fire/reload and both primary-slot loss/recovery already have scoped evidence. |
| FP03 — forced contextual rotation | [VehicleViewHeading](../include/mgs5vr/vehicle_view.hpp), called by [head_camera](../src/head_camera.cpp), captures pre-Y/normal-action heading, tracks actual carrier turns and preserves exit yaw. | Four starting headings across Fulton/vehicle/table/sandbag actions, deliberate driving look/turn and physical seated/standing. Jeep and automatic ledge fixes are already implemented. |
| FP04 — corridor/body size | Fixed-length body, root-relative low-posture camera and cover-specific corrections exist. | Exact reported tight corridor remains undiagnosed; compare native capsule versus visible body with body on/off. No targeted corridor repair established. Do not weaken collision. |
| FP04 — cover | [native_cover](../src/native_cover.cpp) suppresses only automatic ready-to-peek movement, releases native cover when assistance is off, and preserves native readiness elsewhere. [input_bridge](../src/input_bridge.cpp) retains on-foot weapon routing; [head_camera](../src/head_camera.cpp) preserves low-posture footprint. | Wider surfaces/postures and physical exposure/native benefits; original figure identity unresolved. Do not diagnose restricted Mother Base lethal weapons as missing rendering. |
| FP05 — floating | [head_camera](../src/head_camera.cpp) lets helicopter-exit height settle; [arm_ik](../src/arm_ik.cpp) and [controller_rig](../src/controller_rig.cpp) preserve native leg lengths and solve shared pelvis/head/floor reach. | Physical standing/seated calibration and transitions. Scoped exit/posture corrections already passed; no leg scaling permitted. |
| FP06 — helicopter body | Mounted-body admission in controller_rig and carrier-relative seat/boarding fit in head_camera preserve native pelvis/legs and tracked upper body. | Extraction boarding and broader transit/headset continuity. Seated/disembark body repair is implemented, not an entirely open coding task. |
| FP07 — left iDroid | [controller_rig](../src/controller_rig.cpp), [idroid_attachment](../src/idroid_attachment.cpp) and [idroid_rig](../src/idroid_rig.cpp) use left cinematic grip/fingers and native connector/projector frames; [effect adapter](../src/idroid_effect_probe.cpp) fits the native cone. | Continuous both-eye readability/contact and physical orientation across configurations. Ordinary front/turned/stow observations do not certify all motion. |
| FP07/12 — stow/tutorial/camera | [native_actions](../src/native_actions.cpp) handles native stow ordering, close-input ownership, completed-guide cleanup and retained terminal scene handoff; head_camera handles retired-player/exit-notice ownership. | Forced unfinished lessons, earlier-account configurations and wider nested dialogs. Ordinary toggles and repaired exit notices already have scoped evidence; preserve native progress. |
| FP08 — Rocket Arm | [RocketArmSteering](../include/mgs5vr/rocket_arm.hpp) and xr_runtime steer from the left hand with stick override; render_camera/idroid paths retain flight view and handset. | Actual target impact, Blast Arm, wider contact/poses and physical review. Steering/display/return code exists. |
| FP09 — paper/pen | [cabin_studio](../src/cabin_studio.cpp) and controller_rig fit marker shaft/thumb-index contact and paper-edge pinch, with writing/typing/board pinning. | Wider hand orientations and prompt readability. Both-hand scoped contact/write/pin results retained. |
| FP10 — gallery/workbench/cassettes | cabin_studio retains front-slot prints, uses field-paper poses/rays and settles sheets flat; dropped markers use rotated extents. Physical cassette handling remains in [cabin_props](../src/cabin_props.cpp). | Wider stacking/cassette presentation and physical interaction; existing combined workbench passes are retained. |
| FP11 — ACC loadout holsters | controller_rig grabs existing native assemblies with either rendered palm, preserves native publication, returns exact mounts and requires fresh grip after interruptions. | Other loadouts and physical contact. Both rifle assemblies and both-hand interruption subcases already have retained coverage. |
| FP11/15 — mounted physical controls | controller_rig explicitly admits mortar `0x46` and machine gun `0x47`; [mounted_grip](../include/mgs5vr/mounted_grip.hpp) provides native-contact grab, aim servo, operating-hand ownership, release and stick override. | Additional types lack admission in this adapter. Confirm native mount/action/contact before extending; do not assume `0x45`. Mortar pitch/contact/impact and complete MG impact/contact remain unaccepted. |
| FP15 — mortar viewpoint | head_camera retains native mounted operator eye; controller_rig admits mounted body without forcing standing posture. | Wider headings, contact, translucent geometry, firing/impact and physical review. The 62.8 cm standing-eye mismatch is already repaired. |
| FP16 — small rocks | Native floor/posture/body support exists, but no targeted repair for the exact reported rock interruption was established by the audit. | Locate/reproduce in both directions; join floor, velocity, action and eye height. A route guard is not a diagnosed production defect. |
| FP17 — wrist coupling/reach | Acquired-supported-weapon-only torso correction, native clavicle reach, full arm/per-eye clearance and infeasible cross-head fallback in controller_rig/arm_ik. Garment driver translations follow solved arms. | Physical wrist isolation/reach and remaining sleeve crowding. These corrections exist; do not restart bare-wrist torso or cross-head-freeze implementation. |
| Latest body fallback | [forearm_visibility](../src/forearm_visibility.cpp) is connected to production draw hooks and hides upper sleeves for 20 exact admitted native outfit layouts when full body is off/unavailable. Native thigh/shin lengths stay fixed; garment branches follow solved joints. | Admission remains exact and outfit-specific; the 20 profiles expand implementation coverage, not live acceptance. Supported cuff loops have flat independently shaded caps and native attribute-slot aliases are handled. Other outfits/ambiguous boundaries and physical review remain open. The earlier startup-failing reconstruction remains excluded. |
| FP13 — free-roam route | Existing VR menu path: Missions -> Mission List -> Free Roam; [controls](CONTROLS.md) corrected. | Scoped resolved through on-foot arrival. No unlock/save modification or extra root menu required. Broader progression remains FP12. |
| FP14 — sortie head | [player_visibility](../src/player_visibility.cpp) restores native preview visibility and first-person concealment afterward. | Scoped resolved in development; preserve during adjacent transit/body review. |
| FP12 — remaining regressions | Physical binocular raised/marking events and native intel requests exist in [native_actions](../src/native_actions.cpp)/[optic_markers](../src/optic_markers.cpp); retained village marking advances the native lesson. Other systems remain inventoried in [GAMEPLAY_FEATURES](GAMEPLAY_FEATURES.md). | Full missions, other weapons/items/buddies/menus and historical reports need exact-build reconciliation. A lesson or source path is not a whole mission pass. |

## Next batch

Mounted adapter investigation, 25 October: owned `TppTutorial.lua` maps native
`OnPlayerGatling` to the `ANTI_AIRCRAFT` guide. The retained native emitter is
RVA `0x126d50b` inside the handler beginning at `0x126c570`. This is the event to
correlate with the next real anti-aircraft mount. It does not establish its
player action ID. The separate `0x45` writer belongs to another handler and has
not been connected to that event. Action `0x44` is associated with the verified
`OnPlayerSearchLight` handler; do not admit it as a stationary weapon.
The current adapter remains limited to the verified mortar/MG IDs. Next C++
work is to admit the anti-aircraft action only after correlating actual mounting,
its current native palm contacts and aim pose, then exercise both operating
hands, release, native aim limits and impact together. No candidate change or
new mounted acceptance is claimed by this offline investigation.

1. Prioritize the remaining body/arm presentation limits without undoing the
   fixed-length and tracked-garment repairs.
2. Diagnose intact mounted entry/native eligibility, then extend only confirmed
   missing type adapters. Use retained routes; do not repeat destroyed fixtures.
3. Reproduce exact rock/corridor and any newly observed progression failure.
4. Group justified repairs, build/check once, then review the changed groups and
   adjacent interactions together with physical Quest feedback.
5. Publish only the reviewed scope under RELEASE_PLAN's package/pin sequence.

Do not mark all rows complete from unit checks. Do not describe all rows as
unimplemented because hardware or breadth acceptance remains open.
