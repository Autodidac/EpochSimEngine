# SandHybrid v2.4.9

This release fixes the Atmosphere/Fill UI regression in v2.4.8 while preserving the accepted camera, launcher, debug, wet-material, and mission-cache work.

## Independent editing controls

- `ATMOSPHERE` now selects balanced breathable air for ordinary painting.
- `F FILL` is a separate action that fills the active region with the currently selected material.
- The keyboard `F` command and visible Fill button use the same region-fill command.
- Fill preserves the current material selection after the operation.
- `ERASER` remains a separate vacuum/empty selection.
- Atmosphere, Fill, and Eraser have independent hit regions, colors, labels, and actions.
- Selecting Atmosphere no longer temporarily changes the selection, raises the Fill command, or restores a previous material behind the user's back.

## Atmosphere correctness

- CPU-driven region fill initializes Atmosphere with its 54/255 breathable oxygen fraction, matching shader-created Atmosphere cells.
- Pure `OXYGEN` remains independently selectable.

## Solid tile correction

- Stone and every block-capable solid are excluded from whole-tile movement.
- A complete solid region may still report `BULK READY` for cohesion, support, sleeping, and debug visibility.
- Stable tile metadata no longer reattaches damaged loose cells as structural cells.
- Released stone and other block-capable solids fall only as individual fine cells.
- A tile crumbles when 31 of 64 cells are destroyed: 48.4375%, the first whole-cell count at or above 48%.

## Regression coverage

- Static contracts reject any Atmosphere handler that raises `fill_region`.
- Static contracts require the independent Fill handler and Atmosphere composition initialization.
- Static contracts reject block-capable macro movement and stable-tile cell reconstruction.
- The canonical mission cache carries 81 active missions forward: 50 partial, 15 open, 15 regressions, and 1 deferred.

## Validation gate

Publication requires generated-source and shader contracts, mission-cache validation, all deterministic tests, Windows 2022 and Ubuntu 24.04 C++23/Vulkan Release builds, installation, archive creation, and package upload for both platforms.
