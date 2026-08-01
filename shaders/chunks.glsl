#ifndef SANDHYBRID_CHUNKS_GLSL
#define SANDHYBRID_CHUNKS_GLSL

const uint CHUNK_TILES_PER_AXIS = 8u;
const uint CHUNK_CELL_SIZE = TILE_SIZE * CHUNK_TILES_PER_AXIS;
const uint CHUNK_TILE_COUNT = CHUNK_TILES_PER_AXIS * CHUNK_TILES_PER_AXIS;
const uint CHUNK_SLEEP_TICKS = 30u;
const int ACTIVE_REGION_WIDTH_CELLS = 640;
const int ACTIVE_REGION_HEIGHT_CELLS = 360;

const uint CHUNK_ACTIVE = 0x00000001u;
const uint CHUNK_SLEEPING = 0x00000002u;
const uint CHUNK_DIRTY = 0x00000004u;
const uint CHUNK_BOUNDARY = 0x00000008u;
const uint CHUNK_MACRO_ACTIVE = 0x00000010u;
const uint CHUNK_FINE_ACTIVE = 0x00000020u;

struct ChunkState {
    uint flags;
    uint activeTiles;
    uint sleepingTiles;
    uint counters; // low 16 quiet ticks, high 16 present tile count
};

uint chunkColumns(uint width) { return (width + CHUNK_CELL_SIZE - 1u) / CHUNK_CELL_SIZE; }
uint chunkRows(uint height) { return (height + CHUNK_CELL_SIZE - 1u) / CHUNK_CELL_SIZE; }
uvec2 chunkCoordinate(ivec2 p) { return uvec2(max(p, ivec2(0))) / CHUNK_CELL_SIZE; }
uint chunkIndex(ivec2 p, uint width) {
    uvec2 chunk = chunkCoordinate(p);
    return chunk.y * chunkColumns(width) + chunk.x;
}
uint chunkQuietTicks(ChunkState state) { return state.counters & 0xffffu; }
uint chunkPresentTiles(ChunkState state) { return state.counters >> 16u; }
uint packChunkCounters(uint quietTicks, uint presentTiles) {
    return min(quietTicks, 0xffffu) | (min(presentTiles, 0xffffu) << 16u);
}
bool chunkHas(ChunkState state, uint flag) { return (state.flags & flag) != 0u; }

int sectionAbs(int value) { return value < 0 ? -value : value; }

bool sectionCoordinateActive(ivec2 candidate, ivec2 center) {
    ivec2 delta = candidate - center;
    int ax = sectionAbs(delta.x);
    int ay = sectionAbs(delta.y);
    int radius = max(ax, ay);
    if (radius == 0 || radius == 1) return true;
    if (radius != 2) return false;
    return delta.x == 0 || delta.y == 0 || ax == ay;
}

bool sectionActiveAt(ivec2 p, int centerX, int centerY, uint enabled) {
    if (enabled == 0u) return true;
    ivec2 clamped = max(p, ivec2(0));
    ivec2 section = ivec2(clamped.x / ACTIVE_REGION_WIDTH_CELLS,
                          clamped.y / ACTIVE_REGION_HEIGHT_CELLS);
    return sectionCoordinateActive(section, ivec2(centerX, centerY));
}

#endif
