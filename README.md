# EpochSimEngine

EpochSimEngine is a reusable C++23 material-simulation library. `SandHybrid` is the bundled native Vulkan example and systems testbed built on that library.

The repository intentionally separates three layers:

- `EpochSimEngine::EpochSimEngine` — platform-neutral installed library target; `SandHybrid::SandHybrid` remains a compatibility target.
- `EpochSimEngine::VulkanRuntime` — optional Vulkan simulation and presentation backend.
- `SandHybrid_Demo` — bundled Windows/XCB example application; its executable is named `sandhybrid`.

Start with:

- [`docs/library.md`](docs/library.md) for the library API, target graph, installation, and downstream use.
- [`docs/sandhybrid.md`](docs/sandhybrid.md) for the example world, controls, materials, saves, and runtime behavior.
- [`docs/development.md`](docs/development.md) for architecture, validation, packaging, and contributor workflow.

Repository agents must read `missioncache.md` and `MISSION_LEDGER.md` before planning or changing behavior. Those are internal acceptance records and are deliberately not part of the installed runtime package.

## Quick build

Library only:

```bash
cmake -S . -B build/library -DSANDHYBRID_BUILD_APP=OFF -DSANDHYBRID_BUILD_VULKAN_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build build/library --parallel
ctest --test-dir build/library --output-on-failure
cmake --install build/library --prefix build/library-package
```

Bundled SandHybrid example:

```bash
cmake -S . -B build/app -DSANDHYBRID_BUILD_APP=ON -DSANDHYBRID_BUILD_VULKAN_RUNTIME=ON -DBUILD_TESTING=ON
cmake --build build/app --parallel
ctest --test-dir build/app --output-on-failure
```

Windows convenience scripts use Visual Studio 2022 and the configured vcpkg checkout:

```bat
build_windows.bat Release
run_windows.bat Release
```

Linux requires CMake 3.28+, Ninja, XCB development files, Vulkan, and vcpkg:

```bash
VCPKG_ROOT="$HOME/vcpkg" ./build_linux.sh Release
./run_linux.sh Release
```

## License and releases

See `LICENSE` and `CHANGELOG.md`. Public releases are normal stable, versioned releases only; intermediate corrective commits are not publication authorization.
