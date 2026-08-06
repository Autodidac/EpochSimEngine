# SandHybrid v2.5.16

Stable sidebar Blueprint and paused-editing recovery on top of the v2.5.15 macro-tile, Half Water, suspended Beehive, cursor, and Fill fixes.

## Corrected behavior

- Paused Editor painting is no longer suppressed merely because the selected scene contains a player or was in mining mode. Simulation, actors, clocks, lighting, MAP refresh, and effects remain frozen.
- Inventory and Designer now share four real Blueprint slots. Occupied, empty, and selected state is rendered honestly in both sidebar-only workspaces.
- The Designer can publish a trimmed named Static Model or a full-grid Map Chunk into a selected slot without replacing or mutating the main world viewport.
- Selecting an occupied Inventory Blueprint activates a camera-correct world footprint preview. Invalid boundary placement is visibly rejected.
- World placement validates the entire transformed footprint before mutation. Static models place canonical material cells while preserving transparent empty space; Map Chunks can preserve exact empty and non-empty cell state.
- Blueprint paste is live while paused, writes both resident cell buffers transactionally, and is discarded across load/reset boundaries instead of replaying later.
- FILL, painting, mining, and resource deposit cannot fire on the same click as an active Blueprint paste.

## Preserved recovery baselines

- Complete moving liquid and gas tiles retain the packaged v2.5.3 macro-first path with same-tick fine fallback.
- Half Water retains its fine-owned fall, merge, clear-gap attraction, no generic diagonal wandering, and no premature sleep behavior.
- Sandbox, Ecosystem, the Beehive tool, and loaded-map normalization retain the exact hard-coded pre-PR19 Fix36 hive body and contents without importing SimpleSandSim bee behavior.
- World cursor input remains logical-window based and distinct from physical framebuffer scaling.
- Inventory and Designer remain sidebar-only and each owns exactly `INVENTORY` and `BLUEPRINTS` subtabs.

## Validation and active work

The release source must pass the complete shader/interface suite, 26 deterministic C++23 contracts, native Windows and Linux Release builds/tests, fresh installs, archives, content audits, and SHA-256 generation. Runtime frames must show PAUSED state, the world still visible beside Inventory/Designer, real occupied/empty Blueprint slots, and successful transactional paused placement.

`missioncache.md` remains authoritative. Selection marquee operations, exact copied-world Map Chunks, thumbnails, Blueprint persistence, repeated pause/reset stress, and every other unaccepted mission remain active rather than being reported complete from this tranche.

The stable tag workflow publishes exactly these four assets:

- SandHybrid-Windows-x64-v2.5.16.zip
- SandHybrid-Windows-x64-v2.5.16.zip.sha256
- SandHybrid-Linux-x64-v2.5.16.tar.gz
- SandHybrid-Linux-x64-v2.5.16.tar.gz.sha256