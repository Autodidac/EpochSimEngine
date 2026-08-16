# SandHybrid v2.5.25

Distributed persistent World, common terrain level, and durable photographed Fix29 hive recovery.

## Corrected behavior

- The eight authored 640x360 districts are no longer packed into the middle of Large. They remain west-to-east but use the complete resident width: Large begins them at X `0, 1368, 2736, 4104, 5472, 6840, 8208, 9576`. Compact remains contiguous because it is exactly eight districts wide; Standard uses aligned intermediate gaps.
- Every district translates its own authored terrain row onto one shared world grass line. In every 1440-cell preset the main grass surface is world Y `1040`, while the authored structures keep their scene-local height above and below that surface.
- Persistent-world reset addressing is direct arithmetic instead of testing all eight districts for every one of 14,745,600 Large cells. The initial 4x4 active window intersects three authored districts instead of the old packed layout's four.
- Sandbox and Ecosystem retain the photographed historical Fix29 hive cell-for-cell: shell `24 <= radius^2 < 88`, chamber `< 24`, queen centers `(512,234)` / `(512,232)`, right exit, canonical seeded Honey/Pollen contents, and nine complete aligned Wood support tiles.
- Canonical hive Honey and Pollen are now fixed structural content during ordinary simulation. The exact placed body remains unchanged after 120 focused ticks instead of collapsing into the later C-shaped shell. Loaded and normalized hives use the same persistence rule.
- One hundred district-aware SandHybrid bees retain foraging, feeding, pollen, queen, birth, migration, hazards, and return-home behavior; no SimpleSandSim bee runtime was imported.
- Complete liquid/gas packets remain reversible metadata over canonical cells, move every two fixed ticks, and retain the eight-opportunity conditional fine-fallback contract. Half Water, paused editing, sidebar ownership, player recovery, machinery, atmosphere, and fixed-step presentation contracts remain intact.

## Release verification

The native Windows Large Vulkan acceptance report proves exact placed hive content both immediately and after 120 ticks, exact Sandbox/Ecosystem hives and 100 unique bee homes, common Y `1040` terrain for all eight districts, three-district startup footprint, healthy player spawn `(4272,1111)`, consecutive macro Water movement, gas movement, enclosed-Air tiling, productive eighth-opportunity fallback, zero-jitter full Water, and conserved Half Water fall/merge/drip cases.

Windows Release passes 35/35 tests, Linux Release passes 32/32, and the installed Large Windows package passes all 26 production Vulkan checks. Install, package-tree, archive, and checksum evidence is recorded in `MISSION_LEDGER.md`. Broad visual traversal, sustained ecology/weather/machinery behavior, and user observation of the final package remain active in `missioncache.md`; focused readback does not mark those missions COMPLETE.

## Dependency snapshot

The complete vendored EpochGui dependency remains the last exact verifiable snapshot, v0.88.75 at `d8decc9ee2e73e0009f1e8c49d86a52db6748b28`. The required release-time GitHub fetch was attempted on 2026-08-16 but returned account-suspension HTTP 403 and the anonymous endpoint returned 404. A local v0.89.02 engine copy contains uncommitted user changes and no standalone upstream commit provenance, so it was not silently imported into this package.

## Stable local assets

- SandHybrid-Windows-x64-v2.5.25.zip
- SandHybrid-Windows-x64-v2.5.25.zip.sha256
- SandHybrid-Linux-x64-v2.5.25.tar.gz
- SandHybrid-Linux-x64-v2.5.25.tar.gz.sha256

This pass is authorized for local packages only. It does not push, tag, publish, or modify a public GitHub release.