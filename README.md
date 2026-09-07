# MGS5VR

Play **Metal Gear Solid V: The Phantom Pain** in first-person VR, with tracked
hands and a weapon HUD on your **left forearm**.

**Experimental:** the current Quest 3 build received positive basic gameplay
feedback. Sharpness, arm polish and complete interaction testing remain unfinished.
See [what has been tested](docs/STATUS.md).

**Setup in one line:** connect your headset → download and extract → run the
installer once → launch MGSV → load your save and enter VR.

## Before you start

- **Windows x64 and your own PC copy of The Phantom Pain 1.0.15.4.** The installer checks the supported executable.
- **A PC-connected OpenXR headset.** Quest 3 with Touch controllers is the tested setup. Start your PC VR connection and select your headset software as the active OpenXR runtime. Other headsets and controller layouts are unverified.
- The [Microsoft Visual C++ v14 x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist), if it is not already installed.
- Close MGSV. Another mod using `dinput8.dll`, including IHHook, conflicts with this build; the installer preserves existing files.

You do not need Visual Studio, a simulator, or to compile anything.

## Install once

1. Download **MGS5VR-experimental-2026-09-07.1.zip** from the [experimental release](https://github.com/nikamigaming-create/MGS5VR/releases/tag/experimental-2026-09-07.1). Extract the whole ZIP, keeping its folders together. Use the ZIP named MGS5VR, rather than GitHub's Source code download.
2. Find your game's folder using Steam's **Browse local files** option. It must contain `mgsvtpp.exe`.
3. Open the extracted **MGS5VR** folder containing this README, `dinput8.dll` and `tools`. In File Explorer's address bar, type `powershell` and press Enter. Run this command, replacing the quoted path with your game's folder:

```powershell
.\tools\install.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -EnableVR
```

`-EnableVR` enables tracked VR, hands, weapons and the left-wrist HUD together.
The installer records its files for removal and leaves game data and saves intact.
If Windows blocks the script, see [PowerShell setup](docs/ADVANCED_SETUP.md#powershell-blocks-the-installer).

## Play

1. Connect your headset and launch MGSV normally through Steam.
2. Use the game's **Action Type** controller layout. Choose **Continue → Resume Game**.
3. Once gameplay has loaded, **hold left grip and click the left stick** to enter VR. Repeat that combination to return to the large game screen.

Title and loading screens use the large screen; enter tracked VR after loading
your save. Installation is only needed once.

## Essential Touch controls

| Action | Control |
| --- | --- |
| Move / turn | Left stick / right stick |
| Ready / fire | Hold right grip, then right trigger |
| Support the weapon | Bring the left hand near it, or hold left grip |
| Reload | Right B |
| Read ammo and status | Raise your left forearm |
| Select a primary weapon | Release right grip; hold left trigger + left stick up, browse with the right stick, then release |
| Interact / pick up | Left Y when the game offers the action |
| iDroid / pause | Tap left Menu / hold left Menu |

Weapon selection cards open above the left wrist. Full iDroid and pause menus
use the large screen. [All controls, equipment categories and test limits →](docs/CONTROLS.md)

## Picture settings

The current test setup uses **Windowed**, **Depth of Field: Disable**,
**Motion Blur: Off**, **Post Processing: Off**, and **Camera Shake: Off**.
Post Processing Off also removes the game's anti-aliasing, so jagged edges remain
a known limitation; see the [sharpness review](docs/HEADSET_SHARPNESS_REVIEW.md).

The positively reviewed baseline used **1920×1080**. **2560×1440** is the current
sharpness test with the same mod binary; its visual improvement and sustained
performance still need confirmation. [Resolution setup →](docs/ADVANCED_SETUP.md#runtime-and-resolution)

## Current limits

- Arm/cuff polish, sharpness and some blue marker artifacts remain under review.
- Pause and iDroid use a large screen; keeping the stereo world around every menu remains unfinished.
- Optical scope/binocular zoom is unavailable. Vehicles, every weapon and every interaction are not fully tested.
- Metal Gear Online is unsupported.

This is a playable experiment, not a finished conversion. The [acceptance matrix](docs/ACCEPTANCE.md) records the remaining work.

## Update or remove

Close the game and run this from the extracted mod folder:

```powershell
.\tools\uninstall.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP'
```

Removal deletes only the recorded, unchanged mod files and retains the diagnostic
log. Modified files are preserved for review. To update, remove the old version
first, then run the new version's installer.

## More information

[Advanced setup and troubleshooting](docs/ADVANCED_SETUP.md) ·
[Development and building](docs/ADVANCED_SETUP.md#build-from-source) ·
[Contributing](CONTRIBUTING.md)

Independent community project. Mod source is [MIT licensed](LICENSE);
[dependency notices](docs/THIRD_PARTY_NOTICES.md) are included. Downloads contain
no game data.

[![Windows build](https://github.com/nikamigaming-create/MGS5VR/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/nikamigaming-create/MGS5VR/actions/workflows/windows.yml)
