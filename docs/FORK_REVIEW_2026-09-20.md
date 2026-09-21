# Fork review — 20 September 2026

Reviewed every visible branch in all five GitHub forks against upstream
`e20d92223a0a6dfccd8d75eac9d4c7841e635b75`. Inventory and compare results are
in `artifacts/fork-review-20260920`. This is a source review, not a claim of
headset validation of those branches.

The user plans to use both community forks and the two supplied C++ files in
the **next build**, after the approved September 20 package. Decisions below
identify integration work and conflicts, not a decision to discard contributions.
Preserve author attribution and reconcile each change with the current rig.

| Fork | Unique work | Decision |
| --- | --- | --- |
| [s-ilent](https://github.com/s-ilent/MGS5VR/tree/perf/adaptive-frame-pacing) | Adaptive pacing, mailbox worker, diagnostics | Adaptive pacing is worth a bounded integration. Do not import the worker: its final commit removes D3D11 immediate-context synchronization and relies on DXVK behavior. Current native D3D11 must remain supported. |
| [vinionsinho](https://github.com/vinionsinho/MGS5VR/tree/movement-stabilization-test) | Native head animation filter | Keep the useful discontinuity/reset ideas for comparison. Do not layer this filter over the existing root-relative head stabilization: that would add a second filter and change stance response. |
| [lukebluetiger](https://github.com/lukebluetiger/MGS5VR) | Older grenade/support-hand lab and README branches | Compare individual fixes with the retained local lab branches. Do not replace the current rig with the older prototype; it predates subsequent interaction and cabin fixes. |
| [kyus2001](https://github.com/kyus2001/MGS5VR) | Main matches upstream | Nothing unique to import. |
| [zengiswj](https://github.com/zengiswj/MGS5VR) | Main is 22 commits behind | Nothing unique to import. |

The s-ilent duplicate-frame check is inside keyed-mutex ownership, which is
correct, but the existing producer already advances the sequence before
releasing key 1. A second consumer cannot acquire key 1 until a new publish.
The patch alone therefore does not establish a performance improvement.

The pacing change derives a producer period from OpenXR's display period.
Before integrating, bound the supported cadence and reset the fallback on
session loss; a predicted period should not allow an unbounded engine rate.
The asynchronous publish worker additionally needs source-texture lifetime,
immediate-context serialization, and command completion validation.

## Attached source files

`controller_rig (1).cpp` and `head_camera (2).cpp` were received and compared
with the current source after normalizing formatting. They are useful reference
material, not drop-in replacements.

- The controller attachment adds hook-failure cleanup and a camera-only fallback.
  Those merit individual review. Its support-hand inspection release would
  restore a behavior already corrected in the current implementation.
- It omits newer cabin collision/bounds definitions and transition handling.
  Replacing the whole file would discard working changes and can break callers.
- The wrist-panel mount itself matches the current code. The attachment does
  not by itself repair a blank display or incorrect menu input routing.
- The camera attachment contains root-relative stabilization, but locks the
  horizontal animation offset and omits current transition handling. Keep the
  current stance-following behavior until a matched movement test demonstrates
  a better alternative.

## Public issue inventory

Open gameplay requests at review time: world-marker size (#10), left-handed
mode (#8), and Ground Zeroes support (#7). Closed reports for smooth turn (#9)
and Virtual Desktop recenter tilt (#6) still belong in regression checks.
Issue closure is not evidence that the current installed build passes them.
No issues, comments, or pull requests were modified during this review.
