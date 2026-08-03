# SandHybrid v2.5.6

This release completes the runtime-control, map-debugging, scene-layout, Water, Half Water, and Atmosphere repair pass from `missioncache.md`.

## Runtime and interface

- Places Reset and Pause/Run side by side and keeps Atmosphere, Eraser, and Fill visible as distinct primary tools.
- Makes `F` a fill modifier that only executes with a left-click inside the simulation viewport.
- Restores right-click-held edge panning and right-click drag while preventing right-click from painting Oxygen or editing materials.
- Routes WASD explicitly between player control, simulation-camera pan, and the independent map camera.
- Adds a compact clickable Iron/Gold/Copper/Aluminum player inventory and resource-backed BUILD placement.

## World, scenes, and map

- Reshapes the same 64 map footprints into a 16x4 resident world: 10240x1440 cells.
- Extends the world only to the right; the authored 640x360 scene remains at world origin `(960,720)`.
- Updates generated scenes, saved-scene offsets, bee homes, camera reset, scene metadata, and Gold Mine/Demolition/Frontier Base player spawns to that preserved coordinate system.
- Replaces the sparse 17-region starburst with one clipped contiguous 4x4 active window of complete 640x360 regions around the camera.
- Adds a separate slow-refresh full-world map that defaults to the complete 16x4 frame, shows active/inactive regions, and outlines the live main camera without affecting simulation scheduling or LOD.

## Water and atmosphere

- Stops Half Water from crawling horizontally or chasing exposed edges.
- Requires a supported three-cell full-water supply before a ledge split can create Half Water.
- Lets unchanged Half Water and exposed full Water/Atmosphere packets settle instead of remaining in long-lived 8x8 checkerboard activity.
- Smooths balanced Atmosphere presentation and reduces debug/map hierarchy coloring to restrained edge markers so Water, Half Water, and air remain readable.

## Terrain and diagnostics

- Places soil above sand and replaces ruler-straight geology and mud boundaries with deterministic rough transitions.
- Clusters Iron Ore, Copper, Aluminum, and deep Uranium into concentrated veins, reducing false broken-tile noise.
- Preserves the exact structural collapse threshold and keeps runtime-only missions open until packaged observation proves them.

Windows and Linux Vulkan Release packages, source export, headless tests, mission-cache validation, shader contracts, and repository hygiene are required before publication.
