# Stereo view modes: intended behavior

This is the requested design, not a list of completed features. First-person is
the primary target in both The Phantom Pain and Ground Zeroes. GZ's present
scene-camera experiment does not supply its missing first-person rig or arm HUD.

## Three independent choices

Keep the XR session, viewpoint, and input device independent:

| Choice | Values | What changes |
| --- | --- | --- |
| Presentation | Stereo VR / explicitly exit VR | Stereo remains active through view changes, menus, and cutscenes. Only an explicit exit restores flat play. |
| Viewpoint | First person / third person | Changes the player-eye or native chase-camera anchor, arm visibility, aiming, and HUD placement together. |
| Input | VR controllers / native devices | VR controllers use a complete mode-specific mapping. Native gamepad, keyboard and mouse retain their game-owned behavior. |

These are not config settings in the current release. Do not represent a flat
theatre panel or the native-button escape hatch as a finished third-person mode.

## First person

Use the same principles in both games: tracked player-eye anchor; opaque,
properly gripped hands/arms; tracked weapon aiming and optics; arm status HUD;
readable equipment/command selectors; spatial enemy/objective markers; captions
and menus that remain inside VR. Each game needs its own verified player, skin,
weapon and UI adapters. TPP addresses cannot be reused as GZ object layouts.

## Third person

Render the native world independently for both eyes around the native chase
camera, retaining its boom, obstruction handling, transitions and authored
animation. Head rotation is a look offset around the chase camera, not a command
to turn Snake or steer a vehicle. Right stick or mouse controls the chase camera.
Physical translation needs a bounded, collision-aware camera offset; it must not
move Snake, detach the camera, or expose geometry beyond walls. Recenter clears
that offset without changing the character's facing.

Use native UI content with VR placement. Enemy and objective cues remain at
their actual world targets. Health/ammo, captions and notifications use readable
depth-placed panels with a quiet central reading area. Menus become interaction
panels within stereo, not an exit to desktop. Avoid duplicating native screen
pixels and world markers. No on-arm HUD, floating controller arms, tracked weapon
attachment, motion CQC or VR wheel grab in this mode.

Preserve the native camera for close combat, ladders, horse/vehicle use, scripted
events, scopes and binoculars; do not convert every native camera transition into
a change of VR mode. Comfort options can separately constrain camera motion.

## Input ownership and switching

Native device mode must pass through physical XInput and rumble, and leave the
game's keyboard/mouse path untouched. Do not merge two aiming sticks or OR two
sets of held actions. VR-controller mode gets the full native button set plus
documented shift combinations for missing buttons; urgent actions remain direct.
Device changes release the outgoing synthetic state before accepting the new
source. View changes apply at a neutral boundary and retire the old rig, optic,
gesture and UI state together. Keep one pose publication for both rendered eyes.

Current code boundaries that require work:

- `src/gamepad_hook.cpp`: active XR currently claims XInput slot zero; physical
  pad passthrough happens only when the XR sample is inactive.
- `src/camera_observer.cpp`: the tracked player owner/head path is TPP-only;
  GZ observations deliberately do not imply player or skeleton ownership.
- `src/xr_runtime.cpp`: rig availability, gameplay context, automatic entry and
  native-button mode are currently coupled. They need separate view/device state.
- `src/ui_renderer.cpp`: wrist status and native scene UI need an explicit
  third-person destination instead of simply disabling the wrist experiment.

Loading or a lost render transaction must not submit stale geometry as live
stereo. Keep the XR session alive with a safe VR loading/transition surface until
the correct scene can resume. A flat video texture can exist on a depth-placed
panel within stereo; that is different from dropping the headset into flat mode.
