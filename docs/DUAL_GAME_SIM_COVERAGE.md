# Both-game equipment SIM runs

## 13 September continuation

The current B1E1D8C8 build fixes a reproduced ACC-to-mission transition: when
the former verified player stops publishing, a fresh matching mission player
gets a new VR origin and stereo generation. SIM now reaches the Afghanistan
field view with tracked hands, rather than retaining the ACC sky. A concurrently
live unrelated camera and manual VR disable still prevent automatic adoption.

The 100% test save has been observed on the native title screen. All runs remain
single-player; the native Pause / Disconnect action switches the mission offline.
The title percentage is not per-equipment coverage. Stock test loadouts are being
prepared to avoid the downloaded save's modified custom-weapon configurations.

The 7CD5079A optics follow-up connects the equipped native resource's rear/front
sight sockets to the completed weapon/skin frame. A real SIM run of stock
`EQP_WP_60204` (BAMBETOV SV, native sight 17) shows its fixed 4× image/reticle in
the aligned right eye and an ordinary unzoomed left-eye view. Two tracked shots
were issued, and its partial reload returned the wrist ammunition display to
9/31 (including a chambered round). The lens returns after reload/motion and
closes when lowered. Fixed-power zoom remains 4×. Impact/obstruction, empty
reload, other sights and individual grades remain to be exercised.

This run also found a reticle shader compilation error in the earlier B1E1D8C8
build; the 13.1 hotfix corrects it and adds a real shader/draw pixel test. On the
fixed build, binocular hold-B equip, 2×/4× lenses, two-hand support and B stow were
rerun. Private process-audio takes are `tpp-physical-sniper-04-av.mp4` (24.669 s)
and `tpp-binocular-restored-05-av.mp4` (25.135 s). The native video source precedes
runtime optical aspect/crop; final both-eye screenshots were reviewed separately.
All-weapons/all-gadgets coverage and the final combined showcase are incomplete.

Fresh-install binocular import now succeeds from the owner's `chunk0.dat` and
`texture0.dat`, reproducing both exact working local asset hashes without
bundling retail data. The importer validates extents, checksums and expected
output hashes and preserves modified existing files.

Current work is per equipment ID, not a single representative firearm. Variants
sharing a model are kept separate. The tests include equip/stow, actual use and
effect, partial/empty reload, alternate modes, optics, switching, independent
head/hand motion and both-eye presentation where applicable. Native definitions,
selection alone and unit tests are not successful gameplay runs.

`tools/equipment-coverage.py` builds a private JSON worklist from the owner's
extracted TPP equipment table and GZ gun/support parameter tables. It preserves
every parsed ID, refuses to overwrite recorded results, and starts every case as
not run. Classify NPC/demo/resource entries explicitly before excluding them.
Binoculars, iDroid, NVG, mounted weapons and contextual/buddy systems also need
their separate action cases; the gun tables are not the whole game.

## 12 September development run

GZ 1.0.0.5 now has an independently mapped scene renderer, with stereo exercised
in the title cabin and playable Ground Zeroes mission. Head yaw and translation
were exercised. The camera remains the native third-person camera, **not a
completed tracked first-person rig**. The simulator observed native AM MRS-4
ready/fire/reload: 30/210 → 27/210 → 31/206. These are partial native-action
observations; muzzle alignment, obstruction, tracked hands, all reload variants
and final temporal acceptance remain open. Native binocular activation was also
observed; the scope overlay and magnification are not yet VR-correct.

The title-to-mission loading screen previously disappeared behind empty stereo
submissions, hiding its Start Mission confirmation. Present now detects a gap
in native camera publication and exposes the live loading screen. The same
camera resumes with a new stereo generation. A different camera is still
rejected; a loading gap does not bypass camera ownership. Automatic loading
fallback and same-camera resumption were exercised in GZ SIM.

The inspected TPP table has 486 IDs, of which 209 are ammunition/bullet/box
resources and 277 need player/buddy/demo eligibility classification. The GZ gun
and support tables contain 19 enabled entries, including its underbarrel
attachment. These counts are inventories, not success totals or a claim that
every entry is player-accessible. Patch load order and contextual systems still
need reconciliation before calling either inventory exhaustive.

The current SIM run found and fixed a shared-action input collision: the
Vive/WMR trigger-to-confirm fallback could also become Touch A. Physical face
buttons and legacy select/menu inputs now use separate OpenXR actions, resolved
against each hand's active interaction profile. Fresh GZ firing/throwing no
longer changes stance through that collision. Its native WU pistol partial
reload, rifle empty reload, flare call and arriving helicopter, magazine throw,
and NVG enable/disable also have bounded observations. Grenade impact and
tranquilizer/distraction effects are not yet verified.

Fresh TPP recordings show A quick-switching from MRS-4 to WU while readied,
tracked-barrel shot calls, and changing pistol ammunition/reload on the native
wrist HUD. The rifle empty-cycle recording needs temporal review: automatic
reload finished before its requested empty keyframe. Selection and shots alone
do not complete impact, obstruction, reload-animation or tracking checks.

The neutral chooser correctly waits for a deliberate category selection, but
its left edge clipped at a normal close wrist pose. A new shared placement fits
the unfolded guide/cards inside both requested optical frustums without shrinking
text or moving the skin-mounted status. Unit coverage includes asymmetric and
canted eyes and source-pose transforms. Both final eyes now show all four guide
tiles with margins at the reproduced watch pose. The native cards and repeated
selection/motion sequence still need their fitted-layout review.

Neither game's all-equipment SIM coverage is complete. The active TPP campaign
originally showed 1% completion. The user then explicitly authorized an all-unlocked test
save and confirmed the campaign is disposable testing data. A separately kept
100% candidate is installed and its title percentage observed, with a rollback snapshot; actual all-equipment
availability still needs native inspection. No physical-headset testing was
requested. The latest user order is arsenal coverage, then the combined film,
then GZ tracked first-person. GZ native third-person stereo may be supporting
footage, clearly labeled. The finished all-equipment showcase is not assembled.
