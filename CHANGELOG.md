# Changelog

## 2.5.13

- Attempted the first 28 P0 missions as tranche one of five, with deterministic evidence retained per mission.
- Fixed Half Water ambient-Air restoration to canonical balanced Air and prevented pending two-to-four-cell attraction pairs from sleeping.
- Enabled macro-first movement for complete exposed Water/gas packets with same-tick fine fallback when a packet transaction cannot commit.
- Anchored reset generation/effects to time zero, cleared queued edit/actor inputs, and stopped map-snapshot refresh while paused.
- Revalidated packed Atmosphere, actor-medium impulses, Fix28 hives and 100-bee ecology, machinery transactions, continuous ground, scene origins, saves, deposits, and workspace input ownership.
- Restored MC-123 as the packaged cross-system acceptance gate and made mission-cache validation reject P0 references without active rows.
- Consolidated release notes into the current `RELEASE_NOTES.md`, removed obsolete checked-in Fix33 packages, and kept `CHANGELOG.md` as the sole historical release record.


## 2.5.12

- Restored MAP as an independently zoomable/pannable top overlay without replacing the simulation view.
- Kept world cursor and placement controls in Editor and added separate Designer-only palette, brush, placement, zoom, and isolated 64×32 grid state for blueprint authoring.
- Extended the shared terrain surface through every authored scene while preserving non-empty authored structures.
- Added visible simulation-time day/night and sunlight-driven presentation.
- Made pause freeze world mutation and render effects, and made reset rebuild lighting/map/effect state and view framing.
- Added v2.5.12 source contracts and carried all unaccepted blueprint, lighting, runtime, and performance work forward in `missioncache.md`.

## 2.5.11

- Replaced gameplay PPM saves with exact, versioned, checksummed whole-world `.shw` saves.
- Added atomic publication, previous-save backup, corruption fallback, manifests, sanitized slots, and size/scene-aware folders.
- Added Compact, Standard, and Large startup selection through launchers and command-line options.
- Kept PPM as authored-scene import/export only and carried packaged save/load and in-window selector acceptance forward.


## 2.5.10

- Extended natural ground across every column outside the authored scene without moving authored objects.
- Replaced blocky mineral scatter with coherent complete-tile vein cores, curved sand traps, and sparse localized rubble pockets.
- Smoothed vertical and horizontal biome transitions with deterministic dither bands.
- Restored the Volcano water lake by using authored-local width and added a retained crater lava lake plus a larger pressure reservoir.
- Removed the liquid surface age cutoff, increased pressure/drop look-ahead, reduced viscosity gating, and added more equalization passes.
- Separated workspace bodies, removed overlapping captions, restored 31-cell structural collapse, and replaced solid tool beams with sparse pixel bursts.


## 2.5.9

- Restored the early v2.4.1 fine-owned Half Water attraction/consolidation behavior while keeping full Water and Air eligible for validated macro packets.
- Made Lava continuously mobile and cohesive, enabled full-tile Lava packets despite heat, propagated vent pressure through connected Lava, and added pressure-driven overburden breakout.
- Added multi-minute Magma Vent pulse cycles with faster recharge and a larger end-of-cycle eruption window.
- Replaced the Volcano scene's framed pool with a curved natural lake basin.
- Painting Smoke into liquids or gases now shifts represented media upward instead of deleting it.
- Removed generic Acid-to-Silt corrosion; inorganic corrosion produces Dirty Water and Silt remains a sediment product.
- Restored the early suspended Beehive geometry and loaded-scene normalization.
- Added functional workspace tabs, descriptive top-control labels, friendlier material-card `NONE` values, coherent render-only material appearance profiles, and complete cell/tile dimension telemetry.
- Added Compact 4x4, Standard 8x4, and Large 16x4 camera-footprint world presets to the public policy API.
- Expanded `missioncache.md` with the material-regression archaeology and screenshot evidence for the next completion pass.

## 2.5.8

- Falling same-material cells now repair an existing damaged structural tile on contact instead of waiting to settle into a separate tile.
- Acid and other supplied low-viscosity liquids level into basin pools; balanced Air expands into vacuum; Smoke rises through Air.
- Rebuilt the debug state legend around exact overlay precedence and restrained edge colors that preserve material identity.
- Split cursor Air flood fill from the new Heat-category `IGNITE AIR` world action.
- Renamed the visible Atmosphere utility control to Air while retaining the Atmosphere material-card identity.
- Isolated Half Water from ambient-Air pressure bookkeeping and restored canonical Air after consolidation.
- Widened the sidebar and moved the Cursor/card stack below the complete keymap to remove overlap.

## 2.5.7

- Keeps exposed Water and Atmosphere fine-grained while retaining enclosed-medium sleep metadata.
- Removes stale sleeping classification gates that could freeze productive motion.
- Reclassifies Lava as a dense semi-solid slump medium; connected Lava does not cool to Stone.
- Makes Magma Vents persistent producers that stop only against solids and pressurize existing Lava.
- Adds reusable CPU/GLSL terrain generation with complete tile-first metal veins and loose resource cells only inside deliberate collapsible sand traps.
- Enlarges the Volcano scene into a map-scale cone and natural lake.
- Adds complete live debug hierarchy totals for scope/nonempty cells, total/active/sleeping/unclassified tiles, and total/active/sleeping/dirty chunks; movement rows now use their real counter names.

## 2.5.6

- Aligned the complete authored scene to region origin `(1280,720)` and propagated that origin to reset, load/save, actor reset, player spawns, bee homes, camera home, metadata, and contracts.
- Removed generic internal scene side walls, kept a continuous full-width foundation, and limited structural containment to the outer resident-world perimeter.
- Migrated legacy boundary-connected Empty sky to Atmosphere on Load while preserving sealed vacuum pockets.
- Rebuilt Volcano as a Lava chamber with local pressure-backed crater/floor vents that emit Lava or hot Ash/Smoke/Steam into Atmosphere and recharge instead of silently stopping.
- Made Ash settle as particulate powder and prevented failed macro candidates or stale sleeping chunks from freezing productive fine movement.
- Removed per-tile/per-chunk checkerboard paint from MAP and added an explicit screenshot-evidence release ledger to `missioncache.md`.
- Removed the stale unused secondary-button press local after right-click became camera-only, restoring warnings-as-errors runtime builds.
- Aspect-fitted both the live camera and full-world map view from their actual visible dimensions; the 16x4 map now renders at its true 64:9 shape instead of stretching to the simulation panel.
- Reshaped the 64-footprint world from 8x8/4x4 assumptions into a resident 16x4, 10240x1440 world extended to the right while placing the complete authored scene at aligned region origin (1280,720).
- Replaced the sparse 17-region starburst with a contiguous clipped 4x4 camera active window of at most 16 complete 640x360 regions.
- Updated generated scenes, saved-scene offsets, bee homes, camera reset, full-world map framing, and all player spawns to the preserved scene coordinate system.
- Fixed Half Water horizontal crawling and tightened ledge splitting to require a supported three-cell water supply.
- Made exposed full Water/Atmosphere packets perform one fine handoff and then sleep immediately when unchanged, eliminating long-lived 8x8 checkerboard activity.
- Smoothed balanced Atmosphere rendering and reduced medium debug/map overlays to restrained edge markers so Water, Half Water, and air remain visible.
- Stabilized resident mud and ore veins as authored structural geology so they no longer collapse into false voids that wake Atmosphere and liquids.
- Redesigned the sidebar into explicit Scene, Simulation, View, and Tool rows; Reset and Pause are paired, and Atmosphere/Erase/Fill remain large and visible.
- Synchronized shader-rendered sidebar geometry with the C++ hitboxes so the redesigned controls and lower panels remain aligned.
- Changed debug/map hierarchy visualization to preserve Atmosphere, Water, and Half Water colors while showing state through restrained tile edges and markers.
- Replaced scattered per-cell mineral noise with dense deterministic ore veins that preserve rough natural edges while greatly reducing mixed/broken terrain tiles.
- Added MC-121 for a versioned editable selection-blueprint copy/paste system.
- Fixed expanded-world actor spawning to use the same authored scene origin as scene generation and saved-scene loading.
- Added a compact clickable four-slot player inventory; BUILD mode now places the selected owned resource with left click and decrements its count.
- Added an independent slow-refresh full-world debug map with separate camera state, active/inactive region visualization, and the live simulation-camera rectangle.
- Restored camera edge panning only while right-click is held, made right-click camera-only, removed middle-mouse camera behavior, and hardened WASD routing.
- Changed `F` into a held fill modifier that requires a left click inside the simulation viewport.
- Swapped the upper soil/sand geology order and replaced ruler-straight layer and mud boundaries with deterministic cell-scale variation.
- Revised the canonical mission cache and advanced Windows/Linux release automation to v2.5.6.

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
