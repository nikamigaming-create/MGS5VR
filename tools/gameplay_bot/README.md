# MGS5VR gameplay runner

For local work use `python tools/workspace.py launch-sim`, then
`python tools/workspace.py bot --command continue` and
`python tools/workspace.py bot`. Steam remains running. The current build is
always `play/`; bot scratch runs have bounded retention. The default command
only observes. Use `workspace.py pin` before citing a run as acceptance evidence,
and `workspace.py stop-sim` for exact-owned test-session cleanup.
See `docs/CURRENT.md`. The explicit raw-runner examples below remain available
for retained acceptance runs; they are not the default local iteration flow.

Run from the repository root. This tool operates the local single-player game
through the Meta XR operator and observes the installed mod's native state. It
does not need the game in the foreground. A game and simulator session must
already exist; `tools/launch-steam-simulator.ps1` is the supported launch helper.

`session` handles the recognized startup prompts, physical Continue rack and
loading acknowledgment, then runs every case in a suite on the same connection.
If already in gameplay it proceeds directly. Use a fresh output directory:

```powershell
python tools/gameplay-bot.py session `
  --game-dir 'D:\SteamLibrary\steamapps\common\MGS_TPP' `
  --proxy 'D:\code\gta-iv\out-openxr\external\meta-xr-operator-standalone-205.1\extracted\meta-xr-operator-standalone-public\windows\meta-xr-operator-mcp-proxy.exe' `
  --suite tools/gameplay_bot/suites/field-context-roundtrip.json `
  --record --output artifacts/bot-session-example-01
```

The six-case field suite covers equipment hold/release, Commands hold/release,
the VR binocular latch/stow, and Pause open/back. It requires handheld menus and
an immersive field camera. Required settings are checked against the effective
bindings before startup or input; the suite does not change personal settings.
It contains no presentation toggle. Its binocular context outcome is not the
retail BINOCLE status required by the tutorial. This is a prepared scenario, not
a claim that every native optics feature is covered. The complete six-case
queue passed on the C431 ergonomic candidate in `ergonomic-field-suite-live-02`.

For one entry point with validated tool paths, use
`tools/run-gameplay-showcase.ps1 -GameDir <game-directory>`. Add `-Launch` only
when normal game/simulator windows are allowed. The helper reuses the running
Steam client and performs no desktop mouse, keyboard or focus operations.
Cold startup waits for the current process's actual native Press Start show
hook; the earlier mission sequence flag does not mean the prompt is ready.
The title prompt receives the configured Menu tap (native START); autosave and
loading acknowledgments use A. Sending A at the title splash can leave it idle
even when the game consumes that button.

The separate `menu-roundtrip.json` includes iDroid exit, an investigation case:
an extra native tutorial acknowledgment can prevent its expected return to
gameplay. The runner reports that failure rather than pressing through an
unrecognized popup. Prefer a suite of observed field actions for teaching takes.

- `continue` reaches gameplay without running a suite.
- `run --suite <file>` starts a suite in the current scene.
- `menu_navigate` defaults to iDroid. An explicitly selected `owner: "pause"`
  requires nonempty `native_before` scene guards and fresh native Pause ownership,
  with iDroid/title/loading absent and no popup. It uses the effective menu stick
  and stops/releases if ownership or prerequisites change. This does not select
  choices or certify the identity of every nested page.
- `observe --seconds <number>` samples without gameplay actions.
- `move --distance .35` performs a short forward movement through the configured
  stick, reduces the value before the target, releases and checks settling.
  The five-second/one-unit bounds reject stuck movement and scene changes. This
  is a movement primitive, not a demonstrated route or collision planner.
  Braking is currently verified in simulated dynamics only: the old live probes
  overshot, and the new .30/.15 values still need native deadzone calibration.
- `--record` captures rotating native source-eye video and process audio. This
  is **not** the final composited headset image. `tools/record-simulator.py`
  supplies separate final-eye takes; its measured cadence is retained.

Recording also saves individually timed headset and controller grip/aim pose
readbacks before/after captures and after scripted pose
changes. Positions and quaternions can drive the field-guide diagrams. These
queries are not an atomic frame snapshot: close-up gameplay must use the same
real take, and unavailable orientation must never be invented. Short taps do
not perform extra pose queries while held. All held actions avoid compositor
and pose RPCs: those share the serialized input transport and can prevent an
explicit release from reaching the operator. No desktop capture or input is used.

Cases name effective configurable actions, with explicit native preconditions
and outcomes. They wait for observed results, not a fixed 20-second interval.
Unknown startup popups and failed actions stop dependent work. Recorded
`observed_pass` means the native outcome happened; final-eye review and headset
comfort remain separate judgments.

A brief missing control publication after capture is reacquired for up to two
seconds before dispatch. The 250 ms freshness requirement remains, and all
context/prerequisite checks use the new observation. This waits for data only;
it never retries an input whose effect is unknown.

For a held action, a case can specify `during` as well as `before` and `after`.
The final action is observed while held, then explicitly released before
checking `after`. A timed `held_visual_checkpoint` joins the observation to
the continuous native source-eye recording; it is not a final-compositor still.
Without recording, the held result is state-only and says so. Final-eye stills
are captured with neutral inputs before/after. This supports a real enter/exit round trip even when
the neutral before/after predicates are identical. The `during` predicate must
be initially false and needs both a sampled physical input and an observed
outcome. Taps cannot be stretched to wait for a held outcome.

Nonzero inputs arm an operator-side expiry as well as the normal explicit
release. Long gestures get at most five seconds to establish their sampled
outcome; short taps expire at their requested duration. Held face/axis channels
release before idle cleanup and chord modifiers. A two-second input RPC timeout
marks that transport unusable instead of queuing fifteen more requests behind
unknown work. Failed releases retain their channel bookkeeping and block
compositor capture. Timers bound a transport failure; they do not turn it into
a successful action or authorize an automatic retry.

Long gestures use the XR resolver's sampled timestamps. Missing audit snapshots
do not erase the first held timestamp; a new held sample must arrive before any
elapsed time is credited, and a fresh released sample breaks the hold. This is
physical-input evidence, not proof of uninterrupted native XInput consumption.

`system.toggle_vr` requires an explicit `target_presentation` of `immersive` or
`quad`. Immersive recovery is admitted only from the explicit manual-quad state.
An inactive camera that is pending or awaiting the player already has an
immersive request; toggling it would switch VR off. Wait for the camera or report
the missing publication rather than issuing another mode chord. Shutdown can
also stop publication and is not evidence of an independent camera defect.

Each output contains installed build/config identity, effective bindings, an
event timeline, per-case results, remaining case IDs and real composited-eye
stills. Existing output directories are never reused. A case failure releases
inputs. No automatic retry of an ambiguous action is performed.

An independent case can continue after a bounded outcome failure only when the
suite opts into `continue_after_outcome_failure`, its entry predicates still
match, and release/capture cleanup succeeded. `depends_on` names earlier cases;
a failed dependency skips that case. Ambiguous dispatch or release still stops
the queue. A matching entry predicate does not prove nothing else changed.

The A* graph utility currently supports supplied, validated edges. Native
collision-query integration, general route execution, all weapons/modes, saved
queue resumption and the complete showcase are still being implemented. See
[the current delivery plan](../../docs/BOT_AND_SHOWCASE_PLAN.md) for coverage.

Offline checks: `python tests/gameplay_bot_tests.py`.
