# MGS5VR Test Field Kit

A separate local developer tool using the launcher's field-manual design. It
does not change the player build or ship inside the RC5 player ZIP.

Start a regular Meta XR Simulator + MGSV session, then run:

```powershell
.\tools\start-test-field-kit.ps1
```

The launcher opens the authenticated local page. Click **Connect simulator**,
review both eyes and telemetry, and use **Continue to field** if needed. Select
scenarios and click **Run selected**. The queue runs independently of the page
and stops at the first failure. The game need not own Windows keyboard focus.
The desktop service is localhost-only; it is not a remote unattended machine.

The Python server can also run directly:

```powershell
python tools/test-field-kit.py --game-dir 'D:\SteamLibrary\steamapps\common\MGS_TPP' --proxy '<absolute-path-to-meta-xr-operator-mcp-proxy.exe>' --open
```

## Supervisors

**Human** selects the next bounded scenario in the UI. **LLM agent** lets an
external agent choose the same scenarios through the local API. Selecting LLM
does not start or connect a model. Ordinary scripts need no LLM or API key.

Both read the exact same `/api/state` payload: native telemetry, build/config
identity, timestamped paired compositor stills with SHA256, action history,
selected supervisor generation, and completed results. The UI renders those
fields. Images are sequential eye captures, not simultaneous stereo video.
The current telemetry and the captured observation have separate timestamps.

Switching supervisor interrupts a running queue, releases inputs and restores
the selected scenario's equipment/posture when identifiable. Old decisions
are rejected. Human **Stop + release** remains available in LLM mode. One
Windows input lease and one worker serialize all live game operations.

Use `artifacts/test-field-kit/connection.json` for the current local URL and
session credentials. Do not publish this file. The supplied client defaults
to the LLM role:

```powershell
python tools/field-kit-client.py --connection artifacts/test-field-kit/connection.json state
python tools/field-kit-client.py --connection artifacts/test-field-kit/connection.json observe --wait
```

An agent reviews `observation.captures` (the local PNG paths or authenticated
media URLs), then supplies that observation ID and one reviewed image hash:

```powershell
python tools/field-kit-client.py --connection artifacts/test-field-kit/connection.json run --scenarios equipment commands binoculars --observation-id '<reviewed-id>' --image-sha256 '<reviewed-image-sha256>' --wait
```

Input decisions expire after 30 seconds and are checked against fresh native
mission, sequence, scene and control context before execution. The API accepts
catalog IDs only; it exposes no arbitrary shell, Lua, or raw-input execution.
Its session tokens, Host/Origin validation and media allowlist restrict browser
access. Local processes with access to the credentials are trusted supervisors.

## Initial library and evidence

- Equipment picker hold/release.
- Commands hold/release.
- Physical VR binocular equip/stow, preserving the existing VR bindings.
- Standing → crouching → prone → crouching → standing.

These checks need safe field gameplay. They do not prove binocular target
acquisition/marking/zoom, combat, all cinematics or mission completion. The
initial library never opens Pause or changes presentation modes. Continue
handles recognized startup states and stops at unknown prompts.

Results live under `artifacts/test-field-kit/<timestamp>-<scenario>-<id>/`:
installed identity, effective bindings, exact suite/hash, native case outcomes,
events, both-eye images and final observation. Archive loading checks image
hashes. **NATIVE PASS** means the predicates passed; visual acceptance remains
separate. The coverage page reads the existing 55-report release ledger and
labels that historical evidence separately from this session's runs.

Add a guarded suite under `tools/gameplay_bot/suites`, register a catalog entry
in `tools/gameplay_bot/fieldkit.py`, implement scenario-specific cleanup if it
changes persistent state, add relevant tests, and run it in-game before calling
it verified. Do not expose arbitrary unsupervised actions to fill coverage gaps.

**Stop + release** stops the queue; **Disconnect runner** gives up the input
lease. Neither kills the game. The server can remain open for archived review.
Release approval stays with the user after headset testing.
