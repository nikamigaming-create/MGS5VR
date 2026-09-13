# TPP rendering work in progress

Local work on 13 September; this is not a claim about the current public DLL.

- Recon acquisition now uses a configurable, uninterrupted binocular dwell.
  Manual mark/clear remain separate, and looking at terrain cannot auto-place pins.
- The independent native layout-camera person cue now follows the binocular-view
  policy, in addition to scene-camera labels. This is separate from model glow.
- Native skinned-marker depth compression modifies only its root bone after
  copying a world-space palette. The VR adapter bypasses that compression for
  the verified multi-bone marker class. Single-bone and non-VR behavior remain native.
  A later live SIM view shows a normal-sized blue guard silhouette without the
  sky-length wedges. This is a bounded visual result, not a complete motion or
  wall-occlusion acceptance result.
- Recon clone opacity is scoped to each native scene replay. Changing mesh-group
  visibility at that point was too late and still leaked blue bodies into normal
  vision; the replacement uses the clone's native material-opacity parameters.
  Ordinary-eye captures now retain normal people without the blue overlay, and
  an acquired target label is visible through the physical binocular lens.
  Persistent native blue glow inside the lens still needs a positive capture.
  `binocular_actor_glow = 0` disables the body effect independently of labels.
- Title presentation retains its authored camera rather than moving into the
  decorative character's head. A delayed first player publication now initializes
  the floating panel as well. Immediate/delayed entry and gameplay restoration
  have regression tests. The current SIM candidate displays Continue without
  tracked arms around the title menu. The hospital backdrop itself has not been rerun yet.

No change here replaces the existing opaque hands, binocular side-cup fit,
pistol support-hand fix, weapon scope adapter or native stereo rendering.
Missing captions, remaining HUD/rendering issues and gameplay coverage are still
tracked in [tester feedback](QUEST3_TESTER_FEEDBACK.md) and [status](STATUS.md).
The native launcher/artwork work is a separate, additive change.

The reported PC-screen arm popping remains open. During a SIM head sweep the
left controller remained fixed in local space and moved outside the view; three
subsequent stationary submitted-eye samples retain both arms. This does not
establish that every PC-mirror frame is stable. The PC Present path currently
shows the native backbuffer, not an explicit copy of the completed eye pair.
