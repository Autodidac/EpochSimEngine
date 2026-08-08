# SandHybrid v2.5.20

Corrective GPU pacing, the photographed Fix29 hive, complete Stone foundations, and visible fresh-Water flow.

## Corrected behavior

- The renderer submits at most one complete material/actor simulation tick per presented frame. The simulation remains a fixed 60 Hz discrete step when sustainable, drops stale time debt after missed deadlines, and cannot create the former four-pass GPU catch-up spiral in bee-heavy scenes.
- Sandbox, Ecosystem, the Beehive tool, and loaded-map normalization now share the photographed Fix29 Ecosystem body/content cell-for-cell: shell `28 <= radius^2 < 108`, chamber `radius^2 < 28`, exit through `x=12`, and canonical Empty/Honey/Pollen entropy seed `0xD17A55DE`.
- The hive perch is nine complete grid-aligned structural Wood tiles at local `x=472..543`, `y=216..223`. The compact body overrides its overlapping perch cells, and the perch no longer crumbles from loose-cell support logic.
- Every generated authored scene now owns a complete eight-cell Stone foundation across all 640 columns. Imported scene images preserve the same foundation while leaving deliberate authored rooms and cavities empty above it.
- Full Water now uses the working liquid diagonal/ledge path shared with Saltwater and Oil. A supplied ledge needs only one trailing half-unit, isolated full Water can cross an unsupported edge, and Half Water retains fall-first, persistent drip, clear-gap attraction, deterministic merge, conservation, and sleep rules.
- SandHybrid now vendors the complete current EpochGui v0.88.75 tree from commit `d8decc9`, builds its static C++ modules and default tests on MSVC, and uses the same current compatibility headers on GCC toolchains without module dependency scanning.
- The v2.5.19 full-tile scenes, Designer Clear, bounded Fill, supported player recovery, half-speed macro cadence, doubled packet cohesion allowance, paused editing, sidebar workspaces, Blueprint transactions, and logical high-DPI cursor mapping remain preserved.

## Packaged Vulkan acceptance

Run the installed executable with:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

The shipped production Vulkan pipelines and resident buffers must pass 19 state checks: exact Water/Hydrogen macro packets, blocked fine fallback, four Half Water cases, isolated and supplied full-Water ledges, all nine structural Stone foundations, and exact Sandbox/Ecosystem Fix29 hives.

## Validation and active work

The exact release source must pass native Windows and Linux C++23 Release builds, all 31 Windows tests (including EpochGui's three default suites), all 28 Linux SandHybrid tests, fresh package audits and checksums, and packaged Vulkan state readback before publication. These focused results do not close broader visual scene review, long-running leveling/performance, wet granular, machinery, save/load, bee-lifecycle, or complete scene-cycle missions; `missioncache.md` remains authoritative.

The stable tag workflow publishes exactly:

- SandHybrid-Windows-x64-v2.5.20.zip
- SandHybrid-Windows-x64-v2.5.20.zip.sha256
- SandHybrid-Linux-x64-v2.5.20.tar.gz
- SandHybrid-Linux-x64-v2.5.20.tar.gz.sha256
