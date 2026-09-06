# Runtime and game integration

The core includes independent transforms for a native hand's palm frame and for
placing native UI clip coordinates on a world-space panel. The palm frame uses
the wrist and index/little knuckles, with a provisional center 55% of the way
from wrist to knuckle midpoint. It follows the [OpenXR grip-axis convention](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html#semantic-paths-standard-pose-identifiers).
It does not calibrate from the camera or an activation pose. Panel math retains
the native UI's homogeneous layout coordinates while giving each eye its own
view and projection. Tests cover rigid-transform invariance, palm placement,
panel extent/depth, and eye disparity. These primitives alone do not establish
an accepted hand fit or a working in-game wrist HUD.

The arm solver can explicitly transport wrist roll into the forearm while
preserving its solved long axis. Existing callers retain the earlier swing-only
behavior unless they opt in. The independent equipment modifier maps native
equipment directions and shoulder actions, suppressing movement and held button
leakage until release. Contract tests cover both primitives; their runtime
integration and the visual rig still require simulator acceptance.

The installed executable baseline is SHA256 `085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`, file version 1.0.15.4, Steam build 24176213. Engine offsets from 1.0.15.3 must not be applied to it. `profiles/tpp-1.0.15.4.json` deliberately records unverified hooks as null.

## Default theatre pixel path

```text
Game D3D11 Present, before real Present
  -> immediate-context CopyResource / MSAA resolve
  -> shared keyed-mutex texture (key 0 -> 1), epoch + sequence
  -> separate compositor thread and D3D11 device on the XR adapter
  -> consumer-owned cache (key 1 -> 0)
  -> acquired/waited OpenXR sRGB swapchain image
  -> released swapchain image
  -> world-anchored composition quad in LOCAL space
  -> xrEndFrame
```

The producer never waits for the compositor. If the mailbox is occupied, it drops the new capture. The consumer retains valid pixels when the producer stops; XR event polling, tracking and frame submission run independently. Game immediate-context pipeline bindings are untouched. Resize/reset creates a new texture epoch; no backbuffer reference is retained across Present. Both devices must have the same adapter LUID. RGBA8/BGRA8 desktop output and a matching sRGB runtime format are required; HDR/color conversion and cross-GPU capture are unsupported.

The default mode is a mono video surface in a stereo environment. Rendered cinematics and videos appear on that surface. The opt-in native scene mode uses the separate path below; automatic game-state switching is not implemented.

## Experimental native stereo pixel path

```text
One native camera publication + one OpenXR tracking snapshot
  -> native scene-draw hook, inside the native render job
  -> eye 0: native world/view + centered enclosing projection, native scene draw, GPU copy
  -> eye 1: same transaction, second native camera/draw/copy
  -> restore native matrices; keep one desktop present registration
  -> observe exact FinishCommandList identities for each eye copy
  -> observe native ExecuteCommandList for both copies
  -> atomic shared texture array + immutable source eye poses/FOVs
  -> XR consumer cache -> two eye swapchains -> projection layer
```

Both draws use the same source, tracking and activation identifiers. Different native frames cannot be paired. The scene replay does not advance the game update, and checks that camera publication has not advanced during the pair. No alternate-eye scheduling, mono duplication, generated image or depth reconstruction is used. During active stereo, a present without a new complete pair retains the preceding stereo mailbox instead of replacing it with mono. The XR gate rejects missing, mismatched or stale pairs.

The centered render FOV encloses each runtime view and is carried unchanged with
its pixels into projection-layer submission. This removed the sampled screen-fixed
sky rectangle. Native source/culling projection and shared temporal resources still
need separate acceptance; wider render coverage alone does not prove those passes.

## Native listener publication

The version-gated camera publisher selects its listener at publisher+0x60. Its
camera getters feed primary setter 0x1d6a490 (return 0x438132) and virtual setter
0x1d6a550 (return 0x43814e). The primary setter calls the native audio API, then
copies the accepted pose to listener+0x20/+0x30; the virtual setter copies to
+0x40/+0x50. The adapter passes an aligned copy of the same center-head pose used
by the source camera. It leaves raw camera storage and observer getters unchanged.

Only the exact primary caller, source camera pointer and unchanged source pose
qualify. Explicit alternate primary transforms stay native. The virtual call must
follow a successful primary update on the same listener and camera publication.
Both require active, fresh, matching stereo tracking. Theatre, pending activation,
tracking suspension and unmatched callers retain the original setter arguments.
Diagnostic evidence records the exact submitted and native-consumed poses, not
just invocation counts. Headset output, HRTF, occlusion and perceived localization
remain separate physical acceptance gates.

This prototype produced complete pairs and full-eye native gameplay in Meta XR Simulator. It still needs broader visual acceptance. Native temporal resources are shared: previous camera matrices are provisionally set to the current eye, but per-eye temporal effects, wider-FOV culling and scopes are unverified. The default-off [controller rig](CONTROLLER_RIG.md) modifies native skin publication, pins the eye source to that tracking packet, and supplies the ordinary firearm solver with an authored barrel-axis target. The optional weapon/status wrist HUD shares that source. Complete menu, weapon and physical acceptance remain outstanding.

## Camera diagnostics

The player-camera publication also supplies the player's world transform and animated
head-bone transform. The opt-in head camera validates these rigid transforms and joins
the derived head position to the exact native camera object and source pose before
applying the HMD delta. It replaces the third-person boom position without holding ADS.
Tracked poses older than 150 ms suspend writes/submission while preserving the origin
and activation; fresh tracking resumes. Camera identity or matrix mismatches still cancel.

Player visibility follows the camera's player pointer to its appearance component and
bounded model collection, checking the ownership backlink and expected runtime types.
The separate head model is hidden only when it contains the head group and no body or
arm group. Verified normal-only group functions remove the body and head from the
main draw list while preserving their native shadow list and global visibility mask.
The normal-only function is independently identified by the engine's SHADOW_ONLY
draw-mode caller; no shadow geometry is synthesized. Arm groups remain visible.
Restoration checks the same ownership and expected named-group flags, so an appearance
replacement or native visibility change is not overwritten. It never enables a
native-disabled shadow. Anatomical palm frames,
head-relative shoulder anchors and restored animation inputs now reduce the
observed sleeve intrusion. Garment polish and full rig acceptance remain open.

The experimental UI adapter carries the source eye, view and solved forearm panel
through native UI job enqueue and worker execution. It requires an exact camera
identity and fresh active generation. Scene-camera UI uses the captured view even
if the shared native camera has since been restored. Under `wrist_hud_experiment`,
verified artificial layout-camera orders 146-148 draw actual weapon/status pixels
through a projection onto the left forearm; other flat gameplay UI is suppressed.
The back of the panel is culled. No duplicate HUD texture, fabricated values or
new overlay pose is substituted. Full menus use the large-screen mode; pointer
interaction and spatial damage/subtitle/context feedback are still incomplete.

The optional private camera evidence directory enables bounded owner snapshots and getter-consumer records. The caller and instruction signatures are checked against the exact executable baseline; diagnostic getters return the native pointer unchanged. Observations distinguish camera publication from downstream reads, but are sampled summaries without a shared render-frame identifier. They must not be used as the source transaction for a tracked rig or HUD.

Native first-person aiming changes which pose one conditional getter returns. This is not sufficient to classify first-person gameplay, since title cameras can select the same storage. Affine pose conversion, inverse view, viewport publication and perspective builders are integrated under exact executable signatures. The current rig and weapon HUD join that source transaction; unsupported gameplay states remain unverified.

## Contracts prepared for the engine adapter

- Poses are right handed, in meters, quaternion xyzw, +X right, +Y up, -Z forward. `compose(A_from_B, B_from_C)` yields `A_from_C`.
- Wrist pointer rays are transformed into the display plane, reject back-face/parallel/out-of-range/invalid hits, and map into actual texture pixels. Entry/exit hysteresis and dwell/cooldown guard visibility. These rules are unit tested and are not connected to the game's HUD.
- Source pose history joins on `(epoch, sequence)`. Missing, reset, overwritten, and duplicated histories cannot be substituted with the newest pose.
- Weapons derive a root from a calibrated authored primary-grip socket; the muzzle shares that root. Uncalibrated sockets and missing tracking return no solution. Retail skeletons, animation transitions, scopes, ballistics, and secondary grips still need investigation.
- A cinematic skip is a fresh, focused input edge for a stable cinematic generation with engine skip readiness. It must call the native semantic completion path, preserving mission cleanup and state. The implementation never guesses a skip RVA or simulates a desktop keypress.

## Work needed for full VR

1. Complete visual and timing acceptance of same-transaction native eye draws. Verify world scale, culling, temporal resources, scoped views, and fast six-axis motion.
2. Recover authoritative cinematic/video state and native skip completion. Test each cinematic class, including scripted mission gates and unskippable sequences, for progression integrity.
3. Extend the existing weapon/status forearm interface to interactive iDroid, spatial contextual feedback and cursor interaction. Preserve world/background rendering during wrist use.
4. Bind controller aim to weapon presentation **and** gameplay muzzle/ballistics. Calibrate authored sockets and compatible hand/arm animations for every weapon family. Cover recoil, reload, ADS, scopes, throwing, melee/CQC, vehicle/mounted weapons and tracking loss.
5. Exercise the complete acceptance matrix in the simulator and then in a physical headset. A correct screenshot or advancing frame count does not establish motion stability or interaction correctness.

## Sources inspected

- [IHHook](https://github.com/TinManTex/IHHook), MIT, revision `71801f1cb861a1e1ebad051be222e34a5355b4fc`: documents the DirectInput proxy approach and targets executable 1.0.15.3. Used as research context; its engine addresses and Lua hooks were not incorporated.
- [Khronos D3D11 example](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/graphicsplugin_d3d11.cpp) and [session example](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/openxr_program.cpp): reference for graphics requirements, session lifecycle, actions and swapchains.
- Exact executable inspected locally using Ghidrust. Private evidence lives outside the repository and distribution. No game binary or extracted assets are included.
