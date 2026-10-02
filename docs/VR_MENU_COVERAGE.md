# All-menu VR walkthrough

The target is every discovered TPP menu page and legal branch in every eligible
native game state, using the player's effective VR bindings. Test each branch
where it is actually available; do not count an open-menu flag as a menu pass.
Menu discovery, complete automation and physical-headset acceptance are open.
The connected map/state planner is described in GAME_STATE_MODEL.md; menu paths
are one part of that graph, alongside native roles and authored mission states.

## Walkthrough order

| Native state | Menu paths to discover and exercise |
| --- | --- |
| Title / cabin | Every visible title choice, Continue, setup and options branches. |
| ACC | iDroid tabs, customization, mission selection, character/buddy/loadout and sortie preparation. |
| Mother Base | Map, staff/resources, development categories and grades, helicopter upgrades, tapes/intel and available contextual menus. |
| Afghanistan / Africa free roam, missions and Side Ops | Each available iDroid branch, support requests, equipment, buddy commands, interrogation, Pause and options. Keep these states separate. |
| Horse, vehicle driver/passenger, turret/mortar, Walker and helicopter passenger | Discover which menus are available per native role, then test entry, choices, exit and control restoration. Split vehicle/station variants. |
| Cinematics / forced tutorials | Allowed menus, required acknowledgments, forced stow, immediate reopening and return to the correct player/camera. |
| Loading / results / death | Visible start confirmations, clear/abort/death screens, retry and return branches. |
| Online / FOB menu entry | Discover eligibility and entry/cancel branches separately. TPP online gameplay remains unaccepted; MGO and Ground Zeroes need their own game adapters. |

Each row must expand into the actual child pages found in-game, including locked
choices, progression-dependent choices, buddy/ability variants and confirmation
dialogs. The catalog is a starting queue, not an exhaustive list of game screens.
Unexpected pages become new paths; they do not get generic Confirm or repeated Back.
Root rows can change with progression and informational rewards: the October 1
ACC warm review had Rewards before Customize, whereas the earlier cold review
began at Customize. A recorded row count is not a selected-choice identity.
Cursor steps use a sampled edge; choose `mode: "hold"` explicitly only when
reviewed iDroid scrolling needs a bounded hold. Do not count the dispatch audit
as proof that the intended row/page was reached.

## What happens on each path

1. Enter a verified scene/checkpoint. Record mission, sequence, native role,
   progression, page, parent page, selected row and legal choices.
2. Open through the existing VR interaction. Visit every child page, scroll/list
   boundary, tab and applicable map/pointer control. Read the actual choices.
3. Check safe selection, disabled-choice behavior, confirmation, cancellation
   and Back from every depth. Verify the resulting native action or page change.
4. Close, immediately reopen, then close again. Confirm neutral inputs and
   ordinary movement/combat ownership recover, with no menu or pose left behind.
5. Review both final-eye sequences through head/hand motion: crisp readable text,
   complete choices, correct attachment, stable stereo and no blinking. Review
   physical-headset readability and usability separately.

Ordinary status/HUD remains on the **left bionic arm**. Handheld iDroid remains
in the **right hand**. Pause and selected off-wrist presentation use a spatial
stereo panel. Handheld and spatial variants are separate obligations. Native
integration stays behind VR actions; stock gamepad-only operation is not acceptance.
VR uses runtime-recommended eyes/FOVs independently of the 1280 × 720 desktop.

Read-only discovery goes first. Purchases, upgrades, resource use, deployment,
abort/retry and progression-changing actions need an isolated verified save or
checkpoint fixture. The ordinary play save must not become the test fixture.
Settings changes must restore the saved value, including cleanup after failure.

## Upgrade fixture and branch obligations

The fixed private store is `artifacts/test-profiles/`. `tools/test-saves.py`
snapshots and verifies all six paired campaign/personal files together. Switching
requires MGSV and Ground Zeroes to be stopped and both Steam cloud mirrors
disabled; Steam itself stays running. Activation records a verified rollback.
Never infer mission, unlocks or item status from the fixture's name or hashes:
load it and record native state and the actual selected grade first.

| Discovered grade/state | Required branches and resulting evidence |
| --- | --- |
| Already developed | Confirm behavior, detail/list return, Back from every depth and reopening; no purchase claim. |
| Locked by team level, blueprint or prior grade | Each distinct reason displayed, rejected selection and successful exit. |
| Unlocked but insufficient GMP/materials | Display actual required/missing amounts, reject or cancel without spending, restore controls. |
| Eligible undeveloped | Detail -> cancel -> reopen -> reviewed purchase confirmation -> native resource/development change -> result -> Back/reopen. Use a disposable paired-save fixture. |
| Development in progress | Remaining work/status, available cancellation behavior and return; do not force timer completion. |
| Newly completed | Native result/claim, grade availability, return and subsequent equipment/customization selection where eligible. |

Each discovered helicopter body, weapon and optional-equipment grade receives
those applicable branches in ACC and eligible on-foot contexts, with handheld
and selected spatial presentation tracked separately. Same-family UI does not
prove another category, mode or grade. Unknown/transient informational cards,
help overlays and network/result dialogs receive their own page identities.
Pairwise combinations can prioritize runs; they do not remove untested legal
transitions from the queue. Saves, owned game assets and private captures are
not public release assets.

The current `acc-completed-fob-baseline` reproduces the completed Rival guide.
Its actual helicopter list and three-level return were reviewed on October 1;
all nineteen helicopter definitions are already completed, so it cannot test
new helicopter purchases. A fresh first-time/unfinished-guide fixture,
eligible unpurchased grades, costs/locks and completion/result fixtures remain
required. Read the actual completion and resource facts before selecting a
purchase fixture:

```powershell
python tools/native-actions.py --file tools/development-state.lua --raw
```

This is a read-only state report. `requirements_met` can include completed
items; `grade` describes the equipment definition, not progress. Unavailable
native getters stay unavailable. `page_identity` remains unknown and this
report alone cannot choose a row or pass a menu case.
The ACC AM D114 LA transaction in retained `20261001T200807859685Z` spent exactly
the reviewed GMP and both metal costs and restored the cabin during cleanup.
Its native completion timer remains unaccepted. Security Devices entry/Back
passed there after an earlier operator-release timeout remained failed.
The physical A that dismissed Daily Bonus is recorded separately;
the prior test that sent no input remains failed.

## Current tools and gaps

The fixed review view is generated without launching or controlling the game:

```powershell
python tools/workspace.py coverage --menus

# Add reviewed evidence only after pinning it. The latest retained baseline:
python tools/workspace.py coverage --menus --run artifacts/bot/runs/20260930T215049265217Z
```

It overwrites `artifacts/dev/coverage/index.html` and `coverage.json` in the same
folder. Filter/search by menu family, native state or presentation. Planned
menu/state pairs start with availability and page discovery unestablished;
missing adapters do not disappear. These counts are not a percent of the game.
Evidence from another DLL/configuration stays historical. Evidence for one mode
or presentation does not certify any other mode or presentation.

Available automated recipes cover bounded open/close and iDroid fitting; they do
not yet walk every page. Run only an explicitly selected recipe in an owned test
session through `tools/workspace.py bot`. For example:

```powershell
python tools/workspace.py launch-sim
python tools/workspace.py bot --command session --suite tools/gameplay_bot/suites/idroid-fit.json
python tools/workspace.py stop-sim
```

Pause is selected explicitly with `pause-roundtrip.json`; it is never opened by
the default observation command. Release controls, restore test poses/settings,
close test-opened menus and stop owned test sessions on exit. Steam stays running.

The next implementation is exact page/focus/choice observation and guarded
recipes for the discovered branches. Current fast telemetry normally knows
menu/iDroid ownership, not the exact native page or enabled selected choice.
The existing supervisor can review actual final-eye captures and dispatch bounded
semantic VR actions; a broad menu flag cannot safely drive a whole menu tree.
After repeatable simulator traversal, each group receives a physical-headset pass.

The earlier helicopter list/Back baseline is partial field Mission 6 evidence.
Native ACC Development and the Support Helicopter Armament list have now been
reached and reviewed separately. ACC's Mother Base root varies with rewards:
Customize-first and Rewards-first layouts were both reviewed. Development
selection must use the actual current page/focus. The old two-edge cleanup
failed at the three-level leaf.
The October 1 completed-Rival candidate has a reviewed native leaf -> Back ->
stow -> physical-reopen pass with restored Development eligibility. See
ACC_MENU_REGRESSION.md for exact runs and limits. Selected field grades were
already developed, so purchase completion remains open.
Another player reported the upgrade lock-up in the ACC; the user did not
personally experience it. The field baseline does not reproduce that report.
Keep ACC iDroid development and ACC helicopter customization as separate
candidate paths until the exact screen is established. Progression-changing
tests require a verified isolated save/checkpoint fixture.

The targeted customization cancel probe is
`tools/gameplay_bot/suites/acc-helicopter-customization-back.json`. Its corrected
dialog/discard path passed in retained `20261002T040239079392Z`, including live
cabin return and immediate physical iDroid reopening. On a visually reviewed,
already-open ACC helicopter selector,
run it with `tools/workspace.py bot --command run --suite <that path>`.
It opens no selector: ordinary VR Back opens the native cancellation dialog,
then the effective menu stick selects discard and VR Confirm accepts it.
Exact selector, target, popup/save and input-context guards are checked before
dispatch. Return must reach the native main cabin and live camera/control owner,
not merely clear iDroid's open bit. Failed outcomes stop; no repeated Back or
native state forcing is used as the recipe. Development through iDroid remains
a separate path. Part changes, purchases and locked-grade variants remain open.
