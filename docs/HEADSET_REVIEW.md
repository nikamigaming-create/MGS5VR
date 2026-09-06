# Quest 3 review and next playable slice — 2026-09-06

The first physical headset run establishes that the current native VR path can
support real movement and rifle combat. The user reported that it worked well,
but rejected the intrusive HUD and asked for better arms and clearer weapon use.
The next slice is a clear main view with a coherent AM MRS-4 rig and live forearm
status. Enemy and location markers should remain available, per the user's request.

## Evidence and limits

- Installed DLL: `4B299FF8E47EB5EE7934F1FA3E57424913F5DE4B2102752114428A2896A8FB07`,
  the overnight controller build associated with source commit `e82d500`.
- Physical runtime: Oculus, Meta Quest 3. Game process 9076 entered a focused
  session and consumed OpenXR controller input. The game has since exited.
- Supplied Oculus Mirror desktop recording: 153.194646 seconds, 1590x1400,
  H.264 video with 4,597 encoded frames at approximately 30 FPS, plus an AAC
  audio stream. Audio content/synchronization was not assessed in this review.
  Recording SHA256:
  `78EDEA80D6DD386EF6A60F51D9C05B3218D17D7C0229939D61D2F7EF487113D5`.
- Review used contact sheets spanning the entire recording, denser half-second
  samples around activation, hand motion, reload, combat and lowering, and
  original-resolution stills. It is a timestamped defect review, not a full
  normal-speed or every-frame stereo acceptance pass.
- The recording contains one Oculus Mirror view, window chrome and a desktop
  notification. It does not establish which eye was selected, both-eye visual
  correctness, application FPS, tracking latency or long-session comfort.
- Native VR was toggled on at Unix time 1788705194493 ms and off at
  1788705343854 ms: 149.361 seconds. The log records 13,440 projection submissions;
  native diagnostics record 8,934 source eye pairs, zero scene rejections and
  zero reported scene failures. These counters are runtime evidence only.
- The physical run records 110 controller-shot diagnostics, all with the
  printed `barrel_dot=1`, and no logged `Controller aim unavailable` messages.
  Several early-return paths are not diagnosed, so this is not proof that every
  shot used the adapter. Exact impact/obstruction acceptance remains open.
- Native shutdown logged session destruction, instance destruction and worker
  cleanup before process exit. This is one successful Oculus shutdown; earlier
  Meta Simulator/Operator teardown stalls remain a separate unresolved case.
- The fresh local rebuild made during preflight has SHA256
  `6729001B0FC4C80FA51B8BD0994471D4C2A5AE515BF88A11469F45F0E828338D`.
  It passed all five suites but was not installed for this physical run.

Video timestamps below are relative to the supplied file. There is no exported
video-to-log clock join; event-level correspondence must not be presented as
exact per-frame pose or shot correlation. Private recording and log references
are retained in `artifacts/physical-headset-review-20260906.json`.

## Visible findings

| Video time | Observation | Likely owner and limit | Result |
| --- | --- | --- | --- |
| 00:00–00:02 | Giant screen changes to a full game view after the manual activation chord | Matches the physical run's activation event; automatic gameplay/cinematic classification is still absent | Manual transition observed |
| 00:03–00:17 | The lowered rifle's stock crosses the foreground while the hands adopt open poses; sleeves/cuffs produce pointed shapes | Inference: native stow/hand animation and tracked wrist/weapon ownership disagree; garment skinning also needs inspection | Fail held/lowered presentation |
| 00:18–01:10 | Player moves through the outpost with a visible rifle, changing aim and ammunition | Physical movement and firearm use are demonstrated; this is not all-state or all-weapon coverage | Bounded gameplay observed |
| 01:12–01:18 | Reload is visible, but the bionic hand/magazine contact is not consistently convincing; thin geometry extends from the arm | Inference: support/reload animation contact, wrist calibration and sleeve deformation require separate checks | Fail complete rig acceptance |
| 01:37–02:05 | Large climb/reload/action banners occupy the central view during movement and firing | Native flat UI is still composited into the game eyes; current UI hooks do not extract or relocate it | Fail HUD placement |
| Throughout native gameplay | Weapon/ammo panel remains in the lower-right view; a screen reticle and world labels remain visible | Preserve useful enemy/location cues, but separate them from the personal HUD. A screen reticle must not imply verified barrel aim | Fail forearm HUD; marker correctness unproven |
| 02:21–02:31 | Looking down/lowering brings large angular sleeve/arm surfaces across much of the view | Inference: shoulder/garment skinning and animation-state changes, not merely pose smoothing | Fail arm geometry/visibility |
| Around 00:48, 01:19–01:22 and later turns | Black strips appear at a mirror-view edge | FOV coverage, reprojection and mirror crop must be distinguished before diagnosing the renderer | Unproven cause |
| 02:31–end | View returns to the theatre screen | Matches the second manual toggle; the log's sampled cancellation is manual | Manual exit observed |

The desktop notification around 00:48–00:57 is capture content. It is not the
game HUD and must not be used to diagnose the game's UI path.

## Implementation order

1. **Repair the rifle/arm presentation.** Reproduce neutral, ready, lowered and
   reload poses with the same owned outfit and AM MRS-4. Replace calibration from
   an arbitrary activation animation with named controller-to-palm/wrist and
   weapon-grip transforms. Establish explicit held, supported, reloading and
   stowed states. Keep a held weapon attached when it is lowered; intentional
   stow must release it coherently. Correct finger/contact poses and preserve
   the existing muzzle/source-frame join.
2. **Repair shoulders and sleeves.** Check the actual shoulder, clavicle,
   forearm/twist and garment weights under these poses. Use stable shoulder and
   elbow policies, wrist-twist/reach limits and appropriate native animation
   variants. Keep both hands and continuous forearms visible. Do not treat
   hiding all arms or smoothing a malformed skin pose as a repair.
3. **Extract and relocate the actual HUD.** Identify the native weapon/status,
   action-prompt and world-marker draws separately. The current `ui_renderer`
   hooks observe UI jobs and patch some matching projections; they do not yet
   provide a HUD texture. Render live personal status on the left forearm from
   the same source pose as the world and arm. Remove the duplicate from the eye
   image once its replacement is usable. Put contextual prompts near their
   relevant object or on a small readable panel; keep blocking menus escapable.
   Preserve enemy/location markers and validate their world projection in both
   eyes. Handle subtitles and damage feedback separately from ammo/status.
4. **Make the controls explicit.** Retain the current hold-to-ready/fire/reload
   mapping for this slice, with a compact in-game guide away from the aiming
   area. Clarify support-hand attach/release and consistent lowering/stowing.
   Validate the ordinary rifle before extending scopes, other weapon families,
   throws, CQC and mounted weapons.
5. **Validate in the simulator before another physical test.** Use the sequence
   below on the same saved checkpoint. Investigate edge coverage with paired-eye
   captures and measured head motion. The user requested simulator development,
   bounded disk use and a final 15-second video once the slice passes. Automatic
   cinematic switching and universal skipping remain later work and are not part
   of this rifle/HUD acceptance claim.

Stable arm attachment is a prerequisite for a stable forearm display. UI-source
discovery can proceed independently, but the integrated next-build target is
both a correct rifle rig and a clear main view.

## Current controls, while native VR is active

| Control | Current behavior |
| --- | --- |
| Right grip held | Ready the weapon |
| Right trigger with weapon ready | Fire |
| Left grip held | Request the native support-hand pose |
| Right B | Native reload; there is no manual magazine-grab interaction |
| Left stick | Native movement |
| Left grip + left stick click | Toggle native VR/theatre; also used before menus |
| Left Menu tap / hold at least 0.55 s | Native iDroid / pause mappings |

Left trigger remains a legacy ready/aim input. Support-hand tracking and finger
contact are still experimental, so the mapping table is not an ergonomics pass.

## Next-build acceptance

Exercise the existing checkpoint without continuously recording: look down at both
hands, turn wrists, lower/raise the rifle repeatedly, attach/release the support
hand, aim away from the head direction, fire at a visible surface, reload, read
the changed ammo on the forearm, then walk, crouch and turn while checking it.
Inspect a bounded set of captures from both eyes. Once these checks pass, record
the requested 15-second single-eye demonstration; it is not a substitute for the
longer functional checks. Stream encoding or bounded temporary storage should
avoid accumulating raw frame folders. Preserve one known working rollback build.

Pass only when no sleeve spikes or hand/weapon separation appears in the tested
states, the forearm display stays attached and shows actual changing game data,
the main view has no duplicate personal HUD, useful world markers remain, and
the muzzle/impact/ammo behavior agrees. Tracking loss and menu transitions must
remain recoverable. This initial recording does not pass those gates.
