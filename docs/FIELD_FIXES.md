# Field fixes — implementation and acceptance

Updated 25 October 2026 with the incoming headset batch and scoped gameplay
results. The original production-path audit baseline was `5622052` (DLL `17b044cba72e`).

**Implemented is not accepted everywhere; acceptance pending is not missing code.**
The previous checklist mixed these states. Its exact run narratives and checked
subcases remain in [FIELD_FIXES_HISTORY_2026-10-23.md](FIELD_FIXES_HISTORY_2026-10-23.md)
and [FREE_PLAY_ACCEPTANCE.json](FREE_PLAY_ACCEPTANCE.json). No evidence was erased
and no broad report was closed by source inspection.

[CURRENT.md](CURRENT.md) is the handoff. [RELEASE_PLAN.md](RELEASE_PLAN.md) is the
ordered execution plan. All supplied community reports remain in scope.

## Latest incoming reports — 25 October

First-person eyepatch follow-up: checked/deployed `422594fb5ef8` renders Snake's
owned native patch and front strap close to the right pupil with gray transparency.
Actual both-eye on/off/on and head-motion pictures are retained under
`20261025T211153118499Z/on-off-motion`; see CURRENT for placement corrections and
the arrival fixture's no-op assertion. Physical headset fit remains user review.

The new physical-play batch (sandstorm spray, arm twist/reach, Rocket Arm socket
display, mounted machine-gun response, helicopter/D-Walker centering) is implemented
together on main; `f58ef4224995` was the preceding checked candidate. Scoped gameplay
checks pass D-Walker center/facing, helicopter exit, responsive MG grip aiming,
actual Rocket socket steering/screen, arm rotations and world-preserving storm
suppression. See CURRENT for retained runs, repaired failures and exact limits.
The optional character-model eyepatch toggle is in this batch; retained native
face captures confirm on/off/on appearance for the selected Snake face. Physical
Quest acceptance and other face variants remain pending. Native prone-entry eligibility,
all mount types and actual field-pickup boarding are not certified.


Previous playtest candidate: `62f696613e2b` / `5bb471f`. It adds native-terminal
stow routing while Help suspends camera presentation, retained exact-owner Help
acknowledgement, and consumption of the entire handheld Menu press. Forced-Help
stow/reopen passes retained 20261025T152037198735Z. It includes the on-board-to-field ownership
reset and Challenge Tasks mode16 post-stow Help cleanup.

- Sleeve pop during motion: a bounded same-owner camera/skin visibility continuity
  repair is deployed as a33bfd559b9e; scoped moving review passed. This does not close extreme
  cloth crowding or every outfit.
  The retained synthetic cross-head case asks for 1.069 m of shoulder-to-wrist
  reach from 0.539 m of native arm length. It cannot serve as a reachable-arm
  regression baseline; keep the current physical ordinary-motion report separate.
- Open iDroid while boarding field pickup: native passenger stow and deliberate
  on-board reopen are deployed with passing lifecycle cases and flight/field
  open/stow/reopen observations. Actual pickup boarding
  remains unverified because the current helicopter fixture did not land.

## Current candidate — 25 October

The active source batch expands exact forearm profiles to 20 owned outfits,
repairs paused-part cache lifecycle, separates mounted body/eye admission from
generic action input, and invalidates mounted aiming across action changes.
The moving-only intrusion is isolated to the stowed back weapon. Candidate
`1933aa81fc8e` did not fix it; the hip-stock hypothesis is reverted. The source
now conceals the owned stowed back assembly from main view and restores it on
draw/manipulation or leaving field presentation. Candidate `da4ae29de73b` /
`7273c25` also preserves concealment through camera-to-rig publication gaps.
Moving source-eye review and both-eye back-rifle draw review pass their scoped
visual checks; see CURRENT for retained runs and movement guard failures.
The terrain report now has
a regression clue (it began after the first-person rig change), not an exact
location or confirmed cause. These edits do not close FP04/FP16 or whole-game
and physical-headset acceptance.

Earlier candidate `d4b526e24cf5` / source `32f2ab0` included
the apparent third-arm correction, complete iDroid reopen pose/stereo handoff,
and recovery of forearm cropping after native object-buffer replacement. All
58 groups and installer checks passed; the user tests through `Play-Dev.cmd`. The cache fix
preserves exact outfit admission and is not all-outfit acceptance. Earlier
failed/interrupted runs remain failed; no repeated simulator soak is planned.

Candidate `a6dc5fb91151` / source `f11d0d0` adds the reproduced handheld
stale-skin correction. It passes 27 combined sustained/menu/reopen cases and
58 test groups plus installer checks. The earlier extended run failed and is
retained. Current frame repeats, physical Quest and wider field coverage remain
open; see CURRENT for measurements and exact scope.

## Body/arm candidate update — 24 October

The body/cuff batch `351d84672f0e` / source `5ba674f` flattens cuff rims,
separates cap shading and handles repeated native attribute-slot bindings.
Both body modes pass nine combined reach/return/left-iDroid/stow/reopen cases.
Both final eyes show the forearm-only crop with flat closed caps; the previous
`250884f4c156` selector regression is retained as a rejected intermediate result.
Extreme full-body cloth crowding, other outfits and physical Quest remain open.
See CURRENT and FREE_PLAY_ACCEPTANCE for exact scope and cleanup.

Candidate `1a64bb965bc6` / source `4392536` adds upper-sleeve clearance along the
whole upper arm and closes supported forearm-only cut loops using existing
native indexed geometry. Full-body reach/return and all seven forearm-only
reach/iDroid smoke cases have scoped results; native help/Daily Bonus interrupted
two earlier menu suites, whose failures remain recorded. See CURRENT.
That intermediate build's rough cuff rim and shading were corrected by the
later body/cuff batch above. Other outfits and physical Quest remain open;
there is no blanket FP17/body closure.

## Code audit and remaining scope

| Report | Implemented on main | Actual remaining work |
| --- | --- | --- |
| FP01 — idle/tracking camera | Runtime-valid inferred controller poses, complete visual hand-pose retention and owner/reference resets in [xr_runtime](../src/xr_runtime.cpp) and [arm_ik](../src/arm_ik.cpp). | Original physical idle-camera fallback was not reproduced in SIM. Verify physical idle/occlusion/recovery; do not invent a new camera fix. |
| FP02 — weapon/support loss | [controller_rig](../src/controller_rig.cpp) retains acquired weapon-relative contact and same-weapon muzzle calibration through reload; support fingers cup outside native manipulation. [WeaponTrackingContinuity](../include/mgs5vr/input_bridge.hpp) retains held readiness and requires fresh trigger release after tracking recovery. | Bolt cycle, continuous stereo reload and physical handling. General rifle movement/fire/reload and both primary-slot loss/recovery already have scoped evidence. |
| FP03 — forced contextual rotation | [VehicleViewHeading](../include/mgs5vr/vehicle_view.hpp), called by [head_camera](../src/head_camera.cpp), captures pre-Y/normal-action heading, tracks actual carrier turns and preserves exit yaw. | Four starting headings across Fulton/vehicle/table/sandbag actions, deliberate driving look/turn and physical seated/standing. Jeep and automatic ledge fixes are already implemented. |
| FP04 — corridor/body size | Fixed-length body, root-relative low-posture camera and cover-specific corrections exist. | Exact reported tight corridor remains undiagnosed; compare native capsule versus visible body with body on/off. No targeted corridor repair established. Do not weaken collision. |
| FP04 — cover | [native_cover](../src/native_cover.cpp) suppresses only automatic ready-to-peek movement, releases native cover when assistance is off, and preserves native readiness elsewhere. [input_bridge](../src/input_bridge.cpp) retains on-foot weapon routing; [head_camera](../src/head_camera.cpp) preserves low-posture footprint. | Wider surfaces/postures and physical exposure/native benefits; original figure identity unresolved. Do not diagnose restricted Mother Base lethal weapons as missing rendering. |
| FP05 — floating | [head_camera](../src/head_camera.cpp) lets helicopter-exit height settle; [arm_ik](../src/arm_ik.cpp) and [controller_rig](../src/controller_rig.cpp) preserve native leg lengths and solve shared pelvis/head/floor reach. | Physical standing/seated calibration and transitions. Scoped exit/posture corrections already passed; no leg scaling permitted. |
| FP06 — helicopter body | Mounted-body admission in controller_rig and carrier-relative seat/boarding fit in head_camera preserve native pelvis/legs and tracked upper body. | Extraction boarding and broader transit/headset continuity. Seated/disembark body repair is implemented, not an entirely open coding task. |
| FP07 — left iDroid | [controller_rig](../src/controller_rig.cpp), [idroid_attachment](../src/idroid_attachment.cpp) and [idroid_rig](../src/idroid_rig.cpp) use left cinematic grip/fingers and native connector/projector frames; [effect adapter](../src/idroid_effect_probe.cpp) fits the native cone. | Continuous both-eye readability/contact and physical orientation across configurations. Ordinary front/turned/stow observations do not certify all motion. |
| FP07/12 — stow/tutorial/camera | [native_actions](../src/native_actions.cpp) handles native stow ordering, close-input ownership, completed-guide cleanup and retained terminal scene handoff; head_camera handles retired-player/exit-notice ownership. | Forced unfinished lessons, earlier-account configurations and wider nested dialogs. Ordinary toggles and repaired exit notices already have scoped evidence; preserve native progress. |
| FP08 — Rocket Arm | [RocketArmSteering](../include/mgs5vr/rocket_arm.hpp) and xr_runtime steer from the left hand with stick override; render_camera/idroid paths retain flight view and handset. | Actual target impact, Blast Arm, wider contact/poses and physical review. Steering/display/return code exists. |
| FP09 — paper/pen | [cabin_studio](../src/cabin_studio.cpp) and controller_rig fit marker shaft/thumb-index contact and paper-edge pinch, with writing/typing/board pinning. | Wider hand orientations and prompt readability. Both-hand scoped contact/write/pin results retained. |
| FP10 — gallery/workbench/cassettes | cabin_studio retains front-slot prints, uses field-paper poses/rays and settles sheets flat; dropped markers use rotated extents. Physical cassette handling remains in [cabin_props](../src/cabin_props.cpp). | Wider stacking/cassette presentation and physical interaction; existing combined workbench passes are retained. |
| FP11 — ACC loadout holsters | controller_rig grabs existing native assemblies with either rendered palm, preserves native publication, returns exact mounts and requires fresh grip after interruptions. | Other loadouts and physical contact. Both rifle assemblies and both-hand interruption subcases already have retained coverage. |
| FP11/15 — mounted physical controls | controller_rig explicitly admits mortar `0x46` and machine gun `0x47`; [mounted_grip](../include/mgs5vr/mounted_grip.hpp) provides native-contact grab, aim servo, operating-hand ownership, release and stick override. | Additional types lack admission in this adapter. Confirm native mount/action/contact before extending; do not assume `0x45`. Mortar pitch/contact/impact and complete MG impact/contact remain unaccepted. |
| FP15 — mortar viewpoint | head_camera retains native mounted operator eye; controller_rig admits mounted body without forcing standing posture. | Wider headings, contact, translucent geometry, firing/impact and physical review. The 62.8 cm standing-eye mismatch is already repaired. |
| FP16 — small rocks | Native floor/posture/body support exists, but no targeted repair for the exact reported rock interruption was established by the audit. | Locate/reproduce in both directions; join floor, velocity, action and eye height. A route guard is not a diagnosed production defect. |
| FP17 — wrist coupling/reach | Acquired-supported-weapon-only torso correction, native clavicle reach, full arm/per-eye clearance and infeasible cross-head fallback in controller_rig/arm_ik. Garment driver translations follow solved arms. | Physical wrist isolation/reach and remaining sleeve crowding. These corrections exist; do not restart bare-wrist torso or cross-head-freeze implementation. |
| Latest body fallback | [forearm_visibility](../src/forearm_visibility.cpp) is connected to production draw hooks and hides upper sleeves for 20 exact admitted native outfit layouts when full body is off/unavailable. Native thigh/shin lengths stay fixed; garment branches follow solved joints. | Admission remains exact and outfit-specific; the 20 profiles expand implementation coverage, not live acceptance. Supported cuff loops have flat independently shaded caps and native attribute-slot aliases are handled. Other outfits/ambiguous boundaries and physical review remain open. The earlier startup-failing reconstruction remains excluded. |
| FP13 — free-roam route | Existing VR menu path: Missions -> Mission List -> Free Roam; [controls](CONTROLS.md) corrected. | Scoped resolved through on-foot arrival. No unlock/save modification or extra root menu required. Broader progression remains FP12. |
| FP14 — sortie head | [player_visibility](../src/player_visibility.cpp) restores native preview visibility and first-person concealment afterward. | Scoped resolved in development; preserve during adjacent transit/body review. |
| FP12 — remaining regressions | Physical binocular raised/marking events and native intel requests exist in [native_actions](../src/native_actions.cpp)/[optic_markers](../src/optic_markers.cpp); retained village marking advances the native lesson. Other systems remain inventoried in [GAMEPLAY_FEATURES](GAMEPLAY_FEATURES.md). | Full missions, other weapons/items/buddies/menus and historical reports need exact-build reconciliation. A lesson or source path is not a whole mission pass. |

## Next batch

Mounted adapter investigation, 25 October: owned `TppTutorial.lua` maps native
`OnPlayerGatling` to the `ANTI_AIRCRAFT` guide. The retained native emitter is
RVA `0x126d50b` inside the handler beginning at `0x126c570`. This is the event to
correlate with the next real anti-aircraft mount. It does not establish its
player action ID. The separate `0x45` writer belongs to another handler and has
not been connected to that event. Action `0x44` is associated with the verified
`OnPlayerSearchLight` handler; do not admit it as a stationary weapon.
The current adapter remains limited to the verified mortar/MG IDs. Next C++
work is to admit the anti-aircraft action only after correlating actual mounting,
its current native palm contacts and aim pose, then exercise both operating
hands, release, native aim limits and impact together. No candidate change or
new mounted acceptance is claimed by this offline investigation.

1. Prioritize the remaining body/arm presentation limits without undoing the
   fixed-length and tracked-garment repairs.
2. Diagnose intact mounted entry/native eligibility, then extend only confirmed
   missing type adapters. Use retained routes; do not repeat destroyed fixtures.
3. Reproduce exact rock/corridor and any newly observed progression failure.
4. Group justified repairs, build/check once, then review the changed groups and
   adjacent interactions together with physical Quest feedback.
5. Publish only the reviewed scope under RELEASE_PLAN's package/pin sequence.

Do not mark all rows complete from unit checks. Do not describe all rows as
unimplemented because hardware or breadth acceptance remains open.
