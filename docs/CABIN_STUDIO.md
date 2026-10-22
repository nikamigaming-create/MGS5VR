# Physical cabin workbench and camera

The six title tapes rest on the owned helicopter's brown seat. Grip picks up a
tape; release gives it gravity and contact with the measured seat, case, floor,
walls and other tapes. Carry it to the existing cassette player beside iDroid,
align it with the slot and release. A settled available tape requests its
actual native title action once. Touch the player's EJECT area with Trigger to
remove it. Continue uses the game's own title selection and loading handoff.
The separate cutscene player mesh and former floating radio are not drawn.

The tape labels are printed on both casing faces, with readable rear winding.
The native player's embedded lid is not animated by this change. The mod owns
tape physics; the source game's cutscene insertion animation has not been
identified as a reusable native interaction.

The instant camera rests on the case in the cabin and is available at the hip
in the field. With an admitted on-foot body, the resting camera and its pickup
target follow the native right hip in that body's render publication. Head
turning or leaning does not move it to the chest. Unavailable-body contexts
retain the reachable tracking-space fallback. Grip holds its side handle;
Trigger takes a photograph using a
separate native scene view from the camera's actual lens. The rear finder shows
that view. It is not a desktop screenshot. Camera input is consumed while held
so the shutter does not also fire Snake's weapon. The camera is mod-authored:
no suitable native instant-camera model has been confirmed in the item catalog.

The camera's rear buttons use small icons: left/right arrows for Back/Next,
a printer for Print, a trash can for Delete, a camera for Live and pause bars
for Freeze. Resume changes the pause bars to a play triangle. Button size and
the existing hover highlight stay the same.
Aim the empty hand at a button and press Trigger. Back/Next browse the shared
saved-photo library; Print makes another independent paper copy of the selected
image. Live returns to the lens finder. Delete asks on the camera before erasing
the selected library entry; the buttons change to a check mark for Confirm and
an X for Cancel until the choice is made. Press the check to erase it, or X to
keep it. Paper disposal uses the same check/X icons.
Deleting a saved photo never removes, changes or annotates an existing print.
Prints have their own object IDs, locations, captions and marker drawings.

Freeze stops the native game world under a separate mod-owned pause while
head, hand, lens and native tracked-skin rendering keep moving. It does not open
the stock Pause screen. Press Resume on the same button to release it. Stowing
the camera, losing XR focus/tracking or changing scene also releases only the
camera's pause. Freeze is available in accepted gameplay/ACC, with the game
closed to native menus; the launch/title cabin's authored backdrop stays live.
Taking a photo normally leaves the field live. Capture uses the native lens
scene; mod-added props and tracked hands are not baked into the photo.

Photographs become grabbable prints in the cabin. The latest print is offered
at the camera's front film slot and can be held; stowing it keeps it in the cabin collection
for the next visit. The same gallery is shared by the launch cabin, native ACC,
Mother Base and ordinary field scenes, without a mission-number whitelist.
Mounted vehicles, cutscenes, native menus and equipped binoculars own their
existing controls; stow/close those before picking up the camera.

Grip peels a sticky note from the stack. Aim an empty hand at a held note or
print and press Trigger to open the spatial keyboard. Done, Back and Clear
edit only that paper. Trash asks before disposal; Delete confirms, Cancel keeps
it. Confirmed disposal removes that note or print permanently. Notes have no
digital library, history or recycle bin; persistence records current objects.
The physical marker draws while Trigger is held near the paper. Move a paper
onto the native photo wall facing into the cabin and release to attach it;
Grip pulls it off again.

Captions use the Apache-2.0 Permanent Marker font from
[Google Fonts](https://github.com/google/fonts/tree/main/apache/permanentmarker).
Its font, license and metadata are bundled in `assets/fonts/permanent-marker/`.
Text supports the current keyboard's ASCII characters, up to 96 per paper.
The studio keeps at most 32 papers, 512 saved photographs and bounded freehand
strokes. A full paper collection does not prevent saving another photograph.
Paper state
and PNGs live in `%LOCALAPPDATA%/MGS5VR/CabinStudio`; they do not modify MGSV saves.
Writes and PNG encoding run off the render thread. Reloaded papers retain their
photo identity, caption, drawing and board placement.

Props and hands use the same native render publication. Physical props join
verified native scene depth from the exact eye camera and projection; the
native pixel scene's inverse view is checked before accepting that depth.
Held tapes, camera, marker and papers follow the solved native palms, including
the arm's reach and surface limits. The camera has an authored side-palm socket;
paper is held at its edge with a matching native-chain thumb/index pinch. Native firearm wrist aiming cannot override a held
studio prop. Native left-hand iDroid contact remains its existing cinematic fit.
World enemy labels are masked by the physical binocular housing. Binocular
reticles and native acquired marks remain inside their actual optical view.

The retained actual-game workbench loop on DLL `7cf6b1b01747` passed pickup,
drop/recovery, insertion/ejection, camera shutter/PNG, keyboard caption,
freehand stroke and paper attachment/detachment: all 16 phases in retained run
`20261012T191009165360Z`. Both camera and attached-board eyes were reviewed.
The mismatched hand/prop poses were reproduced: the test reached over 1.7 m
while the arm correctly limited its reach. The bot now approaches the furniture,
reads the updated cabin anchor and preserves measured controller grip/aim pairs.
Separately, the shared box-face triangulation omitted half of a quad; that was
corrected, removing the broken pad, film-slot and paper-back geometry.

DLL `369a855abe1c` retains those fixes and enlarges short handwritten captions.
After a fresh game launch, run `20261012T193243872218Z` restored the exact pinned
note, drawing and Polaroid identity; both eyes show their board placement and
readable TAPE caption. The earlier cold-start attempt checked before the cabin
rig was ready; the case now waits for that same native cabin owner.

Retained field run `20261012T193635692919Z` on that DLL passed native Continue,
2x/4x physical binocular zoom, acquisition, clear, no automatic re-mark on
look-away/return, explicit moving-target re-mark and a native-scene camera photo
with no native weapon trigger. Both-eye clear/return/mark/zoom and camera images
were reviewed. The left hand remains at the supported side cup for binocular
captures and rests below the camera afterward. These scoped views do not certify
every wrist angle, all foreground occlusion, native player-lid animation or
physical headset ergonomics. Failed investigations are not accepted outcomes.
Run `20261012T193959514809Z` additionally passed left-hand iDroid opening,
translation/turn, ordinary stow and immediate reopening. All four both-eye pairs
were reviewed for retained native palm contact and connected forearms; forced
tutorial stow and every wrist angle remain separate obligations.

Use `tools/workspace.py bot --command bench --seconds 120` for the reversible
cabin loop and the isolated mission 10195 `studio-field` case for field optics
and camera checks. `studio-board` checks reload without altering saved papers;
`studio-hands` reviews native left-hand iDroid contact and ordinary stow/reopen.
Retain reviewed runs before recording them as evidence.
`studio-manager --suite <policy.json>` runs bounded gallery, independent-print
reload, owned Freeze or ACC binocular-routing cases in a declared native scene.
The current retained gallery/Freeze runs and scoped limits are in
[the acceptance ledger](CURRENT.md#october-12-camera-gallery-freeze-and-acc-grip-routing).
Preview stays 1280 x 720; actual eye targets and FOVs come from OpenXR. Simulator
results do not certify physical headset ergonomics or whole-game readiness.
`Play-Dev.cmd` tests the candidate; `Play.cmd` keeps the published build.

The October 12 anatomical-eye review found the field camera mostly hidden
behind Snake's belt at its first native hip placement. Its center is now at
least 35 cm to the right of the pelvis and 4 cm forward of the right hip,
retaining the native hip height. The captured standing shoulder/arm chain
has about 3 cm of reach remaining at that pickup socket. This calibration
still requires both-eye visibility and same-socket pickup on the candidate;
other bodies, outfits and physical-headset reach remain separate checks.
