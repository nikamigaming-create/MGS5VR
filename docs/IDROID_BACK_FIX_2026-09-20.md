# iDroid controls hotfix â€” playtest v12

The original controller layout is preserved:

| Action | Default control |
| --- | --- |
| Open iDroid | Tap left Menu (under 550 ms) |
| Pause | Hold left Menu (550 ms) |
| Confirm / dismiss native notice | Right A |
| Back / close | Right B |
| Previous / next iDroid tab | Left / right grip |
| Navigate lists | Left stick |
| Pan map | Right stick |
| Map zoom | Triggers |

The right Meta button opens the runtime dashboard/Air Link. It is not a game binding.

## Recovery from the forced tutorial lock

The imported test save opens a forced FOB tutorial that ignores ordinary Back.
Hold the existing Back button (right B by default) for 750 ms while iDroid is open
to request the game's native terminal stop. Ordinary B taps retain native submenu
navigation. No replacement binding or controller chord is introduced.

The native game may display **Exit Tutorial**. Press A to dismiss that notice.
If Snake is still holding the device with its screen closed, tap left Menu once
to stow it; the next tap opens the normal iDroid. This is an escape from the native
tutorial lock, not a change to tutorial completion or save progression.

The recovery requires a fresh Back press after menu entry or focus loss, fires
once per hold, and expires if its native request is not consumed within one second.
The native Lua transaction thread performs the stop. External Lua diagnostics are
not required for the controller recovery.

## Observed on this candidate

DLL SHA-256: `86a8edc4bc81ac43995b5dc3fa637f999ff4a59148657f46abf7697532c51c7f`.

- Release build and all 16 automated suites passed.
- Fresh SIM launch, physical Continue tape, native loading and field arrival passed.
- Original left Menu opened the forced tutorial; short B reproduced the lock.
- Held B invoked native recovery and closed the terminal. The native Exit Tutorial
  notice required A, and left Menu stowed the remaining device before reopening.
- Subsequent normal map pan, both zoom directions, short-B close, left-Menu reopen,
  right-grip Missions tab, A into Supply Drop, left-stick list selection, B back to
  Missions and B back to gameplay passed. No supply drop was ordered.
- Read-only checks throughout reported tutorial state `127` and completion `false`.
  The fix does not assign either variable or replace any save.
- Installed personal settings and bindings were preserved. Controls SHA-256 stayed
  `dc24c0f72d3c7163d3aa1cf3190a1c403a5f0855f3fe78eb26a97fa2eaeca283`.

The first diagnostic automation assumed immediate reopening and stopped on the
native Exit Tutorial notice. Its failed assertion is retained in the local evidence;
the follow-up tests above exercised the actual notice/stow/reopen flow.

## Video and limits

`artifacts/showcase-20260920/34-idroid-v12/map-controls-compositor/simulator.mp4`
is a continuous 17-second right-eye Meta OpenXR compositor capture of map controls,
close, reopen and close. It contains 50 real frames (2.94 captured frames/sec); this
is capture cadence, not the game's frame rate. It is a controls check, not the full
showcase. Separate two-eye captures cover recovery and submenu navigation.

The earlier native-texture recording is diagnostic only: it does not contain the
complete recovery-notice sequence and is not final compositor footage.

Physical headset acceptance of this hotfix is pending. Existing v11 issues remain,
including Pause arm freezing/menu fit, some notifications away from the wrist,
and incomplete weapon/item/guard/vehicle coverage. See `PLAYTEST_V11_2026-09-20.md`.
This candidate is a local beta, not a claim that every game feature is validated.

Use `Launch-Headset.cmd` after closing the simulator session. It selects the physical
OpenXR runtime for the game process and uses the TPP location saved by the launcher.
