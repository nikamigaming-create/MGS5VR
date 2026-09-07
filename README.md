# MGS5VR — native stereo experiment

[![Windows build](https://github.com/nikamigaming-create/MGS5VR/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/nikamigaming-create/MGS5VR/actions/workflows/windows.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%20x64-blue)
![API: D3D11 + OpenXR](https://img.shields.io/badge/API-D3D11%20%2B%20OpenXR-blue)
[![Status: experimental](https://img.shields.io/badge/status-experimental-orange)](docs/STATUS.md)

The mod's implementation, native build adapters, tests and build tooling are open source under the [MIT license](LICENSE). Dependencies are pinned open-source projects with [retained notices](docs/THIRD_PARTY_NOTICES.md). Building requires no private mod code. Running requires your own installation of the game and an OpenXR runtime; neither is redistributed here. This is an independent community project.

This repository is **not a complete VR conversion of The Phantom Pain**. It is a native Windows x64/D3D11/OpenXR development experiment. An opt-in native scene hook draws both eyes within one game render transaction and submits a projection layer. The controller experiment now drives native hands and the rifle, with controller-directed muzzle and projectile output observed for the AM MRS-4. Physical alignment and all-state acceptance remain incomplete. An opt-in left-forearm weapon/status HUD is now implemented. Full arm/menu interaction and universal cinematic skipping remain incomplete.

The requested gameplay view is first person: six-axis head tracking, tracked hands and equipped gear, and weapon/status UI on the left forearm. Complete native menus use a large spatial screen. See the [latest simulator review](docs/SIM_HUD_REVIEW.md) and [complete controls](docs/CONTROLS.md) for the tested slice and its limits.

The current Quest 3 run of build `7BF543BE` received positive user feedback for the basic VR experience, with low resolution and jagged edges remaining. A higher native resolution is now under physical test with the same mod binary; see the [sharpness review](docs/HEADSET_SHARPNESS_REVIEW.md). Earlier builds failed stereo and limb presentation and remain failed references. The current build crops centered renders to each eye's requested optics and places equipment cards above the left wrist. [SIM evidence](docs/SKY_CROP_REVIEW.md) includes a short recording. Complete gameplay and visual acceptance remain unfinished; this is not a polished release.

The default mode captures the game's desktop image onto a large, world-anchored OpenXR quad. With `-EnableHeadCameraExperiment`, left grip plus left stick click toggles experimental native head tracking and same-frame stereo. Both eye cameras use one tracking snapshot; alternate-eye rendering is not used. The camera now anchors to the player's head bone and excludes the player's head model and body group while retaining arm groups. Aiming stays in stereo with the weapon's actual sights; optical zoom is reserved. iDroid and pause automatically use the large screen and restore tracked VR when the same player camera resumes. OpenXR controllers feed the game's own XInput import, without Windows key/mouse injection or window activation. Complete cinematic/loading transitions remain unverified.

## Build

Requires Windows x64, Visual Studio 2022 C++ build tools and Windows SDK, Git, CMake 3.24+, and Python 3. Run from this repository:

```powershell
.\tools\build.ps1
```

Dependencies are pinned: Khronos OpenXR SDK 1.1.49 (`977f6675...`), MinHook 1.3.4 (`c3fcafdc...`). The OpenXR loader is statically linked. The Visual C++ runtime is required. Build outputs are in `build/Release/`.

The Windows CI badge reports the real build and four tests that do not require a GPU. The fifth suite exercises two hardware D3D11 devices locally. CI does not run MGSV or a headset, and the badge does not certify VR playability. Development build artifacts are experimental. See [contributing](CONTRIBUTING.md) for the evidence requirements.

## Check the selected OpenXR runtime

```powershell
.\build\Release\mgs5vr_probe.exe
.\build\Release\mgs5vr_probe.exe --session-seconds 10
```

The second command creates a bounded session without game pixels. Exit status distinguishes missing headset (2), no progressing session (3), and session failure (4). The probe also prints a structured error. Some runtimes print their own diagnostics alongside the JSON.

For a locally installed simulator, set `XR_RUNTIME_JSON` only in the launching PowerShell process to that simulator's manifest. This project does not change the system OpenXR registry selection. Simulator validation cannot certify a physical headset.

## Install the current preview

Extract the complete archive from the [experimental downloads](https://github.com/nikamigaming-create/MGS5VR/releases), including its tools, documentation, profile and licenses. Windows x64, the supported game build, an installed OpenXR runtime and the [Microsoft Visual C++ v14 x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) are required. A source checkout must be built first.

Close the game first. The installer supports only the recorded SHA256 of TPP 1.0.15.4, refuses existing proxy/config files, and records hashes for removal. It does not modify the executable, game archives, or saves.

```powershell
.\tools\install.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -EnableTheatrePreview
```

For the tracked VR test with hands and the left-wrist HUD, install with all five switches:

```powershell
.\tools\install.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -EnableTheatrePreview -EnableCameraObserver -EnableHeadCameraExperiment -EnableControllerRigExperiment -EnableWristHudExperiment
```

In loaded gameplay, hold left grip and click the left stick to toggle native VR.
Tracking stalls suspend eye submission and resume from the same head origin.
This enables tracked hands/arms and the native weapon/status display on the
**left forearm**. These options remain disabled by default.

Right grip readies the weapon; right trigger fires; left grip requests support;
right B reloads. Left trigger plus left-stick direction selects equipment.
Activation uses anatomical hand landmarks rather than an arbitrary ready pose.
Flat crosshairs and destination labels are suppressed; native 3D people cues remain.
Binocular and scope zoom are reserved, so aiming cannot switch to a mono screen.
See the [complete Touch interaction list](docs/CONTROLS.md) for one/two-handed use,
weapon selection, menus, movement and the exact test limits.

For the current VR test setup, use native Graphics Settings to set Depth of Field to Disable, Motion Blur to Off, and Post-Processing to Off. Set Camera Shake to Off in Camera Settings. These saved settings were exercised in the simulator; they do not establish that every cinematic or temporal effect is disabled.

The positively reviewed Quest 3 baseline used native 1920x1080 rendering. A 2560x1440 sharpness test is running with the same DLL; its visual improvement and sustained performance remain unconfirmed. To request that mode after closing the game, use the selected physical runtime:

```powershell
$mgsRuntime = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1').ActiveRuntime
.\tools\launch-simulator.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -RuntimeManifest $mgsRuntime -RenderWidth 2560 -RenderHeight 1440
```

Despite its name, this launcher uses the supplied runtime; a physical run omits `-Headless` and `-OperatorDir`. It backs up changed graphics settings and preserves the global runtime selection. Confirm `Game Present ... size=2560x1440` in `mgs5vr.log`, because unsupported modes can be substituted by the game. The [sharpness review](docs/HEADSET_SHARPNESS_REVIEW.md) separates the working baseline from the higher-resolution test.

The current DLL takes the `dinput8.dll` slot and cannot yet be chained with IHHook or another DirectInput proxy. It activates only in `mgsvtpp.exe`, never `mgsvmgo.exe`. The module remains loaded until process exit. Restart the game to replace it.

With Meta XR Operator loaded, closing the game can take roughly 20 seconds while the runtime and Operator server finish shutting down. The preview requests normal OpenXR session exit and allows up to 30 seconds for cleanup before native process exit. One native exit completed, but later sessions stalled at instance destruction; shutdown reliability remains unresolved.

The default Touch mapping passes A/B/X/Y, sticks, triggers, grip buttons and stick clicks to the corresponding Xbox controls; the controller experiment changes grip behavior as described above. Tap left Menu for Start/iDroid; hold it for at least 0.55 seconds for Back/Pause. Both paths have been exercised in the game through Meta XR Operator. Tracking/focus loss and stale samples release virtual inputs. Other controller profiles have incomplete bindings. The tracked-rig equipment modifier supplies D-pad category selection.

Use Continue and then Resume Game to load the existing checkpoint and its equipped gear. This path was exercised with the saved AM MRS-4. The current save uses the game's Action Type layout. The tracked-rig mapping is documented above; the large-screen mode retains ordinary gamepad mappings. Left X is a quick dive outside aim, not reload. Automatic startup directly into the save is not implemented.

Edit `mgs5vr.ini` while the game is closed: `width_cm=800`, `distance_cm=600`. Supported range is 100–3000 cm. Hold both grips and click the right stick to recenter the screen. Suggested bindings on other controllers are unverified; simple controllers lack the grip chord. Cinematic skipping uses the game's existing controls; no universal skip hook is present.

For simulator testing, `tools/launch-simulator.ps1` takes `-GameDir`, `-RuntimeManifest`, optional `-OperatorDir`, and `-Headless` for Elliott's headless simulator. Headless runs default to a normal **1280×720 window**; other launches preserve the selected game resolution. Supply both `-RenderWidth` and `-RenderHeight` to request another supported game mode. The launcher backs up changed graphics configuration and sets runtime variables only for the launched process. It preserves the system OpenXR registry. The chosen runtime and Operator must already be installed. Check the actual rendered dimensions in the log: the game may substitute an unsupported mode.

The optional `-EnableCameraObserver` installation switch enables a version-checked diagnostic setter hook. It records retail camera transforms and does not alter the camera. It is development evidence, not a 6DoF mode.

## Remove

```powershell
.\tools\uninstall.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP'
```

Modified files are preserved and require manual review. `mgs5vr.log` is retained for diagnostics.

See [implementation status](docs/STATUS.md), [architecture](docs/ARCHITECTURE.md), [acceptance matrix](docs/ACCEPTANCE.md), and the [development handoff](docs/HANDOFF.md) before testing. They distinguish implemented code, verified behavior, and outstanding work.
