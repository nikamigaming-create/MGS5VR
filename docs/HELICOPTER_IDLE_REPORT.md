# Idle stereo and helicopter menus

The user reports blinking in Meta XR Simulator, black physical-headset output
after idle, and unclear next steps when loading and trying to reach free roam.
Complete menus, game states and items remain the release objective. No scoped
test supplies a whole-game or physical-headset pass.

## Reproduced stereo defect and repair

Removing raw head validity for seven seconds in actual ACC reproduced black
compositor output on diagnostic candidate `f87d9be1302f`, with empty XR layers
despite an existing accepted stereo pair. Negative evidence is retained in
`20261004T045321827065Z`.

Candidate `72fdf5e65ad5` retains only the already admitted image pair with its
original poses, eye FOVs and crops during tracking loss. Fresh pixel admission
still needs valid tracking. Texture epoch, camera activation, reference-space
identity and clock ownership must still match. A runtime request not to render
is honored. The desktop preview stays 1280 × 720; eye resolution/FOV remains
the active runtime's recommendation.

Retained `20261004T050618985244Z` passed head-only, view-only and combined
validity loss/recovery: six cases, each loss lease seven seconds. Both eyes
were reviewed at every outcome. The scoped log has no empty-layer event;
camera/rig publications and decoded eye pixels advance after recovery.

Retained `20261004T060748849943Z` passed the same six cases with handheld
iDroid open on **Daily Bonus Received**. Its twelve outcome images show both
arms, the phone, native information card and cabin. Its scoped log also has
no empty-layer event. This is evidence for that observed card, not Map or all
menus. Sequential final-eye images do not establish simultaneous stereo or
continuous absence of blinking.

The earlier twenty-minute no-input simulator observation did not reproduce
the original idle report. Its two short 3 FPS recordings contain no blank
decoded frames; their cadence cannot certify headset stereo or rule out brief
blinking. Actual physical inactivity, headset removal/resume, focus loss,
runtime/Link stalls and reference-space resets require separate evidence on
the user's physical runtime. The reported physical blackout remains open.

## Actual menu results

Retained `20261004T054602946280Z` reviewed the actual Choose a Rival card and
ordinary VR acknowledgment, Missions/Map navigation, Mission List entry and
Back, and root Back restoring the cabin. Effective bindings were A Confirm,
B Back, left grip Previous Tab and right grip Next Tab. The tab boundary does
not wrap. Right-hand iDroid and left bionic arm presentation remain intact.

Native area selection was active but invisible in that ACC state. A temporary
passthrough visibility trace preserved native arguments and was restored.
Calling the native story-dependent helicopter setup once did not restore the
branch; story and GMP were checked unchanged. No forced visibility, menu
unlock or guessed area-selection callback was added to the production code.
The helicopter-to-free-roam path and its guidance remain unresolved.

The later information-card run's Back/stow case failed: Back advanced from
Daily Bonus Received to **Skulls Attack**. Native popup=false and an open-menu
flag did not identify either page. No unknown card was confirmed. The full
run remains failed despite its six successful stereo diagnostic cases;
cleanup closed the owned game/runtime, retaining Steam.

Earlier test entry and operator-release failures also remain failures. A
supervised recovery wait stopped after two seconds while its known seven-second
diagnostic lease was active. The runner now defers that review only for its
own bounded lease, unchanged presentation owner and neutral input; unarmed or
expired loss and unexpected popup ownership still require review.

## Reusable bounded fixtures

Enable the explicit simulator diagnostic, then reach and review actual ACC:

```powershell
python tools/workspace.py launch-sim --tracking-fault-test
python tools/workspace.py bot --command continue --seconds 100
python tools/workspace.py bot --command run --suite tools/gameplay_bot/suites/head-tracking-recovery.json
```

The closed-menu fixture requires ACC and cannot guess or dismiss a menu.
For an already reviewed open handheld iDroid page, select
`tools/gameplay_bot/suites/head-tracking-idroid-recovery.json` instead.
These fixtures only remove raw validity; they cannot synthesize valid poses
or change controller bindings. The independent leases expire within eight
seconds and clear on focus loss. Pin and review new evidence before relying
on it. Release inputs, close test-opened menus and stop owned sessions with
`python tools/workspace.py stop-sim`; retain Steam.

All menus, item grades/costs/locks, unfinished tutorials, native roles,
loading/results/death states and continuous physical acceptance remain in
[the menu ledger](VR_MENU_COVERAGE.md) and
[regression catalog](../tests/vr_regression_catalog.json). The current save's
nineteen helicopter definitions are already developed; it cannot certify
unpurchased, development-in-progress or first-time tutorial branches.
