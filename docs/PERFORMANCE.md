# VR performance

![MGS5VR field manual — original Snake artwork](images/field-header.svg)

The target is 90 fresh stereo frames per second, with room below the 11.11 ms
display interval. A 90 Hz headset alone does not establish that the game renders
90 new frames. Settings and in-game acceptance are still being measured.

The latest physical Quest 3 test retained native **2560×1440**, Post Processing
**High** for native AA, and received positive user feedback. The user accepts
roughly 86 fresh FPS as a useful fidelity compromise; this is not a locked-90
claim. Keep DOF and motion blur disabled, SSAO and volumetric clouds off, effects,
lighting and shadows High, and textures/filtering/model detail Extra High.
Older binocular recordings used a separate 1920×1080 SIM run. The independent
resolution adapter was subsequently checked at 2560×2560 in SIM with a 960×540
desktop preview. These are distinct runs, not physical-headset performance
claims. [Launcher resolution setup](LAUNCHER.md#resolution-without-changing-your-desktop)
reads the active OpenXR recommendation without changing Windows resolution or
using DSR. No upscaler, generated frames or third-party clarity plugin is included.

The VR graphics adapter selects the engine's variable frame-rate option and
removes desktop V-sync from the game mirror. The next local candidate adapts
the producer cap to the runtime display period: 96 FPS for 72 Hz, 120 FPS for
90 Hz and 160 FPS for 120 Hz. It stays bounded to 60–180 FPS and falls back to
120 FPS without an active runtime. Small period jitter does not reset pacing.
The cap also applies to title and loading screens. Simulation delta time is not patched.
Critical engine workers yield with `Sleep(0)` when idle; other worker delays
remain unchanged. A paired one-millisecond timer request prevents coarse
Windows sleep timing from limiting native frame production.
Where available, the native timer API also requests half-millisecond resolution,
following MGSVFix's worker-timing approach. Both timer requests are released at exit.
Where OpenXR exposes display refresh control, the mod requests 90 Hz only if the
runtime lists it as supported. Otherwise, set 90 Hz in the headset PC software.

`mgs5vr.log` reports five-second performance windows:

- `pairs_fps`: completed native stereo pairs.
- `interval_ms_mean_p95_max`: CPU intervals between completed pairs.
- `scene_gpu_elapsed_ms_mean_p95_max`: GPU timestamp span from the first eye's
  scene start through both native eye draws and copies. This is elapsed GPU
  timeline time, including any scheduling gaps, not a GPU utilization percentage.
- `scene_cpu_ms_mean_p95_max`: CPU wall time spent generating both eye scenes.
- `Native present ms`: time in capture, producer pacing and desktop presentation.
- `Mailbox ... ms`: acquire, copy-queue, flush and release mean/max times for
  the shared texture transfer, reported separately for producer and consumer.
- `submissions_fps`: OpenXR submissions while native VR is active.
- `new_pairs_fps`: submissions with a different native scene transaction.
- `repeated_or_empty` and `empty`: submissions without a new pair and submissions
  lacking a stereo layer, respectively.

GPU timestamps are sampled every fourth scene. Their clock query wraps actual
command-list playback; start/end markers follow the native deferred contexts.
Readback uses `D3D11_ASYNC_GETDATA_DONOTFLUSH` and never waits. These measurements
do not include headset compositor/encoding cost or prove physical display timing.

For acceptance, exercise movement, head turns, combat, wrist selection and area
transitions. Record runtime, render resolution, graphics settings, sustained
frame rate, slow-frame percentiles and repeated frames. A quiet checkpoint or
SIM recording cannot establish a guarantee for the entire game.

## Provenance

The next candidate includes the adaptive-period idea and pacing calculation
from [s-ilent's contribution](https://github.com/s-ilent/MGS5VR/commit/7dd09a2470c6dd6fb0194a2a14aef32181918596),
with producer bounds, period jitter handling and reset on session loss/exit.
Its asynchronous immediate-context worker has not been imported. Native D3D11
context ownership and paired-eye texture lifetime must remain correct.

The variable graphics-option approach comes from Lyall's MIT-licensed
[MGSVFix](https://codeberg.org/Lyall/MGSVFix),
[shared graphics adapter](https://codeberg.org/Lyall/MGSVFix/src/branch/main/src/games/common.cpp).
The graphics-option and critical-worker instruction sites were independently
checked in the supported TPP 1.0.15.4 executable and are protected by exact
signatures. The worker bridge preserves incoming flags and other registers.
MGS5VR supplies its
own bounded producer pacing and does not load a second ASI/proxy mod.
The [license](../licenses/MGSVFix.txt) accompanies the package.

MGSV's native AA is enabled through Post Processing High; depth of field and
motion blur can remain disabled. See the
[NVIDIA graphics guide](https://www.nvidia.com/en-us/geforce/news/metal-gear-solid-v-the-phantom-pain-graphics-and-performance-guide/).
Extra High textures and filtering are separate from render resolution, AA and
draw distance. Optional third-party graphics or texture packs have not been
validated with this VR build.

## Current profiling and threading work

The render producer and OpenXR consumer already run separately. Native scene
jobs record deferred command lists; texture copying and optic drawing already
execute on the GPU. Moving those API calls to another CPU thread does not remove
their GPU cost. The game's immediate context must keep one synchronized owner.

A 30-second stationary SIM sample at 1920×1080/90 Hz, with the iDroid paused,
reported these medians across six log windows: 88.38 completed stereo pairs/s,
85.4 fresh submitted pairs/s, 2.44 ms scene CPU time, 4.47 ms GPU elapsed time,
and 2.20 ms producer keyed-mutex acquisition. Median window frame p95 was
13.15 ms. These measurements overlap and must not be added. This is a menu
baseline, not a combat benchmark or a Reverb G2 performance result.

The strongest measured synchronization candidate is the shared texture handoff.
A bounded buffer pool could let rendering and XR copy overlap, but needs a
matched comparison covering GPU memory use, stale-frame latency, resolution
changes, and matching pixels/metadata for both eyes. Parallelizing whole eye
renders also requires isolating native camera, skin and UI state first. Neither
change is represented as shipped or as a measured FPS improvement.

Use the read-only report tool on a tester's log, optionally restricting epoch
milliseconds to one comparable route:

```powershell
python tools/summarize-performance.py 'D:\SteamLibrary\steamapps\common\MGS_TPP\mgs5vr.log' --output performance.json
```

It preserves individual timing windows and distinguishes fresh frames from
runtime submissions. For an A/B run, keep runtime, resolution, graphics, save,
route and capture load identical. Record normal play, binoculars and iDroid
separately. A minimized game mirror stops native rendering in the current game;
exclude that interval from GPU/CPU bottleneck comparisons.
