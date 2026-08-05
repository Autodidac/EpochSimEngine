# SandHybrid v2.5.14

Stable recovery release for macro-tile motion, the Ecosystem beehive, paused editing, and sidebar workspace ownership.

## Recovered behavior

- Restores the v2.5.3 macro policy: complete moving Water and gas tiles receive an 8x8 packet attempt, while blocked or incompatible packets retain same-tick fine-cell fallback.
- Restores only the SimpleSandSim Fix29 beehive body and deterministic chamber contents: the 28-108 shell, chamber, queen center, Honey/Pollen/Empty contents, and 12-cell right exit. SandHybrid ecology and actor behavior remain unchanged.
- `PAUSED` still freezes simulation ticks, actors, clocks, MAP refresh, lighting/day-night, particles, and presentation effects, but direct editing remains live without advancing the simulation.
- Inventory lives in the sidebar with `INVENTORY` and `BLUEPRINTS` subtabs. The Inventory pane shows and selects the player?s Iron, Gold, Copper, and Aluminum resources.
- Designer lives entirely in the sidebar with its isolated 64x32 grid and `INVENTORY` / `BLUEPRINTS` subtabs; selecting Designer no longer replaces the world viewport.

## Retained systems

- Half Water keeps canonical balanced-Air bookkeeping, visible two-to-four-cell attraction, fine ownership, and bounded rest conditions.
- Packed atmosphere, actor-owned ecology, machinery transactions, continuous terrain, exact saves, and independent MAP navigation remain covered by deterministic contracts.
- SandHybrid?s bee forage, return, deposit, feed, migration, hazard, and 100-bee colony policy are retained; no SimpleSandSim bee simulation was imported.

## Release validation

The release source must pass the complete shader/interface suite, deterministic C++23 contracts, Windows and Linux Release builds, tests, install steps, archives, and SHA-256 generation. The tag-gated workflow publishes these four assets:

- `SandHybrid-Windows-x64-v2.5.14.zip`
- `SandHybrid-Windows-x64-v2.5.14.zip.sha256`
- `SandHybrid-Linux-x64-v2.5.14.tar.gz`
- `SandHybrid-Linux-x64-v2.5.14.tar.gz.sha256`

Local Windows/MSVC and native Linux/GNU Release builds compiled all 12 shaders and passed all 24 CTest targets before publication. The tag workflow repeats those builds and produces the published checksum files.

Runtime and visual missions remain active in `missioncache.md` until the packaged executables are observed; static validation alone does not close them.
