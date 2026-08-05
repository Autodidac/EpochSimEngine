# SandHybrid library

`SandHybrid` is the reusable C++23 static library. It owns platform-neutral scene image I/O, active-region scheduling, material and simulation policy headers, and the stable public entry point `sandhybrid/library.hpp`.

Native startup, window creation, event polling, Vulkan presentation, shader packaging, and the demo executable are outside the core target and outside the installed core header set.

## Target graph

- `SandHybrid::SandHybrid` — platform-neutral static library and installed package target.
- `SandHybrid::VulkanRuntime` — optional in-tree Vulkan simulation/presentation target.
- `SandHybrid_Demo` — native Win32 or XCB host executable; output name `sandhybrid`.

The compatibility build-tree alias `Autodidac::SandHybrid` remains available.

## Headless library build

```bash
cmake -S . -B build/library \
  -DSANDHYBRID_BUILD_APP=OFF \
  -DSANDHYBRID_BUILD_VULKAN_RUNTIME=OFF \
  -DBUILD_TESTING=ON
cmake --build build/library --parallel
ctest --test-dir build/library --output-on-failure
cmake --install build/library --prefix build/library-package
```

This path does not configure EpochGui, find Vulkan, compile shaders, include native window sources, or link window-system libraries. Its installed include tree contains only platform-neutral SandHybrid headers; runtime-only Vulkan, window, application, shared-state, UI, and EpochGui headers are deliberately excluded.

## Downstream use

```cmake
find_package(SandHybrid CONFIG REQUIRED)

target_link_libraries(my_simulation PRIVATE SandHybrid::SandHybrid)
target_compile_features(my_simulation PRIVATE cxx_std_23)
```

```cpp
#include <sandhybrid/library.hpp>
```

The installed package exports `SandHybridTargets.cmake`, `SandHybridConfig.cmake`, and a same-major-version compatibility file. The downstream package contract installs the library into a clean prefix, rejects runtime-header leakage, configures an external consumer with `find_package`, and links that consumer without repository-private include paths.

## Native demo build

```bash
cmake -S . -B build/app \
  -DSANDHYBRID_BUILD_APP=ON \
  -DSANDHYBRID_BUILD_VULKAN_RUNTIME=ON
cmake --build build/app --parallel
```

`SANDHYBRID_BUILD_APP=ON` requires the Vulkan runtime. The runtime may be built without the demo for an alternate native host by enabling `SANDHYBRID_BUILD_VULKAN_RUNTIME` and disabling `SANDHYBRID_BUILD_APP`.

## Ownership boundary

The core library never owns a process entry point, native window, input loop, or presentation surface. A consumer owns those resources and may use the optional Vulkan runtime or provide another backend. `SceneCell` and scheduler values are ordinary value types; library APIs use spans, paths, returned values, and caller-owned storage rather than hidden global ownership.

## Deterministic core state contracts

SandHybrid API v3 exposes platform-neutral foundations for the replacement runtime:

- packed Atmosphere composition with exact pressure/component conservation;
- atomic all-or-fallback represented-material packet transfers;
- actor occupancy and medium impulses without encoding actors as material cells;
- atomic directional machine and sluice transactions;
- explicit Ant/Beetle habitat capacity, inputs, outputs, and cadence.

These APIs are deterministic and covered by Windows/Linux contracts. The Vulkan runtime is being migrated onto them incrementally; `missioncache.md` retains every production and packaged-observation requirement until it passes.

## v2.5.6 runtime controls

Right-click exclusively pans: dragging moves the current camera and holding it near a viewport edge performs gated edge panning. `WASD PAN` routes keys to the simulation camera; MAP uses its own camera and a slow full-world snapshot without changing simulation LOD or active-region scheduling. In player scenes, MINE uses left click and BUILD places the selected resource from the sidebar Inventory pane with left click. Hold `F` and left-click the simulation to fill; pressing `F` alone does nothing. Simulation pause keeps those direct editor mutations live while clocks, actors, reactions, MAP refresh, and effects remain frozen.
