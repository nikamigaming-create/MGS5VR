# Finger articulation and wrist selection review

**The first physical test failed thumb and support-hand acceptance. Release is held.**
The user reported backward thumb motion and unwanted two-hand attachment that
obscured the firing hand. The SIM observations below did not catch those defects.

The later corrective headset run received positive feedback, with rotated picker
directions and grenade aiming identified next. The menu direction correction is
retained; the grenade candidate failed its final arm-visibility review and is
withheld. See [the latest results and limits](MENU_GRENADE_REVIEW.md).

The corrective source separates thumb flexion from finger flexion, removes the
left-grip support override, and requires the palm to dwell within 10 cm of the
weapon's support grip for 150 ms. Pulling more than 20 cm away releases contact.
The acquired contact survives native reload motion, but a one-handed reload does
not grab a free hand. Attachment blends over 180 ms. This correction still needs
final-eye/headset acceptance; there is no new passing video for it yet.

Corrective DLL SHA-256:
`4DC82F49C7CB6644D27F5D7B205866766C68ED6EE359B97EBF6FA4DCC6A23123`.
It is installed with the game closed; its Release build and all six local CTest
suites pass, including contact dwell/release and a complete mirrored thumb-chain
fixture that rejects folding behind the wrist.

Rejected 2026-09-07 candidate DLL SHA-256:
`9028782E9291BE7DE50B52B2D6C5C5138688D003249BEBC35B68266E9429EE7A`.

The current change adds controller-driven finger articulation, two-controller
aiming and wrist selection while walking. It does not finish the full VR mod.

## Observed in the simulator

| Feature | Result and limit |
| --- | --- |
| Free hands | Both hands visibly open, curl with grip, bend the index with trigger, and reopen. Native weapon contact remains authoritative. Individual finger tracking is not implemented. |
| Two-handed aim | Moving the support controller changes the rifle's direction while retaining the firing grip and native support contact. The authored barrel supplies the aim axis. One bounded rifle shot matched its rendered barrel (`barrel_dot=1`). Physical controller fit remains unproven. |
| Wrist input | Hold LT to open, right stick up/down to browse, right/left to change category, release LT to close. The left stick continues locomotion. No game pause is added. |
| Wrist layout | Picker width reduced from 70 to 42 cm and its lift above the wrist from 16 to 9 cm. Cards and descriptions were readable in both sampled eyes at the inspection pose. |
| Rendering | World, hands and menu remain present in the reviewed views. The source-scene camera and stereo projection math are unchanged by this slice. |

The published clip is a silent left-eye MP4: **14.871 seconds, 536,413 bytes,
57 reviewed frames**, measured at 3.76 captures per second. Capture rate is not
game frame rate. It covers free-finger curls, wrist rotation, opening/browsing,
rifle selection, support-hand steering, walking with the picker open, switching
to secondary equipment, and closing. No fully blank frames were present.

A separate private sequence reviewed both final eyes. Its left/right requests
are sequential, so frames around input changes can show different moments;
that composite is not a stereo timing or headset comfort certificate.

## Performance boundary

The 1080p compromise enables native AA and keeps Extra High textures, filtering
and model detail. Critical-worker scheduling and variable native frame rate
remove the earlier approximately 60 FPS producer limit. Isolated SIM motion
tests were mostly in the high 80s to 90 fresh stereo FPS, with brief 80–84 FPS
stretches. The 900p trial lowered GPU time without eliminating those dips, so
1080p was retained. Later live windows on this candidate reported about 89.5–89.6
fresh FPS with other GPU work running. Complete windows from the subsequent
physical Quest 3 gameplay run reported roughly 86.6–89.2 fresh FPS; transition
and session-recovery windows were lower. These are samples, not a whole-game
guarantee. See [performance measurement](PERFORMANCE.md).

## Verification and unfinished work

Release build and all six CTest suites passed, including native frame-rate
instruction/flag preservation, D3D11 copies/timing, input transition contracts,
mirrored finger curls and authored-axis support aiming. Tests and SIM observations
do not certify every weapon or interaction.

Clean removal of the shoulders/upper sleeves remains unfinished; it requires
isolating the native meshes while retaining both forearms and shadow geometry.
Extreme arm poses, physical alignment, capacitive-touch behavior, vehicles,
all loot/CQC/item actions, escaping marker streaks, immersive full menus and
all-area LOD behavior remain open. No third-party texture or LOD pack is installed.
