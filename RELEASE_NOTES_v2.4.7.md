# SandHybrid v2.4.7

This release repairs fractional water, terrain tile placement and stability, wet-sand sluicing, debug sampling, authored scene bounds, experiment clarity, and fertilizer mechanics.

## Water, terrain, industry, and debug

- Inspected fractional fresh water is labeled `HALF WATER`.
- Half-water exists only as a solid-supported pre-fall ledge state. It does not hop along exposed water edges, propagate across a water surface, or crawl after falling.
- Sand, dirt, silt, salt, ice, and other terrain-forming materials place as aligned structural 8x8 packets in `TILES` mode.
- Supported dirt tiles remain stable until legitimate damage, support loss, phase change, or collapse.
- Sand and silt absorb new water only in a shallow exposed band. Their represented wetness is not deleted by a drying timer.
- A water-supplied sluice accepts wet sand or silt and conserves output as process water plus trace gold.
- `PAIR TESTS` and `SKIPPED` preserve the last completed simulation sample instead of clearing to zero on render-only frames.

## Scene upgrade

- All nine authored scenes retain their original object coordinates.
- Each scene receives a sparse one-brick resident-width floor and one-brick side walls without dense filler, repeated scenery, stretching, or relocation.
- Engineering adds separated H2/O2 diffusion and paired compost control/treatment experiments.
- Gold Mine adds a powered conveyor, wet-feed sluice, water downcomer, and output line.

## Fertilizer mechanics

- Ember produces ash or fire only and no longer creates fertilizer directly.
- Compost requires ash, organic waste or food, silt/dirt/mud, dirty water, oxygen, and time.
- The treatment converts one compost feed cell to fertilizer while cleaning the matched dirty-water cell.
- Material cards, documentation, authored experiments, and static validation describe the same mechanic.

## Validation policy

Publication requires successful Windows 2022 and Ubuntu 24.04 C++23 Release builds, all 12 shaders, all deterministic contract tests, installation, archive creation, and package upload. Runtime-observed missions remain active in `missioncache.md` until verified in the packaged Windows Vulkan build.
