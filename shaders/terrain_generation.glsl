#ifndef SANDHYBRID_TERRAIN_GENERATION_GLSL
#define SANDHYBRID_TERRAIN_GENERATION_GLSL

const uint TERRAIN_TILE_SIZE = 8u;
const uint TERRAIN_VEIN_CLUSTER_X = 12u;
const uint TERRAIN_VEIN_CLUSTER_Y = 8u;
const uint TERRAIN_TRAP_CLUSTER_X = 24u;
const uint TERRAIN_TRAP_CLUSTER_Y = 14u;
const uint TERRAIN_FLAG_STRUCTURAL = 1u;
const uint TERRAIN_FLAG_DELIBERATE_LOOSE = 2u;
const uint TERRAIN_FLAG_SAND_TRAP = 4u;

uint terrainHash(uvec2 p, uint salt) {
    uint value = p.x * 0x9e3779b9u ^ p.y * 0x85ebca6bu ^ salt * 0xc2b2ae35u;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

bool terrainHostMaterial(uint material) {
    return material == MAT_SAND || material == MAT_DIRT || material == MAT_SILT ||
           material == MAT_MUD || material == MAT_STONE;
}

uint terrainClusterDeposit(uvec2 cluster, uint depth) {
    uint roll = terrainHash(cluster, 0x51e1u) & 4095u;
    if (depth >= 160u && roll < 18u) return MAT_URANIUM;
    if (roll < 50u) return MAT_ALUMINUM;
    if (roll < 160u) return MAT_COPPER;
    if (roll < 460u) return MAT_IRON_ORE;
    return MAT_EMPTY;
}

bool terrainVeinCoreTile(uvec2 tile, uint depth) {
    uvec2 cluster = tile / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    if (terrainClusterDeposit(cluster, depth) == MAT_EMPTY) return false;
    uint seed = terrainHash(cluster, 0x9b7du);
    ivec2 local = ivec2(tile % uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y));
    ivec2 center = ivec2(3 + int((seed >> 8u) % 6u), 2 + int((seed >> 14u) % 4u));
    ivec2 radius = ivec2(2 + int((seed >> 20u) % 3u), 1 + int((seed >> 24u) % 3u));
    ivec2 delta = local - center;
    int ellipse = delta.x * delta.x * radius.y * radius.y +
                  delta.y * delta.y * radius.x * radius.x;
    int limit = radius.x * radius.x * radius.y * radius.y;
    int rough = int(terrainHash(tile, seed) & 3u) - 1;
    return ellipse <= limit + rough * radius.x;
}

bool terrainLooseInclusion(uint baseMaterial, ivec2 position, uint depth) {
    if (baseMaterial != MAT_SAND) return false;
    uvec2 tile = uvec2(position) / TERRAIN_TILE_SIZE;
    uvec2 cluster = tile / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    if (terrainClusterDeposit(cluster, depth) == MAT_EMPTY || terrainVeinCoreTile(tile, depth)) return false;
    bool touchesCore = false;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            if (ox == 0 && oy == 0) continue;
            ivec2 neighbor = ivec2(tile) + ivec2(ox, oy);
            if (neighbor.x >= 0 && neighbor.y >= 0 && terrainVeinCoreTile(uvec2(neighbor), depth))
                touchesCore = true;
        }
    }
    return touchesCore && terrainHash(uvec2(position), 0x10c5u) % 73u == 0u;
}

bool terrainTrapSelected(uvec2 tile, uint depth) {
    if (depth < 72u || depth > 300u) return false;
    uvec2 cluster = tile / uvec2(TERRAIN_TRAP_CLUSTER_X, TERRAIN_TRAP_CLUSTER_Y);
    return ((cluster.x + cluster.y * 3u) % 11u) == 0u;
}

uvec2 terrainSample(uint baseMaterial, ivec2 position, uint depth) {
    if (!terrainHostMaterial(baseMaterial)) return uvec2(baseMaterial, 0u);
    uvec2 tile = uvec2(position) / TERRAIN_TILE_SIZE;
    if (baseMaterial == MAT_SAND && terrainTrapSelected(tile, depth)) {
        uvec2 local = tile % uvec2(TERRAIN_TRAP_CLUSTER_X, TERRAIN_TRAP_CLUSTER_Y);
        uvec2 cluster = tile / uvec2(TERRAIN_TRAP_CLUSTER_X, TERRAIN_TRAP_CLUSTER_Y);
        uint seed = terrainHash(cluster, 0x7135u);
        uint centerX = 8u + seed % 8u;
        uint roofY = 4u + (seed >> 8u) % 4u;
        bool chamber = local.x + 3u >= centerX && local.x <= centerX + 3u &&
                       local.y > roofY && local.y <= roofY + 3u;
        bool looseRoof = local.x + 2u >= centerX && local.x <= centerX + 2u && local.y == roofY;
        if (chamber) return uvec2(MAT_EMPTY, TERRAIN_FLAG_SAND_TRAP);
        if (looseRoof) return uvec2(MAT_SAND, TERRAIN_FLAG_DELIBERATE_LOOSE | TERRAIN_FLAG_SAND_TRAP);
    }
    uvec2 cluster = tile / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    uint deposit = terrainClusterDeposit(cluster, depth);
    if (deposit != MAT_EMPTY && terrainVeinCoreTile(tile, depth))
        return uvec2(deposit, TERRAIN_FLAG_STRUCTURAL);
    if (deposit != MAT_EMPTY && terrainLooseInclusion(baseMaterial, position, depth))
        return uvec2(deposit, TERRAIN_FLAG_DELIBERATE_LOOSE);
    return uvec2(baseMaterial, TERRAIN_FLAG_STRUCTURAL);
}

#endif
