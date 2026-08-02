# SandHybrid Mission Cache

This is the **single canonical product backlog** for SandHybrid. It contains active work, priority, acceptance criteria, permanent invariants, and concise accepted foundations. Release history lives only in `CHANGELOG.md`.

Before changing code:

1. Read this file.
2. Preserve every `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` mission.
3. Update status and evidence in the same source commit.
4. Never mark visual/runtime behavior complete from compilation or static audits alone.
5. Verify Windows and Linux Release builds before publishing.
6. Move a mission to the archive only after acceptance passes. Runtime contradiction reopens the same ID.
7. During the next broad pass, attempt every active mission once. Anything unfinished or runtime-unverified remains active with evidence.

Statuses: `OPEN`, `PARTIAL`, `REGRESSION`, `DEFERRED`.

## Priority lanes

- **P0 / primary release gate:** MC-038, MC-112, MC-115, and MC-116. These are the current release blockers and must pass deterministic contracts plus Windows/Linux Release packaging before publication.
- **P1 / runtime correctness:** simulation, conservation, water, atmosphere, ecology, machinery, UI, scene, and performance missions that require packaged observation or deterministic runtime evidence.
- **P2 / architecture and later integration:** streaming, full replacement-runtime cutover, optional subsystem extraction, and EpochEngine integration.
- Priority is a scheduling property, not a second status system. Every unfinished row remains in this table exactly once.

# Active missions

## Production rewrite and sparse world migration

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-101 | PARTIAL | Execute the production rewrite program | `REWRITE_PLAN.md` is the authoritative staged migration plan. Each stage lands behind deterministic contracts, preserves the old runtime until parity passes, updates this ledger in the same commit, and removes compatibility code only after Windows/Linux runtime acceptance. R1 is complete and R2 is initiated. |
| MC-102 | PARTIAL | Sparse 64x64 section metadata | The platform-neutral core owns signed sparse section coordinates, per-section dirty rectangles, four non-touching phases, boundary-halo wakeups, automatic sleep, and clean metadata retirement. Deterministic contracts pass. Runtime dispatch must still prove that clean sections execute zero material work. |
| MC-103 | OPEN | Canonical paged cell storage | Replace duplicated hierarchy authority with one append-only, save-versioned cell store. Hot fields use structure-of-arrays storage; half-water, moisture, atmosphere components, damage, temperature, and material identity survive page load/save exactly. |
| MC-104 | OPEN | Transactional liquid and gas packet acceleration | Eligible 8x8 liquid/gas packets are derived from canonical cells, validate into scratch state, and atomically commit or run fine fallback in the same tick. No ownership ping-pong, volume loss, hidden fill, or one-frame classification loop is permitted. |
| MC-105 | PARTIAL | Structural solids are metadata only | Stone, iron ore, refined iron, and other block-capable solids use 8x8 support/cohesion/sleep metadata without whole-tile translation. At 31 destroyed cells of 64, the remaining 33 release individually. Runtime fracture and persistence acceptance remain required. |
| MC-106 | OPEN | Canonical packed atmosphere | Atmosphere cells conserve N2/O2/Ar/CO2/H2/He/vapor/contaminants, pressure, and temperature. Painting individual gases modifies composition instead of replacing air, and closed-box tests prove conservation. |
| MC-111 | OPEN | Selectable atmosphere component gases | The palette exposes Nitrogen, Oxygen, Argon, Carbon Dioxide, Neon, Hydrogen, and Helium as distinct tools while retaining balanced Atmosphere. Append-only material/save IDs are preserved. In the replacement core, painting a component changes packed local composition and pressure rather than deleting the other atmosphere components; cards show identity, density, and local percentage. |
| MC-112 | PARTIAL | Stone, Iron Ore, and Iron cell/tile parity | Save ID 48 is canonically `Iron Ore`; no Iron Shavings identifier or alias remains. Stone, Iron Ore, and refined Iron use `CELLS` for loose individual pixels and `TILES` for supported structural 8x8 arrangements. Tile metadata never translates the solid wholesale, and at 31 destroyed cells the remainder releases as loose cells. Static ID, palette, phase, block-capability, and shader contracts pass; packaged placement/fracture proof remains required. |
| MC-113 | OPEN | Exact directional production transactions | Every industrial machine defines input material, power/medium requirements, processing latency, output material, input port, output port, blocked-output behavior, and a player-switchable direction where meaningful. One accepted transaction produces its documented goods on the output side or consumes nothing. Sluice transactions follow MC-088 exactly. |
| MC-107 | OPEN | Component actors and directional machinery | Player, insects, conveyors, sluices, smelters, assemblers, vents, and habitats use occupancy/components outside material identity. Machines expose configurable input/output ports, consume explicit input cells, produce explicit output cells on the opposite side, support player-switchable output sides/directions where applicable, and report accepted/rejected transactions. |
| MC-108 | OPEN | Sparse 512x512 stream pages | Eight-by-eight section pages allocate on demand, load before cross-page transfer, save modified distant pages asynchronously, and evict clean pages. Large mostly-static worlds avoid fixed full-world allocation and scanning. |
| MC-109 | OPEN | Section-driven Vulkan runtime | Vulkan consumes immutable section batches and dirty rectangles from the core, dispatches only active work, retains a CPU reference path, and reports separate simulation/debug/presentation timings. Shader code does not own world policy. |
| MC-110 | OPEN | Deterministic cutover and old-hardware gate | Old and replacement runtimes run identical seeded scenes and compare material totals, gas components, moisture, heat, damage, actors, and machine outputs. One-million-active-cell and mostly-static-large-world baselines pass on Windows/Linux and representative older four-core hardware before old hierarchy code is deleted. |

## Simulation hierarchy and settling

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-011 | PARTIAL | 64x64 section-first rejection | `SparseSectionGrid` now provides dirty rectangles, safe phases, halo wakeups, and sleeping metadata. The production runtime must consume those batches so clean sections execute zero tile/fine/material work while pressure and boundary exchange remain safe. Runtime profiling proves it. |
| MC-012 | REGRESSION | 8x8 bulk-element movement | Full aligned liquids, gases, powders, mud, and wet granular materials use the same gravity, diagonal, lateral, density, erosion, and displacement rules as fine cells. Block-capable solids such as stone never translate as whole tiles; tile metadata is limited to cohesion, support, sleep, and debug state. Real eligible scenes show non-zero bulk moves. |
| MC-100 | PARTIAL | Solid tiles never move wholesale | Full stone and other block-capable solid regions may report `BULK READY` and sleep through tile metadata, but only individual cells may fall after damage or support loss. No macro dispatch may swap a block-capable 8x8 region. Sand, soil, silt, salt, and ice retain granular stability qualification without granting block-capable reconstruction. At 31 destroyed cells (48.4375%), the remaining 33 cells crumble to fine loose cells. Static contracts pass; packaged runtime acceptance remains required. |
| MC-013 | REGRESSION | True liquid settling | Water levels quickly, reaches zero motion, and wakes only from support, pressure, volume, heat, reaction, actor, or tool disturbance. |
| MC-084 | REGRESSION | Half-water solid-ledge behavior and card identity | Inspected fractional fresh water is titled `HALF WATER`. Fractional-water creation and movement occur only as the pre-fall state on a solid-supported ledge. It never hops along exposed water edges, propagates across a water surface, or continues as a post-fall crawling/dripping artifact. |
| MC-014 | PARTIAL | Fractional-water consolidation | Fractional water represents only final surface volume; no lateral splitting loop, chasing, jitter, created air, or isolated pockets. |
| MC-015 | PARTIAL | Hide hierarchy artifacts | Fine repair prevents squares, popping, seams, grid-edge clumping, and diagonal one-cell ramps. |
| MC-016 | OPEN | Durable hierarchy terminology | Rename fine cells, 8x8 bulk elements, 64x64 sections, active starburst, frozen regions, and streamed regions in code, UI, debug, and docs together. |
| MC-017 | REGRESSION | Prevent premature stabilization | Liquid, mud, and wet material sleep only after unchanged volume, support, impulse, boundary height, and erosion state for a bounded confirmation window. Gas rests only after pressure, density, composition, temperature, and incoming/outgoing transfer reach equilibrium. Touching a solid is never by itself a gas-rest condition. No region may sleep while a valid downhill, buoyant, pressure-transfer, erosion, reaction, or actor-driven path remains. |
| MC-018 | OPEN | Bulk/fine parity tests | Deterministic tests prove each eligible 8x8 move matches the equivalent conserved fine-cell result, including Atmosphere displacement, mud erosion, gas boundary flow, and the same rest decision. |
| MC-019 | REGRESSION | Material-specific settling without gas-wall friction | Damping is not a universal per-move slowdown. Gravity, buoyancy, pressure transfer, mud erosion, falling wet material, reactions, boundary changes, tool input, and actor impulses execute at the full material-defined rate. Liquids and mud may use viscosity/internal drag only after no productive move exists, reducing residual lateral oscillation toward rest. Gases have no friction against solids: a wall blocks the normal crossing direction but never adds tangential slowdown or pins gas to the surface. Gas motion ends only through pressure/composition equilibrium, density sorting, or exhausted impulse. Bulk and fine representations make the same active/rest decision. |
| MC-078 | PARTIAL | Authored structures remain intact | Authored beams, walls, machines, tanks, hives, and platforms retain structural support unless phase change, mining/damage, or destruction reaches 31 of 64 cells (48.4375%, the first whole-cell count at or above 48%), leaving 33 or fewer represented cells. The remaining cells then crumble individually. Side-connected and suspended construction never collapses merely because four cells are not directly underneath an 8x8 tile. Runtime scene cycling must show no spontaneous disassembly. |
| MC-053 | PARTIAL | Transient gas/liquid tile packets | A complete 8x8 gas or liquid region may move as one tile packet. After packet motion ends, gas remains tile-owned only when its complete one-cell perimeter is gas; liquid remains tile-owned only when its complete perimeter contains liquid or non-gas matter. Exposed resting packets become `FINE_ACTIVE` and break back to canonical cells without deleting, filling, snapping, or changing volume. Runtime proof must show moving air/water tiles, exposed breakup, enclosed retention, and stable zero-motion results. |

## Atmosphere and closed-system gas

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-020 | REGRESSION | Composite Atmosphere | An append-only `Atmosphere` material now represents normal authored air and carries a breathable oxygen fraction without aliasing pure Oxygen. Full conserved volume, pressure, temperature, N2/O2/Ar/CO2/H2/He/vapor/contaminant packing and exchange remain required. Fire, lightning, and radiation remain effects. |
| MC-021 | PARTIAL | Earth-air baseline | Authored air and the Atmosphere action now use a distinct Atmosphere state with 54/255 (~21.2%) breathable oxygen rather than pure Oxygen. Exact packed N2/O2/Ar/CO2/trace composition, pressure totals, and conservation tests remain required. |
| MC-022 | OPEN | Absorb all true gases into air | Gas painting and emissions modify local Atmosphere composition and pressure instead of replacing it. |
| MC-023 | OPEN | Visible excess-gas settling | No nested mini-simulation. Excess denser than air visibly moves down then sideways until stable; lighter excess visibly moves up then sideways. It cannot cross solids, liquids, sealed barriers, paused sections, or unloaded boundaries. Solids block crossing but do not create tangential gas friction: gas slides freely along valid open boundary paths at its density/pressure-defined rate. |
| MC-024 | OPEN | Reabsorb excess, hide only later | Stable excess reabsorbs when compatible air has capacity. Transit remains visible until Adam accepts it; debug always exposes transport afterward. |
| MC-025 | PARTIAL | Respiration and combustion | Life and combustion convert available O2 to equal represented CO2 volume. Suffocation uses breathable partial pressure. Runtime rates require proof. |
| MC-026 | OPEN | Closed-box conservation tests | Track total Atmosphere and each component, pressure transfer, separated excess, reabsorption, and conservation error. Include wall-following tests proving that gas contact blocks penetration without slowing valid lateral/upward/downward transport. |
| MC-027 | OPEN | Composition rendering | Balanced air renders smoothly; validation builds show excess transport clearly; accepted builds may later hide normal transit. |
| MC-028 | OPEN | Validate corner pressure structures | Preserve corner patterns only when conserved pressure/composition produces them. Reject structures created by gas-wall friction, sticky corners, or solid-adjacency sleep. |
| MC-029 | PARTIAL | Atmosphere inspection/tools | The large Atmosphere action writes the distinct append-only Atmosphere state, Oxygen remains independently selectable, Eraser writes vacuum, and cards distinguish those identities. Pressure and per-component gas percentages still require implementation and runtime acceptance. |
| MC-079 | PARTIAL | Uniform atmosphere edge equilibrium | A completely atmosphere-filled resident map reaches zero gas-edge activity, including outer world boundaries. Finite world edges are sealed boundaries, not permanent exposed-gas breakup edges. Real excess gas remains active until pressure/density equilibrium. |

## Life, ecology, and player interaction

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-030 | REGRESSION | Remove life from fine swaps | Bees, ants, beetles, queens, and player use actor/occupancy state, never exchange material records, stand on hierarchy edges, or count as fine swaps. |
| MC-031 | OPEN | Actor occupancy and medium overlap | Actors overlap air/liquid independently, conservatively displace media, breathe local composition, and can drown or suffocate. |
| MC-032 | PARTIAL | Complete bee lifecycle | Forage, pollen, Beehive return, deposit, honey feeding, queen/Beehive aging, migration, hazards, respiration, replacement, and the 100-bee autonomous cap pass multi-cycle runtime testing. |
| MC-033 | REGRESSION | Recurring readable biohazard | Formation is recognizable, dominant, recurring, and free of grid-edge clumping or premature colony death. |
| MC-034 | OPEN | Real ant behavior | Colonies, pheromone trails, forage/carry/home behavior, hazards, flooding, and permitted digging replace generic particle wandering. |
| MC-035 | OPEN | Real beetle behavior | Beetles crawl surfaces/walls, seek food/shelter, respond to light/hazards, and turn at obstacles without flying/jittering as particles. |
| MC-036 | OPEN | Define or remove insect habitat | Give it explicit species, capacity, inputs, lifecycle, and outputs, or remove it; no generic silent spawning. |
| MC-037 | PARTIAL | Life debug counters | Debug reports actor moves and species counts for bees, queens, Beehives, ants, beetles, habitats, flowers, pollen, and honey. Respiration, suffocation, births, deaths, Beehive returns, and medium displacement require separate counters and runtime acceptance. |
| MC-038 | PARTIAL | Canonical Fix28 Beehive prefab | Material save ID 31 is named `Beehive` in code, shaders, UI, cards, debug, docs, scene keys, and mission text with no alias constant. Sandbox, Ecosystem, and the buildable tool use one shared Fix28 compact prefab: shell `28 <= radius² < 108`, chamber `radius² < 28`, queen at center, and right entrance `x=1..12`, `|y|<=1`. Reset/place/save/load preserve queen and 100-bee colony metadata. Static contracts pass; packaged reset/place/save/load observation remains required. |
| MC-039 | OPEN | Player-medium impulses | Player collisions minimally disturb every gas/liquid through bounded conserved directional impulse, wake touched sections, and then settle through MC-019 only after the injected impulse and resulting productive movement are exhausted. |

## UI and debug

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-040 | PARTIAL | Selectable balanced Atmosphere tool | The always-visible `ATMOSPHERE` control selects the distinct Atmosphere state with ~21% breathable oxygen for ordinary painting. It never triggers a region fill and is not vacuum, pure Oxygen, or an alias. Packaged runtime acceptance and full component packing remain required. |
| MC-041 | PARTIAL | Distinct terrain eraser | Keep an adjacent always-visible `ERASER` that selects vacuum/empty deletion. It remains visibly and mechanically distinct from `ATMOSPHERE` and the selectable `OXYGEN` material. Packaged runtime acceptance remains required. |
| MC-042 | PARTIAL | Clarify movement counters | Use `FINE SWAPS`, `BULK MOVES`, `BULK CELLS`, `FINE REPAIR`, `ACTOR MOVES`, `GAS EXCESS`, `PLAYER IMP`, `GAS TILES`, `LIQUID TILES`, `ENCLOSED TILES`, and `BREAKUP TILES`. Runtime values must correspond to actual work and ownership transitions. |
| MC-043 | PARTIAL | Preserve lower GPU use | Timestamp overlay, grid, text, stats, bulk, fine, Atmosphere excess, actors, collision impulses, and presentation independently. |
| MC-044 | PARTIAL | Remove grid-edge coupling | Debug grid and relational state colors are presentation-only fragment-shader overlays and are not read by simulation shaders. Deterministic runtime comparison with debug enabled/disabled remains required. |
| MC-045 | PARTIAL | High-contrast readable debug UI | Preserve the accepted current text size. Categories use distinct high-contrast colors, tile/chunk and active-region boundaries remain readable, and the color key exactly matches damaged, stable, bulk-moved, fine-active, bulk-ready, settled, sleeping, active, enclosed, and breakup overlays. |
| MC-046 | REGRESSION | Debug never blocks the world | On ordinary layouts the complete debug panel occupies the existing sidebar instead of covering the simulation viewport. The world remains visible and interactive; no full-screen stats rectangle is permitted. Very small windows may hide or page nonessential counters, but may not cover the simulation. |
| MC-085 | REGRESSION | Pair-test and skipped-work debug samples | `PAIR TESTS` and `SKIPPED` preserve the last completed simulation sample. Render-only frames never clear them to zero. Pair tests reflect actual fine/macro pair dispatch scope and skipped work reflects real tile/chunk rejection. |
| MC-080 | PARTIAL | Resource-first activity debug | Keep the accepted UI layout and text size. Sort resource-critical metrics first: FPS, resident cells, active map areas, tested pairs, skipped work, active/sleeping hierarchy. Follow with meaningful activity by cause: fine/bulk motion, gas/liquid transport, structural failures, conveyors, machine input/output, volcano lava/gas, chemistry, actors, and ecology. `FINE ACTIVE` is vivid/hot while `SLEEPING` is dark/cool and their legend order shows active -> settling -> sleeping. Runtime readability remains required. |
| MC-092 | PARTIAL | Soil-facing terminology | Player-facing cards, palette labels, documentation, and UI call material ID 3 `SOIL`; internal IDs remain stable for saves and shader compatibility. Static contracts pass; packaged runtime inspection acceptance remains required. |
| MC-093 | PARTIAL | Distinct Atmosphere, Fill, Oxygen, and Eraser controls | `ATMOSPHERE` selects the balanced-air material, `F FILL` fills with the current selection, `OXYGEN` remains selectable pure gas, and `ERASER` selects vacuum. Their labels, colors, hit regions, and actions are never aliases. Packaged runtime UI acceptance remains required. |
| MC-094 | PARTIAL | Debug state cards and distinct ready/steady colors | The bottom of debug info contains readable cards with a large actual color block plus state name. `BULK READY` is vivid violet and `SETTLED` is vivid green; all relational states remain distinguishable in hue and brightness. Packaged screenshot acceptance remains required. |
| MC-095 | PARTIAL | Default square size-two brush | Fresh startup defaults to square brush shape and size 2 through named shared-state defaults without changing later user selection semantics. Packaged startup acceptance remains required. |
| MC-096 | PARTIAL | Restore Windows launcher | Root `run.bat` searches packaged and common Release build locations, launches `sandhybrid.exe`, forwards arguments, reports failures, and is installed into the Windows package root. Packaged Windows acceptance remains required. |
| MC-098 | PARTIAL | Restrained GUI group separation | Thin dividers separate scene navigation, editing controls, resource metrics, activity metrics, event counters, and debug state cards. Separation clarifies hierarchy without dense boxed clutter or reduced text size. Packaged visual acceptance remains required. |
| MC-099 | PARTIAL | Separate Atmosphere and Fill actions | The always-visible Atmosphere control only selects balanced air. The separate Fill control alone raises the region-fill command and preserves the selected material. Eraser remains a third independent control. Static action, composition, and hit-region contracts pass; packaged runtime acceptance remains required. |
| MC-047 | PARTIAL | Universal cell-or-tile placement | Every selectable material routes through the explicit `CELLS` / `TILES` selector and aligned tile painting path while retaining canonical material IDs and wakeup. Machine metadata, actor exclusions, and runtime parity still require broader acceptance. |

## Scene authoring and repository hygiene

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-115 | PARTIAL | Paint-editable scene image palette | Every material has one unique stable RGB scene color chosen near its ordinary rendered cell color. Save, Load, `material_key.txt`, and `material_key.ppm` use the same shared C++ table; exact colors round-trip losslessly and small Paint/resampling differences choose the nearest material. The old hashed permutation palette is removed. Static round-trip contracts pass; packaged Paint edit/load observation remains required. |
| MC-116 | PARTIAL | Canonical backlog and one brief changelog | `missioncache.md` contains each unfinished requirement once with priority and acceptance criteria, not per-release prose. `CHANGELOG.md` is the only release-history file. Versioned release-note files, one-shot patch payloads, obsolete workflows, and completed agent branches are removed before publication. CI validates the cache and release tree. |

## Chemistry and materials

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-050 | PARTIAL | Correct fertilizer chemistry | Ember becomes only fire or ash, never fertilizer. A mutually paired compost reaction requires visible ash, organic waste/food, silt/dirt/mud, dirty water, oxygen, and time; one feed cell becomes fertilizer while one dirty-water cell becomes clean water. Runtime conservation and rate acceptance remain required. |
| MC-086 | REGRESSION | Tile-mode terrain placement and soil stability | `TILES` mode paints aligned structural 8x8 packets for sand, soil (internal material ID 3), silt, salt, ice, and all block-capable materials. Soil tiles remain stable while supported and release only through damage, lost support, phase change, or the 48%-destroyed collapse threshold. Released block-capable solids move only as fine cells. |
| MC-087 | REGRESSION | Conserved shallow wet sand | Sand/silt absorb water only in a bounded shallow surface band. Once wet, the water remains represented in the wet granular cell and cannot disappear through a timer-based dry flag. |
| MC-088 | REGRESSION | Vertical wet-feed sluice output | A vertical, water-supplied sluice accepts one wet sand or wet silt cell at a time. Each accepted feed has a deterministic seeded 10% gold roll: success outputs exactly one Gold cell and one Water cell; failure outputs exactly one original Sand/Silt cell and one Water cell. Solid and water outputs leave through separate sides, and the player can switch those output sides/directions. Dry feed is rejected; input, roll, output, rejection, and conservation counters remain visible. |
| MC-097 | PARTIAL | Derived wet material variants | Moisture-retaining solids and powders expose derived `WET <MATERIAL>` card identity and darker/cooler wet color treatment without separate palette tools. Wet states arise only through physical mixing, absorption, reaction, or loaded state. Sand remains slightly hydrophobic; wet sand gains explicit density and sinks through ordinary liquids. Static contracts pass; packaged mixing/sinking acceptance remains required. |
| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof | Wet sand/dirt/silt and mud prove full-speed gravity-driven bulk descent and erosion through unsupported material. Wet granular water remains represented until an explicit conserved evaporation/transfer path moves it; timer-based flag deletion is forbidden. A vertical water-supplied Sluice Box processes each wet sand/silt cell exactly once, performs the seeded 10% MC-088 gold roll, separates water from solid output, respects switched output directions, and never stalls because of hierarchy damping. |
| MC-081 | PARTIAL | Local conserved volcano output | A magma vent uses only its own bounded pressure state. Blockage builds pressure; an open outlet releases it. Deterministic pressure events may emit lava, smoke, or steam only immediately at that vent and deduct represented pressure. Ordinary gas activity elsewhere never creates lava. Runtime Volcano proof remains required. |
| MC-082 | PARTIAL | Functional industrial machinery | Powered conveyors visibly transport loose cargo and expose a player-switchable travel/output direction. Smelters, assemblers, and other industrial equipment consume only documented matching input cells and emit the documented goods from the opposite output port; blocked output prevents consumption. Vertical sluice boxes implement MC-088 and expose switchable separated output sides. Habitat controllers process explicit inputs. Machines stay simulation-active, never consume without matching output, and debug reports direction changes, conveyor moves, machine input/output, blocked output, and rejected feed. |
| MC-052 | REGRESSION | Settled granular terrain becomes structural | Dry reconstructable dirt, sand, silt, salt, and ice remain loose while sliding. After at least 52 represented cells in an 8x8 region remain unchanged and physically supported for the stabilization window, those existing cells gain structural and supported state without filling or snapping missing cells. Loss of support, fewer than 32 cohesive cells, mining, heat/phase change, player impact, or renewed motion clears structural state and restores crumbling. Side bracing may support terrain but is not required for floor-supported piles to stabilize. Runtime proof must show the pictured brown slopes settle solid and later release into slides when disturbed. |

## World, camera, scheduling, streaming, and concurrency

The canonical active shape is a **17-region starburst** centered on the camera's current map-footprint region. One active region is the complete pre-expansion map size: **640x360 cells**, never a 64x64 chunk and never an 8x8 tile.

- one center 640x360 region;
- eight adjacent map-sized regions at distance 1: N, NE, E, SE, S, SW, W, NW;
- eight outer map-sized regions at distance 2 along the same directions;
- world bounds clip unavailable spokes;
- no other region animates.

If runtime profiling proves the map-sized starburst too expensive, the only permitted fallback is an explicit camera-visible-region mode. It must be visible in the HUD/debug state and may never silently reinterpret the radius in chunk or tile units.

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-060 | PARTIAL | Camera-visible state availability | The complete bounded resident 4x4 map-footprint window is allocated and renderable at supported zoom levels, including paused state. Far logical regions beyond the resident window remain blocked on MC-063 streaming and runtime visibility acceptance. |
| MC-061 | PARTIAL | Camera-centered 17 map-area starburst | Animate exactly the center 640x360 map-footprint region plus the eight map-sized direction regions at distance 1 and distance 2, clipped only by world bounds. Code and debug must never treat 64x64 chunks or 8x8 tiles as these active regions. Runtime profiling must prove the scope; an explicit camera-visible fallback is allowed only when shown in the HUD. |
| MC-062 | PARTIAL | Hard pause outside active map areas | Outside MC-061: no movement, chemistry, ecology, actors, Atmosphere transport, pressure propagation, or simulation-debug work. Wake deterministically upon entry. Frozen regions remain renderable and cannot corrupt authored maps. |
| MC-063 | OPEN | Far-section disk streaming | Serialize clean distant paused sections, keep only a bounded resident GPU window, free buffers where appropriate, and reload deterministically with versioned corruption-safe saves. This is required before the full logical 8x8 world may be resident without MC-077 risk. |
| MC-064 | PARTIAL | Automatic active-region worker scheduler | Reserve the main thread completely for windowing, input, rendering coordination, and everything non-region-related; simulation jobs never run there. Determine hardware concurrency automatically. Use up to 17 simulation workers, one per active 640x360 map-area region when at least 18 hardware threads exist including the reserved main thread. With fewer than 18, pair additional regions onto workers from the furthest outer spokes first. Never oversubscribe, never schedule paused regions, and preserve deterministic boundary results. Current scheduler assignment exists; actual independent region execution remains runtime-unverified. |
| MC-066 | OPEN | Safe pause/wake/boundary transfer | Resolve pending transfers, actors, impulses, pressure, and dependencies before pause. Crossing the starburst edge conserves all state without allowing the paused side to animate. |
| MC-067 | PARTIAL | Expand logical world 8x8 dimensions | The logical address space is 8 times the old width and height. Until MC-063 streams distant regions, only a bounded 4x4-map-footprint resident window may occupy GPU memory. Update generation, buffers, indexing, saves, limits, overflow checks, paging, and profiling without restoring a fully resident 64x cell allocation. |
| MC-068 | PARTIAL | Camera home/reset | Every scene starts centered on its authored upper-center 640x360 footprint at world Y=0. Reset returns to exactly that footprint and one-map scale without touching simulation state. The three resident map footprints below remain available for geology and exploration. Runtime verification remains required. |
| MC-069 | PARTIAL | 2x2 maximum zoom-out | Maximum zoom-out displays exactly 1280x720 cells: four 640x360 footprints in a 2x2 view. Default/reset displays exactly 640x360. Minimum/default/maximum zoom values scale with the resident window, preserving pointer mapping, panning, active-scope clarity, and culling. |
| MC-074 | PARTIAL | Upper-center authored objects with common world stack | Every authored object retains its exact local coordinate inside the original 640x360 footprint, horizontally centered at resident-world Y=0. Scenes never repeat or stretch. A common one-brick stone foundation spans the footprint bottom, and three filled subterranean footprints continue below. Actors, hives, machines, metadata, generated scenes, saved PPM loads, reset, and camera home use the same offset. Runtime scene cycling/load acceptance remains required. |
| MC-075 | PARTIAL | Camera navigation and scope HUD | Middle-mouse drag, mouse-edge scrolling, and camera reset work at every zoom. Keyboard camera panning is permitted only when the current scene has no player; MC-076 owns that routing contract. The sidebar always shows current zoom, active-region mode, and active-region count, while debug draws map-area boundaries clearly. Drag, edge, reset, HUD, and boundary rendering remain runtime-unverified. |
| MC-089 | PARTIAL | Upgrade every scene to the shared resident geology stack | Sandbox, Blank, Volcano, Waterworks, Ecosystem, Engineering, Gold Mine, Demolition, and Frontier Base use the same resident-width stone foundation, side shell, three sand/soil/silt/mud/stone subterranean zones, stone lava cap, two-brick lava band, and stone bottom shell without moving authored objects. Generated scenes and loaded saved counterparts are identical outside the authored footprint. Runtime acceptance remains required. |
| MC-114 | PARTIAL | Common four-footprint vertical world layout | The authored scene and default camera occupy the upper-center 640x360 footprint. Exactly three 640x360 resident footprints below are filled with deterministic layered sand, soil, silt, mud, and stone. A one-brick stone separator sits under the scene; the world bottom contains a continuous two-brick lava band enclosed by one-brick stone cap, bottom, and side shells. Reset and Load produce the same substrate. Static contracts pass; packaged visual and save/load acceptance remain required. |
| MC-090 | PARTIAL | Scientific experiment layouts | Engineering uses controlled thermal, single-aperture gas-diffusion, sediment-separation, and paired compost control/treatment experiments. Gold Mine includes a visible water-fed conveyor/sluice station. Inputs, controls, treatment, and outputs are spatially distinguishable without relocating existing objects. Runtime observation remains required. |
| MC-091 | PARTIAL | Explain fertilizer mechanics in-world | Ember and ash/fertilizer cards state that ember becomes ash only and fertilizer requires compost ingredients. Engineering contains a clean-water control and dirty-water compost treatment so the mechanic explains itself through visible inputs and different outcomes. No hidden ember-to-fertilizer shortcut remains. |
| MC-077 | PARTIAL | Windows GPU memory-manager crash in tile mode | Selecting `TILES` never allocates, resizes, or rebinds GPU memory. The logical world remains 8x8 map footprints, but the resident GPU window is bounded to 4x4 map footprints until MC-063 implements deterministic streaming. Windows/Linux CI must pass; Windows runtime must prove repeated CELLS/TILES switching, painting, reset, scene cycling, save/load, and debug use without device loss, TDR, WDDM reset, allocation growth, or stale descriptors. |

## Library architecture

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-071 | PARTIAL | Thin `SandHybrid_Demo` | `SandHybrid_Demo` owns native startup/events and links the optional in-tree `SandHybrid::VulkanRuntime`; Vulkan renderer code no longer enters the core archive. Exported runtime/native-host embedding acceptance remains required. |
| MC-072 | PARTIAL | Optional subsystems | `SANDHYBRID_BUILD_APP=OFF` with `SANDHYBRID_BUILD_VULKAN_RUNTIME=OFF` builds, tests, installs, and externally consumes a clean core package without configuring Vulkan, EpochGui, shaders, windowing, or native event sources. Independent switches for streaming, debug, UI, actors, ecology, and factories remain open. |
| MC-073 | DEFERRED | EpochEngine integration | Later migrate/rewrite using canonical `epochengine::` APIs while preserving reusable `SandHybrid` boundaries. |

## Backlog review policy

Every P0 mission is attempted in the current release. Every P1/P2 row is reviewed for contradiction and retained until its own acceptance passes. Static compilation never closes visual, runtime, conservation, or performance work.

# Permanent invariants

- Every generated scene and every loaded saved counterpart uses the same upper-center authored footprint, three-zone subterranean geology stack, and stone-wrapped two-brick bottom lava band.
- Scene PPM colors are unique, stable, paint-friendly representatives of visible cell colors and share one Save/Load/material-key table.
- Material save ID 31 is Beehive and ID 48 is Iron Ore; neither has a deprecated source alias.
- Material cells, actors, Atmosphere composition, and medium impulses are authoritative; hierarchy metadata only accelerates them.
- Behavior is canonical and provenance-independent.
- Gas/liquid volume, every gas component, pressure, and cross-boundary transfer are conserved.
- Normal air is one Atmosphere mixture.
- Excess-gas transit remains visible until accepted.
- Every gas and liquid can rest and wake deterministically.
- Damping never suppresses a valid gravity, buoyancy, pressure, erosion, reaction, tool, or actor-driven move. It only removes non-productive residual oscillation after local equilibrium.
- Gas-solid contact is collision only: solids prevent penetration but never apply tangential friction, sticky-wall drag, or adjacency-based sleeping to gas.
- Mud and wet bulk elements continue gravity-driven erosion at the same material-defined rate as equivalent fine cells.
- Player/actor disturbance transfers bounded momentum/pressure without erasing or minting medium.
- Only the 17-position starburst of whole 640x360 map-area regions animates; unavailable spokes are clipped by world bounds and every other region is paused.
- The main thread never executes section simulation work.
- Worker assignment is deterministic and pairs outer map-area regions first when fewer than 17 workers are available.
- World expansion, zoom, pausing, streaming, and concurrency do not change deterministic reference results.
- Directional keyboard input has exactly one owner: the player when present, otherwise the camera. Mouse-edge and middle-mouse camera input remain independent.
- Debug statistics never cover the normal simulation viewport; debug state coloring and active-area boundaries may render in the world.
- 8x8 terrain regions qualify for stability only and never reconstruct missing cells.
- Each terrain pixel takes two laser hits; after more than half are dislodged, the represented remainder collapses rather than vanishes.
- Authored structural assemblies do not infer failure solely from missing direct-below support; they release only through explicit damage, phase change, mining, or the represented collapse threshold.
- Uniform atmosphere may sleep at sealed resident-world edges; world boundaries never manufacture perpetual gas activity.
- Industrial machines are active simulation participants, and every consumed represented input has a matching inventory/output transition.
- Project-owned branding is `SandHybrid`; Legacy branding remains only when it is part of a proper external dependency or integration name.
- Failed, missed, deferred, and regressed missions remain active until accepted.



# Accepted foundations

| ID | Accepted foundation | Evidence location |
|---|---|---|
| MC-070 | Reusable platform-neutral `SandHybrid::SandHybrid` core library, optional Vulkan runtime, thin demo host, installed CMake package, and downstream consumer contract. | `CHANGELOG.md`; Windows/Linux CI history. |
| MC-065 | Deterministic simulation code has no coroutine execution path. CI rejects coroutine machinery across production C++ until bounded streaming or I/O ownership and cancellation are defined under MC-063. | `tools/validate_no_simulation_coroutines.py`; `.github/workflows/core-hygiene-contracts.yml`; CI history. |
| MC-083 | `SandHybrid` is the only project-owned product identity across production code, build files, workflows, docs, and tests. Proper external names and the current repository-host URL remain explicitly allowed. | `tools/validate_project_branding.py`; `HALF_WATER.md`; `.github/workflows/core-hygiene-contracts.yml`; CI history. |
| MC-117 | Protected handoff `agent/nonoverlap-core-hygiene` at `bf2d052a9dbd275ce968cab9453b70b26b6a2771` was reconciled onto current `main` without dropping later mission-cache work. Adam explicitly required the source branch to remain preserved. | PR #45 validation history; PR #46 integration history; Windows/Linux v2.5.3 Release CI. |

Detailed release history belongs only in `CHANGELOG.md`. Completed source branches and one-shot patch machinery are deleted after publication.
