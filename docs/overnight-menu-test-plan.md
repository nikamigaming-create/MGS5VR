# Overnight native VR menu testing

This plan extends the retained ACC and Map results. Its queue is not a claim
that every native page, transaction or controller has passed. Read the current
DLL/configuration/fixture identity and actual final-eye page before admitting
each next action. Keep the ordinary save out of progression-changing tests.

## Dispatch and result contract

All inputs use the effective semantic VR binding exported by the running mod.
Read `menus.confirm`, `menus.back`, tab actions and `axes.menu` from that export;
do not assume A, B or a particular stick. Record the actual chord/gesture/touch
tokens displayed by the prompt and sampled by the native resolver. A sampled
button or cursor edge proves delivery; it does not prove the selected page or
transaction. Every result below also needs both final-eye page review.

Before each action, re-read mission, sequence, native role, title/loading/demo,
save state, popup and tutorial ownership. Recheck the same native identity after
input release. Require fresh menu input and known page/focus; a missing getter
stays unknown. Held cursor sampling must not perform a slow Lua request. Always
release input, require sampled neutral sticks/buttons, restore saved hand poses
and finish in the live camera/gameplay input owner. Unknown dialogs receive no
automatic Confirm or repeated Back.

For ordinary field Pause require `tutorial_pause=false`, `popup=false`,
`saving=false`, `demo=false` and `demo_nonplayable=false`. Both fast and native
owners must agree: ordinary Pause is menu=true, pause=true, iDroid=false; ordinary
iDroid is menu=true, pause=false with its native input-ready publication.
ACC selectors use menu=true, pause=false, iDroid=false, mission=40010,
helicopter_space=true and `Seq_Game_WeaponCustomize`, plus the exact
`customization_kind`. They are not ordinary Pause or ordinary iDroid.

## Ready native regression

`tools/gameplay_bot/suites/overnight-menu-field-pause-roundtrip.json` runs the
explicit field Pause open -> Back -> immediate reopen -> Back path. It is
restricted to the observed early-field mission 10020 / `Seq_Game_RescueMiller`
fixture. It rejects the s10020 tutorial Resume/Skip page before input. Each Back
requires a live tracked camera, gameplay rig input and neutral controls. Do not
relax its native sequence to make a mismatched fixture pass; discover that
fixture's actual legal Pause page and make a separate case.

The existing `acc-completed-rival-roundtrip.json` remains the completed-FOB
fixture regression. Its eligibility result is independent of a Development
page transition. The existing `acc-helicopter-customization-back.json` is a
known helicopter selector discard path, starting from a visually reviewed
selector. It must not be run on a weapon/vehicle selector or an unknown popup.

## Next ACC and Development cases

Run each retained path again on the candidate identity. For handheld and
explicitly selected spatial iDroid presentation, retain these separately:

| Path | Fresh admission and actual result |
| --- | --- |
| Completed Rival Help | Completed FOB state 127, mission 40010/main game, actual Choose a Rival Help identity. Physical Confirm closes that Help, restores the captured eligibility/restriction owner, then root Back returns to live cabin and physical iDroid reopens. |
| Unfinished Rival guide | A genuinely unfinished paired-save fixture; record its actual required page and permitted actions. Preserve the guide. A completed-guide result cannot cover it. |
| Daily Bonus | Review this exact information card. Confirm must reveal its actual next page; a separate Skulls Attack card is a new owner requiring new review. |
| Mother Base root -> Development | Review the actual root and Development focus. Rewards may precede Customize; zero-based or fixed-edge row counts are not choice identities. Physical Confirm must show the Development category page. |
| Each of four categories | Weapons/Items, Buddy Equipment, Helicopter and Security Devices each get entry, list bounds, item detail, cancel, Back at every depth, stow and immediate reopen. Review both eyes of each actual page. |
| Already completed grade | Read the native develop ID, completion and eligibility. Confirm/no-op must not spend resources; return and reopen. Never call this a purchase. |
| Locked grade | Separate team-level, blueprint, previous-grade and any discovered lock reason. Display/rejection must match that reason; leave safely and restore controls. |
| Insufficient resources | Native and displayed GMP/material shortfall must agree; rejection/cancel leaves each resource unchanged and returns to its actual parent. |
| Eligible undeveloped grade | Disposable paired-save only. Review exact develop ID and all costs; cancel first, reopen, buy through physical Confirm, then verify exact native resource delta and development-in-progress status. |
| In-progress grade | Read its actual development status/time and legal cancellation options. Do not force a timer or equate a successful purchase with completion. |
| Newly complete grade | Native completion/result, claim where required, subsequent availability and eligible equipment/customization selection. Use a genuinely complete result fixture. |
| On-foot Development | Verify availability in Mother Base and progressed Afghanistan/Africa states independently. The early Mission 10020 empty Mother Base page is neither a valid purchase fixture nor proof of a disabled implementation. |

`tools/development-state.lua` is read-only and reports actual helicopter IDs,
completion, requirements and resources. It deliberately publishes
`page_identity=null`: definition grade and requirements met cannot identify the
selected UI row or prove a new purchase. The completed ACC fixture has all
nineteen helicopter definitions developed and needs a distinct purchase fixture.

## ACC customization branch queue

The owned retail `HELI_COMMON` source registers weapon, helicopter and vehicle
targets separately. `Customize_Abort` transitions through
`Seq_Game_WeaponCustomizeAbort` to the native main cabin and resets the
`CustomizeSelector` pad owner. `Customize_End` follows the distinct End path,
restores camera/time/weather/pad state and saves Mother Base management. Those
are different acceptance outcomes; terminal-open=false alone proves neither.

| Exact kind | Separate cases and result evidence |
| --- | --- |
| Helicopter | Retain selector entry; Back -> reviewed cancellation dialog; Confirm on Cancel returns to the same selector with edits preserved; Back -> reviewed discard selection -> main cabin; immediate physical iDroid reopen. Test no-change save and actual legal chassis/weapon/option/color edits, then re-enter to verify persistence on a disposable fixture. |
| Weapon | Independently discover entry and actual cancellation selection. Test Cancel, discard and no-change/changed End using the same verified native target. Exercise every discovered part-selection page and locked/compatible-part branch; re-entry verifies actual selected parts. |
| Vehicle | Independently discover eligible native subtype/selection. Test Cancel, discard, no-change/changed End and every discovered available/locked selection; verify camera/input return and persisted changes. |

The helicopter Back dialog has Cancel selected by default in reviewed footage.
Do not infer that choice on a different dialog or selector. `GetPopupSelect()`
is a completed popup result, not a proven live focus/selection getter.
`mvars.heliSpace_currentEditTarget` is updated by the authored
`Customize_ChangePart` message and may help observe the camera edit target. It
does not establish the selected UI row or all currently legal parts.

## Exact page observer still required

The new read-only popup observer covers the supported TPP UiSystem popup
owner, independently of the broader menu flags. It validates that retail
adapter's getter/owner ABI and typed UiSystem, reads complete bounded ranges,
and rejects a replaced parent/child, changed dialog or changing snapshot.
It never calls the native readiness getter, which can initialize a cache.
An open child with readiness still unestablished remains unknown.

`nativePopupSnapshot()` keeps current numeric ID, StringId and the signed last
completed response separate. Current IDs are reported only with a known active
popup terminal; a closed popup has no current ID. The last result is not live
selection/focus. `reportUiRenderer` appends this diagnostic as `popup_observer`
to the native matrix journal with its sample time and `coherent_frame=false`.
Periodic journal records are not automatically fresh action prerequisites.
The read-only `inspect-bot-state` RPC now samples the same `popup_observer`
contract freshly, with a separate `sample_ms`. `NativeReader` and `Live.observe`
retain unknown/null values, exact StringId text and signed `last_result`.
This packet is individually sampled, not a coherent rendered-frame transaction.
Native page/focus observation still needs separate work, and a popup ID is not
automatically an accepted dialog map or permission to Confirm.

Prompt diagnostics reserve independent finite pools for plain captions and
`<I=G=` input-tag sources. Typewriter subtitles cannot exhaust the menu-tag
budget. Entries retain the actual text-entry node/unit or parser contexts,
known/unknown font, source pointer, exact text and bounded caller stack.
Parser contexts are not claimed to be recovered native menu owners.

The source contract was recovered statically from the private, owned supported
retail image (SHA256
`3fdc4ea24d616f7d05965c8ea8dda1e25a7872fbc94733c00ba4b65490255203`).
The public observer is independently written; owned code/data, static listings
and runtime dumps remain private. Its contract test exercises missing/partial
reads, unsupported ABI/type, replaced owners, changing IDs, readiness unknown,
both popup-terminal branches, stale-ID suppression and preservation of native
bytes. This is diagnostic ownership verification, not an accepted dialog map.

For an unattended full tree walker, obtain a read-only, build-validated
publication of current page, parent page, selected choice ID, enabled choices,
popup/help identity and popup selection. Associate it with the same native
sequence, input sample and rendered frame. Test disappearance, recreation,
unknown IDs, focus change during capture and same-open-bit page replacement.
The current scenario query reports broad menu and selector ownership; it has
no authoritative Development page/focus getter. Reviewed compositor stills
can admit a bounded next action, but cannot drive unknown nested paths without
new review. Known stock/error IDs supported by `IsShowPopup(id)` must be
validated for their exact dialogs; do not invent IDs for unknown ones.

The same observer is needed for native Options/submenus, support/deployment,
staff/resources, cassettes/intel, online/FOB entry, loading/results/death and
role-dependent pages. Keep horseback, each vehicle role/subtype, emplacement,
Walker, helicopter passenger, cinematic and forced tutorial states separate.
Expand the existing `vr-menu-paths.json` family queue from actual discovery.
Every legal branch remains an obligation even when pairwise runs prioritize
which combination is tested first.

## Promotion and release

Retain exact source/build, effective bindings, six-file fixture/rollback,
sampled inputs, outcome facts and both final-eye captures. Promote only a
finished bounded run with cleanup verified; failed experiments retain their
failure status. Sequential stills do not prove continuous stereo or absence of
blinking. Use final-eye motion video and separate physical-headset usability
for those claims. Preserve left-arm ordinary HUD, right-hand iDroid, physical
optics and 1280 x 720 desktop independently of recommended VR eye sizes.

The root owns the one live game/session, fixture activation and release. Build
and synchronize fixed `play/` only after the owned game is stopped. Steam stays
running. Report tested paths and unresolved branches in the release notes;
contract tests alone never certify all menus or the whole game.
