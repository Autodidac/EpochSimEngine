SandHybrid demo: EpochSimEngine persistent World and legacy scene images

The normal runtime owns one connected persistent World containing the eight former
640x360 scenes as west-to-east districts in one canonical resident cell field.
There is no selectable scene carousel. Reset, simulation, weather, hierarchy,
actors, MAP, tools, and exact saves operate on that single World.

The P6 PPM files in this directory are compatibility and authoring inputs. A
legacy 640x360 image is mapped only into its matching World district during
migration; it is never a second live world and current SAVE does not export a
full-world PPM. Exact runtime SAVE/LOAD uses checksummed schema-2 world saves and
retains every canonical cell plus the complete actor owner. Schema-1 cell-only
saves remain readable through the documented migration path.

material_key.txt and material_key.ppm define the stable RGB import palette. Exact
key colors round-trip losslessly and nearby colors map to the nearest material.
Boundary-connected legacy Empty sky migrates to balanced Atmosphere while sealed
vacuum pockets remain Empty. Hive normalization reconstructs the current
saturated-golden, no-perch Fix29 body and its district-local queen/home metadata;
legacy PPM pixels do not authorize the superseded Wood perch or 100-bee colony.

Generated terrain, authored structures, and starting liquid/gas volumes enter the
resident World as complete aligned 8x8 ownership candidates. Fine-authored actors,
vegetation, smoke, loose process cargo, and the cell-resolution hive remain fine.
Canonical cells are always authoritative; tile/chunk state is reversible metadata.
