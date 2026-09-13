# Quest 3 / Virtual Desktop feedback — 13 September 2026

Physical tester reports take precedence over earlier simulator-only results.
This is the current issue inventory, not a claim that all entries are fixed.

| Report | Current change / remaining work |
| --- | --- |
| Right-stick up sprints out of prone | Default Sprint moved to left-stick click; grip + left-click zoom consumes Sprint. |
| Binocular grip requires a 90-degree wrist twist | Separate controller aim from authored anatomical palm contact; palms/fingers cup the housing. Final skin reattachment moves housing, ray and lens together. Headset fit needs the tester's confirmation. |
| Disable snap, restore smooth turning | `settings.turn_mode = native_smooth`; native yaw sensitivity, tracked HMD pitch. |
| Horse / vehicle forward decoupled from viewing | Mounted right stick now sends native horizontal camera input, with artificial snap offset removed. Native steering remains native, not head-relative driving. Physical result unconfirmed. |
| Body / camera center drifts; visible in cardboard box | Existing normal-VR recenter is left grip + Menu. Need reproduce repeated snap/lean/native-yaw transitions and inspect the character center, not only the eye pivot. Not marked fixed. |
| Conflicts require starting and closing game | External `Edit-Controls.cmd`: live validation, invalid-save protection, prior-file backup. In-game editing and clearer context visualization are not implemented. |
| Undocumented binding coexistence exceptions | Supplied tester file passes both the 13.3 and current checker. X is not forbidden in gameplay chords: Commands opening shares that active context and is consumed by a longer chord. Added regression coverage for direct-B pickup and grip+R3 / grip+X switch. Plain hold plus tap on the exact same button remains ambiguous and rejected. |
| Subtitles / enemy / objective markers often absent | Current wrist path explicitly suppresses most native flat HUD layers (`src/ui_renderer.cpp`). Binocular waypoints/person distances exist but are not full native HUD parity. Restore appropriate stereo subtitle/world-marker paths; not fixed. |
| Enemy ghosts stretch into tails | Suspected scene/UI skin or projection ownership issue, not established. Needs matched per-eye draw inspection; do not hide it by disabling unrelated actors. Not fixed. |
| Lens flares drift from lights during head motion | Inspect source versus per-eye post-process projection and source-depth association. A latest-HMD offset is not a valid fix. Not fixed. |
| Urgent combat/pickup should be single-button | Tester file already maps native pickup/carry to bare B and moves binocular equip to grip+Y. That layout validates; its explicit bindings are preserved. The public default still uses grip+B. Complete native mode also retains native B holds. Do not silently double-map support grip. |

No multiplayer testing or headset launch is part of this work. GZ tracked
first-person arms, weapon aiming and wrist presentation remain separate unfinished
adapters; complete native button access does not complete those adapters.
