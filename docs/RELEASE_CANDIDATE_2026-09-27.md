# Community test candidate RC2 — 27 September 2026

RC2 contains the adapted Coco fixes and the verified fixes below. It is a local
community test build, with known mission and presentation failures still open.
The entire game and physical-headset acceptance are not complete.

DLL SHA-256: `cd91d99e5a892cc216bb444e7706670b14a742083214fe90ef97a33b4375c06b`.
Supported game: The Phantom Pain 1.0.15.4, Windows x64, PC OpenXR headset.
Archive: `MGS5VR-community-20260927-rc2.zip`. RC1 is retained separately.

## Changes

- Coco's camera changes suppress ordinary gait bob, re-anchor during posture
  changes and apply the supplied camera/shoulder offsets. Physical head motion
  remains direct. Noninteractive cinematics gain position/yaw smoothing and a
  level horizon, with resets at cuts and an exception for interactive look.
- Coco's controller recovery keeps visual hands for up to 250 ms of tracking
  loss and blends recovery over 100 ms. Cached poses cannot fire, throw, steer
  or operate wrist controls. Identity and reference-space changes clear them.
- Coco's stationary pelvis/leg correction is applied to temporary render bones
  after movement stops. Horizontal correction is bounded; actor transforms and
  collision remain native. Vehicles, menus and prone presentation are excluded.
- The visibility watchdog and stale-demo recovery preserve the newer native
  owner, camera, generation and presentation checks. See the
  [Coco integration record](COCO_INTEGRATION_2026-09-27.md).
- The left modular prosthetic refreshes while Pause is open. Its render palette
  can legitimately have fewer bones than the animation skeleton; the previous
  equality check rejected those refreshes.
- Cassette labels wrap within their columns; loading panels fit both eyes.
- An already accepted stereo image survives the narrowly identified stale
  source-pose suspension. New stereo images still require a valid pose join.
- Simulator ownership checks recognize same-process sockets. The required
  ownership module ships with the installer; no simulator override is enabled
  in the package.
- The experimental duplicate binocular screen has been removed. The physical
  optic, magnification and ordinary equip/stow behavior are restored.

## Validation

- Release build succeeded; all 34 CTest groups and 7,273 native contract checks
  passed. The retail game's Lua VM passed 76 isolated presentation fixtures.
- A fresh simulator run passed ordinary field Pause, with both hands moving in
  both reviewed eye views. Menu input produced 0 m displacement and 0° yaw.
- Walk/stop and crouch/stand passed. Physical binocular lens content and zoom
  were reviewed without the additional native screen.
- Normal field iDroid Back closed the map and returned to gameplay. This does
  not accept the forced Mission 1 tutorial or the remaining screen-fit defect.
- Native title Continue, loading, helicopter arrival and disembark worked.
- A measured 800.247 ms producer delay had zero missing-layer submissions in
  its bounded 900-frame window. The full session had seven missing-layer frames
  during cinematic activation changes; the broad flicker claim remains failed.
- The installer is checked against a staged package in Windows PowerShell 5.1.
  Its completed result is recorded in `PACKAGE_AUDIT.json` in the ZIP.
- The installed personal settings and controls remained byte-identical.

The launcher carries the hashed per-claim evidence ledger. Historical evidence
keeps its actual DLL identity. Sequential eye screenshots do not establish
synchronized stereo timing or physical comfort. Tracking-loss recovery and the
full visible lower-body result still need live headset acceptance.

## The unexpected Resume/Skip screen

This was the game's tutorial Pause page. The test's Mission 1 abort left its
tutorial-pause UI flag active in Mission 6. Only that stale UI flag was cleared
after verifying Mission 6 was active and idle; sequence/checkpoint stayed the
same. No Skip or tutorial-completion function was called. The normal field
Pause page was then retested. The harness now rejects that contaminated state
and does not leave Pause open after a completed field check.

## Still open

- Mission 1: native binocular tutorial progression.
- Mission 6: reaching and verifying the actual bridge background demo.
- Seven missing-layer frames during cinematic transitions; sustained natural
  flicker/performance routes and physical Quest Link/Air Link acceptance.
- iDroid map clipping at some hand positions; forced tutorial exit/reopen.
- Headset readability/fit of small cassette captions, wrist UI and iDroid.
- Cosmetic cassette props remaining beneath the loading panel.
- Remaining claims in the [55-report ledger](COMMUNITY_VERIFICATION_2026-09-27.md).

## Install or update

Close MGSV, extract the entire folder and open `MGS5VR-Launcher.exe`. Select the
supported `mgsvtpp.exe`, then use **Update / Keep My Settings** for an existing
installation or **Install VR** for a new one. Connect PC VR, select the headset
runtime, then Launch. Keep the whole package together; do not copy only the EXE.
The update creates a dated rollback backup and preserves existing settings.
The package contains no extracted retail game assets, game archives or saves.
