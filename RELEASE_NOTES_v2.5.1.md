# SandHybrid v2.5.1

SandHybrid v2.5.1 makes the resident world vertically useful instead of leaving every authored scene stranded at the bottom of an oversized empty allocation.

## Common scene stack

- Every generated scene now occupies the horizontally centered upper 640x360 resident footprint at world Y=0.
- Camera startup and Home/reset target that exact authored footprint at one-map scale.
- The bottom eight cells of the authored footprint form a continuous structural Stone foundation across the resident width.
- Exactly three additional 640x360 footprints below the authored scene are filled with deterministic Sand, Soil, Silt, Mud, and Stone geology.
- The resident-world bottom contains a continuous two-brick Lava band.
- Lava is enclosed by one-brick Stone cap, bottom shell, and side walls.

## Saved-scene parity

- Save continues to store the editable 640x360 authored scene image.
- Load starts from balanced Atmosphere, places the saved authored image at the same upper-center origin, then rebuilds the exact common foundation, geology, lava, and shell.
- Generated scenes and loaded saved counterparts therefore share identical resident-world structure outside the authored footprint.

## Library contract

- Added platform-neutral `sandhybrid/world_layout.hpp`.
- Added compile-time contracts for scene origin, three subterranean zones, geology samples, structural strata, and the stone-wrapped two-brick lava band.
- Updated the canonical mission ledger with MC-114 and corrected the obsolete bottom-centered scene requirements.

## Validation

Publication requires shader/generated-source validation, canonical mission-cache validation, C++23 warnings-as-errors headless tests, full Windows 2022 and Ubuntu 24.04 Vulkan Release builds/tests, package installation, and archive generation.
