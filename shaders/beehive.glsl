#ifndef SANDHYBRID_BEEHIVE_GLSL
#define SANDHYBRID_BEEHIVE_GLSL

const int BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 24;
const int BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 88;
const int BEEHIVE_CHAMBER_RADIUS_SQUARED = 24;
const int BEEHIVE_EXIT_MIN_X = 1;
const int BEEHIVE_EXIT_MAX_X = 10;
const int BEEHIVE_EXIT_HALF_HEIGHT = 1;
const int BEEHIVE_SUPPORT_TILE_SIZE = 8;
const int BEEHIVE_SUPPORT_WIDTH = 72;
const int BEEHIVE_SUPPORT_HEIGHT = 8;
const int BEEHIVE_SUPPORT_LEFT_BIAS = 40;
const int BEEHIVE_SUPPORT_TOP_BIAS = 16;
const uint BEEHIVE_CANONICAL_WIDTH = 640u;
const ivec2 BEEHIVE_CANONICAL_SANDBOX_QUEEN = ivec2(512, 234);
const ivec2 BEEHIVE_CANONICAL_ECOSYSTEM_QUEEN = ivec2(512, 232);
// Canonical photographed-map chamber entropy shared by reset, tool, and load.
const uint BEEHIVE_CANONICAL_SEED = 0xD17A5EEDu;

// Exact compact hive body and chamber rule from the photographed historical
// SimpleSandSim Sandbox map. SandHybrid owns the surrounding bee population and behavior.
ivec2 beehiveSupportOrigin(ivec2 queen) {
    return ((queen - ivec2(BEEHIVE_SUPPORT_LEFT_BIAS,
                           BEEHIVE_SUPPORT_TOP_BIAS)) /
            BEEHIVE_SUPPORT_TILE_SIZE) * BEEHIVE_SUPPORT_TILE_SIZE;
}

bool beehiveSupportCell(ivec2 queen, ivec2 offset) {
    ivec2 p = queen + offset;
    ivec2 origin = beehiveSupportOrigin(queen);
    return p.x >= origin.x && p.x < origin.x + BEEHIVE_SUPPORT_WIDTH &&
           p.y >= origin.y && p.y < origin.y + BEEHIVE_SUPPORT_HEIGHT;
}

uint beehivePrefabEntropy(ivec2 offset) {
    ivec2 canonical = BEEHIVE_CANONICAL_ECOSYSTEM_QUEEN + offset;
    uint canonicalIndex = uint(canonical.y) * BEEHIVE_CANONICAL_WIDTH + uint(canonical.x);
    return hash32(canonicalIndex ^ BEEHIVE_CANONICAL_SEED);
}

uint beehivePrefabEntropy(ivec2 queen, ivec2 offset) {
    return beehivePrefabEntropy(offset);
}

// MATERIAL_COUNT means the position belongs to the surrounding SandHybrid swarm.
uint beehivePrefabMaterial(ivec2 queen, ivec2 offset, uint entropy) {
    int radiusSquared = offset.x * offset.x + offset.y * offset.y;
    if (radiusSquared == 0) return MAT_QUEEN_BEE;
    if (offset.x >= BEEHIVE_EXIT_MIN_X && offset.x <= BEEHIVE_EXIT_MAX_X &&
        abs(offset.y) <= BEEHIVE_EXIT_HALF_HEIGHT)
        return MAT_EMPTY;
    if (radiusSquared >= BEEHIVE_SHELL_MIN_RADIUS_SQUARED &&
        radiusSquared < BEEHIVE_SHELL_MAX_RADIUS_SQUARED)
        return MAT_BEEHIVE;
    if (radiusSquared < BEEHIVE_CHAMBER_RADIUS_SQUARED) {
        if ((entropy & 3u) == 0u) return MAT_EMPTY;
        return (entropy & 4u) == 0u ? MAT_HONEY : MAT_POLLEN;
    }
    if (beehiveSupportCell(queen, offset)) return MAT_WOOD;
    return MATERIAL_COUNT;
}

#endif
