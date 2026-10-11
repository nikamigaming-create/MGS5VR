# NVG visibility fix — October 11

NVG's native goggles model obstructed the first-person camera. The night-vision
effect itself worked. Equipping already-owned NVG at safe Mother Base reproduced
the user's black/yellow obstruction in both eyes on DLL `da914b1531d7`.

`src/player_visibility.cpp` now conceals the exact normal goggles and rain-cover
model resources in the player's typed item interface. The native asset paths are
`/Assets/tpp/item/thm/Scenes/thm0_main0_def.fmdl` and
`/Assets/tpp/item/thm/Scenes/thm0_rain0_cv0.fmdl`. Their geometry is not redistributed.
The existing native group visibility helpers clear the normal-draw bit and keep
shadow flags. Ownership is revalidated before hide and restore. The change does
not modify the NVG filter, equipment selection, ordinary hands or binoculars.

## Verified development

Use `Play-Dev.cmd` for DLL `02b9d914dd8c`. `Play.cmd` continues to restore the
published `experimental-2026-10-10.1` pin, DLL `ba5c0ccbf406`.

- 55/55 automated test groups and installer checks passed.
- Actual safe Mother Base NVG on: clear view in both eyes, native green effect.
- Yaw ±45°, pitch ±35° and positional lean: both eyes remain unobstructed.
- Native None selection: ordinary night view restored in both eyes.
- Native NVG re-equip: clear green view immediately restored in both eyes.
- Before model group flags `15,15,15` become `11,11,11`; only bit 4 is removed.

The bounded test calls the shipping `Player.ChangeEquip` Lua API with the
already-owned NVG ID and `toActive=true`, then the retail item-slot API for None.
It does not grant items or funds. The earlier shipping `DEBUG_ChangeEquip`
attempt did nothing. `Player.SetVisibilityToEquip` also ignored the ITEM slot;
that attempted fixture is rejected evidence. Neither is the implementation.

## Retained evidence

`artifacts/dev/helicopter-rewards-current/` contains:

- `nvg-actual-before/`: actual obstruction and native game/build identity.
- `nvg-native-hide-check/`: read-only identification of the offending model.
- `nvg-after-fix/`: both-eye captures for each pose and the two toggles.
- `nvg-after-fix/video/simulator.mp4`: 35 seconds, 102 real left-eye compositor
  frames; measured capture cadence about 2.89 fps, not game frame rate.
- `nvg-before-after.png`: labeled matched-view still comparison.
- `nvg-review-1.png`, `nvg-review-2.png`, `nvg-video-review.png`: reviewed sheets.
- `nvg-acceptance.json`: scoped review, identities, hashes and cleanup receipt.
- `nvg-build-checked.log`: automated and installer results.

The normal model's simulator visibility is accepted for this slice. Rain weather,
the physical wrist picker's NVG slot, physical headset comfort and complete-game
playability remain outside this evidence. Original saves were restored, test
inputs released, the game/simulator closed, Meta OpenXR restored and Steam retained.
