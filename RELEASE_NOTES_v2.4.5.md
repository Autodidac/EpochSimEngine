# SandHybrid v2.4.5

This release restores the camera framing lost in v2.4.4 and repairs several simulation paths that made scenes, atmosphere, and industrial machinery appear broken.

## Camera

- Restores the one-map 640x360 starting/reset view.
- Restores maximum zoom-out to exactly 1280x720 cells, a 2x2 view of authored map footprints.
- Keeps pointer mapping, panning, and camera clamps tied to authored-map scale instead of incidental resident GPU dimensions.

## Scene stability and atmosphere

- Preserves authored structural assemblies through side and inherited support rather than requiring four directly-below support samples for every 8x8 tile.
- Retains the established fewer-than-32-of-64 collapse threshold for actual structural failure.
- Treats resident-world edges as sealed atmosphere boundaries so a uniformly filled map can reach zero gas-edge activity.
- Adds counters for structural failures and gas-edge activity.

## Industry and volcanoes

- Prevents sleeping tile/chunk optimizations from disabling conveyors, smelters, assemblers, sluice boxes, habitats, factory cores, or magma vents.
- Prevents machine feed from being consumed when the matching inventory slot is full.
- Makes smelters accept iron/aluminum feed, assemblers consume their documented recipe, and sluice boxes require nearby flowing water while conserving eight feed into seven silt and one gold.
- Adds a working conveyor/sluice line to the Engineering scene.
- Makes volcano lava/gas output local to the vent's own pressure and deducts pressure for every emission.

## Debug and branding

- Moves resident-memory and work-pressure information to the top of the existing debug panel.
- Adds meaningful activity counters for motion, gas/liquid transport, structural collapse, conveyors, machine input/output, volcano output, chemistry, actors, and bees.
- Makes `FINE ACTIVE` vivid hot magenta and `SLEEPING` dark cool blue, with the legend ordered by state relationship.
- Renames project-owned targets, namespaces, headers, binaries, packages, logs, workflows, and documentation to SandHybrid.
- Preserves proper external names such as EpochGui and EpochEngine integration references.

Runtime-observation missions remain active in `missioncache.md` until accepted in the Windows Vulkan build.
