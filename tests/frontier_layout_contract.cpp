#include <array>
#include <cstdint>
#include <iostream>

namespace authored {
using uint = std::uint32_t;
#include "material_ids.glsl"

struct ivec2 {
    int x;
    int y;
    constexpr explicit ivec2(int both) : x(both), y(both) {}
    constexpr ivec2(int first, int second) : x(first), y(second) {}
};
constexpr ivec2 operator+(ivec2 a, ivec2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr ivec2 operator-(ivec2 a, ivec2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr int BRICK_SIZE = 8;
constexpr ivec2 AUTHORED_WORLD_CELLS{640, 360};
constexpr ivec2 worldBrickSize() { return {80, 45}; }
constexpr ivec2 brickCoordinate(ivec2 p) { return {p.x / 8, p.y / 8}; }

// Independent scalar adapters for the existing standard half-open brick
// primitives. The complete production Frontier constructor is included below.
bool brickRect(ivec2 p, ivec2 minimum, ivec2 maximum) {
    return p.x >= minimum.x * 8 && p.x < maximum.x * 8 &&
           p.y >= minimum.y * 8 && p.y < maximum.y * 8;
}
bool brickFrame(ivec2 p, ivec2 minimum, ivec2 maximum, int thickness) {
    return brickRect(p, minimum, maximum) &&
           !brickRect(p, minimum + ivec2(thickness), maximum - ivec2(thickness));
}
bool brickStair(ivec2 p, ivec2 start, int count, bool rising_right) {
    const auto brick = brickCoordinate(p);
    const int x = brick.x - start.x;
    if (x < 0 || x >= count) return false;
    return brick.y == (rising_right ? start.y - x : start.y + x);
}

#include "frontier_layout.glsl"

// Exact pre-fix constructor from 79a43aa, retained as an independent frozen
// ordering oracle. Do not update it to bless a new layout or current output.
uint oldFrontierMaterial(ivec2 p) {
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
    if (brickRect(p, ivec2(enemyMin.x + 2, 25), ivec2(enemyMin.x + 4, 28))) material = MAT_STEEL;
    if (brickRect(p, ivec2(enemyMin.x + 5, 25), ivec2(enemyMin.x + 7, 28))) material = MAT_COPPER;
    if (brickRect(p, ivec2(enemyMin.x + 8, 25), ivec2(enemyMin.x + 10, 28))) material = MAT_PLASMA_AMMO;

    if (material == MAT_EMPTY && brickRect(p, baseMin + ivec2(1), baseMax - ivec2(1)) && ((b.x + b.y) & 1) == 0)
        material = MAT_ATMOSPHERE;
    if (p.y == surface * BRICK_SIZE - 1 && p.x > 150 && p.x < 250 && (p.x % 31) == 0) material = MAT_ANT;
    if (p.y == surface * BRICK_SIZE - 1 && p.x > AUTHORED_WORLD_CELLS.x - 220 && p.x < AUTHORED_WORLD_CELLS.x - 80 && (p.x % 17) == 0)
        material = MAT_BEETLE;
    return material;
}

uint normalized(uint material) {
    // Production reset finalizes explicit Empty interiors as baseline Air.
    // This is a material-only authored-map test, not gas payload/runtime proof.
    return material == MAT_EMPTY ? MAT_ATMOSPHERE : material;
}
uint current(ivec2 p) { return normalized(frontierBaseMaterial(p)); }
uint previous(ivec2 p) { return normalized(oldFrontierMaterial(p)); }

bool check_changed_cell(ivec2 p, uint before, uint after, std::uint32_t& changes) {
    uint expected_before = before;
    uint expected_after = before;
    // Exact six-tile difference set, expressed directly in cell coordinates
    // rather than sharing the constructor's brick rectangles.
    if (p.x >= 64 && p.x < 80 && p.y >= 176 && p.y < 184) {
        expected_before = MAT_GLASS;
        expected_after = MAT_ATMOSPHERE;
    } else if (p.x >= 488 && p.x < 504 && p.y >= 192 && p.y < 200) {
        expected_before = MAT_ATMOSPHERE;
        expected_after = MAT_STEEL;
    } else if (p.x >= 488 && p.x < 496 && p.y >= 216 && p.y < 224) {
        expected_before = MAT_STEEL;
        expected_after = MAT_FACTORY_CORE;
    } else if (p.x >= 496 && p.x < 504 && p.y >= 216 && p.y < 224) {
        expected_before = MAT_STEEL;
        expected_after = MAT_ATMOSPHERE;
    }
    if (before != expected_before || after != expected_after) {
        std::cerr << "Unexpected Frontier cell " << p.x << ',' << p.y
                  << " old/new=" << before << '/' << after << " expected="
                  << expected_before << '/' << expected_after << '\n';
        return false;
    }
    changes += before != after ? 1u : 0u;
    return true;
}
}

int main() {
    using namespace authored;
    std::array<std::uint32_t, MATERIAL_COUNT> old_counts{}, new_counts{};
    std::uint32_t changes{};
    for (int y = 0; y < 360; ++y) {
        for (int x = 0; x < 640; ++x) {
            const auto before = previous({x, y});
            const auto after = current({x, y});
            if (!check_changed_cell({x, y}, before, after, changes)) return 1;
            if (before >= MATERIAL_COUNT || after >= MATERIAL_COUNT) return 2;
            ++old_counts[before];
            ++new_counts[after];
        }
    }
    if (changes != 384u || old_counts[MAT_WATER] != 5120u ||
        new_counts[MAT_WATER] != old_counts[MAT_WATER] ||
        new_counts[MAT_STEEL] != old_counts[MAT_STEEL] ||
        new_counts[MAT_GLASS] + 128u != old_counts[MAT_GLASS] ||
        new_counts[MAT_FACTORY_CORE] != old_counts[MAT_FACTORY_CORE] + 64u ||
        new_counts[MAT_ATMOSPHERE] != old_counts[MAT_ATMOSPHERE] + 64u) return 3;
    for (uint material = 0u; material < MATERIAL_COUNT; ++material) {
        if (material != MAT_GLASS && material != MAT_FACTORY_CORE &&
            material != MAT_ATMOSPHERE && old_counts[material] != new_counts[material])
            return 4;
    }

    // Every downcomer column now has a real open route from its initial Water
    // to the basin. Original Glass roof is an explicit pre-fix counterexample.
    for (int x = 64; x < 80; ++x) {
        for (int y = 64; y < 208; ++y) {
            const auto expected = y >= 176 && y < 184 ? MAT_ATMOSPHERE : MAT_WATER;
            if (current({x, y}) != expected) return 5;
        }
        for (int y = 176; y < 184; ++y) {
            if (previous({x, y}) != MAT_GLASS) return 6;
        }
        if (current({x, 208}) != MAT_GLASS) return 7;
    }
    for (int x = 40; x < 120; ++x) {
        if ((x < 64 || x >= 80) && current({x, 176}) != MAT_GLASS) return 8;
    }

    // The full 8x8 enemy Core and all 2x3 relocated Steel stock tiles survive
    // final constructor ordering, not merely an earlier assignment statement.
    if (previous({491, 219}) != MAT_STEEL ||
        current({491, 219}) != MAT_FACTORY_CORE) return 9;
    for (int y = 192; y < 216; ++y) {
        for (int x = 488; x < 504; ++x) {
            if (current({x, y}) != MAT_STEEL) return 10;
        }
    }
    for (int y = 216; y < 224; ++y) {
        for (int x = 488; x < 496; ++x) {
            if (current({x, y}) != MAT_FACTORY_CORE) return 11;
        }
    }
    // Exact existing actor body: foot(168,207), height23, half-width4. Spawn
    // owner and initially uncharged enemy policy are not edited by this stage.
    for (int y = 185; y <= 207; ++y) {
        for (int x = 164; x <= 172; ++x) {
            if (current({x, y}) != MAT_ATMOSPHERE ||
                current({x, y}) != previous({x, y})) return 12;
        }
    }
    for (int x = 164; x <= 172; ++x) {
        if (current({x, 208}) != MAT_STEEL) return 13;
    }

    // Changes may not introduce partially filled authoring tiles. Fine actors
    // on the separate surface row remain byte-for-byte material-identical.
    for (int by = 0; by < 45; ++by) {
        for (int bx = 0; bx < 80; ++bx) {
            const ivec2 first{bx * 8, by * 8};
            if (current(first) == previous(first)) continue;
            for (int dy = 0; dy < 8; ++dy) {
                for (int dx = 0; dx < 8; ++dx) {
                    const ivec2 p{first.x + dx, first.y + dy};
                    if (current(p) != current(first) || previous(p) != previous(first)) return 14;
                }
            }
        }
    }
    std::cout << "Frontier actual constructor: 230400 cells checked, " << changes
              << " exact changed cells; 5120 Water and Steel totals preserved; "
                 "16 open inlet columns, 64 restored enemy Core cells, spawn intact. "
                 "Authored CPU evidence only; reset GPU/flow validation remains pending.\n";
    return 0;
}
