SandHybrid scene images

Each scene is stored as a binary PPM (P6) image at the simulation resolution, currently 640x360.

F5 or the SAVE button writes the current scene. F9 or LOAD reads the selected scene.
Missing built-in scene images are generated automatically the first time the procedural fallback is reset.

material_key.txt and material_key.ppm are generated beside the scenes. Use their exact colors for lossless material editing. Other colors are mapped to the nearest material.

Structural rule: a structural material needs at least 32 represented pixels of the same material in its aligned 8x8 region. Below 32 it becomes loose and crumbles. Regions from 32 through 51 remain structurally weak; 52 or more receive full structural durability.
