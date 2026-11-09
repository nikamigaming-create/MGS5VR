These files describe MGS5VR's authored native cabin animal scene. They contain
engine property metadata, two cabin locators and references to the player's
retail assets; they contain no retail mesh, texture, motion or script payload.

`cabin-rats.fox2.xml` is the editable layout. `cabin-rats.fox2` is its FoxTool
0.2.6 compilation, embedded in `tools/cabin-actor-data.cs` so the asset importer
has no extra executable dependency. Changing it requires recompiling the XML,
updating the embedded base64 and checking the compiler roundtrip. Pointer labels
are local graph identifiers, not current process memory addresses. Preserve
their retained values, entity identity counters and explicit empty-string
elements: normalization changes the FoxTool binary, even when XML readers see
equivalent text. Both the normalized variant and restored graph initially crashed a cold scene;
the failure was ultimately isolated to FPKD resource ordering, not graph IDs.
Failed evidence is retained; parser success alone did not validate native loading.

The embedded ROUT v3 table describes two separate four-node walking loops at
Y=1197.305. Route 00 uses X=-0.35/-0.55, Z=-0.50/-0.30; route 01 uses
X=0.35/0.55, Z=-0.45/-0.65. Both use native route-node waits of 300. Their native
StrCode32 identifiers are fb33a487 and 86358b80. These are authored positions,
not proof that actors were visible in any particular game run.

The importer reads rat geometry, motion, parts and target data from the owned
retail mission packages with exact content checks. It augments all three native
helicopter scenes (40010, 40020, 40050), preserving other package payloads and
references. The prior experiment only augmented 40050, leaving Afghanistan and
Africa without cabin rats.

FPKD assets must follow the observed native resource type groups. Appending the
rat parts after Lua left the native parts FilePtr null and crashed initialization.
An order-only cold test, with identical payloads and runtime DLL, fixed that load
failure. The importer moves only its dataset, parts and target into their groups;
all other file ordering, payloads and references are preserved. Native models
then loaded, but visible realization/pickup still require separate live acceptance.
