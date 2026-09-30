# Immediate handoff — September 19

User priority: restore visible, live 3D first, then finish the tester fixes and release. No flat-screen workaround, speculative feature work, long log searches, repeated permission questions, or duplicate SIM sessions. The user explicitly requested this concise handoff; coding stopped.

## Do these in order

1. **Remove the black framing from the stereo scene.** User's latest instruction: "DROp the black framing mate." The scene now renders behind two opaque black UI rectangles; do not replace it with a flat screen. Identify and suppress the cinematic mask/letterbox draw in stereo while preserving subtitles and actionable menus. Current suspect path is `ui_renderer.cpp::node`, which remaps front-end layout layers onto a panel without clipping their original screen bounds. Do not guess a draw-order filter: current active orders are 133, 171, 196, 210, 252 and scene-camera order 9. Reuse the running SIM and verify fresh both-eye geometry during a small head turn after the correction.
2. **Prove the complete entry route.** Native title/menu → visible Continue → loading/Start Mission confirmation → controllable prologue, remaining in stereo when a 3D scene exists. Keep real loading UI actionable over retained stereo surroundings. Never guess a New Game selection or alter saves to manufacture a pass.
3. **Check tracking/body fixes.** Controller occlusion and idle must not black out the world. Check horse/body alignment after mounting, recentering, hiding and posture changes. Verify left support hand stays attached during motion, and hands/weapons remain visible when prone/near geometry.
4. **Check controls/UI fixes.** Pause/back/restart, visible controls editor, bindings/manual agreement, smooth-turn setting reload, readable menu aspect, and context/pickup prompts without wrist-angle gating. Distinguish handheld iDroid from Pause.
5. **Finish gameplay defects.** Verify turret, mortar, tank and Pequod aiming; binocular marking and lag; native firing obstruction versus the tracked muzzle. These are not yet established as fixed.
6. **Finish installation/resolution.** Verify manual DLL+INI enables VR, launcher preserves personal settings, and actual game rendering above 4096 works on suitable hardware. Resolve Infinite Heaven/IHHook's existing `dinput8.dll` conflict without overwriting its loader. No compatibility solution has been implemented.
7. **Release only the verified build.** Run release build, full CTest and recorder tests after final code changes. Update controls/known issues, installed-file hashes and the issue matrix. Build a new asset-free package with matching source/DLL hashes. Do not publish the rejected RC1 or claim full prologue/headset acceptance without evidence. Stance indicator and physical squat were suggestions, not implemented fixes.

## Exact current state

- Workspace `D:\code\MGS5VR`, branch `main`, HEAD `9bf8922e352d62c39d65058513a747131343b5eb`. Many changes predated this task: **do not reset the working tree**. Baseline diff: `artifacts/release-rescue-20260919/starting-changes.patch`.
- Latest built and locally installed DLL SHA-256: `BF605D9B4ABA967D0C57579890471BF7FBF73D40380670D7A1ED3E1C3153945E`. Game: `D:\SteamLibrary\steamapps\common\MGS_TPP`.
- Live game PID **36672**, Meta XR Simulator PID **33288**, gameplay runner PID **23208**. Recheck liveness, reuse them; no request is pending. Runner session `18ff5495-a179-4079-8f3f-ad05e2e47fa0`.
- Build passes. All 14 CTest suites passed before the last render-liveness correction; the latest DLL passes **7141 contract checks**. Four recorder tests passed earlier. Run the full suite once after the final correction.
- The earlier build introduced a blank-SIM regression. Latest correction allows the demo camera to take over: fresh evidence shows both native camera identities applied and new stereo pairs advancing. **The latest capture shows hospital geometry again, but large opaque black rectangles still obscure both eyes. Visual acceptance remains open.**
- Latest inspected captures: `artifacts/gameplay-proof/handoff-black-framing-20260919-{left,right}.png`. Latest completed capture request: `35e683a0-a3c6-474e-9561-56f679746f91`. Earlier `rescue-render-owner-hospital` captures actually show the KONAMI logo and are not hospital acceptance.
- Recent sessions start in mission **10010**, `gvars.ini_isTitleMode=true`, sequence `Seq_Demo_StartHasTitleMission`; the native title widget is hidden during the intro. No actual Continue selection or full prologue completion has been verified.

## Camera correction to review first

- `src/head_camera.cpp`: scripted shots preserve native position/pitch/roll instead of freezing the initial upright title camera. Initial camera still requires a matching player publication. A different render camera may take over only after the accepted camera stops publishing for 500 ms; secondary cameras cannot cancel a live primary.
- **Latest critical fix:** `resolveCurrentForRig()` evaluates skin before rendering. It must neither advance `lastView_` nor adopt a replacement camera. Otherwise a retired player camera looks perpetually live and blocks the actual demo camera. Only a render publication now advances `lastView_`.
- `src/xr_runtime.cpp`: title detection uses native TitleMode rather than visibility of its widget. Hospital title uses `ControllerFrame.authoredCamera`; helicopter title retains its cabin anchor. The attempted `nativeScreen` fallback was removed. Planned loading may retain accepted stereo surroundings.
- `src/ui_renderer.cpp`: removed menu-plane reprojection of scene-camera geometry; retained per-eye source matrices. Layout menus still map to panels. Pause uses its anchored 1.6 m panel; iDroid alone uses the handheld screen. This change still needs live visual review.
- `src/native_actions.cpp` / `src/proxy.cpp`: read-only title-state detection is always installed for the supported TPP adapter; optional developer commands remain gated.

## Other implemented changes awaiting live acceptance

Controller-occlusion skin publication; horizontal head-offset convergence; persistent acquired support grip; head-readable context prompts; native held buttons and movement/look input during unsupported/scripted scenes; loading handoff guard; visible WinForms controls editor; 8192 configuration bounds with a passing real 4160×4160 DXGI test; VR-enabled downloadable INI with explicit installer modes preserved. Experimental cassette/actor cabin defaults off. Existing mounted aiming, binocular and control fixes were already in the starting tree—do not call them newly verified.

## Minimal operating commands and cleanup

- Build: `cmake --build --preset release --parallel 6`
- Checks: `ctest --preset release --output-on-failure`; `python -m unittest discover -s tests -p record_simulator_tests.py`
- Live request: `python artifacts/gameplay-request.py <steps.json>`. Example: `[{"op":"capture","label":"max-live-baseline","eyes":["left","right"]}]`. Only one pending request. Inspect its returned PNGs.
- Bounded camera evidence: `artifacts/release-rescue-20260919/camera/matrices-36672.jsonl`; inspect only recent camera/status rows if the current capture fails. Do not reread historical logs.
- Installed INI temporarily has `camera_evidence_dir=D:/code/MGS5VR/artifacts/release-rescue-20260919/camera`. Restore the original empty value after diagnosis. **Installation-record hashes are stale after direct DLL copies**; update them with the final verified files, preserving settings and ownership.
- Original installed DLL/INI/controls/install record are backed up as `artifacts/release-rescue-20260919/installed-*`. No saves were edited.
- Keep this single SIM view available for the user. Before a necessary DLL swap, stop the runner gracefully, close the game normally and allow up to 30 seconds for XR cleanup. Stop an orphan SIM only after the game has exited. Start one replacement session, then one runner; do not kill the game to recover a runner.
- `dist/MGS5VR-2026-09-19-rc1-rejected-flat.zip` and its directory are **rejected**. RC1's flat-title visual evidence and `candidate-source.patch` are obsolete. Nothing has been publicly released or committed.

## Verified continuation update — 2026-09-19 21:44 PT

This additive update supersedes earlier PID, DLL-hash and capture-state observations in this historical handoff. Earlier sections are preserved for provenance.

- `mgsvtpp.exe` PID **32528** and `MetaXRSimulator.exe` PID **6340** are still running. Gameplay runner PID **30936** is stopped (`ready=false`, `stopped=true`); no request is pending. The last read-only native query reported mission `10010`, sequence `Seq_Game_GameOverBeforeSmokeRoom`, title mode off, and no active demo. No gameplay inputs or save edits were made during this continuation.
- Revalidated at **21:38 PT** through the native read-only pipe: `10010|false|Seq_Game_GameOverBeforeSmokeRoom|false|false`. The same game/SIM PIDs and stopped runner remain; built and installed DLL hashes are unchanged.
- Latest built Release DLL: `build/Release/dinput8.dll`, SHA-256 `A228EFF720ECA874C926B5A48E67F25A72603EF41AC2ABFBD976E60D01D00DC6`, 1,181,184 bytes. The live installation still has the old DLL, SHA-256 `B1ED7E441D341A38C0AC1E22274BE31D36C80BAD35D8F7BAD5BB888549C8F823`, 1,179,136 bytes.
- The pause/game-over quad now has a separate scale and leftward recenter for its right-authored rows; title canvas and handheld iDroid layouts are kept separate, and the name/appearance layout stays enlarged. Unit regressions cover both eyes. The previous read-only eye pair at `artifacts/gameplay-proof/fresh-priority-state-20260919-{left,right}.png` came from the old DLL: it shows pause rows clipped on the right, without opaque black rectangles. This does not rule out brief black frames.
- Scripted demos now pin the first accepted world camera pose across native camera animation/cuts while retaining physical HMD translation and rotation. Tracking loss preserves that anchor; leaving the demo restores native camera motion. Regression checks cover same-camera animation, camera-object cuts, tracking recovery, live physical head movement, and camera restoration. This preserves native stereo geometry and actors in code, but full-body visibility and comfort are still pending final-eye review.
- Full-body visibility policy is now a named pure rule used by the native camera observer: scripted demos reveal the native model, and gameplay hides it again. Core regressions cover inactive VR, first-person gameplay, demo visibility, and return to gameplay; live model/culling behavior remains unverified.
- Final validation after these source changes: Release build succeeded; CTest **14/14**, recorder tests **4/4**, native presentation fixtures **13/13**, native cabin lifecycle fixtures **43/43** passed. `git diff --check` found no whitespace errors (only existing line-ending normalization notices).
- Do not copy the build DLL over the live install. After the game exits normally, install this build and verify a fresh both-eye view through the bed name/appearance quad, a scripted prologue scene, the crawl/hide route, first-person body/hand visibility and the Afghanistan horseback handoff. Capture continuous final-eye evidence; the old low-cadence left-eye clip is not temporal or stereo acceptance. Full prologue and tester-issue completion remain open.
