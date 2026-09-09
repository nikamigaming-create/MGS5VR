# Controls field card

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
illustration remains the public-release quick card. The attempted bitmap update
did not return a usable image; the companion uses code-native type and rules.
