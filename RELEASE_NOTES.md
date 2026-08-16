# SandHybrid v2.5.24

Horizontal World, productive macro packets, and photographed Fix29 hive recovery.

## Corrected behavior

- The normal runtime owns one persistent `WORLD` whose eight 640x360 districts run west-to-east in a single horizontal band: Sandbox, Ecosystem, Engineering, Frontier Base, Volcano, Waterworks, Gold Mine, and Demolition. Compact, Standard, and Large buffers are 5120x1440, 7680x1440, and 10240x1440; the district band is centered vertically and never stacked.
- The Volcano is terrain-sunken near the far-right side of its district. It retains the supplied stepped silhouette, broad left lake, short above-ground crater rim, buried Lava chamber/throat, bottom return, and complete structural Stone foundation.
- `AUX_MOVED` is one-fixed-tick ownership. Complete aligned liquid and gas packets move every two 60 Hz simulation ticks and carry their exposure counter through successful exact swaps as well as blocked attempts. After eight due opportunities, a fresh perimeter check permits fine fallback only for an open or incompatible boundary. Compatible/enclosed Air remains tiled.
- Canonical cells remain authoritative and continually requalify into complete 8x8 packets. Macro movement swaps exact 64-cell payloads; it does not replace, create, or delete an element cell.
- Sandbox and Ecosystem use the photographed historical Fix29 hive cell-for-cell: shell `24 <= radius^2 < 88`, chamber `< 24`, centered queen, right exit, canonical seeded Honey/Pollen contents, and nine complete aligned Wood support tiles. One hundred live SandHybrid bees form a denser three-lobed biohazard without overlapping the body or support.
- Bee metadata now addresses the queen's correct district in the 8x1 World. Foraging, pollen, feeding, landing, newborn slots, migration, hazards, and return-to-hive behavior remain active; no SimpleSandSim bee runtime was imported.
- Editor mutations remain live while RUNNING and PAUSED. Inventory/Blueprint and Designer ownership, clear/fill behavior, fixed simulation cadence, Half Water rules, player recovery, machinery, atmosphere, and debug presentation retain their prior accepted contracts.

## Release verification

This source carries deterministic audits for the exact hive body/content counts, 100 unique non-overlapping bee slots and home metadata, compact biohazard geometry, horizontal World address mapping, sunken Volcano tokens, one-tick movement ownership, two consecutive exact macro Water moves, eighth-opportunity fine fallback, and enclosed-Air retention. Native Windows and Linux Release build/test/package results and the one deliberate Vulkan readback are recorded in `MISSION_LEDGER.md`; active long-duration visual/ecology missions remain open unless directly observed.

Run the installed executable manually when a fresh driver shader-cache warmup is acceptable:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

## Dependency snapshot

The complete vendored EpochGui dependency remains v0.88.75 at `d8decc9ee2e73e0009f1e8c49d86a52db6748b28`. The required release-time upstream query was attempted on 2026-08-16, but GitHub returned account-suspension HTTP 403. The available local checkout is older at `d279747`; the newer complete vendor was retained intact instead of being downgraded or partially copied.

## Stable local assets

- SandHybrid-Windows-x64-v2.5.24.zip
- SandHybrid-Windows-x64-v2.5.24.zip.sha256
- SandHybrid-Linux-x64-v2.5.24.tar.gz
- SandHybrid-Linux-x64-v2.5.24.tar.gz.sha256

This pass is authorized for local packages only. It does not push, tag, publish, or modify a public GitHub release.