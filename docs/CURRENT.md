# Current MGS5VR work — 26 October 2026

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


## Oval wrist display follow-up — 26 October

The requested final-eye video reproduced the mounting mistake on `fbc50f53c4d3`:
the rectangle stood at the socket end instead of lying on top of the wrist.
Retained continuous take `artifacts/rocket-proof-20261026/take1` shows real launch,
phantom-hand pitch/yaw/twist, translation, joystick input and native return, but
is rejected for the requested presentation. Left/right capture cadence is only
3.38/3.33 fps; it does not establish smooth headset motion or impact.

The user clarified a dorsal wrist plane and then requested an OVAL with a shallow
beveled rim. The revised display follows the socket rigidly, lies 25 mm above its
centerline and extends forward from its near edge. A 64-segment oval aperture
crops camera corners without stretching the feed; a graphite three-slope rim
adds depth. This replaces the previous rectangle contract, not its history.
Build/deploy and a corrected bounded video are pending. Preserve left-hand gyro,
additive joystick and live settings. Physical headset acceptance remains open.

The route also reproduced recurring Choose a Rival Help. Native FOB tutorial
state is already FINISH (127); ordinary B/Back dismisses it and deployment then
completes. Retained failed confirm route `20261026T161740178213Z` and successful
Back dismissal `20261026T161941318842Z` distinguish Help from lost tutorial save
progress. Persistence of all tutorial notices remains unresolved; no save flags
were changed. Landing/selection route `20261026T162112602837Z` is retained.

## Rocket Arm requested interaction — 26 October

The user clarified that the LEFT controller remains the missing hand: wrist
rotation controls guidance, the screen attaches to the socket, and joystick
correction works alongside physical steering. Baseline `c5900adb1a22` did not
implement that exact contract: it discarded independent wrist rotation, used
stick override/rebasing, and floated a head-facing screen 12 cm above the wrist.
The earlier `f58ef4224995` steering/display evidence retains its original scope;
it cannot accept this new behavior.

The new batch fixes the rectangle's lower edge to the solved left wrist endpoint,
with screen up along forearm dorsal and its normal back along the forearm. It
keeps native camera aspect and both-eye source-transaction ownership. Guidance
uses that publication's solved phantom LEFT palm rotation at the socket; yaw,
pitch and optional wrist twist drive native two-axis steering. Joystick correction
adds within native bounds without recentering the hand. Pure socket translation
moves the screen without injecting angular steering. Real tracking loss, flight
exit/menu ownership and gyro-off clear steering; short publication gaps still
suppress input while preserving the neutral for at most 150 ms.

New live options: Rocket Arm screen, screen width (10–35 cm, default 20), left-hand
gyro, wrist twist, and gyro sensitivity (0.25–4, default 2). The three switches
default on. There are now 69 gameplay settings, including 18 switches.
Focused core/editor checks pass, including rigid rectangle attachment, resizing,
phantom-wrist motion, stick blending, reference/loss resets, all-switch save/live
round trips and fractional sensitivity adjustment. The first combined run passed
57/58 groups; the launcher bridge test retained a hardcoded 64-setting count and
rejected the new 69-row schema. Deployment did not run. That expectation is
updated with explicit Rocket option/default/range checks; failure log retained
as `artifacts/rocket-socket-build-failed.log`.

Checked/deployed **`fbc50f53c4d3` / source `3421214`** passes all 58 groups and
installer checks in `artifacts/rocket-socket-build.log`. Installed DLL and both
slot manifests verify. Personal controls/runtime files are byte-identical and
the published manifest is unchanged (`artifacts/rocket-socket-preservation.json`).
64-bit Meta Link CheckOnly passes, Steam is retained, and no game/simulator or
test input was started. No obsolete build targets remain. Outcome prose is newer
than the packaged docs; no runtime source changed after this checked build.

No new in-game/final-eye or physical headset acceptance is claimed. Follow the
latest delivery direction: one checked deployment for the user's headset review,
without restarting per-case simulator loops. Existing field limitations remain.

## Live settings and headset handoff — 26 October

The settings audit found all 64 gameplay settings exposed in the headset Gear
editor and launcher, including 15 on/off options with production consumers.
Eyepatch already controls native model visibility and the first-person patch
live; the retained on/off/motion evidence below remains its visual scope.
Gear now puts Seated mode, Full body and Eyepatch immediately after Immersive VR,
then the other toggles before numeric fitting. Launcher copy describes the
current matte dark patch and explains body/cinematic options. No new gameplay
or rendering repair is inferred from this usability change.

Personal binding audit: hold BOTH stick clicks for 650 ms opens the editor;
select Gear, toggle, check to save, then release buttons/grips/triggers and center
sticks. Allow about two seconds for the file poll. Runtime/graphics setup still
requires a restart. Saved eyepatch=1, full_body=0 and seated_mode=1 are preserved.
Checked/deployed development: **`c5900adb1a22` / source `72b3636`**. All 58 groups
and installer checks passed in `artifacts/live-settings-build.log`. The editor
check passed every gameplay switch through save and live reload in both
directions, held-trigger deferral, and personal-binding retention. Both slots
verify; installed DLL matches development. Personal controls/runtime files are
byte-identical and published play's manifest is unchanged (receipt:
`artifacts/live-settings-preservation.json`). Meta Link CheckOnly selects the
64-bit Oculus runtime. Steam is running; no owned game/simulator was launched.
No obsolete build targets remain. Use Play-Dev for the user's physical review.
This outcome prose is newer than the packaged docs; runtime source is unchanged.

The previous deployed DLL is `27e9108f2a58`. It includes the held-stick exit repair;
the user's broader physical movement report remains open. Remaining concrete
scope: undiagnosed exact rock/corridor snags, physical arm fit/extreme cloth and
other outfits, additional mounted adapters beyond verified mortar/MG, actual
field-pickup boarding, wider progression/weapon and headset/performance coverage.
Previously measured frame repeats remain a performance limitation; no fresh
performance or final-eye claim is made for this settings-only change.
Do not describe these as universally fixed. Next is the user's Play-Dev headset
review, without restarting simulator/soak loops. Published play stays pinned.

## New blocker: movement unavailable after deployment

User reports "we couldn't move once deployed" after the eyepatch review.
User confirmed this happens after getting out of the helicopter. Prior disembark
acceptance did not include a subsequent walking segment. The centered-stick
route on `1e50576a0e7d` clears passenger mode and moves both the native player
and camera: retained route `20261025T225131890400Z`, movement
`20261025T225445132506Z`, follow-up `20261025T225603602110Z`. The movement probe
failed its 0.75-unit envelope at 0.7635 units, not a no-progress check; do not
label that run a pass or the user's lock resolved from it.

Source diagnosis: the cabin-transit bit changed stick ownership on disembark,
latching any held stick off until centered. Candidate `27e9108f2a58` removes
transit from that identity; the existing passenger mask still prevents native
walking aboard, and ACC/menu/loading/focus protections retain their owners.
Build and installer checks passed; deployed development only. Retained run
`20261025T225945448637Z/held-stick-exit` passes the targeted held-stick disembark:
effective movement stick held at 0.45 before exit, native forward axes remain
nonzero after passenger mode clears, and horizontal displacement reaches 0.4035
units without a centering cycle. Inputs are zero after release. Both final-eye
captures retained; owned simulator closed, Steam retained and Meta Link restored.
This verifies the held-stick latch repair; the user's physical session has not
been replayed. No collision or rig geometry was changed.

## First-person eyepatch — current picture request

The user clarified that the eyepatch must be visible from inside VR, gray and
translucent so both eyes remain usable. External model visibility did not satisfy
that request. Development `422594fb5ef8` added the exact owned face0 mesh20
(142 vertices / 137 triangles, patch and strap) with its original UVs and native
face diffuse texture. A gray transparency treatment is applied only to this
first-person draw. No generated substitute shape is used. The existing eyepatch
setting controls both presentations.

Current candidate `1e50576a0e7d` addresses the user's reflection/darkness feedback:
the first-person shader no longer samples the face atlas's baked detail. It uses
constant matte near-black at 72% opacity (previously gray at 48%). Native geometry,
stereo placement and the character-model appearance remain unchanged. Build and
installer checks passed in `artifacts/eyepatch-matte-build.log`; development is
deployed and the published pin is unchanged. Fresh simulator pictures and an
explicitly illustrative binocular composite accompany this material revision:
retained run `20261025T212952693176Z` arrived in the cabin and captures both eyes.
`eyepatch-matte-stereo.png` preserves the eye images; the separate
`eyepatch-binocular-approximation.png` blends them 50/50 after registering the
cabin background. That edited illustration is not a prediction of human fusion
or stereo evidence. The actual eye images show dark translucent native geometry
without the earlier baked atlas detail. Owned simulator closed and Steam retained.

The same captured head/right-pupil pose and the enclosing render FOV project the
surface before the runtime's per-eye crop, including clipping straps that extend
behind the eyes. Native scale is retained; the patch is close to the right pupil,
with its edge and front strap visible from the left eye. The rest of the
native head remains hidden. The importer obtains the exact model/texture from
the user's game archives; those assets are not included in the mod package.
All 58 build groups and installer checks passed; published play is unchanged.
Actual simulator pictures are retained in `20261025T211153118499Z`, including
`eyepatch-vr-on.png` and `on-off-motion/`. On/off/on removes and restores the
patch; the head-translation/yaw capture retains its eye-relative placement.
World remains visible through the gray patch. Physical headset distance and
comfort remain for user review. The earlier `20261025T210608859981Z` capture
exposed mirrored placement and wrapping straps; the current candidate corrects
the imported X basis, render-FOV projection and rear-strap clipping. The latest
arrival bot's no-op outcome assertion failed after arrival; its screenshots are
retained as pictures, not a passed arrival test. The separate toggle/motion
capture completed. Owned simulator closed, eyepatch=1/full_body=0 retained,
Steam retained and Meta Link restored. Use Play-Dev for this candidate.

## Earlier incoming headset batch — 25 October

The latest batch fixes the reported sandstorm camera spray, forearm twist/reach,
Rocket Arm socket display/guidance, mounted machine-gun response, and helicopter/
D-Walker centering. It also adds the default-on character-model eyepatch option.
All changes are on main. Published `play/` is unchanged.

Previous checked/deployed candidate: **`f58ef4224995`**. Build and installer
results: `artifacts/release-playtest-final-build.log`. Use `Play-Dev.cmd` for
physical review. The owned test session is closed, test input released, original
`full_body=0` restored, Steam retained, and per-game Meta Link restored.

### Delivered gameplay changes

- D-Walker mounting now recognizes native ridden type 44 even when the engine
  leaves ON_VEHICLE/ON_HORSE false and retains the entry crouch flag. Native seat
  eyes/body and mount-forward tracking replace on-foot placement. Entry clears
  positional origin, snap pivot and cached view; subsequent physical leaning
  remains available. This does not add car pedals to D-Walker controls.
- Helicopter deployment clears virtual cabin walking and positional tracking;
  mounted placement no longer depends on full-body visibility or on-foot height.
- Stationary machine-gun aiming no longer feeds rotating native player-root yaw
  back into the VR viewer. Physical grip response is increased while preserving
  native limits, joystick override, and separate mortar tuning.
- Rocket Arm has no borrowed iDroid. A 20 cm guidance display sits 12 cm above
  the solved left wrist endpoint in gravity-up and faces the viewer. That same
  forearm socket axis steers the projectile; wrist roll does not redirect it.
  Short camera/skin publication gaps emit zero input while retaining the neutral
  reference up to 150 ms. Real hand tracking loss clears it immediately.
- Arm solving uses authored bind segment lengths in both visibility modes and
  bounds/unwraps corrective skin twist; tracked palm contacts and native leg
  lengths remain intact.
- Suppress only the exact native sandstorm camera emitter, preserving world
  weather. Eyepatch visibility affects reviewed native face0/3/4 patch geometry,
  not either eye's VR image or character save data.

### Gameplay results and limits

| Area | Retained run | Result |
| --- | --- | --- |
| D-Walker entry/exit | `20261025T184019469888Z` | Crouched, turned approach with -20 cm room-scale offset centers on native seat; view/root yaw both 2.96872 degrees. Native prone entry eligibility is not established. |
| Mounted body/lean | `20261025T181031988326Z` | Down/left/right/lean phases pass; native seat and authored legs retained. |
| Machine gun | `20261025T184154219366Z` | Four 12-degree hand requests produce 11.40–11.46 degrees native gun movement, zero induced viewer yaw; both-hand grip, stick override and release pass. Firing/impact was outside this case. |
| Helicopter | `20261025T183116920712Z` | Off-center sortie disembark passes. This is not field-pickup boarding acceptance. |
| Rocket Arm | `20261025T185436979277Z` | Screen visible above socket in both final eyes; actual projectile elevation changes +3.43 degrees and left yaw +12.42 degrees from arm movement; stick override and native return pass. |
| Arm presentation (historical; raw run no longer retained) | `20261025T174010920773Z`, `20261025T180234717411Z` | Reviewed rotations in forearm-only and full-body modes. Extreme cloth/all outfits remain open. |
| Sandstorm (historical; raw run no longer retained) | `20261025T175335820470Z` | Exact suppression hook exercised; world storm remains visible. Temporary weather request cancelled. |

Final gameplay checks span the batch and targeted repairs: c219d25e7567 covered
mounts/MG; 91ee741d93a1 covered the repaired Rocket steering continuity. The final
build additionally rejects cached socket steering immediately on raw tracking
loss, using the already tested immediate-reset branch. The whole suite was not
replayed after that guard. Build checks do not substitute for physical review.

The retained failed MG run `20261025T181213677630Z` exposed roughly 75-degree gun
response and rotating viewer; the root-yaw feedback repair corrected it. Rocket
run `20261025T184458811736Z` showed the screen but intermittent hand guidance;
its procedural flight completion did not establish steering. The final run
above checks actual projectile angle response. These failures remain retained.

Eyepatch follow-up `20261025T190403353416Z` verifies on/off/on configuration
round-trip and live settings application. Reviewing the retained
`model-toggle-turn/view-2` face pixels resolves the earlier whole-frame
inconclusive assessment: the right eyepatch and strap disappear when disabled
and return when enabled, while the face and bandanna remain. The labeled
`eyepatch-face-review.png` magnifies those original pixels 5x and increases
brightness 4x for inspection; unmodified source images remain alongside it.
This covers the selected native Snake face in ordinary native-camera
presentation, not every face variant or physical stereo acceptance. The later
close-up operator timeout remains a failed attempt and does not invalidate the
already captured toggle. Eyepatch=1, full_body=0 and Meta Link are restored;
the owned session is closed. Candidate code and installed DLL are unchanged.

Retention audit: the original arm/storm run directories 174010920773,
180234717411 and 175335820470 are no longer present. Their run logs and this
handoff preserve the observations made during the batch, but they must not be
cited as currently retained image evidence. The mount, MG, final Rocket and
eyepatch runs listed above are present and retained. Do not repeat completed
working flows solely to reconstruct missing archives or inflate coverage.

This named repair batch is implemented, built, deployed and smoke-tested;
follow-up findings in MG aiming and Rocket guidance were fixed. Physical Quest
comfort/arm behavior, actual iDroid-open field-pickup boarding, exact rock/corridor
reproduction and broader progression/weapon coverage remain open in FIELD_FIXES.
Do not claim every mount, outfit, mission or physical headset has passed. The
next step is the user's Play-Dev review and targeted repair of actual findings.

## Previous playtest candidate — native terminal Help stow

**`62f696613e2b` / source `5bb471f` is built, checked and deployed for Play-Dev.**
All 58 groups and installer checks passed in
`artifacts/release-help-terminal-build.log`. Retained
`20261025T152037198735Z` passes Challenge Tasks Help Menu stow, immediate
reopen and restow, following normal ACC sortie/disembark. Both final stow eyes
show the menu cleared. Reopen captures straddle screen allocation; they do not
establish continuous stereo readiness. Published play is unchanged.

The owned game/simulator are closed, inputs released, Steam retained and the
per-game Meta Link preference restored. `Play-Dev.cmd` is ready for the user's
physical test; `Play.cmd` retains the published release. The release goal remains
open for exact terrain/corridor reproduction, extreme cloth crowding, actual
field-pickup boarding and the broader field/headset scope in FIELD_FIXES.

The retained synthetic cross-head sleeve case is not a reachable-arm baseline.
Reconstructing the requested wrist through the production tracking transform and
the sampled palm-to-wrist frame gives 1.069 m from its fitted shoulder, versus
0.284 m upper arm plus 0.255 m forearm. The solved wrist is 0.616 m from that
request. `20261025T145002391598Z/sleeve-reach-constraint.json` retains the values
and the roughly 7 ms palette/observation separation. The arm solver and tracking
transform are unchanged between that run and the current candidate. Do not tune
clearance from this impossible reach or treat it as an ordinary-motion failure.
The physical arm-pop report still needs feedback on the current candidate.

Pickup follow-up cleared the arrival point through native walking
(`20261025T152529456360Z`), but the called helicopter stayed hovering
(`20261025T152644554491Z`). It also hovered in the bounded native-camera comparison
(`20261025T153018416570Z`); immersive presentation was restored. The return route
(`20261025T153124762503Z`) stopped against the parked jeep, visible in the final
eye capture. This is not a terrain-hang diagnosis or boarding acceptance.
The first comparison fixture was rejected before toggle input because it omitted
the required target presentation; that failed record is retained too.

### Reproduction and correction

Candidate `4d5dbdf92e06` / `e842c7c` passed required build checks but retained
`20261025T151404474920Z` reproduced a remaining Challenge Tasks Help stow failure.
Its Menu diagnostic reports `live=0`, `ui_only=1`, tutorial 16 while native
terminal state remains open. Help suspended the camera's iDroid presentation;
using that flag skipped the stow branch entirely. The follow-up uses actual
native terminal state for Menu, held-Back recovery and deferred close. It does
not broaden the admitted Help owners. The failed run is retained; owned game
and simulator were closed, Steam retained and Meta restored before rebuilding.

The field UI-only Menu/held-Back request previously reached the one-shot native
close mailbox while Help still blocked terminal closing. The source now uses
the existing exact-owner Help acknowledgement before retaining and delivering
terminal stow for a field handset, as it already does for the cabin. A handheld
Menu press also remains consumed until physical release, so closing its terminal
cannot turn the rest of that same press into native START. Unknown Help owners
and unrelated native pauses remain excluded. This extension now has the scoped
live Help-to-stow/reopen result above.

## Previous development candidate — lifecycle follow-up

**`994c6d57762a` / source `3fc9cdc` is built, checked and deployed for Play-Dev.**
All 58 check groups and installer checks passed in
`artifacts/release-lifecycle-followup-build.log`. Retained
`20261025T150300810232Z` passed fresh flight opening, disembark with the handset
still open, and field stow without a leftover menu. Runtime logs confirm the
on-board-to-field ownership reset. The sequential eye captures straddle the
exit animation and differ in scale; this is not continuous stereo acceptance.
The first route attempt was rejected by a contradictory private scene predicate
before disembark input; its failure is retained. Challenge Tasks mode 16 cleanup
has contract checks and the retained pre-fix reproduction; its new live tutorial
path is not yet accepted. Game/simulator are closed, Steam retained, Meta Link
restored and published play unchanged. Play-Dev is ready for physical testing.
Two related corrections are included. A handset deliberately
opened on a helicopter returns to field ownership after a confirmed on-foot exit;
the next pickup can therefore stow it instead of inheriting a permanent exemption.
Challenge Tasks tutorial mode 16 now participates in the existing exact-owner
post-stow Help cleanup. Retained 20261025T143421433785Z reproduced its Exit Tutorial
notice remaining after terminal stow; 20261025T143543140256Z acknowledged it manually.
The correction does not mark a lesson complete or modify save/progress flags.

Current sleeve diagnosis: retained 20261025T145002391598Z joins read-only native
palette samples with the owned sna5 mesh and stereo eye observations within 8 ms.
The nearest sampled right-sleeve vertex is about 9.8 cm from the right eye in the
extreme cross-head pose (about 22 cm neutral). It is entirely elbow-weighted.
This does not establish eye penetration or justify increasing clearance blindly;
full-body occlusion in extreme poses remains a presentation limitation. The
reach reconstruction above identifies this particular fixture's infeasible
target; it does not establish ordinary-use crowding. The native
animation channels are restored after the skin hook; no movement-capsule write
was found in the rig/camera audit. Exact terrain failures remain unlocalized.

## Previous development candidate — arm/pickup batch

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
