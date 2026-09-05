# Runtime and game integration

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

This prototype produced complete pairs and full-eye native gameplay in Meta XR Simulator. It still needs broader visual acceptance. Native temporal resources are shared: previous camera matrices are provisionally set to the current eye, but per-eye temporal effects, wider-FOV culling and scopes are unverified. Native ADS supplies the current rifle and arms; controller-driven rig, ballistics and wrist HUD are not yet attached to this frame transaction.

## Camera diagnostics

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
