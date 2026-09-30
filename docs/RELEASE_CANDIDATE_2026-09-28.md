# Community test candidate RC5 - 28 September 2026

RC5 preserves the physical VR binoculars and fixes recognition of their actual
Mission 1 lesson actions. The hand-held optic still equips, hand aims, zooms,
automatically acquires visible enemies, and explicitly marks targets. No stock
binocular camera, replacement stick aiming, or second optic screen is introduced.
The unexpected Resume/Skip panel was the retail tutorial Pause menu opened by the
old harness; that automatic Pause action was removed.

Local candidate DLL SHA-256:
`c69b75c6099d77d773b6503caee339c51ae33411d3c65062c2b0ba28c51ca79e`.
Supported game: The Phantom Pain 1.0.15.4, Windows x64, PC OpenXR headset.
Publication awaits the user's instruction.

## Fixes and actual validation

- A shared clock and retained future events prevent the physical binocular
  notification queue from losing the raise at the producer/consumer boundary.
- The actual Mission 1 binocular lessons now advance through normal VR actions
  into the Miller explanation and return to RescueMiller on this DLL. Three
  separate 45-second left-eye takes contain 477 nonblack frames. The contact
  sheet and both return eyes were reviewed. No Skip or timer expiry was used.
- Authored camera handoff no longer waits on an image discarded at entry. The
  earlier build reproduced a black explanation; the final run renders it.
- Native sortie and failure screens own the appropriate VR menu context. Normal
  ACC deployment and checkpoint retry were exercised on preceding d5529f5c,
  with one visible failure panel instead of overlapping panels.
- Real enemy automatic acquisition, clear, and explicit re-mark were observed
  on d5529f5c: native person flags 1 -> 3 -> 1 -> 3, stock binocular camera off.
  Historical captures retain their actual binary identity.
- Mission 6's actual bridge event starts and ends normally on this DLL. Its
  player view and gameplay control context remain active; subsequent VR movement
  covers 6.717 native units. Five separate compositor takes contain 699 nonblack
  frames, with both trigger and return eyes reviewed.
- All 35 CTest groups, 7,324 core checks, 291 control checks, and 102 retail Lua
  presentation checks pass. Coco's integrated contract checks are included.

## Remaining acceptance

Mission 6 completion/bridge checkpoint recovery, forced FOB iDroid stow/reopen, remaining
display and recovery cases, and physical headset acceptance remain open.
The native navigation bot uses owned NAV2 paths and real enemy telemetry;
a planned route or completed approach is not a mission-completion test.

Mission 1 evidence covers the lessons and explanation, not the entire mission.
Low-cadence simulator capture does not certify headset comfort. Historical failed
runs remain in the evidence; the ledger retains all 55 community reports and
individual claim statuses. See RELEASE_VALIDATION_2026-09-28.md.

## Install or update

Close MGSV, extract the complete candidate, and open `MGS5VR-Launcher.exe`.
Select the supported `mgsvtpp.exe`, then choose **Update / Keep My Settings**.
Updating creates a dated rollback backup. Keep the whole folder together.
The package contains no extracted retail assets, game archives, navigation
tiles, saves, or forced simulator runtime.
