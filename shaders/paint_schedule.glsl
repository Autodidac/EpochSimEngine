#ifndef EPOCHSIMENGINE_PAINT_SCHEDULE_GLSL
#define EPOCHSIMENGINE_PAINT_SCHEDULE_GLSL

// Scalar helpers are compiled by both the renderer/CPU contract and GLSL.
// A paint column may write its own vertical shaft and either neighboring
// column through gas displacement. Three global X phases make those complete
// read/write footprints disjoint, even for clipped brushes and translated grids.
const uint PAINT_COLUMN_FIRST_MODE = 6u;
const uint PAINT_COLUMN_PHASES = 3u;

bool paintColumnMode(uint mode) {
    return mode >= PAINT_COLUMN_FIRST_MODE &&
           mode < PAINT_COLUMN_FIRST_MODE + PAINT_COLUMN_PHASES;
}

bool paintColumnOwned(int worldX, uint mode) {
    return worldX >= 0 && paintColumnMode(mode) &&
           uint(worldX) % PAINT_COLUMN_PHASES == mode - PAINT_COLUMN_FIRST_MODE;
}

#endif
