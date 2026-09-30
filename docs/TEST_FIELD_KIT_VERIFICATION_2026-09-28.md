# Test Field Kit verification — 28 September 2026

The separate developer kit is implemented. Start with
`tools/start-test-field-kit.ps1`; usage and supervisor API are in
`docs/TEST_FIELD_KIT.md`. It preserves the launcher's illustrated paper, ink
and red field-manual design. The player DLL and RC5 release archive were not
changed or published.

## Verified locally

- 13 new contract tests pass: ownership, stale image/decision rejection,
  bounded catalog, HTTP authentication/origin/path checks, stop, cleanup,
  failed-queue termination and archive image identity.
- Related session and supervisor contracts also pass (11 checks each).
  All three selected CTest groups pass; see `artifacts/test-field-kit/ctest.txt`.
- Browser QA passes on desktop and at 650px width, with no horizontal overflow
  or JavaScript errors. All four library entries and 55 release reports render;
  filtering, page navigation and archived-eye review work.
- Four real regular-simulator scenarios passed against RC5 DLL
  `c69b75c6099d77d773b6503caee339c51ae33411d3c65062c2b0ba28c51ca79e`:
  equipment hold/release, Commands hold/release, physical binocular
  equip/stow, and all four standing/crouching/prone round-trip transitions.
  Eight native checks passed on the resumed Mission 6 field state (10040).
- A second live queue, recorded in the video, repeated equipment, Commands
  and binoculars successfully. It is real UI operation over the actual runner.
- Live handoff: an LLM-role API request started binoculars; changing to Human
  while the binocular context was active stopped the queue, stowed the optic,
  returned to gameplay and skipped the dependent posture scenario. No menu
  was left open. `live-handoff-result.json` records the deliberate STOP result.
  This exercises the external-agent API; no model-provider connection is bundled.
- Restarted the service and recovered all eight saved runs: seven native
  passes and one intentional handoff stop. Captured-image hashes were checked.
- Final cleanup released the input lease, stopped the owned game/simulator
  and service, removed only the unchanged task-created runtime override, and
  closed the recording browser. Personal INIs and the installed DLL retain
  their original SHA256 values. No save was overwritten or global runtime changed.

## Video

`artifacts/test-field-kit/demo/MGS5VR-Test-Field-Kit.mp4`

41.16 seconds, H.264, 1600×1200, 25 fps, 1,029 frames, no audio.
SHA256: `093ddf99f9e9f904071e68fca04d907ff3e1317ac44f5a871e608a5828ec7a2a`.

This is a browser UI recording. The gameplay panels display timestamped,
sequential final-compositor stills; they are not a continuous headset video.
The recording and contact sheet were reviewed. It shows queue execution,
native results, the supervisor switch, scenario library, coverage and archive.

## Scope still open

Native outcome passes are separate from full visual/headset acceptance.
These short scenarios do not prove binocular acquisition/marking/zoom,
Mission 1/6 completion, combat or whole-campaign regression. Those remain
explicit library/coverage gaps. Existing release evidence is historical and
does not change automatically when field-kit scenarios pass. Release approval
remains with the user after testing at home.

Full evidence: `artifacts/test-field-kit/`, including `browser-qa/`,
`demo/qa.json`, `demo/native-results.json`, `live-handoff-result.json`,
`archive-restart-qa.json`, and `session-cleanup.json`.
