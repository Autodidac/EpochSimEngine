# SandHybrid v2.5.13

This is the first of five planned P0 completion tranches. It attempts the first 28 P0 missions in canonical order, with Half Water and liquid hierarchy behavior as the primary runtime repair.

## Water, Half Water, and hierarchy

- Restores canonical balanced-Air state after Half Water consolidation instead of restoring the pure-Oxygen-style state.
- Prevents aged Half Water from sleeping while another half is visible two to four clear cells away.
- Gives complete 8×8 Water and gas regions a macro transaction attempt even at exposed boundaries, while retaining same-tick fine fallback when the exact packet move cannot commit.
- Keeps Half Water permanently fine-owned and excludes structural solids from whole-tile translation.

## Pause and reset

- Reset generation is now anchored to simulation step zero, so mineral, particle, lighting, and presentation effects return to the same deterministic initial state.
- Reset clears held painting and queued Fill, Ignite Air, fire-tool, and deposit actions so no input leaks into the rebuilt scene.
- The MAP snapshot no longer refreshes while paused; the complete world, map data, day/night clock, material animation, particles, actors, tools, and lighting remain frozen while camera/UI navigation stays available.

## World, ecology, and production coverage

- Revalidates continuous ground through all map columns, aligned scene/camera/bee/player origins, exact world saves, deterministic mineral deposits, the Fix28 hive, the 100-bee lifecycle, packed Atmosphere conservation, actor-medium impulses, packet transactions, directional machinery, and Editor/Designer input separation.
- Restores the missing MC-123 packaged cross-system acceptance row and strengthens mission-cache validation so every P0 reference must resolve to exactly one active mission.

## Validation and publication

- Headless C++ contracts, source/shader validators, ecology/hive audits, mission-cache validation, and downstream package consumption run in CI. Obsolete checked-in Fix33 packages and versioned note fragments are removed from the source tree.
- Windows and Linux native Release builds/tests produce the packaged assets before GitHub publishes v2.5.13.
- Packaged visual/runtime items remain active in `missioncache.md` until directly observed; this release does not falsely archive them from source-only evidence.
