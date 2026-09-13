# Original artwork and current field-manual layouts

The original **controls.png is preserved unchanged**. Its Snake portraits,
controllers, helicopters, landscape and grenade sketch define the visual identity:
warm ivory, charcoal, signal red and distressed field-manual headings.

Current README and guides reuse pieces of that original in
[field-header.svg](field-header.svg), [controls-quick.svg](controls-quick.svg),
[field-gear.svg](field-gear.svg), and [control-modes.svg](control-modes.svg).
The native **MGS5VR Field Terminal** embeds the same original image. These are
code-native viewports/layouts, not newly generated replacements for Snake's art.
The old bitmap's obsolete mappings must never be presented as current controls.

Regenerate with `python tools/field-art.py` and
`python tools/render-control-schema.py`. The latter uses
[CONTROL_SCHEMA.json](../CONTROL_SCHEMA.json) for the detailed bindings.

## Historical September 8 card

[controls.png](controls.png) is the illustrated quick reference for the
2026-09-08 experimental build. The accessible, detailed equivalent is
[CONTROLS.md](../CONTROLS.md). That guide also records mounted/menu differences
and the unfinished actions; the card covers normal on-foot play.

Created with the built-in image-generation tool and reviewed against the current
input mappings. This is an illustration, not a photograph of the hardware or a
game screenshot. No game archive assets were extracted for it.

The design prompt requested an MGSV tactical operations field manual: warm ivory,
charcoal and signal red, technical controller illustrations, readable button
callouts and a three-step wrist-picker sequence. Binding text came from the
current Touch controls. A second targeted edit moved the right GRIP callout to
the pale inner side button, separate from the black outer TRIGGER button.

The card shows the intended bindings. It does not certify all physical controller
gestures or close the gameplay gaps listed in the guide.

The development companion [control-modes.svg](control-modes.svg) includes the
additional menu, Commands, zoom, melee and vehicle mappings. Its editable source
is [CONTROL_SCHEMA.json](../CONTROL_SCHEMA.json); run
`python tools/render-control-schema.py` to regenerate it. The September 8
illustration is retained as source artwork and a historical card, not the current
binding reference. Current layouts reuse its illustrations with updated text.
