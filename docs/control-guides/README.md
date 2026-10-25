# Control guides

For current work use the [release plan](../RELEASE_PLAN.md). These guides are
references, not acceptance or a separate implementation backlog. Regenerate only
when effective defaults or the renderer change; preserve personal bindings.

The four packaged PNG pages use the public default controls. They are generated
from **effective bindings**, exported by the same native parser used in the game
and launcher:

1. Move and fight
2. iDroid and menus
3. Optics and equipment
4. Driving and travel

Each PNG is 2480 × 3508 and uses the launcher's Field Terminal artwork,
paper grain, charcoal panels and signal-red control callouts. The controller
images are rendered from its actual Quest Touch Plus GLB models through the
shared `ControllerPlayer`, including authored button poses and views that
expose the selected face, grip or trigger. They are not drawn substitutes.

The launcher also exports a selected page from its
current saved bindings; use **PNG control guides → Export PNG** in the training,
mode or controller view. Save any remap first, then export fresh pages.

To regenerate the public set after building development, run from the checkout:

```powershell
node tools/export-control-guides.cjs --controls config/mgs5vr-controls.ini --checker build/dev/mgs5vr_controls.exe
```

The offline exporter needs Node.js, the Playwright package and Microsoft Edge.
To export a personal set, pass your installed `mgs5vr-controls.ini` and a separate
`--output` directory, or export from the launcher. An existing dependency bundle
can be selected with `NODE_PATH`. The exporter uses a headless
browser to render the shared guide module.
It does not operate the desktop, launch the game, inject inputs, change an INI,
or modify the pinned build. `manifest.json` records the exact controls file,
effective binding catalog, checker, renderer and original artwork/model hashes.
`effective-bindings.json`
includes the complete action catalog, including optional bindings not printed
on the quick-reference pages.

These are control references. They are not a claim that every vehicle, weapon,
tutorial, mission or physical headset interaction has passed acceptance.
