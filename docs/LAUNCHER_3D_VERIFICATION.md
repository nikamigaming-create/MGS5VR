# Embedded field-terminal verification — 2026-09-27

The native launcher now opens `MGS5VR-FieldKit.exe` by default; `--classic`,
headless commands and the original self-tests retain their behavior. The UI is
served from packaged local files through a restricted WebView2 virtual origin.
There is no Python server dependency in the shipped launcher.

## Original artwork and copy restored

The launcher again uses the unmodified original Snake artwork from
`docs/images/controls.png` and `field-header.svg`: the portrait, helicopter,
rifle illustration, charcoal lettering and red field-manual rules. Only
illustration viewports are reused; the historical card's printed bindings are
excluded. The art files are packaged locally and tracked as build dependencies.
The cassette models now have bevelled shells, tape reels, hubs, moulded edges,
screws and recording labels. Page headings, help, lesson descriptions and all
46 tuning descriptions use direct labels and instructions instead of slogans.

`artifacts/launcher-3d-qa/field-manual/result.json` records the complete browser
and real-bridge regression check after the visual changes. It passed saved
remapping and replay, all eight modes, all 96 action locations, automatic
rotation, 3D picking, settings widgets and minimum-width layout with no browser
errors. `field-manual-host/host.json` and adjacent screenshots verify the updated
packaged app with the installed game configuration. This is a launcher update;
it does not change the native DLL or close the remaining headset checks.

## Evidence

- `artifacts/launcher-3d-qa/browser/result.json`: a real bridge write in a
  disposable installation changed binoculars from left grip + Y to left grip +
  X. Reopening the lesson lit X and kept the exact same video URL. All eight mode
  cards, 39 sliders, five switches, two enumerations, fractional-value save,
  automatic camera rotation and a raycast click on 3D A passed. No browser errors
  or instructional elements over the gameplay video. Minimum-width check passed.
  All 96 action references resolved their physical 3D locations. A fresh numeric
  inventory contains 7,435 literals in 84 runtime files and is searchable in
  Field notes. Inventory coverage is distinct from a completed review.
- `artifacts/launcher-3d-qa/host-final/host.json` and adjacent screenshots:
  real embedded WebView2 loaded the installed configuration, both controller
  models, ten lesson excerpts from two source recordings, mode guide and all 46 settings. This read
  did not change the installed INIs or launch a game.
- `tests/launcher_bridge_tests.ps1`: 18 assertions cover the serialized schema,
  exact fractional values, validated save, byte-exact backup, conflicting mapping
  rejection, stale revision rejection and invalid runtime rejection.
- Eight selected CTests passed: classic launcher contracts/UI, display setup,
  headset launcher, controls configuration, effective binding export, legacy
  settings editor and the new settings bridge.

The source videos retain their original hashes and source-eye provenance.
Controller framing and cues are instructional views of the current binding;
they are not a reconstruction of measured finger/hand motion in the take.

## Rebuild and repeat

`cmake --build --preset release --target mgs5vr_launcher --parallel 4` builds the
Win32 entry point, .NET Framework host and local UI bundle. The WebView2 SDK is
pinned to 1.0.3537.50 with SHA-256
`5EA526BBD728ADDA0DA4D31219267E96460494A427E4894C4E09D9F320F4B9AA`.
The package retains the WebView2, Three.js and controller-asset licenses.

`tools/qa-launcher-3d.cjs` requires Playwright and the local read-only development
server at port 8766. It uses the actual PowerShell settings bridge against its
own fixture. `MGS5VR-FieldKit.exe --smoke-output <directory> --game-exe <path>`
captures the embedded pages read-only and disables launch/install/maintenance.

Public release acceptance is still open. These launcher checks do not certify
the unresolved native Pause panel fit, iDroid hologram alignment, all community
reports, or every historical feature.
