#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')

def require(path: str, tokens: tuple[str, ...]) -> None:
    source = text(path)
    for token in tokens:
        if token not in source:
            errors.append(f"{path}: missing {token!r}")

require('include/epoch/sand/shared_state.hpp', ('Scene::ecosystem',))
require('shaders/reset.comp', (
    'farmInterior', 'farmWallThickness = 4', 'tankThickness = 4',
    'MAT_INSECT_HABITAT', 'MAT_ANT : MAT_BEETLE', 'MAT_BEETLE : MAT_ANT',
    'innerWave', 'middleWave', 'outerWave', 'Canonical new hive',
))
require('shaders/move.comp', (
    '#include "debug_stats.glsl"', 'regionalTargetSignal', 'beeWaveVertical',
    'beeWaveHorizontal', 'STAT_MOVE_SWAPS', 'STAT_BEE_MOVES',
    'STAT_ANT_MOVES', 'STAT_BEETLE_MOVES', 'movePc.reserved0',
))
move = re.sub(r'//.*?$|/\*.*?\*/', '', text('shaders/move.comp'), flags=re.M | re.S)
if re.search(r'\bfor\s*\(', move):
    errors.append('shaders/move.comp: bounded movement shader reintroduced loops')

require('shaders/debug_stats.glsl', (
    'DEBUG_STAT_WORD_COUNT = 128u', 'STAT_MOVE_PAIR_TESTS', 'STAT_MOVE_SWAPS',
    'STAT_BEE_COUNT', 'STAT_ANT_COUNT', 'STAT_BEETLE_COUNT', 'STAT_MATERIAL_BASE',
))
require('shaders/debug_stats.comp', (
    'local_size_x = 256', 'atomicAdd(debugStats[STAT_MATERIAL_BASE + cell.material]',
    'STAT_SLEEPING_TILES', 'STAT_HABITAT_COUNT',
))
require('src/vulkan_renderer.cpp', (
    'debug_stat_word_count = 128', 'debug_stats_pipeline', 'reset_debug_stats',
    'record_debug_stats', 'debug_stats.comp.spv', 'collect_debug_stats ? 1u : 0u',
    'config.grid_width * config.grid_height * 9u / 2u',
    'VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT',
))
require('shaders/fullscreen.frag', (
    '#include "debug_stats.glsl"', 'statPixel', '75u',
    'STAT_MOVE_PAIR_TESTS', 'STAT_BEE_MOVES', 'STAT_ANT_MOVES',
    'STAT_BEETLE_MOVES', 'STAT_MATERIAL_BASE + selected',
))
require('tools/generate_ui_text.py', (
    '"DEBUG STATS"', '"BEE MOVES"', '"ANT MOVES"', '"BEETLE MOVES"',
    '"ACTIVE TILES"',
))
require('CMakeLists.txt', ('debug_stats.comp', 'debug_stats.glsl'))

if errors:
    print('Fix28 audit failed:', file=sys.stderr)
    for error in errors:
        print(f'  - {error}', file=sys.stderr)
    raise SystemExit(1)
print('Fix28 audit passed: ecosystem default, curved bee wave, repaired insect farm, and live debug statistics.')
