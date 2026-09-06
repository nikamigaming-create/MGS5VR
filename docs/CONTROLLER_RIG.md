# Controller rig experiment

Work in progress for the exact TPP 1.0.15.4 executable in the profile. This is
disabled by default. One rifle has simulator evidence; physical alignment and
complete weapon acceptance remain unproven.

The OpenXR input path captures left and right grip and aim poses at the same
predicted display time as the head and eyes. Grip and aim validity are independent.
An immutable source sample travels through native skin publication to the eye
camera. A different native camera publication or stale rig sample withholds the
view instead of combining unrelated poses.

The current native experiment intercepts skin publication at RVA `0x1a6caa0`.
It requires the binding to be the player-owned body model, reached through the
verified camera owner and character chain. The full pose supplies parent-space
hierarchy metadata and absolute joint rotations/positions. Two-bone arm solves
preserve segment lengths and clamp unreachable targets. Descendants retain their
animated relation to the wrist, including fingers. The initial native wrist
orientation supplies an experimental controller-to-wrist calibration; a physical
anatomical calibration has not been accepted.

An earlier experiment at RVA `0x1016e40` changed the separate 21-joint gameplay
query skeleton. Live observation showed it did not steer the rendered rifle.
That approach is not installed by the current source.

## Native firing

The shot solver at RVA `0x1044ff0` reads the rendered wrist through the native
getter at `0xaf0ea0`. Its weapon attachment getter at `0x1042e40` and authored muzzle
socket provide the barrel frame. The adapter requires that wrist to match the
latched rig publication within 3 cm and quaternion dot 0.999. It substitutes a
barrel-axis target in a private copy of the solver state and publishes the authored
muzzle origin. Native projectile creation, ammunition and reload remain in the
game. A private probe at projectile creation observed the modified origin and
direction, not merely an intermediate calculated ray.

Current coverage is the ordinary firearm mode exercised with AM MRS-4. Native
scope/alternate weapon modes and stale or unmatched rig publications use the native
shot path and do not establish controller aiming. Do not treat that fallback as
tracked aiming. Collision near walls and all weapon families require further work.

Right grip holds the native gun ready; right trigger fires. Left grip requests
support-hand contact. Left trigger still supports the earlier ready/aim mapping.
These grip inputs no longer invoke native shoulder-button actions while the rig
is active. Support input is stored with the same tracked pose packet as the arm
solve. Releasing the weapon can expose native stow/attachment animation defects.

## Observations

The first 29.25-second continuous left-eye recording contains 101 distinct captured
frames (3.45 captures/second), right-hand translation, yaw, pitch and roll, native
firing/ammo changes and reload with the head held fixed. No fully black frame was
captured. A separate head translation/yaw check retained controller poses and the
rifle's world direction. Walking retained the first-person camera. Sleeve intrusion,
unheld-weapon animation and the original face HUD remain visible defects.

## Remaining acceptance

The current grip-control build also completed a 35.234-second left-eye sequence:
121 distinct captured frames, 3.43 captures/second, right-grip readiness with the
legacy aim trigger released, two aiming directions, fire, reload and an independent
head lean/yaw. No fully black frame was captured. Its mobile MP4 is silent 720x754
H.264 Baseline and is not evidence of 60 FPS capture.

- Verify the full rendered hand, bionic forearm and weapon attachment together.
- Extend the observed muzzle/projectile binding to scoped and alternate weapons;
  verify impacts, obstruction and all firing/reload states.
- Check independent head motion, reach limits, reload, weapon changes, stances,
  reference-space changes and loss of either controller.
- Remove camera HUD duplication and complete the wrist interface.
- Capture real, continuous single-eye evidence; simulator footage is not physical
  headset acceptance or evidence of 60 FPS capture.

Private executable analysis and captures remain outside the source distribution.
