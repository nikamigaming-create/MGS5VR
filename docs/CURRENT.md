# Current work and test build

The release goal is playable TPP from start to finish, with reliable physical
weapons, optics and binoculars, the existing left-arm weapon/status display and
left-arm HUD popups, and the right-hand iDroid. Pause uses a spatial stereo
panel. Floating firearm aiming overlays stay off. No desktop/input automation
or Steam shutdown is needed by the local workflow.

## Current result: October 3

### Unreleased iDroid regression investigation

The latest built local candidate `7a5b00774fd2` passed all **44 CTest
groups and installer transactions**, including **120 real D3D11 mailbox
checks**. It includes paired grip/aim retention, right-hand handheld iDroid
defaults, native opacity and modular-arm boundary repairs, recording lineage,
completion fences and separate layer-preparation and `xrEndFrame` timing.
The new typed routing recognizes 20 outgoing device layouts, excludes the
shared Mission Orders scene, and uses an exact post-native-A8 handset pose.
A bounded native visibility diagnostic can observe opacity and arm-group state;
it does not establish GPU visibility or repair acceptance. These changes have
code/test coverage but still need their native transition replay.
This remains an unreleased investigation candidate;
the published October 2 release below is a separate build.

The first `7a5b00774fd2` launch timed out before the first game image. A subsequent
baseline-pacing launch of the same DLL reached native Present and OpenXR at
2520 x 2640 per eye; both Autosave eyes were reviewed and one acknowledgment
was sampled. The initial startup failure remains unexplained. Display pacing
starts after first-image capture, so this is not evidence that the display cap
caused the failure. A later debugger-cleanup defect was separately identified
and cleared; it did not explain the initial hang. Steam was retained.

On prior candidate `f694be5341fb`, retained `20261003T054823809620Z` reached
Afghanistan Mission 10020, with both
arrival eyes reviewed. Retained `20261003T054944466769Z` passed seven ordinary
iDroid state cases and schema 2 recording validation. Reviewed accepted source
samples show a full-size attached handset at opening. Immediate reopening
still loses both arms for one recorded source image, `95403`; the neighboring
accepted images retain them. Exact matching body and attachment publications
rule out a stale attachment argument on that image. The remaining visibility
cause needs a source-bound native draw/opacity observation.

Typed observations identify 20 Map/device layouts routed to the left arm at
first-closed sources `95146` and `95646`, separately from four ordinary HUD
layouts and a shared Mission Orders scene. Neither exact closing pair was
copied by the recorder. The built `7a5b00774fd2` changes route only verified
device-owned layouts, carry a strictly matching native handset pose, and
suppress those outgoing device pixels when no current pose exists. Native
replay remains pending; continuous closing animation is not accepted.
Authored native transforms remain untouched.

The 20.533-second recording has 570 accepted copies, 47 scheduled losses and
46 explicit CFR repeat fills; it is not gapless evidence. The final capture-off
stowed field window spans 295.233 seconds: 87.67 XR submissions/s and 54.98
fresh pairs/s. Exact `xrEndFrame` reaches 811.755 ms while layer preparation
stays below 0.182 ms. This establishes an API submission stall, not its runtime
or driver cause. The reopen visibility defect is not concurrent with the logged
large stall. A matched process-only display-pacing comparison is next;
freshness limits, recommended per-eye resolution and eye FOV remain intact.

The six-cycle, 180-second motion suite on `5a7bae2a2864` failed in cycle six
after five complete cycles. No current sustained no-drops pass supersedes it.

The user additionally reports that entering **any ACC Development window**
freezes selection, navigation and Back. The `4df840eda253` replay confirms
Weapons/Items Down/Up and eventual exit through three short Back edges in
handheld mode, but its whole recording run failed the disk-reserve check.
The `f694be5341fb` spatial-mode comparison reached the Development selection;
its next reopening failed because physical input was never sampled. On `7a5b00774fd2`,
the spatial-mode Down probe had one observed edge but a measured hold of 0 ms;
it does not establish a usable native navigation hold or reproduce a freeze.
Retained `20261003T071122059279Z` then reproduced a narrower exit failure:
Up responded and wrapped to the bottom row, but three separate 120 ms Back
commands were sampled as B and followed by neutral input while both final
eyes still showed the Weapons grid. Retail XInput logs independently confirm
all three deliveries and releases. The run ended with attention required and
clean input/session cleanup. This establishes a spatial-mode Back defect in
Weapons/Items, not a complete navigation freeze. A handheld-mode comparison
on the same DLL is in progress; the other Development windows remain open.
This remains a release blocker, with no ambiguous action replay or development
item purchase.

Supply-drop cues, intermittent weapon selection and lower-priority cutscene
reports are recorded separately. The exact fight and selection symptom are
unknown. Existing replacement cues do not identify supplies; no marker fix or
Quest-specific cause is claimed without a native reproduction.

The release gate remains blocked for the linked hand/arm and iDroid transition
features, performance and the ACC Development report. No full-game,
continuous-feature or physical-headset acceptance is claimed. The
[regression catalog and coverage report](REGRESSION_COVERAGE.md) distinguish
implemented behavior, unit coverage, bounded observations and missing sustained
checks. Local investigation sessions release test controls and close only their
owned game/runtime during cleanup; Steam is retained.

### October 2 release source

The October 2 experimental release contains the verified Map marker/footer
binding fixes, native prompt route corrections, support-hand contact repair,
bound-touch input ownership and bounded test recovery. Release notes are in
`docs/RELEASE_2026-10-02.md`; the package's `BUILD.json`, `RELEASE.json` and
`SOURCE_REVISION.txt` identify its exact committed source and checked files.
The final native candidate `600ec71d1b2a` passed all 41 CTest groups and
installer transactions. Local work continues in the fixed `play/` tree.

Retained `20261002T145255433589Z` passed Map opening, native-button mode on/off,
Back, immediate reopening and final Back on restored personal settings.
All twelve outcome eyes were reviewed. The footer now shows the effective
Y, left-click, right-click and LT/RT bindings, returns to native graphics in
native-button mode, and restores VR captions afterward. The actual native
Map uses mode 82/help IDs 35--38; earlier mode 46/47 admission rejected every
row. Admission now requires the observed owner, node, row and mode identity.
Long remaps, other languages and physical-headset readability remain open.

Retained `20261002T145122033446Z` reached Mission 10020 through physical
Continue on that candidate; both final arrival eyes were reviewed. Earlier
`20261002T144635365551Z` on `e7091ee0d2a5` passed standing, crouched, prone,
crouched and standing with sampled VR A input and neutral cleanup. A 120 ms
tap crouches; the separately tested 300 ms hold went prone. This confirms
the scoped existing stance flow, not a reproduction or closure of every
reported hardware crouch issue.

Retained `20261002T135259921572Z` passed the reviewed autosave action and reached
the native title sequence with clean supervisor exit. Retained
`20261002T135547048922Z` reached Mission 10020 through physical Continue, with
both final arrival eyes reviewed. These are startup/arrival results only.

Retained `20261002T144514844155Z` on `e7091ee0d2a5` passes physical binocular
equip, fixed-pose 2x-to-4x-to-2x zoom and stow. All eight final-eye images were
reviewed: magnification changes inside the physical ocular while housing,
palm contact and the outside-world view stay fixed; stow removes the housing.
The scoped manifest is `artifacts/bot/acceptance/binocular-fixed-pose-e709.json`.
Enemy acquisition, explicit marking, continuous motion and headset acceptance
were not tested. Native material insertion remains zero despite observed
three-target binds, so the lighting repair is not accepted. The experiment
now defaults off, including older INIs without the setting; the existing
physical housing rendering remains available. Explicit opt-in preserves the
same strict source, geometry and material checks.

Retained `20261002T142717882392Z` on `77ade9c8950d` passed ordinary iDroid
opening, two hand poses, stow, immediate reopening and final stow; all twelve
outcome eyes were reviewed. The optional native effect writer now admits the
observed complete three-particle batch with transactional transform/bounds
checks. Native writes were observed, but exact cone endpoint alignment,
forced tutorial stow/reopen and headset acceptance remain open. Cone fitting
and its diagnostics remain off by default. A separate custom-fit replay was
reviewed but its scratch pixels expired before promotion; it is excluded from
retained visual acceptance.

The bot waits for real pixels after a sampled startup action only while the
known boot owner and released controls remain verified. Exact read-only title
observations may use one eight-second response deadline on the same request;
actions retain the ordinary deadline and are never replayed. The focused
supervisor and behavior suites pass 27 and 198 tests. The native title replay
exercised that admission; its responses were fast, so the delayed-response
branch has unit coverage rather than a native timeout-recovery pass.

The next playability work remains binocular environment lighting, forced
tutorial iDroid lifecycle, cover/turret/traversal defects, all-weapon sight,
muzzle, hit and reload alignment, and continuous physical-headset acceptance.
These scoped simulator passes do not certify whole-game completion. Personal
settings are restored after tests, owned test sessions are closed, and Steam
is retained.

### Earlier investigation: October 2 overnight

The installed local candidate `83fce4fc8ebc` passes all 41 CTest groups and
installer transactions. The exact owned canonical native material shader was
imported locally and verified; source archives and saves were untouched. This
candidate adds guarded binocular native material insertion, bounded iDroid
Light discovery for already-loaded effects and one bounded recovery for an
unsampled axis pulse. Its native lighting and projection acceptance is pending.

The previous local candidate `0143f88c264c` passed all 41 CTest groups and
the installer transactions. `play/` and the recorded game installation match;
personal controls remain unchanged. Its current primary firearm support attachment passed
five scoped simulator cases in retained `20261002T095140317683Z`: exact contact,
rigid translation left and right, distant-hand rejection and release to one
hand. All ten during-checkpoint final eyes were reviewed and the scoped
manifest is `artifacts/bot/acceptance/field-smg-support-0143.json`. Continuous
motion, finger articulation, reload, the reported compact silent long gun,
muzzle/hit alignment and physical-headset acceptance remain open.
Runtime resource `27873` does not identify a weapon class/model/definition.
The native left-arm display separately showed AM MRS-4, DMG31/175; legacy
`smg` case/file names are retained trace links, not a verified SMG classification.

The same candidate reproduced the binocular housing's environment-lighting
mismatch. A native GBuffer material insertion is being implemented; no lighting
fix or projection-cone fit is accepted yet. The early-field movement probe also
failed in `field-mg-approach-06`: a 654 ms simulator `xrEndFrame` blocked across
the entire independently expiring 250 ms stick pulse. Controls stayed neutral
and the actor did not move. The runner must distinguish that unsampled pulse
from consumed input while keeping freshness, expiry and displacement limits.
Retained `field-mg-approach-07` then passed the short bootstrap with two sampled
250 ms pulses and the native NAV2 route to the coarse turret goal, life6000.
No swallowed-pulse retry occurred in that successful replay. Enemy fire later
interrupted stationary turret review; no mount input was sent and the owned
test session was closed. Steam remains running. Turret, cover and whole-game
gates remain open.

On `0143f88c264c`, ordinary field Pause open/Back/immediate reopen/second Back
passed in retained `20261002T104509634657Z`, with all eight final eyes reviewed.
`20261002T105356113073Z` separately reached Options and Control Settings in
both eyes. Three rapid sampled Back presses returned only to ordinary Pause;
later maintained cleanup returned the live field. The nested Back case is
failed and retained for timing investigation, not a proven menu lock or
all-Options acceptance. Future Back steps require released stabilization and
fresh page review between transitions.

The previous integrated candidate `369713f9251c` passed all 41 CTest groups and
the packaged installer transactions. `play/` and the recorded game installation
match; personal controls remain unchanged. This build adds the corrected
native R3 destinations for Items Use and Commands Confirm, the native vehicle
L1 destination, and ownership recovery for every effectively bound capacitive
touch sensor. Hardware touch dispatch and these additional prompt families
still require native acceptance.

Fresh popup diagnostics now distinguish the current numeric/StringId identity
from the signed last completed response. Unknown readiness remains unknown;
the completed response is never treated as the highlighted choice. Input-tag
traces have a separate bounded budget so typewriter subtitles cannot consume
the menu audit. These are investigation tools, not menu pass results.

The candidate also observes the popup's current selection separately from its
last response. A typed native update callback supplies a bounded owner lease;
the reader requires a unique fresh owner, matching dialog/backlink, valid
button count/index and stable repeated reads. Unknown choice remains null.
The bot now honors popup prerequisites on cursor navigation and rejects a
changed dialog before dispatch. Native choice-reader acceptance is pending.

Retained `20261002T073555368643Z` reproduced the helicopter customization
selector, Back dialog, physical selection of Discard, cabin return and immediate
iDroid reopening. Both final eyes were reviewed. Daily Bonus and Skulls Attack
information cards were then acknowledged separately and the physical map
returned. Mother Base Help opened with the effective Menu hold. A reused Confirm
close recipe failed and left Help visible, so the whole investigation is
**failed**; the private close recipe now follows its actual Hold Menu footer and
still requires a fresh native replay. The maintained failure cleanup closed the
owned game/runtime, retained Steam and left no test menu open. No upgrade
purchase/completion or whole-menu certification follows from this selector run.

The early field fixture exposed a separate startup Online Information branch.
Automatic Continue timed out in `20261002T081345580078Z`; both eyes showed the
unhandled information page. Supervised `20261002T081959231430Z` separately
reviewed and exercised Next, Close and the return to the title. This is retained
startup investigation, not field or full-game acceptance.

On public DLL `959f9ac763fb`, retained run `20261002T050047479366Z` passed
ordinary field Pause open, Back, immediate reopen and second Back. Both final
eyes were reviewed at each outcome; the spatial panel and live field return
were visible. Tutorial Resume/Skip, nested Options and cutscene Pause remain
separate cases.

Retained investigation `20261002T051649249386Z` reproduced the binocular
housing's environment-light mismatch at sunset and night. Its physical ocular
reached about 4 cm, then the 3 cm safety limit; stowing restored gameplay.
An overstrict transient grip-position predicate failed at 4 cm. The firearm
inspection then failed to establish an active native firearm, so the overall
run is **failed**, not acceptance. Inputs were released and the owned game
session closed. Original settings and six original save files were restored
before the next isolated ACC replay. Do not infer weapon readiness, continuous
stability or headset comfort from the close-up stills.

The ongoing scope remains every known community defect and the user's reports:
menu and upgrade recovery; accurate binding prompts; physical optics and hit
alignment; binocular lighting/contact; iDroid emitter/screen fit and forced
stow; weapon/support/reload contact; cover/Y camera continuity; turret pitch and
reticles; traversal; left-arm HUD readability and temporal stereo stability.
An automated build pass does not close this scope.

The completed-login notice was reproduced in both final eyes in retained
`20261002T055911070140Z`: startup remained at `Seq_Demo_LogInKonamiServer`,
native popup template 1 / StringId `0x4dc3cae5b486`, with **Logged in to server**
visible. The runner now acknowledges only that verified active identity, once,
and rechecks it before dispatch. Retained `20261002T060448782406Z` exercised
the corrected branch and physical Continue to the ACC; neutral inputs and hand
poses were restored. Progress, unknown results and closed dialogs receive no
generic confirmation.

Retained `20261002T060559660205Z` completed a bounded ACC investigation with
clean menu/input/pose cleanup. Both eyes were reviewed for actual Development
Weapons/Items, Buddy Equipment, Helicopter and Security Devices lists and their
ordinary Back paths. One reused Weapons/Items recipe instead reached Helicopter,
and a presumed Customize route reached Resources; those labels are not accepted
as their intended destinations. Helicopter selector Cancel/Discard, upgrade
completion, all other modes and full-game acceptance remain open.

The test-loop continuation adds bounded proxy stderr diagnostics and one
read-only reconnect after an unheld capture timeout. Its original partial pair
is retained and invalidated; fresh native observations and a completely new pair
are required. Input RPCs are never replayed. Actual recovery still needs a native
timeout replay. The supervisor's review window is now 60 seconds for two-eye
review/tool latency; native control freshness and presentation gates are retained.
New opt-in iDroid Light and weapon-component diagnostics identify existing
native owners/resources without changing geometry, bones or control behavior.
The Light cone's separate player owner and transform repair remain unresolved.

Retained `20261002T065631090452Z` on candidate `fd35a92ed161` exercised
physical Continue after the autosave and exact completed-login notices and
reached the ACC with restored hand poses. Both arrival eyes were reviewed.
Retained investigation `20261002T065751143255Z` reached the actual Development
category page, returned to Mother Base, selected the reviewed Customize row
**above** Development, and entered the actual helicopter selector. Its Back
dialog had Cancel selected; physical Confirm returned to the same selector,
with both final eyes reviewed. The overall investigation is **failed** because
the bounded review deadline expired while that selector remained open and
cleanup refused an unreviewed exit. The exact owned game/simulator session
was then closed; Steam remained running. This is scoped Cancel-path evidence,
not full upgrade or successful roundtrip acceptance. Discard and immediate
reopening still require their own completed replay.

The noncompact two-hand guidance now follows the acquired native support-palm
contact rather than the muzzle axis. A measured exact-contact counterexample
previously swung the primary palm 18.4 degrees and moved the authored contact
10.1 cm. Eight geometry checks now preserve exact contact, follow moved contact
and retain rigid-frame invariance. The compact FAKEL branch, muzzle socket and
shot axis are unchanged. Native rifle motion and physical grip comfort are open.

The optional binocular lighting probe identifies exact native pixel shaders,
reads their bounded constant buffers asynchronously without a GPU wait, and
requires the accepted stereo source plus matching native camera matrices before
reporting a coherent frame. It changes no rendering. The housing lighting fix
still needs these actual head-pass values; a diagnostic contract pass is not
visual acceptance. The first ordinary iDroid replay installed its Light probe
but produced no graph registration. Bounded hook diagnostics now distinguish
the missing loader path from rejected identities; no projection fix is claimed.

### User request status

The release is **not accepted from start to finish**. Keep this list separate
from build passes and from older scene evidence.

The current local work adds a shared live 3D settings editor and revises the
iDroid holding frame. All 55 VR adjustments have a fitting view; hand/device
position and rotation, screen width/distance/origin/rotation, left-arm HUD,
optics and spatial menus update immediately in the reference preview. Resetting
a fit preserves bindings and interaction modes. See VR_FIT_SETTINGS.md.

The local fit build, identified in `play/BUILD.json`, uses controller grip position with the same-frame
pointing orientation, retaining the native right-wrist CNP and cupped fingers.
The hologram now rises from a lower-edge anchor above the projector. The first
anatomical-palm attempt made it edge-on in game and was rejected. The revised
default and configured-fit runs each pass seven native cases, including
distinct grip/pointing bases, inspection and ordinary immediate reopening.
The retail light cone still fails to retarget to every nonzero screen
offset/rotation. That defect, forced tutorial stow and physical ergonomic
acceptance remain open. Public package identity is recorded in RELEASE.json;
the local candidate and historical runs keep their own exact build identities.
The editor passed 38 headless fixture checks, including complete save/reload,
discard, orbit/zoom, accurate fractional text scale and reset preserving
handheld mode. The retained controller guide still resolves 98 action locations
across eight contexts. The focused bot contracts now have 138 checks.

Inline VR prompts use the effective personal bindings and shared native action
markup, keeping translated captions and unidentified icons. Compact `LT/RT`
fits Map's bottom-row slot in both eyes. Five scoped Map cases passed in retained
`20261001T225420725862Z`, including default Confirm dispatch and native-button
mode on/off. Its stock A graphic remained visible, so prompt replacement is not
accepted. The failed group/widget hiding changes are removed; the current
direct Confirm-image build `822a8077d38a` now passes seven scoped default cases
in retained `20261002T003629964618Z`, with both-eye review of default prompts,
native-mode stock-image restoration and steady reopening. Remapped run
`20261002T004055181967Z` passes five native cases: both eyes show `[X + L GRIP]`
with no stock A, and that physical chord reaches native Confirm. Its initial
reopening pair still spans the native animation. Controls were restored exactly.
The reviewed Map scope is recorded under `artifacts/bot/acceptance/`; other
prompt families and physical headset acceptance remain open. Map captions now
default on; the broader prompt adapter remains opt-in. The expanded footer
is still pending the native repair described above. See VR_CONTROL_PROMPTS.md.

Coco's `scope_stabilizer.patch` filters native ocular drift relative to
the authoritative aiming hand, with the objective sharing the same rigid
correction. **Steady scope glass** defaults on and remains separate from the
older optional hand/sight stabilization. Synthetic jitter/reset checks pass;
this is not physical headset acceptance. Coco's world-brightness bug was
reproduced in native Mission 10020: moving only the physical binocular changed
the head adaptation by 0.30 in its retail scalar. The repair gives the lens
separate registered CPU luminance textures, GPU readbacks and adaptation
history, restoring the head owner immediately afterward. Cloning only the GPU
readbacks was insufficient because their CPU destination remained shared.

The matched no-lens/shared/isolated field run `20261002T023059054018Z` and cold
default-on replay `20261002T024344212455Z` each pass 16 scoped samples with
both-eye review. In the latter, head means are -13.3062 without the extra lens
and -13.3091 with isolation; the lens still adapts independently and head
motion still changes ordinary exposure. These are native scalars, not claimed
EV units. This repair is now **on by default** in `play/` on supported TPP;
Ground Zeroes stays untouched. Firearm-specific motion, continuous flicker,
other lighting scenes and physical headset acceptance remain open. See
WEAPON_SCOPES.md and `artifacts/bot/acceptance/optic-exposure.json`.

The local weapon fit adds a separate **One-hand weapon tilt**, default -30
degrees, applied through the common native firing wrist. Gun, hand, physical
sight and muzzle retain one owner. Binoculars, iDroid, menu pointers and the
left-arm display do not receive that trim. Older personal INIs with a nonzero
general right-hand pitch receive zero additional automatic trim; explicit
firearm-pitch values remain authoritative. See VR_FIT_SETTINGS.md.

The current-loadout inspection covers FAKEL (SLEEP), AM MRS-71 and URAGAN-5
AIR-S. Both scoped guns publish a barrel/optic axis at the requested -30
degrees in the stationary one-hand case. This does not certify impacts,
reloads, support contact, weapon transitions or physical headset comfort.
The latest retained run `artifacts/bot/runs/20260930T215049265217Z` on local DLL
`5761c47a74a0` passes 19 scoped cases: the three one-hand loadout inspections,
support acquisition on both long guns, seven iDroid fit/ordinary-reopen cases,
and seven helicopter-list navigation/Back cases. Both-eye support contact was
reviewed, but the reported FAKEL grip feel and every reload/transition remain
open; acquisition alone does not certify a perfect grip.
The owned equipment worklist contains 418 firearm definitions across 83 model
entries, including eligibility and variant questions; those are a coverage
queue, not 418 passing weapon tests. See WEAPON_FIT_AUDIT.md.

The reproduced completed-FOB ACC **Choose a Rival** lock-up is repaired.
Its native guide disabled all 78 menu entries and retained its own pause after
Help dismissal. Recovery restores each captured prior disabled bit, preserves
independent restrictions and releases the exact guide pause handle, only in the
completed ACC/main-game context. ACC root Back completes the closure already
requested by the native menu. It does not cancel unfinished tutorials or edit
campaign progression.

Candidate DLL `d893eb437a06`, retained run `20261001T181154160585Z`, reached the
actual Missions, Map, Mother Base, Development and Support Helicopter Armament
pages through physical VR bindings. Eleven scoped cases passed: three Back
levels restored the live cabin camera/control owner, then physical iDroid
reopened with Development still eligible. Both eyes of the leaf, cabin and
reopening were reviewed. The later Daily Bonus test rejected its own already-
satisfied outcome before input; the overall run remains partial.
A second cold boot repeated Help dismissal and restored eligibility. Its
12-sample Back test exceeded the two-second review deadline despite a returned
live cabin. The maintained `acc-completed-rival-roundtrip.json` uses three
stable samples with the same camera/control predicates, matching cleanup.
Retained `20261001T183825927585Z` proved physical Confirm dismisses
Daily Bonus and reveals the separate Skulls Attack informational card.
That card received no stale confirmation. See ACC_MENU_REGRESSION.md.

Final DLL `b6fd45533f6f` passed all four maintained completed-Rival cases on
another cold boot in retained `20261001T190258541444Z`. Both final-eye pairs
show the actual Help dismissal, live cabin return and reopened iDroid. Static
captures establish those page/ownership results; they do not establish
continuous flicker-free or physical-headset acceptance.

The current default-exposure build `431d6cb9a5ad` repeats all four maintained
Rival cases on another cold boot in retained `20261002T025907840443Z`.
Both eyes of the Help dismissal, cabin return and immediate reopening were
reviewed. Its test launcher uses the existing Steam client and a temporary
game-local runtime configuration; cleanup restores the exact prior bytes or
the original absence, preserving any concurrent personal edit.

Community Development and stair reports supplied October 1 have unknown build
identity; the user says they may predate the local fixes. Both attached videos
show metal stairs/catwalk movement, not Development. Keep the menu report and
traversal report separate and exercise the current candidate in ACC and on foot.
Retained warm run `20261001T191943458398Z` missed a 120 ms iDroid tap; the
300 ms tap in `20261001T192821425533Z` opened the same native ACC correctly.
That run also exposed bot cursor overshoot: two sampled holds moved Rewards
to Staff Management instead of Development. Cursor steps now use one fresh
sampled edge; an iDroid scroll hold must be explicitly selected. The longer
default iDroid tap remains below the effective personal hold boundary.
Native replay passed on cold `20261001T193945164460Z` and warm
`20261001T194125435098Z`: the default tap opened and two sampled edges reached
Development exactly. Player bindings are unchanged.

Warm ACC runs reviewed all four Development categories. The weapon cost/time
confirmation canceled successfully. Security Devices returned visually to its
parent in `20261001T195300890687Z`, but a later neutral-input acknowledgment
timed out; that case stays failed. Fresh `20261001T200807859685Z` repeated the
Security entry/Back successfully and dismissed both the Daily Bonus and distinct
Skulls Attack information cards through reviewed physical Confirm.
That run also purchased the eligible AM D114 LA development: native GMP fell
by exactly 1,633,120, Common Metal by 31,300 and Minor Metal by 29,800, matching
the reviewed confirmation. Completion is pending its native development timer;
the developed count stayed 453. Cleanup restored the live cabin camera, neutral
controls and saved hand poses. Nineteen scoped action outcomes passed; these
are not nineteen complete menu families or stereo/flicker acceptance.

The completed ACC fixture has all 19 native helicopter development definitions
already completed. The reviewed body grades and Speaker Confirm are no-ops,
not successful new upgrades. `tools/development-state.lua` reads actual
completion, eligibility, grades and resources for selecting a useful fixture.
Eligibility alone also includes already-completed items. The earlier paired-save
fixture reached Mission 10020, where Mother Base's reviewed page was empty.
That early field state does not establish usable on-foot Development. Retained
`20261001T213743341533Z` passed physical iDroid open, root Back and immediate
reopening. Cleanup returned the live field camera, neutral input and saved hand
poses. The bot's transient null-input-audit cleanup failure is repaired; the
freshness and stable-playable-state requirements remain in force.

The distinct ACC helicopter customization selector now has a retained native
entry/Back/discard/live-cabin/immediate-iDroid-reopen pass in
`20261002T040239079392Z` on DLL `431d6cb9a5ad`. Ten scoped action outcomes pass;
both eyes of the selector, cancellation dialog, cabin and reopening were reviewed.
Back opens a cancellation dialog with Cancel selected; it does not immediately
return to the cabin. The corrected bot supports the exact selector and its
popup through the effective VR menu stick. The reopening stills span a queued
Daily Bonus card, so they do not certify a stable common menu image or continuous
refresh. The original saves and personal controls were restored; the temporary
runtime configuration was removed and Steam remained running.

The community helicopter-upgrade report remains open beyond these scoped paths:
helicopter purchase/result, locked and unavailable grades, selector part changes,
unfinished guides and physical headset use need separate acceptance.
The paired-save fixture tool supports verified
campaign/personal snapshots and six-file rollback while Steam remains running,
with both cloud mirrors required to be disabled. The private
`acc-completed-fob-baseline` reproduces this completed-guide flow; it does not
make every upgrade eligible or certify purchase completion.

The all-menu VR queue now keeps menu families, native game states and handheld,
left-arm/spatial presentations separate. `python tools/workspace.py coverage --menus`
refreshes one fixed review view. Page/choice discovery, guarded nested recipes,
progression-changing isolated fixtures and physical-headset acceptance remain
open. See VR_MENU_COVERAGE.md; a matrix count is not the number of game screens.

The map/state foundation now has a native-location-selected navigation atlas and
a directed state-transition planner. The current owned manifest still covers
only 40 Afghanistan tiles; the other declared worlds remain unimported. The
factual registry and declared state/transition obligations are viewable with
`python tools/workspace.py model`. Four coarse control-owner probes have guarded
recipes. The authored inventory adds 35 named states from the owned ACC/common
helicopter and Mission 6 sources, with literal target references retained as
blocked obligations. Exact menu pages, native roles, sequence conditions and
VR recipes remain unresolved. See GAME_STATE_MODEL.md. This is infrastructure, not full-game
acceptance, and its counts are not a percentage of the game.
An explicitly selected guarded transition can now run through
`tools/workspace.py bot --command state --transition probe.idroid.open`;
the state planner compiles a route with a neutral return, and the existing
native VR runner stops dependent steps on the first failure. Pause still
requires an explicitly named Pause transition.

The resumed native session reaches the ACC (mission 40010) by the real Pause
Return to ACC confirmation and subsequent physical Continue. Steam stays running.
The state-planner route command itself has not been replayed in game. The native
Pause navigation/return result and ACC observations do not certify every graph
transition, menu page or mission.

The live ACC also exposed a rendering bug: it drew the title-only cassette rack
with Continue/Options/Delete/ Quit choices during ordinary cabin play. That
extra render branch is removed; the real title rack and physical Continue still
work. Both final eyes were reviewed at the title and in the native ACC cabin.

Earlier completed-guide menu, held-Back and stale-camera failures remain in
ACC_MENU_REGRESSION.md. Cleanup requires the live camera and rig input to return;
a cleared terminal flag alone cannot pass. Unknown native Help/popup owners
receive no ordinary navigation or blind confirmation. Failed owned sessions
close with Steam retained.

Startup testing also exposed a popup-admission race and a login result stall.
A popup that closes during capture now returns to observation only when no
input was dispatched. Unknown login dialogs receive no blind confirmation:
two cold boots progressed naturally, while another stopped on Logged in to
server. That result identity and reliable unattended cold startup remain open.

The live Pause navigation run reproduced repeat overshoot: three sampled holds
selected Options. The corrected Pause path uses a single fast sampled edge,
with full Lua prerequisites checked before input and after release; three edges
now select the actual Return to ACC row in both eyes. iDroid's existing sampled
hold is preserved. A separately selected `pause_popup` owner requires an explicit
popup prerequisite and visual review before selecting any confirmation.
The resumed supervisor also failed on a Windows status-file replacement conflict.
Status writes now retry only the same atomic file transaction for at most 500 ms;
the previous complete document remains readable. A real Windows reader-handle
test passes. These repair scoped test-loop defects; the ACC upgrade report remains open.

The ACC exit trace confirms that helicopter customization uses the native
`Customize_Abort`/End path and `CustomizeSelector` pad mask; held-Back iDroid
recovery cannot close that selector. The scenario query now reports the native
customization kind and our separate player-pad exclusion. A guarded, unrun
`acc-helicopter-customization-back.json` probe sends one ordinary VR Back only
on an already-open helicopter selector, rechecks its native identity immediately
before input and requires return to `Seq_Game_MainGame`/cabin. It performs no
purchase, forced terminal close or save write. This is reproduction tooling,
not a claimed fix, and does not cover development through iDroid.

The cutscene's exact hologram distance and scale have **not** been recovered.
The current 8 cm default is approximate. The native device sockets, parts
attachments and matching cutscene animation/UI package are recovered, but
their raw canvas units and effect offsets are not a measured screen fit.
Do not replace this remaining native projection work with another guessed
default or describe the present fit as an exact cutscene match.

| Request | Current result |
| --- | --- |
| One checkout and one stable playable folder | Implemented: this checkout, fixed play/, Play.cmd, automatic verified local synchronization. |
| Stop making dated local distributions | Implemented; 37 old generated folders/archives removed, 2.904 GiB reclaimed. |
| Keep Steam running; no computer control | Preserved throughout this work. Only owned test sessions and explicitly authorized unused simulator helpers are closed. |
| Left bionic forearm weapon display and ordinary HUD | Restored authored layout; both eyes and head/hand motion reviewed on the display baseline. Physical headset reading and every HUD context remain open. |
| Crisp high-resolution VR, stereo; desktop 720p | Native 2520 × 2640 per eye, distinct updating final eyes, desktop 1280 × 720. Continuous physical refresh and comfort remain open. |
| Stop simulator blinking and stale test loops | Large swapchain failure repaired; decoded-eye blank/freeze checks and bounded cleanup implemented. Reviewed recordings contain no observed blank frame; sub-sample flicker is unproven. |
| Pause and optional off-wrist HUD as spatial stereo | Paired off-wrist baseline passes retained. Current iDroid build passes native pause open/close and both-eye review. Physical headset and all off-wrist contexts remain open. |
| Optional scope stabilization | Installed at personal 100 ms; measured sampled-grip yaw jitter fell 72.5%. Public default remains off. Physical comfort and sight/muzzle/hit checks remain open. |
| Bring physical binoculars closer to the face | Installed: separate 3 cm safety clearance from 10 cm optical reference. Native observed ocular now reaches approximately 4 cm when requested, instead of stopping at 10 cm. |
| Keep binoculars inspectable, hand-aimed, zooming, acquiring and explicitly marking | Existing interactions preserved; no second zoom window. Current-build mission/acquisition/marking regression remains open. |
| Binoculars and hands always in the same space | Existing common final-palm publication preserved. Intermittent report and transition coverage remain open. |
| Match binocular lighting to the world | Open. Custom housing lighting is fixed and does not yet consume the native environment. Lens lighting under hand motion also needs reproduction. |
| Keep cover/lean/shoot and Y contextual actions without a third-person camera pull | Open. Native cover remains enabled; reproduce the reported wall transition before changing camera ownership. |
| Remove remaining turret reticles; preserve native yaw and pitch | Open. Need native mounted-state and vertical-travel reproduction. |
| Repair silent other long gun's support hand | Open. Native wrist selection identifies the other long gun as FAKEL (SLEEP). Native support-socket telemetry is installed; reproduce the bad two-hand contact before changing grip behavior. |
| Map capacitive face-button, stick, trigger and thumb-rest contacts independently | Implemented and installed: ten separate inputs, parser checks and fresh simulator runtime initialization pass. Physical sensor acceptance remains open because the simulator operator cannot synthesize capacitive contact. |
| Show the actual mappings and a quick video in the launcher | Installed in the fixed play tree: saved mappings, alternatives, one-hand/two-hand chords, ten amber contact surfaces and red pressed surfaces. 98 UI actions and eight settings-bridge contexts checked; 44-second personal mapping video available at artifacts/dev/controller-mapping.mp4. |
| Live rotatable 3D previews for the settings | Installed: all 55 controls settings have a reference fitting view, with live edits, drag/orbit/zoom, front/side/top/back, filtering, save/reload, discard and numeric-fit reset. Runtime theatre dimensions are also previewed. Reference geometry is separate from native projection-effect and headset acceptance. |
| Default -30 degree weapon tilt and check the arsenal | Firearm-only fit implemented, configurable and installed; current-loadout one-hand scope direction inspected. All-weapon, impact, reload, support-contact and headset coverage remains open. |
| Helicopter upgrade menu coverage | Completed Rival restriction repaired; current candidate reaches the actual ACC helicopter list, three-level exit and physical reopening. Selector, locked grades and purchase/result coverage remain open. |
| Audit one-hand/two-hand control chords | Effective personal binding audit retained; no unsolicited remapping. Headset usability and crouch/prone community report remain open. |
| Align right-hand iDroid with its native emitter/cutscene and survive forced stow/reopen | Native attachment retained; natural holding frame and lower-edge screen anchor revised. Default and custom fitting controls pass scoped simulator motion/reopening checks. Native light-cone retargeting at custom offsets/rotations, forced tutorial stow and physical headset acceptance remain open. |
| Preserve Coco's fixes; burn down community reports; all weapons/missions/game completion | Coco's scope-glass stabilizer is integrated and configurable. Independent lens exposure is on by default after matched native field comparisons and a cold replay. Firearm-specific optics, remaining reports and full-game acceptance remain open. |
| Publish the new build on GitHub | September 30 experimental prerelease packages the tested local DLL and committed source; see RELEASE_2026-09-30.md. Full-game and headset acceptance remain open. |

The next iDroid work is unfinished-guide/upgrade transaction coverage, exact authored screen fitting and native light-cone retargeting,
then physical fitting/readability. FAKEL (SLEEP) support-hand reproduction,
cover camera ownership, mounted weapon aim/reticles, binocular native lighting
and full mission regression remain on the existing release queue.

The September 30 released DLL is `edc6960890a4b45f`. It retains the left-arm display,
binocular clearance, scope stabilization, touch inputs and launcher tour. The
iDroid repair replaces the guessed anatomical palm mount with the measured
native right-wrist attachment and the device's named connector/hologram sockets.
That build's upright simulator grip holds the device upright; the projection inherits
the final rendered device pose. The authored native mount and screen stay
together during hand motion. See `docs/IDROID_ALIGNMENT.md`.

The released build's 38 automated checks and installer transactions pass. Its focused bot suite
has 93 passing checks, including safe input cleanup and explicit numeric pose
tolerances. Native Continue reached Mission 6; all 11 selected iDroid/menu cases
passed on this DLL. Both final eyes were reviewed at normal and opposing side
poses, through ordinary close/reopen and pause. The opening animation appears
in early captures; the settled interface is visible in full in both eyes.
These sequential stills establish scoped fit and menu behavior; they do not
establish continuous flicker, refresh rate, forced tutorial stow or headset
readability. Exact results are in `artifacts/dev/idroid-alignment-acceptance.json`.
The test game and its owned simulator were closed with inputs released and
menus closed. Steam and the user-requested launcher remain running. Both
personal INIs retain their recorded hashes.

The display baseline DLL is `f8dda93e8d1d4aec`. All 38 automated
checks and the installer transaction checks passed on that baseline. Fresh Mission 6 gameplay at
**2520 × 2640 per eye** now produces updating, distinct final compositor eyes.
The compact readout is back on the **left forearm**, with readable weapon,
ammo and suppressor text. The fix restores the native HUD's authored 128 × 72
layout; it does not move arm bones or replace personal bindings.

Both final eyes were inspected through wrist translation/rotation and opposing
head/hand motion. Four 24-second recordings contain 308 decoded, distinct
frames with no blank frame or detached readout observed. These are sequential
left/right compositor captures at 3.0–3.4 samples/second, not synchronous stereo
video or a measurement of headset refresh. Flicker between samples, physical
comfort and the rest of the game still require acceptance.

On that display baseline, Pause opened and closed on a spatial stereo panel with wrist HUD enabled and
explicitly disabled. Off-wrist ordinary HUD also remained spatial in both
eyes. iDroid opened and closed normally, but the tested hand pose clipped its
projection in the left eye. That historical failure is retained; the native
mount repair and current review above supersede its fit result.

Exact build hashes, retained runs and scoped review outcomes are recorded in
`artifacts/dev/display-acceptance.json`. Personal VR settings and controls were
restored byte for byte. Test inputs and poses were released/restored, test menus
closed, and both owned game sessions stopped. Steam was retained.

The first physical `Play.cmd` launch exposed a stale global runtime entry for
the removed Meta XR Simulator v205. The recorded previous Oculus runtime was
valid, but the launcher skipped that fallback when the active file was missing.
The fallback now handles a missing active manifest as well as a selected
simulator, without changing the global registry. `Play.cmd` retains a fixed
`artifacts/dev/headset-launch.log` and keeps errors visible. A connected Quest 3
was detected and MGSV was launched through Oculus; its focused XR session and
live controller inputs were observed. The user's game exited normally on
September 29 at 18:56 PDT; subsequent simulator checks use owned test sessions.
Never inject or release inputs in a user-owned play session.

The binocular clearance and adaptive scope stabilization are installed in
DLL 10726111959b. Native Mission 6 before/after observations show the close
ocular change and the measured grip-jitter reduction. The static AM MRS-71
grip inspection is retained under artifacts/bot/runs/20260930T031625630435Z.
The earlier DLL e54dac7bc79f added all ten touch inputs and native support-socket
diagnostics. Its native Continue reached Mission 6 with the new controls/rig
telemetry. The current DLL retains that work and adds the native iDroid repair;
the launcher tour is verified and synchronized into the same play tree.

## One place to work

Use this checkout. `play/` is the fixed current local test tree;
`Play.cmd` launches the installed game on the physical headset. The launcher
is always `play/MGS5VR-Launcher.exe`. `play/BUILD.json` identifies the source,
DLL and every installed file. Personal game settings remain authoritative.

```powershell
# Build, run checks, refresh the same play folder, and sync the recorded game.
tools/build.ps1

# Compare source, current play tree, installed DLL and issue status.
python tools/workspace.py status

# A temporary game-local runtime lease is restored by stop-sim; Steam stays open.
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
python tools/workspace.py bot --seconds 15
python tools/workspace.py stop-sim

# Run a deliberate case, or a bounded route over the private owned NAV2 graph.
python tools/workspace.py bot --command run --suite tools/gameplay_bot/suites/pause-roundtrip.json
python tools/workspace.py navigate --goal X Y Z --seconds 60
```

`build.ps1 -NoDeploy` refreshes play without changing the game installation.
Normal synchronization requires MGSV closed, checks the recorded install's
ownership, preserves both personal INIs byte for byte, and updates actual DLL
hashes. One previous play tree and one small installed-binary rollback are kept
under build; no dated distribution folders or ZIPs are made for local changes.
Promotion updates changed files in place, commits BUILD.json last and preserves
unmanaged local files. An unchanged open launcher stays running; a blocked file
update rolls back changed files. Failed tests do not promote a build.

`launch-sim` asks the already-running Steam client to start MGSV and records
the exact new game/runtime process generations. It temporarily selects the
simulator in `mgs5vr-runtime.ini`, without changing Steam's environment or the
global OpenXR runtime. `stop-sim` restores that file after the owned session
exits; an originally absent file is removed. Concurrent edits are preserved
and reported instead of overwritten. A timed-out launch remains recorded for
exact cleanup. The default build remains at `play/` throughout.

For an explicitly requested GitHub release, commit the public source, run the
same build/checks, then use `python tools/package-release.py --tag experimental-YYYY-MM-DD`.
The packager verifies the checked play tree and public defaults, includes the
committed source and per-file hashes, and writes to the fixed `build/github-release/`
directory. Private game data and simulator dependencies are rejected. Publishing
a prerelease does not close the remaining gameplay/headset acceptance queue.

The local settings file is `private/workspace.json`. The operator dependency
is imported once to `.deps/meta-xr-operator`, removing the other-repository
dependency. Retail assets and navigation remain private. Bot scratch evidence
lives under `artifacts/bot`; `latest.json` is the stable pointer. Retention keeps
three completed scratch runs, targeting 2 GiB. The latest active run and pinned
evidence can exceed that budget; use recordings deliberately. `workspace.py pin`
protects the latest run before citing it in acceptance. Existing historical
acceptance evidence is preserved. Only an explicitly selected case opens Pause.

## What the audit establishes

The September 30 experimental prerelease includes the retained September 27/28
candidate work, Coco's six supplied patches and the native A* bot. The previous
public release was September 24. All 55 community reports and 77 scoped claims
remain in the evidence ledger:

| Claim state | Count |
| --- | ---: |
| Simulator verified | 13 |
| Automated contracts verified | 4 |
| Failed | 4 |
| Needs physical headset | 7 |
| Not tested | 40 |
| Not implemented | 9 |

The ledger's RC5 DLL is c69b75c6. The installed build audited on September 29
was bc37bd31, while its install record still named RC4. The fixed workflow
records the actual current build and separates historical evidence from it.
Passing a build does not close those reports.

Coco's visibility, bob/height, cinematic camera, recovery, tracking continuity,
lower-body and shoulder work is retained. The native-map bot has demonstrated
Mission 6 bridge arrival using A* and ordinary VR inputs. General combat and
full mission completion remain unaccepted. Mission 1's real binocular lessons
and explanation/return passed on RC5; that is not complete Mission 1 acceptance.

## Immediate acceptance queue

1. Finish iDroid recovery acceptance: the current native connector/hologram
   mount passes normal grip, both-eye front/side fit and ordinary close/immediate
   reopen. Forced FOB tutorial stow and physical headset use still need actual
   game reproduction. Do not replace the native attachment with a palm guess
   or infer those outcomes from ordinary reopening. See `docs/IDROID_ALIGNMENT.md`.
2. Verify scope/reflex/holographic and muzzle/hit alignment per weapon family,
   zoom, eye, head/hand motion, reload, cover, and transition. Round scopes
   already use native sockets; reflex/holographic coverage is incomplete.
   Binoculars must remain inspectable in the hand, with one attached optic
   view, physical zoom, visible-enemy acquisition and explicit marking.
3. Audit the effective personal gestures for same-hand versus two-hand use.
   Preserve the established mapping during defect repair. Simplify only with
   explicit physical testing and current-context conflicts checked. GitHub
   crouch/prone issue #11 remains open; contracts alone do not close it. The
   recorded personal mapping uses two hands for reload (left grip + B), left
   hand only for binocular equip (left grip + Y), and right hand only for weapon
   switching (right grip + stick click). See `artifacts/dev/control-audit.json`.
4. Extend the left-arm and stereo acceptance to high-frequency capture,
   physical Quest motion/reading, all HUD contexts and recovery transitions.
5. Complete native mission/checkpoint routes, cinematic and display/recovery
   cases, and physical Quest testing. Burn down the rest of the community
   ledger against the same identified build before the next GitHub release.

Fresh baseline evidence is in `artifacts/dev`: `baseline`, `pause-baseline`,
`idroid-baseline`, `off-wrist-baseline`, `off-wrist-pause`. Native outcomes and
both-eye images are retained. The original personal INI hashes were restored;
the test game/simulator were closed. Preserved runtime work is archived in
`artifacts/dev/preserved-runtime-work.diff`. Release notes identify the scoped
September 30 prerelease; it does not claim full-game acceptance.

## Display and test-loop recovery

The desktop preview is **1280 × 720**, independently of VR quality. Preserve the
active runtime's per-eye FOVs; a square target is not a substitute for correct
optics. Meta XR Simulator v207 recommends 1680 × 1760 here. The current test
renders native eyes at 2520 × 2640 (150% in each dimension) and filters each
eye separately into fixed 1680 × 1760 simulator swapchains, in linear light.
Physical runtimes retain the direct native-resolution path. This avoids the
simulator's failed large-swapchain path without lowering native scene detail
to desktop resolution. Earlier stale large-target takes remain failed evidence.

The bot checks both decoded compositor eyes for blank or frozen gameplay,
including left-arm motion and stationary active gameplay. Startup waits for
native Lua readiness and the real title/loading fade; a blank gameplay frame
still fails. Pause may legitimately retain static pixels. The final source
and compositor recordings have separate provenance; a native recording alone
does not certify the simulator output. Recorder overhead lowered measured XR
submission cadence during some runs, so a 90 Hz physical performance claim
has not been made.

Historical display evidence and current iDroid evidence are pinned under
`artifacts/bot/runs`; each record identifies its tested DLL:

- `20260929T233417986949Z`: native Continue, left-arm cases, two-eye stills,
  `temporal-accepted` wrist motion and `head-temporal` head/hand motion.
- `20260929T233927210347Z`: deliberate Pause and iDroid roundtrips; iDroid fit
  remains open despite successful native open/close.
- `20260929T234722570591Z`: explicitly disabled wrist HUD, front stereo HUD,
  Pause roundtrip and cleanup. Original settings restored afterward.
- `20260930T062309020656Z`: current native iDroid attachment, centred normal
  grip, front/opposing side views, ordinary close/immediate reopen, pause and
  final close; 11 native passes and scoped two-eye visual review. Tutorial
  forced stow and physical headset acceptance remain open.

The local build cleanup reclaimed 2.904 GiB from 37 generated distribution
folders/ZIPs, retaining unique binaries and their inventory in
`artifacts/dev/retired-builds`. Local iterations use the same `play/` tree.
