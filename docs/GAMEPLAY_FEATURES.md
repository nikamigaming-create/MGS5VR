# Gameplay beyond weapons

Updated 24 October 2026. See [FIELD_FIXES.md](FIELD_FIXES.md) for the code audit and [RELEASE_PLAN.md](RELEASE_PLAN.md) for execution. These are native game systems that the VR controls,
camera, hands and UI must preserve. A mapped button is not a claim that the whole
interaction works. Multiplayer, FOB and MGO are outside this project request.

Complete **native-button mode** is now available through Menu+A held 0.55 s.
Its separate config maps all 14 native buttons, both analog triggers and all
stick axes, retaining native holds/combos in both games. The table below describes
normal VR presentation and remaining integration, not missing native input bits.
See [Quest 3 / Virtual Desktop tester issues](QUEST3_TESTER_FEEDBACK.md) for the
historical hardware reports, reconciled against FIELD_FIXES; renderer issues are not considered fixed by remapping.

| System | Current access | Remaining work |
| --- | --- | --- |
| Hold-up and interrogation | Ready with right grip; Commands on held X, right-stick choice, A confirm. Existing aim/CQC holds now survive that menu. | Complete real guard interaction, readable choices/responses, target selection from the VR view, camera/hand behavior throughout restraint. |
| CQC | Right trigger with weapon lowered reaches native CQC; analog trigger travel is preserved. | Grab/release, throws, chained takedowns, choking, knife finish, disarm, human shield and wall/cover contexts need complete native-action and rig integration. Physical punches have contact support, not complete enemy-reaction coverage. |
| Bodies and prisoners | Hold B for native pickup/carry; Y for native context; lowered trigger for native throw. | Carry, set down, throw, loading into vehicles/helicopters and rescue completion. There is no physical hand-grabbing system for bodies. |
| Extraction | Held Y reaches native context actions. | TPP Fulton soldiers/objects and failures; helicopter prisoner extraction in both games. GZ does not acquire TPP's Fulton/buddy systems through a remap. |
| Stealth and traversal | Walk, run, crouch/stand, prone, dive and context inputs are mapped. | All cover/peek, crawling/rolling, climbing, ladders, ledges, locked doors and hiding-place contexts; alert/Reflex/damage feedback and camera transitions. |
| Reconnaissance | Physical binocular equip/aim/zoom, automatic acquisition, explicit marking and native intel requests are implemented; village marking has scoped lesson evidence. NVG and selected authored weapon optics exist. | Wider target/unlock/tutorial cases, marked-actor presentation and physical readability. Do not restart the existing native integration. |
| Buddy orders | Wrist Commands; D-Horse call/Stay back, riding and distraction knock exercised. Historical D-Dog pet observations retain their original build scope. | Latest `19e51307` field strokes failed: intermittent native bone-query loss in the input-polling path interrupts contact. Native-transaction and raw-tracking-motion repairs are checked/deployed; successful physical pet filming remains open. Wider buddy orders and target interactions remain; only the deployed native buddy occupies the active slot. |
| Powered arms | Equipped native action remains accessible through ready/trigger and Commands. During Rocket/Blast Arm flight, tilt the left hand from its launch orientation to steer; return to neutral to stop steering. The movement stick overrides hand steering and resets its neutral orientation. | Stun and sonar need their own acceptance. Rocket Arm steering and normal weapon return have scoped simulator coverage; target impact, Blast Arm and physical-headset acceptance remain open. |
| Items and support equipment | NVG on/off, C4 placement/detonation, decoy inflation and smoke have individual SIM observations. | Other grenades/mines, box/poster/hide/slide/eject actions, magazines, phantom cigar, stealth items, drugs and state-specific use/alternate controls. |
| Transport | Horse basics and bounded mounted rifle shots; truck entry, wheel acquisition/steering and exit observed. | Sustained driving/braking, mounted turrets/mortars, Walker Gear/D-Walker, helicopter roles and all transitions. The vehicle-weapon handoff exists in `src/vehicle_weapon.cpp`; broad driving/fire acceptance remains. Physical mounted admission currently covers mortar and MG only. |
| iDroid and progression | Left cinematic handset/projector, ordinary stow/reopen, scene handoff and exit-notice repairs are implemented; native menus and spatial Pause exist. | Forced unfinished lessons, continuous stereo and physical acceptance; broader Support drops/strikes, helicopter requests, deployment, development, Mother Base, mission completion/retry, death and replacement transitions. |
| Presentation and feedback | Fixed-length native body/pelvis fit, tracked garment drivers, hands/forearms, left-forearm HUD, left-hand iDroid and spatial Pause. | Flat cuff caps have scoped stereo review on the admitted outfit; other outfits, extreme full-body cloth crowding and physical reach remain. Broader spatial subtitles/damage/context feedback, title/loading/cinematic cases and action-specific hand animation remain unaccepted. |
| Ground Zeroes | Independent native first-person stereo, tracked arms/weapon aim, left-hand iDroid, forearm ammo HUD, physical binoculars and native acquisition/mark/clear/intel. Normal setup imports its owned optic assets. | Remaining settings/material effects and complete weapon, guard, rescue and mission interactions. Current simulator observations do not certify whole-game or physical-headset behavior. |
| Showcase | A 4:23 private working cut combines cabin, clear TPP field actions, paired iDroid/wipes, GZ recon/intel, retained Rocket/Blast Arm and live settings. Six chapters now use animated official Quest Touch Plus instructions. | Remaining field features and the finished combined film; exact source/build scopes remain in the edit indices. Failed physical D-Dog takes are excluded. |

Controls are editable in `mgs5vr-controls.ini`. The detailed mapping is in
[CONTROLS.md](CONTROLS.md); recorded scope/action coverage is in
[DUAL_GAME_SIM_COVERAGE.md](DUAL_GAME_SIM_COVERAGE.md).

Native guard behavior reference: Konami's
[tactics manual](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html).
