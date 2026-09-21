# MGS5VR

![MGS5VR — original Snake field-manual artwork](docs/images/field-header.svg)

**The Phantom Pain in native stereo VR.** Tracked hands and weapons, physical
binoculars and scopes, and equipment on your left wrist.

**[Download the public build](https://github.com/nikamigaming-create/MGS5VR/releases)**
· [Controls](docs/CONTROLS.md) · [Setup help](docs/ADVANCED_SETUP.md) · [Current release and known issues](docs/RELEASE_2026-09-20.md)
· [Live native actions](docs/NATIVE_ACTIONS.md)

Experimental Windows mod. Requires your own **TPP 1.0.15.4** and a PC-connected
OpenXR headset. Physical feedback currently comes from **Quest 3 + Touch**;
other headsets are unverified. This is not a standalone Quest app.

## Get in

1. Close the game. Download the ZIP and **extract the complete MGS5VR folder**.
2. Open **MGS5VR-Launcher.exe**, choose **mgsvtpp.exe**, and select **Install VR**.
   Older packages without the launcher use **Install.cmd**.
3. Connect PC VR, select your headset software as the active OpenXR runtime,
   and choose **Launch**. Steam must be running and signed in. Use the game's
   **Action Type** controls. `Launch-Headset.cmd` also launches the saved selection.
4. Load **Continue → Resume Game**. Tracked VR enters automatically.

The native C++ launcher has game selection, recoverable updates, a controls editor,
and the illustrated field guide. No Electron, browser runtime, or launcher account.
[Launcher details](docs/LAUNCHER.md)

Setup imports binocular assets from your own game. Game archives and saves stay
untouched; no game assets are distributed. For a Steam install, find the folder
under **Manage → Browse local files**. Another mod's `dinput8.dll` is never
silently overwritten.

## Your field controls

![Illustrated current default Touch controls](docs/images/controls-quick.svg)

These are the defaults. Your **mgs5vr-controls.ini beside the game executable**
is authoritative. Choose **Edit Controls** in the launcher, or **Edit-Controls.cmd**
in older packages. The editor checks conflicts before saving and keeps a backup.

Save, release all buttons/grips/triggers, and center both sticks for two seconds:
valid edits apply **live**, without restarting. Buttons, taps, holds, chords,
turning, HUD preferences, and physical fit have annotated settings.

[Complete illustrated control modes](docs/images/control-modes.svg)
· [Accessible full guide](docs/CONTROLS.md)
· [Annotated configuration](config/mgs5vr-controls.ini)

## Hands, optics, equipment

![Original Snake support-hand and grenade field illustrations](docs/images/field-gear.svg)

Hold **left grip + Y** to equip binoculars; tap **B** to stow. Raise the ocular
to your eye. **Left-stick click** changes binocular magnification; **right-stick
up** runs with binoculars and changes a compatible weapon scope's zoom on foot.
**Right-stick click** dives.

**Left trigger** opens the wrist selector and waits for your category choice.
**Press X** opens Commands. [Weapon scopes](docs/WEAPON_SCOPES.md)
· [Buddies and other interactions](docs/SYSTEMS_ACCESS.md)

Recon cues default to `hud_mode = binoculars_only`; merely carrying binoculars
or using a firearm scope does not enable them. `full` opts into normal-view
world cues; `off` hides them. Native model-glow leakage and caption restoration
remain under repair—this setting is not a claim that every native HUD effect
is already handled.

## Tune it / get help

- **Sharper picture:** update the mod, then use **Detect XR → Headset recommendation**
  or **Custom → Apply Size** in the launcher with TPP closed. Native VR rendering
  is independent of the small PC preview; no desktop-resolution or DSR changes.
  Depth of Field and Motion Blur stay off. [Resolution setup](docs/LAUNCHER.md#resolution-without-changing-your-desktop)
  · [Performance](docs/PERFORMANCE.md)
- **Smooth turning:** set `[settings] turn_mode = native_smooth`.
  **Left grip + Menu** recenters without changing facing.
- **Update:** select **Update / Keep My Settings** in the launcher. The previous
  mod is backed up; custom controls and VR settings are retained.
- **Remove:** **Remove Mod** disables the active mod and retains a recoverable
  backup. The older **Uninstall.cmd** uses the original recorded-file removal.
- **Launch or menu trouble:** [setup help](docs/ADVANCED_SETUP.md#quick-setup-help).
  The reported hospital-bed/menu case is being fixed; do not assume finishing
  the prologue is required or that it solves the issue.

## What is still experimental?

TPP is the priority. Missing captions and other HUD elements, blue enemy-outline
tails, lens-flare alignment, some interactions, and complete weapon/gadget coverage
are still being worked through. [Tester feedback](docs/QUEST3_TESTER_FEEDBACK.md)
· [Current status](docs/STATUS.md) · [Latest rendering work](docs/RENDER_FIX_NOTES.md)

**Ground Zeroes 1.0.0.5 is not a finished first-person VR port.** Its independent
native stereo experiment runs in SIM; tracked player anchoring, arms, wrist HUD,
and weapon aiming are not connected. The launcher can open an existing GZ
installation, but does not pretend to install a complete GZ first-person adapter.
[Both-game coverage](docs/DUAL_GAME_SIM_COVERAGE.md)

---

Community project; not affiliated with Konami. [MIT license](LICENSE)
· [Third-party notices](docs/THIRD_PARTY_NOTICES.md)
· [Original artwork](docs/images/ARTWORK.md)
· [Build from source](docs/ADVANCED_SETUP.md#build-from-source)
