# SandHybrid v2.5.19

Scenic full-tile scenes, live tool recovery, visible players, and frame-independent macro pacing.

## Corrected behavior

- Every scene now owns its intended full `8x8` grass/dirt/foundation surface row. Authored non-Blank interiors stay empty instead of being backfilled by resident substrate, while structural frames and starting liquid volumes use complete aligned tiles.
- Volcano and Waterworks starting water is block-authored; the scene-specific terrain heights preserve the intended silhouettes and leave room for a blocky far-right resident hill.
- Designer has a sidebar `CLEAR` action that empties only its isolated grid. Editor `FILL` remains a confirmed four-connected Air replacement and now uploads only changed resident ranges instead of rewriting both complete world buffers.
- Gold Mine, Demolition, and Frontier Base players start on supported, body-clear, breathable cells. Invalid configured spawns recover deterministically nearby rather than remaining embedded, suffocating, or invisible.
- Complete liquid and gas packets attempt macro movement every two fixed 60 Hz ticks—half the prior rate—and retain macro cohesion for two classifier ticks before fine breakup. A blocked packet still enters fine fallback on the attempted tick.
- Simulation, actors, clocks, effects, packet cadence, and breakup age advance from fixed ticks with bounded catch-up. Presented frame rate no longer changes their speed, and one-shot input is consumed only on the first catch-up tick.
- The exact Fix36 pre-PR19 hive model and SandHybrid bee behavior, Half Water fall/merge/clear-gap attraction, paused editing, sidebar Inventory/Designer ownership, Blueprint transaction rules, and logical high-DPI cursor mapping remain preserved.

## Packaged Vulkan acceptance

Run the installed executable with:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

The shipped Vulkan pipelines and resident buffers must retain all nine focused checks: exact Water and Hydrogen packets, conserved blocked fine fallback, Half Water attraction/merge, fall-first and persistent drip behavior, supplied-ledge creation, and exact Sandbox/Ecosystem hard-coded hives.

## Validation and active work

The exact release source must pass native Windows and Linux C++23 Release builds, all 28 deterministic/shader/interface tests on each platform, fresh package audits and checksums, and packaged Vulkan state readback before publication. These focused results do not close broader scenic visual review, leveling, wet granular, machinery, save/load, bee-lifecycle, or complete scene-cycle missions; `missioncache.md` remains authoritative.

The stable tag workflow publishes exactly:

- SandHybrid-Windows-x64-v2.5.19.zip
- SandHybrid-Windows-x64-v2.5.19.zip.sha256
- SandHybrid-Linux-x64-v2.5.19.tar.gz
- SandHybrid-Linux-x64-v2.5.19.tar.gz.sha256
