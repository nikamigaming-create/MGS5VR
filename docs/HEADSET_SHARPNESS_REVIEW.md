# Physical recovery and sharpness — 2026-09-07

The user tested DLL
`7BF543BE8E4EC882295AD8491DA6CCEB88B98BB878120DDBC54C0D6C371B170A`
on the Quest 3 and reported that the experience was working well, with everything
else looking good for the moment. Low resolution and jagged edges, especially
when looking up, remain the reported problem. This is positive physical feedback
for the basic experience, including recovery from the earlier stereo complaint.
It does not certify every interaction, material, arm pose or loading transition.
The earlier failed builds remain failed; their results must not be substituted
for this specific build's evidence.

The live game renders each native eye at 1920x1080. The requested physical eye
optics select a 1546x906 region from each source image. This is a substantial
pixel-density limitation before headset presentation. Increasing only the size
of a copied image cannot recover missing scene detail. The game already has
ExtraHigh textures, texture filtering and model detail.

The preserved baseline includes the exact DLL, INI, installation record and
graphics configuration. After the user requested an immediate restart, the same
DLL launched on the Oculus runtime at native 2560x1440, confirmed by the game
present and swapchain logs. Quality settings are unchanged: 33% more pixels in
each dimension and about 78% more total rendered pixels. Native stereo resumed,
with roughly 60 complete pairs/second in the sampled interval. Visual improvement
and sustained performance still require the user's report; runtime counters alone
do not pass either gate.

The native Post Processing setting is currently Off. NVIDIA's game-specific
guide identifies High as enabling post-process anti-aliasing and ambient
occlusion, and notes that the AA can also soften the image. This is a separate
controlled test after resolution, with depth of field and motion blur kept off;
per-eye correctness and cost need checking before adopting it. It is also a lead
for the earlier flat-ground complaint, not a confirmed explanation of that
complaint. [NVIDIA graphics guide](https://www.nvidia.com/en-us/geforce/news/metal-gear-solid-v-the-phantom-pain-graphics-and-performance-guide/).

Do not change camera/eye geometry or the rig while evaluating sharpness. Check
actual native dimensions, submitted eye regions, visible edge/terrain detail,
both-eye agreement, native pair cadence and headset feel. Keep the working
1080p baseline available if higher resolution harms performance or presentation.
