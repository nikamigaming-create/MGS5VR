# Live 3D fitting

Open the existing launcher in `play/`, then **VR settings → VR adjustments**.
The preview uses the current installed values plus your unsaved edits. Drag it
to rotate, scroll to zoom, or use Front, Side, Top and Back. Arrow keys rotate;
plus/minus zoom and zero resets the view. Turning the preview never changes a
setting. Each numeric setting has a slider and an exact value box.

The shared fitting view covers all 53 controls settings:

| View | What changes |
| --- | --- |
| iDroid | Right hand/device grip position and rotation; projected screen width, distance, horizontal/vertical offset and rotation. |
| Left forearm | Weapon/ammo setback, surface clearance, text scale, HUD mode and equipment popup height/width. Choose Weapon/ammo, HUD popup or Equipment picker in the preview. |
| Hands & weapons | Separate left/right hand fits, relaxed/touch finger curl, support acquisition/release radii and optional aim-shake demonstration. |
| Optics | Binocular rotation and viewing clearance, scope eye distance and marking/glow preferences. |
| Spatial menus | Pause/off-wrist panel width, distance and tilt; theatre dimensions under Runtime & assets. |
| Movement | Eye height without changing world scale, turning, physical melee and animal-touch preferences. |

Clicking or focusing a setting selects its fitting view. **Show only these
settings** filters the controls; **Show all settings** restores the full list.
**Reset this fit** creates an unsaved draft of that view's numeric fit defaults;
it preserves interaction modes, bindings and feature toggles.
Per-setting Reset, Save and Discard retain their existing behavior. Saving
validates the complete INI, keeps a backup and refuses conflicting edits.

In-game controls tuning reloads after two seconds with buttons/triggers/grips
released and sticks centred. Runtime and asset preferences require a restart.
File paths and diagnostic switches keep ordinary controls; they do not invent
geometry in the preview. Display settings continue to treat the 1280 × 720
desktop mirror separately from the runtime's per-eye resolution and FOV.

The preview uses reference geometry, not extracted retail art. It shows the
same units, rotation order and screen anchor as the runtime. The iDroid screen
keeps its 16:9 ratio, grows upward from its lower-edge anchor and pivots there
when tilted. Grip translation is in controller grip space; grip rotation uses
the controller's pointing basis. General hand calibration also affects held
objects; iDroid grip calibration applies only while holding that device.

The reference projection volume illustrates the intended emitter-to-screen
connection. It does not certify the retail projection effect at every fitting
extreme. Simulator eye captures, physical headset feel, native effect alignment
and forced tutorial stow are separate acceptance checks. See CURRENT.md and
IDROID_ALIGNMENT.md for the current result.

The UI check uses a disposable INI and headless browser. It cannot launch a game
or write the player's installation:

```powershell
# The local preview server supports the recordings' byte-range requests.
python tools/field-guide/serve_release_desk.py --port 8766
# Run with Playwright available to Node.
node tools/qa-vr-fit.cjs artifacts/dev/vr-fit-qa
node tools/qa-launcher-3d.cjs artifacts/dev/launcher-regression
```
