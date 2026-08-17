# SandHybrid

Read `missioncache.md` before repository work. It is the single canonical mission document.

SandHybrid is currently a standalone C++23 Vulkan material, terrain, ecology, factory, and combat simulation testbed. Fine particles, liquids, gases, machines, actors, and aligned Terraria-style terrain blocks share one deterministic cell simulation. There is no second rigid-body or voxel physics world.

- Native Win32/Vulkan on Windows
- Native XCB/Vulkan on Linux
- Explicit render worker using `std::thread`, atomic cancellation, and deterministic join
- Renderer-neutral EpochGui layout, hit testing, and embedded bitmap font
- CMake 3.28+ with a pinned vcpkg manifest
- No SDL, GLFW, ImGui, Boost, or proprietary runtime framework

## Production rewrite

The reusable library now contains the first production rewrite primitive: a sparse 64x64 section grid with dirty rectangles, safe non-touching phases, halo wakeups, sleeping, and 512x512 streaming-page coordinates. See `REWRITE_PLAN.md`. The existing Vulkan runtime remains available during staged parity migration.

## Canonical material state

Material behavior is determined by the material ID, current temperature, phase, packed physical state, and local environment. Placement provenance is not part of the physics contract.

The following creation paths produce the same canonical behavior:

- pre-placed World district content
- stabilized Terraria-style terrain cells
- mouse-painted cells
- broken structural fragments
- spawned particles
- reaction products
- loaded cells

A material does not disappear because it came from a particular creation path, because provenance is missing, or because a damaged region is incomplete. Structural breakup releases represented cells into the same material simulation.

## Thermal and phase model

Every material has generated canonical physical data shared by C++ contracts, GLSL physics, UI labels, and inspection cards:

- base phase and density
- strength and erosion resistance
- thermal conductivity
- service, softening, melting, boiling, vaporization, and ignition temperatures
- acid resistance
- strengths, weaknesses, conversions, ecological role, and dangers

Copper, gold, iron, aluminum, uranium, and steel retain separate thresholds. Copper and gold melt before steel. Ordinary 1300 C lava softens steel but does not melt or vaporize it; hotter material can cross the configured steel melting point. Remaining metal mass is preserved during damage and phase changes. Structural metal and ore cells placed through `TILES` never auto-crumble because neighboring cells are missing.

Plastic is no longer inert. Standard and acid-resistant plastics can soften, melt, ignite, decompose, and interact with lava. Configured products include oil-like melt, smoke, waste/char, soot/ash, and gas-cycle products. Acid-resistant plastic has its own higher thermal and corrosion limits.

## Volcano and thermal vents

Magma vents participate in the same heat, pressure, gas, and magma rules as the volcano:

- blocked vents accumulate pressure faster
- open vents release pressure
- intermediate pressure releases gas
- high pressure can push magma or produce an eruption
- eruption thresholds are pressure-driven rather than timer-driven
- an open vent does not automatically trigger a major eruption

The World's Volcano district provides a working pressure reservoir, vent path, magma source, gas space, cone, and cooling water.

## Water and fluids

Water movement uses bounded local pair passes and a bounded eight-cell pressure probe. It does not teleport or scan the complete map.

- fast falling and diagonal movement
- repeated horizontal equalization
- local basin pressure balancing
- waterfall and channel response after terrain changes
- volume-preserving pair swaps
- waterfall aeration into adjacent empty cells
- conservative salt concentration transfer between liquid cells

Fresh water, saltwater, dirty water, oil, acid, lava, and honey retain different density, viscosity, and flow behavior. Stable liquid cells stop changing once local pressure and level differences are resolved.

## Terraria-style terrain stability

The world uses 8x8 aligned terrain regions made from ordinary cells. Regions never reconstruct, synthesize missing pixels, fill gaps, snap material into place, or become a second tile object. Existing cells may qualify for stability only when all of these conditions hold:

- at least 52 of 64 cells are occupied
- occupied cells are one compatible material
- temperature and phase are stable
- cells have remained still long enough
- no active burning, melting, erosion, reaction, falling, or displacement exists
- the 120-tick stability qualification completes
- the 240-tick restabilization cooldown has expired

Qualification changes only the coherence/support state of cells that already exist, allowing the region to stop falling. Empty positions remain empty. Material, temperature, represented mass, and existing damage are preserved.

Each stabilized granular-terrain pixel has 255 integrity and an ordinary laser hit applies 144 damage, so every pixel requires exactly two hits to dislodge. Granular terrain may release below its cohesion threshold. Block-capable material placed through `TILES` is different: each surviving stone, metal, ore, glass, wood, plastic, or machine cell remains structural regardless of regional occupancy or support heuristics, and changes only through direct damage or a real material phase/reaction.

## Terrain rendering and debug view

Normal rendering hides the raw 8x8 simulation grid and shades adjacent material cells as cohesive Terraria-like terrain. Static procedural detail is used for stone, crystal, glass, wood, ice, metals, and structural machinery.

Press `F3` to reveal the structural debug layer:

- tile boundaries
- occupancy
- structural and supported states
- sleeping and active regions
- damage and collapse state
- stability candidates

The overlay reads simulation state only and does not alter it.

Ground continues across the resident world outside the composed district envelope. Biome seams are broad deterministic transitions, mineral deposits use coherent complete-tile cores, and broken resource cells appear only in localized rubble pockets or curved sand traps.

Liquids use pressure-aware falling and equalization without a fixed surface-age cutoff. Water-like liquids spread farthest, oil and acid use bounded intermediate reach, honey remains slower without becoming stuck, and lava retains its cohesive semi-solid path.

## Exact material inspection

Hold `Alt` while the cursor is inside the simulation to display the card for the exact cell under the mouse. Lookup is direct by grid coordinate; it does not scan or select the world.

The live card includes available information such as:

- material name, group, and current phase
- temperature and density
- tile occupancy and represented mass
- structural integrity and tile state
- softening, melting, boiling, vaporization, and ignition thresholds
- strengths, weaknesses, conversions, ecological role, and danger
- nearby live hazard warning

The card follows the cursor without covering the inspected point when space permits and disappears immediately when `Alt` is released. Painting and mining are suppressed while inspecting.

## Ecology and conservation

SandHybrid targets a near-zero-loss material cycle. Reactions convert represented cells instead of using deletion as failure recovery.

Examples:

- dirty water settles into clean water and silt
- silt becomes fertilizer near moist crops or dries into soil
- crops consume fresh water and fertilizer to create food and return soil
- food becomes waste
- waste returns to soil, dirty water, smoke, or carbon dioxide
- compost combines ash, organic waste or food, silt/soil/mud, dirty water, oxygen, and time to produce fertilizer while cleaning the paired water cell
- smoke plus steam becomes dirty steam
- dirty steam condenses into dirty water
- oxygen and carbon dioxide cycle through respiration, fire, water aeration, and plants
- salt dissolves into existing water concentration without creating liquid volume
- structural breakup preserves fragments
- phase changes preserve the represented cell

Debug accounting tracks cells created at explicit world boundaries, converted, intentionally boundary-lost, stabilized, broken, and conservation errors. Debug mode periodically reports non-zero conservation errors.

## Material readability and gases

The world, palette, cards, and warnings use one generated material catalog. Frequently interacting materials use separated luminance and hue. Carbon dioxide uses near-black translucent charcoal; hydrogen uses bright pink. Oxygen remains light cyan, smoke dark gray, and steam pale blue-white so gases stay readable at a glance.

Gas drawing is isolated behind `gasPresentation(...)`. The current pass uses restrained color and opacity. The boundary is ready for later material-specific density fields, flow animation, diffusion visualization, heat distortion, and layered translucency without changing chemistry or material IDs.

## Interface

The interface was rebuilt as a pixel-aligned EpochGui layout. The native title bar remains simple; game controls stay inside the Vulkan viewport.

- compact `SandHybrid` header and FPS display
- non-overlapping global controls and distinct workspace bodies
- eight consistent material groups and slots
- larger title, FPS, controls, palette labels, and character HUD
- readable hover, selection, pause, brush, and character status
- separate always-visible `ATMOSPHERE`, `F FILL`, and `ERASER` controls; `OXYGEN` remains an independent material
- bottom-anchored debug state cards with real swatches and thin section dividers
- inspection card separated from ordinary controls
- `F3` debug controls visually separated from normal gameplay UI
- the runtime exposes one `WORLD`; no scene-carousel buttons exist
- Inventory remains in the sidebar and switches between `INVENTORY` resource slots and the separate `BLUEPRINTS` pane
- Designer remains in the sidebar with its isolated 64x32 authoring grid and `INVENTORY` / `BLUEPRINTS` panes; the main viewport always remains the live world

## Resource pixels and sifting

Shared geology contains sparse deterministic loose pixels of Iron Ore, Copper, Aluminum, and deeper Uranium mixed through sand, soil, silt, mud, and stone. Gold remains authored/special and is not randomly distributed. The generated reset path and loaded-scene substrate use the same distribution. Gravity, filters, conveyors, sluices, and magnets separate those pixels without changing identity. The same materials become durable structural cells only when deliberately placed through `TILES`; walking nearby never collects structural construction.

## Persistent World districts

One reset creates eight connected 640x360 districts ordered west-to-east and distributed across the available resident World width:

1. Sandbox
2. Ecosystem and suspended hive
3. Engineering
4. Frontier Base and player recovery spawn
5. Sunken Volcano and lake
6. Waterworks
7. Gold Mine and water-fed Sluice
8. Demolition

They share canonical cells, actor state, atmosphere, weather, temperature, hierarchy scheduling, conservation, and one save. `Blank` and former scene IDs remain migration-only and are not selectable runtime worlds.

## Controls

### General

- Default brush: square, size 2
- `ATMOSPHERE`: select balanced breathable air for painting
- `F FILL`: fill the active region with the currently selected material
- `ERASER`: select vacuum/empty deletion
- `OXYGEN`: selectable pure-gas material in the Engineering group
- There are no previous/next scene controls; all districts coexist in `WORLD`
- `R`: reset the complete persistent World while preserving both simulation-camera and MAP center/zoom state
- `P` or the `RUNNING`/`PAUSED` button: pause or resume simulation; direct paint, erase, fill, Ignite Air, selection, and blueprint placement remain live while paused without advancing time
- `N`: one simulation step while paused
- `M`: switch the persistent player between Mine and Build ownership while Inventory is active
- `F3`: structural/debug overlay
- Hold `Alt`: inspect exact cell
- Mouse wheel over world: brush radius
- Middle-mouse drag: direct responsive camera pan across the World
- Right-mouse drag: camera pan while `WASD PAN` mode is enabled
- `H` or the sidebar `H CAM HOME` button: restore the active view; MAP returns to its full-world frame, otherwise the simulation camera returns to the supported Frontier player spawn. `0` and Home remain compatibility aliases.
- Cell/Tile placement selector: place any selected material as fine cells or one aligned 8x8 tile
- Escape: exit

### Camera navigation

- `PLAYER WASD` / `WASD PAN`: choose whether WASD controls the persistent player or camera. World `RESET` never changes camera navigation state; use `H CAM HOME` explicitly.
- `W` / `A` / `S` / `D`: control the player unless `WASD PAN` is enabled
- Middle-mouse drag always pans; right-mouse drag pans in `WASD PAN` mode
- Mouse-edge camera movement is removed

### Player tools

- `W` / `A` / `S` / `D`: control the player when `PLAYER WASD` is selected
- Mine mode, left mouse: drill terrain; automatically fire plasma when the first hit is a hostile and ammo is available
- Mine mode, right mouse: deposit carried resource
- Build mode, left mouse: place selected material
- Build mode, right mouse: erase


Balanced authored air and the Atmosphere tool use an append-only `Atmosphere` material/state with an approximately 21% breathable oxygen fraction. The separate Fill action fills with whichever material is currently selected. Pure Oxygen remains independently selectable.

## Windows launcher

The Windows package includes `run.bat` at its root. It locates `sandhybrid.exe` in the packaged `bin` directory or common local Release build directories, forwards command-line arguments, and keeps failures visible instead of closing silently.

## GPU scheduling

- simulation clock: fixed 60 Hz
- presentation ceiling: 120 FPS
- FIFO swapchain pacing
- deterministic disjoint pair movement phases
- tile-level sleeping and active flags
- chemistry and movement early-out for stable sleeping regions
- sunlight updated every fourth simulation tick
- machine work restricted to valid controller/output locations
- paused state performs no continuous simulation ticks, actor updates, MAP refresh, lighting/day-night, reactions, or effects; direct editor mutations still commit and dirty affected hierarchy state

Compact, Standard, and Large resident worlds are 5120x1440, 7680x1440, and 10240x1440 cells. Compact places the eight districts contiguously; Standard and Large distribute aligned gaps between them, with Large starts at X `0, 1368, 2736, 4104, 5472, 6840, 8208, 9576`. Each district translates its intended authored terrain row onto shared grass Y `1040`. The active simulation gate remains one contiguous clipped 4x4 window around the camera; the Large startup window intersects only three authored districts, and stable off-window regions reject expensive chemistry, neighborhood, and movement work without becoming another simulation instance. Sparse far-region streaming remains active in `missioncache.md`.

## Windows build

The script uses Visual Studio 2022 and this standalone vcpkg checkout:

```text
C:\Users\iammi\source\repos\vcpkg
```

```bat
build_windows.bat Release
run_windows.bat Release
```

Output:

```text
build\windows\Release\sandhybrid.exe
```

Run the complete build, shader, warning, and contract validation separately:

```bat
validate_windows.bat Release
```

The ordinary build script builds only the application and does not run tests.

## Linux build

```bash
sudo apt install build-essential cmake ninja-build libxcb1-dev pkg-config
VCPKG_ROOT="$HOME/vcpkg" ./build_linux.sh Release
./run_linux.sh Release
```

## Validation

See [`VALIDATION.md`](VALIDATION.md) for the required 24-item validation matrix and the distinction between compile-time contracts, static shader/interface checks, and Windows Vulkan runtime checks.

Manual contract commands:

```bash
cmake -S . -B build/tests -DSANDHYBRID_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
python tools/generate_ui_text.py
python tools/validate_shader_contracts.py
```

## Architecture

```text
native event thread
    Win32 or XCB input/window
    EpochGui layout and exact hit testing
    atomic SharedState

Vulkan thread
    fixed-rate simulation and acquire/present
    reset/paint
    sunlight
    tile stability/sleep controller
    canonical chemistry/thermal pass
    deterministic movement and salinity passes
    actor/factory pass
    cohesive terrain, gas boundary, UI, debug, and inspection render
```

Each cell is 16 bytes: material ID, age, signed temperature, and packed material-specific state. Each aligned region has a compact controller containing material, occupancy, stability flags, settled time, and restabilization cooldown. Neither structure contains creation provenance used by physics.

## EpochGui snapshot

The complete vendored EpochGui dependency, exact upstream commit, and cross-platform module compatibility policy are documented in `third_party/EpochGui/SNAPSHOT.md`. SandHybrid releases must update this full dependency to the current upstream version rather than retaining a selected-header snapshot.


## Hierarchical simulation

Uniform aligned 8x8 material regions can move as exact 64-cell packets while their canonical cells remain authoritative. Complete Water and gas packets attempt movement every two fixed 60 Hz ticks. Exposed or misaligned packets receive eight due opportunities, then fall back to fine simulation only when a fresh perimeter check still finds open or incompatible neighbors; enclosed Air never untile merely because time passed. Fine cells continually requalify for full-tile ownership, and macro/fine work in one tick must use disjoint cells. Half Water remains fine simulated. Above that, 64x64 chunks cache activity and sleep state for fast rejection. See `HIERARCHICAL_SIMULATION.md`.


## Half-volume fresh water

Fresh water supports conserved faint half-cells, a three-half-unit ledge release threshold, and solid-supported pre-fall droplets that cannot hop along water edges or crawl after falling. See `HALF_WATER.md`.


## v2.5.25 distributed World and persistent Fix29 hive recovery

The local v2.5.25 target keeps one connected `WORLD`, distributes its eight former authored areas across the available width, and aligns every intended grass surface to world Y `1040`. The player, saves, resident buffers, atmosphere, heat, weather, and hierarchy state persist while travelling between districts. Normal saves live under `saves/worlds/<size>/world/<slot>/`.

Complete liquid/gas packets remain reversible scheduling metadata over canonical cells. They move every two fixed ticks and carry an eight-opportunity travel budget through successful swaps and blocked attempts; only a fresh open or incompatible perimeter permits fine fallback, while enclosed Air remains tiled. Sandbox and Ecosystem share the cell-exact photographed Fix29 hive body and contents, nine complete Wood support tiles, and 100 live district-aware SandHybrid bees in a compact three-lobed biohazard. Hive Honey/Pollen remain structural body content during ordinary motion and chemistry; placed exactness is checked again after 120 focused ticks. Deterministic and native evidence is recorded in `MISSION_LEDGER.md`; broader traversal, long ecology/weather cycles, and visual/performance observation stay active in `missioncache.md`.
## v2.5.22 systems, scene, machinery, and presentation recovery

SandHybrid v2.5.22 was the preceding local recovery target, with native Windows/Linux packages, SHA-256 files, and no prerelease marker.

This recovery uses the actual scene-local Fix29 hive hash in Sandbox, Ecosystem, tool placement, and load normalization. Liquid work is single-spend per fixed tick; Half Water keeps its conserved fall/merge/clear-gap/supplied-drip rules, while settled full-Water surfaces choose deterministic nearest outlets and sleep without residual fine scheduling.

The supplied 2026-08-04 Volcano silhouette now anchors the scene: a broad left lake, smoother layered underground, far-right Stone cone, central Lava throat and chamber, and bottom Lava return. Engineering, Industry, and Gold Mine recover active experiment layouts and a visible water-fed ten-percent Sluice path.

Half Water is the darker conserved one-unit state again, including the supplied ledge hang/drip; full Water continues the corrected ledge/diagonal route. Normal materials render statically except for the retained metal/ore glint. State-driven gases conserve pressure while Atmosphere expands into Vacuum and raw atmospheric gases mix locally.

`IGNITE AIR` is in `ACTIONS` above `KEYMAP`. Settings provides 30/60/120/UNLIMITED presentation caps while simulation remains fixed at 60 Hz. Debug now uses bounded rotating-region samples, and the persistent World has a recoverable supported breathable player spawn.

The generated, placeable, and loaded hive uses the photographed historical compact model (`24..87` shell, `<24` chamber, exit through `x=10`, seed `0xD17A5EED`) over nine complete Wood support tiles. EpochGui remains the complete last-verifiable v0.88.75 / `d8decc9` snapshot; the release-time fetch is explicitly recorded as blocked by the suspended GitHub endpoint, and no dirty local dependency tree is imported.

The shipped executable can run the production Vulkan state/readback gate:

```text
sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json
```

Broader visual, long-running performance/leveling, full bee-cycle, and complete cross-system acceptance remains active in `missioncache.md`.
## v2.5.16 sidebar Blueprints and paused editing

Inventory and Designer remain normal sidebar workspaces with exactly `INVENTORY` and `BLUEPRINTS` subtabs. Four shared slots now report real occupied/empty state. Designer publishes authored Static Models or Map Chunks directly to those slots without replacing the world viewport. An occupied slot produces a camera-correct world footprint and a confirmed world left-click performs one all-or-nothing placement.

Blueprint placement remains live while `PAUSED`, including with the persistent player enabled. It cannot share a click with paint, Fill, mining, or deposit and cannot leak through load/reset. The exact bounded selection/copy, thumbnails, and persistence portions remain active in `missioncache.md`.
## v2.5.15 recovery baselines

The Sandbox, Ecosystem, build-tool, and loaded-map hive use the hard-coded Ecosystem model that SimpleSandSim Fix36 explicitly restored from immediately before PR #19: queen centers (512,234)/(512,232), loose Wood perch offsets x=-37..29 and y=-16..-13, squared shell radii 25 through 91, a chamber below 25, deterministic Honey/Pollen/Empty contents, and a right exit through x=10. Only the model and contents are shared; SandHybrid retains its own bee actors, forage, return, deposit, feed, migration, hazard, and 100-bee colony rules.

Moving macro media match the packaged v2.5.3 reference. Runtime acceptance for both recovered baselines remains tracked in `missioncache.md`.


## Resident World envelope

All eight authored districts retain local 640x360 coordinates while their X origins are distributed across the resident width and their intended terrain surfaces meet at shared world Y `1040`. Their boundaries are ordinary shared cell boundaries, not scene walls or load transitions. Intentional authored air stays empty/atmospheric, complete tile-authored floors and foundations remain aligned, and only the outer resident world supplies hard containment.

Engineering presents controlled thermal, gas diffusion, sediment separation, and paired compost experiments. Gold Mine includes a water-fed conveyor and Sluice line. Ember becomes ash only; fertilizer requires ash, organic waste, mineral soil/silt, dirty water, Oxygen, and time. Their broader runtime operation remains active until packaged observation covers it.

## Deterministic core state contracts

SandHybrid API v3 exposes platform-neutral foundations for the replacement runtime:

- packed Atmosphere composition with exact pressure/component conservation;
- atomic all-or-fallback represented-material packet transfers;
- actor occupancy and medium impulses without encoding actors as material cells;
- atomic directional machine and sluice transactions;
- explicit Ant/Beetle habitat capacity, inputs, outputs, and cadence.

These APIs are deterministic and covered by Windows/Linux contracts. The Vulkan runtime is being migrated onto them incrementally; `missioncache.md` retains every production and packaged-observation requirement until it passes.

## v2.5.6 runtime controls

Right-click exclusively pans: dragging moves the current camera and holding it near a viewport edge performs gated edge panning. `WASD PAN` routes keys to the simulation camera; MAP uses its own camera and a slow full-world snapshot without changing simulation LOD or active-region scheduling. In the persistent World, MINE uses left click and BUILD places the selected resource from the sidebar Inventory pane with left click. Hold `F` and left-click the simulation to fill; pressing `F` alone does nothing. World `RESET` preserves both cameras. `H` and the sidebar `H CAM HOME` button explicitly restore the active camera; `0` and Home remain aliases.


## World sizes and exact saves

Launch Compact, Standard, or Large with the supplied scripts or `--world-size`. Save slots use `--save-slot NAME`. Gameplay saves are exact whole-world `.shw` files under the portable `saves/worlds/<size>/world/<slot>/` tree; see `SAVE_LAYOUT.md`. PPM files are retained only for authored 640x360 district import/export.
