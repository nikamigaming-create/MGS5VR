# VR performance

The target is 90 fresh stereo frames per second, with room below the 11.11 ms
display interval. A 90 Hz headset alone does not establish that the game renders
90 new frames. Settings and in-game acceptance are still being measured.

The latest physical Quest 3 test retained native **2560×1440**, Post Processing
**High** for native AA, and received positive user feedback. The user accepts
roughly 86 fresh FPS as a useful fidelity compromise; this is not a locked-90
claim. Keep DOF and motion blur disabled, SSAO and volumetric clouds off, effects,
lighting and shadows High, and textures/filtering/model detail Extra High.
Current binocular recordings use a separate 1920×1080 SIM run and restore the physical
profile afterward. Its performance numbers must not be substituted for 1440p
headset results. No upscaler, generated frames or third-party clarity plugin is
included.

The VR graphics adapter selects the engine's variable frame-rate option and
removes desktop V-sync from the game mirror. Its producer is capped at 120 FPS
to feed the asynchronous 90 Hz OpenXR consumer with a small margin. The cap also
applies to title and loading screens. Simulation delta time is not patched.
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
