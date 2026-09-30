# Bot supervision checkpoint - 27 September 2026

Latest installed build and release work are now recorded in
[the release acceptance checkpoint](RELEASE_ACCEPTANCE_2026-09-27.md).
The candidate identities and counts below are retained historical checkpoints.

The runner now executes a reviewed sequence continuously on one input connection.
In the live test, ten field/posture cases passed their native predicates in
28.109 seconds, with no model handoff between cases. This is a bounded proven
sequence, not completion of the full community campaign.

## Why the old loop sat idle

The first Luna watcher could report a waiting state but was not authorized to
submit the next decision. Giving the worker individual decisions still inserted
tens of seconds of model work between routine actions. The fix is an executable
plan: the runner observes and advances known cases; the model reviews unexpected
screens. Models are not guaranteed to react every second.

An unmet supervised predicate is reported after two seconds plus eye-capture
latency. Dependent cases stop, all inputs release, and current left/right eyes
are attached. A decision must match the current observation and image hash;
observations older than 30 seconds are rejected and refreshed. A ten-second
worker-wait event identifies a coordination delay, not a game failure. A completed
finite queue and recorder finalization are also distinct from a stuck game.

`gameplay-bot.py supervise --suite <known-suite.json>` starts the explicitly
selected, guarded suite immediately. Without `--suite`, it waits for a reviewed
decision. `kind=plan` accepts at most 32 unique cases, records every result and
stops on the first unexpected outcome. No blind confirm or toggle retry loop
has been added. `gameplay-watch.py <run> --tail --seconds 10` follows new events;
the worker must inspect existing result/state once before tailing, since a short
run may already have completed.

## Evidence on the current native candidate

DLL SHA-256: `9ea94a4b88e3eebfa0585b59db42ff0c0c4bc95c9c865cb35a261dc6e25c24de`.
The game ran under process-scoped Meta XR Simulator v207. The global OpenXR
registry and personal control/settings files were not changed.

| Run | Observed result | Limit |
| --- | --- | --- |
| `artifacts/bot-20260927/supervised-02` | Ten equipment, commands, VR binocular, pause and posture cases passed in 28.109 s. Normal-map iDroid open and Back close also passed. | Native outcomes; this does not complete visual/headset or retail BINOCLE acceptance. |
| `artifacts/bot-20260927/supervised-03` | Explicitly selected pause suite started 0.797 s after runtime readiness and passed open/close in 4.844 s without any decision-file handoff. | Verifies the immediate-start path for a known guarded suite. |
| `artifacts/bot-20260927/idroid-04` | Six fresh native palm poses; both menu sticks; 16 navigation samples; 0.000 m measured player drift; normal Back exit. | Normal map only. Some panel views clip. Continuous motion and physical headset fit remain open. |
| `artifacts/bot-20260927/idroid-03` | Native palm motion passed; a later null control sample stopped navigation. | This run did not have frozen hands. Its failure remains preserved. |
| `artifacts/bot-20260927/campaign-05` | Tutorial/Help retained a static native palm with `rig_sequence=0` while XR grip poses changed. | Confirmed paused-tutorial rig failure; normal-map success does not erase it. |
| `artifacts/bot-20260927/tutorial-ack-01` | Visible “Exit Tutorial / Suspending the tutorial for now / A Close” acknowledged; gameplay returned. | Suspension, not tutorial completion. The native handset could remain out after its UI closed. |

One missing control publication is now reobserved within a bounded window. No
input is replayed or extended; every observed player position/yaw is still
checked, and two distinct actual stick samples per probe remain mandatory.
Persistent missing input, lost menu ownership or movement fails and releases.

Raw results are retained unchanged. `coverage-links.json` sidecars bind reviewed
case indices and IDs to the exact result SHA-256. The regenerated field-kit
ledger contains all 55 reports, 35 situations, 26 recovered feature families,
96 effective actions and 899 equipment definitions. Definitions are not verified
usable equipment. No report is automatically closed by a native subcase pass.

## Remaining release work

Paused tutorial hand updates, lingering terminal/handset state, iDroid clipping
and continuous pose parity remain open. Mission 1 retail binocular tutorial
state, Mission 6 recovery, NVG and the other community acceptance scenarios
remain in the master queue. Coco's exact two-second demo recovery is not integrated.
The field-kit PDF is a verification edition with controller plates and the issue
register; the complete historical feature showcase and instructional film are
not finished. No new public patch has been published.

The source/tests are uncommitted in the existing working tree. Preserve the
other in-progress changes; this checkpoint is not permission to replace source
files wholesale, rewrite saves or mark incomplete features accepted.

Verification: seven gameplay contract suites passed CTest. The supervision
contracts include immediate preselected execution, interruption before dependent
cases, stale snapshot/image rejection and replay rejection. Watcher contracts
cover missing/partial event files, deduplication and attaching to new events.

Artifacts: `output/pdf/MGS5VR-Field-Kit.pdf` (31-page verification edition,
all 96 effective actions checked present); the searchable ledger remains at
`artifacts/field-guide-20260927/verification/index.html`. The short
`artifacts/field-guide-20260927/supervision-proof/supervision-proof.mp4`
is a native source-eye overview with a marked 0.334-second recorder changeover,
not an uninterrupted final-compositor recording. Its provenance records source
hashes, segment QPC anchors and the limitation from dropped source frames.
