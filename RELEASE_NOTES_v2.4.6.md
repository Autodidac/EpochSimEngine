# SandHybrid v2.4.6

This release is cut from the fully cleaned `main` branch and supersedes v2.4.5.

## Included fixes

- Restores scene start/reset framing to one complete 640x360 authored footprint.
- Restores maximum zoom-out to a 1280x720 view covering four authored footprints in a 2x2 layout.
- Prevents authored beams, walls, hives, tanks, platforms, and machines from collapsing because of incomplete direct-below support sampling.
- Allows a uniformly atmosphere-filled finite map to reach edge equilibrium instead of remaining permanently active at its extremities.
- Makes volcano lava and gas output local, pressure-driven, bounded, and unrelated to ordinary gas activity elsewhere.
- Repairs conveyor, sluice, smelter, assembler, habitat, machine wake-up, inventory-capacity, and feed/output conservation paths.
- Places resource-critical debug information first and adds activity-by-cause counters for liquids, gases, structural collapse, machinery, volcanoes, chemistry, actors, and ecology.
- Makes `FINE ACTIVE` and `SLEEPING` clearly distinct in hue and brightness.
- Uses SandHybrid for project-owned targets, binaries, packages, UI text, namespaces, logs, tests, scripts, workflows, and documentation while retaining external proper names such as EpochGui.

## Validation gate

The release must pass the complete Windows 2022 and Ubuntu 24.04 C++23 Release matrix, all 12 shader builds, all deterministic contract tests, installation, archive creation, and package upload before publication.

Runtime-observed missions remain active in `missioncache.md` until verified in the packaged Windows Vulkan build.
