# Hierarchical Cell Simulation

The cell buffer remains authoritative. The hierarchy only accelerates work; it never invents, deletes, reconstructs, or replaces represented material.

## 8x8 movable macro-cells

A full aligned 8x8 region is eligible for a bulk move only when all 64 cells:

- contain the same enabled movable material,
- are non-structural,
- can make the same fall, diagonal, density-swap, or liquid-spread move,
- contain no fresh-water half-unit state.

One 8x8 Vulkan workgroup validates one macro-cell pair. When valid, its 64 lanes swap the canonical cells in parallel. Mixed, partial, damaged, reacting, structural, or half-water regions immediately fall back to the normal per-cell passes. This preserves pixel behavior at edges while allowing large uniform bodies to travel eight cells per bulk step.

## 64x64 sleeping chunks

Eight macro tiles per axis form a 64x64 scheduling chunk. Each chunk caches active, sleeping, dirty, boundary, and quiet-tick state.

- Empty oxygen atmosphere can sleep.
- A chunk sleeps only after every present 8x8 tile is sleeping for 30 consecutive ticks.
- Painting, actor tools, macro movement, and fine movement atomically dirty affected chunks.
- A dirty chunk is rescanned on the next tick.
- Chemistry skips a chunk only when its complete one-chunk neighborhood is sleeping, preserving boundary reactions.
- Fine movement rejects a pair with one chunk lookup when both endpoints are in sleeping chunks.

The debug view draws 8x8 tile boundaries and stronger 64x64 chunk boundaries. It reports sleeping/active chunks, macro moves, macro cells moved, and cells bypassed by chunk sleeping.
