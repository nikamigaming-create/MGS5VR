# MGS5VR gameplay runner

For local work use `python tools/workspace.py launch-sim`, then
`python tools/workspace.py bot --command continue` and
`python tools/workspace.py bot`. Steam remains running. The current build is
always `build/dev/`; pinned `play/` stays unchanged. Bot scratch runs have
bounded retention. The default command
only observes. Use `workspace.py pin` before citing a run as acceptance evidence,
and `workspace.py stop-sim` for exact-owned test-session cleanup.

To compare the published baseline, stop the owned simulator session, use
`workspace.py launch-sim --slot play`, then pass `--slot play` to each
`workspace.py bot` command. Both commands default to development. This selects
and verifies an existing retained slot; it never changes the published pin.
Body inspection policies are supplied with `bot --command body --suite <file>`;
`--policy` belongs to the mortar command. Size probes can separately exercise
standing/seated height, `transitions` or `rotations`. Traversal probes support
`seated`, `full_body` and bounded `head_offset` comparisons, and restore those
preferences and tracking poses afterward. A stopped movement probe is an
investigation result, not a route pass.

Hospital event movement uses `hospital_navigate_event` with an exact native
sequence, event name, and private manifest containing hashes of the owned scene
and NAV2 tiles. The manifest supplies the authored trigger bounds, destination
floor and witnessed root-to-floor calibration. The executor follows the full
directed A* path, including intermediate doorway and stair nodes, through the
effective VR stick bindings. Native movement locks release input; observed
obstructions expire and cause bounded replanning. NPC topology is a planning
prior, and predicted player clearance remains unknown until actual traversal.
God-mode debug traversal does not certify ordinary-difficulty playability.

The NAV2 reader retains hash-checked empty split tiles as provenance without
inventing nodes. The owned Africa corpus validates `201403240` and `201403242`
NavWorld records under the same bounds, index, flag and portal checks;
`201403241` is admitted only for exact 112-byte empty tiles. Unknown versions,
unexplained empty payloads and unsupported nonempty layouts stop planning.

For the reviewed mortar field fixture, use
`python tools/workspace.py bot --command mortar --policy private/mortar-fire-policy.json --record --seconds 60`.
Run Continue first when starting a new simulator session. The policy identifies
the authored mortar, native NAV map and observed operator position. The bot runs
there, finishes its short approach, stands, enters, triggers once, records the
native shell-load/discharge response, dismounts and releases inputs. Damage,
owner changes and the selected deadline stop the case.
The explicit policy boolean `active_camp_diagnostic` acknowledges an already
detected camp for this firing diagnostic; damage still interrupts the run.
Stationary mortar entry
does not require a ridden-vehicle ID; the actual game keeps those getters empty.
Input delivery alone leaves `shot_proven=false` until the recording is reviewed.
The separate `emplacement` diagnostic still measures exact-ID and aiming-axis
coverage; its vehicle-ID expectation does not establish mortar firing failure.
The simulator launcher forwards to the existing Steam client and temporarily
selects the runtime in the game's own `mgs5vr-runtime.ini`. Cleanup restores its
exact prior bytes or absence; personal edits made during a test are preserved.
Neither Steam nor the global OpenXR runtime is restarted or reconfigured.
See `docs/CURRENT.md`. The explicit raw-runner examples below remain available
for retained acceptance runs; they are not the default local iteration flow.

Run from the repository root. This tool operates the local single-player game
through the Meta XR operator and observes the installed mod's native state. It
does not need the game in the foreground. A game and simulator session must
already exist; `tools/workspace.py launch-sim` is the supported launch entry.

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
  `owner: "customization"` and `owner: "customization_popup"` cover the native
  ACC weapon/helicopter/vehicle selector and its reviewed popup without a Pause
  owner. Both require exact mission, sequence and authored target prerequisites;
  the popup owner additionally requires `popup: true`. They send one sampled
  edge through the effective VR menu stick. The helicopter discard recipe is
  separate from its iDroid Development list and upgrade transactions.
  Pause uses one fast sampled stick edge, avoiding repeats caused by paused Lua
  queries. Full native guards are checked before input and after release.
  `owner: "pause_popup"` additionally requires `native_before.popup: true` and a
  reviewed known popup. A case can verify its sampled axis through `during`;
  this records actual held controls and does not fabricate a held-eye capture.
  Review the final menu selection before Confirm.
- `observe --seconds <number>` samples without gameplay actions.
- A `gameplay.fire_or_cqc` action can explicitly use `include_ready: true`
  for a bounded on-foot firing probe. It resolves the effective grip modifier,
  waits for 200 ms of sampled native weapon readiness, then sends a
  0.03–0.35 second trigger burst. Owner changes, unready weapons and unsupported
  remaps stop before the trigger; cleanup releases both channels. This verifies
  the input sequence. Actual firing still needs ammunition and visual evidence;
  it does not require or certify a magnified scope.
- `move --distance .35` performs a short forward movement through the configured
  stick, reduces the value before the target, releases and checks settling.
  The five-second/one-unit bounds reject stuck movement and scene changes. This
  is a movement primitive, not a demonstrated route or collision planner.
  The standalone move command uses the same .35 braking value and .1-second
  pulses as `walk_local`. On October 20 the former .15 brake stalled at
  0.292 native units; the .35 probe reached 0.533 for a 0.35 target and settled.
  This remains bounded coarse movement, not precision arrival or corridor proof.
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

The route executor imports owned NAV2 data and drives ordinary VR locomotion
over its explicit graph edges. It reads every enemy-definition page before
choosing a posture, tracks native floor/position/life, and replans around actual
blocked edges. NAV2 is an NPC planning prior; actual traversal establishes player
clearance. A precise final approach can select a 0.75–3.5-unit arrival radius
without relaxing floor tolerances. Scene passes do not establish every road or
vehicle route.

`workspace.py combat --camp NAME --seconds 30` explicitly selects bounded
firearm combat. The bot joins authored soldier identities and native life with
a leased observation from the native marker job. That job reads person-marker
capsules and uses the existing signature-verified world visibility query from
the solved weapon muzzle. Unknown/blocked terrain refuses firing. The bot turns
its simulated headset, aims through separate VR grip/aim poses, closes the loop
against the actual native muzzle, and uses effective ready/trigger bindings.
For a particular mission actor, `--target sol_NAME` requires that exact living
native soldier as well as the selected camp, range and geometry; a nearer guard
cannot substitute for it. Rifle corrections move the firing hand and measured
authored support contact together from one publication, preserving both
controllers' separate grip/aim frames. The final clear-ray, solved barrel and
attached-support checks still admit each trigger independently.
Actor incapacitation and ammunition are observed separately; a buddy can affect
attribution. Both final eyes still need visual review. This is not full combat,
body handling, all optics, supply-drop visibility, or campaign acceptance.

Reusable policy and native facts must retain their tested build/world and proof
scope. Dynamic enemy positions and blockers must be refreshed. Internal planning
knowledge never substitutes for the player seeing usable binoculars, markings,
items, prompts and action results. See
[the current delivery plan](../../docs/BOT_AND_SHOWCASE_PLAN.md) for coverage.

`workspace.py autonomy --camp NAME --seconds 600 --record --stealth-loadout
PATH --emplacement PATH` selects the bounded camp policy and an optional
reviewed emplacement. The loadout policy must identify this fixture's wrist
card and actual slot; customized slot numbers are not global gun identities.
The bot runs in clear ground, crouches/crawls near observed guards, equips the
physical binoculars and explicitly marks only after current exact-actor native
acquisition. Its suppressed pistol range is 25 metres. Gun poses preserve both
controller grip/aim pairs and use the rig's authored support contact. Fresh
native barrel, view, visibility and attached support checks gate every trigger.
Moving-target rejection can cause bounded re-aiming before dispatch; uncertain
trigger completion never permits another shot. Damage takes priority over
attack, with a reviewed primary-weapon fallback after detection.

Cover planning probes at most eight connected NAV floor points and four actual
person-marker points on the native marker job. Requests join the observed
player root with a focused, fresh, same-owner native eye; missing/stale results
cannot count as cover. Terrain obstruction ranks approach positions. A short
crouched peek seeks a clear ray toward the selected guard while retaining
obstruction toward others; actual muzzle visibility is checked separately.
The bot returns to mapped cover after an observed outcome. Marker rays do
not prove enemy vision, shadows, AI concealment or body hiding.

Recorded held gun inspections use a checkpoint into continuous NativeEyeAV
source-eye video, avoiding a reproduced blocking compositor RPC. This produces
no still image or final-compositor pass. Neutral checkpoints still capture
both final eyes. Twelve-second recording segments retain source lineage,
cadence repeats/drops and actual game audio. Review those frames before making
visual acceptance claims. A local artifact director is in
`private/snake-director`; it observes evidence and accepts only a fresh pending
commander waypoint with both reviewed eye hashes. Its learning file remains a
candidate until a later action and regression verify it.

The emplacement follow-up requires two native camp-incapacitation snapshots
and no nearby active guard. Registry `gimmickType` (mortar category 5) and native
`type` (mortar object 81) are distinct domains. Exact authored locator, dataset,
lookup status and object ID must match. It tests one mount interaction, four
isolated aim-axis pulses, a sampled native fire status and dismount. Packets and
fire status do not certify barrel motion, a projectile, all emplacements, road
driving or whole-game acceptance.

Offline checks: `python tests/gameplay_bot_tests.py`.
