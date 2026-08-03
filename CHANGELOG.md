# Changelog

## 2.5.5

- Added a platform-neutral fixed-capacity packed Atmosphere model for N2, O2, Ar, CO2, Ne, H2, He, water vapor, and contaminants with exact conserved transfer, enrichment, respiration, and oxygen-percentage contracts.
- Added exact all-or-fallback material packet transactions and proved a complete 8x8 Water packet matches represented fine-unit transfer without partial mutation.
- Separated actor occupancy from gas/liquid state and added conserved respiration, drowning/suffocation evaluation, and bounded medium impulses.
- Added atomic directional machine recipes, deterministic seeded sluice transactions with separate solid/Water outputs, and explicit Ant/Beetle habitat policy.
- Advanced the reusable core API to version 3, installed the new headers, added four cross-platform contracts, and reconciled every active mission without falsely closing runtime-only work.
- Advanced Windows and Linux packages and release automation to v2.5.5.

## 2.5.4

- Prevented freshly reset enclosed Atmosphere/Oxygen tiles from blanket activation; `ACTIVE TILES` now excludes clean enclosed air.
- Removed mouse-edge camera movement, made middle drag four times more responsive, and retained native mouse capture outside the viewport.
- Added live pause and `PLAYER WASD`/`WASD PAN` GUI controls; camera mode also enables right-drag panning without triggering erase/deposit.
- Made every block-capable `TILES` placement durable: surviving structural stone, metals, ores, glass, wood, plastics, and machines no longer auto-crumble from occupancy, minority-material, or support heuristics.
- Prevented powder-phase block-capable materials such as Iron Ore from entering whole-tile macro movement.
- Added deterministic loose Iron Ore, Copper, Aluminum, and deeper Uranium deposits to shared generated/loaded geology while preserving sky, scene, shell, and lava bands; Gold remains authored/special.
- Corrected authored bee-home metadata to the crystal-row Y=720 origin and canonicalized legacy loaded hives to the exact Fix28 prefab.
- Made wetted Sand/Silt release stale structural state and sink through liquids.
- Added one-shot fine-boundary ownership and bounded gas expansion so water surfaces and Atmosphere edges can reach stable sleep.

## 2.5.3

- Integrated the protected `agent/nonoverlap-core-hygiene` handoff while preserving its source branch.
- Completed MC-065 with a deterministic CI contract rejecting coroutine machinery from simulation code.
- Completed MC-083 with a repository-wide SandHybrid branding contract while preserving proper external EpochGui and EpochEngine names.
- Completed MC-117 by reconciling the handoff onto current `main` without dropping later mission-cache work.
- Added both core-hygiene contracts to pull-request and Windows/Linux release validation.
- Moved every generated and loaded authored scene to the crystal-marker camera row at world Y=720, leaving the two complete resident rows above as sky and moving the shared foundation/geology stack down with it.

## 2.5.2

- Restored the compact SandHybrid Fix28 Beehive model as the single generated/buildable prefab and renamed material ID 31 to Beehive without aliases.
- Renamed save ID 48 to Iron Ore, retained loose-cell gravity, and enabled structural tile placement alongside refined Iron.
- Replaced hashed scene-image colors with unique paint-friendly RGB values close to rendered cells; Save, Load, and both material keys now share one palette.
- Reorganized the mission cache around explicit P0/P1/P2 priorities and removed embedded release-history bulk.
- Consolidated release history into this file and removed obsolete versioned release-note files/workflows.

## 2.5.1

- Added the common upper-center authored scene, three subterranean geology zones, and stone-wrapped two-brick bottom lava band to generated and loaded scenes.

## 2.5.0

- Established the reusable library architecture and initiated the sparse 64x64 section-grid production rewrite with dirty rectangles, safe phases, halo wakeups, sleeping, and 512x512 stream-page coordinates.
