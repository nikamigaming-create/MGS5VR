# Quest 3 / Virtual Desktop feedback — 13 September 2026

Physical tester reports take precedence over earlier simulator-only results.
This is the current issue inventory, not a claim that all entries are fixed.

| Report | Current change / remaining work |
| --- | --- |
| Right-stick up sprints out of prone | Default Sprint moved to left-stick click; grip + left-click zoom consumes Sprint. |
| Binocular grip requires a 90-degree wrist twist | Default device pitch is -90 degrees (down). Both palms face the housing sides, knuckles point upward and fingers curl over the top. The initial counter-rotation pointed the fingers toward the eyepiece and was rejected; side-cup orientation is now preserved at every fit angle. Housing, ray and lens stay attached through final skin reattachment. Existing configs with explicit pitch 0 need -90. Headset ergonomics remain unverified. |
| Disable snap, restore smooth turning | `settings.turn_mode = native_smooth`; native yaw sensitivity, tracked HMD pitch. |
| Horse / vehicle forward decoupled from viewing | Mounted right stick now sends both native camera/turret axes, with the artificial snap offset removed. Native steering remains native, not head-relative driving. Physical result unconfirmed. |
| Body / camera center drifts; visible in cardboard box | Fixed a world-space snap-pivot accumulator that failed to follow native camera yaw; regression covers leaned snap → native yaw → recenter. Left grip + Menu clears the offset without changing facing. Collision-following roomscale and the tester's full cardboard-box case are not marked fixed. |
| Conflicts require starting and closing game | External `Edit-Controls.cmd`: live validation, invalid-save protection, prior-file backup. Valid saved edits now apply in the running game after all controls return to neutral; invalid edits preserve the last working layout. SIM exercised rejection and successful reload without restarting. In-game editing and clearer context visualization are not implemented. |
| Undocumented binding coexistence exceptions | Supplied tester file passes both the 13.3 and current checker. X is not forbidden in gameplay chords: Commands opening shares that active context and is consumed by a longer chord. Added regression coverage for direct-B pickup and grip+R3 / grip+X switch. Plain hold plus tap on the exact same button remains ambiguous and rejected. |
| Subtitles / enemy / objective markers often absent | Acquired people and A-Z waypoints reproject from native world positions using one snapshot for both eyes. SIM shows a trigger-placed A waypoint remaining on the landscape after binocular stow and head yaw. `settings.hud_mode` chooses full/binoculars_only/off world cues. General layout layers now have a separate source-head plane; wrist status and pickers stay separate. Captions did not appear in the native radio fixture and are not marked fixed. Other objective types and full native HUD parity remain open. |
| Enemy ghosts stretch into tails | UI workers now consume the complete source-eye projection, including depth, instead of mixing eye FOV with a restored desktop near plane. This fixes a concrete mismatch, but the silhouette-tail cause/result is not established. Not marked fixed. |
| Lens flares drift from lights during head motion | Native focal/aspect parameters now follow the same eye projection during each replay, with independent TPP/GZ field contracts and exact restoration. Previously only the raster matrix changed. This removes a concrete projection mismatch; complete flare/source-depth association and the tester's full motion case are not marked fixed. |
| Urgent combat/pickup should be single-button | The public default now uses the validated tester layout: native pickup/carry is bare B and binocular equip is grip+Y. Reload is the separate grip+B tap so a held B remains pickup/carry. Complete native mode also retains native B holds. Do not silently double-map support grip. |

No multiplayer testing or headset launch is part of this work. GZ tracked
first-person arms, weapon aiming and wrist presentation remain separate unfinished
adapters; complete native button access does not complete those adapters.

Front-end tracked arms are now excluded at native player publication, before
draw preparation; the title scene no longer contains floating controller hands.
The tracked rig is not applied to title animation. Gameplay restores the arm
groups and retains the existing opaque-hand policy and native shadows.

The requested recon policy now defaults to binoculars-only. Native scene intel
layers and reprojected markers share an explicit view role: ordinary world,
eye-aligned binoculars, or other optics. Holding the device down and aiming
through a weapon scope do not enable binocular intel. Captions, interaction
prompts, native menus and wrist status are separate. This policy does not add
the still-missing automatic target analysis or GZ UI adapter.

In the current SIM field case, binoculars-only removes the A waypoint and
scene-camera yellow glows from normal vision and restores the waypoint at the
ocular. A small native green person icon remains outside the device; complete
native marker/silhouette suppression is not yet claimed.

The first downward-fit SIM candidate retained a live ocular and support contact,
but its hand orientation was rejected by the user: fingers pointed toward the
eyepiece instead of over the top. It was not published as a release. Side-cup
contact supersedes that candidate; physical headset comfort remains unverified.

The corrected SIM view keeps both palms on the binocular sides with fingers
over the top, including support attach/release. The device remains pitched down
90 degrees relative to controller aim.

Pistol support now recognizes the native close palm contact (6.5 cm in the
tested handgun) without requiring a rifle's forward support cone. The firing
hand retains aim; weapon changes reset the acquired contact. SIM exercised
beside/forward engagement, opposing head/hand motion, release, reacquisition
and a reload input. Contact remained acquired during the motion segment with
no support-driven aim rotation. Other handgun variants and physical headset
play remain unverified; this is not an exhaustive weapon-completion claim.
