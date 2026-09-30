# MGS5VR Field Terminal

![Original MGS5VR field-manual artwork](images/field-header.svg)

The launcher opens a local **3D field terminal** inside a native Windows window.
It uses Three.js, the installed Edge WebView2 runtime, and the cream/red field-kit
design. There is no account or automatic game launch. The original Win32/GDI+
maintenance window remains available with `MGS5VR-Launcher.exe --classic`.

## Interactive field kit

**Quick controls tour** plays a short 3D walkthrough of the saved bindings,
including alternatives and whether each chord uses one hand or both hands.
It reloads the installation's mapping on replay. All ten capacitive contacts
have their own physical surfaces: four face buttons, two sticks, two triggers
and two thumb rests. Amber means light touch and does not depress the control;
red means press or squeeze. In Controller bindings and Game modes, choose
**Light touch** before clicking the 3D surface to inspect or assign its touch.
An optional right-trigger contact can ready the weapon; a thumb-rest contact
can hold open the wrist equipment picker. Existing mappings remain authoritative.

The field terminal accepts --page tour, --page modes and --page controls for
direct entry. The mapping video exporter writes one fixed MP4 and poster in
artifacts/dev, using read-only personal settings and no desktop automation.

- **Deploy:** select your installed game, install/update, choose Quest Link / Air
  Link or the active OpenXR runtime, then launch. Quest selection applies to the
  child game process and does not change the global runtime registry.
- **Training tapes:** select a cassette to play unobstructed source gameplay
  alongside the 3D controllers. Automatic views turn toward the face, grip or
  trigger used by the current mapping. Drag or scroll to inspect; **Auto view**
  resumes framing. **Zoom to control** shows a close view in the controller pane.
- **Game modes:** all eight contexts from the native bindings export are listed.
  Click an action or a physical control on the model to inspect matching actions,
  gesture thresholds and alternatives. Stick axes appear in their applicable mode.
- **Controller lab:** edit buttons, chords, alternatives, tap/hold timing and
  stick assignments. The native validator rejects conflicts. Saving uses an
  expected revision, atomic replacement and a backup; an outside edit is not
  silently overwritten. Release all controls and center sticks for two seconds
  to apply a saved configuration in game.
- **VR settings:** all 46 exposed tuning values have bounded controls, exact
  numeric fields and per-setting reset. Runtime flags and owned-asset paths are
  separate and require a restart. The display pane retains the existing native
  render-size setup.
- **Field notes:** search recovered features, community reports and a source-bound
  inventory of 7,435 runtime numeric literals. The inventory is read-only;
  addresses and contract constants are not presented as user-adjustable settings.

Lessons store action identifiers and timing, not physical button annotations.
Reopening, playing or replaying reloads the saved mapping. Unsaved binding edits
are labelled previews and do not change lesson playback. Recording details keep
the original capture build/hash; a new instructional mapping is not a claim that
the source take was recorded with that mapping. Videos have no controller inset
or added in-frame instructional overlay.

The current local candidate includes ten recorded action lessons, 26 recovered
feature families and 55 tracked reports. It is not the completed historical tour
or a certification that all reports pass. Controller geometry is Quest Touch
Plus; other device shapes need their own reviewed models.

The following maintenance details also describe the retained classic window.

## Use it

Extract the **complete release folder**, then open **MGS5VR-Launcher.exe**.
Choose `mgsvtpp.exe` once with **Browse**; the path is remembered for your Windows
user. **Install VR** invokes the existing version-checked TPP installer.
**Launch** starts the selected game with its physical OpenXR runtime and Steam
app identity. Steam must already be running and signed in. `Launch-Headset.cmd`
is the equivalent command-file entry point. With **Keep current settings**,
graphics are retained. A selected resolution preset is applied before launch;
the OpenXR runtime, Steam configuration and multiplayer settings are unchanged.

## Resolution without changing your desktop

Update the mod with this package first. Connect Virtual Desktop / PC VR and
select its quality setting, then **Detect XR** reads the active runtime's
recommended resolution per eye. Choose **Headset recommendation** and a scale,
or enter a **Custom** width and height, then **Apply Size** with TPP closed.
The size is per eye, not the combined SBS width. Dimensions must be even:
width 640–8192, height 360–8192. These are configuration bounds, not a guarantee
of playable performance at the maximum. Reduce scale if necessary.

The launcher backs up the selected Steam account's `TPP_GRAPHICS_CONFIG`, uses
TPP's native `FlexibleWindowed` path, and disables depth of field and motion blur.
Other quality settings are preserved. **Account Config** selects the correct
account file if more than one is found. Saves are not edited.

The game creates its native render targets at that size, then the mod reduces
the desktop preview to **1280 × 720** without reducing the VR render targets.
The window can briefly be larger while those targets initialize. No Windows
resolution, DSR, or exclusive-fullscreen mode switch is requested. A hidden
preview is not implemented. **ACTUAL** shows the live native buffer and real
PC-window dimensions; it is not just a display of the requested preset.

The game-local `mgs5vr-display.ini` retains the chosen render/preview sizes.
Restart to apply a different size. **Keep current settings** retains that choice;
it does not undo it. Ground Zeroes keeps its existing graphics settings.

Meta XR Simulator keeps its output swapchains at the runtime's recommended
per-eye size. A larger native render target still supplies supersampled scene
detail; each eye is filtered separately in linear light before submission.
Physical headset runtimes keep the direct native-resolution path. Both use
the runtime's eye FOVs independently of the 720p desktop preview.

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

### Local simulator override

For testing through an already running Steam client, an explicit
`mgs5vr-runtime.ini` beside the game can select a runtime for that game process:

```ini
[runtime]
enabled=1
manifest=C:\Program Files\MetaXRSimulator\v207.0\meta_openxr_simulator.json
headless=1
```

The manifest must be an existing absolute path. Optional `api_layer_path` and
`api_layers` select an existing API-layer folder and named layers together;
`data_dir` accepts an existing absolute simulator data directory. An explicit
`XR_RUNTIME_JSON` inherited by the game takes priority. The override is not
included in the package and does not change the machine's OpenXR registry.
Remove the override or set `enabled=0` when finishing simulator testing.

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
