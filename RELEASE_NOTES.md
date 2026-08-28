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
- Waterworks now owns 52 complete aligned Cloud tiles at world Y `520..559` in the middle of the next camera region up. Its open catchment, powered boiler, Steam riser, mature Cloud storage, and one-for-one rain form the actual conserved apparatus loop. Inventory starts with actor weapon ownership while Editor remains available for independent live paint.
- The persistent player is one shared 9x23-cell body, just under three complete tiles tall; collision, support, recovery, breathing, medium displacement, rendering, pickup exclusion, and laser origin all derive from those dimensions.

## Release verification

The native Windows and Linux Large Vulkan acceptance reports prove exact placed hive content both immediately and after 120 ticks, exact Sandbox/Ecosystem hives and 100 unique bee homes, common Y `1040` terrain for all eight districts, three-district startup footprint, a clear/supported/breathable 207-cell player body at `(4272,1111)`, consecutive macro Water movement, gas movement, enclosed-Air tiling, productive eighth-opportunity fallback, zero-jitter full Water, and conserved Half Water fall/merge/drip cases.

The corrected native tree passes Windows Release 40/40 tests and Linux Release 32/32; standalone EpochGui v0.89.27 passes 10/10 upstream Windows tests. Rebuilt Large Windows/RTX 5080 and Linux/llvmpipe executables each pass all 46 production Vulkan checks. Install, package-tree, archive, and checksum evidence is recorded in `MISSION_LEDGER.md`. Broad visual traversal, sustained ecology/weather/machinery behavior, measured frame time, and user observation of the final package remain active in `missioncache.md`; focused readback does not mark those missions COMPLETE.

## Dependency snapshot

The complete vendored EpochGui dependency is synchronized from the canonical GitHub-independent Site mirror to v0.89.27 at `b23f283dd9b0d6021dccd8fbc2235306418aa66a`. The immutable 96,950-byte source archive verifies SHA-256 `842f9e6372a9742b1a0eaf72b4ac456a0d1f0a596888cac0b0dbbccbca3e01a0`; every non-CMake source file matches byte-for-byte, and the documented CMake integration is the only delta.

## Stable release assets

- SandHybrid-Windows-x64-current.zip
- SandHybrid-Windows-x64-current.zip.sha256
- SandHybrid-Linux-x64-current.tar.gz
- SandHybrid-Linux-x64-current.tar.gz.sha256
- SandHybrid-Windows-x64-v2.5.25.zip
- SandHybrid-Windows-x64-v2.5.25.zip.sha256
- SandHybrid-Linux-x64-v2.5.25.tar.gz
- SandHybrid-Linux-x64-v2.5.25.tar.gz.sha256
- simengine-source.tar.gz
- simengine-source.tar.gz.sha256

Exact byte sizes and SHA-256 values are published beside the finalized hosted artifacts after both native package/runtime gates pass. The normal stable Site release and committed-tree source update are authorized; GitHub tags, releases, and repository state remain untouched.