# September 20 community test candidate

Extract the complete package and open **MGS5VR-Launcher.exe**. Select your owned
`mgsvtpp.exe`, choose **Update / Keep My Settings** (or Install VR), connect PC VR
and Launch with Steam signed in. This is a local test candidate; the previous
public release remains available.

## Changes

- iDroid and Pause default to a tilted panel in the 3D world. Handheld menus are
  optional (`settings.handheld_menus=1`). The iDroid pause leaves native UI
  updates running and releases before the closing animation, avoiding both
  opening/navigation freezes and a stuck exit.
- One-hand firearm alignment uses the authored muzzle and runtime aim pose.
  Hand, weapon, sights and shot origin still share the solved wrist.
- Binocular eye distance defaults to 30 cm, adjustable from 15 to 50 cm. The
  eye box is wider without increasing the lens mesh or changing weapon scopes.
- Cabin movement uses native swept clearance and floor checks. The special
  dog hand pushback was removed; native pet contact and response remain.
- Player height, hand offsets, menu size/depth/tilt, weapon smoothing, support
  grip/detach distance and resting/touched finger curl are editable.
- Connected Xbox pads retain native input in 3D and big-screen modes. Manual
  big-screen selection survives loading. Physical Xbox hardware still needs a
  separate playtest.
- Launcher settings can be edited and validated without displaying a window.
  Saves retain personal bindings, create backups and reject invalid changes.

## Controls to test

Tap **left Menu** for iDroid, hold it for Pause. **Left stick** navigates lists;
**right stick** pans the map; grips change tabs; triggers zoom the map;
**A** selects and **B** backs out/closes. A forced FOB tutorial can be escaped
by holding B for 750 ms and dismissing Exit Tutorial with A. If the device
remains out afterward, tap Menu to stow it before reopening.

Hold **left Menu + right B** for 550 ms to switch between 3D and the giant
screen. Release the controls after switching. Normal gameplay status and
equipment choices remain on the left forearm/wrist.

In **VR SETTINGS / CONTROLS**, choose `settings.turn_mode=snap` and set
`settings.snap_turn_degrees` (5–90, initially 30). Smooth turning is the default.
The selected snap angle applies on foot and in the cabin; center the stick
between turns. Mounted weapons and vehicles retain native camera input.

## Validation and remaining work

All 18 local automated suites pass, covering renderer/input contracts, hidden
launcher button dispatch, settings persistence, invalid edits, installer
fixtures and high-resolution configuration. The launcher test saves snap mode
with a 45-degree angle and rejects 91 degrees without damaging the saved file.
Live SIM turns also completed at +45 and -45 degrees, returning to the original
view. Personal turning settings were restored afterward. Release controls and
center both sticks for two seconds after saving to allow the live edit to apply.
Configuration validation does not establish 5K headset performance.

The candidate's SIM checks include cold launch, hand-selected Continue,
loading/Resume, forced tutorial recovery, native iDroid map zoom, tabs,
submenu navigation, normal B close and reopening. A continuous 28-second
compositor recording captures that iDroid sequence. Its approximately 3 fps
capture cadence is an operator-capture limit, not a measurement of game FPS.
Both-eye captures retain the 3D environment. The binocular lens rendered in both
eyes with the controller 30 cm forward of the head; 2x/4x switching preserved
the surrounding world. A one-hand pistol shot used the rendered barrel, aligned
with the supplied level runtime aim pose. These are SIM results, not G2 hardware
acceptance. A prone shot near a well edge was
blocked; crawling back allowed firing without standing. That does not resolve
the separate intermittent UI-sound report.

Physical G2 aiming/tracking and Meta idle/system-menu blackouts still need the
affected hardware. Arm reach, raised-arm sleeve deformation, prone model fading,
tanks/turrets, long sessions and third-party loader compatibility remain open.
This candidate does not claim every weapon, mission or VR feature is verified.

Contribution details are in `docs/FORK_REVIEW_2026-09-20.md`. The bounded adaptive
pacing contribution is included; no additional optimization work is planned for
this candidate. The alternate fork filter and supplied C++ replacements have
not been imported wholesale.
