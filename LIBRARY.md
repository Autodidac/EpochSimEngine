# SandHybrid library

`SandHybrid` is the reusable C++23 static library. It owns platform-neutral scene image I/O, active-region scheduling, material and simulation policy headers, and the stable public entry point `sandhybrid/library.hpp`.

Native startup, window creation, event polling, Vulkan presentation, shader packaging, and the demo executable are outside the core target.

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

This path does not configure EpochGui, find Vulkan, compile shaders, include native window sources, or link window-system libraries.

## Downstream use

```cmake
find_package(SandHybrid CONFIG REQUIRED)

target_link_libraries(my_simulation PRIVATE SandHybrid::SandHybrid)
target_compile_features(my_simulation PRIVATE cxx_std_23)
```

```cpp
#include <sandhybrid/library.hpp>
```

The installed package exports `SandHybridTargets.cmake`, `SandHybridConfig.cmake`, and a same-major-version compatibility file. The downstream package contract installs the library into a clean prefix, configures an external consumer with `find_package`, and links that consumer without repository-private include paths.

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
