# Controller rig experiment

Work in progress for the exact TPP 1.0.15.4 executable in the profile. This is
disabled by default. AM MRS-4 and WU pistol interactions have simulator observations;
physical alignment and complete weapon acceptance remain unproven. See the
[complete Touch controls](CONTROLS.md) before testing.

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
animated relation to the wrist, including fingers. Native wrist and metacarpal
landmarks define the anatomical grip frame, following the OpenXR palm axes.
Activation no longer calibrates from an arbitrary weapon animation. Shoulder
anchors follow the head and native torso yaw. Authored joint axes define the
elbow hinge; the elbow itself does not inherit wrist roll. Verified arm corrective
joints 97-110 use their bind offsets and recomputed native local corrections.
The two forearm twist helpers carry 35% and 75% of wrist twist. Elbow and wrist
flexion helpers counter-rotate by 55%; shoulder weights retain the native
left/right difference. Other descendants retain their native animation.
The forearm HUD's long edge follows elbow-to-wrist, with its front on the dorsal
side of the forearm. Native animation input
is restored after matrix publication, avoiding feedback into the next pose.
Physical controller fit has not been accepted.

The primary body's verified bone 53 (`SKL_300_ASRROOT`, hash `ec70c442`,
parent 0) is the hip holster. Its published render matrix has zero scale during
an accepted VR rig update. Native animation channels and the held wrist socket
are preserved. The stowed rifle stock disappeared in the observed pistol/prone
views, while drawing the rifle remained functional. This is a guarded local
mount rule, not a global model-visibility scan or an all-holster solution.

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

Current coverage is the ordinary firearm mode exercised with AM MRS-4 and WU pistol. Native
scope/alternate weapon modes and stale or unmatched rig publications use the native
shot path and do not establish controller aiming. Do not treat that fallback as
tracked aiming. Collision near walls and all weapon families require further work.

Right grip holds the native gun ready; right trigger fires. Left grip requests
support-hand contact. Tracked hand proximity also acquires contact within 30 cm, releases beyond
45 cm, and gives way to wrist inspection. Selection retains support intent while
the gun is stowed, then checks the tracked hand distance again. Left trigger is the equipment modifier. Native reload
and bolt-cycle state temporarily own support contact, with a 120 ms attach/release
blend. The right hand remains the weapon's primary controller.

The optional wrist HUD relocates native UI orders 146-148 from verified artificial
layout cameras onto the left forearm. Other flat gameplay UI from those cameras
is suppressed, including the reticle and destination labels. Scene-camera 3D
people cues retain the exact captured eye view and projection. The native weapon
selection cards share the forearm surface. The rear side is hidden rather than
rendering mirrored text. This is weapon/status UI, not a complete iDroid or damage/
subtitle interface. Blocking menus still use their native controls and pixels.

A binocular/iron-sight screen prototype was tested and removed at the user's
request. The tracked VR input path reserves the optical action; it does not
transition aiming to a mono quad or invent zoom for iron sights. Authored stereo
optics remain future work.

## Observations

The [September 6 HUD/rig review](SIM_HUD_REVIEW.md) records the current build,
15-second clip, controls exercised, and unresolved gates. It shows real changing
forearm ammo, support/reload contact, rifle-to-pistol selection and head movement.
Sleeve/cuff shaping and physical controller alignment still need acceptance.

Historically, the first 29.25-second continuous left-eye recording contains 101 distinct captured
frames (3.45 captures/second), right-hand translation, yaw, pitch and roll, native
firing/ammo changes and reload with the head held fixed. No fully black frame was
captured. A separate head translation/yaw check retained controller poses and the
rifle's world direction. Walking retained the first-person camera. That older build
showed sleeve intrusion, unheld-weapon animation defects and the original face HUD.

## Remaining acceptance

The earlier grip-control build also completed a 35.234-second left-eye sequence:
121 distinct captured frames, 3.43 captures/second, right-grip readiness with the
legacy aim trigger released, two aiming directions, fire, reload and an independent
head lean/yaw. No fully black frame was captured. Its mobile MP4 is silent 720x754
H.264 Baseline and is not evidence of 60 FPS capture.

- Verify the full rendered hand, bionic forearm and weapon attachment together.
- Extend the observed muzzle/projectile binding to scoped and alternate weapons;
  verify impacts, obstruction and all firing/reload states.
- Check independent head motion, reach limits, reload, weapon changes, stances,
  reference-space changes and loss of either controller.
- Extend the weapon/status wrist interface to contextual actions, damage, subtitles
  and interactive iDroid menus.
- Capture real, continuous single-eye evidence; simulator footage is not physical
  headset acceptance or evidence of 60 FPS capture.

Private executable analysis and captures remain outside the source distribution.
