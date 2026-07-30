#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding='utf-8', newline='\n')


def one(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)


def rx(text: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f'{label}: expected one regex match, found {count}')
    return result


# Make the repaired ecosystem/hive scene the first scene shown on startup.
shared = read('include/epoch/sand/shared_state.hpp')
shared = one(
    shared,
    '    std::atomic_uint32_t selected_scene{static_cast<std::uint32_t>(Scene::sandbox)};\n',
    '    std::atomic_uint32_t selected_scene{static_cast<std::uint32_t>(Scene::ecosystem)};\n',
    'default ecosystem scene')
write('include/epoch/sand/shared_state.hpp', shared)


# Replace the ecosystem procedural fallback. It keeps the successful new hive,
# adds authored curved bee bands, and excavates a real ant/beetle farm instead
# of leaving the farm interior filled with terrain.
reset = read('shaders/reset.comp')
ecosystem = r'''uint ecosystemMaterial(ivec2 p) {
    int width = int(pc.width);
    int height = int(pc.height);
    int floorY = height - 10;
    uint material = MAT_EMPTY;

    if (p.y >= floorY) return MAT_STONE;
    if ((p.x < 8 || p.x >= width - 8) && p.y >= 8) material = MAT_STONE;

    int groundY = floorY - 54 + int(hash32(uint(p.x) * 31u) % 5u);
    if (p.y >= groundY && p.y < floorY)
        material = p.y == groundY ? MAT_GRASS : MAT_DIRT;

    // Four-pixel reservoir walls satisfy the 32-pixel cohesive minimum in each
    // aligned region; scene glass can no longer survive as one-pixel threads.
    int tankLeft = 22;
    int tankRight = 148;
    int tankTop = 54;
    int tankBottom = groundY - 8;
    int tankThickness = 4;
    bool tankBounds = p.x >= tankLeft && p.x <= tankRight &&
                      p.y >= tankTop && p.y <= tankBottom;
    bool tankWall = tankBounds &&
        (p.x < tankLeft + tankThickness || p.x > tankRight - tankThickness ||
         p.y < tankTop + tankThickness || p.y > tankBottom - tankThickness);
    bool outlet = p.x >= tankRight - 7 && p.x <= tankRight &&
                  p.y >= tankBottom - 11 && p.y <= tankBottom;
    if (tankWall && !outlet) material = MAT_GLASS;
    if (p.x >= tankLeft + tankThickness && p.x <= tankRight - tankThickness &&
        p.y >= tankTop + 14 && p.y <= tankBottom - tankThickness)
        material = MAT_WATER;

    int trenchY = groundY - 5;
    if (p.x >= tankRight && p.x < 430 && p.y >= trenchY + 4 && p.y < trenchY + 8)
        material = MAT_GLASS;
    if (p.x >= tankRight && p.x < 430 && p.y > trenchY && p.y < trenchY + 4)
        material = MAT_WATER;

    for (int bed = 0; bed < 3; ++bed) {
        int left = 178 + bed * 78;
        int right = left + 58;
        if (p.x >= left && p.x < right && p.y >= groundY - 8 && p.y <= groundY)
            material = p.y == groundY - 8 ? MAT_GRASS : MAT_DIRT;
        int seedX = left + 14;
        int matureX = left + 38;
        if (p.x == seedX && p.y == groundY - 9) material = MAT_SEED;
        if (p.x == matureX && p.y >= groundY - 12 && p.y <= groundY - 9)
            material = MAT_PLANT_STEM;
        if (p.x == matureX && p.y == groundY - 13) material = MAT_FLOWER;
        if (p.x == right - 4 && p.y == groundY - 9) material = MAT_FERTILIZER;
    }

    // Proper bottom insect farm: clear the chamber first, then add thick walls,
    // substrate, two habitats, recoverable waste/fertilizer, ants, and beetles.
    int farmLeft = 244;
    int farmRight = 420;
    int farmTop = groundY + 10;
    int farmBottom = floorY - 4;
    int farmWallThickness = 4;
    bool farmInterior = p.x > farmLeft && p.x < farmRight &&
                        p.y > farmTop && p.y < farmBottom;
    if (farmInterior) material = MAT_EMPTY;
    bool farmWall = p.x >= farmLeft && p.x <= farmRight &&
                    p.y >= farmTop && p.y <= farmBottom &&
        (p.x < farmLeft + farmWallThickness || p.x > farmRight - farmWallThickness ||
         p.y < farmTop + farmWallThickness || p.y > farmBottom - farmWallThickness);
    bool farmVent = p.y < farmTop + farmWallThickness &&
                    ((p.x >= farmLeft + 28 && p.x < farmLeft + 44) ||
                     (p.x >= farmRight - 44 && p.x < farmRight - 28));
    if (farmWall && !farmVent)
        material = p.y < farmTop + farmWallThickness ? MAT_GLASS : MAT_WOOD;

    int substrateTop = farmBottom - 12;
    if (p.x >= farmLeft + farmWallThickness && p.x <= farmRight - farmWallThickness &&
        p.y >= substrateTop && p.y < farmBottom - farmWallThickness)
        material = p.y == substrateTop ? MAT_GRASS : MAT_DIRT;

    if (rectContains(p, ivec2(farmLeft + 16, substrateTop - 8),
                        ivec2(farmLeft + 32, substrateTop)))
        material = MAT_INSECT_HABITAT;
    if (rectContains(p, ivec2(farmLeft + 48, substrateTop - 8),
                        ivec2(farmLeft + 64, substrateTop)))
        material = MAT_INSECT_HABITAT;

    if (rectContains(p, ivec2(farmRight - 62, substrateTop - 7),
                        ivec2(farmRight - 42, substrateTop)))
        material = ((p.x + p.y) & 1) == 0 ? MAT_WASTE : MAT_FERTILIZER;
    if (rectContains(p, ivec2(farmRight - 36, substrateTop - 7),
                        ivec2(farmRight - 16, substrateTop)))
        material = ((p.x + p.y) & 1) == 0 ? MAT_WOOD : MAT_WASTE;

    if (p.y == substrateTop - 1 && p.x > farmLeft + 72 && p.x < farmRight - 70 &&
        (p.x % 7) == 0)
        material = ((p.x / 7) & 1) == 0 ? MAT_ANT : MAT_BEETLE;
    if (p.y == substrateTop - 2 && p.x > farmLeft + 20 && p.x < farmLeft + 64 &&
        (p.x % 11) == 0)
        material = ((p.x / 11) & 1) == 0 ? MAT_BEETLE : MAT_ANT;

    // Canonical new hive. The shell/chambers stay compact, while three curved
    // authored bee bands establish the synchronized wave immediately on reset.
    ivec2 queen = ivec2(width - 104, groundY - 72);
    ivec2 q = p - queen;
    int q2 = q.x * q.x + q.y * q.y;
    if (q2 >= 28 && q2 < 108) material = MAT_BEE_NEST;
    if (q2 == 0) material = MAT_QUEEN_BEE;
    else if (q.x >= 1 && q.x <= 12 && abs(q.y) <= 1) material = MAT_EMPTY;
    else if (q2 < 28) {
        uint chamber = hash32(indexOf(p) ^ pc.seed ^ 0xb33u);
        material = (chamber & 3u) == 0u ? MAT_EMPTY
            : ((chamber & 4u) == 0u ? MAT_HONEY : MAT_POLLEN);
    }

    if (material == MAT_EMPTY && q2 > 130 && q2 < 6200) {
        float radius = length(vec2(q));
        float angle = atan(float(q.y), float(q.x));
        float wavePhase = angle * 3.0 + radius * 0.14;
        bool innerWave = abs(radius - (18.0 + sin(wavePhase) * 3.5)) < 1.15;
        bool middleWave = abs(radius - (30.0 + sin(wavePhase + 1.7) * 5.0)) < 1.25;
        bool outerWave = abs(radius - (44.0 + sin(wavePhase + 3.2) * 7.0)) < 1.35;
        uint swarm = hash32(indexOf(p) ^ pc.seed ^ 0xbee51u);
        if ((innerWave || middleWave || outerWave) && (swarm & 3u) != 0u)
            material = MAT_BEE;
        else if (radius < 58.0 && (swarm % 53u) == 0u)
            material = MAT_BEE;
    }

    for (int flower = 0; flower < 6; ++flower) {
        int x = width - 240 + flower * 26;
        if (p.x == x && p.y >= groundY - 5 && p.y <= groundY - 2)
            material = MAT_PLANT_STEM;
        if (p.x == x && p.y == groundY - 6) material = MAT_FLOWER;
    }

    if (material == MAT_EMPTY && p.y > groundY - 62 && p.y < groundY - 12 &&
        p.x > 150 && p.x < 430 &&
        (hash32(indexOf(p) ^ pc.seed ^ 0x02u) % 17u) == 0u)
        material = MAT_OXYGEN;
    if (material == MAT_EMPTY && p.y >= groundY - 16 && p.y < groundY &&
        (hash32(indexOf(p) ^ pc.seed ^ 0xc02u) % 13u) == 0u)
        material = MAT_CARBON_DIOXIDE;

    int gasLeft = width - 188;
    int gasRight = width - 20;
    int gasTop = 18;
    int gasBottom = 72;
    int gasThickness = 4;
    bool gasBounds = p.x >= gasLeft && p.x <= gasRight &&
                     p.y >= gasTop && p.y <= gasBottom;
    bool gasWall = gasBounds &&
        (p.x < gasLeft + gasThickness || p.x > gasRight - gasThickness ||
         p.y < gasTop + gasThickness || p.y > gasBottom - gasThickness);
    if (gasWall) material = MAT_GLASS;
    if (p.x >= gasLeft + gasThickness && p.x <= gasRight - gasThickness &&
        p.y >= gasTop + gasThickness && p.y <= gasBottom - gasThickness &&
        (hash32(indexOf(p) ^ pc.seed ^ 0x4a2u) & 1u) == 0u)
        material = MAT_HYDROGEN;

    return material;
}

'''
reset = rx(
    reset,
    r'uint ecosystemMaterial\(ivec2 p\) \{.*?\n\}\n\nuint engineeringMaterial',
    ecosystem + 'uint engineeringMaterial',
    'ecosystem scene')
write('shaders/reset.comp', reset)
