# Repository Work Instructions

Before planning, editing, reviewing, building, or releasing SandHybrid:

1. Read `missioncache.md` and `MISSION_LEDGER.md`.
2. Preserve every non-COMPLETE mission and its acceptance criteria.
3. Update mission status and evidence in the same source commit.
4. Never report a visual/runtime mission COMPLETE from static token checks alone.
5. Build and test Windows and Linux Release packages before publishing.

Behavior and release invariants:

6. Preserve the v2.5.3 macro-tile baseline: complete moving liquid and gas tiles remain macro-eligible, with fine simulation fallback when a packet cannot commit.
7. Use the exact hard-coded suspended Ecosystem hive that SimpleSandSim Fix36 restored from immediately before PR #19: queen centers (512,234) in Sandbox and (512,232) in Ecosystem, loose Wood perch offsets x=-37..29 and y=-16..-13, shell 25 <= radius^2 < 92, chamber radius^2 < 25, queen at zero, and exit x=1..10 with |y|<=1. Share that model with the build tool and loaded-map normalization. Keep SandHybrid bee population and behavior; import no SimpleSandSim bee runtime behavior.
8. `PAUSED` freezes simulation, actors, clocks, MAP refresh, and effects, but painting, erasing, filling, igniting, selection, and blueprint placement remain live without advancing the simulation.
9. Inventory is sidebar-only and owns exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`.
10. Designer is sidebar-only and owns its isolated authoring grid and exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`; it must never replace the main world viewport.
11. Public releases must be normal visible releases with stable tags and names: no prerelease marker and no `-test` public tag.
12. Half Water falls first, merges with adjacent Half Water, attracts only across a clear 2-4-cell gap, never uses generic full-Water diagonal wandering, and may sleep only when no fall, merge, attraction, reaction, heat, or motion is pending.
13. World cursor controls belong to Editor in the sidebar. Designer keeps independent cursor controls in its sidebar-only workspace. Visible world cursor position, shape, and radius must use the same logical-window input mapping and effective brush policy as the committed edit, including during resize and high-DPI presentation; keep logical input geometry distinct from the physical framebuffer; do not show a clamped ghost cursor while the pointer is in the sidebar.
14. Blueprint slots are shared by the sidebar-only Inventory and Designer, must report honest occupied/empty state, preserve exact authored cell payload and transformed dimensions, and perform one complete bounds/material validation before either resident world buffer changes. A Blueprint click never also paints, fills, mines, or deposits.
15. Player presence or mining mode may suppress direct Editor painting only while running. While `PAUSED`, direct Editor mutations and Blueprint paste remain live in every scene and queued one-shot mutations are discarded across reset/load.
