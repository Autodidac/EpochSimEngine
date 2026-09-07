# EpochSimEngine

EpochSimEngine is the project and reusable C++23 material-simulation library. **SandHybrid is only its bundled native Vulkan demo**, not an alternate name for the engine or library.

The repository intentionally separates three layers:

- `EpochSimEngine::EpochSimEngine` — platform-neutral installed library target; `SandHybrid::SandHybrid` remains a compatibility target.
- `EpochSimEngine::VulkanRuntime` — optional Vulkan simulation and presentation backend.
- `SandHybrid_Demo` — bundled Windows/XCB demo application; its executable is named `sandhybrid`.

Start with:

- [`docs/library.md`](docs/library.md) for the library API, target graph, installation, and downstream use.
- [`docs/sandhybrid.md`](docs/sandhybrid.md) for the example world, controls, materials, saves, and runtime behavior.
- [`docs/development.md`](docs/development.md) for architecture, validation, packaging, and contributor workflow.

Repository agents must read `missioncache.md` and `MISSION_LEDGER.md` before planning or changing behavior. Those are internal acceptance records and are deliberately not part of the installed runtime package.

## v2.5.29 release scope

The explicitly authorized September 6 release contains the **Windows demo and committed-source archives only**. The existing public Linux download remains **v2.5.27**. Linux v2.5.29 is **HELD**, not a current native download; including Linux build support in the source archives does not certify its runtime.

Native source `716722969d059e8c76791107b3ff9b0e267eb372` passes Windows Release CTest 90/90, installed Large Vulkan state checks 164/164, and 12/12 material/save cycles. Idle Windows presentation passes 1,920 frames and fixed ticks in 34.5001 seconds; the paired REGION draw p95 increment is 1.2361 ms, not an attributed speedup. Linux Release CTest passes 79/79, but the final runtime run was intentionally stopped without a completed JSON report; its subsequent finite/presentation gates did not run.

This release-only exception does not close any of the 123 active missions. Personal visual approval, longer hive/ecology recurrence, Editor controls clipping at 720p, crowded Designer text, legacy KEYMAP entries, and broader performance/material work remain open. See `VALIDATION.md` for evidence boundaries.

## Quick build

Library only:

```bash
cmake -S . -B build/library -DEPOCHSIMENGINE_BUILD_DEMO=OFF -DEPOCHSIMENGINE_BUILD_VULKAN_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build build/library --parallel
ctest --test-dir build/library --output-on-failure
cmake --install build/library --prefix build/library-package
```

Bundled SandHybrid demo:

```bash
cmake -S . -B build/app -DEPOCHSIMENGINE_BUILD_DEMO=ON -DEPOCHSIMENGINE_BUILD_VULKAN_RUNTIME=ON -DBUILD_TESTING=ON
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

The demo checks the selected Vulkan device's limits before allocating a World.
On the current 128-MiB-limit llvmpipe software driver, use
`./build/linux-Release/sandhybrid --world-size compact` after building; Large
requires a device supporting a full 225-MiB storage-buffer range. Compact keeps
all eight districts. See `docs/sandhybrid.md` for the complete size requirements.

## License and releases

See `LICENSE` and `CHANGELOG.md`. Project releases and committed-source archives are named EpochSimEngine; SandHybrid Windows/Linux packages contain the demo. Old API identifiers and published URLs remain compatibility interfaces, not project branding. Public releases are normal stable, versioned releases only; intermediate corrective commits are not publication authorization.
