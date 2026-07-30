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


# Add compact live debug labels to generated text storage.
generator = read('tools/generate_ui_text.py')
generator = one(
    generator,
    '    "BRACKETS SCENE", "F5 SAVE PPM", "F9 LOAD PPM",\n',
    '    "BRACKETS SCENE", "F5 SAVE PPM", "F9 LOAD PPM",\n'
    '    "DEBUG STATS", "STEP", "PAIRS", "SWAPS", "MOVED", "CELLS", "SLEEP TILES",\n'
    '    "BEES", "BEE MOVES", "QUEENS", "NESTS", "FLOWERS", "HONEY", "ANTS",\n'
    '    "ANT MOVES", "BEETLES", "BEETLE MOVES", "HABITATS", "SELECTED",\n'
    '    "STRUCT", "LIQUID", "GAS", "POLLEN", "ACTIVE TILES",\n',
    'debug text labels')
write('tools/generate_ui_text.py', generator)


# Add the stats shader, enlarge/share the existing conservation/debug buffer,
# collect movement swaps only while debug is enabled, and run one bounded stats
# reduction immediately before rendering.
renderer = read('src/vulkan_renderer.cpp')
renderer = one(
    renderer,
    'constexpr std::uint32_t sunlight_local_size = 64;\n',
    'constexpr std::uint32_t sunlight_local_size = 64;\n'
    'constexpr std::uint32_t debug_stats_local_size = 256;\n'
    'constexpr std::uint32_t debug_stat_word_count = 128;\n',
    'renderer debug constants')
renderer = one(renderer, '    VkPipeline actor_pipeline{};\n', '    VkPipeline actor_pipeline{};\n    VkPipeline debug_stats_pipeline{};\n', 'debug pipeline member')
renderer = one(renderer, '            if (actor_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, actor_pipeline, nullptr);\n', '            if (actor_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, actor_pipeline, nullptr);\n            if (debug_stats_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, debug_stats_pipeline, nullptr);\n', 'debug pipeline cleanup')
renderer = one(
    renderer,
    '        conservation_buffer = create_buffer(sizeof(std::uint32_t) * 8u, storage_usage,\n',
    '        conservation_buffer = create_buffer(sizeof(std::uint32_t) * debug_stat_word_count, storage_usage,\n',
    'debug buffer size')
renderer = rx(
    renderer,
    r'(\.binding = 5,\s*\.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,\s*\.descriptorCount = 1,\s*\.stageFlags = )VK_SHADER_STAGE_COMPUTE_BIT(,)',
    r'\1VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT\2',
    'debug fragment descriptor stage')
renderer = one(
    renderer,
    '        actor_pipeline = create_compute_pipeline("actor.comp.spv");\n',
    '        actor_pipeline = create_compute_pipeline("actor.comp.spv");\n'
    '        debug_stats_pipeline = create_compute_pipeline("debug_stats.comp.spv");\n',
    'debug compute pipeline')
insert_stats = r'''
    void reset_debug_stats(const VkCommandBuffer command_buffer) const {
        constexpr VkDeviceSize first_debug_word = sizeof(std::uint32_t) * 8u;
        constexpr VkDeviceSize debug_bytes = sizeof(std::uint32_t) * (debug_stat_word_count - 8u);
        vkCmdFillBuffer(command_buffer, conservation_buffer.handle,
                        first_debug_word, debug_bytes, 0u);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void record_debug_stats(const VkCommandBuffer command_buffer, const SharedState& state,
                            const std::uint32_t movement_pair_tests) const {
        const SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .radius = movement_pair_tests,
            .material = state.selected_material.load(std::memory_order_relaxed),
        };
        bind_compute(command_buffer, debug_stats_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        const auto cell_count = config.grid_width * config.grid_height;
        vkCmdDispatch(command_buffer, divide_round_up(cell_count, debug_stats_local_size), 1, 1);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

'''
renderer = one(
    renderer,
    '    void record_simulation_step(const VkCommandBuffer command_buffer) {\n',
    insert_stats + '    void record_simulation_step(const VkCommandBuffer command_buffer,\n'
    '                                const bool collect_debug_stats) {\n',
    'debug stats recording functions')
renderer = one(
    renderer,
    '                .parity = static_cast<std::int32_t>(\n                    phase == 5\n                        ? ((simulation_step + static_cast<std::uint32_t>(phase_index)) & 1u)\n                        : ((simulation_step + static_cast<std::uint32_t>(phase)) & 1u)),\n',
    '                .parity = static_cast<std::int32_t>(\n                    phase == 5\n                        ? ((simulation_step + static_cast<std::uint32_t>(phase_index)) & 1u)\n                        : ((simulation_step + static_cast<std::uint32_t>(phase)) & 1u)),\n'
    '                .reserved0 = collect_debug_stats ? 1u : 0u,\n',
    'movement debug flag')
renderer = one(
    renderer,
    '        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,\n                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,\n                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);\n\n        VkClearValue clear_value{};\n',
    '        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,\n                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,\n                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);\n'
    '        buffer_barrier(command_buffer, conservation_buffer,\n'
    '                       VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,\n'
    '                       VK_ACCESS_SHADER_READ_BIT,\n'
    '                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,\n'
    '                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);\n\n'
    '        VkClearValue clear_value{};\n',
    'debug render barrier')
renderer = one(
    renderer,
    '        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n'
    '        const bool run_simulation = !reset_this_frame && (step_once ||\n'
    '            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n'
    '        if (run_simulation) record_simulation_step(frame.command_buffer);\n',
    '        const bool debug_stats = state.debug_visualization.load(std::memory_order_relaxed);\n'
    '        if (debug_stats) reset_debug_stats(frame.command_buffer);\n'
    '        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n'
    '        const bool run_simulation = !reset_this_frame && (step_once ||\n'
    '            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n'
    '        if (run_simulation) record_simulation_step(frame.command_buffer, debug_stats);\n',
    'draw debug reset')
renderer = one(
    renderer,
    '        if (run_simulation || reset_actor || actor_action || actor_motion)\n'
    '            record_actor(frame.command_buffer, state, reset_actor, actor_simulation);\n\n'
    '        record_render(frame.command_buffer, image_index, state);\n',
    '        if (run_simulation || reset_actor || actor_action || actor_motion)\n'
    '            record_actor(frame.command_buffer, state, reset_actor, actor_simulation);\n\n'
    '        if (debug_stats) {\n'
    '            const auto movement_pair_tests = run_simulation\n'
    '                ? config.grid_width * config.grid_height * 9u / 2u\n'
    '                : 0u;\n'
    '            record_debug_stats(frame.command_buffer, state, movement_pair_tests);\n'
    '        }\n'
    '        record_render(frame.command_buffer, image_index, state);\n',
    'draw debug stats pass')
write('src/vulkan_renderer.cpp', renderer)
