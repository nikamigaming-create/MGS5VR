# Firearm proximity preference

Implementation status: a bounded actual-game On/Off test passed on October 11;
physical headset acceptance remains pending. The physical report of a clear gun
being denied near cover/prone still needs its matching headset scenario. A
successful baseline prone shot alone does not close that report.

`block_firing_near_cover` defaults to **0 (Off)**. For the verified current local
VR firearm, it suppresses only a positive result from the native pre-fire
proximity sweep. The query still runs and its asynchronous job is consumed
normally. Actual projectile collision, ammunition, reloading, other weapon
restrictions and the accepted-shot path are unchanged.

With cover blocking enabled, `tracked_weapon_obstruction` (default **1**) moves
that sweep onto the barrel of the tracked gun. Disabling the tracked option
uses the original posture-derived sweep. The tracked option has no effect while
cover blocking is Off.

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
- The result wrapper has ABI `int32(physics, query)` and forwards the query to
  `0x1b9b790`. That function waits for its job and performs native cleanup before
  returning the signed hit count. Return address `0x103b6d4` is the exact weapon
  consumer; its signed `count > 0` branch supplies the proximity contribution.
  When cover blocking is Off, this caller receives zero instead of a positive
  count, after the original consumer returns. Zero/negative results and every
  other physics caller receive the unmodified result.

Both adapters require a held firearm, the same published rig's tracked right
palm, and matching local player, weapon component/state/index, physics interface,
body model and resource. Source and weapon samples must be no older than 100 ms.
Missing, stale, switched, unsupported, native-gamepad or native action samples
retain native behavior. A vehicle requires its existing fresh weapon-ready
acknowledgement. The tracked sweep additionally requires the authored muzzle
socket; the Off preference does not invent or require a replacement segment.

The tracked segment runs on the authored barrel axis from the grip's depth to
the muzzle, so a head or stock behind cover cannot extend it backward into
cover. Query descriptor, filters and radius stay native. Neither adapter writes
shared weapon flags or query storage. The consumer still computes other native
restriction contributors and its clearance delay; changing the preference can
therefore retain an already active native clearance delay briefly.

Native wrapper bytes, the exact submit/result callers, the result implementation
signature and both vtable entries are checked at installation. Portable contract
tests cover setting combinations, local ownership, tracking/readiness, sample
age, foreign physics/callers and signed result preservation. These checks do not
establish game or headset acceptance. Local static analysis is retained under
`artifacts/dev/headset-feedback-current/cover-*`.

The actual-game proof on DLL `e1dc46d11bc7` is retained in
`artifacts/dev/field-idroid-checkpoint-current/close-cover-proof-v3/`. In native
ON THE TRAIL, a tracked prone pistol barrel had a fresh positive obstruction
before and throughout the VR trigger hold. All seven during-trigger samples
returned a positive applied hit count with blocking On, and ammunition stayed
10. After saving Off, releasing controls to apply the edit and readying at the
same requested controller pose, the consumer selected ignore-proximity mode
and a VR trigger consumed one round (10 → 9). The Off native sweep itself
returned zero in this take; this does not prove a positive native-body sweep was
bypassed in game. Portable contracts cover signed positive-count suppression.
Both-eye released outcome images were reviewed; they are not stills of a held
weapon firing. Other weapons/cover and physical comfort remain separate checks.
