# SandHybrid v2.4.8

This release completes the current UI, inspection, launcher, wet-material, and debug-readability pass while auditing every active mission in the canonical cache.

## Interface and terminology

- Player-facing material ID 3 is labeled `SOIL` while internal IDs and saves remain compatible.
- `ATMOSPHERE` is an always-visible balanced-air fill action.
- `ERASER` is an adjacent, visually distinct vacuum/empty deletion tool.
- `OXYGEN` remains an independent selectable material.
- Fresh startup uses a square brush at size 2.
- Thin separators clarify scene controls, editing controls, resource metrics, simulation activity, events, and debug state cards without adding boxed clutter.

## Debug readability

- Relational debug states are presented as bottom-anchored cards with a real color swatch and state name.
- `BULK READY` uses vivid violet; `SETTLED` uses vivid green; sleeping remains dark and cool.
- Resource-critical information remains first.
- Pair-test estimates now reflect active map-footprint areas rather than the entire resident allocation.
- Debug integers support eight digits, eliminating the false `9999999` saturation shown in prior builds.
- `PAIR TESTS` and `SKIPPED` retain the last completed sample instead of clearing on render-only frames.

## Wet materials

- Moisture-retaining solids and powders show derived `WET <MATERIAL>` inspection names and wet color treatment.
- Wet variants have no palette creation controls and arise only from mixing, absorption, reaction, or loaded state.
- Sand remains slightly hydrophobic.
- Wet sand receives a density bonus and can sink through ordinary liquids.

## Windows launcher and packaging

- Root `run.bat` is restored, searches packaged and common Release locations, forwards arguments, and reports launch failures.
- The Windows install/package includes `run.bat` at the package root.

## Mission-cache integrity

- All recent requirements MC-092 through MC-098 are inside the active mission section.
- Conflicting legacy Atmosphere/Eraser missions were corrected rather than silently ignored.
- A mission-cache validator rejects duplicate, malformed, misplaced, or missing recent missions.
- Every active mission was reviewed once in the v2.4.8 broad-pass audit. Large simulation, Atmosphere, ecology, streaming, and runtime-observed work remains active until actually accepted.

## Validation gate

Publication requires generated-source validation, the mission-cache contract, all shaders, Windows 2022 and Ubuntu 24.04 C++23/Vulkan Release builds, all deterministic tests, installation, archive creation, and package upload.
