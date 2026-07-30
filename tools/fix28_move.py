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


# Restore a coherent old-style travelling wave while retaining current hive,
# flower, pollen, honey, and hazard goals. Multi-scale fixed probes provide a
# bounded directional field without reintroducing loops into move.comp.
move = read('shaders/move.comp')
move = one(move, '#include "tiles.glsl"\n', '#include "tiles.glsl"\n#include "debug_stats.glsl"\n', 'movement stats include')
move = one(
    move,
    'layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };\n',
    'layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };\n'
    'layout(std430, binding = 5) buffer DebugStatsBuffer { uint debugStats[]; };\n',
    'movement stats buffer')
move = one(
    move,
    '    cells[aIndex] = b;\n    cells[bIndex] = a;\n',
    '    cells[aIndex] = b;\n    cells[bIndex] = a;\n'
    '    if (movePc.reserved0 != 0u) {\n'
    '        atomicAdd(debugStats[STAT_MOVE_SWAPS], 1u);\n'
    '        uint movedMaterial = a.material != MAT_EMPTY ? a.material : b.material;\n'
    '        if (movedMaterial == MAT_BEE) atomicAdd(debugStats[STAT_BEE_MOVES], 1u);\n'
    '        else if (movedMaterial == MAT_ANT) atomicAdd(debugStats[STAT_ANT_MOVES], 1u);\n'
    '        else if (movedMaterial == MAT_BEETLE) atomicAdd(debugStats[STAT_BEETLE_MOVES], 1u);\n'
    '    }\n',
    'movement swap counters')
regional = r'''
int regionalTargetSignal(ivec2 p, uint first, uint second) {
    int score = localTargetSignal(p, first, second) * 12;
    score += localTargetSignal(p + ivec2(8, 0), first, second) * 7;
    score += localTargetSignal(p + ivec2(-8, 0), first, second) * 7;
    score += localTargetSignal(p + ivec2(0, 8), first, second) * 7;
    score += localTargetSignal(p + ivec2(0, -8), first, second) * 7;
    score += localTargetSignal(p + ivec2(12, 12), first, second) * 5;
    score += localTargetSignal(p + ivec2(-12, 12), first, second) * 5;
    score += localTargetSignal(p + ivec2(12, -12), first, second) * 5;
    score += localTargetSignal(p + ivec2(-12, -12), first, second) * 5;
    score += localTargetSignal(p + ivec2(24, 0), first, second) * 4;
    score += localTargetSignal(p + ivec2(-24, 0), first, second) * 4;
    score += localTargetSignal(p + ivec2(0, 24), first, second) * 4;
    score += localTargetSignal(p + ivec2(0, -24), first, second) * 4;
    score += localTargetSignal(p + ivec2(40, 20), first, second) * 2;
    score += localTargetSignal(p + ivec2(-40, 20), first, second) * 2;
    score += localTargetSignal(p + ivec2(40, -20), first, second) * 2;
    score += localTargetSignal(p + ivec2(-40, -20), first, second) * 2;
    score += localTargetSignal(p + ivec2(64, 0), first, second);
    score += localTargetSignal(p + ivec2(-64, 0), first, second);
    score += localTargetSignal(p + ivec2(0, 64), first, second);
    score += localTargetSignal(p + ivec2(0, -64), first, second);
    return score;
}
'''
move = one(move, '    return score;\n}\n\nint beeTargetSignal', '    return score;\n}\n' + regional + '\nint beeTargetSignal', 'regional bee field')
move = one(move, '        return localTargetSignal(p, MAT_QUEEN_BEE, MAT_BEE_NEST);\n', '        return regionalTargetSignal(p, MAT_QUEEN_BEE, MAT_BEE_NEST);\n', 'home signal')
move = one(move, '    int flowerSignal = localTargetSignal(p, MAT_FLOWER, MAT_FLOWER);\n', '    int flowerSignal = regionalTargetSignal(p, MAT_FLOWER, MAT_FLOWER);\n', 'flower signal')
move = one(move, '        int foodSignal = localTargetSignal(p, MAT_HONEY, MAT_QUEEN_BEE);\n', '        int foodSignal = regionalTargetSignal(p, MAT_HONEY, MAT_QUEEN_BEE);\n', 'food signal')
bee_wave = r'''int beeWaveVertical(Cell bee, ivec2 p) {
    uint lane = ((bee.aux >> 8u) & 1u) * 3u;
    uint phase = (movePc.step / 2u + uint(max(p.x, 0)) * 2u + lane) % 32u;
    if (phase < 8u) return -1;
    if (phase < 16u) return 0;
    if (phase < 24u) return 1;
    return 0;
}

int beeWaveHorizontal(Cell bee, ivec2 p) {
    uint lane = ((bee.aux >> 9u) & 3u) * 5u;
    uint phase = (movePc.step / 3u + uint(max(p.y, 0)) * 2u + lane) % 64u;
    return phase < 32u ? 1 : -1;
}

bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;

    ivec2 delta = targetPosition - sourcePosition;
    int sourceSignal = beeTargetSignal(bee, sourcePosition);
    int targetSignal = beeTargetSignal(bee, targetPosition);
    if (sourceSignal != targetSignal) return targetSignal > sourceSignal;

    int desiredVertical = beeWaveVertical(bee, sourcePosition);
    int desiredHorizontal = beeWaveHorizontal(bee, sourcePosition);
    if (delta.x == desiredHorizontal && delta.y == desiredVertical) return true;
    if (delta.x == desiredHorizontal && delta.y == 0) return true;
    if (delta.y == desiredVertical && delta.x == 0) return (randomValue & 1u) == 0u;
    return (randomValue & 15u) == 0u;
}
'''
move = rx(
    move,
    r'int beeWaveVertical\(Cell bee, ivec2 p\) \{.*?\n\}\nbool beeMoveAllowed\(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue\) \{.*?\n\}\n',
    bee_wave,
    'bee wave movement')
write('shaders/move.comp', move)
