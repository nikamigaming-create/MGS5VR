# Release execution plan — 26 October 2026

### Ground Zeroes nightly publication - 27 October

The user requested the current nightly with Ground Zeroes and the accumulated VR work. The batch includes GZ first-person stereo, tracked hands/aim, left-hand iDroid, forearm HUD, physical recon and production owned-asset setup, alongside the TPP/cabin and contact repairs. The existing GitHub nightly workflow builds and verifies committed main. Three hardware-only tests remain local; the complete local 59-group run passed before publication work.

Published and anonymously download-verified: [nightly-2026-10-28-55b20f1ccc97](https://github.com/nikamigaming-create/MGS5VR/releases/tag/nightly-2026-10-28-55b20f1ccc97) from `55b20f1ccc979a19c741f479c191a6bf63b6e9e2`. All 56 hosted CPU-compatible CTest groups, Python checks and packaged installer checks passed. All 6 public assets and 231 manifest files verified; installer ZIP SHA-256 `3e3a9a75ca858ec8aec7d62793067307c2fa9123db8ee70c3a7f809ed776f462`. [Successful workflow](https://github.com/nikamigaming-create/MGS5VR/actions/runs/40631300775). The three GPU tests and native/simulator/headset sessions were not run by CI. Recommended release and local Play pin remain unchanged.

Next: finish successful physical pet filming, remaining applicable gameplay/settings work and the complete combined Field Kit showcase. Nightly publication does not close those existing acceptance gaps.


### Checked contact repairs and 4:23 combined working cut — 27 October

Development `6ef5532fb054d2ca0855c2d34264507e7495c1768d585e3790f85230ddf89011` is installed in both games. Physical animal interaction now runs in the native Lua transaction rather than XInput polling; the earlier simultaneous diagnostic had bones in 691/691 native queries but 56 missing-bone observations in the old consumer. The intermediate repair exposed an idle false pet from native skin/animal movement. The final repair requires raw controller movement and movement relative to head translation as well as rendered contact. Regression coverage preserves slow real strokes while rejecting animation-only contact, walking, head-only motion, jitter and rapid hits. All 59 CTest groups and installer checks passed.

Fresh TPP reached the ACC and Afghanistan field. A 702-sample idle observation remained at zero responses while outside contact. The attempted approach did not reach its predicted position predicate and D-Dog remained out of reach; a successful physical pet is still unfilmed. The first startup exited during initialization; a clean retry worked, with the original failure retained. These results do not certify the pet interaction or whole-game readiness.

The [combined private Field Kit working cut](../private/showcase/current-field-kit/MGS5VR-Field-Kit-working-cut.mp4) is now **4:23**, eleven chapters. It includes cabin interactions, clear field controls, paired iDroid/wipes, GZ recon/intel, live settings, and retained 26 October Rocket Arm steering/Blast Arm watchtower collapse. Original build scopes and audio are preserved. Full decode and representative layout review passed. It remains an incomplete all-feature showcase.

All temporary personal settings and tracking poses are restored, menus/inputs released and owned simulator/game/readers closed. Steam is retained; published play pin unchanged. Next: finish physical pet filming and remaining shared/game-specific gameplay/settings work, then complete the final feature video. GZ already has independent first-person VR; TPP-native powered arms/buddies are not claimed as GZ content.


### Clear field actions added; physical buddy stroke unresolved — 27 October

Retained `20261028T024923719888Z` on checked `19e51307a1a3` records equipment/commands open and release, equipped secondary ready/fire/reload, prone/crouch/stand and left-hand iDroid open/stow. Both ready, fire, reload and iDroid final eyes were reviewed. Ammunition changes 42→39→43 are observed; target hits/damage are not claimed. The combined private Field Kit working cut is now **3:28**, nine chapters, with a continuous 38.6-second field chapter, animated official controller lessons and original game audio. Full decode and representative layout review passed; the all-feature movie remains unfinished.

Physical D-Dog stroking failed in retained `025523260491Z`, `025844651644Z` and `031126539491Z` (28 October UTC): tracked hand contact/motion did not increment the native physical-response counter. The last take also records the dog walking out of reach. Generic motion-case pass is not pet success. Checked diagnostic development `41db3ae40e593fa2d023b6dabc93edd0850564e4be61c5391498918ad7b0e7cc` adds per-hand contact/reset state without changing thresholds; it is installed in TPP only. GZ retains checked `19e51307a1a3`. All 59 CTest groups and installer checks completed.

Next: use the ongoing TPP session to isolate and repair the physical-stroke interruption, then remaining shared settings/adapters and final feature filming/edit. Exact earlier field settings were restored; the active diagnostic eyepatch transaction is `private/showcase/clear-view-buddy-setting.json` and must be restored before session closure. Steam and published play pin remain unchanged.


### Ground Zeroes normal setup and animated controller lessons — 27 October

Checked development `19e51307a1a3af763b8b0b870421dda729069874ff1d5b190253175f795bc455` is installed in both games. Normal setup and Field Terminal now enable GZ's connected first-person rig and forearm HUD, with independent native adapters and correct GZ optic paths. The production importer selectively reads the player's GZS archives and imports the exact binocular model and four material maps. GZ updates preserve personal settings. Stale installer and launcher refusals have been removed.

All 59 CTest groups and installer checks passed. Actual owned GZ outputs match the assets used in prior simulator footage; TPP import still matches its model/material/shader identities. Both owned sessions remain closed, Steam and published pin unchanged. The 2:49 Field Kit working cut now contains animated official Quest Touch Plus instructions in the cabin, recon and intel chapters. Full video/audio decode and representative layout review passed; it is still the unfinished combined film.

Next: clear TPP field actions from the observed iDroid page, remaining shared gameplay adapters/settings and the complete feature showcase. Native GZ optic shader, visible glow-on and broad weapon/equipment/vehicle coverage remain incomplete. Installer checks do not extend gameplay or physical-headset acceptance.


### Ground Zeroes live recon visibility — 27 October

Checked development `50fca91f11de43814a3ef9a1839394b92ca57d25962227a73489bda79c1744ff` is installed in both games. GZ HUD Off previously left its native 79M marker visible (`20261028T020601947606Z`). The repaired native marker routing hides it in retained `021711220890Z` and restores it with Full in `021834570739Z`; both final eyes were reviewed. The separate GZ native glow material scope is connected, but visible glow-on is still unverified. Source actors, native fade state and personal controls are preserved.

All 59 CTest groups and installer checks passed. Exact original GZ controls bytes are restored, optic stowed, inputs/poses cleaned up and the owned game/simulator closed. Steam and the published play pin are unchanged. The single private Field Kit working cut now runs **2:49**, with eight chapters including a 12-second real HUD Off/Full comparison. It remains an incomplete film with source/build scopes retained in its edit indices.

Next: production GZ owned-asset import and remaining gameplay/control differences, clear TPP field footage, animated controller lessons and the complete combined showcase. Terrain marker interactions, independent world-enemy preference and visible glow-on remain unresolved coverage rather than claimed completion.


### Paired Field Kit working cut — 27 October

A single private [2:37 working cut](../private/showcase/current-field-kit/MGS5VR-Field-Kit-working-cut.mp4) now combines the cabin, a real TPP/GZ left-hand iDroid comparison with wipes, GZ recon/intel, live VR switching and body/eyepatch effects. Its [chapter index](../private/showcase/current-field-kit/index.html) identifies seven chapters. All 4,723 encoded frames and stereo audio decode; sampled motion/layout review is retained. This is explicitly not the finished all-feature film. Native source-eye footage and the 4.039-fps final-compositor VR switch remain separately identified. The recon chapter retains its preceding `fd053e` build scope; the new paired footage uses checked `050ae82a4832`.

Fresh GZ runs `20261028T012220370500Z` and `012347615433Z` show the left-hand handset/map and held rifle in both eyes. Clear-view TPP ACC iDroid `013412671686Z` shows open/hold/stow in both eyes. Personal eyepatch was temporarily off for that take and exact original controls bytes were restored. The later deployment sequence encountered a different remembered iDroid page and stopped at a mission acceptance dialog; cleanup cancelled/closed it. No mission was accepted. Both owned simulator sessions are closed and Steam retained.

Next: finish missing GZ native settings/adapters and remaining gameplay families, capture unobscured paired field actions, animate controller instruction insets and complete the final full-feature edit. Reopen TPP deployment from its observed current page rather than assuming MISSIONS root. Published build pin is unchanged.


### TPP cabin and shared-feature filming — 27 October

The same checked `050ae82a4832` development candidate is installed in TPP and GZ. Retained TPP `20261028T003108427231Z` records a continuous 77.33-second cabin sequence: cassette pickup/insertion/touch eject, either-hand camera prints and board attachment. Both final eyes were reviewed for cassette, camera print and board states. The private 75.7-second `tpp-cabin.mp4` chapter uses Field Kit graphics, official controller plates and original audio. It is not the finished combined film.

Field footage now includes equipped secondary fire (42 to 39), reload (39 to 43), prone/crouch/stand, left-hand iDroid open/stow, full-body on/off and eyepatch off/on. Both iDroid and body/eyepatch effect eyes were reviewed. The ordinary source-eye recorder excludes the controls panel; a separate 45-frame compositor take records VR off/on at measured 4.039 capture fps. Original personal values and the 300 ms binocular chord are restored. The editor recorder now supports the short compositor chapter and restores owned temporary file changes.

TPP recon remains incomplete: no target acquisition before an operator pose request timed out. The game kept rendering, a new connection recovered, and exact saved head/grip/aim poses were restored and read back. Retained failures distinguish script assumptions (wrong ammo slot/count and an early iDroid interval) from game behavior. Next: fresh matched GZ capture, paired side-by-side/wipe editing, then remaining feature adapters. Published pin and Steam remain unchanged.


### Ground Zeroes native binocular intel — 27 October

Checked development `050ae82a4832018d2832ce01c73a3697405addfcc622ae2eb1435488a75352a4` connects physical binocular X to the native mission event pump. Retained `20261028T002337220348Z` records the tower guard candidate and native request with matching identity. Both action eyes were reviewed. Local transcription of the captured process audio identifies the guard warning and instruction to avoid combat. This audio result is machine transcription, not direct listening. The earlier radio-UI hooks did not run outside authored binocular mode; their failed scope is retained.

All 59 CTest groups and installer checks passed. Inputs and temporary poses were restored, optic stowed, menus closed and owned GZ session closed with Steam retained. Published play pin is unchanged. A 16.6-second Field Kit recon chapter now exists privately with official controller plates, aspect-correct gameplay and original audio; the combined film remains unfinished. Next: deploy this checked candidate to TPP, capture the cabin and matching shared features, then assemble the paired Field Kit edit. Remaining GZ adapters and unreviewed gameplay contexts stay explicit in the ledger.


### Ground Zeroes native binocular acquisition, mark and clear — 27 October

Checked development `fd053ed8167ca89a9e3f2ec01b85b8035084024670de9ad7b781927f367064ca` is installed in GZ. The physical optic now drives the native character visibility job and native player marking. Retained `20261027T234007158128Z` shows the real tower guard at 79 m: 4× acquisition, A clear and right-trigger explicit re-mark. Both final eyes were reviewed at each state; the recorded source-eye take includes process audio. The earlier `5940c447d193` takes `232907` and `233026` failed to acquire because the adapter omitted the native optical range multiplier. Both failures are retained. The corrected adapter uses the same multiplier field as GZ's own binocular zoom setters, native collision results and native marking/clearing functions.

The current-player input ownership repair is deployed; these takes show no unsolicited optic stow. Both candidates passed all 59 CTest groups and installer checks. Inputs, head/controller poses and menus are restored, binoculars stowed and recorders/readers closed. Steam, TPP installation and published play pin are retained. The 18.77-second source video has a 0.80-second startup gap before the optical actions; its cadence ledger is retained. No complete movie, mission or physical-headset claim is made.

Next: GZ intel/radio and remaining shared controls/settings, then TPP/cabin review and the combined Field Kit film. Terrain waypoints, configurable native marker/glow behavior, production GZ owned-asset import and native optic materials remain unfinished.

### Ground Zeroes physical optic and native foot controls — 27 October

Checked development `274a81f4013bece41c10ae82329abad72be85df73e629d6b37328e219b7b27c6` is installed in GZ. Its native basic-action query enables shared foot controls and the physical binocular. The owned GZ model and decoded textures use the existing configurable optic paths. Both final eyes show right-hand equip (`225433`), the real ocular and lens (`225455`), 2×/4× zoom (`225818`), supporting-hand grip (`230129`) and the enlarged left-forearm ammo card (`225851`); full retained IDs are in the ledger. These runs also contain native source-eye recordings with process audio, distinct from final compositor stills. The complete Field Kit edit is unfinished.

The optic spontaneously stowed during a slow rendered frame before take `225557`, which is retained as a failed zoom attempt. The adapter's 150 ms player freshness threshold was tied to camera publication. A source-only follow-up now resolves input ownership from the current native player registry; pose freshness remains separate. That follow-up is not built/deployed yet. All 59 CTest groups and installer checks passed for the installed candidate.

Inputs and temporary poses are restored, menus closed and binoculars stowed; recording requests/readers are closed. Personal full-body setting remains 1. Steam, TPP installation and published play pin are retained. Next: deploy the ownership repair and finish remaining shared control behavior, GZ native enemy acquisition/marking/intel, then TPP/cabin review and the paired Field Kit video. GZ owned asset import currently uses a private local helper, and the native optic material shader is not yet ported. Whole-game and physical-headset completion remain unclaimed.

### Shared forearm cuff repair deployed in Ground Zeroes — 27 October

Checked development `a3c9a9eed06e0408da8fcd6a6a7c8579066595eae7ab7e87db8a4726357acc04` is installed in GZ. Native axial rim steps were collapsing to duplicate projected corners, preventing the closest Snake2 LOD from closing. The shared cap triangulator now collapses those corners while fitting the complete native rim and preserving native skin data. Both final eyes in retained `20261027T222812915905Z` show closed body-off sleeve ends without the previous jagged flaps.

Both posed eyes in `20261027T222919313085Z` retain the left-hand native handset/map. After native B closure, both held eyes in `20261027T223039155404Z` show the rifle without a floating reticle. This is a staged pose/draw check, not a new shot or continuous-motion claim. All 59 CTest groups and installer checks passed. Full-body controls were restored byte-for-byte, poses restored, inputs released and menus closed. TPP installation and the published pin remain unchanged.

Next: GZ native action/travel and physical binocular integration, small forearm ammo-card readability and remaining shared controls, followed by TPP/cabin review and the combined Field Kit movie. The shared cuff repair still needs fresh TPP visual review; whole-game and physical-headset completion remain unclaimed.

### Ground Zeroes body and handset fit — 27 October

Checked development `6b1a1a9405aabb2407a619e0b27548c3f9098d668830501f7ca6a44d524c6e8e` is installed in GZ. The native anatomical eye join, fixed-length torso fit when reachable, body-off shoulder placement and clavicle reach improve the left-hand iDroid fit. Native legs remain untouched. Both final eyes in retained `20261027T220710049689Z` show the main map at readable scale; outer labels are still clipped at the staged left-FOV edge. Native B closure was also observed.

Retained `20261027T221020345667Z` shows the rifle in both held eyes with no flat reticle. The preceding shorter hold `20261027T220854419847Z` showed no rifle; cause remains unconfirmed, and this batch does not establish a confirmed shot. Body-off wrist pose `20261027T221121566780Z` exposes jagged sleeve cut ends in both eyes. The native mesh reproduces a shared triangulation failure at its closest LOD: separate axial vertices coincide when flattened. That cuff repair is source-only and building.

All 59 CTest groups and installer checks passed for the installed candidate. Full-body controls were restored byte-for-byte, temporary poses restored, inputs released and iDroid closed. TPP installation and published pin remain unchanged. Next: finish cuff geometry, remaining common GZ controls and physical binoculars, then the combined Field Kit movie. Whole-game and physical-headset completion remain unclaimed.

### Ground Zeroes native left-hand iDroid connected — 27 October

Checked development `a1228d1f3e8b7655aece116ab2ae4741a1b49b5b316539214bf58d7b5dcb014c` is installed in GZ. The exact native variant-3 attachment now publishes from the left wrist during the player's skin call. The existing measured left-hand cup uses GZ's matching original finger lengths; handset and native map share that solved placement. GZ menu navigation keeps its own native controls. The preceding `4fc749434228` candidate exposed a TPP-only input acknowledgment blocking B; this is repaired.

Both final eyes in retained `20261027T215120784175Z` show handset/map in the staged straight-on pose, but much of the map is clipped. The native shoulder reach clamps the device close to the eyes; final fit remains unresolved. Both final eyes in `20261027T215140842885Z` and `20261027T215728452935Z` show clean native closure, including after reopening. Inputs and temporary poses are restored, and iDroid is closed. Both candidates passed 59 CTest groups and installer checks. TPP installation and published play pin remain unchanged.

Next: the source-only anatomical eye join, fixed-length spine/shoulder fit and clavicle reach batch is building. Finish the display fit and remaining shared gameplay controls/physical binoculars, then the combined Field Kit film. No complete iDroid usability, whole-game, movie or physical-headset completion is claimed.

### Ground Zeroes body-off, forearm HUD and stowed rifle — 27 October

Checked development `d127d2129ea4787f3eef2eef81a3b450bf18f80778c39cb89a823503cbe7c3b2` is installed in GZ. Its native body visibility preserves the arm groups and uses verified Snake2/Snake6 forearm buffers when full body is off. Native ammo/status now uses the left forearm; flat firearm reticles are suppressed while actual optics remain. Stowed primary rifle geometry is concealed through its native named group and restored when raised.

Both final eyes in retained `20261027T213048323509Z` show the body-off forearms, compact left ammo card and absence of the intrusive rifle stock. Both held eyes in `20261027T213236942029Z` show the rifle restored without a floating crosshair. Forearm cut ends remain angular and moving HUD readability is unreviewed. Earlier `5674b2f7b476` wrist-up footage exposed an oversized card; the earlier initial ready run lacked a rifle, with cause undiagnosed. These earlier observations remain in the ledger.

Both candidates passed 59 CTest groups and installer checks. Temporary full-body setting and controller poses were restored, inputs released, and menus closed. TPP installation and published pin are unchanged. Next: complete the left-hand iDroid handset/display and remaining shared controls/body fitting, then the requested combined Field Kit movie. Full-game, continuous-motion and physical-headset completion are not claimed.

### Ground Zeroes menus and native firing connected — 27 October

Checked development `8c4f2eb06c1e639a395b83b73f7d58df0de4fa201166bc09ddd239bb021638ea` is installed in GZ. Its native aim job uses a private camera basis from the last successfully published weapon muzzle, preserving native collision and recoil. Retained `20261027T203054611119Z` used saved grip/trigger controls: both held final eyes show the rifle and ammunition decreased from 30/210 to 28/210. The runtime observed the aim adapter. Impact alignment and moving firing remain unreviewed. The batch also implements proximity-based support contact and a joined left forearm panel pose; these are not yet visually exercised or connected to native HUD routing.

The preceding checked `55dbb4ccf1beaad972ab93104d7c1299695bf669761fd506adaf4b073ab0d9ef` repairs invisible Pause through the native spatial canvas. Retained `20261027T202211258666Z` (Pause) and `20261027T202256786902Z` (iDroid) show their native menus in both eyes. B returns to gameplay; both eyes after iDroid closure were reviewed in `20261027T202331784896Z`. Retained `20261027T202134636398Z` shows native muzzle-based wrist alignment and sharp arms/rifle. iDroid is an interim spatial canvas with its native right-hand handset, not the required physical left-hand implementation. Free fingers now use controller curl, with native firearm contact retained; continuous articulation remains unreviewed.

Both batches passed 59 CTest groups and installer checks. Inputs were released and test menus closed. TPP installation and published play pin remain unchanged. Next: native HUD/reticle routing, support and moving shot alignment, left-hand handset/projection, body-off/full-body fit and physical binoculars, then the complete combined Field Kit film. No whole-game, finished-movie or physical-headset completion is claimed.

### Ground Zeroes tracked arms, weapon-ready control and focus — 27 October

Checked development `e30550f3367f1b43fcde05deea201beec23cc118f6e817527537723f9ff1128e` is installed in GZ. Native arms/attachments now use the independently mapped skin publisher and shared fixed-length IK. GZ's verified Pause/iDroid owners route saved controls without enabling TPP hooks. Retained `20261027T200728345658Z` shows the saved right grip raising the rifle; both held final eyes were reviewed and the former close-up DOF blur is absent. No shot was fired. Native camera axes remain available pending GZ travel-action mapping.

Retained `20261027T200801636407Z` exposed invisible Pause: its native kind became 1 but the reviewed left final eye kept the world without the menu. B closed it and the native kind returned to 0. The next source batch routes GZ native menus onto the existing spatial canvas, preserves native menu navigation, aligns the tracked wrist through GZ's own muzzle socket and adds shared free-finger articulation. That batch is not yet deployed or visually checked.

All 59 CTest groups and installer checks passed for the installed candidate. TPP installation and the published play pin are unchanged. Next: finish the menu/weapon batch, then connect actual firing/support contact, left-hand iDroid and wrist UI. Full-body/body-off behavior, physical binoculars, remaining common interactions and the combined Field Kit movie remain unfinished. These simulator endpoints do not establish full-game or physical-headset completion.

### Ground Zeroes first-person head obstruction repaired — 27 October

Checked development `8b563b1358d5c6783acd89c3ccc248e7d064282a20272b43ac8d09f2a597cd5c` is deployed to GZ. Its native named head group is hidden only for the matching VR player; the native visibility publisher remains authoritative on VR exit. Retained on/off runs `20261027T193530760583Z` and `20261027T193559967649Z` were reviewed in both final eyes: the prior hair obstruction is absent in first person, and Snake's head returns in the native third-person view. These endpoints do not establish continuous motion or physical headset behavior. All 59 CTest groups and installer checks passed.

Next: connect the independently mapped GZ skin publisher and named arm/finger hierarchy to tracked hands, retaining native skeleton lengths and attachments. The arm adapter is being implemented; tracked weapons, spatial UI, shared interactions and the combined Field Kit movie remain unfinished. TPP installation and published play pin are unchanged.

### Ground Zeroes now reaches native first person — 27 October

Checked development `8a3b9a58ea984fece51ebd1e366a062157b2aeb517045b975c7f3542b168b3df` is deployed to GZ. Its adapter resolves the selected player camera and the body's named HEAD matrix, converts that published world transform through the actual body root, and supplies the shared head camera. Retained `20261027T192058758856Z` was reviewed in both final compositor eyes: the viewpoint is at Snake's head, with no player ahead of the camera. Hair still intersects the upper edge. Native flat HUD, spotlight bloom, tracked arms/weapon and complete interactions remain open. This stationary simulator observation does not establish motion, transitions or headset completion.

The same checked batch passed 59 CTest groups and installer checks. Runtime confirms the independent desktop preview is 1280 x 720 while GZ retains its complete native 3840 x 2160 canvas. TPP's installed build and the published pin are unchanged. Next: the reproduced head/hair intrusion is being repaired through GZ's own named head visibility group and native publisher, preserving ordinary visibility on VR exit. That repair is source-only. GZ's animation driver, 138-bone pose and native skin publication have also been identified for the shared rig connection. The requested combined Field Kit movie remains unfinished.

### Ground Zeroes live display repaired — 27 October

GZ's native focus-loss branch was suspending scene presentation when the simulator opened. The exact-build adapter now preserves native focus/input state while suppressing that display-suspension request. Checked development DLL `ffc7bbf0696f313e1b95ef0a0d013cb8acb8ab308ca8b93be33318c69b23725d` passed 59 CTest groups and installer checks and is deployed to GZ. Saved Continue and Start Mission reached live night gameplay. Retained `20261027T185839163442Z` shows the personal VR toggle switching to native stereo; both final compositor eyes were reviewed. This is still a third-person camera with heavy spotlight bloom and native flat HUD. First-person head, tracked rig and shared interactions remain unfinished.

The GZ launcher now owns its exact launcher/game child pair and restores the per-game simulator lease during cleanup. Bounded GZ controls use saved bindings without TPP memory addresses. The 1280 x 720 preview correction is source-only and awaits the next meaningful build batch. TPP remains installed at `9d646890a2b4`; the published `play/` pin remains `b627c7cacc9a`. Current work is the GZ native head/skin connection, followed by shared rig/UI and the requested combined Field Kit video. No movie, whole-game or physical-headset completion is claimed.
### First public nightly verified - 27 October

Nightly `nightly-2026-10-27-53f6bf541ef3` is public from committed source
`53f6bf541ef37b3e862d470a5bec0318eddcd343`:
[nightly-2026-10-27-53f6bf541ef3](https://github.com/nikamigaming-create/MGS5VR/releases/tag/nightly-2026-10-27-53f6bf541ef3).
The Windows build, 56 hardware-independent CTest groups, nightly contracts and
packaged installer checks passed in [the publication run](https://github.com/nikamigaming-create/MGS5VR/actions/runs/40564021306).
All six public assets were downloaded without authentication and hash-verified;
the installation archive's 231 manifest files verified. Archive SHA-256:
`1c3fa35d600047db970dd9a253b14a5d7c07e18e0a7e81cfa76ca1a4cf762251`.
Hosted DLL SHA-256: `61a323d8b1b527e5087b51a4d5d198fef58b98a03071ff32b0adebd1de6ef913`.

The first attempt stopped before publication because CTest wrote its XML relative
to the build directory. The workflow now supplies an absolute report path and
requires the report to exist. The corrected complete publication passed. A plan
check on the published commit returned `publish=false`; the repeat dispatch is
[recorded here](https://github.com/nikamigaming-create/MGS5VR/actions/runs/40565064013).

Daily publication is active at 10:37 UTC, with manual dispatch available. New or
changed blocker snapshots stop the channel; the accepted existing limitations
are disclosed, not resolved. The reviewed October 19.1 release and its asset
identities are unchanged. This task used an isolated release-automation branch
and did not alter the primary checkout, installed development or local Play pin.
The two GPU-only tests, simulator, physical headset and full-game acceptance
remain outside this CI result. Existing gameplay evidence keeps its original
candidate scope; see [Nightly builds](NIGHTLIES.md).

Next release action: commit and push reviewed development batches normally;
the daily job considers new main commits. Keep new failures in the catalog and
hold publication when needed. Reviewed-release promotion remains separate.

### Diagonal cabin take failed at framing guard — 27 October

Retained `20261027T174300006026Z` on installed development DLL `9d646890a2b40468a556054f3643da71b79c595650fa7ed35904c28a2a811e2a` observed cassette pickup, release and recovery, then stopped before insertion: the temporary diagonal viewpoint reached distances of 0.509 and 0.454 m but the measured slot-face dot products were -0.310 and -0.256, below the 0.5 guard. This is a staging failure, not a diagnosed cassette gameplay defect. The observed carrier normal was approximately [0.848, 0.259, 0.463], mostly horizontal; the temporary offset followed the prior head direction instead of the carrier normal, putting the observer behind its face. A normal-relative offset is an unverified staging lead. Camera and photo actions were not reached. The partial source-eye take recorded 557 accepted frames, 34 dropped/repeated samples (10 missed cadence, 24 queue drops); the audio capture reports no gaps, but sound was not auditioned. It is not the requested movie. Temporary staging was saved privately and removed; inputs were released and owned simulator/game closed with Steam retained. No build, pin, or physical-headset acceptance changed. Do not rerun this unchanged viewpoint.


### Latest cabin recording sequence review — 27 October

In `20261027T172437888879Z`, the 61 missing encoded slots occur in 30 accepted-sample gaps. Thirty-six of those missing slots are in gaps whose endpoint source and image sequence numbers do not advance. The largest gap, 0.215 seconds, begins at 62.327 seconds; the next largest begins at 62.541 seconds. Several long gaps cluster around 60–63 seconds, after the left-camera print; two composited-eye capture requests run at 60.382–60.739 and 60.935–61.259 seconds. Endpoint observations do not show intermediate transactions, prove a stall, establish causation, or show that capturing the final eyes explains later gaps. Separate per-frame scheduling/copy timing from source production before changing behavior. No new game run or headset acceptance is claimed.


### Latest cabin recording drop breakdown — 27 October

Retained `20261027T172437888879Z` reports 61 missed samples: 48 missed cadence slots, three encoder-queue drops and ten staging-busy samples, with no staging discards. Cadence is evaluated on recorder frame calls after ready GPU frames are copied; these counters do not distinguish delayed render calls from time spent within the recorder, nor prove a root cause. Enlarging the encoder queue alone cannot address the 48 unscheduled cadence slots in this take. Investigate the frame-call/copy timeline before changing scheduling; retain the earlier acceptance limits and do not infer a gameplay or headset pass.


### Integrated cabin recording on checked candidate — 27 October

Retained run `artifacts/bot/runs/20261027T172437888879Z` on installed development DLL `9d646890a2b40468a556054f3643da71b79c595650fa7ed35904c28a2a811e2a` observed cassette pickup, seating, ejection and recovery, right- and left-hand camera shutter actions, and photo placement. Both final-eye board stills show the new cabin-wall photo. Both final-eye cassette-ejection stills were subsequently reviewed: the player and tape are visible, but a large dark forearm end obscures the hand. This does not establish the crop cause, natural grip or clear insertion. Sampled source-eye frames leave hand contact and cassette action poorly framed; this is not continuous stereo acceptance. The 76-second source-eye recording has an AAC game-audio track with nonzero signal (mean -23.1 dB, peak -11.5 dB), not auditioned; it has 61 dropped and 61 repeated frames, with a 0.215-second maximum accepted-sample gap. Body and eyepatch toggles are absent. Reject this take as the requested finished movie. The change in gap and drops versus prior takes does not establish recorder performance because conditions were not controlled.

The first run `20261027T172304096887Z` stopped before interaction when the native anchor was absent. A bounded wait for anchor publication was exercised in the successful temporary route and is retained in the workbench; the existing bot suite passes. Test photos were backed up with matching hashes, and retained same-session cleanup runs `20261027T172848508279Z` and `20261027T172926154386Z` removed exactly their prints and library entries, preserving the two original identities. Temporary film and cleanup edits were removed, inputs released, and the owned game and simulator closed; Steam remains running. The release pin is unchanged. No physical Quest pass, authentic lid-motion verification, clear moving contact, complete toggle showcase, or universal interaction acceptance is claimed. Next: address the identified framing and cadence problems, audition sound, and verify remaining interactions without replacing the established controls; user headset testing remains separate.


### Recorder buffer reuse candidate — 27 October

The latest cabin take had 15 encoder-queue drops and copied 2368 x 2576 RGBA frames. A standalone same-size UCRT/RAM microbenchmark found median allocation-plus-copy 4.024 ms versus 0.821 ms with a reused buffer; it does not measure GPU readback, encoder contention or in-game performance. The native recorder now retains at most four idle CPU frame buffers, reusing only a matching size after the encoder has copied the pixels into its own Media Foundation buffer. Dropped CPU copies return to the pool, and an allocation failure unmaps and accounts for the staging sample. Eye dimensions, source ordering, timestamps and encoded repeat accounting are unchanged. New tests check buffer size, exclusive ownership and concurrent reuse.

The first build failed on a test argument conversion warning; after correcting it, a full check found the new header absent from the source catalog, which was then mapped. The final development build passed all 58 test groups and installer checks. Development DLL `9d646890a2b40468a556054f3643da71b79c595650fa7ed35904c28a2a811e2a` is installed; published play remains pinned to `b627c7cacc9a`. No simulator or physical-headset run has exercised this candidate, so no improvement in actual cadence or video quality is claimed. The earlier cabin interaction evidence applies only to its recorded candidate. Clear hand contact, true lid motion, full showcase and headset acceptance remain open.


### Latest take gap timing — 27 October

In the retained combined take, the 0.523-second accepted-sample gap occurred before the first filmed chapter; a separate 0.274-second gap occurred between the tape-on-seat and tape-seated markers. Trimming the startup alone therefore would not remove all visible cadence problems. `artifacts/bot/runs/20261027T164745284337Z/capture-gap-phase-analysis.json` records the timing; markers do not establish exact hand-motion boundaries or the cause. No final-eye cadence, gameplay repair or finished movie is inferred.


### Showcase camera framing correction — 27 October

The completed combined take captured dark cabin-seat subjects because its scripted camera target used the controller/head orientation rather than the actual held-camera lens orientation. For film runs, the workbench now waits through the existing pickup easing, derives the current camera-to-controller attachment and aims the camera at the native cabin board for both hands. A previous retained staged take using the same attachment/lens-aware approach produced cabin-wall images, but this integration has not been rerun. Python compilation and the existing bot contract suite passed; these do not prove camera pixels, natural grasp, continuous stereo, audio, cadence or physical Quest behavior. This is showcase staging, not a gameplay repair or completed movie.


### Combined cabin take and test cleanup — 27 October

On development DLL `aedd2a4c42ed322cfbf04690288c950b20e7572cd2369e53f487dbd3d470b78f`, retained run `artifacts/bot/runs/20261027T164745284337Z` observed cassette pickup, insertion, eject and recovery, both-hand camera shutters and newly printed photo placement. The corrected open-hand left-camera approach succeeded; the two existing print IDs and room-local positions were unchanged in the observed start and end states. The open player and placed photo were reviewed in both final-eye snapshots. The sample contact-sheet review still shows obstructed hand contact and dark, poorly composed camera subjects; these results are not continuous two-eye contact proof. The pre-compositor recording carries AAC audio with mean -23.1 dB, but it was not auditioned. It reports 23 dropped source samples, 22 encoded repeats and a 0.523-second maximum accepted-sample gap. Body/eyepatch toggles are not demonstrated, and this is rejected as the finished movie.

Both test PNGs were privately backed up with matching hashes. In `artifacts/bot/runs/20261027T165156081291Z`, exact test prints 14 and 15 were removed but digital cleanup stopped at the camera delete hover. After the owned session restarted, `20261027T165455483354Z` stopped before modifying the collection because title prop mode was not ready; same-session `20261027T165632449221Z` confirmed and removed exactly the two test gallery IDs. The last observed collection contains the two original gallery IDs and prints at their observed prior positions. Temporary filming and cleanup changes were removed, test inputs released, game and simulator closed, Steam retained. No physical Quest, new gameplay repair or completed showcase is claimed.


### Displaced loose print restored — 27 October

Retained `artifacts/bot/runs/20261027T164143753479Z` returned the specific loose print 12 / photo 13437590506521, moved by the oriented take, through tracked pickup and release. The observed final room-local position differs from the prior position by about 0.29 mm and the normalized orientation matches to displayed precision. Both existing paper IDs and both gallery IDs remain. Both final-eye stills were reviewed but the print is obscured by the forearm, so the position result is telemetry, not visual contact proof. The first attempt `20261027T164004914011Z` stopped before touching the collection while the title native menu was open; the same-session retry entered prop mode. Temporary restoration code was removed; inputs released, game and simulator closed, Steam retained. This is cleanup, not a gameplay or physical-headset acceptance pass.


### Cabin capture-gap review — 27 October

In the retained oriented take, the source-eye ledger has ten accepted-sample gaps over 120 ms. The largest (0.590 seconds) spans five source-sequence advances and overlaps one operator capture event; the next (0.433 seconds) spans 20 source and image advances with no overlapping capture event. Several shorter gaps have no endpoint sequence advance. These are endpoint observations, not proof of intermediate GPU execution or of capture causing the gap. The measurements in `artifacts/bot/runs/20261027T162149228010Z/capture-gap-analysis.json` show that moving all eye checkpoints alone cannot be assumed to solve cadence. A controlled source/recording comparison is needed before changing the recorder. No gameplay repair or new movie is claimed.


### Left-hand camera approach correction — 27 October

The failed oriented take observed the scripted left hand acquiring loose photo 12 while grip was held during travel to the camera. The existing workbench route now releases input before that approach, moves the left hand to the camera with grip open, and grips at contact. It stops if a paper or marker is acquired there rather than moving it as though it were the camera. Python compilation and the existing gameplay bot contract suite passed; those checks do not exercise this route in the game. This is a demonstration-route correction, not a gameplay repair or a finished movie. The next integrated take must verify actual pickup, user-print preservation, clear contact, audio and cadence in the game; physical Quest remains unverified.


### Test-image cleanup — 27 October

Retained same-session retry `artifacts/bot/runs/20261027T163331720348Z` removed the exact backed-up digital test image 13437591790496 through the camera confirmation flow. The observed library retains the two previous image IDs, and the observed paper IDs 12 and 13 remain; loose print 12 remains at its previously documented displaced position. The earlier retry `20261027T163143242615Z` stopped before touching the gallery because the title native menu was still open; the corrected route opened prop mode in the same session. The prior cleanup's camera moved during pickup easing after the aim was sampled, a plausible reason its button hover missed but not proof of the consumed ray. Waiting for the camera to settle let this cleanup reach and confirm the exact control. Temporary cleanup code was removed, inputs released, game and simulator closed, and Steam retained. No gameplay fix, movie acceptance or physical Quest pass is claimed.


### Oriented cabin take and partial cleanup — 27 October

On development DLL `aedd2a4c42ed322cfbf04690288c950b20e7572cd2369e53f487dbd3d470b78f`, retained run `artifacts/bot/runs/20261027T162149228010Z` observed cassette pickup, insertion and ejection plus one right-hand camera photo. Two temporary observer corrections aligned the open carrier to a measured face-normal dot of 0.796 at 0.321 m; both final-eye ejection stills show its face more clearly. Sampled source-eye motion still obscures hand contact. The take failed when the scripted left hand acquired existing loose photo 12 instead of the camera; its last observed room-local position changed from [0.473453, -2.2112, -1.08705] to [0.513061, -2.2112, -1.23604]. This is a route collision, not a proven camera pickup defect. The pre-compositor recording has audio, 119 dropped/repeated samples and a 0.590-second largest source gap; audio was not auditioned. The take is rejected as the requested movie and does not verify physical Quest or continuous final-eye motion.

One newly created image was backed up with matching SHA-256. Retained cleanup `artifacts/bot/runs/20261027T162624724008Z` removed only test print 14; its digital photo 13437591790496 remains because the delete-control hover was not reached. The last observed library contains that test image and the two existing images; both existing print IDs remain, with photo 12 displaced as above. The separate `studio-board` inspection `20261027T162423799609Z` failed because its fixed expected note and Polaroid were absent; it is not a reload verdict. The temporary film and cleanup changes are saved privately and removed from source. Inputs released, game and simulator closed, Steam retained. Next preserve and tidy that exact test image via verified camera controls, avoid holding grip while approaching unrelated prints, and produce a readable complete take; no additional gameplay fix is inferred.


### WM-R55 lid reference — 27 October

A [photo of an open WM-R55](https://www.ebay.com/p/21011535430) shows the actual empty compartment and open door; a local reference copy is retained privately. It is one perspective and does not measure hinge travel or establish the loading path. A [WM-R55 video](https://www.youtube.com/watch?v=hh8WE7Wl9bM) was identified but could not be retrieved for review (HTTP 403), so it is not evidence of the motion. Compare a clear in-game insertion against an accessible moving reference before changing the existing guide or lid. The sleeve cap visible in the body-off cabin still also remains a visual issue; its cause has not been isolated.


### Source-eye cadence check - 27 October

In `artifacts/bot/runs/20261027T160026917677Z`, the two largest accepted-sample gaps were 0.654 and 0.563 seconds. The closest recorded final-eye capture events were about 2.84 and 2.83 seconds from their midpoints, respectively. One gap preceded the first chapter; the other fell in the tape sequence. The recording ledger reports 34 missed cadence slots, 16 queue drops, eight staging-busy samples and one staging discard. This does not identify the underlying cause or prove capture calls had no indirect effect; simply moving stereo snapshots is not a justified complete fix. The take remains rejected and physical-headset acceptance remains open.


### Source-eye cabin take and cleanup - 27 October

Retained `artifacts/bot/runs/20261027T160026917677Z` on development DLL `aedd2a4c42ed322cfbf04690288c950b20e7572cd2369e53f487dbd3d470b78f` completed scripted tape, camera and photo-placement states. Separate final-eye endpoint captures were reviewed: the placed photo is visible in both eyes, but cassette/camera hand contact is obscured. The source-eye recording has AAC audio with nonzero signal, 59 dropped and 58 repeated samples, and a longest accepted-sample gap of 0.654 seconds; audio was not auditioned. The take is rejected as the requested finished movie, is not continuous final-eye proof, and does not show body or eyepatch toggles. Temporary source-film staging was removed after recording and its local patch preserved privately. In the same live session, retained cleanup `artifacts/bot/runs/20261027T160532234306Z` removed only the two earlier test prints and their digital entries; the final observed collection had the two newly printed photos and no notes. Test images were privately backed up and hash-verified. One-time cleanup code was removed; the game and simulator closed and Steam retained. Held-grip deletion, board layering under overlap and physical Quest remain unverified on this candidate.


### Cabin board paper layering - 27 October

Retained run `artifacts/bot/runs/20261027T145943690455Z` placed two prints at nearly the same board pose, within about 0.215 mm, although each print has nonzero thickness. Pinning a sheet now preserves its chosen board coordinates and positions it just in front of overlapping pinned sheets; non-overlapping sheets keep the original depth. Both-hand mixed photo/note and separated-placement checks passed, as did all 58 test groups and installer checks. Development DLL `aedd2a4c42ed322cfbf04690288c950b20e7572cd2369e53f487dbd3d470b78f` is installed; published play remains pinned. This does not retroactively move existing pinned sheets and has not been tested in the simulator or Quest. It does not establish the cause of the older gray print. Large stacks, grip presentation, the completed movie and physical-headset acceptance remain open.


### Test-photo cleanup completed - 27 October

Retained `artifacts/bot/runs/20261027T154906297397Z` used the in-game confirmation flow to remove the remaining five identified older test prints and eight older test-photo library entries. Together with the earlier three-print cleanup, the final observed game state contains only the two newest test prints and their two digital entries, with no notes. All ten test PNGs were backed up and hash-verified privately before cleanup. Each removal checked exact identities; the holding grip was released before final confirmation. This is cleanup evidence, not a live test of the new held-grip re-grab fix. One-time cleanup code was removed, the game and simulator were closed, and Steam retained. Persistence after restart, actual storage mapping, headset acceptance and the requested movie remain open.


### Cabin paper deletion re-grab fix - 27 October

In retained cleanup run `artifacts/bot/runs/20261027T153735536542Z`, deleting a held test print while grip remained down immediately acquired neighboring prints. Cabin paper deletion now requires the hand that held the deleted paper to release before it can pick up another object. The requirement survives tracking loss and clears only on a tracked release; normal fresh-grip pickup remains available. Checks cover both hands, held grip, tracking loss and recovery, and release/re-grip. All 58 groups and installer checks passed. Development DLL `91869f7f27103cf8133b12415160d15d625e60f4fed57d0f1208107d7c223dc8` is installed; published play remains pinned. The held-grip fix has not been tested in the simulator or physical Quest. The editor-open failure, partial photo cleanup, old gray print, hand presentation and requested movie remain open.


### Partial test-photo cleanup - 27 October

All ten identified test-created PNGs were copied to a private local backup and verified before cleanup. The first attempt `artifacts/bot/runs/20261027T153614496312Z` stopped before touching a print because title prop mode was not ready. In the same live session, retained `artifacts/bot/runs/20261027T153735536542Z` removed three older physical test prints through in-game confirmation. It stopped when the fourth print was held but its editor did not open. At the last observation, seven physical prints remained, including the two newest; all ten digital library entries remained. The fourth print was subsequently released and may have moved. No digital deletion was attempted. One-time cleanup code was removed; the game and simulator closed and Steam retained. Cleanup remains partial, and photo storage mapping, the gray old print, the requested movie and headset acceptance remain open.


### Test-created note cleanup - 27 October

The specific note created by retained run `20261027T151753032724Z` was removed through the in-game confirmation path in retained `artifacts/bot/runs/20261027T153047221916Z`. The note ID and text were checked before interaction; the observed other paper identities and digital library were unchanged across the cleanup. A first attempt `artifacts/bot/runs/20261027T152836609861Z` stopped before touching the note because title prop mode was not ready; the same live session was used for the successful attempt. One-time cleanup code was removed. The game and simulator were closed, Steam retained. This is cleanup evidence only, not photo, grip, movie or headset acceptance.


### Test-note cleanup ray review - 27 October

In retained run `20261027T151753032724Z`, the right-hand aim command at 99.563 seconds geometrically intersects the front of the test note at the nearby 99.750-second observation, about 10 mm from its center. Camera and marker were not held and no camera control was hovered at that observation. These are time-neighbor samples, not an atomic input/pose join: the consumed aim, trigger edge and moving note pose are not established. The failed cleanup remains unresolved and its test note may remain. No gameplay change is justified by this calculation alone.


### Cabin integrated check and cleanup failure - 27 October

Retained run `artifacts/bot/runs/20261027T151753032724Z` on development DLL `a1174b19694f` observed tape pickup, seating and ejection, camera photos with both hands, note typing, marker strokes with both hands, and new-photo board placement. Both-eye board stills show the new print's image in a clear position; an older gray print remains, and the marker grip is obscured. The run failed afterward: the test-created note was acquired for disposal, but its editor did not open, so the note may remain. The experimental cleanup was removed after the run. The workbench now refuses to rewrite pre-existing notes and checks that a newly created note is reacquired before typing; that guard was exercised on the run, but its refusal case was not exercised in-game. A source-eye/audio segment was captured with 19 dropped and 19 repeated samples; this is diagnostic footage, includes note work, and is not the requested showcase. The game and simulator were closed and Steam retained; game storage mapping, existing gray print, final movie and physical Quest remain open.


### Cabin take and test-note cleanup - 27 October

Retained run `artifacts/bot/runs/20261027T150737725386Z` removed the exact pinned test note ID 5 with text TAPE through its in-game confirmation before recording. It observed tape pickup, seating, ejection and recovery and camera photos from both hands, but failed before photo placement when the compositor capture timed out. The source-eye/audio segment completed with 19 dropped samples and 18 repeats. Sampled frames remain steep and obscure the hand/object contact; the take is rejected as the finished movie. The experimental view and one-time cleanup code were removed after the run; no gameplay source changed. The game and owned simulator were closed, Steam retained, and game-visible studio storage mapping remains unverified. Physical headset and the requested movie remain open.


### Cabin showcase recording and photo overlap - 27 October

Retained run `artifacts/bot/runs/20261027T145943690455Z` completed scripted tape, camera and new-photo placement states on development DLL `a1174b19694f`. Its source-eye recording has non-silent AAC audio, 18 dropped samples and 18 repeated frames; audio content was not auditioned. Separate compositor chapters measured only 3.28-3.99 captures per second. Source-eye visual review found steep framing, obscured grips and a previous test note visible, so this is rejected as the requested movie. The newly placed photo and an older pinned photo occupied nearly the same board pose, making the gray board result ambiguous. The workbench now selects an unoccupied board position or stops; replay of recorded state finds clear candidates, but the change has not been run. The owned session closed and Steam remained. Game studio storage mapping remains uncertain; test-created photos and note may remain in the game-visible store. No physical-headset acceptance.


### Cabin recording route correction - 27 October

The existing workbench commands ignored `--record`. They now start the existing source-eye/process-audio recorder after native workbench readiness and before interactions, with the recorder's first-frame gate and normal cleanup. This is a capture-route correction, not a gameplay fix or completed movie. Python compilation and gameplay_bot_contracts passed; title-cabin recording, actual cadence, sound content, image framing and headset acceptance remain unverified. The separate compositor chapters remain silent.


### Cabin photo source check - 27 October

The preserved PNG for the older photo placed in `20261027T144612968660Z` visibly contains the cassette player, rather than a uniform gray source image. Both board-eye checkpoints show a gray rectangle at the placed photo. The evidence narrows the question to loading or presentation but does not distinguish an unavailable file, stale material, back face or another cause; the game process texture lookup and true storage mapping remain unobserved. The owned photo and local hash evidence stay private.


### Photo selection follow-up - 27 October

Retained retry `20261027T144612968660Z` printed photo `13437586037990` but placed older photo `13437585700621`. The older ID originated in the preceding run and its PNG was absent from the restoring process's view. The workbench route now selects the photo it just printed and fails explicitly if it is absent. Offline review of the retained state confirms that the new ID was present; the corrected route has not been run and the gray rectangle's cause is not proven. The first run began with no paper photos even though the restoring process saw a board referencing two other IDs; the second began with the first run's IDs. Actual game storage and cleanup therefore remain unresolved.


### Title-cabin interaction retry - 27 October

The retained run `artifacts/bot/runs/20261027T144612968660Z` on development DLL `a1174b19694f` completed the scripted cassette, camera, note, typing, marker and photo-placement state checks. The first failed run targeted the note using the room pose from before moving the observer: its last commanded hand was approximately 275 mm from the later published notepad position. Refreshing the room and note pose let the retry acquire the note. This repairs the test route, not a demonstrated gameplay defect. Paired eye snapshots show a gray rectangle where the photo was placed; the marker grip is obscured. The selected photo ID was from the earlier run and does not match the restored studio filenames, so storage visibility and photo selection require investigation before assigning cause. No motion video was recorded and physical Quest remains unverified. Steam was retained and the owned simulator closed. The restoring process reported matching studio files, but the game-loaded photo IDs disagree with that view, so actual game storage mapping is not yet verified.


### Title-cabin interaction check - 27 October, later

On development DLL `a1174b19694f`, retained run `artifacts/bot/runs/20261027T144043760382Z` observed tape pickup, release, recovery, seating, ejection and recovery, plus camera pickup and a photo from each hand. It stopped because the scripted hand did not acquire a notepad sheet; the marker and photo-placement portions were not reached. This is a reproduced route failure, not yet a diagnosed interaction defect: hand-to-notepad contact and ownership need investigation. Both eyes were reviewed at the seated-tape and camera checkpoints; the view is steep, grips are obscured, and forearm ends dominate the foreground. These stills do not verify contact during motion. No video was recorded in this run. Test outputs were preserved and the restoring process reported matching files; later game-loaded photo IDs do not match that view, so the actual storage mapping remains under investigation. The owned simulator and game were closed and Steam retained. Physical Quest and the requested movie remain open.


### Cassette compartment geometry lead — 27 October

A static check clips the reconstructed cassette interaction box against the
owned player mesh. At the default closed carrier pose, eight stationary
triangles intersect the box; two form a broad plane about 0.47 mm from the
cassette center. At sampled open fractions 0.5, 0.75 and 1.0, no mesh-10
triangles intersect the idealized box. This is not a visible-clipping finding:
a further static check finds at least 204 actual cassette mesh triangles
crossing that plane, but material and dynamic eye occlusion are unverified.
A 5 mm hypothetical outward shift clears the
broad plane but still intersects side faces; 7 mm intersects the lid, so no
offset is justified. See `artifacts/opening-cassette-volume-lead-20261027.json`.
Review an actual close two-eye insertion and native material before changing
the working guide. The original hinge motion and finished movie remain open.


### Cassette-player model identification — 27 October

The owned `pcp0_main0_def_c00_bsm` cutscene-player texture bears the
inscription `STEREO CASSETTE-CORDER WM-R55`. Its corresponding cutscene
model has 478 vertex positions matching the cabin interior player to a
maximum 0.000193 mm after placement (see `artifacts/opening-player-model-comparison-20261026.json`).
This strongly supports a Sony WM-R55 physical reference for the cabin
model. It does not prove the interior texture binding, original hinge
travel, animation, or correct tape insertion. The owned texture and crop
remain private. [Sony product support](https://support.sony.jp/electronics/support/portable-music-players-other-portable-music-players/wm-r55)
confirms that model exists. Next inspect direct opening/loading footage and
the native animation before changing lid behavior; the movie remains open.


### Cabin weapon contact and rack cues — 27 October

The native cabin weapon path now forwards a tactile cue when the hand reaches
an eligible owned assembly. After moving that assembly away, returning it
within 8 cm of the saved mount with orientation agreement above 0.98 emits
a one-shot return cue; leaving the mount region rearms it. The cues use the
existing XR haptic output and do not change acquisition, loadout ownership,
cabin firing restrictions, or the existing automatic return on release. This
is not free placement or a completed physical re-holstering feature. Core
checks cover the cue state including initial contact, misalignment, rearming
and invalid data. All 58 groups and installer checks passed; candidate
`a1174b19694f` is installed in development and the published pin is unchanged.
Neither the cue nor the existing rifle path was tested on this candidate in
the simulator or physical Quest. The broader cabin interactions and movie
remain open; do not treat tactile design thresholds as headset acceptance.


### Expanded cabin interaction request — 27 October

The user wants a deeply interactive first-person cabin, including removing
wall-mounted guns and putting them back. Existing native cabin gear code
admits up to six verified owned equipment assemblies, acquires one at its
native root with either tracked hand, follows the solved palm, and restores
the parked native transform on grip release or interruption. Earlier build
`aaffbee70236` has both-eye, bounded carry/return evidence for a second rifle
(`artifacts/bot/runs/20261023T100324956937Z` and 062844); close views crop
the grip. This is automatic return, not a validated physical placement or
coverage of every wall object or loadout. Current candidate and physical Quest
remain unverified for this loop. Cassettes, the reconstructed player lid,
the camera, photos, paper and marker have separate existing interaction paths;
see earlier scoped reports. Fixed native cabin interior geometry is not proven
independently movable. Next map visible objects to native ownership and test
their existing interactions before changing working flows; design physical
return with safe interruption and loadout identity. Preserve native cabin
firing restrictions and the unfinished movie requirement.


### Cabin take and maintenance notice — 27 October

Candidate `789a8e11c795` completed cassette pickup, insertion and ejection,
both camera shutters and photo placement in `artifacts/bot/runs/20261027T133035987440Z`.
Body and eyepatch changes were requested and checked in both eye snapshots;
body-off snapshots show large black sleeve caps. The steep cassette shot remains
obstructed and is rejected as a finished movie. Source-eye recordings have 15
and 5 dropped samples in two segments; audio was not independently auditioned.
The marker change was not exercised. A server-maintenance popup blocked the
first attempt; a guarded startup confirmation for its witnessed popup advanced
the same live session. The existing bot contract suite passed. Personal settings
and studio files were restored, owned simulator closed and Steam retained.
See `visual-review.json` in the run; headset and continuous final-eye motion
remain unverified. Next audit the user-requested broader cabin interactions.


### Marker pickup continuity candidate — 27 October

A retained earlier actual-game trace showed the marker stationary in 15 samples
over 3.062 seconds; the last left controller pose coincided with its last
observed position, and the first held sample was 70.66 mm away. There was no
marker observation immediately before grip, so the exact pickup discontinuity
was not isolated. The code immediately replaced the acquired contact with a
fixed writing grip. Marker pickup now starts at the acquired pose and eases
to that writing grip over 300 ms while tracking hand movement; the correction
does not add throw velocity. Both-hand pickup, final grip and release checks
passed as part of all 58 test groups and installer checks. A subsequent core
check also passed for stationary early release during the transition with both
hands; it does not establish rendered or physical behavior. Development candidate
`789a8e11c795` is installed; the published pin is unchanged. The new candidate
has not been run in-game or in a headset. Writing during transition, actual
contact, release/throw and both-eye presentation need user validation. The
showcase movie remains unfinished. Evidence: `artifacts/opening-marker-continuity-20261027.json`.


### Diagonal cabin showcase attempt — 27 October

The retained bounded bot run `artifacts/bot/runs/20261027T131055498302Z`
observed tape pickup, insertion, eject/recovery, both camera shutters and photo
placement on candidate `1ed68066ab7c`. A diagonal slot viewpoint reduced the
scripted downward pitch to about 48 degrees. Sampled source-eye frames still
show sleeves obscuring the cassette and camera grip; paired final-eye stills
show the open player but do not prove moving contact. Time-neighbor pose
analysis found the cassette face approaching an 82-degree angle to the
view direction near seating, nearly edge-on; see `cassette-face-view-analysis.json`
in the run. A future view must account for the tracking-space slot normal
and be checked for reach and collision; no replacement has been tested. The recording is 30 fps
pre-compositor video with an AAC audio stream, 24 dropped source samples and 23
encoded repeats. Audio content was not independently auditioned. This take did
not demonstrate body or eyepatch toggles and is rejected as the finished movie.
No note interactions were run. Personal settings and the three-file studio tree
were restored; transient bot changes were removed, simulator closed, Steam kept.
See `visual-review.json` in the run for scope and limits. The finished showcase,
continuous final-eye motion and physical Quest remain open.


### Cassette showcase viewpoint diagnosis — 27 October

The recorded source-eye cassette transfer was reviewed against simulator pose
commands. The staged approach points nearly straight down at the player: the
initial target is 90 degrees down and the subsequent re-aim is 87.3 degrees
down, where heading becomes ill-conditioned. Playback shows a steep, poorly
readable interaction. This is a recording-route problem, not proof of a game
mechanism failure. Evidence: `artifacts/opening-hand-palette-20261026/cassette-viewpoint-analysis.json`.
For the next full showcase, use a reachable diagonal offset with horizontal
separation and a stable gravity-up heading; inspect motion in both final eyes.
The diagonal proposal was later exercised in the retained run described above;
its framing still obscures actions. The movie and physical Quest remain open.


### Final-eye camera framing review — 27 October

A private diagnostic projects a time-neighbor candidate hand mesh over retained
final-eye stills. The right-eye wireframe overlaps the hand and sleeve region;
the left-eye action is clipped at the frame edge. This is not proof of the
rendered hand mesh or grip: the samples are seconds apart, palette reads are
non-atomic, and the overlay has no scene depth. Evidence and timing are recorded
in `artifacts/opening-hand-palette-20261026/eye-overlay-review.json`. The
candidate camera pickup improvement is deployed, but clear hand contact,
continuous two-eye presentation, a finished movie and physical Quest acceptance
remain open. Do not install a speculative grip based on this overlay.


### Opposed-handle search and draw-identity check — 27 October

A multi-start opposed-side grip search using distal skin and full-finger
penalties produced an extreme modeled wrist rotation and over 60 mm translation;
full recombination still places 22 selected index vertices inside approximate
camera planes. It is rejected as a production pose; the search is not exhaustive
and its contact landmarks are hypotheses. Evidence:
`artifacts/opening-hand-palette-20261026/camera-opposed-elite-recombined.json`.
During the earlier palette session the log showed one uniquely admitted
`sna3_main1_def` forearm profile and a player-owned color draw, but the closest
observation puts that draw in the title scene before the ACC transition. It
therefore does not establish draw ownership for the later held-camera image.
A loaded buffer or shared skeleton is not a substitute for the required
current-eye draw and skin join. No candidate grip was installed. Next establish
current gameplay draw ownership and derive a supported hand/handle relationship;
review continuous eye motion and physical controller alignment. The movie and
physical Quest remain open.

### Camera grip visual geometry review — 27 October

A private static diagnostic rendered the candidate hand skin and the camera's
beveled body, base and handle from multiple angles. It is synthetic geometry
for analysis, not gameplay footage. The opposed-side grip candidates still
show modeled palm/handle overlap and large palm rotations; full recombination
reports up to 13.88 mm approximate palm-plane penetration for the selected pose.
Evidence and limits: `artifacts/opening-hand-palette-20261026/camera-opposed-visual-review.json`
and `camera-opposed-full-recombined.json` in the same folder. The candidate
mesh was observed loaded, not proven drawn, and the palette/camera samples are
non-atomic time-neighbors. The visualizations do not establish actual rendered
contact, so no production grip was changed. Next validate a draw-matched hand
and derive a supported pose that works for both hands, preserving controller
alignment, camera shutter and print behavior; then review both final eyes
through motion. The finished movie and physical Quest remain open.

### Finger palette composition validation — 26 October, later

Retained actual-game right-hand samples validate the offline palette-composition
convention for the thumb, middle, ring and little when grip changes from 0.08
to 1.0: after accounting for each actual parent transform, maximum difference
across 12 bones is 0.108 degrees and 0.001 mm. An independent older cabin
sample validates the index transition 0.08 to 0.42 to the displayed precision.
Both sets are non-atomic time-neighbor reads on earlier builds, not proof of
current draw ownership or a correct camera grasp. Evidence:
`artifacts/opening-hand-palette-20261026/finger-delta-validation.json`.
A constrained full-skin candidate search using that convention reduced modeled
intersections but also removed nearly all nearby finger contact; it is rejected
(`artifacts/opening-hand-palette-20261026/camera-articulated-union-recombined.json`).
The model still uses an unproven drawn mesh, approximate surface planes and no
physical wrist or motion validation. Next establish actual draw ownership and
reasonable finger/handle contact targets rather than optimizing only distance.
The grip, finished movie and physical Quest acceptance remain open.

### Articulated camera-grip exploration — 26 October, later

A private diagnostic reposed five weighted finger groups from the retained
nearby native skin palette while exploring palm rotation and translation. The
baseline reconstructed sample vertices match the previous projection exactly,
but the inferred bone-palette delta convention has not been validated against a
live re-pose. Recombining all candidate skin weights exposed intersections
missed by the sample-only search. An adaptive candidate removed estimated finger
intersections but retained 16 palm vertices inside the approximate handle/body
planes, up to 10.04 mm, and required a large wrist rotation. A search that
penalized every selected palm vertex reduced that overlap but introduced a
7.34 mm modeled little-finger intersection and left the index without nearby
contact. Evidence: `artifacts/opening-hand-palette-20261026/camera-articulated-adaptive-recombined.json`
and `artifacts/opening-hand-palette-20261026/camera-articulated-palm-recombined.json`.
Neither pose is a production fix: the mesh is loaded but not draw-proven,
the palette/camera join is not same-frame, planes approximate surface contact,
and ergonomics, skin triangles, control reach and motion are unverified. Next
validate the palette-delta convention in a bounded real pose and constrain
the actual drawn hand, camera surfaces and controller motion together. Both-eye
motion, physical Quest and the finished movie remain open.

### Projected full-hand grip search — 26 October, later

A static candidate-hand projection was grouped by thumb, four fingers and palm
using the retained loaded `sna3_main1` asset and nearby non-atomic skin sample.
An approximate beveled-camera surface search found a rigid socket adjustment
that reduced selected palm overlap, but the full selected-vertex review showed
new finger intersections. A stricter candidate reduced those intersections but
left the thumb without nearby contact and retained palm overlap. Evidence:
`artifacts/opening-hand-palette-20261026/camera-projected-hand-search.json` and
`artifacts/opening-hand-palette-20261026/camera-projected-hand-strict-search.json`.
Both are rejected for production: loaded asset is not proven drawn, the sample
is not same-frame, surface planes and weighted vertex subsets do not certify
skin collision, and a fixed rigid rotation does not establish physical wrist
comfort or a natural five-finger grasp. Next combine valid surface landmarks
with finger articulation and a draw-matched skin join, then test both eyes and
motion. Do not promote either pose from aggregate distance alone. The film and
physical headset remain unaccepted.

### Candidate current hand geometry — 26 October, later

The game log for the palette session reports `sna3_main1_def` forearm resources
loaded after the exact game process started. All three input-buffer hashes match
the retained owned `sna3_main1_def` asset; its mesh 18 has 42 vertices at least
half weighted to right index bone 42, 30 fully weighted. A non-atomic,
double-read-stable palette and a held-camera observation 4.52 ms apart project
the index bone origin within 0.058 mm of the published position. Applying that
palette to this candidate mesh yields all 42 selected vertex positions outside
the approximate camera body/base/handle bevels; the smallest signed-plane
distance is 50.75 mm (55.03 mm for fully bone-42-weighted vertices). Evidence:
`artifacts/opening-hand-palette-20261026/skin-projection-lead.json`.
This is a strong contact lead, not exact visual proof: resource load is not
draw ownership, the palette and observation are not the same render transaction,
and only a vertex subset was modeled. Positive plane distance is a lower bound,
not exact surface distance. Identify the drawn mesh and evaluate the full skin
and grasp through motion in both eyes before a repair or acceptance claim. The
raw asset and palette remain private; physical Quest and the movie remain open.

### Live camera-hand palette — 26 October, later

A combined actual-game take on `1ed68066ab7c` completed tape and camera/photo
actions while a bounded read-only skin-palette reader sampled a stationary
right-hand camera grip. There were 72 double-read-stable samples; 12 had
same-owner held-camera observations within 20 ms, with the closest 4.52 ms.
The live model code differs from the older `sna5` sleeve-analysis model, even
though their first 53 bone names and parents match. Shared skeleton does not
establish mesh identity. The reads are non-atomic and a time-neighbor is not
the same render transaction. See `artifacts/opening-hand-palette-20261026/skin-palette-identity.json`;
raw owned palette stays private. No mesh-contact or physical-headset pass is
claimed. The user's two photos and board were restored from
`private/opening-clean-film-baseline/thirteenth-trial`, the test prints archived
in `fourteenth-trial`, settings restored, the owned simulator closed, and Steam
retained. Next identify the exact current model/mesh and obtain a same-frame
skin-to-camera join; then review contact through motion in both final eyes.
The movie remains unfinished.

### Owned hand-mesh inventory — 26 October, later

A previously retained owned `sna5` mesh contains a right-hand mesh whose
23 vertices have at least 50 percent weight on bone 42; 17 are fully weighted.
Their bind-pose distance from that bone origin ranges from 5.38 to 25.11 mm
(`artifacts/opening-centered-camera-20261026/owned-index-mesh-inventory.json`).
This makes clear why a distal-bone-to-camera distance is not fingertip clearance.
The asset was used by an older sleeve analysis, but its identity and skinning
palette were not joined to the centered-camera render; no actual camera contact
or visible fingertip position follows from this inventory alone. The owned asset
remains private. Next capture matched model identity, skin palette and camera
pose from the same current render transaction and evaluate the deformed surface
in both eyes. Physical headset and the finished movie remain open.

### Correction: camera contact point — 26 October, later

The telemetry field `index_tip_local` is published from the position of the
third articulated index bone (`controller_rig.cpp`), not from a measured
skinned fingertip vertex. The 35.6 mm right and 23.5 mm left camera-box
distances below are therefore **distal bone-origin distances**, not direct
measurements of the visible fingertip. The earlier statements calling them
fingertip gaps are too strong. The modeled other fingers and earlier curl
searches also end at joint origins; they cannot establish visible skin contact.
Both-eye images still show an unclear grip, but exact finger-surface separation
or penetration remains unproven. Do not change the contact socket solely
from these distances. Next identify the actual owned hand mesh and its skin
weights for the matched render transaction, then compare visible finger
geometry to the camera surface through motion. The same naming limitation
also applies to historical paper distance reports; their original results are
retained with this qualification. Physical headset and finished movie remain open.

### Full-hand camera contact model — 26 October, later

The retained native-left 16-joint hand offsets, mirrored for the right, now
include thumb, index, middle, ring and little chains and their shared parent.
The camera surface analysis reconstructs body, base and side-handle bevel
triangle planes from the mod-authored geometry. The actual captured index tip
was within 1.9 mm (left) and 5.7 mm (right) of the nominal model at the
sampled print poses (`artifacts/opening-centered-camera-20261026/camera-full-hand-live-estimate.json`).
Other finger joints are modeled, not published measurements; negative plane
distance is only a centerline warning, not proof of skinned-mesh penetration.
An exploratory 150,000-candidate-per-hand search with hypothesized surface
landmarks still yielded large fingertip errors and some internal joints
(`artifacts/opening-centered-camera-20261026/camera-full-hand-search.json`).
The search is neither exhaustive nor evidence that a supported grip is
impossible, and its landmark targets are assumptions. No pose is promoted.
Next: obtain full actual hand contacts and establish valid camera-side contact
landmarks, then derive and test the hand/handle transform; verify motion and
both eyes before changing acceptance. Physical headset and the movie remain open.

### Camera grip geometry search — 26 October, later

An offline nominal-hand search compared index and thumb positions with boxes
approximating the mod-authored camera body, base and side handle. The first
low-tip-distance candidates put intermediate finger joints inside those boxes.
A second search penalized those intersections but converged to both tips on
the underside of the base with about 60 degrees of palm roll. These are not
accepted grips or gameplay fixes: they use nominal chains and boxes, omit other
fingers, skin and ergonomics, and were not exercised in the game. Evidence:
`artifacts/opening-centered-camera-20261026/camera-grip-exploration.json` and
`artifacts/opening-centered-camera-20261026/camera-grip-joint-search.json`.
Do not promote a pose merely because fingertip-to-box distance is small. Next
use exact camera-side surfaces and complete finger chains to derive a supported
grasp, then verify actual hand/controller motion and both eyes. Physical
headset comfort and the finished cabin movie remain open.

### Camera hand contact investigation — 26 October, later

The centered-camera run exposed a specific unresolved grip problem. Published
rendered index fingertips in the shutter poses were 35.6 mm (right) and 23.5 mm
(left) outside the conservative camera bounding box; the front/side final-eye
images show the right fingers below the camera. See
`artifacts/opening-centered-camera-20261026/camera-contact-analysis.json`.
The camera palm anchors are inside the modeled side-handle volume. An offline
nominal finger-chain calculation predicts that index curl alone would leave
about 29 mm right and 18 mm left outside even that conservative box. This is a
prediction for the sampled pose, not runtime proof, and bounding-box contact is
not exact mesh contact. Do not fix the gap by an arbitrary camera offset: that
may bury the palm. Next derive and test compatible palm orientation, handle
contact and articulation together, then inspect full hands through motion in
both eyes. Other grip poses, thumb contact and physical Quest remain unverified.

### Centered-camera take — 26 October, later

`artifacts/opening-centered-camera-20261026` exercised the current checked
candidate with a centered camera position and a deliberate cassette-face reveal.
Tape seat/eject, both camera shutters and photo placement were observed; the
source-eye recording had 11 dropped samples and 10 repeats. Both final-eye
print checkpoints were reviewed. The camera body is more central, but the
right sleeve hides the gripping hand and the cassette face remains largely
obscured during insertion. The take is rejected as a finished showcase; this
does not establish whether hand geometry is inside the camera or merely hidden
from the chosen viewpoint. Preserve that distinction when investigating contact.
The user's photo pair and board were restored from `private/opening-clean-film-baseline/eleventh-trial`,
new test prints archived in `twelfth-trial`, settings restored, and owned
simulator closed. Continuous final-eye motion and physical headset remain open.
Next: verify the camera-side grip geometry and a reachable viewpoint that exposes
the hand, and validate the cassette interaction without the covering arm.

### Camera pickup continuity repair — 26 October, later

A controlled stationary right-hand grip reproduced the camera moving 33.28 mm
in 116 ms (`artifacts/opening-stationary-camera-20261026`). Camera pickup now
starts at the acquired pose and eases camera attachment and rendered palm toward
the authored handle over 300 ms; contact easing is excluded from throw velocity.
Candidate `1ed68066ab7c` passed all 58 test groups and installer checks and was
installed in `build/dev`. In `artifacts/opening-camera-fix-verified-20261026`,
the equivalent 115 ms sample moved 9.94 mm, while the head stayed stationary.
This is a partial transition sample, not a claim of zero final adjustment. Tape
seat/eject, both camera shutters and photo placement completed in the same run.
Both final-eye pickup stills were reviewed but crop the hand; continuous final-eye
contact, different grip orientations, physical Quest and cinematic quality remain
unaccepted. The film still obscures tape insertion. The user's two fresh photos
and board were restored from `private/opening-clean-film-baseline/ninth-trial`;
test photos were archived in `tenth-trial`, settings were restored, the owned
simulator closed, and the published `play/` pin remains unchanged. Next: verify
full-view hand contact during motion and produce a readable cassette sequence.

### Camera displaced by interrupted test — 26 October, later

`artifacts/opening-cinematic-offset-20261026/camera-fall-cause.json` narrows
the camera-on-floor observation: after the simulated tape grip expired, the
script reissued right grip; the following sample showed the tape unheld and
the camera held. Cleanup released the camera, and the same-session retry then
found it on the floor. This is test-route contamination, not evidence of an
idle camera falling by itself. The interval does not identify exactly when tape
ownership was lost. The next film route must confirm tape ownership while
renewing a bounded grip and stop without re-grabbing if ownership is lost;
verify the camera is at its native resting location before recording. The
previously documented camera pickup movement remains a separate, unisolated
lead. No gameplay change or headset acceptance is inferred.

### Offset-view retry — 26 October, later

The first offset-view take (`artifacts/opening-cinematic-offset-20261026`)
stopped when its five-second scripted grip lease expired during slow observer
motion. It was a test-control failure, not a diagnosed headset interaction bug.
The same-session retry (`artifacts/opening-cinematic-offset-retry-20261026`)
renewed the bounded grip and observed tape pickup, seating and ejection, camera
shutters with both hands, photo placement and body/eyepatch requests. The native
source-eye recording reports four dropped samples and three repeated frames.
Visual review rejects the new angle: it hides the tape during insertion and
the camera is on the floor during approach, so it is not a cinematic showcase.
The prior two fresh photographs and matching board were restored from
`private/opening-clean-film-baseline/fifth-trial`; retry files were archived in
`sixth-trial`. Personal settings match their original hashes, Steam was retained
and the owned simulator closed. The original lid motion, both-eye motion quality
and physical Quest acceptance remain open. Do not repeat this route unchanged;
frame the grip and carrier from a reach-valid pose and reset the prop before filming.

### Revised cabin staging — 26 October, later

`artifacts/opening-cinematic-staged-20261026` records an actual ACC take on
the unchanged checked candidate. The test route now carries the cassette to
the player without its prior deliberate drop, eases observer motion, and derives
the two photo aims from the camera's acquired attachment and lens direction.
Both resulting PNGs show the cabin wall; the prior take photographed the floor.
Cassette seat/eject, both camera shutters, photo placement, body and eyepatch
requests were observed. Both eyes were inspected at seated tape and camera
print checkpoints. This is an improvement to demonstration staging, not a
production gameplay fix or a physical-headset pass. Review still finds the arm
crosses the cassette interaction, camera movement lacks cinematic composition,
and the 30 fps pre-compositor source-eye recording dropped 59 samples and
repeated 59 frames. Continuous final-eye quality and original lid motion remain
unproven. The live studio contains two fresh photos and two paper records;
the previous fresh pair is backed up in `private/opening-clean-film-baseline/fourth-trial`.
Personal INIs match their original hashes and the owned simulator is closed.
Next: solve the framing and movement from actual playback, then capture and
review both final eyes through motion; do not call this finished footage.

### Clean-set cabin take — 26 October, later

At the user's request, the prior custom photo and note collection was cleared;
its 47 PNGs and board were backed up under `private/user-cleared-cabin-studio-20261026`.
The live studio now contains two newly captured photo PNGs and two paper records.
`artifacts/opening-clean-final-attempt-20261026` records the actual ACC interaction
loop, cassette seating/ejection, camera use with both hands, photo placement and
requested body/eyepatch switches. The 30 fps recording is a native source-eye
capture with game-process audio, before compositor; eight samples were dropped
and seven encoded frames repeated. Review rejected it as the requested cinematic
showcase: abrupt viewpoint/hand changes obscure the action, and the photographs
are poorly framed. Final-eye stills do not establish continuous stereo or physical
Quest acceptance. Settings were restored and the owned simulator closed. Next:
resolve camera and hand staging and verify a continuous, readable final-eye take;
do not present this take as finished or treat the scripted loop as headset proof.

### Cabin showcase attempts — 26 October

### Current next action after cabin/settings review - 27 October

The requested simulator completeness review used the existing checked candidate
`9d646890a2b4`: title workbench and ACC settings/remapping completed in one session.
Reviewed runs `20261027T180104125768Z` and `20261027T180305839994Z` are retained;
CURRENT records the scoped results, visual obstructions and exact restoration.
No new runtime build, publication or published-pin promotion occurred.

Next, diagnose the initial ACC foreground obstruction and cabin hand-contact
presentation from these retained views before changing the rig. Do not repeat
unchanged narrow passes or treat every missing acceptance case as a defect.
Ground Zeroes is a separate implementation task: establish its player/camera and
skin ownership, then head anchoring, tracked hands/aim and wrist UI using its own
verified layout. Preserve applicable personal settings and migrate the old GZ
bindings with the new parser; copying TPP-specific hooks cannot provide parity.
Full-game and physical-headset acceptance remain open. Batch justified runtime
repairs into one checked development candidate when there are diagnosed repairs.

Multiple actual-game takes were attempted after the user requested a clear, human
showcase. A later side-angle take (`artifacts/opening-showcase-side-retry-20261026`)
completed at 30 fps with game audio but still obscured insertion behind the arm;
it was rejected as a finished demonstration. An opposite-hand retry
(`artifacts/opening-showcase-left-20261026`) completed the interaction loop,
but its recorded arm caps obscured insertion even more; it was also rejected.
Its test photos were archived and the studio baseline restored exactly
(`artifacts/opening-lefttake-restore-20261026.json`). The source-eye recorder
captured 30 fps with game-process audio;
this is pre-compositor footage, not final-headset video. The last complete
workbench take `artifacts/opening-showcase-close-20261026` observed cassette
seat/eject, camera printing with each hand, note editing and marker writing,
and photo/note board placement. Review rejected it as a finished showcase:
the player's insertion is obscured by hands/props and the motions do not read
clearly. The earlier reframe attempt stopped when an approaching left hand
grabbed a loose photo instead of the camera; it is a failed take, not proof of
a universal camera defect.

A separate native ACC take `artifacts/opening-showcase-body-20261026` recorded
full-body requested off/on/off with both final eyes sampled at each state. The
on state admitted the body; the off state showed arms. Both-eye review found
prominent forearm cut ends and incomplete body framing, so this does not certify
a polished headset presentation. The two recordings are separate takes and
must not be represented as one continuous scene or as a completed showcase.
The studio board and existing images matched their pre-test inventory exactly;
temporary photos were archived privately. Both user INIs matched their original
hashes after restoration, the owned simulator/game session closed and Steam was
retained. The checked development DLL did not change.

Next: improve the actual hand/prop view and visually demonstrate a clear
cassette insertion and eject, camera/photo/note handling and body switch with
sound; inspect the completed takes and both eyes before delivery. Physical Quest
and original native lid motion remain open.


### Follow-up contact evidence

Retained motion samples expose a contact regression in the scripted center-grip
poses: the previous authored attachment put the index tip within 3–6 mm but
snapped the sheet 8–9 cm and about 106 degrees; the current attachment avoids
that snap but leaves a measured 30–38 mm fingertip gap during the sampled holds
(`artifacts/opening-paper-contact-comparison-20261026.json`). Those scripted
poses are not naturally aligned pinch trials. A nominal finger-chain search
suggests the existing curl axes alone cannot close that gap in those poses; it
is not a full IK or visual acceptance result
(`artifacts/opening-paper-curl-feasibility-20261026.json`).

Additional leads, not diagnosed failures: camera handle alignment predicts a
33 mm pickup shift for the sampled right grip, while the sampled room-relative
camera shift was 32 mm; the trace lacks a tight stationary before/after bracket
(`artifacts/opening-camera-pickup-lead-20261026.json`). The marker's large
right-hand displacement coincided with a 58 cm input-to-rendered-palm separation;
a left-hand sample had close palm alignment but was not tightly bracketed
(`artifacts/opening-marker-pickup-leads-20261026.json`). Reproduce these in a
future combined pass with reachable, deliberately aligned grasps, inspect both
eyes and preserve typing, drawing, release and throw behavior. Do not infer
physical Quest results or change the existing flow solely from these leads.

## Opening cabin follow-up — 26 October

Checked/deployed development DLL `217df3f4b9be` includes a repair for a
reproduced stationary pickup jump: the pinned note and Polaroid previously
moved 8–9 cm and rotated about 106 degrees when gripped. Pickup now preserves
the selected hand-relative pose, with only a small aligned pinch adjustment;
contact adjustment does not manufacture throw velocity. In the combined native
run `artifacts/opening-paper-lid-validation-20261026`, stationary samples
moved about 1 mm and 0.09 degrees. The run completed cassette pickup, off-axis
rejection, seating, eject/recovery, camera printing with both hands, note
editing, marker writing, and note/photo board placement. All 58 build test
groups and installer checks passed. The user's board was restored byte-for-byte
and existing photos verified (`artifacts/opening-studio-validation-restore.json`).

Eight coincident lid end-face vertices previously stayed behind; the exact
native interior mesh now moves 76 lid-body vertices plus four window vertices
(`artifacts/opening-lid-end-seams-20261026.json`). The second native player bone
influences a small control, not the identified lid vertices; this does not
exclude other native animation mechanisms. Original lid travel remains unverified.
The paired-eye stills are too distant or obstructed by sleeves to establish
clean fingertip contact or the quality of the moving lid. Dynamic lid motion,
wider grab orientations, physical Quest testing and whole-scene acceptance
remain open. The test-owned session is closed and Steam retained.


## Opening cabin cassette and lid — 26 October

The user reports unnatural cassette insertion and questions the tape-player
model and lid motion; Polaroid and Post-it handling are also in scope. The
existing player body is owned native helicopter-interior geometry. A point-set
comparison of all 478 vertices against the separate owned `pcp0` cutscene player
matched within 0.0002 mm after the measured placement; this proves geometry
correspondence, not native animation or material parity. The white instant
camera is a separate mod-authored object. The lid uses reconstructed motion;
the original cutscene animation and exact travel remain unverified.

A live off-axis attempt reproduced an 89.36-degree cassette rotation at the
guide boundary (`artifacts/opening-guide-repro2-20261026/guide-snap.json`).
Source `4945ca9` preserves the hand-driven rotation, rejects a sideways entry,
introduces guide correction continuously and interpolates seating after release,
including reversed aligned tapes. The exact hashed interior mesh had ten lid
triangles mixing animated and stationary vertices; the previously identified lid components
then moved together (68 body vertices plus four window vertices; see follow-up above). Geometry audit:
`artifacts/opening-lid-topology-20261026.json`.

Checked/deployed candidate `d2341d57320d` passed all 58 test groups and
installer checks. Fresh native run `artifacts/opening-guide-verified-20261026`
measured zero rotation at the formerly snapping sideways guide boundary and
completed pickup, seat, eject and supported recovery. Both final-eye seated and
open-player images were reviewed; dynamic motion and physical headset comfort
remain unaccepted. Earlier paired workbench `20261027T011954036566Z` completed
Polaroid and Post-it pickup, writing and board placement on the prior candidate;
this is not proof of smooth contact at every orientation. In-game tests added
four photos and four strokes to an existing note; the pre-test library and
note stroke count were restored, test images retained privately, and other
paper records preserved byte-for-byte (`artifacts/opening-studio-cleanup.json`).

Next: review the reconstructed lid travel against a verified original native
animation or physical reference, and reproduce any remaining paper contact or
placement instability before editing those working paths. Physical Quest and
whole-scene acceptance remain open. Steam retained; owned session closed;
published play pin and personal control settings unchanged.


## Gameplay toggle demonstrations — 26 October

Source `52585ca` adds a 25–150% marked-enemy size control (60% default),
front-loads God Mode and common switches, and groups related settings in the
headset and launcher. A reproduced live-edit bug stowed binoculars when marker
settings changed; display/recon-only changes now preserve the active optic
after neutral release. Binding and other control changes retain the reset.

Checked/deployed dev DLL `4187a681cd4f` passed 58 test groups and installer
checks. `artifacts/toggle-showcase-20261026/field-kit-gameplay-showcase.mp4`
is an edited native right-eye gameplay recording with game audio; paired
final-eye checkpoints and exact scopes are in `coverage.json`. It demonstrates
body/eyepatch (previous candidate), binocular/world marker visibility and size,
close-muzzle firing allowed/blocked, and cover attachment/release/re-attachment.
The sandbag entry needed a lateral approach; lean/aim retained native cover at
night, with visual and physical acceptance still open. A second binocular glow
setup lost the bot's fresh tracked owner; glow remains unverified. God Mode
damage immunity, other switch effects, dual sights, physical Quest behavior,
daytime shifting shadows, and wider mounted interactions remain open. This is
not a complete all-toggle showcase or a whole-game acceptance claim.

Next: finish effect-based demonstrations for the remaining switches and
investigate the tracked-owner interruption; diagnose daytime shadow movement.


## Mounted spatial aim and daytime shadow report — 26 October

The user clarified that the daytime report is **larger light/shadow patches
shifting during head nods**. Trees never disappear. Do not diagnose this as
missing foliage/culling or tiny specular sparkles. The cause remains undiagnosed;
no rendering or graphics-setting change is claimed.

Retained `artifacts/foliage-20261026/baseline-lz` records the initial daylight
head-pitch sweep and paired final eyes. `daylight-lighting` in the same directory
records a noon fixture, exposure observations and both eyes; the original clock
was restored. `camp-lighting` occurred after sunset and is excluded from daytime
acceptance. These captures do not establish a shimmer fix or headset acceptance.

Source now extends the existing mortar `0x46` / machine-gun `0x47` handle adapter:
spatial hand displacement uses the actor's authored forearm length as an input
lever; wrist rotation remains additive; either hand operates and the other
supports/transfers at the live bearing. Right-stick trim adds to the target,
stops at the attained bearing on release, limits pending travel at native stops,
and does not rotate the viewer. Native mechanical limits, action ownership,
tracking-loss release and fresh-grip reacquisition remain in force. This is not
a newly verified physical emplacement pivot or universal mounted-gun adapter.

The retained mortar attempt `20261026T192853043999Z` refused the already destroyed
native fixture before mounting. It is a fixture refusal, not a new input failure
or an acceptance pass. The new spatial/trim behavior has contract coverage;
in-game hand contact, discharge/impact and physical responsiveness remain open.
Helicopter/anti-aircraft handle admission is still unverified. Initial Blast Arm
launch heading, recurring tutorial notices and previous physical limits remain.

Checked/deployed candidate `87186cb55ebe` / source `939d41a` passes all
58 test groups and installer checks. Final review also repaired the one-frame
hand-transfer fallback to camera turning. The initial handoff build failed for a
missing include; the corrected final build passes. New in-game/physical mounted
acceptance remains open. Use `Play-Dev.cmd` for the user's review.

Next: diagnose the reported daytime shadow patches and confirm actual native
helicopter/anti-aircraft contacts before extending admission. Do not substitute
a speculative shader patch or disable shadows and mark the report fixed.
Published play remains pinned; all 424 published and 434 development files,
installed DLL and personal settings verify. Owned simulator/input session is
closed, clock/poses restored, Steam retained and Meta 64-bit Link checked.
Receipt: `artifacts/mounted-spatial-preservation.json`; final build log:
`artifacts/mounted-spatial-final-build.log`. No whole-game/headset sign-off.

## Blast Arm tower destruction with game audio — 26 October

The user rejected take6's 3.3 fps capture and missing game audio. Replacement
`artifacts/rocket-proof-20261026/take7-native-av/field-kit-rocket-arm-game-audio.mp4`
retains a continuous 22.9-second native right-eye source and synchronized MGSV
process audio: 680 unique image transactions, 29.69 unique fps, eight dropped
samples, longest source gap 0.266 seconds. The 30 fps container includes repeated
frames; no interpolation. Full render FOV aspect is corrected, without cropping.
This is in-engine eye footage, not final-compositor video. Separate paired final
eye checkpoints are retained. No synthesized narration. Actual launch, socket
motion, gyro, stick and native return are shown; no demolition in this take.

Crouched A-star approach `20261026T181347204901Z` reached the outpost. The earlier
stance-input failure remains scoped to its own run. Simulator Thumbstick X/Y
writes were observed to replace one another, preventing diagonal radial choices;
this does not establish a physical-controller defect.

User authorized stopping the separate OpenNV test to free Meta XR. Its verified
process and child crash handler were stopped; Steam retained. Fresh MGSV process
34376 loaded the ACC, where Blast Arm was selected through the native sortie
loadout (retained `artifacts/rocket-tower-20261026/blast-equipped`). Supply-drop
waiting was abandoned. Deployment `20261026T184813917645Z`, approach
`20261026T185023101192Z` and flank `20261026T185239276285Z` are retained.

**Delivered:** `artifacts/rocket-tower-20261026/blast-shot2/field-kit-blast-arm-tower-game-audio.mp4`.
32.4 seconds, 970 unique native images (29.94 fps), two repeated CFR frames,
maximum source gap 0.0993 seconds. Synchronized 48 kHz stereo MGSV audio has no
reported capture gaps; mean -18.7 dB, peak -0.5 dB. No narration or interpolation.
Native hand ID 518 and God mode 1 are recorded. Launch, additive socket/gyro/stick
guidance and explosion collapse the wooden watchtower at about 24.5 seconds.
Both final eyes after smoke clears show the tower gone. No enemy casualty claimed.
Review/limits: `blast-shot2/visual-review.json` in the same retained directory.

First take `blast-shot1` missed: launch follows the native camera heading, not
HMD gaze. Existing native-smooth right-stick turning aligned the second shot.
This is an unresolved launch-aim contract gap, not a source repair or a perfect
rocket-arm pass. Next fix: investigate tracked LEFT socket aim at initial launch;
retain recurring tutorial notices and physical-headset acceptance as open.

Owned session closed, input/poses restored, Steam retained, Meta 64-bit Link
checked. All 424 published and 434 development manifest files, installed DLL and
personal settings match the earlier receipt. Published pin unchanged. Receipt:
`artifacts/rocket-tower-20261026/preservation.json`.


## Checked spatial controls and silent video — 26 October

Candidate `9d5e15c0a101` / source `e162ef6` passed all 58 test groups and installer
checks and is deployed to development. The new spatial socket channel, stronger
wrist gyro and joystick are additive. Live screen/socket/gyro/twist and fitting
settings remain available; the oval stays flipped up above the wrist.

Retained `artifacts/rocket-proof-20261026/take6` shows centered arm approach,
palm-down wrist, launch, spatial raise/wave with wrist rotation fixed, wrist
pitch/yaw/twist, all three controls together, downward guidance and native return.
All 77 left and 24 right captured frames reviewed. Capture cadence is 3.30/1.00
fps, so smoothness and physical headset acceptance remain open. Native horse
crosses the foreground. User accepts intentional right-eye patch overlap.
Take5 ended before completing all three inputs; it remains incomplete.

Delivery is `take6/field-kit-rocket-arm-no-voice.mp4`: continuous real left-eye
footage with Field Kit labels outside the image. Robotic narration was rejected
and removed; ffprobe confirms video only. No game audio was captured. Do not
add synthesized narration back to this delivery.

Recurring Choose a Rival Help remains unresolved despite saved FINISH state;
normal Back dismisses it. Latest retained arrival/arm selection route is
`20261026T171614902974Z`. No tower demolition or enemy impact is yet demonstrated.
Tower approach `20261026T173208372900Z` reached the ridge foot. Ascent
`20261026T173417070766Z` failed when requested physical input was not observed
by the native XR sampler; no shot or demolition occurred. Earlier wrong-height
goal refusal `20261026T172703692067Z` is retained separately. Cause of ascent
input failure is undiagnosed; no collision or gameplay repair is claimed.

Owned simulator/game session is closed, inputs released, Steam retained, Meta
64-bit Link runtime restored/checked. Both build slots and installed DLL verify;
personal settings and published pin are unchanged. Receipt:
`artifacts/rocket-spatial-preservation.json`. Next: user Play-Dev headset review;
retain tutorial persistence and tower-ascent input failure as unresolved. Older
sections below retain the historical stage and build they originally describe.

## Socket + gyro + joystick clarification — 26 October

User explicitly requests three concurrent controls: spatial forearm/socket aim,
stronger independent phantom-wrist gyro, and fine joystick correction. Prior
`0451221ea76d` demonstrates wrist gyro and joystick, but moving a socket with
unchanged palm orientation did not itself steer. Its centered take4 is retained
at `artifacts/rocket-proof-20261026/take4`: arm approach/palm-down pose, launch,
wrist rotation, translation, joystick, downward burst and native return; 22 s,
65 left/63 right frames at 2.95/2.86 capture fps. Both complete frame sequences
reviewed; left feed clear. User confirms the noticeable right-eye patch overlap
is intentional and acceptable, not a defect by itself. No physical acceptance.
Take3 timed out waiting for automatic return and is retained as incomplete.

New source adds the solved elbow-to-socket direction as a separate yaw/pitch
channel at gain 1, alongside the existing wrist gyro (default gain 2) and stick.
Socket roll is excluded from this channel to avoid duplicating wrist twist.
All three add before native bounds; none overrides/recenters another. A new
live Rocket Arm socket steering switch defaults on. There are 70 settings and
19 switches. Build/check/deploy and spatial-steering video are pending. Retain
the flipped-up oval, original personal settings and published play pin.

Latest user clarification: flip the oval UP above the socket, facing the pilot.
Source now lifts it 60 degrees from the dorsal wrist plane about its attached
near edge; the complete mount still follows the wrist. This supersedes the flat
plane requested earlier. Flat candidate `eca68f998fb9` passed 58 groups and
installer checks, but take2 ended during yaw before all controls were shown.
The flight/feed returned early; impact cause is unproven. Retain
`artifacts/rocket-proof-20261026/take2` as incomplete. Flipped-up `e56cff6352e9`
also passed 58 groups/installer and deployed. The following build synchronizes
launcher wording and this evidence record; flipped-up visual review is pending.


Next action: build/check/deploy the corrected dorsal OVAL wrist display, then
record the user-requested bounded final-eye demonstration. The earlier upright
rectangle failed visual review. Show actual launch, wrist twist, translation,
additive joystick and native return; inspect both eyes. Preserve the published
pin and saves, restore poses/input/runtime and close the owned simulator after
recording. No universal or physical-headset acceptance from this take.

Current action completed: checked/deployed `fbc50f53c4d3` / source `3421214` of the clarified Rocket Arm batch:
rectangle attached to the left socket, phantom left-hand gyro including optional
wrist twist, additive joystick correction and live tuning. All 58 groups and
installer checks pass; both slots and installed DLL verify, personal settings
and published play are preserved. Next use the user's physical headset feedback.
Do not claim the new interaction is
perfect from contracts or reuse old Rocket Arm images as acceptance of it.

Latest action completed: checked/deployed `c5900adb1a22` / source `72b3636` for the live-settings
usability batch (first-page body/eyepatch, switches before fitting controls,
accurate launcher descriptions). All 58 groups and installer checks pass; both
slots verify, saved preferences and published pin remain intact. Next, use
the user's Play-Dev physical feedback; no repeated
simulator/soak runs. Existing unresolved field scope remains in FIELD_FIXES.
This change does not establish new gameplay or headset acceptance.

This is the durable plan for finishing the user-reported repairs and preparing
the next TPP experimental release. Read [CURRENT.md](CURRENT.md), then execute
this plan. [FIELD_FIXES.md](FIELD_FIXES.md) separates implemented repairs from
remaining code work and acceptance. Do not restart the three-day investigation
from unchecked historical boxes.

The user has now explicitly set release readiness as an active goal: complete
all justified fixes on main, then test the batch and continue through remaining
field/progression checks. Do not stop merely because one cuff case passes.

Current checked/deployed candidate is `27e9108f2a58`, preserving held walking
input across helicopter disembark. The targeted held-stick smoke passes, with
0.4035 units horizontal travel after exit and neutral input restored afterward.
Retain the user's broader physical movement report until its behavior is confirmed.
Previous `1e50576a0e7d` darkens the native
first-person eyepatch to matte black and removing reflection-like atlas detail.
The prior `422594fb5ef8` supplies retained stereo on/off/motion pictures.
The preceding `f58ef4224995` batch remains included. The reported arm,
sandstorm, Rocket Arm, MG and mount-centering batch is implemented and has scoped
combined gameplay results in CURRENT. Actual follow-up defects (MG root-yaw
feedback and Rocket guidance neutral resets) are corrected. Original settings
and Meta Link are restored and Play-Dev is ready. Retained native face captures
confirm eyepatch on/off/on; physical headset review remains pending. Retain the older unresolved field scope;
do not repeat completed narrow cases or certify untested combinations.

Previous checked candidate `62f696613e2b` / `5bb471f`:
It fixes terminal stow routing under camera-suspending Help and retains the whole
physical Menu press as handheld-owned. Forced-Help stow/reopen passes retained
20261025T152037198735Z.
It additionally retires the on-board exemption after disembarking and admits
Challenge Tasks to exact-owner stow-Help cleanup. Open-handset exit/stow smoke
passes; the corrected live Challenge Tasks stow/reopen now passes too. Physical
review remains open.
It includes camera/skin body-visibility continuity and field-pickup iDroid
stow/reopen ownership. Scoped full-body/forearm-only movement and flight/field
handset lifecycle reviews passed. Actual pickup entry remains unaccepted because
the helicopter fixture did not land. Extreme cloth, exact terrain failures and
broader field/headset scope remain open. The previous stock correction and its
retained failures remain recorded in CURRENT and FREE_PLAY_ACCEPTANCE.

The identities below record the original audit baseline.

## Starting point and authority

- Work in the existing checkout on `main`; no new branch or worktree.
- Audited gameplay source: `5622052bd8799603ef8aae66235ec82d3c7bce6f`.
- Checked development DLL: `17b044cba72e33e6d6eefc89be5f00bcb313ba0fee4bb8751da9d43e52b50e4f`.
- Published local play DLL: `b627c7cacc9acd540e04193a811d3ca14bd59c9be542cc587345f1cd5ca2b47c`,
  source `f8cf03f949aeb37203dccf67ad5ce7a306d7fe6d`.
- Before this documentation cleanup, development's complete source identity
  matched the checkout and both slots had zero manifest file mismatches.
  The existing October 23 report records 58 passing test groups, zero failures.
  These are existing results, not a new gameplay or headset pass.
- Documentation changes now make the checkout newer than the packaged docs.
  Do not rebuild merely to refresh prose; include them in the next planned batch.
- The source audit verified production callers and relevant test assertions.
  It did not launch the game, change bindings, or certify physical play.
- `AGENTS.md` contains the effective rules. Historical directions and reports
  retain provenance but cannot override newer rules or reopen a repaired subcase.

## Decisions already made

The fixed-length legs/pelvis solver, reach and wrist-isolation repairs, garment
drivers, reload support retention, tracking recovery, contextual-heading guard,
cover entry/exit fixes, transit body, left iDroid lifecycle/projection, Rocket Arm
steering, cabin paper/printing and ACC rifle grabbing are implemented on main.
Preserve them. FP13's free-roam route and FP14's sortie head are scoped resolved.

Concrete implementation limits: physical mounted aiming admits native actions
`0x46` and `0x47` only. The active source batch expands forearm-only cropping
from one sna5 layout to 20 exact owned outfit profiles; the combined build passed and the selected outfit was visually inspected.
Other outfits remain unreviewed. The previous sna5 cuff has scoped stereo review;
extreme full-body sleeve crowding remains a visible limitation.
The exact rock/corridor reports remain undiagnosed. Missing coverage for another
weapon, outfit or lesson does not establish a new defect.

Historical contribution refs are not outstanding feature branches to merge.
The asynchronous mailbox worker was withheld over native D3D11 synchronization;
the extra movement filter was not layered over the existing stabilizer. Preserve
those decisions and attribution; see [fork review](FORK_REVIEW_2026-09-20.md).

## Execution order

### 0. Documentation preparation — completed in this pass

Consolidate current rules and entry points; preserve old evidence as history;
record code state separately from acceptance; keep the release gate conservative.
No gameplay code, build, deployment, pin or publication is part of this step.

### 1. One bounded baseline investigation for the remaining unknowns

Use the existing checked development before editing working flows. Inspect the
effective personal bindings and current native state once, then select the
following scene groups. Reuse retained passing subcases; do not repeat ordinary
ACC toggles, both-rifle tracking recovery or binocular village marking just to
generate another pass count.

| Order | Scene group | Exact question to resolve | Required outcome before a code change |
| --- | --- | --- | --- |
| 1 | Body/arms, then eligible field cover | Does the current cuff/cross-head crowding obstruct ordinary use? Does full-body fallback retain hands/forearms without upper sleeves for the selected outfit? | Identify exact outfit, vertices/weights or admission failure; preserve native thigh/shin lengths and left bionic arm. Do not restore the startup-failing cuff experiment. |
| 2 | Mounted fixture | Why does native entry fail at the intact Mother Base gun, and what native action/contacts belong to each additional type? | Verify intact status, native interaction eligibility and actual mounting. The latest local handoff already reaches the adjacent platform and records failed rear-entry attempts. Do not repeat approach loops, infer `0x45`, or override Mother Base's lethal-weapon restriction. Use a genuinely usable alternative if the base gun is unavailable. |
| 3 | Reported terrain | Can the exact low-rock interruption and tight passage be repeated? | Record native actor/capsule/floor state versus camera/body in both travel directions and body modes. A bot displacement guard or blocked route is not itself a collision bug. If the location cannot be recovered, record that precise dependency and continue independent work. |
| 4 | Remaining progression/weapon gaps | Do forced iDroid lessons, bolt-action support, Fulton/climb headings or powered-arm impact actually fail on this candidate? | Identify native owner/state and visible failure before altering their working generic paths. No tutorial/save/progression bypass. |

Each case has a bounded deadline and neutral cleanup. A timeout without a new
diagnostic fact ends that attempt; resume only after a changed prerequisite or
specific diagnostic hypothesis. No supervisor loop or repeated destroyed-gun
entry. Missing hardware/scene is recorded as unproven, not a code defect.

### 2. One coherent correction batch

Prioritize body/head/arm presentation, then confirmed mounted/interaction issues
and any terrain or progression failure reproduced in step 1. Use the source
ownership map in FIELD_FIXES; extend existing implementations instead of adding
parallel rigs, optics or menu owners. Additional mounted types require confirmed
native action identity, authored contacts, native limit enforcement, release and
stick override. Broader outfit cropping requires source-bound geometry admission,
not arbitrary model hiding. Do not weaken collision to clear a route.

Keep every selected repair in one reviewed candidate. Run focused contracts
while editing where they test the failure; do not build/deploy after each small
subcase. If step 1 finds no justified gameplay change, use the existing fixes and
proceed to final source/docs packaging checks without inventing work.

### 3. Commit reviewed source/docs, build once, validate the combined candidate

Record actual outcomes in CURRENT/FIELD_FIXES/FREE_PLAY_ACCEPTANCE and reconcile
the five conservative catalog blockers with their exact evidence. Prepare the
release notes before the final source commit/build so the package includes them.
No source push or publication is implied by a local commit.

From the repository root:

```powershell
python tools/workspace.py status
.\tools\build.ps1
python tools/workspace.py launch-sim
python tools/workspace.py bot --command continue
```

`build.ps1` builds/checks development only. `launch-sim` explicitly deploys it;
do not call `pin-build`. Run the selected bounded cases through `workspace.py bot`
with reviewed existing suites/policies. Do not pretend a named suite exists for
an unimplemented fixture; select its actual guarded case before dispatch.

| Combined review | Required behavior and adjacent transitions |
| --- | --- |
| Body and movement — FP04/05/06/17 | Standing/seated calibration and recenter; crouch/prone/walk; both-arm reach and wrist isolation; cross-head return; cover lean/peek/exit; full-body on/off and fallback; boarding/transit/disembark. Native thigh/shin lengths remain unchanged. |
| Combat and recovery — FP01/02/08 | Moving supported rifle, fire/visible impact, reload, bolt cycle, release/reacquire and weapon swap; valid idle and invalid tracking with no held-trigger discharge on recovery; Rocket Arm flight/impact/return and eligible Blast Arm case. |
| Mounted and contextual actions — FP03/11/15 | Each claimed mounted type: actual entry, either-hand yaw/pitch, limits, contact, operating-hand transfer, stick override, release, discharge/impact, exit. Vehicle entry/exit, deliberate driving turn, Fulton and table/sandbag climb at four starting headings; fixed HMD heading must not inherit forced actor alignment. |
| iDroid/progression — FP07/12/13/14 | Left-hand contact/projector and readable both-eye motion; ordinary and forced stow/immediate reopen; authored-scene return and exit notice; Development and nested dialog navigation without unintended purchases; sortie preview and native free-roam route; physical binocular inspection/zoom/acquisition/explicit marking preserved. Complete a naturally available mission through its objective and return; do not call a lesson a mission pass. |
| Cabin — FP09/10/11 | Both-hand paper/marker contact, write/type/pin, front-slot printing, stacking and cassette presentation; both native rifle assemblies carry/return and reject held-grip reacquisition across interruptions. Recheck already-passed cases only when the changed code affects them. |

Review final composition in both eyes and through transitions, not only endpoint
stills. Preserve exact candidate/run identity and failed cases. For sustained
claims, the existing catalog requires at least 180 seconds/five cycles per
feature and a 600-second integrated soak; a short slice cannot replace these.
Measure fresh XR submissions, repeats, stalls and actual runtime period with
unchanged per-eye recommendation/FOV. Capture FPS is not runtime performance.

Retain only reviewed evidence using `python tools/workspace.py pin` (run
retention, not build promotion). End with `python tools/workspace.py stop-sim`:
release inputs, restore poses/settings, close test-opened menus and owned
processes, restore Meta runtime; keep Steam running. A failed test does not
authorize leaving Pause, a terminal or a background harness open.

### 4. Physical Quest Link review and release decision

Launch the checked candidate through `Play-Dev.cmd` on wired Meta Quest Link.
The user performs physical reach/scale, seated/standing, wrist isolation,
handset readability, sight/contact, tracking recovery and comfort checks during
the combined gameplay flow. Simulator observations cannot close these reports.
Record the actual feedback against the exact candidate; do not repeat old
hardware feedback as acceptance of the new build.

Any new reproducible crash, progression lock, forced view rotation, lost hands,
stretched legs or broken established interaction blocks a claim that it is fixed.
Repair affected code and rerun its affected group; do not restart every unrelated
case. No promise of a same-day full-game certification: the mission ledger still
has no full-mission accepted passes across its 51 required missions/variants.

Present one release decision: delivered fixes, actual remaining defects,
unproven combinations, physical feedback and exact candidate. If unknowns remain,
the experimental release must disclose them. The existing `--allow-known-issues`
packaging option is not a default way to bypass review; a release with those
specific blockers needs an explicit decision on the concrete candidate.

### 5. Authorized packaging, publication and pin

Confirm the requested destinations and unused tag at release time. Proposed tag:
`experimental-2026-10-24`, only if it is still October 24 and that tag is unused.
Do not reuse the date automatically on a later day.

```powershell
python -B tools/regression_catalog.py --release
python tools/package-release.py --tag experimental-2026-10-24
```

Packaging requires clean committed source matching BUILD.json, passing checks,
matching public defaults, notes and complete payload hashes. If post-review docs
change the source identity, refresh/check development once before packaging and
verify whether runtime binaries changed; do not substitute a different DLL for
the reviewed one without appropriate revalidation.

Verify ZIP contents, source archive, launcher, licenses, default settings and
checksum; exclude owned assets, saves and private diagnostics. Publish only when
authorized to the confirmed server/GitHub destinations. Verify uploaded payloads
against the local checksum. A draft or source push is not publication completion.
Only then run `python tools/workspace.py pin-build` for that exact candidate.
Verify both Play paths and retain exactly `play/` and `build/dev/`; cleanup of
recognized obsolete builds uses `tools/clean-local-builds.ps1 -Apply` only after
both slots verify. Never remove settings, saves, source or retained evidence.

## Resume record

- Completed on main: documentation/source audit; previous body/head, garment,
  reach and interaction repairs; flat native cuffs with independent shading and
  correct repeated attribute-slot binding. Gameplay source `5ba674f`.
- Checked candidate `351d84672f0e`: 58 groups and installer checks pass. Both
  slots verify; published play remains October 19.1 (`b627c7cacc9a`).
- Combined affected review completed: nine cases in each body mode, including
  left iDroid stow/immediate reopen. Both final eyes reviewed. The rejected
  intermediate selector regression remains recorded; see CURRENT and the ledger.
- Native Afghanistan travel/fire diagnosis did not establish a terrain repair
  or actual intact anti-air mounting. Do not repeat combat-blocked approaches.
- Remaining: physical Quest feedback, extreme cloth crowding/other outfits,
  exact terrain and mounted/progression cases, sustained performance. Keep
  implemented fixes distinct from missing coverage; five conservative gates remain.
- Cleanup complete: full_body1, owned game/simulator/readers closed, Steam
  retained. Both Play commands/saved headset preference select Meta. Windows'
  global OpenXR registration is Virtual Desktop; an attempted Meta restoration
  was denied by registry permissions. See CURRENT for this precise limitation.
- Extreme-reach follow-up: the captured fixture requests106cm from a53.9cm
  native arm chain; dense comparison does not justify replacing its fallback.
  Current mesh penetration remains unproven. Do not repeat that impossible-pose
  test as evidence of a new tracking defect; see CURRENT.
- Next: process physical feedback on this candidate and diagnose a newly
  reproducible remaining failure before editing. No package/publication/pin or
  release-gate waiver. Outcome documentation may be newer than packaged prose.

- Sustained follow-up found a reproducible handheld stale-skin camera publication
  failure in cycle six. Source correction and stall/recovery regression are in
  this batch. Build completed: all 58 groups and installer checks pass. Retained
  combined run `20261025T044511277442Z` passes all 27 cases, including Pause and
  immediate reopening. The prior run stays failed. Candidate `a6dc5fb91151`
  is ready through Play-Dev. Next: physical Quest feedback and a newly
  reproducible field case; frame repeats remain measured and unresolved.

## Current execution checkpoint — 25 October, completed batch

Steps 1–3 below are complete: `d4b526e24cf5` is installed and ready through
`Play-Dev.cmd`. Continue with the user's play feedback; do not rerun this batch
or resume historical simulator loops. Remaining limits are in FIELD_FIXES.

1. Finish the existing code batch on main: rigid prosthetic shell, retained
   iDroid reopen camera/stereo, and forearm crop cache replacement. Preserve
   fixed leg lengths, native interaction mappings and exact outfit admission.
2. Commit code and this handoff, then run `tools/build.ps1 -SkipBootstrap -Deploy`
   once. Required build checks are part of that batch. Repair a concrete build
   failure if one appears; do not launch repeated gameplay/soak loops.
3. Leave the checked candidate installed and available through `Play-Dev.cmd`
   for the user. Keep Steam running, Meta Link selected, and all test sessions
   closed. Record exact build/deploy outcome in the ledger.
4. The remaining terrain/corridor location, actual additional mounted contacts,
   other outfits/extreme cloth, broader progression and physical acceptance stay
   explicit. Do not invent repairs or reopen already implemented work merely
   because acceptance is incomplete. Use the user's next gameplay feedback.
5. Publishing and changing the published pin remain separate from deployment.

This checkpoint supersedes the repeated simulator workflow in historical plan
sections above. The user wants accumulated code delivered as one candidate.
