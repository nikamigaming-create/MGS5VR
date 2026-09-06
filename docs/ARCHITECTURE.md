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
  -> eye 0: native world/view + asymmetric projection, native scene draw, GPU copy
  -> eye 1: same transaction, second native camera/draw/copy
  -> restore native matrices; keep one desktop present registration
  -> observe exact FinishCommandList identities for each eye copy
  -> observe native ExecuteCommandList for both copies
  -> atomic shared texture array + immutable source eye poses/FOVs
  -> XR consumer cache -> two eye swapchains -> projection layer
```

Both draws use the same source, tracking and activation identifiers. Different native frames cannot be paired. The scene replay does not advance the game update, and checks that camera publication has not advanced during the pair. No alternate-eye scheduling, mono duplication, generated image or depth reconstruction is used. During active stereo, a present without a new complete pair retains the preceding stereo mailbox instead of replacing it with mono. The XR gate rejects missing, mismatched or stale pairs.

This prototype produced complete pairs and full-eye native gameplay in Meta XR Simulator. It still needs broader visual acceptance. Native temporal resources are shared: previous camera matrices are provisionally set to the current eye, but per-eye temporal effects, wider-FOV culling and scopes are unverified. The default-off [controller rig](CONTROLLER_RIG.md) modifies native skin publication, pins the eye source to that tracking packet, and supplies the ordinary firearm solver with an authored barrel-axis target. Wrist HUD and complete weapon/physical acceptance remain outstanding.

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
arm group. The body group uses verified native visibility functions; arm groups remain.
Restoration checks the same ownership and expected hidden state, so an appearance
replacement or native mask change is not overwritten. Shoulder/sleeve intrusion still
requires a proper first-person rig.

The experimental UI adapter carries the source eye through native UI job enqueue and
worker execution. Projection changes require an exact camera/view match and a fresh,
active source generation. Only some observed UI jobs match these checks. This is
unfinished groundwork: there is no HUD pixel extraction, wrist attachment, or proven
world-marker correction.

The optional private camera evidence directory enables bounded owner snapshots and getter-consumer records. The caller and instruction signatures are checked against the exact executable baseline; diagnostic getters return the native pointer unchanged. Observations distinguish camera publication from downstream reads, but are sampled summaries without a shared render-frame identifier. They must not be used as the source transaction for a tracked rig or HUD.

Native first-person aiming changes which pose one conditional getter returns. This is not sufficient to classify first-person gameplay, since title cameras can select the same storage. Affine pose conversion, inverse view, viewport publication and perspective builders are now integrated in the experimental scene path under exact executable signatures. Weapon and HUD state still need to join that source transaction.

## Contracts prepared for the engine adapter

- Poses are right handed, in meters, quaternion xyzw, +X right, +Y up, -Z forward. `compose(A_from_B, B_from_C)` yields `A_from_C`.
- Wrist pointer rays are transformed into the display plane, reject back-face/parallel/out-of-range/invalid hits, and map into actual texture pixels. Entry/exit hysteresis and dwell/cooldown guard visibility. These rules are unit tested and are not connected to the game's HUD.
- Source pose history joins on `(epoch, sequence)`. Missing, reset, overwritten, and duplicated histories cannot be substituted with the newest pose.
- Weapons derive a root from a calibrated authored primary-grip socket; the muzzle shares that root. Uncalibrated sockets and missing tracking return no solution. Retail skeletons, animation transitions, scopes, ballistics, and secondary grips still need investigation.
- A cinematic skip is a fresh, focused input edge for a stable cinematic generation with engine skip readiness. It must call the native semantic completion path, preserving mission cleanup and state. The implementation never guesses a skip RVA or simulates a desktop keypress.

## Work needed for full VR

1. Complete visual and timing acceptance of same-transaction native eye draws. Verify world scale, culling, temporal resources, scoped views, and fast six-axis motion.
2. Recover authoritative cinematic/video state and native skip completion. Test each cinematic class, including scripted mission gates and unskippable sequences, for progression integrity.
3. Identify the complete HUD/iDroid pixel source and semantic action dispatcher. Route that source onto an actual wrist display, provide cursor interaction and preserve world/background rendering during pause.
4. Bind controller aim to weapon presentation **and** gameplay muzzle/ballistics. Calibrate authored sockets and compatible hand/arm animations for every weapon family. Cover recoil, reload, ADS, scopes, throwing, melee/CQC, vehicle/mounted weapons and tracking loss.
5. Exercise the complete acceptance matrix in the simulator and then in a physical headset. A correct screenshot or advancing frame count does not establish motion stability or interaction correctness.

## Sources inspected

- [IHHook](https://github.com/TinManTex/IHHook), MIT, revision `71801f1cb861a1e1ebad051be222e34a5355b4fc`: documents the DirectInput proxy approach and targets executable 1.0.15.3. Used as research context; its engine addresses and Lua hooks were not incorporated.
- [Khronos D3D11 example](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/graphicsplugin_d3d11.cpp) and [session example](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/openxr_program.cpp): reference for graphics requirements, session lifecycle, actions and swapchains.
- Exact executable inspected locally using Ghidrust. Private evidence lives outside the repository and distribution. No game binary or extracted assets are included.
