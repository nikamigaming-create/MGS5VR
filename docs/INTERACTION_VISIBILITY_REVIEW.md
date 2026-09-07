# Wrist selection, grenade pointing and vegetation visibility — 2026-09-07

The development candidate fixes repeated wrist selections and adds hand-directed
grenade preview/throwing. A conservative camera-visibility correction is included
for tree pop-in during leftward head turns. That specific physical symptom remains
unverified. This is a draft candidate, not a new release or complete-mod acceptance.

Tested and installed Release DLL SHA-256:
`EB89CE699E8700FE70F330DAE4FB449591E2601FD9AC3DFA998E9C3EFF412958`.
Game: TPP 1.0.15.4, existing 1% checkpoint. Runtime: Meta XR Simulator v205,
headless, simulated Quest 3, native 1280×720. The original 2560×1440 physical
graphics profile was restored afterward and the owned game/SIM processes stopped.
The system's physical OpenXR runtime selection was preserved.

## Changes

- Holding left trigger alone sends no equipment category. A fresh cardinal flick
  chooses one; browsing waits for the expanded native description and 80 ms of
  neutral stick input. Equip/stow delays cannot send browsing into the camera.
- Eight-direction card browsing retains the original axis signs. A direction
  settles for 60 ms and stays latched until 80 ms of neutral input. B returns to
  categories. A or right-stick click sends one native Use pulse. Held inputs are
  consumed on closing; left-stick movement continues.
- The small native NVG label layer is included on the wrist while Items is
  selected. NVG is the upward slot; selecting None switches it off. Optical
  binocular zoom remains unavailable.
- Grenade origin and velocity are committed together from one published rendered
  palm/aim pose for preview and actual throw. Native equipment strength comes
  from private neutral-angle copies, without patching shared camera angles.
  Hand position and tilt own the arc; readied tracked grenades consume no
  right-stick Y input. Horizontal body turning remains available.
- Acquired support contact stops chasing ordinary stow animation. Reload keeps
  its native contact animation; menu, lowering, non-firearm selection and tracking
  loss release support ownership.
- Before native visibility planes are built, the source clip projection covers
  both eye orientations/FOVs with a symmetric 0.12-radian margin. Wider native
  coverage is retained. Image projection, depth mapping, resolution, texture
  settings and LOD distances are unchanged. This is angular coverage, not an
  exact near-field union of two displaced eye frusta.

## Observed results

| Check | Result and limit |
| --- | --- |
| Trigger-only opening | Previous equipment remains selected until a category flick. |
| Four categories and Back | Primary/up, secondary/down, support/right and items/left reached expanded browsing; Back returned to category choice. |
| Card directions | Primary right/None and left/rifle visibly matched; item cardinals were exercised. All eight directions, wobble and neutral bounce pass contract tests. Scalar SIM input did not establish diagonal controller gestures. |
| Item access | NVG description appeared, NVG turned on, and selecting None turned it off. A used Phantom Cigar in the preceding same-input candidate; B ended time passage. NVG's bright/grainy daylight effect and headset performance remain unaccepted. |
| Grenade aiming | With a fixed head and neutral stick, hand yaw and pitch changed the native arc. Right-stick Y did not pitch the view while readied. Both preview and actual-throw adapter callers were recorded. |
| Grenade presentation | Both final eyes showed the ready grenade and hands after the sampled throw. The clip shows a grenade leaving the hand and exploding. Earlier prone/slope hand occlusion remains open; rising restored the hands. |
| Vegetation | The visibility hook ran. Both-eye samples and short head turns showed world/vegetation without a blank eye or sky rectangle. This does not prove the user's left-turn tree pop-in is eliminated. |

The final silent left-eye clip is **14.729 seconds, 724,588 bytes, 40 frames**.
Acquisition lasted 15.094 seconds at 2.65 captures/second; this is capture cadence,
not game FPS. All 40 encoded frames were inspected. It opens/closes Support,
aims left/right/up, throws and turns the head both ways. No blank frames or
missing hands were observed in this clip. Sequential right-eye stills supplement
it; they do not certify simultaneous stereo timing or physical comfort. Private
artifacts are excluded from the public source.

Release build and all six automated suites passed, including input transitions,
point-throw invariants, support release and asymmetric-eye visibility bounds.
Captured SIM intervals were approximately 70–82 native stereo pairs/s during the
interaction run. This was not a clean 1440p headset benchmark and establishes
neither a performance improvement nor locked 90 FPS. See [performance settings
and measurement limits](PERFORMANCE.md).

## Next physical check

Check one deliberate category/card choice, NVG on/off, the grenade arc with the
stick centered, and trees while turning left/right in the same location. Retain
the accepted 1440p/native-AA profile. Full vegetation streaming/LOD, prone terrain,
upper sleeves, every item, vehicles, optical zoom and immersive full menus remain
outside this acceptance.
