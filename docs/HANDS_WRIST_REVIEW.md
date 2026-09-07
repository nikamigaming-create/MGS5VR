# Finger articulation and wrist selection review

2026-09-07 candidate DLL SHA-256:
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
fresh FPS with other GPU work running. These are samples, not a whole-game or
physical-headset guarantee. See [performance measurement](PERFORMANCE.md).

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
