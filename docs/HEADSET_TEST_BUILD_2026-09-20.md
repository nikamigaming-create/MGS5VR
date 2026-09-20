# September 20 headset test build

This candidate is ready for human headset testing. It is not an all-weapons,
all-missions, or physical-headset pass. The simulator gameplay sweep is stopped.

## Start testing

1. Connect PC VR and leave Steam signed in.
2. Extract the complete package. Open `MGS5VR-Launcher.exe`, select TPP, and
   choose **Update / Keep My Settings** for an existing MGS5VR installation.
3. Choose **Launch**. `Launch-Headset.cmd` also launches the saved TPP selection.
   The launcher uses the physical OpenXR runtime directly for the game, even
   when an already-running Steam client inherited the simulator environment.
   It does not restart Steam or change the global runtime registration.

When the globally selected runtime is Meta XR Simulator, the headset launcher
uses the recorded previous physical runtime, if available. An explicit choice
is also supported:

```powershell
./tools/launch-headset.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\MGS_TPP' -RuntimeManifest 'C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json'
```

Use your installed runtime's actual path. `-CheckOnly` checks the launch
configuration without opening the game. A fresh September 20 launch reached
the Oculus runtime on Meta Quest 3, a focused XR session, and both Touch
controllers without restarting Steam. Gameplay acceptance belongs to the
headset tester.

## Included changes to test

- The iDroid projection follows the cupped right palm and is centered on the
  handset. The always-on weapon status remains attached to the left forearm.
- Both binocular eyes receive the optic image. The field recordings include
  zoom, waypoint placement, native guard interrogation, and the revealed map.
- Brief gaps in player publication no longer immediately discard VR. Loading
  can retain the stereo surroundings, and its nearer panel distance is reset
  when ordinary menus return.
- In the enabled interactive title cabin, late helicopter NPC-stop state no
  longer freezes D-Dog. The cassette rack retains its initial cabin anchor.
  Petting, reaching Continue, and returning to field gameplay were recorded in
  the simulator. Existing cabin preferences are preserved during update.
- Headset launch removes inherited simulator control input from this game
  process while preserving unrelated enabled layers and the caller's settings.

## Known unfinished behavior

| Area | Current limitation |
| --- | --- |
| Equipment categories | Holding left trigger alone does not yet show the requested four native category icons. Select a direction to enter the native category menu. |
| Mounted weapons | Native right-stick aiming is routed; physical hand aiming for mounted guns is unfinished. Sustained driving and the complete turret/mortar/tank set are not established. |
| Guns and support items | The indexed inventory is not per-item gameplay verification. Unsupported test-loadout changes were discarded. |
| Cabin actors | Two usable native rats and Quiet seated in the cabin are unfinished. |
| Mod compatibility | Infinite Heaven/IHHook's existing dinput8.dll conflict is unresolved; the installer preserves that loader. |
| Ground Zeroes | Complete tracked first-person arms, weapons, and wrist UI are unfinished. |

The first headset feedback should cover forearm and palm attachment while
moving, binocular visibility/zoom in both eyes, physical sight alignment,
controller tracking loss, and scene transitions that previously went black.
These remain physical checks even where automated regressions pass.

Temporary test damage protection and simulated held controls were cleared.
The simulator was stopped. No campaign save was loaded from another profile
for this headset handoff.

Validation: all 15 CTest suites passed; the final launcher edits passed the two
affected suites again. The assembled package passed installer/update/rollback
tests on Windows PowerShell 5.1. These checks do not certify headset gameplay.
