# Owned item and prop catalog

The local catalog joins the installed game's archive inventory, extracted
equipment registrations, existing weapon fit records, declared script
references, model measurements and item observations. It answers where an
object is, which versions contain it, whether a model has bones, and what
interaction or animation evidence has actually been recorded.

The October 12 scan indexed 34,303 FPK/FPKD packages across 15 readable installed
archives, including the update archives. It located 6,886 model paths, including
139 models under `item/`, and 416 animation/container resources. There are 780
equipment definitions after joining the 486 retained native registration rows
and the individual weapon fit worklist. Definitions include grades, contextual,
enemy and demo equipment; this is not a count of player-usable items.
The native `Item` type has 62 definitions, including contextual and variant
entries. The summary retains every equipment type's count.

The database is `private/item-catalog/catalog.sqlite`. Its scan receipt and all
generated data remain private. Model content hashes distinguish versions;
archive membership does not resolve patch precedence. Equipment source hashes
are checked against the retained extraction before importing its records.
The scanner reads archives without changing the game or exporting payloads.

```powershell
python tools/workspace.py items summary
python tools/workspace.py items view
python tools/workspace.py items search cassette
python tools/workspace.py items search /item/ --category models
python tools/workspace.py items show EQP_IT_Cassette
python tools/workspace.py items show /Assets/tpp/item/pcp/Scenes/pcp0_main0_def.fmdl
python tools/workspace.py items search player2 --category animations
python tools/workspace.py items refresh
```

`refresh` uses the configured owned game and the local GzsTool dependency. It
replaces generated metadata after a completed scan. An interrupted scan cannot
replace the completed database. Hand-authored observations in
`tools/item-observations.json` survive rescans and require an evidence reference.
The local browser view is `private/item-catalog/index.html`; it searches 8,082
equipment/model/animation records and includes reviewed native geometry
thumbnails for the 139 item-folder models. A thumbnail is displayed only when
its content hash matches the indexed model. These views do not reproduce native
materials, skin animation or gameplay.
The browser filters entries with recorded observations, shows the remaining
coverage gaps and exports the current selection as JSON. Square geometry
previews preserve each object's proportions.

Each item needs separate answers for identity, scene availability, model/rig,
animation binding, native use, collision and VR interaction. A bone count does
not establish animation playback. A clip in the same package does not establish
that the clip drives a particular prop. `show` explicitly labels those nearby
resources and retains unverified bindings. Native and mod-authored behavior are
recorded separately. Existing unrun weapon fit checks stay unrun.

The scan recognizes all 20 installed data files: 15 asset archives and five ASF
media containers. The latter were first incorrectly passed to the QAR reader;
their mandatory header GUID and local `ffprobe` results identify them as video
files, and they are now retained as media records. The format's header is
documented in [FFmpeg's ASF definitions](https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/asf_tags.c).
The current scan completed with zero archive/package errors.
The denominator remains incomplete: 34,498 top-level asset names are unresolved.
Finding all named
models is not proof that every interactive object, cutscene binding or item
variant is understood. Individual clips inside motion archives are not yet
enumerated. Full-game and physical headset acceptance remain separate work.

No native instant camera has been confirmed. The candidate `cpr` model was
visually identified as hospital scissors, and `cfb` remains an unidentified
prop. Neither is used as a camera. The cabin's existing cassette player is
embedded in the helicopter interior; `pcp` is a separate two-bone cutscene
player model. The latter is not drawn as a duplicate cabin player.
