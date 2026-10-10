# Tracked firearm clearance

Implementation status: code complete; fresh game and headset acceptance remain
pending. The physical report of a clear gun being denied near cover/prone is the
reported defect. A successful baseline prone shot does not close that report.

The `tracked_weapon_obstruction` setting defaults to enabled. It moves the
native weapon obstruction sweep onto the barrel of the tracked gun. Disabling
the setting restores the exact native sweep. It does not disable world collision
or grant ammunition, firing eligibility, or an accepted shot.

The owned executable establishes this path:

- `0x1062a30` computes the weapon obstruction segment from native body/bone
  transforms, with a separate prone branch at `0x1062b00`.
- `0x1064aab` submits query `weaponComponent+0xb0` through the physics interface
  at `weaponComponent+0x70`, vtable `0x24f3760`, slot `+0x28` (`0x1bf0fc0`).
  The return address `0x1064aae` identifies this exact caller.
- The wrapper's verified ABI is
  `bool(physics, query, uint32 flags, Vec4* start, Vec4* end, float radius, bool temporary)`.
  It forwards to `0x1b9b690`. The native job setter `0x1b9aaf0` copies both
  endpoint vectors before returning, so aligned temporary vectors are sufficient.
- The default radius comes from `0x218ee08` (`0.03`); weapon-state `+0x3c2`
  bit 0 selects `0x20b35f8` (`0.01`). Both native radii are retained.
- `0x103b680` consumes that same query through slot `+0x30` (`0x1bf1040`),
  publishes its positive obstruction result in weapon-state `+0x3c4` bit 21,
  and retains the native 0.2-second clearance delay and bit 25 publication.
  Other native reasons contributing to that publication remain untouched.

The adapter only changes this caller and the currently owned local firearm.
It requires the same published rig's tracked right palm, authored muzzle socket,
and accepted weapon component, state, player index, body model and resource.
Source and weapon samples must be no older than 100 ms. Missing, stale, switched,
unsupported, native-gamepad or vehicle/action-owned samples use the original
query. The segment runs on the authored barrel axis from the grip's depth to the
muzzle, so a head or stock behind cover cannot extend it backward into cover.
The query descriptor, filters, radius, native asynchronous result handling and
return value are retained. Neither shared weapon permission flags nor the
accepted-shot/projectile path is patched.

Native wrapper bytes, the exact caller, and both vtable entries are checked at
installation. Local static analysis is retained under
`artifacts/dev/headset-feedback-current/cover-*`; no new gameplay validation was
run while the user requested code completion first.
