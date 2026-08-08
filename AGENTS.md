# Repository Work Instructions

Before planning, editing, reviewing, building, or releasing SandHybrid:

1. Read `missioncache.md` and `MISSION_LEDGER.md`.
2. Preserve every non-COMPLETE mission and its acceptance criteria.
3. Update mission status and evidence in the same source commit.
4. Never report a visual/runtime mission COMPLETE from static token checks alone.
5. Build and test Windows and Linux Release packages before publishing.

Behavior and release invariants:

6. Preserve the v2.5.3 macro-tile baseline: complete moving liquid and gas tiles remain macro-eligible, with fine simulation fallback when a packet cannot commit.
7. Recover the photographed historical SimpleSandSim Fix29-era Sandbox hive body/content cell-for-cell: shell `24 <= radius^2 < 88`, chamber `radius^2 < 24`, queen at zero, right exit `x=1..10` with `|y|<=1`, and canonical contents `hash32((y * 640 + x) ^ 0xD17A5EED)`. Preserve queen centers `(512,234)` in Sandbox and `(512,232)` in Ecosystem. Reset, tool, and load normalization share this model. Its Wood perch spans absolute local cells `x=472..543`, `y=216..223` as nine complete aligned structural `8x8` tiles; hive-body cells override overlapping perch cells. Keep SandHybrid bee population and behavior; import no SimpleSandSim bee runtime behavior.
8. `PAUSED` freezes simulation, actors, clocks, MAP refresh, and effects, but painting, erasing, filling, igniting, selection, and blueprint placement remain live without advancing the simulation.
9. Inventory is sidebar-only and owns exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`.
10. Designer is sidebar-only and owns its isolated authoring grid and exactly two nested tabs: `INVENTORY` and `BLUEPRINTS`; it must never replace the main world viewport.
11. Public releases must be normal visible releases with stable tags and names: no prerelease marker and no `-test` public tag.
12. Half Water falls first, merges with adjacent Half Water, attracts only across a clear 2-4-cell gap, never uses generic full-Water diagonal wandering, and may sleep only when no fall, merge, attraction, reaction, heat, or motion is pending.
13. World cursor controls belong to Editor in the sidebar. Designer keeps independent cursor controls in its sidebar-only workspace. Visible world cursor position, shape, and radius must use the same logical-window input mapping and effective brush policy as the committed edit, including during resize and high-DPI presentation; keep logical input geometry distinct from the physical framebuffer; do not show a clamped ghost cursor while the pointer is in the sidebar.
14. Blueprint slots are shared by the sidebar-only Inventory and Designer, must report honest occupied/empty state, preserve exact authored cell payload and transformed dimensions, and perform one complete bounds/material validation before either resident world buffer changes. A Blueprint click never also paints, fills, mines, or deposits.
15. Player presence or mining mode may suppress direct Editor painting only while running. While `PAUSED`, direct Editor mutations and Blueprint paste remain live in every scene and queued one-shot mutations are discarded across reset/load.
16. The Water Half flag `0x00800000` is reserved state, never random entropy. Every CPU/GPU constructor for ordinary full Water must mask random auxiliary bits with `0x007fff00` so full Water cannot be misclassified as Half Water.
17. The fragment shader converts physical framebuffer coordinates to logical window coordinates before applying the shared UI/world layout. Native input stays logical; framebuffer scale is presentation-only.
18. Generated scene terrain and authored structural/liquid starting volumes use complete aligned `8x8` tiles. Resident grass/dirt/foundation begins at each scene's intended surface row, and intentional empty authored interiors are never backfilled by resident substrate. Fine-authored actors, vegetation, smoke, loose cargo, and the canonical cell-resolution hive remain exempt.
19. Designer exposes an explicit sidebar `CLEAR` action that empties only its isolated authoring grid. Editor `FILL` remains a click-confirmed four-connected Air replacement and stays live while paused.
20. Gold Mine, Demolition, and Frontier Base reset with an enabled player at a supported, body-clear, breathable authored spawn; invalid configured spawns must recover deterministically instead of embedding, suffocating, or hiding the player.
21. Fixed 60 Hz simulation ticks, not presented frames, own materials, actors, clocks, effects, macro-packet cadence, and breakup age. A presented frame submits at most one complete tick and stale time debt is discarded to prevent a GPU catch-up spiral; one-shot input is consumed once. Complete macro packets attempt movement every two ticks and exposed packets survive two classifier ticks before fine breakup.
22. Before every SandHybrid release, fetch `Autodidac/EpochGui`, vendor the complete current dependency rather than selected headers, record the exact upstream commit/version in `third_party/EpochGui/SNAPSHOT.md`, and build its supported upstream tests. Never silently retain an older EpochGui pin.
23. Ignite Air belongs only to the sidebar ACTIONS section immediately above KEYMAP; normal materials are visually static except the existing metal/ore animation, while gas presentation derives only from authoritative gas state.
24. Presentation limits 30, 60, 120, and UNLIMITED never change the fixed 60 Hz simulation cadence. Debug collection is disabled at zero cost when hidden and bounded/asynchronous when visible.
25. Every valid scene has a supported, body-clear, breathable recoverable player spawn. Volcano follows the 2026-08-04 reference silhouette; Engineering, Industry, and Gold Mine machinery remain active, including the water-fed Sluice.
