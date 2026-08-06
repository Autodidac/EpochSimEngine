# SandHybrid v2.5.15

Stable runtime recovery for macro-tile motion, Half Water, the exact suspended Beehive, and world-cursor editing.

## Recovered behavior

- Full uniform compatible liquid and gas targets remain eligible for an exact 8x8 macro displacement even when boundary scheduling marks them fine-active. Mixed, partial, structural, incompatible, or already-moved targets still reject the transaction and use same-tick fine fallback.
- Intact macro packets stay out of the ordinary fine pass unless either side genuinely needs fine fallback.
- Half Water falls first, merges with adjacent halves, attracts only across a clear two-to-four-cell gap, cannot use generic full-Water diagonal wandering, and cannot sleep with fall, merge, attraction, reaction, heat, or motion pending.
- Sandbox, Ecosystem, the Beehive tool, and loaded-map normalization now share the exact hard-coded Ecosystem hive restored by SimpleSandSim Fix36 from immediately before PR #19: queen centers (512,234)/(512,232), loose Wood perch x=-37..29 and y=-16..-13, shell 25 <= radius squared < 92, chamber radius squared < 25, queen center, deterministic Empty/Honey/Pollen contents, and exit x=1..10 with absolute y <= 1.
- The visible world cursor now shares the committed edit position, effective radius, and shape, keeps logical pointer geometry distinct from physical framebuffer presentation during resize and high-DPI display, and disappears instead of clamping to the viewport edge while the pointer is in the sidebar.
- FILL is a one-shot armed tool: its sidebar control does not mutate the world, and the next confirmed world left-click supplies the connected region. Holding F and left-clicking remains the direct shortcut.
- PAUSED continues to freeze simulation, actors, clocks, MAP refresh, lighting/day-night, particles, and effects while direct Editor mutations remain live without advancing simulation time.

## Retained systems

- SandHybrid retains its own bee actors, forage, return, deposit, feed, migration, hazard response, authored-home metadata, and 100-bee colony policy. No SimpleSandSim bee population or runtime behavior is imported.
- Inventory remains sidebar-only with INVENTORY and BLUEPRINTS subtabs.
- Designer remains sidebar-only with its isolated authoring grid, independent cursor state, and INVENTORY and BLUEPRINTS subtabs; it never replaces the world viewport.

## Release validation

The exact release source must pass the complete shader/interface suite, deterministic C++23 contracts, native Windows and Linux Release builds and tests, install/package/archive audits, and SHA-256 generation. The stable tag workflow publishes exactly these four assets:

- SandHybrid-Windows-x64-v2.5.15.zip
- SandHybrid-Windows-x64-v2.5.15.zip.sha256
- SandHybrid-Linux-x64-v2.5.15.tar.gz
- SandHybrid-Linux-x64-v2.5.15.tar.gz.sha256

Runtime and visual missions remain active in missioncache.md until the packaged executables are observed; static validation alone does not close them.
