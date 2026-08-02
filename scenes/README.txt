SandHybrid scene images

Each scene is stored as a binary PPM (P6) image for the editable 640x360 authored footprint.
The authored footprint is horizontally centered at the top of the resident world, and the default camera starts there.

F5 or the SAVE button writes only the current authored footprint. F9 or LOAD reads the selected scene image, places it at the same upper-center origin, and rebuilds the shared world outside it:

- one 8-cell Stone foundation at the authored-zone bottom;
- three 640x360 subterranean zones containing Sand, Soil, Silt, Mud, and Stone;
- one 8-cell Stone cap above the world-bottom Lava;
- one 16-cell/two-brick Lava band;
- one 8-cell Stone bottom shell and 8-cell Stone side shells.

This keeps procedural scenes and loaded saved counterparts identical outside the editable image. The common geology and world shell are intentionally not duplicated inside every PPM.

Missing built-in scene images are generated automatically the first time the procedural fallback is reset.

material_key.txt and material_key.ppm are generated beside the scenes. Use their exact colors for lossless material editing. Other colors are mapped to the nearest material.

Structural rule: a structural material needs at least 32 represented pixels of the same material in its aligned 8x8 region. Below 32 it becomes loose and crumbles. Regions from 32 through 51 remain structurally weak; 52 or more receive full structural durability.
