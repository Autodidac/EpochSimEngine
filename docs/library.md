# EpochSimEngine library

EpochSimEngine is a platform-neutral C++23 static library; the bundled SandHybrid application is an example consumer, not the library itself. New consumers use `find_package(EpochSimEngine)` and `EpochSimEngine::EpochSimEngine`. The installed `SandHybrid::SandHybrid` target and `sandhybrid/*` public headers remain compatibility APIs.

## Targets

- `EpochSimEngine::EpochSimEngine` owns public value types and deterministic policies for materials, Atmosphere, actors, inventory, packets, machinery, scene images, saves, sections, terrain, and persistent-world layout.
- `EpochSimEngine::VulkanRuntime` is optional and owns the GPU cell simulation, renderer, EpochGui integration, shaders, and runtime persistence.
- `SandHybrid_Demo` owns the native process, window, input loop, and bundled example world.

The core target never owns a process entry point, native window, input loop, presentation surface, or hidden global world. Callers own storage and pass spans, paths, and value objects across the boundary.

## Downstream use

```cmake
find_package(EpochSimEngine CONFIG REQUIRED)
target_link_libraries(my_simulation PRIVATE EpochSimEngine::EpochSimEngine)
target_compile_features(my_simulation PRIVATE cxx_std_23)
```

```cpp
#include <sandhybrid/library.hpp>
```

The installed package exports `EpochSimEngineConfig.cmake` plus the compatibility `SandHybridConfig.cmake`, target export, and same-major version files. A library-only build neither finds Vulkan nor configures EpochGui, shaders, or native window dependencies.

## Public state contracts

- Canonical cells remain the source of truth; tiles and chunks are reversible scheduling metadata.
- Atmosphere composition, pressure, material packets, inventory transfers, machine transactions, and save owners use explicit conserved ownership.
- The persistent-world layout and scene-image/save formats are deterministic and platform-neutral.
- Runtime-only headers are not installed with the core public API.

The current implementation is evolving behind preserved mission acceptance. API or behavior claims are complete only when their platform contracts, runtime evidence, and package checks pass.

## Licenses and binary requirements

EpochSimEngine's own code uses the root MIT license. The optional SandHybrid runtime also incorporates EpochGui under its separate `LicenseRef-MIT-NoSell` terms; runtime installs carry the exact notice in `docs/licenses/EpochGui.txt`. The Windows package includes the Vulkan loader and its notice in `docs/licenses/VulkanLoader.txt`. These dependency terms are not replaced by the library's MIT license.

The native example needs an x64 system and a Vulkan-capable driver. Windows binaries are unsigned and require the Microsoft Visual C++ v14 x64 runtime (`MSVCP140.dll`, `VCRUNTIME140.dll`, and `VCRUNTIME140_1.dll`); those system redistributables are not included in the archive. Linux binaries use the system Vulkan loader, XCB, and C++ runtime; software Vulkan/Xvfb validation is functional evidence, not a hardware frame-rate promise. Keep each package's executable, shaders, and dependency files together.
