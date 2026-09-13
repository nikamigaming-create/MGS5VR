# Gameplay beyond weapons

Updated 13 September 2026. These are native game systems that the VR controls,
camera, hands and UI must preserve. A mapped button is not a claim that the whole
interaction works. Multiplayer, FOB and MGO are outside this project request.

Complete **native-button mode** is now available through Menu+A held 0.55 s.
Its separate config maps all 14 native buttons, both analog triggers and all
stick axes, retaining native holds/combos in both games. The table below describes
normal VR presentation and remaining integration, not missing native input bits.
See [Quest 3 / Virtual Desktop tester issues](QUEST3_TESTER_FEEDBACK.md) for the
current hardware-reported gaps; renderer issues are not considered fixed by remapping.

| System | Current access | Remaining work |
| --- | --- | --- |
| Hold-up and interrogation | Ready with right grip; Commands on held X, right-stick choice, A confirm. Existing aim/CQC holds now survive that menu. | Complete real guard interaction, readable choices/responses, target selection from the VR view, camera/hand behavior throughout restraint. |
| CQC | Right trigger with weapon lowered reaches native CQC; analog trigger travel is preserved. | Grab/release, throws, chained takedowns, choking, knife finish, disarm, human shield and wall/cover contexts need complete native-action and rig integration. Physical punches have contact support, not complete enemy-reaction coverage. |
| Bodies and prisoners | Hold left grip+B for native pickup/carry; Y for native context; lowered trigger for native throw. | Carry, set down, throw, loading into vehicles/helicopters and rescue completion. There is no physical hand-grabbing system for bodies. |
| Extraction | Held Y reaches native context actions. | TPP Fulton soldiers/objects and failures; helicopter prisoner extraction in both games. GZ does not acquire TPP's Fulton/buddy systems through a remap. |
| Stealth and traversal | Walk, run, crouch/stand, prone, dive and context inputs are mapped. | All cover/peek, crawling/rolling, climbing, ladders, ledges, locked doors and hiding-place contexts; alert/Reflex/damage feedback and camera transitions. |
| Reconnaissance | Physical binocular view, zoom, manual waypoint mark/clear and native NVG. | Automatic enemy acquisition/analysis/intel, readable target information and stable native marked-actor visuals. Manual waypoint placement is not automatic enemy scanning. |
| Buddy orders | Wrist Commands; D-Horse call/Stay back, riding and distraction knock exercised. D-Dog petting observed. | All D-Dog, Quiet and D-Walker orders, deployment/replacement, ability availability and their target interactions. Only the deployed native buddy occupies the active slot. |
| Powered arms | Equipped native action remains accessible through ready/trigger and Commands. | Stun, sonar, Rocket/Blast Arm actions and guided-camera return; these are not established by ordinary hand punches. |
| Items and support equipment | NVG on/off, C4 placement/detonation, decoy inflation and smoke have individual SIM observations. | Other grenades/mines, box/poster/hide/slide/eject actions, magazines, phantom cigar, stealth items, drugs and state-specific use/alternate controls. |
| Transport | Horse basics and bounded mounted rifle shots; truck entry, wheel acquisition/steering and exit observed. | Sustained driving/braking, mounted turrets/mortars, Walker Gear/D-Walker, helicopter roles and all transitions. Personal gunfire while driving requires additional native integration. |
| iDroid and progression | In-headset map/Pause, tabs and basic navigation. | Support drops/strikes, helicopter requests, deployment, development, Mother Base, mission completion/retry, death and replacement transitions. |
| Presentation and feedback | TPP tracked arms, wrist ammo/equipment/Commands and floating native menus. | Spatial subtitles/damage/context feedback, all title/loading/cinematic cases, cuffs and action-specific hand animation. |
| Ground Zeroes | Independent native third-person stereo experiment; several native weapon/item actions exercised. | Player-head first person, tracked arms, weapon aim and wrist UI adapters; GZ-specific guard/rescue/mission interaction coverage. TPP offsets are not used as a substitute. |
| Showcase | An 80-second TPP/GZ field edit exists; additional native scope footage is recorded. | Finished expanded combined film with accurate overlays; never label missing gameplay as complete. |

Controls are editable in `mgs5vr-controls.ini`. The detailed mapping is in
[CONTROLS.md](CONTROLS.md); recorded scope/action coverage is in
[DUAL_GAME_SIM_COVERAGE.md](DUAL_GAME_SIM_COVERAGE.md).

Native guard behavior reference: Konami's
[tactics manual](https://mgstpp-app.konamionline.com/manual/pc/na/en/pc_12.html).
