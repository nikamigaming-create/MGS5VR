# Advanced setup and development

For normal play, start with the [README quick start](../README.md). This guide
covers individual options, diagnostics and building from source.

## Installer options

`-EnableVR` enables the existing tracked VR configuration: OpenXR presentation,
the camera observer, native head tracking/stereo, tracked hands and weapons, and
the left-wrist HUD. It is a shortcut for all five switches below. It does not
change the runtime binary or mark incomplete features as accepted.

| Switch | Purpose and dependencies |
| --- | --- |
| `-EnableTheatrePreview` | Present the game on a large OpenXR screen. |
| `-EnableCameraObserver` | Enable the version-checked camera observer; this alone does not enable head tracking. |
| `-EnableHeadCameraExperiment` | Enable native head tracking/stereo; requires the two preceding options. |
| `-EnableControllerRigExperiment` | Enable tracked hands and weapons; requires the head-camera experiment. |
| `-EnableWristHudExperiment` | Enable the left-forearm HUD; requires the controller rig. |
| `-CameraEvidenceDir` | Absolute private directory for bounded camera snapshots; requires the observer. |

Without switches, the installer leaves all experiments disabled. The supported
TPP 1.0.15.4 executable has SHA256
`085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45`.
The installer checks this before copying files and refuses existing
`dinput8.dll`, `mgs5vr.ini` or `mgs5vr-install.json` files. It records installed
hashes so removal can preserve modified files. It cannot chain IHHook or another
DirectInput proxy. Activation is restricted to `mgsvtpp.exe`, never `mgsvmgo.exe`.

## PowerShell blocks the installer

Use the ZIP from this project's release page and review its scripts. If Windows
marked the download as blocked, right-click the ZIP, open **Properties**, select
**Unblock** if offered, then extract it again. If PowerShell reports that running
scripts is disabled, the following setting applies only to that PowerShell
session; run the installer again afterward:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
```

This does not change the machine's policy. Organization policy can still take
precedence. See Microsoft's [execution policy documentation](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies).

## Runtime and resolution

A simulator is not required for headset play. Connect through your headset's PC
software and select it as the active OpenXR runtime. The physical baseline uses
Quest 3, Touch controllers and the Meta PC runtime. Other hardware remains
unverified; the controls assume the game's **Action Type** layout.

The positively reviewed baseline used native 1920x1080 rendering. The current
2560x1440 test keeps the same DLL; its visual improvement and sustained performance
await physical feedback. Close the game before requesting that mode:

```powershell
$mgsRuntime = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1').ActiveRuntime
.\tools\launch-simulator.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -RuntimeManifest $mgsRuntime -RenderWidth 2560 -RenderHeight 1440
```

Despite its name, this launcher uses the supplied runtime. A physical run omits
`-Headless` and `-OperatorDir`. It selects Windowed mode, backs up changed graphics
settings in the extracted mod's `artifacts` folder, and preserves the global
OpenXR runtime selection. If more than one Steam account has a graphics config,
supply `-GraphicsConfig` with the chosen account's absolute
`userdata/<account>/287700/local/TPP_GRAPHICS_CONFIG` path.

Check `Game Present ... size=2560x1440` in the game's `mgs5vr.log`: the game may
substitute unsupported display modes. Enlarging an XR image alone cannot recover
detail missing from the native render. The [sharpness review](HEADSET_SHARPNESS_REVIEW.md)
records the observed dimensions, performance and remaining anti-aliasing test.

## Large screen, menus and transitions

Tap left Menu for iDroid; hold it for at least 0.55 seconds for pause. These use
the large game screen. A confirms, B goes back, sticks navigate, grips act as
LB/RB, and triggers retain LT/RT. Close iDroid with B or hold Menu again to unpause.
Tracked VR returns when the same player camera resumes. Release held inputs once
before moving or firing again. The manual left-grip + left-stick-click toggle
remains available; switching VR off inside a menu disables automatic return.

Hold both grips and click the right stick to recenter the large screen. To resize
it, edit `mgs5vr.ini` with the game closed: defaults are `width_cm=800` and
`distance_cm=600`, with a supported range of 100-3000 cm. Other controller profiles
have incomplete bindings. Cinematic skipping uses the game's controls; automatic
save loading and universal cinematic skipping are not implemented.

The module stays loaded until process exit; restart before replacing it. With
Meta XR Operator, shutdown can take roughly 20 seconds. Cleanup allows up to 30
seconds, but later sessions have stalled at instance destruction. Shutdown
reliability and all cinematic/loading transitions remain open acceptance items.

## Check the OpenXR runtime

From an extracted release, with other VR applications closed:

```powershell
.\mgs5vr_probe.exe
.\mgs5vr_probe.exe --session-seconds 10
```

The second command creates a bounded session without game pixels. Exit codes
distinguish missing headset (2), no progressing session (3), and session failure
(4). The probe prints structured errors; runtimes may add their own diagnostics.
In a source build the executable is under `build/Release/`.

## Build from source

Requires Windows x64, Visual Studio 2022 C++ build tools and Windows SDK, Git,
CMake 3.24+, and Python 3. Run from the repository:

```powershell
.\tools\build.ps1
```

Dependencies are pinned: Khronos OpenXR SDK 1.1.49 (`977f6675...`) and MinHook
1.3.4 (`c3fcafdc...`). The OpenXR loader is statically linked. The Visual C++
runtime is required. Outputs are in `build/Release/`. CI runs four suites that do
not need a GPU; the fifth suite exercises two hardware D3D11 devices locally.
CI does not run the game or certify headset playability.

Stage a player package without dependency SDK headers and libraries:

```powershell
cmake --install build --config Release --component MGS5VR --prefix dist/MGS5VR-development
```

See [contributing](../CONTRIBUTING.md), [architecture](ARCHITECTURE.md), and the
[acceptance matrix](ACCEPTANCE.md) for implementation and evidence requirements.

## Simulator testing

`tools/launch-simulator.ps1` takes `-GameDir`, `-RuntimeManifest`, optional
`-OperatorDir`, and `-Headless` for Elliott's headless simulator. The runtime and
Operator must already be installed. Headless runs default to a normal 1280x720
window; other launches preserve the selected resolution. Supply both
`-RenderWidth` and `-RenderHeight` for another supported mode and check the actual
rendered dimensions in the log.

The launcher sets runtime variables only for the launched process and preserves
the system OpenXR registry. Use semantic OpenXR/controller inputs for test
automation, not OS keyboard/mouse injection or window activation. Simulator
validation is separate from physical headset acceptance.
