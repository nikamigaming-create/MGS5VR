# MGS5VR Field Terminal

![Original MGS5VR field-manual artwork](images/field-header.svg)

A small **native C++ / Win32 launcher** using Windows GDI+ and the original
field-card artwork. No Electron, WebView, bundled browser, telemetry, accounts,
or automatic game/headset launch. The launcher itself has a static C++ runtime.

## Use it

Extract the **complete release folder**, then open **MGS5VR-Launcher.exe**.
Choose `mgsvtpp.exe` once with **Browse**; the path is remembered for your Windows
user. **Install VR** invokes the existing version-checked TPP installer.
**Launch in Steam** starts the normal Steam game; it does not change your OpenXR
runtime, graphics settings, Steam configuration or multiplayer settings.

**Edit Controls** opens the existing validating editor with the installed file
already selected. **Field Guide** opens the illustrated full control chart.
The original command files remain available as fallbacks.

## Recoverable updates

**Update / Keep My Settings** validates the old installation and the custom
control file before changing mod files. It moves the previous mod's five top-level
files into a dated `mgs5vr-launcher-backups` folder, installs the new version,
then restores your controls and VR settings. If installation fails, it restores
the previous files. A file changed by something else during recovery is preserved
for manual review, with the backup path in the log.

Modified/unrecognized binaries, linked update paths, unsupported game versions,
and a running game are refused. No game executable, archive or save is modified.

**Remove Mod** moves the active mod files into the same recoverable backup area.
Imported binocular cache and diagnostic logs remain. It does not delete other
mods. The transmission log shows actual installer output and recovery locations.

Ground Zeroes remains clearly labeled as an incomplete native stereo experiment.
Existing GZ installations can launch; new tracked first-person installation is
disabled until its independent adapters are ready.

## Build

```powershell
cmake --build build --config Release --target mgs5vr_launcher
.\build\Release\MGS5VR-Launcher.exe --self-test
```

`--render-preview <absolute-output.png>` renders the native layout without
starting the interactive launcher, changing settings, installing anything, or
opening the game. It is a layout preview, not a game/headset test.

The launcher embeds the unmodified original artwork. Current binding text lives
in code-native layouts; the old bitmap's historical controls are not copied back
as current instructions. [Artwork provenance](images/ARTWORK.md)
