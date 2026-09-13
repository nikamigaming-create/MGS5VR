# MGS5VR

Play **Metal Gear Solid V: The Phantom Pain in VR**, with tracked hands and a HUD on your left wrist.

**[Download MGS5VR for Windows](https://github.com/nikamigaming-create/MGS5VR/releases/download/experimental-2026-09-13.3/MGS5VR-experimental-2026-09-13.3.zip)**

You need your own PC copy of **TPP 1.0.15.4** and a PC-connected VR headset. Tested on **Quest 3 + Touch controllers**; other headsets are unverified. This is an experimental mod.

This build also recognizes **Ground Zeroes 1.0.0.5**. Its independently
mapped native stereo renderer now runs in SIM, but player head anchoring, tracked
arms, wrist UI and weapon aiming are **not connected**. GZ is not a finished VR
port. `tools/setup.ps1 -TheatrePreview` selects the explicit large-screen preview;
normal VR setup will not substitute a flat screen or apply TPP hooks. The scene
experiment requires the explicit installer experiment switches and a configured
VR toggle. See [both-game SIM coverage](docs/DUAL_GAME_SIM_COVERAGE.md).

## Install

1. Close MGSV. Download the ZIP above and **extract it all**.
2. Open the extracted **MGS5VR** folder and double-click **Install.cmd**.
3. Select **mgsvtpp.exe** in your game folder. Done.

Setup imports the binocular model/material from your own TPP archives. No game
assets are bundled; no archive or save is modified. Import needs only the .NET
Framework included with supported Windows installations, not Python or a download.

Find the game folder in Steam: right-click MGSV → **Manage → Browse local files**.
Updating? Run **Uninstall.cmd** first. Other mods using `dinput8.dll` must be removed through their own uninstall process.

## Play

1. Connect your headset to PC VR and make its software the **active OpenXR runtime**.
2. Launch MGSV through Steam. Use **Action Type** controls and load **Continue → Resume Game**.
3. Tracked VR enters automatically. Optional presentation/recenter bindings are in **mgs5vr-controls.ini**.

No simulator or compiling needed. Install once; launch normally afterward.

## Controls

![Current control modes](docs/images/control-modes.svg)

Current builds install **mgs5vr-controls.ini** beside the game executable. Edit it,
save, then restart MGSV. No rebuild needed. [Annotated default config](config/mgs5vr-controls.ini)
supports per-mode buttons, taps, holds, release actions and combinations.

**[Full controls, grenades and night vision →](docs/CONTROLS.md)**

[All control modes: menus, Commands, zoom, melee and vehicles](docs/images/control-modes.svg)

## Help

- **Sharper picture:** start at 1920×1080 Windowed, Post Processing **High** for AA, Depth of Field **Disable**, Motion Blur **Off**. [More picture settings](docs/PERFORMANCE.md).
- **Install or launch trouble:** [setup help](docs/ADVANCED_SETUP.md#quick-setup-help). You may need the [Microsoft Visual C++ x64 runtime](https://aka.ms/vc14/vc_redist.x64.exe).
- **Remove the mod:** close MGSV, double-click **Uninstall.cmd**, and select the same `mgsvtpp.exe`.

The optics hotfix restores binocular lens rendering and connects TPP's round weapon scopes to their native sight sockets. BAMBETOV SV's fixed 4× scope has been exercised in SIM; other sights and weapon variants are still being checked. [Scope controls and coverage](docs/WEAPON_SCOPES.md).

Complete weapon/gadget coverage, some reconnaissance/powered-arm interactions, physical body grabs and GZ tracked first-person remain unfinished. [Current status](docs/STATUS.md) · [Known interaction gaps](docs/SYSTEMS_ACCESS.md)

Community project, not affiliated with Konami. [MIT license](LICENSE) · [Third-party notices](docs/THIRD_PARTY_NOTICES.md) · [Build from source](docs/ADVANCED_SETUP.md#build-from-source)
