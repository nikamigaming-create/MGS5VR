# Current handoff - 28 September 2026

Read the user's VR mandate in AGENTS.md. Preserve physical hand-held binoculars,
hand aiming, zoom, visible-enemy acquisition and marking. Never substitute stock
binocular stick controls or a second screen. Do not leave test-opened Pause menus.

Current installed/staged candidate: RC5, DLL SHA-256
`c69b75c6099d77d773b6503caee339c51ae33411d3c65062c2b0ba28c51ca79e`.
No publication is authorized; the user will decide when to release.

## Latest user stop point: test kit finished, waiting until home

At the user's request, implemented the separate Test Field Kit with the
launcher motif, Human/LLM supervisor selection, identical telemetry/eye
observations, a guarded scenario library, historical coverage and run archive.
Four live scenarios / eight native checks passed. The recorded repeat queue
also passed; actual mid-binocular supervisor handoff released/stowed cleanly.
13 new contracts and the related session/supervisor suites pass. See
`docs/TEST_FIELD_KIT_VERIFICATION_2026-09-28.md` and `docs/TEST_FIELD_KIT.md`.

Delivered video: `artifacts/test-field-kit/demo/MGS5VR-Test-Field-Kit.mp4`
(41.16 seconds). This is the kit UI with real timestamped compositor stills,
not continuous headset video. All owned game/simulator/kit/browser processes
are closed and the task runtime override is removed (20:39 UTC).
User asked to finish the video and stop for today until they come home.
Do not resume unattended game testing or publish without their next instruction.
The RC5 player build and personal settings remain unchanged.

## Completed evidence

- Mission 1 physical lessons advance through the complete Miller explanation
  into RescueMiller. The actual raise/zoom/VR intel/waypoint actions are used;
  no Skip, timeout completion or sequence writes. Three separate compositor
  takes contain 477 nonblack frames. Both return eyes and contact sheet reviewed.
- The optic event queue now uses one producer/consumer clock and retains future
  events for the next update. The regression failed before the fix.
- The authored-camera handoff keeps render-source liveness independent of the
  retired image, fixing the reproduced black explanation.
- Mission 6 native-map traversal reached p31_020020_000, which is a playable
  in-game event. The normal 0 -> 1 -> 2 transition completed. All 190 active
  observations retain the player camera and gameplay context. Five separate
  compositor takes contain 699 nonblack frames. VR movement afterward covers
  6.717 native units, with full health and no alert. Both trigger/return eyes
  and contact sheet reviewed. This is not full Mission 6 completion.
- All 35 CTest groups pass, including 7,324 core checks and 291 control checks.
  All 102 retail Lua presentation fixtures pass. Coco integration is retained.
- The staged RC5 installer/update/rollback fixtures pass. Personal settings and
  controls remain byte-identical. No saves were overwritten.

Evidence: artifacts/release-validation-20260928/mission1-clock-fixed-intel,
mission6-native-astar-04, mission6-native-bridge-watch and
mission6-playable-bridge-movement. The final movement began AFTER the bridge
scene ended; do not claim movement was tested during it. Historical failed
runs and their original binary identities are retained.

## Bot and private map

Maintained tools/gameplay-navigate.py reads the private hash-checked NAV2
manifest, preserves height/directed portals, runs A*, observes live soldiers,
and uses ordinary VR inputs. Thirteen navigation tests pass. Reproduced bot
faults were fixed: lost failure evidence on transport cleanup, posture threshold
oscillation, overly slow node movement, and slope-height arrival jitter.
No general combat/shooting policy has been validated.

Private manifest: private/navigation-20260928/manifest.json (40 tiles, 44,171
nodes). Never package these retail assets or the private reference-parser clone.
The PNG is artifacts/release-validation-20260928/mission6-native-navigation-map.png.
It shows a planned route and a soldier snapshot; live progress is in run results.

## Remaining

Forced FOB iDroid suspension/handset stow and immediate reopen, display-fit
matrix, actual movement during the playable bridge event, bridge checkpoint
reload, other reported cinematic/field routes, and physical headset acceptance
remain open. All 55 reports and 77 scoped claims are retained in the ledger.
Do not turn a scoped tutorial/bridge pass into full-mission or full-game acceptance.

Continue from current native observation, not a remembered PID or saved checkpoint.
The last test reached Mission 6 event sequence 4 near [2003,348,-316] after the
bridge finished. All test inputs are released. Owned MGSV and simulator processes
are stopped, and the unchanged task runtime override was removed. Consult
artifacts/release-validation-20260928/session-cleanup.json for exact cleanup
status; the next launch must establish a fresh regular simulator session.

Personal settings SHA-256:
`a2b836b8ebd5b3ec725f175f922796b3da87ac41d86d31fa531c9e1e05a56565`.
Personal controls SHA-256:
`f8f4be591a1164d681607015026043d2c93fb1b2b56bedb27ff5f01e12564c9f`.
The working tree contains substantial pre-existing work. No commit, push or
public upload was made. Usage was 94% consumed when last checked; preserve the
remaining budget for the unresolved defects.
