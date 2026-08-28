# Hierarchical Cell Simulation

The cell buffer remains authoritative. The hierarchy only accelerates work; it never invents, deletes, reconstructs, or replaces represented material.

## 8x8 movable macro-cells

A full aligned 8x8 region is eligible for a bulk move only when all 64 cells:

- contain the same enabled movable material,
- are non-structural,
- can make the same fall, diagonal, density-swap, or liquid-spread move,
- contain no fresh-water half-unit state.

One 8x8 Vulkan workgroup validates one macro-cell pair. When valid, its 64 lanes swap the canonical cells in parallel. Mixed, partial, damaged, reacting, structural, or half-water regions immediately fall back to the normal per-cell passes. This preserves pixel behavior at edges while allowing large uniform bodies to travel eight cells per bulk step. Half Water split/merge remains a fine-cell transaction over equal represented units: both halves retain source Water temperature, and the displaced-medium temperature is packed with its material/volume until deterministic restoration.
The v2.5.3 ownership rule is authoritative for fluids: a complete moving liquid or gas tile remains macro-eligible even at an exposed boundary, while a stationary tile requires a compatible perimeter. Eligibility only schedules an atomic packet attempt. If the destination cannot accept the exact 8x8 transaction, the tile keeps fine ownership and runs canonical per-cell movement in the same tick.

Pause does not change ownership or advance either movement path. Direct editor mutations still update canonical cells and dirty the affected hierarchy while the simulation clock remains frozen.

Clouds, Steam, Smoke, Air, and all other gases follow the same rule: complete 8x8 metadata may accelerate transport, but the 64 underlying cells remain the only material ownership. The continuous high-sky Cloud deck therefore sleeps and wakes by ordinary reversible hierarchy state, spans district gaps, and stays inside the two hard boundary columns. Weather conversion changes exact canonical units rather than replacing a tile payload.

## 64x64 sleeping chunks

Eight macro tiles per axis form a 64x64 scheduling chunk. Each chunk caches active, sleeping, dirty, boundary, and quiet-tick state.

- Empty oxygen atmosphere can sleep.
- A chunk sleeps only after every present 8x8 tile is sleeping for 30 consecutive ticks.
- Painting, actor tools, macro movement, and fine movement atomically dirty affected chunks.
- A dirty chunk is rescanned on the next tick.
- Chemistry skips a chunk only when its complete one-chunk neighborhood is sleeping, preserving boundary reactions.
- Fine movement rejects a pair with one chunk lookup when both endpoints are in sleeping chunks.

The debug view draws 8x8 tile boundaries and stronger 64x64 chunk boundaries. It reports sleeping/active chunks, macro moves, macro cells moved, and cells bypassed by chunk sleeping.
