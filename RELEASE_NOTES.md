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
- Full Water now keeps one renderer identity and cannot cascade into Half Water from a deep reservoir; only a terminal two-cell supplied ledge creates the conserved hang/drip pair.
- Waterworks now owns reachable aligned sky Clouds and a conserved Steam riser, and Inventory starts with actor weapon ownership while Editor remains available for independent live paint.

## Release verification

The native Windows Large Vulkan acceptance report proves exact placed hive content both immediately and after 120 ticks, exact Sandbox/Ecosystem hives and 100 unique bee homes, common Y `1040` terrain for all eight districts, three-district startup footprint, healthy player spawn `(4272,1111)`, consecutive macro Water movement, gas movement, enclosed-Air tiling, productive eighth-opportunity fallback, zero-jitter full Water, and conserved Half Water fall/merge/drip cases.

The corrected native tree passes Windows Release 40/40 tests and Linux Release 32/32; standalone EpochGui v0.89.27 passes 10/10 upstream Windows tests. Rebuilt Large Windows/RTX 5080 and Linux/llvmpipe executables each pass all 45 production Vulkan checks. Install, package-tree, archive, and checksum evidence is recorded in `MISSION_LEDGER.md`. Broad visual traversal, sustained ecology/weather/machinery behavior, measured frame time, and user observation of the final package remain active in `missioncache.md`; focused readback does not mark those missions COMPLETE.

## Dependency snapshot

The complete vendored EpochGui dependency is synchronized from the canonical GitHub-independent Site mirror to v0.89.27 at `b23f283dd9b0d6021dccd8fbc2235306418aa66a`. The immutable 96,950-byte source archive verifies SHA-256 `842f9e6372a9742b1a0eaf72b4ac456a0d1f0a596888cac0b0dbbccbca3e01a0`; every non-CMake source file matches byte-for-byte, and the documented CMake integration is the only delta.

## Current corrective local assets

- SandHybrid-Windows-x64-current.zip — 3,328,226 bytes — SHA-256 8d35eb2698f3a8347a0e098ff087ed02d7cc6e8d2e1965fd2a17e8a52bd43511
- SandHybrid-Linux-x64-current.tar.gz — 3,095,360 bytes — SHA-256 79c3cd23ffaf98cb4958616621cdc5becc51f8681a339817751d19a90f8f0e1d

Both sibling checksum files verify. These mutable local assets supersede the rejected earlier current packages; the immutable v2.5.25 archives remain untouched.
## Stable local assets

- SandHybrid-Windows-x64-v2.5.25.zip
- SandHybrid-Windows-x64-v2.5.25.zip.sha256
- SandHybrid-Linux-x64-v2.5.25.tar.gz
- SandHybrid-Linux-x64-v2.5.25.tar.gz.sha256

This pass is authorized for local packages only. It does not push, tag, publish, or modify a public GitHub release.