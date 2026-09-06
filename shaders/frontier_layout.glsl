#ifndef EPOCHSIMENGINE_FRONTIER_LAYOUT_GLSL
#define EPOCHSIMENGINE_FRONTIER_LAYOUT_GLSL

// Authored Frontier material construction, shared verbatim with the CPU map
// contract. The reset shader supplies the existing aligned brick primitives.
uint frontierBaseMaterial(ivec2 p) {
    ivec2 world = worldBrickSize();
    ivec2 b = brickCoordinate(p);
    int surface = 17;
    int floorRow = world.y - 2;
    uint material = MAT_EMPTY;

    if (b.y >= floorRow) return MAT_STONE;
    if (b.y == surface) material = MAT_GRASS;
    else if (b.y > surface) material = MAT_DIRT;

    // Player bunker aligned around the fixed actor spawn at x=168, y=200.
    ivec2 baseMin = ivec2(6, 20);
    ivec2 baseMax = ivec2(50, 41);
    if (brickFrame(p, baseMin, baseMax, 1)) material = MAT_STEEL;
    else if (brickRect(p, baseMin + ivec2(1), baseMax - ivec2(1))) material = MAT_EMPTY;
    for (int level = 26; level < 41; level += 7)
        if (brickRect(p, ivec2(baseMin.x + 1, level), ivec2(baseMax.x - 1, level + 1))) material = MAT_STEEL;
    if (brickStair(p, ivec2(7, 39), 9, true)) material = MAT_STEEL;

    // Brick waterfall aerator.
    ivec2 waterMin = ivec2(5, 3);
    ivec2 waterMax = ivec2(14, 9);
    if (brickFrame(p, waterMin, waterMax, 1)) material = MAT_GLASS;
    else if (brickRect(p, waterMin + ivec2(1), waterMax - ivec2(1))) material = MAT_WATER;
    if (brickRect(p, ivec2(8, 8), ivec2(10, 22))) material = MAT_WATER;
    if (brickRect(p, ivec2(7, 8), ivec2(8, 23)) || brickRect(p, ivec2(10, 8), ivec2(11, 23)))
        material = MAT_GLASS;
    ivec2 catchMin = ivec2(5, 22);
    ivec2 catchMax = ivec2(15, 27);
    if (brickFrame(p, catchMin, catchMax, 1)) material = MAT_GLASS;
    else if (brickRect(p, catchMin + ivec2(1), catchMax - ivec2(1))) material = MAT_WATER;
    // The downcomer ends immediately above this roof. Preserve the initial
    // Water owners and open two complete inlet tiles into the catchment.
    // Reset turns these explicit Empty owners into the normal Air baseline.
    if (brickRect(p, ivec2(8, 22), ivec2(10, 23))) material = MAT_EMPTY;

    // Unified brick-grid factory floor.
    if (brickRect(p, ivec2(14, 35), ivec2(49, 36))) material = MAT_CONVEYOR;
    if (brickRect(p, ivec2(24, 34), ivec2(25, 35))) material = MAT_FACTORY_CORE;
    if (brickRect(p, ivec2(28, 34), ivec2(29, 35))) material = MAT_SMELTER;
    if (brickRect(p, ivec2(34, 34), ivec2(35, 35))) material = MAT_ASSEMBLER;
    if (brickRect(p, ivec2(41, 34), ivec2(42, 35))) material = MAT_INSECT_HABITAT;
    if (brickRect(p, ivec2(15, 32), ivec2(18, 35))) material = ((b.x + b.y) & 1) == 0 ? MAT_IRON : MAT_GOLD;
    if (brickRect(p, ivec2(31, 32), ivec2(33, 35))) material = MAT_COPPER;
    if (brickRect(p, ivec2(37, 32), ivec2(39, 35))) material = MAT_POWER_CELL;

    // Enemy bunker and factory remain on the same aligned grid.
    ivec2 enemyMin = ivec2(world.x - 21, 20);
    ivec2 enemyMax = ivec2(world.x - 2, 34);
    if (brickFrame(p, enemyMin, enemyMax, 1)) material = MAT_STEEL;
    else if (brickRect(p, enemyMin + ivec2(1), enemyMax - ivec2(1))) material = MAT_EMPTY;
    if (brickRect(p, ivec2(enemyMin.x + 2, 27), ivec2(enemyMin.x + 3, 28))) material = MAT_FACTORY_CORE;
    if (brickRect(p, ivec2(enemyMin.x + 7, 28), ivec2(enemyMin.x + 8, 29))) material = MAT_INSECT_HABITAT;
    if (brickRect(p, ivec2(enemyMin.x + 1, 29), ivec2(enemyMax.x - 1, 30))) material = MAT_CONVEYOR;
    // Keep all six Steel stock tiles above, never over, the Factory Core.
    if (brickRect(p, ivec2(enemyMin.x + 2, 24), ivec2(enemyMin.x + 4, 27))) material = MAT_STEEL;
    if (brickRect(p, ivec2(enemyMin.x + 5, 25), ivec2(enemyMin.x + 7, 28))) material = MAT_COPPER;
    if (brickRect(p, ivec2(enemyMin.x + 8, 25), ivec2(enemyMin.x + 10, 28))) material = MAT_PLASMA_AMMO;

    if (material == MAT_EMPTY && brickRect(p, baseMin + ivec2(1), baseMax - ivec2(1)) && ((b.x + b.y) & 1) == 0)
        material = MAT_ATMOSPHERE;
    if (p.y == surface * BRICK_SIZE - 1 && p.x > 150 && p.x < 250 && (p.x % 31) == 0) material = MAT_ANT;
    if (p.y == surface * BRICK_SIZE - 1 && p.x > AUTHORED_WORLD_CELLS.x - 220 && p.x < AUTHORED_WORLD_CELLS.x - 80 && (p.x % 17) == 0)
        material = MAT_BEETLE;
    return material;
}

#endif
