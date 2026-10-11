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

User direction, 4 October 2026:

The handheld iDroid belongs in the LEFT bionic hand, matching Snake's native
arrival cinematic. This supersedes the earlier right-hand iDroid direction.
Keep ordinary status and HUD on the left forearm. Match the cinematic contact
and projector from the owned native data, and verify handset motion together
with its screen in both eyes.

User direction, 10 October 2026:

Keep exactly two retained local builds: `play/` is the pinned playable build,
and `build/dev/` is the working build for tests and debugging. The user selected
the installed `6ac226f4bf2c` candidate as the initial pin; headset acceptance
is still pending. This supersedes automatic replacement of `play/` during builds.
`tools/build.ps1` builds and checks development only. `-Deploy`, `Play-Dev.cmd`
or `workspace.py launch-sim` explicitly installs development for testing.
`Play.cmd` verifies and restores the pinned build before physical play.
Only an explicit user decision to promote a reviewed candidate authorizes
`python tools/workspace.py pin-build`. Building or debugging never implies
permission to replace the pin. Never label pinning as headset acceptance.
Do not keep dated packages, old binary snapshots or persistent rollback builds.
Transactional staging/rollback exists only during an update; retain a failed
transaction only until recovery. Compiler output belongs to development, and
the installed game is a deployment of one slot, not another retained version.
Use `tools/clean-local-builds.ps1 -Apply` to remove recognized obsolete builds
after both slots verify. Keep source, personal settings, saves and test evidence.

This user's physical headset uses wired Meta Quest Link and Meta OpenXR, not
SteamVR. Honor the saved MGS5VR `HeadsetRuntime` preference before Windows'
global runtime. Both local Play commands use the shared headset launcher;
the pinned payload remains unchanged. Simulator leases must restore the
per-game Meta runtime. Do not switch a physical play session to SteamVR.

User direction, 11 October 2026:

`play/` must always contain the last successfully published release and its
matching launcher, available for local play through `Play.cmd`. `build/dev/`
contains the latest completed, checked candidate for the user's testing through
`Play-Dev.cmd`. Keep both launch paths ready while development continues.
An authorized release is packaged from checked development without replacing
the published pin. After publication to the requested server/GitHub destinations
succeeds and the uploaded payload is verified, pin that exact candidate. Both
slots then match until the next development changes. Preparing a package,
pushing source or creating a draft release does not advance the pin. This
supersedes the earlier choice of an unpublished candidate as the normal play
pin; publication and pinning still do not imply physical-headset acceptance.

The user explicitly requested fixing the PC's physical-headset default after
a Play-Dev launch selected SteamVR. Windows' 64-bit OpenXR ActiveRuntime is
now Meta Quest Link, and both local Play commands explicitly pass `-MetaLink`
to the shared headset launcher. Preserve this choice through development and
simulator cleanup. Do not switch the physical session back to SteamVR.
