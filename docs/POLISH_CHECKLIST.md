# Full gameplay polish worklist

Requested scope: all gameplay and interactions, September 6, 2026. A checked
implementation still needs final-eye and physical evidence where applicable.
Use native game behavior and the player's owned assets; retain the left forearm
HUD, suppress flat world markers/reticles, and keep aiming in stereo. Optical
zoom remains reserved at the user's request.

| Area | Work and required playable check | Current gate |
| --- | --- | --- |
| Arms and visible gear | Correct skinning, wrist roll, support, reach, cuffs; preserve complete shadows | Standing sleeves and reproduced prone ground penetration improved in both-eye SIM and compact clips; other terrain, close garment/hand presentation, all poses/outfits and physical fit remain open |
| Weapons | Draw/stow, category swap, aim, fire, impact/obstruction, reload; each family and alternate action | Rifle/pistol partial; remaining families unproven |
| Loot and recovery | Automatic ammo pickup, held weapon swap, plants, materials, containers, pick up/set down/throw bodies, Fulton; distinguish taps from holds and show contextual feedback | Unproven in tracked VR |
| Items and combat | Throwables, placement, binocular marking, bionic arm, CQC, grab/interrogate/carry, Fulton, healing and consumables | Item selection and Phantom Cigar observed; broader interactions unproven |
| Locomotion | Walk, sprint, crouch, prone, crawl, dive, climb, ledges, ladders, cover and contextual traversal | Basic movement/stance partial |
| Buddies and transport | Call, commands, mount, ride, gallop, hide, fire/dismount; vehicle entry, driving, passenger and mounted roles, exit | Horse basics observed; vehicle play unproven |
| Wrist and menus | Readable left-arm status, equipment selection, map/iDroid, inventory, upgrades, missions, map markers, pause/back and dialogue | Forearm status and automatic iDroid/pause screen return observed in SIM; map zoom/tabs work. Every branch and spatial menu interaction remain open |
| Spatial feedback | Context actions, subtitles, damage, warnings, objectives and marked people; no flat aiming/destination clutter | Native action-icon layer moved beside forearm status; horse/Y prompt checked through mount/dismount. Other contexts, subtitles and damage remain open |
| Camera and audio | Stereo/culling/shadows, independent head motion, native listener orientation, comfort and tracking loss/recovery | SIM partial; physical gates open |
| Game states | Startup/save/continue, cinematics, death/retry, transitions, helicopter/ACC, loading, suspension and clean exit | Same-player menu return and manual override observed; complete cinematic, loading and player-replacement transitions unproven |
| Distribution | Asset-free build, documented simple controls, clean install/update/rollback, correct defaults and tests | Experimental build; polished release not accepted |

For each slice record the exact build, native bindings, both-eye results, relevant
input transitions, failure cases and a compact capture. Update this list only from
observed behavior. Keep incomplete categories open; one successful weapon or mount
does not establish the rest of the game.
