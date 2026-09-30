# MGS5VR interaction mandate

Keep the desktop preview at 1280 × 720. VR uses the active runtime's per-eye
recommendation and correct eye FOVs; never derive VR sharpness or stereo
acceptance from the desktop preview size.

User direction, 28 September 2026:

This is a complete VR game. Preserve the existing VR interactions. Use native
game controls only when absolutely necessary; even then, retain a VR interaction
for the player wherever possible. Implement necessary native integration behind
that VR interaction rather than replacing it with stock gamepad operation.

The binoculars remain physical hand-held binoculars: equip and hold them, aim
with the hand, zoom, automatically acquire visible enemies, and explicitly mark
targets using the existing VR bindings. A mission or tutorial compatibility fix
must preserve those behaviors. Do not substitute native stick aiming or add a
second binocular display.

Reproduce reported failures in the actual game before changing a working flow.
The release task is to fix defects and preserve established VR behavior.

Do not automatically open Pause and leave a failed test on its Resume/Skip
screen. Release test inputs and close test-opened menus during cleanup. The
user has explicitly asked not to leave test sessions and panels open.

The weapon/status display and HUD popups stay on the LEFT bionic forearm.
The right hand carries iDroid. Opening it must not move ordinary status onto
the right arm. Preserve the established month of rig and interaction work.
Do not shut down or restart Steam. No desktop or window-control automation.

User direction, 29 September 2026:

Work from this checkout. `play/` is the fixed local test installation tree.
Run `tools/build.ps1` to build, test, refresh it and synchronize the recorded
local game installation. Do not make dated distribution folders for local
iterations. ZIPs and public release assets are for explicitly requested GitHub
releases. Read `docs/CURRENT.md` for current acceptance and next work.

Keep floating aiming reticles off. Preserve physical gun sights and reticles
inside their actual optics, with sight, muzzle, hit and hand alignment verified.
Keep ordinary HUD on the bionic wrist/arm. Pause and explicitly selected
off-wrist presentation belong on a spatial stereo panel. Preserve physical
binocular inspection, hand aiming, zoom, acquisition and marking. Do not
replace optics with a detached zoom window. Audit control chords using the
effective personal bindings before proposing changes.

The handheld iDroid's projection must match its native emitter and authored
presentation; study native/cutscene evidence before changing alignment.
Acceptance includes ordinary and forced tutorial stow and immediate reopening.

Use `tools/workspace.py bot` for bounded local runs. It defaults to observation;
Pause is exercised only by an explicitly selected case. Promote a run to
acceptance evidence before relying on it in the ledger; scratch runs expire.
Full-game playability is the release objective. Scoped scene passes and unit
checks do not certify whole missions, every weapon, or headset acceptance.
