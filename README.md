# MGS5VR

![MGS5VR — original Snake field-manual artwork](docs/images/field-header.svg)

**The Phantom Pain in native stereo VR.** Tracked hands and weapons, physical
binoculars and scopes, and equipment on your left wrist.

**Development status — 24 October 2026:** main already includes the user-report
repairs for body/arm fitting, native leg lengths, tracking and rifle support,
contextual camera/cover/transit, left-hand iDroid, Rocket Arm and cabin interaction.
Remaining code limits and verification gaps are separated in
[Field fixes](docs/FIELD_FIXES.md); an open acceptance item does not mean missing code.

Local work keeps two builds: published `play/` and checked `build/dev/`.
`tools/build.ps1` refreshes development only; `Play.cmd` restores the published
pin and `Play-Dev.cmd` selects development. Both use Meta Quest Link locally.
Only successful verified publication advances the pin. Full-game and physical
headset acceptance remain open.
[Current handoff](docs/CURRENT.md) · [Exact release plan](docs/RELEASE_PLAN.md)
· [Documentation map](docs/README.md).

**[Download the public build](https://github.com/nikamigaming-create/MGS5VR/releases)**
· [Controls](docs/CONTROLS.md) · [Setup help](docs/ADVANCED_SETUP.md) · [Published baseline notes](docs/RELEASE_2026-10-19.1.md)
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

For free roam from the helicopter, open **iDroid → Missions → Mission List**
and change its category to **Free Roam** (default: hold left grip and flick
right stick right). Choose a destination, landing zone and drop time.

The field terminal has interactive 3D training cassettes, clean gameplay beside
rotating controllers, a guide to all eight control modes, and a binding editor.
Lessons resolve your current saved mapping; changing a binding does not change
the recording. VR tuning uses sliders, switches and dropdowns. The embedded UI
uses the installed Edge WebView2 runtime and requires no launcher account.
`MGS5VR-Launcher.exe --classic` retains the original Win32 maintenance tools.
[Launcher details](docs/LAUNCHER.md)

Setup imports binocular assets from your own game. Game archives and saves stay
untouched; no game assets are distributed. For a Steam install, find the folder
under **Manage → Browse local files**. Another mod's `dinput8.dll` is never
silently overwritten.

## Your field controls

![Illustrated current default Touch controls](docs/images/controls-quick.svg)

These are the defaults. Your **mgs5vr-controls.ini beside the game executable**
is authoritative. Choose **Controller lab** in the launcher, or **Edit-Controls.cmd**
in older packages. The editor checks conflicts before saving and keeps a backup.

Save, release all buttons/grips/triggers, and center both sticks for two seconds:
valid edits apply **live**, without restarting. Buttons, taps, holds, chords,
turning, HUD preferences, and physical fit have annotated settings.
The **Character eyepatch** setting shows or hides Snake’s native eyepatch on
his standard face models. It defaults on and does not mask either VR eye.

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
world cues; `off` hides them. Historical model-glow and caption reports retain
their original scope in the acceptance records; this setting does not certify
every native HUD effect.

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
  For a hospital/menu report, identify the exact build and native scene; do
  not assume finishing the prologue is required or that it solves the issue.

## What is still experimental?

TPP is the priority. Forearm-only rendering supports 20 exact native outfit
layouts; broader visual acceptance, additional mounted types, exact rock/corridor
reports and complete mission/weapon/gadget coverage remain open.
[Current candidate and known failures](docs/CURRENT.md) track development separately
from published releases. Historical HUD/rendering reports retain their tested-build scope.
[Tester feedback](docs/QUEST3_TESTER_FEEDBACK.md)
· [Current status](docs/STATUS.md) · [Historical rendering notes](docs/RENDER_FIX_NOTES.md)
· [Community report history](docs/COMMUNITY_RECOVERY_PLAN.md)

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
