# TPP rendering work in progress

Local work on 13 September; this is not a claim about the current public DLL.

- Recon acquisition now uses a configurable, uninterrupted binocular dwell.
  Manual mark/clear remain separate, and looking at terrain cannot auto-place pins.
- The independent native layout-camera person cue now follows the binocular-view
  policy, in addition to scene-camera labels. This is separate from model glow.
- Native skinned-marker depth compression modifies only its root bone after
  copying a world-space palette. The VR adapter bypasses that compression for
  the verified multi-bone marker class. Single-bone and non-VR behavior remain native.
  The first SIM captures after the change have no sky-length wedges with a marked
  guard in view. Persistent glow, wall occlusion and complete lens-only model-glow
  routing still require verification; absence of glow is not treated as success.
- Title presentation retains its authored camera rather than moving into the
  decorative character's head. A delayed first player publication now initializes
  the floating panel as well. Immediate/delayed entry and gameplay restoration
  have regression tests. The hospital backdrop itself has not been rerun yet.

No change here replaces the existing opaque hands, binocular side-cup fit,
pistol support-hand fix, weapon scope adapter or native stereo rendering.
Missing captions, remaining HUD/rendering issues and gameplay coverage are still
tracked in [tester feedback](QUEST3_TESTER_FEEDBACK.md) and [status](STATUS.md).
The native launcher/artwork work is a separate, additive change.
