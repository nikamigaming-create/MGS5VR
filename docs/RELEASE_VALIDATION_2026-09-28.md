# Release validation in progress

User authorization: fix and test the release candidate, including Missions 1
and 6 and every supplied Coco adaptation. Publication requires the user's
later instruction. Preserve the VR interaction mandate in `AGENTS.md`.

Starting candidate: RC4, DLL SHA-256
`1b19dac0e86b021e4f14d876eb48150460a47a3f80cedf021e73fa91cee029dd`.
Evidence for this pass lives in `artifacts/release-validation-20260928`.
Historical captures keep their original binary identity.

Active local test DLL: `c69b75c6099d77d773b6503caee339c51ae33411d3c65062c2b0ba28c51ca79e`.
It queues real physical binocular-use and waypoint notifications through
`Mission.SendMessageToSubscribers`, and adds the VR X intel request against the native
registered radio volumes/person targets. Mission progression remains owned by
the retail scripts. No native binocular camera, extra display or sequence
override is used. Release acceptance remains incomplete; publication is not authorized.

Automated checks for this work: all 35 CTest groups passed, including 7,324
core checks and 291 control checks. All 102 native-presentation Lua checks
passed in the retail Lua VM. Coco's integrated contract checks are included.
The earlier `7f01499c...` attempt called the Lua message handler directly and
lost its native resend request; the actual lesson remained blocked. The
`257bed47...` attempt failed a new radio byte-signature check and disabled its
marker adapter. Both failed runs are retained. The current build corrects the
signature and keeps marker initialization independent of radio initialization.
One operator timeout interrupted the replay at the iDroid lesson while the
game continued rendering; the same session was reconnected without restarting
the game for transport recovery.

The `74a1477f...` live replay advanced every actual Mission 1 binocular lesson:
physical raise -> Zoom, hand-aimed village -> Intel Radio, VR X accepted by the
native radio -> Custom Marker, physical-ray waypoint -> Miller explanation.
The retail binocular camera stayed off. The native explanation then completed
and returned to `Seq_Game_RescueMiller`, but its VR output was black for about
118 seconds (10,657 empty stereo submissions). Both-eye captures and native
states are retained in `mission1-broadcast-*` and `mission1-rescue-miller-return`.
This was a failed cinematic acceptance run, not a full Mission 1 pass.

The new camera regression reproduced a deadlock: entering authored mode cleared
the previous image, and replacement-camera admission required that cleared
image's timestamp. The active candidate preserves render-source liveness
independently of retired pixels. The regression failed before the fix and passes
after it. On `9d07ff7c...`, the actual physical lesson actions again advanced to
the explanation and native `Seq_Game_RescueMiller` returned afterward. Three
separate 45-second left-eye compositor recordings cover the explanation and
return. All 478 decoded frames contain a rendered scene, with no black frames
or identical adjacent frames. Both-eye movie stills and return stills were
reviewed. No extra binocular or Pause panel appeared in that replay.

The case runner still reports failure because its control observation became
null during the movie; that result is retained, not relabeled as passed. The
independent recorder and fresh native return observation establish the camera
fix. Recording cadence was only 3.3-3.8 fps and does not prove headset comfort.
One diagnostic clock race could reject a newer control publication as
future-dated. It is now corrected by copying the publication before
sampling observation time; the same 250 ms freshness rule remains enforced.

The d5529f5c final replay reproduced a lost physical-raise notification. Its
consumer sampled GetTickCount64 while the producer used steadyMilliseconds;
observed clocks differed by 26 ms. The queue also discarded an event published
after its consumer sampled time. A new regression failed before the repair.
The c69b75c6 build uses the producer's clock and retains genuinely future events
until the next update, preserving the 250 ms/activation checks.

On c69b75c6, `mission1-clock-fixed-mounted` advances into the actual Zoom lesson.
`mission1-clock-fixed-intel` then completes Intel, Custom Marker, the full Miller
explanation, and returns to RescueMiller through real physical VR actions.
No Skip, tutorial timeout, progression writes, or stock binocular mode was used.
Three separate 45-second left-eye takes contain 477 nonblack frames, no identical
adjacent frames, and no capture errors. The movie contact sheet and both final
return eyes were reviewed. A separate deployment capture hit a black transition
frame and remains a failed arrival check; it is not silently relabeled.

Earlier Mission 6 runs reached the approach outpost and created a real checkpoint. Repeated
route attempts hit terrain and guards killed the character; the bridge event
remained zero. This does not validate the bridge. Its alternate fort landing
starts past the bridge event and is explicitly excluded from bridge evidence.
Normal retry exposed overlapping failure-screen panels, and normal ACC
deployment exposed missing VR menu ownership for MissionPrep screens. These
were corrected by detecting the actual retail sortie/game-over presentation.
On `d5529f5c...`, ordinary VR menu bindings confirmed deployment without a
native-buttons fallback, and both reviewed failure-screen eyes show one native
panel. Normal checkpoint retry returned to the field with camera and input
ownership restored. The private route runner also encountered a delayed
Chicken Hat prompt; its initial retry timeout is retained as a harness failure.

The final c69b75c6 native-map traversal reached the actual bridge event at full
health and without alert. `mission6-native-astar-04` observes p31_020020_000;
`mission6-native-bridge-watch` observes its normal end, and
`mission6-playable-bridge-movement` measures 6.717 native units of ordinary VR
movement afterward. Event sequence progresses 0 -> 1 -> 2. The bridge scene is
playable (`isInGame=true`): all 190 active observations retain the immersive
player camera and gameplay control context. The movement probe began after the
event ended, so it is not proof of movement during the event.

Five separate 45-second compositor takes contain 699 nonblack frames, with no
identical adjacent frames or capture errors. The contact sheet and both trigger
and return eyes were reviewed: world and arms remain visible without an added
panel. Low recording cadence (~3.1 fps), distant NPC performance, full Mission 6
completion, bridge checkpoint recovery and headset comfort remain outside this
result. The earlier transport failure and slope-arrival oscillation are retained;
the bot now preserves transport-failure results and uses floor-aware arrival
plus directional progress instead of accepting sideways jitter as progress.

The current candidate's physical binoculars acquired a real moving Mission 6
patrol soldier (native ID 1076). `mission6-visible-patrol-dwell/result.json`
records native person flags 1 -> 3 after physical sight dwell, 3 -> 1 after the
VR clear action, and 1 -> 3 after a freshly aimed explicit VR mark. Both eyes
of acquisition and the right eye of explicit marking were reviewed: the enemy
marker is inside the held physical optic, with no second binocular screen.
The stock binocular camera stayed off. Earlier cliff-obstructed targets
remained unmarked; physical rays hit nearer terrain and explicit marking
placed terrain waypoints. Sky-negative and final horseback coverage remain
open. Under-fire stills do not certify motion comfort.

| Claim | Required live exercise | Evidence and acceptance | Current result |
| --- | --- | --- | --- |
| Mission 1 progression | Native mission replay, physical iDroid open/close, binocular raise, zoom, intel and target marking | Native sequences advance from real VR actions; no Skip, timer expiry, direct progression writes or replaced controls; both eyes reviewed | Clean physical lesson progression and complete explanation/return observed on c69b75c6; 477 recorded frames nonblack; both return eyes reviewed; prior failures retained |
| Binocular interactions | On foot and horseback, hold and hand aim, zoom, visible enemy automatic acquisition, explicit enemy/terrain mark, clear and stow | Actual native marker state plus both-eye composition; include occluded enemy/sky negative cases | Physical enemy auto-acquire/clear/explicit re-mark observed on d5529f5c; native flags 1->3->1->3; terrain occlusion observed; mounted equip/zoom/waypoint/stow previously pass; sky and final mounted coverage pending |
| Mission 6 bridge | Deploy and traverse the actual reported bridge/cinematic encounter | Before/during/after view and movement; native sequence, camera and input evidence; no missing world/hand layers | Actual bridge trigger, natural event end and 6.717 units of subsequent VR movement pass on c69b75c6; 699 nonblack compositor frames; bridge checkpoint recovery and full mission remain open |
| iDroid lifecycle | Ordinary and forced FOB tutorial close, physical stow and immediate reopen | Native UI closes, handset exits, normal controls return; both-eye screen fit | Forced tutorial stow previously failed; repair pending |
| Cutscene pause | Pause, hand motion, resume during authored shot and after handoff | Both eyes retain world and hands; menu input ownership and movement recover | Pending on final candidate |
| Display changes | Native options/resolution changes and return to play | Swapchains and camera remain live; both eyes and UI readable without checkpoint reload | Pending |
| Session recovery | Supported simulator focus/tracking transitions and recovery | Session/camera/rig return without stuck controls or black world; physical headset claims separate | Pending |
| Combat | Fire near cover and prone; C4 placement along VR heading | Native action result and final-eye spatial evidence, without changing established controls | Pending |
| Coco adaptations | Visibility watchdog, tracked hand recovery, bounded stationary lower body, bob/fit, cinematic comfort, stale demo recovery | Contract tests plus final-eye moving field/cinematic runs; cached poses never authorize interaction | Integrated; combined candidate acceptance pending |
| Distribution | Build/tests, install/update/rollback/uninstall, settings preservation, archive hashes and licenses | Verify the exact final DLL/archive; no retail assets or simulator overrides | RC4 passed; rerun after changes |

For visual claims inspect both submitted eyes over slow/fast head and hand
motion, opposing motion, and repeated transitions. Record duration and capture
cadence. Telemetry alone and isolated stills do not certify motion or headset
comfort. Record each claim as passed, failed, or unproven; retain rejected runs.
