# Simulator flicker investigation — 27 September 2026

## Current RC3 result — 28 September

DLL `dc4934e32394e0e8a8ae5beb4fdaa29da3de0b55c2fecab3442f779969119c3a`
fixes the reproduced cinematic activation gap. The complete unskipped native
Mission 1 intro returned to player control after 128.516 seconds. Startup,
Continue/loading and intro telemetry measured 17,035 frame cycles with zero
missing layers, zero invalid tracking and seven camera-handoff frames covered
by the bounded retained image. One startup cycle was requested non-rendering.

The old image keeps its source poses. A verified native camera handoff can
retain it for at most 250 ms while fresh stereo arrives; it is not re-admitted
as new gameplay. This adds no quad. A fresh new player camera after a movie and
a same-owner player resume both preserve only the short-lived authorization.
Ordinary travel, recenter and reference/graphics resets remain rejected.

Evidence: `artifacts/community-verification-20260928/dc49-intro/` contains the
exact identity, native sequence history, presentation-window log, review and
two compositor takes. Both-eye stills at 60/121 seconds and return were reviewed.
One-frame-per-second contact sheets were reviewed, not every video frame. The
actual takes captured 5.727/3.723 frames per second, so the counters are separate
evidence for missing submissions. Native fades are not missing-layer failures.

All 34 CTest groups and 7,298 native contract checks pass. Whole-game, sustained
natural-route, RC3 field Pause/stall and physical-headset acceptance remain open.
The scoped regression is R05-C4. Earlier E246/36DF attempts each retained one
return-to-player empty frame and remain failed evidence. See
[RC3 notes](RELEASE_CANDIDATE_2026-09-28.md).

## Historical RC2 result

DLL `cd91d99e5a892cc216bb444e7706670b14a742083214fe90ef97a33b4375c06b` retained the scene through an 800.247 ms producer delay: 900 measured frames in the neighboring telemetry windows, zero missing-layer submissions and 29 retained frames older than 500 ms. The world and both hands remain visible in reviewed during/after eye captures.

The complete CD91 session recorded **seven missing-layer frames**, at Mission 1 cinematic activation changes. The diagnostic rows show valid tracking and ready eye textures but an activation handoff with no submitted layer. This is distinct from the bounded producer-stall fix. A prior accepted pair must not be relabeled as fresh or used to bypass camera/pose admission checks. The next fix must preserve an appropriate prior image through the transition and be verified against the complete native intro. No new unverified transition change is included in RC2.

Evidence: `artifacts/community-verification-20260927/meta-cd91-field/presentation-review.json`, `meta-cd91-field/visual-review.json` and `meta-cd91-session.log`. The community ledger marks the broad startup/transition claim failed. Physical headset and sustained natural-route acceptance remain open. See [candidate notes](RELEASE_CANDIDATE_2026-09-27.md).

## Historical 853E investigation

The rest of this document records the earlier 853E bounded pass. Its then-unfixed prosthetic and label observations do not describe current RC2; the latest notes and ledger take precedence.

Installed DLL: `853e63eb1758561a44f27673c6cde517585e382d6e0f61a541ce019c620016f0`.

The latest bounded simulator run passed the missing-frame checks. Over 620.978 seconds, 49,974 frame cycles recorded zero missing-layer submissions and zero invalid-tracking frames. The runtime requested one normal non-rendering startup frame. This is a flicker regression result, not acceptance of every community issue or Quest Link.

## Causes and changes

- Initial helicopter activation and closing Pause could temporarily have no complete stereo pair. The last accepted startup/menu image now covers those handoffs. Compatible swapchains survive a source-generation change, while newly admitted pixels still require matching generation metadata.
- An already accepted stereo image expired after 500 ms. Under a delayed producer or runtime submission, that turned a freeze into a black flash. The compositor now retains one complete accepted image with its original eye poses and FOV. New images must still pass the 150 ms freshness and exact stereo-join checks; epochs, activation changes and clock reversal invalidate retained gameplay.
- A rig older than 150 ms was reported as a broken pose join even when its exact source camera, owner and activation matched. That delay now has a separate reason. It never publishes the old rig as new pixels; it permits only the already verified image to remain visible. Actual identity, source-pose and clock mismatches remain rejected.
- Every five-second telemetry window now reports missing layers in all rendering modes, image age, long retention and slow runtime submission. Empty-frame diagnostics include rig rejection flags and age.

## Evidence

Evidence is under `artifacts/community-verification-20260927/`. Every run records its actual DLL and process identity.

| Test | Result | Evidence directory/file |
| --- | --- | --- |
| Older DLL comparison | Failed: a completely black final-eye capture was reproduced on `8234d044…` | `flicker-comparison-8234-continue/` |
| C61B candidate, complete session | Failed: 19 missing-layer frames; maximum runtime submit 2,325 ms | `flicker-c61b-resource-pressure-video/presentation-summary.json` |
| 853E candidate, helicopter → loading → field | Arrived through native Continue; sampled both eyes reviewed | `flicker-853e-continue-03/visual-review.json` |
| 853E, deliberate 0.8-second native producer delay | Measured 801,134 µs. Image age reached 834 ms, with 30 presentations over 500 ms and no empty submission. Both eyes stayed visible and fresh rendering resumed | `flicker-853e-stall-800ms/result.json`, `visual-review.json` |
| 853E, Pause open/move head and hands/sticks/close | Native test passed; zero missing-layer submissions. Snake displacement 0 m and yaw change 0°. Sampled canvas fits both eyes; prosthetic motion separately failed | `flicker-853e-pause/result.json`, `visual-review.json` |
| 853E, complete bounded session | 620.978 s, 49,974 frame cycles, zero missing layers, 201 retained presentations over 500 ms; maximum image age 1,444 ms and submit 823 ms | `flicker-853e-final-eye-video/presentation-summary.json`, `runtime-current-session.log` |
| Actual composited-eye video | 33 captured left-eye frames over 12.219 s at 2.701 measured frames/s; no near-black interval detected | `flicker-853e-final-eye-video/capture.json`, `video-review.json`, `simulator.mp4` |
| Automated regressions | Five selected suites passed after the new core test executable finished compiling | `flicker-853e-regressions.txt` |

The sparse compositor video does not inspect all 90 Hz frames. The runtime counters, both-eye stills, and video are distinct evidence layers. Pause's recorded MP4 is explicitly a native source-eye recording, before final runtime composition.

## Environment findings and limits

The 12 GB GPU reached roughly 11.8 GB usage while MGSV, Valheim and OpenNV test sessions were active. Meta's synthetic environment also reported substantial shared GPU memory. Older-binary and newer-binary stalls both occurred under this pressure; resetting only the owned MGSV simulator tree temporarily improved frame rate. This supports a resource-contention contribution, not a claim that every stall has been eliminated.

Other projects also connected to the same singleton Meta simulator GUI. Creating that GUI does not prove that it remains exclusively owned. No other project's process was stopped. An isolated test awaits coordination approval. The final test game used a direct launch with child-scoped Steam app identity, v207 runtime and operator settings. Steam's ordinary relaunch was held by a surviving simulator child; a hidden direct game launch produced no first image and was recorded as a failed setup. The normal interactive direct launch succeeded. No desktop/window control was used.

The simulator launcher now records exact process generations and refuses orphan cleanup when live TCP clients cannot be matched to its recorded session. Unknown/shared clients and incomplete inspection block cleanup. Twenty-three ownership fixtures pass in both PowerShell 7 and Windows PowerShell 5.1, including fractional timestamp preservation. The actual cleanup path has not been exercised against this shared live simulator; it must not be described as a live acceptance pass.

The personal VR and controls INIs remain byte-identical. The previous C61B DLL is backed up in `D:\SteamLibrary\steamapps\common\MGS_TPP\mgs5vr-launcher-backups\20260927-132657-5714634a9e594eab84f3cfd69f9df7f8`. Saves and the global OpenXR registry were not changed.

## Failures recorded on historical 853E

- Left prosthetic geometry remains frozen during Pause even though native palm coordinates move. `modular_arms=0` remains a live failure. Palette arrays pass the saved-data checks; the first rejecting live ownership/root/group guard still needs identification.
- The helicopter tape labels overlap. The loading quad extends beyond a horizontal image edge in each neutral eye capture. Neither UI presentation issue is passed by this flicker result.
- Mission 6's bridge trigger has not been replayed. Mission 10040 is unlocked in the master save; that is setup evidence, not a mission pass.
- Mission 1's binocular tutorial still needs its native state bridge and first-playthrough proof.
- Physical Quest Link acceptance, sustained performance under isolated conditions, and the remaining community cases are open. Coco's adopted camera/visibility guards remain in this DLL; their automated tests do not substitute for the mission checks.
