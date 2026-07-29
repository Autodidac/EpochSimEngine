#ifndef EPOCH_SAND_TILES_GLSL
#define EPOCH_SAND_TILES_GLSL

const uint TILE_SIZE = 8u;
const uint TILE_CELL_COUNT = 64u;
const uint TILE_STABILITY_OCCUPANCY = 52u;
const uint TILE_COLLAPSE_OCCUPANCY = 32u;
const uint TILE_STABILIZE_TICKS = 120u;
const uint TILE_RESTABILIZE_COOLDOWN = 240u;

const uint TILE_STRUCTURAL = 0x00000001u;
const uint TILE_SUPPORTED = 0x00000002u;
const uint TILE_SLEEPING = 0x00000004u;
const uint TILE_ACTIVE = 0x00000008u;
const uint TILE_CANDIDATE = 0x00000010u;
const uint TILE_STABLE = 0x00000020u;
const uint TILE_COLLAPSING = 0x00000040u;
const uint TILE_DAMAGED = 0x00000080u;

struct TileState {
    uint material;
    uint occupancy;
    uint flags;
    uint counters; // low 16: stability ticks, high 16: restabilization cooldown
};

uint tileColumns(uint width) { return (width + TILE_SIZE - 1u) / TILE_SIZE; }
uvec2 tileCoordinate(ivec2 p) { return uvec2(max(p, ivec2(0))) / TILE_SIZE; }
uint tileIndex(ivec2 p, uint width) {
    uvec2 tile = tileCoordinate(p);
    return tile.y * tileColumns(width) + tile.x;
}
uint tileStableTicks(TileState state) { return state.counters & 0xffffu; }
uint tileCooldown(TileState state) { return state.counters >> 16u; }
uint packTileCounters(uint stableTicks, uint cooldown) {
    return min(stableTicks, 0xffffu) | (min(cooldown, 0xffffu) << 16u);
}
bool tileHas(TileState state, uint flag) { return (state.flags & flag) != 0u; }

#endif
