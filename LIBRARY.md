# EpochSimEngine library

`EpochSimEngine` is the project and reusable C++23 simulation library. `SandHybrid` names only its bundled demo. Old SandHybrid API/package identifiers remain legacy compatibility interfaces, not library branding. The current compact guide is [`docs/library.md`](docs/library.md).

Native startup, window creation, event polling, Vulkan presentation, shader packaging, and the demo executable are outside the core target and outside the installed core header set.

## Target graph

- `EpochSimEngine::EpochSimEngine` — canonical platform-neutral library target.
- `EpochSimEngine::VulkanRuntime` — canonical optional Vulkan runtime target.
- `SandHybrid::SandHybrid` and `SandHybrid::VulkanRuntime` — compatibility aliases.
- `SandHybrid_Demo` — native Win32 or XCB host executable; output name `sandhybrid`.

The compatibility build-tree alias `Autodidac::SandHybrid` remains available.

## Headless library build

```bash
cmake -S . -B build/library \
  -DEPOCHSIMENGINE_BUILD_DEMO=OFF \
  -DEPOCHSIMENGINE_BUILD_VULKAN_RUNTIME=OFF \
  -DBUILD_TESTING=ON
cmake --build build/library --parallel
ctest --test-dir build/library --output-on-failure
cmake --install build/library --prefix build/library-package
```

This path does not configure EpochGui, find Vulkan, compile shaders, include native window sources, or link window-system libraries. Its installed include tree contains only platform-neutral EpochSimEngine headers and their legacy compatibility counterparts; runtime-only Vulkan, window, application, shared-state, UI, and EpochGui headers are deliberately excluded.

## Downstream use

```cmake
find_package(EpochSimEngine CONFIG REQUIRED)

target_link_libraries(my_simulation PRIVATE EpochSimEngine::EpochSimEngine)
target_compile_features(my_simulation PRIVATE cxx_std_23)
```

```cpp
#include <epochsimengine/library.hpp>

static_assert(epochsimengine::library_name == "EpochSimEngine");
```

The installed core package exports `EpochSimEngineTargets.cmake`, `EpochSimEngineConfig.cmake`, and a same-major-version file. `SandHybridConfig.cmake` is a compatibility shim. The optional Vulkan backend is currently build-tree-only. The downstream package contract installs the library into a clean prefix, rejects runtime-header leakage, and executes canonical and legacy consumers without repository-private include paths.

## Native demo build

```bash
cmake -S . -B build/app \
  -DEPOCHSIMENGINE_BUILD_DEMO=ON \
  -DEPOCHSIMENGINE_BUILD_VULKAN_RUNTIME=ON
cmake --build build/app --parallel
```

`EPOCHSIMENGINE_BUILD_DEMO=ON` requires the Vulkan runtime. The backend may be built without the demo for an alternate native host by enabling `EPOCHSIMENGINE_BUILD_VULKAN_RUNTIME` and disabling `EPOCHSIMENGINE_BUILD_DEMO`. Legacy `SANDHYBRID_*` options remain accepted.

## Ownership boundary

The core library never owns a process entry point, native window, input loop, or presentation surface. A consumer owns those resources and may use the optional Vulkan runtime or provide another backend. `SceneCell` and scheduler values are ordinary value types; library APIs use spans, paths, returned values, and caller-owned storage rather than hidden global ownership.

## Deterministic core state contracts

EpochSimEngine API v4 exposes platform-neutral foundations for the replacement runtime:

- packed Atmosphere composition with exact pressure/component conservation;
- atomic all-or-fallback represented-material packet transfers;
- actor occupancy and medium impulses without encoding actors as material cells;
- atomic directional machine and sluice transactions;
- explicit Ant/Beetle habitat capacity, inputs, outputs, and cadence.

These APIs are deterministic and covered by Windows/Linux contracts. The Vulkan runtime is being migrated onto them incrementally; `missioncache.md` retains every production and packaged-observation requirement until it passes.

## v2.5.6 runtime controls

Right-click exclusively pans: dragging moves the current camera and holding it near a viewport edge performs gated edge panning. `WASD PAN` routes keys to the simulation camera; MAP uses its own camera and a slow full-world snapshot without changing simulation LOD or active-region scheduling. In the persistent World, MINE uses left click and BUILD places the selected resource from the sidebar Inventory pane with left click. Terminal laser damage is an exact world-to-world transfer into an adjacent body-clear gas cell, or it retains the source when no such destination exists. It never deletes matter or collects inventory. Hold `F` and left-click the simulation to fill; pressing `F` alone does nothing. Simulation pause keeps those direct editor mutations live while clocks, actors, reactions, MAP refresh, and effects remain frozen. World `RESET` preserves both cameras; `H` and the sidebar `H CAM HOME` button explicitly restore the active simulation or MAP view.
