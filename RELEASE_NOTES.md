# SandHybrid v2.5.21

Scene, Water, machinery, atmosphere, hive, debug, and presentation recovery.

## Corrected behavior

- Volcano reset now follows the supplied 2026-08-04 reference: broad left lake, continuous detailed strata, a far-right Stone cone, open crater, straight Lava throat, bulb chamber, local vents, and a bottom Lava return band. Underground transitions are wider and smoother, and decorative broken-tile pockets are much rarer.
- Full Water keeps the restored Saltwater/Oil-style ledge flow. Half Water is again a darker static one-half-unit state with fall-first movement, adjacent merge, clear two-to-four-cell attraction, and the conserved supplied-ledge hang/drip. A residual Water pixel may rest only when no productive fall, merge, equalization, pressure, reaction, heat, actor, or tool move exists.
- Engineering, Industry, and Gold Mine recover active production layouts. The visible water-fed Sluice processes one wet feed at a time, retains its Water stream, uses a deterministic ten-percent Gold roll, and emits dry Silt on the non-Gold route without consuming inputs when output is blocked.
- `IGNITE AIR` now lives in a dedicated sidebar `ACTIONS` section immediately above `KEYMAP`; it remains a paused-live edit and no longer occupies a material slot.
- Normal material presentation is static. The existing metal/ore glint remains the only render-clock cosmetic animation. Gas boundaries and opacity derive from current gas identity, pressure, temperature, and neighboring authoritative state.
- Atmosphere expands conservatively into connected Vacuum. Adjacent raw Oxygen, Carbon Dioxide, Hydrogen, and Atmosphere cells mix into a common pressure-bearing Atmosphere without creating pressure.
- Fire decays primarily to Smoke, Fire-to-Ember is reduced, Ember-to-Ash is reduced fourfold, and Ash-producing fuel/plastic/volcanic routes are throttled.
- Every valid scene now requests a supported, body-clear, breathable player spawn and uses deterministic recovery when the authored location becomes invalid.
- Reset, Beehive placement, and loaded-map normalization share the photographed historical SimpleSandSim Sandbox hive: shell `24 <= radius^2 < 88`, chamber `radius^2 < 24`, exit `x=1..10`, canonical entropy `0xD17A5EED`, and nine complete structural Wood support tiles. SandHybrid retains its own bee runtime and colony cap.
- Debug telemetry samples one rotating authored region at a bounded cadence instead of scanning the resident world every frame. The overlay uses the shared logical sidebar layout.
- Settings now offers `30`, `60`, `120`, and `UNLIMITED` presentation limits. Simulation remains fixed at 60 Hz: 30 presents every other simulation submission, 60 presents each tick, and 120/unlimited may insert render-only frames without extra simulation work.
- The complete EpochGui snapshot remains synchronized to current upstream `main`, v0.88.75 at `d8decc9ee2e73e0009f1e8c49d86a52db6748b28`.

## Packaged Vulkan acceptance

Run the installed executable with:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

The production Vulkan reset/movement/readback gate checks macro Water and gas packets, failed-packet fine fallback, Half Water cases, Water ledges, all scene foundations, and both generated canonical hives. Broader visual, lifecycle, long-duration leveling, and performance missions remain active in `missioncache.md` until packaged observation supplies their evidence.

## Stable assets

- SandHybrid-Windows-x64-v2.5.21.zip
- SandHybrid-Windows-x64-v2.5.21.zip.sha256
- SandHybrid-Linux-x64-v2.5.21.tar.gz
- SandHybrid-Linux-x64-v2.5.21.tar.gz.sha256