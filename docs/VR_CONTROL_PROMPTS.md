# In-game VR control prompts

The requested result is an inline native prompt whose control label matches the
effective personal VR action. Map Place Marker uses `menus.confirm`; changing
that action must change its prompt. Binocular marking uses `binoculars.mark`,
and must not inherit Map's Confirm binding. No extra floating legend is needed.

The local candidate resolves the native `<I=G=...>` input tags before the
game measures and lays out UTF-8 text. It uses the same validated
`ControlBindings` that drive input, including touch sensors, chords, gestures,
axis overrides and disabled actions. UI workers use immutable binding snapshots.
Translated captions, styling and non-input icons are preserved. Unidentified
tags retain the original native icon rather than receiving a guessed binding.
Native pad ownership retains native prompts. Map's plain action caption uses a
separate native owner: its translated text receives the effective Confirm label,
and only its authored Confirm image is suppressed while that label is available.
The image's previous opacity is restored when native-button mode resumes. This
adapter leaves the cursor, markers, caption group and unrelated widgets alone.

Examples with the current defaults:

| Prompt action | VR label |
| --- | --- |
| Map Confirm / Place Marker | A |
| Menu previous / next tab | L GRIP / R GRIP |
| Binocular explicit marking | RT |
| Binocular zoom | L CLICK |
| Reload on foot | TAP B + L GRIP |
| Equip physical binoculars | HOLD Y + L GRIP |

`LT` and `RT` mean the left and right trigger. Adjacent menu trigger alternatives
share one compact `[LT/RT]` chip. Other adjacent icons retain separate labels;
their meaning is not guessed from proximity.

The native text unit borrows its string pointer. Replacement strings are
immutable and retained for the module lifetime in a bounded pool. A signature
mismatch or exhausted pool retains native text. The candidate is selected with
`MGS5VR_VR_BUTTON_PROMPTS=1` or `[diagnostics] vr_button_prompt_experiment=1`
in the game INI. `MGS5VR_PROMPT_TRACE=1` or `vr_prompt_trace=1` enables bounded
native text traces. Environment switches take precedence. The candidate is not
enabled in ordinary builds until its
actual native placement and width have been reviewed. Build/unit passes alone
do not establish readable in-game chips or complete prompt coverage.

The shared parser replaced Toggle MAP/NAV, Reset Coordinates, Switch Zoom and
trigger zoom labels in both eyes; compact `LT/RT` fits the bottom-row slot.
The earlier formatted-text candidate overflowed that slot and missed image-only
paths. Retained run `20261001T225420725862Z` passed five scoped native cases:
Map opening, two physical default Confirm dispatches and native-button mode on
and off. Caption switching was reviewed in both eyes, but the stock A graphic
remained visible: that candidate failed graphic replacement acceptance.

Attempts to hide a caption sibling group or its nested widget also left the
stock A graphic visible. Those targets are removed from the implementation.
The direct Confirm-image build `822a8077d38a` passed seven scoped native cases
in retained `20261002T003629964618Z`: opening, two default Confirm dispatches,
native mode on/off, root Back and immediate reopening. Steady default/reopened
Map and both mode transitions were reviewed in both eyes. The stock A appears
only in native mode; VR mode shows `[A]` beside the original translated caption.

Retained `20261002T004055181967Z` passed five scoped native cases using a
disposable `menus.confirm=x + left_grip` override. Both eyes show
`[X + L GRIP] PLACE/REMOVE MARKER`, without the old A image. During both Confirm
dispatches the physical X/left-grip channels were sampled with native A=4096;
Back restored the camera/player control owner. The personal file was restored
byte-for-byte. Its immediate-reopen capture spans the native opening animation,
so that pair is not a settled stereo presentation pass. The reviewed scope and
capture hashes live in `artifacts/bot/acceptance/control-prompts-map.json`.
Exact marker-count mutation and physical headset readability are not certified.

Acceptance follows each prompt's native action and owner, then the actual
effective binding, held-input evidence and both final-eye images. Test default
bindings and a personal remap, including longer chords. Exercise Map, nested
Development, Help/tutorials, Pause, equipment, commands, physical binoculars,
horse/vehicle transitions and the native-button escape mode separately.
Fixed-width icon-only layouts, live edits on already-cached pages, ambiguous
semantic tags, alternate pad presets and every language remain explicit work.

The native adapter is specific to the verified TPP executable. Ground Zeroes
does not acquire prompt acceptance from these checks.
