# SandHybrid v2.4.2

This release is a broad runtime implementation pass against the unified `missioncache.md`.

## Included implementation attempts

- Camera-centered 17-section starburst and deterministic section-worker assignment.
- Expanded world/camera foundation, camera-zero control, and wider zoom range.
- C++23 `SandHybrid` static-library target with a thin demo executable.
- Exact suspended-hive restoration path.
- Settled granular terrain transition to structural state, with collapse on renewed instability.
- Gas/liquid damping and bounded player-medium disturbance.
- Transient 8x8 gas/liquid movement packets: exposed resting media return to fine-cell ownership; fully enclosed media may remain tile-owned.
- Larger high-contrast debug statistics, additional hierarchy counters, and an on-screen color key.
- Atmosphere-tool and UI cleanup.

## Build corrections

- Strict GCC/Clang warning-as-error builds retain every warning except the intentional Vulkan C-aggregate zero-fill diagnostic.
- The Linux thin demo now receives `VK_USE_PLATFORM_XCB_KHR`, matching the renderer library and enabling the XCB Vulkan surface API declarations.

## Validation policy

Windows and Linux Release builds compile all 12 entry shaders, build the C++23 targets, run the contract tests, install packages, and archive platform artifacts.

Runtime-only behavior is not marked complete from compilation. Open, partial, regressed, and deferred missions remain in `missioncache.md` for post-release scene testing and carry forward unchanged until accepted.
