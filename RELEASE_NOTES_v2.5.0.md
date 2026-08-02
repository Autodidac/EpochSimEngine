# SandHybrid v2.5.0

SandHybrid v2.5.0 establishes the reusable library architecture and starts the production simulation rewrite. The current Vulkan runtime remains available while the replacement section-driven core is migrated behind deterministic and runtime parity gates.

## Reusable library architecture

- `SandHybrid::SandHybrid` is a platform-neutral C++23 static library.
- `SandHybrid::VulkanRuntime` is optional and owns Vulkan simulation/presentation integration.
- `SandHybrid_Demo` remains the thin native startup, window, input, and event host.
- Installed CMake package exports support downstream `find_package(SandHybrid)` consumers.
- Public API, downstream package, Windows, Linux, headless, and full Vulkan contracts remain enforced.

## Production rewrite initiated

- Added `REWRITE_PLAN.md` as the staged production migration program.
- Added MC-101 through MC-110 to the canonical mission cache.
- Established canonical ownership rules: cells are material truth, solid tiles are metadata only, and liquid/gas packets are transactional acceleration.
- Preserved the existing runtime until deterministic, conservation, visual, Windows/Linux, and old-hardware gates pass.

## Sparse section-grid foundation

- Added a platform-neutral sparse grid of signed 64x64 simulation sections.
- Added per-section local dirty rectangles so work can scale with changed area.
- Added four deterministic non-touching phases; sections in one phase share no edge or corner.
- Added boundary-halo wakeups, automatic sleeping, and clean metadata retirement.
- Defined 512x512 stream pages as eight-by-eight section groups for the later persistence layer.
- Added negative-coordinate, cross-boundary, phase, sleep, halo, capacity, and retirement contracts.

## Carried-forward simulation corrections

- `ATMOSPHERE`, `F FILL`, `OXYGEN`, and `ERASER` remain independent controls.
- Balanced Atmosphere preserves its breathable component in CPU region fill.
- Block-capable solids never translate as whole 8x8 packets.
- At 31 destroyed cells of 64 (48.4375%), the remaining solid cells release individually.
- Runtime-unverified water, half-water, packet ownership, atmosphere composition, machinery, ecology, and streaming missions remain active in `missioncache.md`.

## Repository cleanup

- Removed the obsolete v2.4.9 package workflow.
- Removed temporary one-shot patch applicators and staging payloads.
- The permanent v2.5.0 workflow is the sole release-package matrix for this version.

## Validation gate

Publication requires shader/generated-source contracts, mission-cache validation, C++23 warnings-as-errors builds, all deterministic tests, downstream package installation/use, and complete Windows 2022 and Ubuntu 24.04 Vulkan Release package builds from the exact final source head.
