# Acceptance matrix

Use pass, fail, or unproven per claim; do not convert a passing infrastructure test into full-mod readiness.

The latest [HUD/rig simulator review](SIM_HUD_REVIEW.md) documents a bounded
14.973-second clip and the current [controls](CONTROLS.md). It improves the
weapon/status and arm presentation while leaving the complete rig, every weapon,
interactive menus and physical acceptance open. It is not the longer full-mod
demonstration described below.

## Required full-mod demonstration

The requested full-mod demonstration is one continuous single-eye recording from actual native 3D gameplay. Deliver a normal single-eye video, not a side-by-side presentation. Demonstrate all six axes of head motion and report actual capture cadence separately from game/runtime frame rates. The current recordings do not provide 60 FPS video evidence. The user chose to try the experimental build on a physical Quest 3 on September 6 while this gate remained incomplete; [the review](HEADSET_REVIEW.md) records observed gameplay and remaining arm/HUD failures.

Start at the existing saved checkpoint with its equipped gear. Demonstrate translation along all three axes as well as yaw, pitch and roll, with visible world parallax. Look down at the forearm-mounted live HUD; move the arm while the head remains still, then move the head while the arm remains still. Raise the held gun, align its sights, aim away from the head direction, fire at a visible surface or target, and show the resulting impact and native ammunition change. Reload and return to the arm HUD, showing that its values updated. Hands, forearms, gear and the world must remain correctly attached and visible throughout.

Capture the submitted game eye and its attached geometry from the same frame transaction. Keep timestamps, pose/source-frame identifiers, capture rate and dropped-frame counts alongside the video. Do not interpolate missing frames or replace live HUD/game imagery with a mockup. Inspect both eyes internally for correct projection and stereo separation; deliver the user's requested single eye. A single-eye video can demonstrate parallax and interaction but cannot alone certify binocular stereo or physical-headset comfort. The current theatre preview does not meet this gate, and no qualifying video exists yet.

| Claim | Required evidence | Minimum motion/observation | Negative fixture | Current result |
|---|---|---|---|---|
| DirectInput proxy compatibility | Real system/proxy success and invalid-version parity | Startup + input enumeration | Invalid API version | Pass in test host; actual game startup and menus exercised |
| Shared D3D11 pixels | Actual GPU readback, two devices, resize/reset epochs, MSAA | Content update and paused producer | Overwrite an unread frame, mismatched context | Pass in GPU fixture |
| OpenXR lifecycle | READY through FOCUSED, advancing begin/end pairs | Bounded simulator/physical session | No headset available | Simulator and initial physical Quest 3 sessions progressed; full gameplay acceptance remains separate |
| Native process shutdown | Session/instance destroyed, worker joined, process signaled, DLL unlocked | Native Quit through OpenXR input | Exit while runtime cleanup is still active | One 21.27-second native exit observed; later sessions stalled at instance destruction. Reliability unresolved despite passing exit fixture |
| Giant game screen | Both final composited eyes show changing real game pixels | 60 seconds + yaw/pitch/lean and recenter | Black source, wrong alpha/UV, tracking loss | Real pixels and sustained session observed; complete motion/recenter gate unproven |
| Native controller UI | Actions reach the game import and change native UI | Continue, iDroid, Pause, navigation and back | Stale samples, focus/tracking loss | Basic Touch/Meta simulator controls exercised; other profiles incomplete |
| Existing save and equipped gear | Native Continue/Resume Game, visible saved weapon and ammunition | Load checkpoint, aim, fire, reload | Starting a new game or substituting a synthetic loadout | Existing checkpoint and AM MRS-4 observed; one native shot/reload verified |
| Camera observer | Version-matched bytes, unchanged setter writes/getter pointers, actual camera samples | Title -> gameplay -> right-stick look/ADS | Mismatched signature, synthetic sentinel data | 100 synthetic setter calls and 120 getter calls pass; owner/consumer traces observed live. Separate opt-in render override now exercised; stored native pose remains unchanged |
| Automatic cinematic switching | Authoritative scene state + final composition | Gameplay -> cinematic/video -> gameplay | Same-looking gameplay frame | Not implemented |
| Every cinematic skippable | Native completion + playable next mission state | Each cinematic class and repeated transitions | Press while loading, hold across transitions | Not implemented |
| Native game stereo | Distinct game-eye images, correct projection and culling | 60 seconds, fast six-axis head motion | Duplicate or swapped eyes | Live same-transaction eye pairs and projection-layer output observed; full motion, culling and temporal-effect acceptance unproven |
| First-person visibility | Hands and held gear visible, body excluded; source-bound player head anchor | Crouch, prone, sprint, ADS, traversal | Camera inside head/body, third-person boom | Head anchor, shoulder policy and player exclusion observed; guarded hip-mount suppression removes the tested stowed stock. Complete stance/rig gate remains unproven |
| Complete arm HUD | Actual meaningful changing HUD/iDroid pixels in both eyes | Open, browse, select edges, back, close, reopen; 60 seconds | Blank/stale surface, threshold oscillation | Weapon/status forearm UI and native equipment cards implemented and observed; complete interactive iDroid and spatial feedback remain incomplete |
| Both hands and forearms | Final-eye video of anatomical connection and correct skinning | Neutral, reach limits, opposing head/hand motion | Suppress/detach a limb | Anatomical palm binding, forearm roll, animation restoration and native reload/bolt support observed; full gate unproven with cuff/garment polish and transient motion scrutiny outstanding |
| All weapons | Grip, muzzle and actual impacts agree in final-eye video | Every family, reload/ADS/scope/swap/throw/melee/mounted | Wrong socket, stale pose, failed tracking | AM MRS-4 and WU pistol ready/fire/reload and selection observed; optical input reserved; other modes and impact/obstruction acceptance unproven |
| Physical readiness | Actual headset/controller observation | Full mission session and comfort assessment | Simulator-only or single-eye evidence | Initial Quest 3 combat recording and favorable user report; arm/HUD failures remain, full mission/comfort/stereo acceptance unproven |

Save final composited images for both eyes, then inspect the full time sequence. Record capture cadence, dropped frames, source transaction, audio coverage and runtime state. A black theatre environment is expected for the preview; a black background during promised native gameplay/wrist interaction is a failure.

Core math and state-machine tests are necessary checks, not visual acceptance. In particular, the wrist focus and weapon socket tests use synthetic fixtures, not retail data.
