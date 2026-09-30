# Coco patch integration status

The six supplied patches are preserved privately with their original hashes and attribution. Their behavioral changes have been integrated into the current tree with the ownership, tracking and camera publication checks required by the newer code. The old full files were not substituted for the current implementation.

| Supplied change | Integrated behavior |
| --- | --- |
| `player_visibility`: hidden-state lock and 250 ms restore | The watchdog restores still-owned groups after 250 ms without first-person publication only when current presentation policy allows it. Pause retains concealment. Owner/model/group checks and mutex protection remain active. |
| `head_camera`: bob suppression and fit | Uses Coco's 0.20 s posture response, 0.15 m vertical deadband, 0.35 m backward offset and 0.25 m height offset. Ordinary horizontal gait oscillation is held; large posture changes re-anchor. Native actor movement and physical head tracking are not smoothed. |
| `head_camera`: cinematic comfort | Authored noninteractive shots use 0.25 s position and 0.45 s yaw response, a 1.2 rad/s yaw cap and a level horizon. A position jump over 4 m or yaw jump over 0.9 rad is treated as a cut. Interactive hospital look retains native pitch. Camera/activation changes and stale samples reset the filter. |
| `head_camera`, `ui_renderer`, `input_bridge`: stale demo recovery | A shared native snapshot carries recovery identity/generation. Explicit inactive and playable native results, finished preparation and two seconds of fresh matching camera/player publication are required. Active demos, menus, missing evidence and identity changes cancel recovery. No independent mutable override flag is used. |
| `controller_rig`: hand tracking recovery | A visual-only cache bridges up to 250 ms of controller loss and blends reacquisition over 100 ms. It resets on owner/model/activation/reference-space changes and clock discontinuity. Cached hands never authorize firing, throwing, wrist interaction, melee or steering. |
| `controller_rig`: stationary pelvis and legs | After native on-foot movement stops, the rendered pelvis/lower-body pose follows the upper-body pivot. Stance adjustment is bounded, horizontal correction is capped at 0.5 m and vertical correction is zero. It does not change the actor transform or collision. Menus, vehicles, horses, authored shots and prone presentation are excluded. |
| `arm_ik`: shoulder fit | Uses Coco's shoulder offset `{0, -0.23, -0.04}`. Both sleeve roots receive the same rigid upper-body transform. |

The hand continuity, stationary lower-body geometry, bob suppression, physical-head response and cinematic behavior have direct native contract cases in `tests/core_tests.cpp`. The separate CTest groups `player_visibility_watchdog` and `stale_demo_recovery` exercise visibility policy and real HeadCamera recovery integration.

Contract checks establish the implementation boundaries; they do not establish physical headset comfort. The exact tested build, live simulator observations and remaining mission cases are recorded in the release notes and community evidence ledger. Mission 1's experimental native-binocular bridge was withdrawn and is not part of this integration.
