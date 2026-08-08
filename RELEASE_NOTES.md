# SandHybrid v2.5.18

Stable terrain, Half Water, Blueprint, hard-coded Beehive, and EpochGui recovery.

## Corrected behavior

- Every generated scene now continues the authored top terrain as a flat grass-over-dirt plateau down to the shared stone foundation. The noisy low strip at scene edges is removed; far-right resident terrain remains available for later block-aligned hills.
- Half Water now owns an early fine-only state machine: it falls first, merges adjacent halves, attracts only through a clear two-to-four-cell gap, never enters generic full-Water flow, and is protected from full-cell chemistry until consolidation.
- Blueprint placement validates the complete transformed payload once, then performs a bounded GPU upload to both resident buffers. It no longer downloads and reuploads the complete world, and Inventory Blueprint selection places directly without Designer ownership.
- Sandbox and Ecosystem reset, the Beehive tool, and loaded-map normalization share the exact Fix36 pre-PR19 hard-coded hive: canonical queen centers, loose Wood perch, shell/chamber/exit geometry, queen, and scene-local Empty/Honey/Pollen contents. SandHybrid bee behavior remains unchanged.
- The selected EpochGui font contract is synchronized to upstream commit 130f33fe31d73564a35a622f3bb5ddcc2b5105d5, including logical font height and DPI conversion while native input remains logical.
- The v2.5.3 macro-tile baseline, paused live editing, sidebar-only Inventory/Designer workspaces, and high-DPI cursor mapping remain enforced.

## Packaged Vulkan acceptance

Run the installed executable with:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

The shipped Vulkan pipelines and resident buffers must pass all nine focused checks: exact Water and Hydrogen packets, conserved blocked fine fallback, Half Water attraction/merge, fall-first and persistent drip behavior, supplied-ledge creation, and exact Sandbox/Ecosystem hard-coded hives.

## Validation and active work

The exact release source must pass native Windows and Linux C++23 Release builds, all 27 deterministic/shader/interface tests on each platform, package installation and archive audits, and the packaged Vulkan state-readback gate before publication. These focused results do not close broader leveling, wet granular, machinery, save/load, bee-lifecycle, complete scene-cycle, or visual acceptance missions; missioncache.md remains authoritative.

The stable tag workflow publishes exactly:

- SandHybrid-Windows-x64-v2.5.18.zip
- SandHybrid-Windows-x64-v2.5.18.zip.sha256
- SandHybrid-Linux-x64-v2.5.18.tar.gz
- SandHybrid-Linux-x64-v2.5.18.tar.gz.sha256
