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

The reviewed exact-owner Map adapter is enabled by default with
`[ui] map_control_prompts=1` in `mgs5vr.ini`. The launcher's Runtime & assets
settings expose this as **Map control labels**. Set it to `0` to retain the
native Map captions and icons; changes apply after restarting the game. The
adapter refreshes its label from live personal bindings and restores native
captions and image opacity when native-button mode resumes.

The general text/parser adapter remains opt-in with
`MGS5VR_VR_BUTTON_PROMPTS=1` or `[diagnostics] vr_button_prompt_experiment=1`.
Its disabled diagnostic switch does not disable the default Map adapter.
The Map footer uses the same product setting. The actual field replay on
October 2 proved native common-footer mode **82**, caption entries **35–38**,
and the exact caller, text-node owner, five-unit count and active row state.
Historical enum-name extraction had incorrectly assigned this page to modes
46/47, so the earlier adapter admitted no rows and both eyes retained native
icons. The corrected admission accepts only the observed mode82 rows. Typed
input tags identify Toggle MAP/NAV, Reset Coordinates, Switch Zoom and Zoom
In/Out. Other common footers retain their diagnostic policy. The native caption
bytes are retained for restoration, and the same native update refreshes a
cached footer after a neutral personal-binding edit or native-button mode
change. Native-button ownership bypasses both the scoped rewrite and generic
parser reentry, preserving the original markup.

Retained `20261002T145255433589Z` on candidate `600ec71d1b2a` passes six scoped
native cases: open, native-button mode on/off, Back, immediate reopen and final
Back. All 12 after-state eye images were reviewed at original resolution. The
four VR labels appear on open, mode-off and reopen; mode-on restores native
icons. The marker caption also restores correctly, and both Back pairs clear
the Map/device. The footer labels are tightly spaced at this presented scale;
longer personal chords, localized captions and physical-headset readability
remain unproven. The scoped manifest is
`artifacts/bot/acceptance/control-prompts-map-footer-600ec.json`. Sparse pairs
do not certify continuous temporal stability or every footer action's dispatch.
`MGS5VR_VR_BUTTON_PROMPTS=0` explicitly disables both adapters; the Map-only INI
opt-out remains effective even when the general parser is enabled.
`MGS5VR_PROMPT_TRACE=1` or `vr_prompt_trace=1` additionally enables bounded
native text traces when the general adapter is enabled.

The native text unit borrows its string pointer. Replacement strings are
immutable and retained for the module lifetime in a bounded pool. A signature
mismatch or exhausted pool retains native text. Other prompt families remain
pending native placement and width review. Build/unit passes alone do not
establish readable in-game chips or complete prompt coverage.

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
