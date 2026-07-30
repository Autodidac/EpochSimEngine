#ifndef EPOCH_SAND_TILES_GLSL
#define EPOCH_SAND_TILES_GLSL

const uint TILE_SIZE = 8u;
const uint TILE_CELL_COUNT = 64u;
const uint TILE_STABILITY_OCCUPANCY = 52u;
const uint TILE_MIN_COHESIVE_CELLS = 32u;
const uint TILE_COLLAPSE_OCCUPANCY = TILE_MIN_COHESIVE_CELLS;
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
const uint TILE_HAS_QUEEN = 0x00000100u;
const uint TILE_HAS_HIVE = 0x00000200u;
const uint TILE_HAS_FLOWER = 0x00000400u;
const uint TILE_HAS_HONEY = 0x00000800u;
const uint TILE_HAS_BEES = 0x00001000u;
const uint TILE_HAS_MIGRATING_QUEEN = 0x00002000u;
const uint TILE_BEE_HAZARD = 0x00004000u;

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

const uint TILE_OCCUPANCY_MASK = 0x0000007fu;
const uint TILE_QUEEN_X_SHIFT = 7u;
const uint TILE_QUEEN_Y_SHIFT = 10u;
const uint TILE_FLOWER_X_SHIFT = 13u;
const uint TILE_FLOWER_Y_SHIFT = 16u;
const uint TILE_HONEY_X_SHIFT = 19u;
const uint TILE_HONEY_Y_SHIFT = 22u;
const uint TILE_BEE_COUNT_SHIFT = 25u;

uint tileOccupancy(TileState state) { return state.occupancy & TILE_OCCUPANCY_MASK; }
ivec2 tileQueenLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_QUEEN_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_QUEEN_Y_SHIFT) & 7u));
}
ivec2 tileFlowerLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_FLOWER_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_FLOWER_Y_SHIFT) & 7u));
}
ivec2 tileHoneyLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_HONEY_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_HONEY_Y_SHIFT) & 7u));
}
uint tileBeeCount(TileState state) { return (state.occupancy >> TILE_BEE_COUNT_SHIFT) & 127u; }

uint packTileOccupancy(uint occupancy, ivec2 queenLocal, ivec2 flowerLocal,
                       ivec2 honeyLocal, uint beeCount) {
    return min(occupancy, 64u) |
           (uint(clamp(queenLocal.x, 0, 7)) << TILE_QUEEN_X_SHIFT) |
           (uint(clamp(queenLocal.y, 0, 7)) << TILE_QUEEN_Y_SHIFT) |
           (uint(clamp(flowerLocal.x, 0, 7)) << TILE_FLOWER_X_SHIFT) |
           (uint(clamp(flowerLocal.y, 0, 7)) << TILE_FLOWER_Y_SHIFT) |
           (uint(clamp(honeyLocal.x, 0, 7)) << TILE_HONEY_X_SHIFT) |
           (uint(clamp(honeyLocal.y, 0, 7)) << TILE_HONEY_Y_SHIFT) |
           (min(beeCount, 127u) << TILE_BEE_COUNT_SHIFT);
}

ivec2 brickOriginFromIndex(uint index, uint width) {
    uint columns = tileColumns(width);
    return ivec2(int(index % columns), int(index / columns)) * int(TILE_SIZE);
}
ivec2 tileQueenPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileQueenLocal(state);
}
ivec2 tileFlowerPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileFlowerLocal(state);
}
ivec2 tileHoneyPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileHoneyLocal(state);
}

#endif
