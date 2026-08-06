#ifndef SANDHYBRID_BEEHIVE_GLSL
#define SANDHYBRID_BEEHIVE_GLSL

const int BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 25;
const int BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 92;
const int BEEHIVE_CHAMBER_RADIUS_SQUARED = 25;
const int BEEHIVE_EXIT_MIN_X = 1;
const int BEEHIVE_EXIT_MAX_X = 10;
const int BEEHIVE_EXIT_HALF_HEIGHT = 1;
const int BEEHIVE_SUPPORT_MIN_X = -37;
const int BEEHIVE_SUPPORT_MAX_X = 29;
const int BEEHIVE_SUPPORT_MIN_Y = -16;
const int BEEHIVE_SUPPORT_MAX_Y = -13;

// Exact suspended Ecosystem hive restored by SimpleSandSim Fix36 from the hard-coded map
// immediately before PR #19. SandHybrid owns the surrounding bee population and behavior.
bool beehiveSupportOffset(ivec2 offset) {
    return offset.x >= BEEHIVE_SUPPORT_MIN_X && offset.x <= BEEHIVE_SUPPORT_MAX_X &&
           offset.y >= BEEHIVE_SUPPORT_MIN_Y && offset.y <= BEEHIVE_SUPPORT_MAX_Y;
}

// MATERIAL_COUNT means the position belongs to the surrounding SandHybrid swarm.
uint beehivePrefabMaterial(ivec2 offset, uint entropy) {
    if (beehiveSupportOffset(offset)) return MAT_WOOD;
    int radiusSquared = offset.x * offset.x + offset.y * offset.y;
    if (radiusSquared == 0) return MAT_QUEEN_BEE;
    if (offset.x >= BEEHIVE_EXIT_MIN_X && offset.x <= BEEHIVE_EXIT_MAX_X &&
        abs(offset.y) <= BEEHIVE_EXIT_HALF_HEIGHT)
        return MAT_EMPTY;
    if (radiusSquared < BEEHIVE_CHAMBER_RADIUS_SQUARED) {
        if ((entropy & 3u) == 0u) return MAT_EMPTY;
        return ((entropy >> 2u) & 1u) == 0u ? MAT_HONEY : MAT_POLLEN;
    }
    if (radiusSquared >= BEEHIVE_SHELL_MIN_RADIUS_SQUARED &&
        radiusSquared < BEEHIVE_SHELL_MAX_RADIUS_SQUARED)
        return MAT_BEEHIVE;
    return MATERIAL_COUNT;
}

#endif
