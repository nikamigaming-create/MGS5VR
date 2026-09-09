# Advanced setup and development

For normal play, start with the [README quick start](../README.md). This guide
covers individual options, diagnostics and building from source.

## Quick setup help

`Install.cmd` opens a file picker for `mgsvtpp.exe` and runs the existing installer
with `-EnableVR`. `Uninstall.cmd` opens the same picker and removes the recorded,
unchanged mod files. Both keep the result window open so errors can be read.
They use Windows PowerShell with an execution-policy override for that process
only; they do not change your system policy. Cancel the picker to leave everything
unchanged. No administrator access is requested.

- **Cannot find the game:** in Steam, right-click MGSV → Manage → Browse local files.
  Copy that folder path into the installer's file picker, then select `mgsvtpp.exe`.
- **Existing mod files:** close the game and use `Uninstall.cmd` from the previous
  MGS5VR package first. Another mod's `dinput8.dll` needs that mod's own removal
  process. Modified files are preserved and identified in the error message.
- **Script blocked:** extract the entire ZIP, keeping `tools` beside `Install.cmd`.
  See [PowerShell setup](#powershell-blocks-the-installer) if Windows or an
  organization policy blocks scripts.
- **Missing DLL/runtime:** install the
  [Microsoft Visual C++ x64 runtime](https://aka.ms/vc14/vc_redist.x64.exe).
- **Only a large screen:** load the save, use the game's Action Type controller
  layout, then hold left grip and click the left stick. The title screen uses the
  large screen. Make sure your headset is connected to PC VR with its software
  selected as the active OpenXR runtime.

For manual installation, open PowerShell in the extracted MGS5VR folder and run:

```powershell
.\tools\install.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -EnableVR
```

To remove it manually, with the game closed:

```powershell
.\tools\uninstall.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP'
```

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

The latest physical 2560x1440/native-AA session received positive feedback, and
that resolution is retained for the tester. Start at 1920x1080 on other hardware;
the higher setting is not a universal performance recommendation. Close the game
before requesting that mode:

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
records the earlier dimensional checks; [performance notes](PERFORMANCE.md)
describe the current profile and measurement limits.

## Large screen, menus and transitions

Tap left Menu for iDroid; from gameplay, hold it for at least 0.55 seconds for
Pause. Both menus float in front of the tracked stereo world. Holding Menu
inside iDroid opens its native Help instead. A confirms, B goes back, sticks
navigate, grips act as LB/RB, and triggers retain LT/RT. Close iDroid with B or
hold Menu again to unpause. Release held inputs once before moving or firing
again. The manual left-grip + left-stick-click VR toggle remains available.

Title, loading and cutscene transitions can still use the large in-headset screen.

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
runtime is required. Outputs are in `build/Release/`. CI runs five native suites
that do not need a GPU; the sixth exercises two hardware D3D11 devices locally.
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
