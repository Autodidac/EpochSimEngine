SandHybrid scene images

Each scene is stored as a binary PPM (P6) image for the editable 640x360 authored footprint.
The authored footprint is horizontally centered in the third resident camera row at world Y=720, where the crystal marker sits, and the default camera starts there. The two complete 640x360 rows above remain empty sky.

F5 or the SAVE button writes only the current authored footprint. F9 or LOAD reads the selected scene image, places it at the same crystal-row origin, and rebuilds the shared world outside it:

- two complete resident rows of empty sky above the scene;
- one 8-cell Stone foundation at the authored-zone bottom;
- one lower resident footprint containing Sand, Soil, Silt, Mud, and Stone;
- one 8-cell Stone cap above the world-bottom Lava;
- one 16-cell/two-brick Lava band;
- one 8-cell Stone bottom shell and 8-cell Stone side shells.

This keeps procedural scenes and loaded saved counterparts identical outside the editable image. The common geology and world shell are intentionally not duplicated inside every PPM.

Missing built-in scene images are generated automatically the first time the procedural fallback is reset.

material_key.txt and material_key.ppm are generated beside the scenes. Their stable RGB swatches closely match the visible simulation cells, so the PPM can be edited by eye in ordinary Paint. Exact key colors are lossless; nearby colors map to the nearest material.

Structural rule: a structural material needs at least 32 represented pixels of the same material in its aligned 8x8 region. Below 32 it becomes loose and crumbles. Regions from 32 through 51 remain structurally weak; 52 or more receive full structural durability.
