#ifndef SANDHYBRID_TERRAIN_GENERATION_GLSL
#define SANDHYBRID_TERRAIN_GENERATION_GLSL

const uint TERRAIN_TILE_SIZE = 8u;
const uint TERRAIN_VEIN_CLUSTER_X = 20u;
const uint TERRAIN_VEIN_CLUSTER_Y = 12u;
const uint TERRAIN_TRAP_CLUSTER_X = 28u;
const uint TERRAIN_TRAP_CLUSTER_Y = 16u;
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
    if (depth >= 160u && (terrainHash(cluster, 0x711du) & 4095u) < 100u)
        return MAT_URANIUM;
    if ((terrainHash(cluster, 0xa16fu) & 4095u) < 220u) return MAT_ALUMINUM;
    if ((terrainHash(cluster, 0xc077u) & 4095u) < 420u) return MAT_COPPER;
    if ((terrainHash(cluster, 0x1a0fu) & 4095u) < 900u) return MAT_IRON_ORE;
    return MAT_EMPTY;
}

bool terrainVeinCoreTile(uvec2 tile, uint depth) {
    uvec2 cluster = tile / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    if (terrainClusterDeposit(cluster, depth) == MAT_EMPTY) return false;
    uint seed = terrainHash(cluster, 0x9b7du);
    ivec2 local = ivec2(tile % uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y));
    int centerX = 6 + int((seed >> 7u) % 8u);
    int centerY = 3 + int((seed >> 13u) % 6u);
    int slope = int((seed >> 19u) % 5u) - 2;
    int length = 6 + int((seed >> 23u) % 4u);
    int width = 1 + int((seed >> 27u) % 2u);
    bool vertical = (seed & 1u) != 0u;
    int along;
    int cross;
    if (vertical) {
        int bend = int(terrainHash(
  uvec2(cluster.x + uint(local.y / 3 + 1), cluster.y), 0x52b1u) % 3u) - 1;
        int lineX = centerX + ((local.y - centerY) * slope) / 5 + bend;
        along = abs(local.y - centerY);
        cross = abs(local.x - lineX);
    } else {
        int bend = int(terrainHash(
  uvec2(cluster.x, cluster.y + uint(local.x / 3 + 1)), 0x52b1u) % 3u) - 1;
        int lineY = centerY + ((local.x - centerX) * slope) / 5 + bend;
        along = abs(local.x - centerX);
        cross = abs(local.y - lineY);
    }
    int edgeNoise = int(terrainHash(tile, seed) % 3u) - 1;
    return along <= length && cross <= width + edgeNoise;
}

bool terrainVeinCoreTileSigned(ivec2 tile, uint depth) {
    return all(greaterThanEqual(tile, ivec2(0))) && terrainVeinCoreTile(uvec2(tile), depth);
}

uint terrainNeighboringVeinMaterial(uvec2 tile, uint depth) {
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
  if (offsetX == 0 && offsetY == 0) continue;
  ivec2 neighbor = ivec2(tile) + ivec2(offsetX, offsetY);
  if (!terrainVeinCoreTileSigned(neighbor, depth)) continue;
  return terrainClusterDeposit(
      uvec2(neighbor) / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y), depth);
        }
    }
    return MAT_EMPTY;
}

bool terrainRubblePocketSelected(uvec2 tile) {
    return terrainHash(tile, 0xb04du) % 16u == 0u;
}

uint terrainRubbleDistance(ivec2 position, uvec2 tile) {
    uint seed = terrainHash(tile, 0x8a21u);
    ivec2 center = ivec2(2 + int((seed >> 5u) % 4u),
               2 + int((seed >> 11u) % 4u));
    ivec2 local = ivec2(uint(position.x) % TERRAIN_TILE_SIZE,
              uint(position.y) % TERRAIN_TILE_SIZE);
    ivec2 delta = local - center;
    return uint(delta.x * delta.x * 3 + delta.y * delta.y * 4);
}

uint terrainTrapResource(uvec2 cluster, uint depth) {
    uint roll = terrainHash(cluster, 0x3a71u) & 1023u;
    if (depth >= 160u && roll < 38u) return MAT_URANIUM;
    if (roll < 220u) return MAT_COPPER;
    if (roll < 360u) return MAT_ALUMINUM;
    return MAT_IRON_ORE;
}

bool terrainTrapSelected(uvec2 cluster, uint depth) {
    return depth >= 72u && depth <= 320u &&
           ((terrainHash(cluster, 0x6f31u) & 1023u) < 160u ||
            ((cluster.x + cluster.y * 3u) % 17u) == 0u);
}

uvec2 terrainSample(uint baseMaterial, ivec2 position, uint depth) {
    if (!terrainHostMaterial(baseMaterial)) return uvec2(baseMaterial, 0u);

    uvec2 coreTile = uvec2(position) / TERRAIN_TILE_SIZE;
    uint coreDepth = (depth / TERRAIN_TILE_SIZE) * TERRAIN_TILE_SIZE;
    uvec2 coreCluster = coreTile /
        uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    uint coreDeposit = terrainClusterDeposit(coreCluster, coreDepth);
    if (coreDeposit != MAT_EMPTY && terrainVeinCoreTile(coreTile, coreDepth))
        return uvec2(coreDeposit, TERRAIN_FLAG_STRUCTURAL);

    const uint trapWidthCells = TERRAIN_TRAP_CLUSTER_X * TERRAIN_TILE_SIZE;
    const uint trapHeightCells = TERRAIN_TRAP_CLUSTER_Y * TERRAIN_TILE_SIZE;
    uvec2 trapCluster = uvec2(position) / uvec2(trapWidthCells, trapHeightCells);
    if (baseMaterial == MAT_SAND && terrainTrapSelected(trapCluster, depth)) {
        uint seed = terrainHash(trapCluster, 0x7135u);
        ivec2 local = ivec2(uint(position.x) % trapWidthCells,
                  uint(position.y) % trapHeightCells);
        int centerX = 72 + int(seed % 80u);
        int centerY = 48 + int((seed >> 8u) % 28u);
        int radiusX = 28 + int((seed >> 15u) % 18u);
        int radiusY = 16 + int((seed >> 21u) % 10u);
        int dx = local.x - centerX;
        int dy = local.y - centerY;
        int ellipse = dx * dx * radiusY * radiusY + dy * dy * radiusX * radiusX;
        int limit = radiusX * radiusX * radiusY * radiusY;
        if (ellipse <= limit) return uvec2(MAT_EMPTY, TERRAIN_FLAG_SAND_TRAP);
        int roofY = centerY - radiusY + (dx * dx * 5) / (radiusX * radiusX);
        bool looseRoof = abs(dx) <= radiusX && local.y >= roofY - 4 && local.y <= roofY + 3;
        if (looseRoof) {
  bool concentrated = abs(dx) * 3 < radiusX * 2;
  bool resourceCell = concentrated && terrainHash(uvec2(position), 0x10c5u) % 11u == 0u;
  uint material = resourceCell ? terrainTrapResource(trapCluster, depth) : MAT_SAND;
  return uvec2(material, TERRAIN_FLAG_DELIBERATE_LOOSE | TERRAIN_FLAG_SAND_TRAP);
        }
    }

    uvec2 tile = uvec2(position) / TERRAIN_TILE_SIZE;
    uint tileDepth = (depth / TERRAIN_TILE_SIZE) * TERRAIN_TILE_SIZE;
    uvec2 cluster = tile / uvec2(TERRAIN_VEIN_CLUSTER_X, TERRAIN_VEIN_CLUSTER_Y);
    uint deposit = terrainClusterDeposit(cluster, tileDepth);
    if (deposit != MAT_EMPTY && terrainVeinCoreTile(tile, tileDepth))
        return uvec2(deposit, TERRAIN_FLAG_STRUCTURAL);

    uint neighboringDeposit = terrainNeighboringVeinMaterial(tile, tileDepth);
    if ((baseMaterial == MAT_SAND || baseMaterial == MAT_SILT) &&
        neighboringDeposit != MAT_EMPTY && terrainRubblePocketSelected(tile)) {
        uint distance = terrainRubbleDistance(position, tile);
        if (distance <= 11u) {
  bool resourceCell = terrainHash(uvec2(position), 0x441du) % 5u == 0u;
  return uvec2(resourceCell ? neighboringDeposit : MAT_EMPTY,
               TERRAIN_FLAG_DELIBERATE_LOOSE);
        }
        if (distance <= 28u)
  return uvec2(baseMaterial, TERRAIN_FLAG_DELIBERATE_LOOSE);
    }

    return uvec2(baseMaterial, TERRAIN_FLAG_STRUCTURAL);
}

#endif
