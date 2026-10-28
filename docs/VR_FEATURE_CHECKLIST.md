# VR feature checklist

Controls inventory checked 27 October 2026; historical gameplay coverage below retains its original scope. [FIELD_FIXES.md](FIELD_FIXES.md) maps actual repairs to remaining work. Built does not mean every issue is fixed. Source paths, bounded simulator runs, and headset acceptance are different evidence; most interaction families remain partial.

1. **Tracked stereo and head movement (TPP):** implemented; broad transitions and headset motion remain open.
2. **Tracked hands, forearms and body rig:** implemented; physical fit and extremes remain open.
3. **Finger poses:** grip, trigger and supported touch sensors animate fingers; no individual finger tracking.
4. **Weapon grip:** right-hand ready/aim and left-hand support exist; selected firearm coverage only.
5. **Wrist UI:** equipment, Commands and weapon/status panels exist; some layers and fit remain unfinished.
6. **iDroid:** left cinematic contact/projector, ordinary stow/reopen, scene handoff and exit-notice repairs are implemented. Continuous stereo, forced unfinished lessons and physical fit remain unaccepted.
7. **Cabin and Continue:** physical cassette path reaches gameplay in current simulator evidence; headset acceptance open.
8. **Binoculars:** tracked stereo lens, equip/stow, zoom and mark paths exist; tutorial/intel coverage incomplete.
9. **Weapon scopes:** authored optics work for selected sights; not all weapons or modes.
10. **Locomotion:** walking, running, lean and button-driven stances are mapped; physical posture inference is absent.
11. **Turning and recenter:** smooth, optional snap and recenter paths exist; mounted comfort needs headset review.
12. **Fire and reload:** tracked muzzle path and native actions cover selected firearms; full weapon matrix incomplete.
13. **Throw and place:** selected grenade, smoke, C4 and decoy paths observed; full gadget coverage incomplete.
14. **Equipment picker:** four native categories, use and cancel exist; item outcomes are not all checked.
15. **CQC and melee:** native contextual routing and motion-contact code exist; enemy reaction and interaction coverage incomplete.
16. **Pickup, carry and Fulton:** native button routes exist; no physical body grab and full recovery path is not proven.
17. **Buddy commands:** Commands and selected D-Horse orders/distraction work; other buddy actions remain incomplete.
18. **D-Dog petting:** native contact response observed in simulator; headset and other animals remain open.
19. **Rat pickup:** native interaction path exists; current-build pickup/release is not verified.
20. **Horse:** basic mount, ride, orders and dismount have simulator observations; full transitions remain open.
21. **Vehicles:** truck entry, wheel steering and exit observed; other vehicles and mounted roles incomplete.
22. **Powered prosthetic arm:** left-hand Rocket Arm steering, flight display and normal return are implemented with scoped simulator evidence; impact, Blast Arm and physical acceptance remain.
23. **HUD and world cues:** wrist status and selected cues exist; captions, notifications and marker issues remain.
24. **Controls editor:** external and in-headset remapping exist. The spatial editor has contextual 3D controller feedback and confirmed live remapping; ACC, conflict rejection and automatic binocular mode passed a bounded simulator run. Physical-headset fit and broader mode coverage remain open.
25. **Native gamepad:** XInput/native-button mode and VR handoff exist; full context matrix not accepted.
26. **Ground Zeroes:** first-person stereo, tracked arms/aim, left-hand iDroid, forearm ammo HUD, physical binoculars and native acquisition/mark/clear now run in the simulator. Intel, terrain waypoints, remaining settings and full gameplay coverage are incomplete.
27. **Pause:** spatial stereo panel over paused gameplay is implemented, retaining live head/hand tracking. Wider context and physical acceptance remain. Prior wrist-panel reports describe the earlier design.
28. **Presentation toggle:** immersive VR and large-screen modes have a held-button switch.
29. **Settings:** launcher tuning/validation/backups and the headset editor exist. The headset gear panel exposes the registered gameplay settings alongside action/button/chord/axis binding edits; broader physical usability remains unaccepted.
30. **Training lessons:** ten recorded lessons sit beside interactive 3D controllers; cues use current bindings.
31. **Control coverage:** the checked October 27 development exports 105 semantic actions, ten axes and 71 gameplay settings across eight decoder contexts; the headset editor distinguishes ACC, launch cabin and iDroid presentation modes. Not every action is gameplay-tested, and an exported setting does not implement Ground Zeroes' missing native adapters.
32. **Field terminal:** 3D lesson playback, controller inspection and settings UI are implemented; product flow still receives QA.
33. **Install/update tools:** launcher install, update, backup and recovery paths exist; full user-environment matrix remains open.
34. **Runtime diagnostics:** native snapshots, captures, bounded cases and evidence sidecars exist; they do not replace visual review.

**No blanket acceptance is claimed for:** physical stand/crouch/prone inference, roomscale body following, hand-to-head NVG gesture, stance indicator, complete left-handed rig, full Ground Zeroes first-person VR, and exhaustive mission/weapon/item/buddy/vehicle support.

Sources: [historical feature register](../tools/gameplay_bot/catalogs/historical-features.json), [current execution plan](RELEASE_PLAN.md), [controls](CONTROLS.md), [gameplay gaps](GAMEPLAY_FEATURES.md), [controller rig](CONTROLLER_RIG.md), [system access](SYSTEMS_ACCESS.md), [simulator coverage](DUAL_GAME_SIM_COVERAGE.md), and [launcher](LAUNCHER.md). Main implementation: `src/xr_runtime.cpp`, `src/head_camera.cpp`, `src/controller_rig.cpp`, `src/controls.cpp`, `src/input_bridge.cpp`, `src/idroid_rig.cpp`, `src/menu_surface.cpp`.
