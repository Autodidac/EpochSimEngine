# SandHybrid

SandHybrid is a standalone C++23 Vulkan material, terrain, ecology, factory, and combat sandbox. Fine particles, liquids, gases, machines, actors, and aligned Terraria-style terrain blocks share one deterministic cell simulation. There is no second rigid-body or voxel physics world.

- Native Win32/Vulkan on Windows
- Native XCB/Vulkan on Linux
- Explicit render worker using `std::thread`, atomic cancellation, and deterministic join
- Renderer-neutral EpochGui layout, hit testing, and embedded bitmap font
- CMake 3.28+ with a pinned vcpkg manifest
- No SDL, GLFW, ImGui, Boost, or proprietary runtime framework

## Canonical material state

Material behavior is determined by the material ID, current temperature, phase, packed physical state, and local environment. Placement provenance is not part of the physics contract.

The following creation paths produce the same canonical behavior:

- pre-placed scene content
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

Copper, gold, iron, generic metal, and steel retain separate thresholds. Copper and gold melt before steel. Ordinary 1300 C lava softens steel but does not melt or vaporize it; hotter material can cross the configured steel melting point. Remaining metal mass is preserved during damage, collapse, fragmentation, and phase changes.

Plastic is no longer inert. Standard and acid-resistant plastics can soften, melt, ignite, decompose, and interact with lava. Configured products include oil-like melt, smoke, waste/char, soot/ash, and gas-cycle products. Acid-resistant plastic has its own higher thermal and corrosion limits.

## Volcano and thermal vents

Magma vents participate in the same heat, pressure, gas, and magma rules as the volcano:

- blocked vents accumulate pressure faster
- open vents release pressure
- intermediate pressure releases gas
- high pressure can push magma or produce an eruption
- eruption thresholds are pressure-driven rather than timer-driven
- an open vent does not automatically trigger a major eruption

The Volcano scene provides a working pressure reservoir, vent path, magma source, gas space, cone, and cooling water.

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

Each stable terrain pixel has 255 integrity and an ordinary laser hit applies 144 damage, so every pixel requires exactly two hits to dislodge. The region remains coherent with 32 pixels left. Once fewer than 32 remain—more than half dislodged—all remaining pixels release and drop in the same simulation pass.

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
- silt becomes fertilizer near moist crops or dries into dirt
- crops consume fresh water and fertilizer to create food and return soil
- food becomes waste
- waste returns to dirt, dirty water, smoke, or carbon dioxide
- ash plus water returns minerals to fertilizer
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

- compact `Epoch Sand` header and FPS display
- responsive scene and mode controls
- eight consistent material groups and slots
- larger title, FPS, controls, palette labels, and character HUD
- readable hover, selection, pause, brush, and character status
- inspection card separated from ordinary controls
- `F3` debug controls visually separated from normal gameplay UI
- compact layouts hide nonessential scene buttons rather than overlapping them

## Resource pixels and sifting

SandHybrid has no authored ore blocks. Gold, iron, copper, and generic metal exist as independent material pixels mixed sparsely through sand or silt. Gravity, filters, conveyors, and magnets separate those pixels without changing them into nuclear fuel or radiation particles. Structural steel, iron, or copper is never collected merely because the player walks nearby.

## Default scenes

Cycle with `[` and `]` or the on-screen controls.

1. Sandbox
2. Blank
3. Volcano
4. Waterworks
5. Ecosystem
6. Engineering Lab — contained thermal vessel, sediment/metal sifter, and sealed gas cell
7. Platformer — multi-level playable structure based on the supplied reference
8. Demolition
9. Frontier Base

All scenes use the same canonical material and structural rules.

## Controls

### General

- `[` / `]`: previous or next scene
- `R`: reset scene
- Space: pause or resume
- `N`: one simulation step while paused
- `M`: Mine/Build mode in sandbox scenes; character scenes always keep player tools active
- `F3`: structural/debug overlay
- Hold `Alt`: inspect exact cell
- Mouse wheel over world: brush radius
- Escape: exit

### Character scenes

- `A` / `D`: walk
- `W`: jump
- Mine mode, left mouse: drill terrain; automatically fire plasma when the first hit is a hostile and ammo is available
- Mine mode, right mouse: deposit carried resource
- Build mode, left mouse: place selected material
- Build mode, right mouse: erase

## GPU scheduling

- simulation clock: fixed 60 Hz
- presentation ceiling: 120 FPS
- FIFO swapchain pacing
- deterministic disjoint pair movement phases
- tile-level sleeping and active flags
- chemistry and movement early-out for stable sleeping regions
- sunlight updated every fourth simulation tick
- machine work restricted to valid controller/output locations
- paused state performs no continuous simulation ticks

The current storage remains a dense 640x360 GPU grid for deterministic direct lookup and simple save compatibility. Stable regions still receive the minimal dispatch envelope but exit before expensive chemistry, neighborhood, and movement work. A later sparse-chunk allocator can replace storage without changing canonical material rules.

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
build\windows\Release\epoch_sand.exe
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
cmake -S . -B build/tests -DSAND_HYBRID_BUILD_APP=OFF -DBUILD_TESTING=ON
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

The vendored backend-neutral subset is documented in `third_party/EpochGui/SNAPSHOT.md`.
