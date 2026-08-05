# Repository Work Instructions

Before planning, editing, reviewing, building, or releasing SandHybrid:

1. Read `missioncache.md` and `MISSION_LEDGER.md`.
2. Preserve every non-COMPLETE mission and its acceptance criteria.
3. Update mission status and evidence in the same source commit.
4. Never report a visual/runtime mission COMPLETE from static token checks alone.
5. Build and test Windows and Linux Release packages before publishing.

Behavior and release invariants:

6. Preserve the v2.5.3 macro-tile baseline: complete moving liquid and gas tiles remain macro-eligible, with fine simulation fallback when a packet cannot commit.
7. Import only the Fix29 Beehive shell, chamber, exit, and chamber contents; do not import SimpleSandSim bee population or behavior.
8. `PAUSED` freezes simulation, actors, clocks, MAP refresh, and effects, but painting, erasing, filling, igniting, selection, and blueprint placement remain live without advancing the simulation.
9. Inventory is sidebar-only and owns exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`.
10. Designer is sidebar-only and owns its isolated authoring grid and exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`; it must never replace the main world viewport.
11. Public releases must be normal visible releases with stable tags and names: no prerelease marker and no `-test` public tag.
