# EpochSimEngine library

EpochSimEngine is the project and platform-neutral C++23 static library. SandHybrid names only the bundled demo. New consumers use `find_package(EpochSimEngine)`, `EpochSimEngine::EpochSimEngine`, `<epochsimengine/library.hpp>`, and `epochsimengine::`.

## Targets

- `EpochSimEngine::EpochSimEngine` owns public value types and deterministic policies for materials, Atmosphere, actors, inventory, packets, machinery, scene images, saves, sections, terrain, and persistent-world layout.
- `EpochSimEngine::VulkanRuntime` is optional and owns the GPU cell simulation, renderer, EpochGui integration, shaders, and runtime persistence.
- `SandHybrid_Demo` is the demo and owns its native process, window, input loop, and example world.

The core target never owns a process entry point, native window, input loop, presentation surface, or hidden global world. Callers own storage and pass spans, paths, and value objects across the boundary.

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

The installed core package exports `EpochSimEngineConfig.cmake` and `EpochSimEngineTargets.cmake` with a same-major version file. A library-only build neither finds Vulkan nor configures EpochGui, shaders, or native window dependencies. `EpochSimEngine::VulkanRuntime` is currently a build-tree target, not a supported installed renderer SDK.

### Legacy compatibility

Existing `find_package(SandHybrid)`, `SandHybrid::SandHybrid`, `<sandhybrid/...>` includes, and `sandhybrid::` source/ABI identifiers remain supported. They identify legacy interfaces, not a second project or library. Canonical `epochsimengine/*` headers expose the same types through a namespace alias, so old and new consumers can coexist without payload or ABI migration. The legacy `SANDHYBRID_*` CMake options remain accepted; new builds use `EPOCHSIMENGINE_BUILD_DEMO`, `EPOCHSIMENGINE_BUILD_VULKAN_RUNTIME`, `EPOCHSIMENGINE_ENABLE_VALIDATION`, and `EPOCHSIMENGINE_WARNINGS_AS_ERRORS`.

## Public state contracts

- Canonical cells remain the source of truth; tiles and chunks are reversible scheduling metadata.
- Atmosphere composition, pressure, material packets, inventory transfers, machine transactions, and save owners use explicit conserved ownership.
- The persistent-world layout and scene-image/save formats are deterministic and platform-neutral.
- Runtime-only headers are not installed with the core public API.

`<epochsimengine/bee_colony.hpp>` exposes the deterministic Bee home/slot codec and exact Beehive placement-footprint policy. Call `bee_home_encodable` before constructing a new address. Persistent authored districts retain their original 8-cell home bins and bytes; sky/gap homes use a tagged 16-cell global bin within the supported 16384x2048 envelope. The runtime resolves each bin to its Queen's exact position and rejects overlapping colony footprints. These are Bee-specific bits, not Water state. Schema-2 serialization continues to preserve the complete canonical cell payload unchanged.

The nonpersistent packer's historical PPM constructor bytes are retained for import compatibility, not corrected in place. New-address admission rejects offset legacy canvases such as 1280x360, whose old absolute coordinates plus authored-origin flag would decode to a different home. The canonical zero-origin 640x360 map and current persistent presets remain supported.

The current implementation is evolving behind preserved mission acceptance. API or behavior claims are complete only when their platform contracts, runtime evidence, and package checks pass.

## Licenses and binary requirements

EpochSimEngine's own code uses the root MIT license. Its optional Vulkan backend, used by the SandHybrid demo, also incorporates EpochGui under its separate `LicenseRef-MIT-NoSell` terms; demo installs carry the exact notice in `docs/licenses/EpochGui.txt`. The Windows demo package includes the Vulkan loader and its notice in `docs/licenses/VulkanLoader.txt`. These dependency terms are not replaced by the library's MIT license.

The native example needs an x64 system and a Vulkan-capable driver. Windows binaries are unsigned and require the Microsoft Visual C++ v14 x64 runtime (`MSVCP140.dll`, `VCRUNTIME140.dll`, and `VCRUNTIME140_1.dll`); those system redistributables are not included in the archive. Linux binaries use the system Vulkan loader, XCB, and C++ runtime; software Vulkan/Xvfb validation is functional evidence, not a hardware frame-rate promise. Keep each package's executable, shaders, and dependency files together.
