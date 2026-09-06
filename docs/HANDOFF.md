# Development handoff - 2026-09-06

The latest installed build adds centered projection and native listener tracking. Read
[SIM_HUD_REVIEW.md](SIM_HUD_REVIEW.md) for the evidence and open gates, and
[CONTROLS.md](CONTROLS.md) for the complete Touch interaction list. The initial
physical Quest 3 reference remains in [HEADSET_REVIEW.md](HEADSET_REVIEW.md).

## Installed and observed

- Real native weapon/ammo/status on the left forearm, joined to the same skin/eye
  publication. Back-facing text is hidden. Flat reticles and destination labels
  are suppressed; native scene-camera people cues remain.
- Anatomical palm binding, head-relative shoulders, forearm roll, native animation
  restoration, and native support contact during reload/bolt cycles.
- Verified player hip mount suppression removes the stowed rifle stock in the
  tested pistol/prone views. Held weapons still draw and fire.
- Right grip readies, right trigger fires, left grip supports, B reloads.
  Left trigger plus left-stick direction selects equipment. Zoom is reserved;
  there is no native scope/binocular screen transition in tracked VR.
- Full menus use the manual large-screen toggle and native controls. Map zoom,
  tab navigation, close, pause and return to VR were exercised.
- All five local suites passed. CI builds all targets and runs four non-GPU suites.

Local DLL SHA256:
`6A1EC3044DFAC1EE473C5ED9C2EEE62BD31D54F3B4548CE543BF997F90E49AA4`.
The last physical-test rollback is retained locally with DLL SHA256
`70779290C3A4872A458E73C03111B79B012C69BDEE7222CD4AF66B84C1460C15`.
No game executable, archives or saves were patched. Installation hashes were
updated to the actual installed DLL/config. All experiment flags remain off by
public default; the local test enables the camera, rig and wrist HUD explicitly.

## Follow-up evidence and remaining work

The latest `artifacts/lighting-followup-15s/simulator.mp4` is 14.803 seconds,
687,646 bytes, silent H.264, with 58 reviewed distinct frames at 3.850 captures
per second. There were no black frames or capture/action/encoder errors. It shows
sky yaw, left-arm status, automatic rifle support, fire/reload, pistol selection
and head lean. It is not a full rig, physical lighting or audio-localization pass.

The preceding `artifacts/forearm-followup-15s/simulator.mp4` is 14.883 seconds,
580,622 bytes, with 60 distinct frames at 3.967 captures/second. All frames were
reviewed; no raw frame sequence was written. It shows automatic support across
the rifle/pistol swap, left forearm reading, reload and wrist/head movement.
Both-eye live checks supplement the single-eye clip. Garment/body intrusion near
the pistol support pose remains open. This clip predates the projection fix.
The supplied physical recording's sky rectangle was reproduced with asymmetric
render projection, including with Volumetric Clouds off. Centering the render
coverage and carrying that same FOV with the submitted pixels removes the rectangle
in both sampled SIM eyes. Clouds were restored to On and verified by reopening
settings; head yaw/pitch/lean, left HUD reading, rifle support/fire/reload and horse
dismount were checked. Shadow/culling/temporal and physical acceptance remain open.
The native primary and virtual audio setters now consume the source center-head
pose. SIM yaw/pitch/lean, horse-to-foot transition and theatre restoration were
checked. Output/HRTF/occlusion and physical localization remain unproven.

D-Horse mount/walk/gallop/dismount and bounded mounted rifle shots have SIM
observations. Vehicle input routing is implemented and contract-tested, but no
vehicle has been driven. Native player death interrupted a village approach;
checkpoint recovery succeeded. Use the road for the next approach and establish
the cause before claiming travel acceptance. Audio listener camera readers were
identified at native caller RVAs 0x438123 and 0x43813f. Observer getters remain
unmodified; the downstream native listener setters receive the guarded head pose.

## Earlier private short clip

`artifacts/sim-polish-15s/simulator.mp4`: 14.973 seconds, silent H.264, 688 x 720,
561,573 bytes. The encoder streamed actual composited-eye PNGs in memory and wrote
no raw frame sequence. Capture/action timestamps and verification metadata sit
beside the clip. All 56 decoded frames were reviewed; they are distinct, with no
fully black frame. Capture cadence was 3.676 images/second, largest packet gap
0.304 seconds. This does not demonstrate headset smoothness or 60 FPS capture.

The clip shows support, rifle fire/reload, changing arm ammo, stow, pistol
selection/fire/bolt cycle, and independent head movement. Main-view flat markers
are absent. Angular cuff shapes remain visible around stow/selection; a transient
free-hand view in the wider checks needs higher-cadence scrutiny. Full rig
acceptance is therefore open. Do not claim that every weapon or interaction works.

## Resume

Use the existing Continue/Resume checkpoint and equipped gear, with the native
Action Type controller layout. Use OpenXR semantic input; no Windows key/mouse or
focus automation. The per-process simulator selection preserves the system Oculus
runtime. The final simulator session was left paused. Do not silently start a
physical session while the user requested simulator work.

The known forward rifle grip for simulator inspection is local position
`[0.16,-0.25,-0.30]`, quaternion xyzw
`[0.61595203,-0.00760909,0.07982154,0.78369236]`. Left HUD inspection is
`[-0.10,-0.10,-0.42]`, `[0,0.70710678,0,0.70710678]`. The earlier negative
90-degree right-grip pitch points the weapon backward and must not be reused.
The simulator head setter also moves controllers: reapply explicit local hand
poses when testing independent head motion. Keep private probes and game-derived
media outside the public source; retain one rollback and avoid raw frame folders.

Next work is cuff/garment polish, physical anatomical fit, higher-cadence rapid
motion and live tracking-loss checks, weapon impacts/obstruction and remaining
weapon families. Spatial damage/subtitle/context feedback, interactive wrist
menus, authored stereo optics and automatic cinematic handling remain unfinished.
