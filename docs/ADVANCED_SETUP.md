# Advanced setup and development

![MGS5VR field manual — original Snake artwork](images/field-header.svg)

For normal play, start with the [README quick start](../README.md). This guide
covers individual options, diagnostics and building from source.

## Quick setup help

**MGS5VR-Launcher.exe** provides the native installer/update interface, controls
editor and field guide. Its update path keeps custom settings and a recoverable
backup. [Launcher and recovery details](LAUNCHER.md). Earlier ZIPs without the
launcher retain the command-file workflow below.

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
- **Only a large screen:** use the game's Action Type controls and load
  Continue → Resume Game. Current TPP builds enter tracked VR automatically;
  the public `system.toggle_vr` default is Menu + B held 550 ms. Personal bindings
  may differ. Check that PC VR is connected and its software is the active OpenXR
  runtime.
- **Menu missing / viewpoint inside the hospital bed:** this is a reported
  startup report, not a requirement to finish the prologue first. Title-camera
  anchoring has a source repair; reconcile historical hospital reports against
  the exact build before calling them current failures.
  See [current field status](FIELD_FIXES.md) and the
  [historical rendering report](RENDER_FIX_NOTES.md). Please include your build
  version, headset/runtime and whether the problem is the title, pause or an
  in-prologue prompt when reporting it.

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

Desktop preview stays **1280 x 720**. Native stereo rendering uses the active
OpenXR runtime's per-eye recommendation and correct FOVs, independently of the
mirror. Use the launcher's **Detect XR -> Headset recommendation** or explicit
custom sizing with the game closed; do not change Windows desktop resolution
or judge headset sharpness from the preview. See [launcher resolution](LAUNCHER.md#resolution-without-changing-your-desktop).

This checkout's physical headset uses wired Meta Quest Link and Meta OpenXR.
`Play.cmd` restores published `play/`; `Play-Dev.cmd` installs checked `build/dev/`.
Both call the shared launcher with `-MetaLink`. Preserve the saved per-game
`HeadsetRuntime` and the user's Windows Meta runtime. Do not select SteamVR.
For an extracted public release, use its launcher or `Launch-Headset.cmd` with
the user's selected runtime. A simulator is not needed for physical play.

Earlier 1920x1080/2560x1440 and simulator dimensions in capture reports identify
historical tests, not today's required preview or a universal headset preset.
Measure runtime frame pacing at the actual eye dimensions; keep DOF and motion
blur disabled. [Performance and evidence limits](PERFORMANCE.md).

## Large screen, menus and transitions

Public defaults: tap Menu for iDroid, hold Menu for Pause (native Help while in
iDroid). The handset is in the **left bionic hand**; hold left grip to focus its
menu controls. Ordinary status stays on the left forearm. Pause and an explicitly
selected off-wrist menu use a spatial stereo panel. Ordinary and tutorial stow
must preserve the native lifecycle and immediate reopening.

Left grip + Menu recenters without changing facing. Menu + B held 550 ms changes
VR/screen presentation. Menu + left-stick click held 650 ms opens the headset
controls/settings editor. The installed personal config is authoritative; use
[CONTROLS.md](CONTROLS.md) rather than historical chords. Release held inputs
and center sticks when ownership changes.

Title/loading/cinematic presentation follows native scene ownership. Universal
cinematic skipping and complete transition acceptance are not implied by basic
menu support. Close the game before replacing its loaded DLL. Bounded local
sessions use the workspace launcher/cleanup below; do not leave Pause, test-opened
menus or a runner behind, and never restart Steam for cleanup.

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

Requires Windows x64, Visual Studio 2022 C++ tools, Windows SDK, Git, CMake 3.24+
and Python 3. From this checkout on `main`:

```powershell
.\tools\build.ps1
```

This builds and runs the required checks, stages checked `build/dev/` and leaves
published `play/` and the installed game unchanged. `-Deploy` explicitly installs
development. `Play-Dev.cmd` or `workspace.py launch-sim` also selects development;
`Play.cmd` verifies/restores the published slot. Compiler intermediates are not
an additional playable build. Both retained slot manifests must remain intact.

Dependencies are pinned in CMakeLists.txt. GPU-dependent checks need local
hardware; CI does not launch the game or certify a headset. Do not keep dated
local packages or use the old `dist/MGS5VR-development` staging workflow.
For an authorized release, use `tools/package-release.py` against clean committed
source matching checked development, then publish/verify before pinning the
exact candidate. See [RELEASE_PLAN.md](RELEASE_PLAN.md),
[contributing](../CONTRIBUTING.md) and [acceptance](ACCEPTANCE.md).

## Simulator testing

Use `python tools/workspace.py launch-sim`, then bounded `workspace.py bot`
cases. It deploys development by default; `--slot play` selects the published
comparison without changing the pin. Default bot action is observation; Pause
requires an explicit case. Retain reviewed runs with `workspace.py pin` and
close exact-owned sessions with `workspace.py stop-sim`.

Simulator leases restore the per-game runtime and preserve the Windows Meta
selection. Keep Steam running. No desktop keyboard/mouse, focus or window-control
automation. Both-eye simulator review is separate from physical Quest acceptance.
See [runner documentation](../tools/gameplay_bot/README.md).
