#ifndef SANDHYBRID_BEEHIVE_GLSL
#define SANDHYBRID_BEEHIVE_GLSL

const int BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 28;
const int BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 108;
const int BEEHIVE_CHAMBER_RADIUS_SQUARED = 28;
const int BEEHIVE_EXIT_MIN_X = 1;
const int BEEHIVE_EXIT_MAX_X = 12;
const int BEEHIVE_EXIT_HALF_HEIGHT = 1;

// Restored suspended hive geometry from the early SandHybrid ecology scene. MATERIAL_COUNT means
// the position belongs to the surrounding swarm rather than the hive body.
uint beehivePrefabMaterial(ivec2 offset, uint entropy) {
    int radiusSquared = offset.x * offset.x + offset.y * offset.y;
    if (radiusSquared == 0) return MAT_QUEEN_BEE;
    if (offset.x >= BEEHIVE_EXIT_MIN_X && offset.x <= BEEHIVE_EXIT_MAX_X &&
        abs(offset.y) <= BEEHIVE_EXIT_HALF_HEIGHT)
        return MAT_EMPTY;
    if (radiusSquared < BEEHIVE_CHAMBER_RADIUS_SQUARED) {
        if ((entropy & 3u) == 0u) return MAT_EMPTY;
        return (entropy & 4u) == 0u ? MAT_HONEY : MAT_POLLEN;
    }
    if (radiusSquared >= BEEHIVE_SHELL_MIN_RADIUS_SQUARED &&
        radiusSquared < BEEHIVE_SHELL_MAX_RADIUS_SQUARED)
        return MAT_BEEHIVE;
    return MATERIAL_COUNT;
}

#endif
