# SandHybrid v2.4.9

This release fixes the Atmosphere/Fill UI regression in v2.4.8 while preserving the accepted camera, launcher, debug, wet-material, and mission-cache work.

## Independent editing controls

- `ATMOSPHERE` now selects balanced breathable air for ordinary painting.
- `F FILL` is a separate action that fills the active region with the currently selected material.
- `ERASER` remains a separate vacuum/empty selection.
- Atmosphere, Fill, and Eraser have independent hit regions, colors, labels, and actions.
- Selecting Atmosphere no longer temporarily changes the selection, raises the Fill command, or restores a previous material behind the user's back.

## Atmosphere correctness

- CPU-driven region fill initializes Atmosphere with its 54/255 breathable oxygen fraction, matching shader-created Atmosphere cells.
- Pure `OXYGEN` remains independently selectable.

## Regression coverage

- Static contracts reject any Atmosphere handler that raises `fill_region`.
- Static contracts require the independent Fill handler and Atmosphere composition initialization.
- The canonical mission cache carries the remaining runtime and architecture work forward instead of falsely closing it.

## Validation gate

Publication requires generated-source and shader contracts, mission-cache validation, all deterministic tests, Windows 2022 and Ubuntu 24.04 C++23/Vulkan Release builds, installation, archive creation, and package upload for both platforms.
