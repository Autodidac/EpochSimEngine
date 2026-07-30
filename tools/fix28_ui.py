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


# Fragment shader reads the shared stats tail and overlays a four-row code-stat
# strip at the top of the simulation only when F3 debug is enabled.
fullscreen = read('shaders/fullscreen.frag')
fullscreen = one(fullscreen, '#include "ui_text.glsl"\n', '#include "ui_text.glsl"\n#include "debug_stats.glsl"\n', 'fragment stats include')
fullscreen = one(
    fullscreen,
    'layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };\n',
    'layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };\n'
    'layout(std430, binding = 5) readonly buffer DebugStatsBuffer { uint debugStats[]; };\n',
    'fragment stats buffer')
fullscreen = one(
    fullscreen,
    'uint decimalLength(uint value) {\n    if (value >= 10000u) return 5u;\n',
    'uint decimalLength(uint value) {\n    if (value >= 1000000u) return 7u;\n    if (value >= 100000u) return 6u;\n    if (value >= 10000u) return 5u;\n',
    'seven digit decimal length')
fullscreen = one(
    fullscreen,
    'uint decimalDivisor(uint positionFromRight) {\n    if (positionFromRight == 4u) return 10000u;\n',
    'uint decimalDivisor(uint positionFromRight) {\n    if (positionFromRight == 6u) return 1000000u;\n    if (positionFromRight == 5u) return 100000u;\n    if (positionFromRight == 4u) return 10000u;\n',
    'seven digit divisor')
fullscreen = one(fullscreen, '    value = min(value, 99999u);\n', '    value = min(value, 9999999u);\n', 'seven digit clamp')
stat_helper = r'''
bool statPixel(ivec2 pixel, ivec2 origin, uint labelId, uint value) {
    bool label = fixedPixel(pixel, origin, 1, labelId);
    int numberX = int(fixedTextLength(labelId)) * 6 + 4;
    return label || numberPixel(pixel, origin + ivec2(numberX, 0), 1, value);
}
'''
fullscreen = one(fullscreen, 'bool borderPixel(uint x, uint y, uint left, uint top, uint right, uint bottom) {\n', stat_helper + '\nbool borderPixel(uint x, uint y, uint left, uint top, uint right, uint bottom) {\n', 'stat text helper')
stat_panel = r'''
    if (renderPc.debugMode != 0u) {
        uint panelLeft = renderPc.viewportLeft + 4u;
        uint panelRight = viewportRight > 4u ? viewportRight - 4u : viewportRight;
        uint panelTop = renderPc.viewportTop + 4u;
        uint panelBottom = min(viewportBottom, panelTop + 76u);
        if (x >= panelLeft && x < panelRight && y >= panelTop && y < panelBottom) {
            color.rgb = vec3(0.018, 0.027, 0.040);
            if (borderPixel(x, y, panelLeft, panelTop, panelRight, panelBottom))
                color.rgb = vec3(0.16, 0.30, 0.42);

            bool statsText = fixedPixel(pixel, ivec2(int(panelLeft + 7u), int(panelTop + 4u)), 1, 75u);
            uint columnWidth = max((panelRight - panelLeft - 12u) / 6u, 1u);
            uint row0 = panelTop + 19u;
            uint row1 = panelTop + 33u;
            uint row2 = panelTop + 47u;
            uint row3 = panelTop + 61u;
            uint columnLeft = panelLeft + 7u;

            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 0u), int(row0)), 76u, debugStats[STAT_SIMULATION_STEP]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 1u), int(row0)), 77u, debugStats[STAT_MOVE_PAIR_TESTS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 2u), int(row0)), 78u, debugStats[STAT_MOVE_SWAPS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 3u), int(row0)), 79u, debugStats[STAT_MOVED_CELLS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 4u), int(row0)), 80u, debugStats[STAT_ACTIVE_CELLS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 5u), int(row0)), 81u, debugStats[STAT_SLEEPING_TILES]);

            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 0u), int(row1)), 82u, debugStats[STAT_BEE_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 1u), int(row1)), 83u, debugStats[STAT_BEE_MOVES]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 2u), int(row1)), 84u, debugStats[STAT_QUEEN_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 3u), int(row1)), 85u, debugStats[STAT_NEST_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 4u), int(row1)), 86u, debugStats[STAT_FLOWER_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 5u), int(row1)), 87u, debugStats[STAT_HONEY_COUNT]);

            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 0u), int(row2)), 88u, debugStats[STAT_ANT_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 1u), int(row2)), 89u, debugStats[STAT_ANT_MOVES]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 2u), int(row2)), 90u, debugStats[STAT_BEETLE_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 3u), int(row2)), 91u, debugStats[STAT_BEETLE_MOVES]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 4u), int(row2)), 92u, debugStats[STAT_HABITAT_COUNT]);
            uint selected = min(renderPc.selectedMaterial, renderPc.materialCount - 1u);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 5u), int(row2)), 93u, debugStats[STAT_MATERIAL_BASE + selected]);

            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 0u), int(row3)), 94u, debugStats[STAT_STRUCTURAL_CELLS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 1u), int(row3)), 95u, debugStats[STAT_LIQUID_CELLS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 2u), int(row3)), 96u, debugStats[STAT_GAS_CELLS]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 3u), int(row3)), 97u, debugStats[STAT_POLLEN_COUNT]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 4u), int(row3)), 98u, debugStats[STAT_ACTIVE_TILES]);
            statsText = statsText || statPixel(pixel, ivec2(int(columnLeft + columnWidth * 5u), int(row3)), 1u, renderPc.framesPerSecond);

            if (statsText) color.rgb = vec3(0.94, 0.98, 1.0);
            outColor = vec4(color.rgb, 1.0);
            return;
        }
    }

'''
fullscreen = one(
    fullscreen,
    '    if (actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {\n',
    stat_panel + '    if (actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {\n',
    'debug stats panel')
write('shaders/fullscreen.frag', fullscreen)


# Build and dependency graph for the new compute shader/shared constants.
cmake = read('CMakeLists.txt')
cmake = one(cmake, '        actor.comp\n        fullscreen.vert\n', '        actor.comp\n        debug_stats.comp\n        fullscreen.vert\n', 'debug shader source')
cmake = one(
    cmake,
    '                    "${SHADER_SOURCE_DIR}/conservation.glsl"\n',
    '                    "${SHADER_SOURCE_DIR}/conservation.glsl"\n'
    '                    "${SHADER_SOURCE_DIR}/debug_stats.glsl"\n',
    'debug shader dependency')
write('CMakeLists.txt', cmake)

print('Fix28 bee wave, insect farm, default ecosystem, and debug stats applied.')
