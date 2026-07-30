#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/vulkan_renderer.cpp"
text = PATH.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"vulkan_renderer.cpp: expected one match, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)


replace_once(
    "        const auto radius = material == static_cast<std::uint32_t>(Material::bee_nest)\n"
    "            ? (std::max)(requested_radius, 4u)\n",
    "        const auto radius = material == static_cast<std::uint32_t>(Material::bee_nest)\n"
    "            ? 64u\n")
replace_once(
    "                if (material == Material::bee_nest) return (std::max)(state.brush_radius.load(std::memory_order_relaxed), 4u);\n",
    "                if (material == Material::bee_nest) return 12u;\n")

anchor = '''std::uint32_t divide_round_up(const std::uint32_t value, const std::uint32_t divisor) {
    return (value + divisor - 1u) / divisor;
}
'''
helper = anchor + r'''

constexpr std::uint32_t fill_aux_structural = 0x04000000u;
constexpr std::uint32_t fill_aux_supported = 0x02000000u;
constexpr std::uint32_t fill_aux_state_mask = 0x000000ffu;
constexpr std::uint32_t fill_aux_random_mask = 0x00ffff00u;

std::uint32_t fill_hash(std::uint32_t value) noexcept {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

SceneCell make_fill_cell(const std::uint32_t material_id, const std::uint32_t index) {
    const auto material = static_cast<Material>(material_id < material_count ? material_id : 0u);
    SceneCell cell{
        .material = static_cast<std::uint32_t>(material),
        .age = 0u,
        .temperature = 20,
        .aux = fill_hash(index ^ material_id * 0x9e3779b9u) & fill_aux_random_mask,
    };
    if (material == Material::magma_vent || material == Material::lava) cell.temperature = 1300;
    else if (material == Material::fire || material == Material::lightning) cell.temperature = 700;
    else if (material == Material::ember) cell.temperature = 420;
    else if (material == Material::ice) cell.temperature = -20;
    else if (material == Material::snow) cell.temperature = -8;
    else if (material == Material::steam || material == Material::dirty_steam) cell.temperature = 110;

    if (material == Material::saltwater || material == Material::dirty_water) cell.aux |= 96u;
    else if (material == Material::salt || material == Material::honey ||
             material == Material::silt || material == Material::fertilizer ||
             material == Material::food || material == Material::waste) cell.aux |= 255u;
    else if (material == Material::oxygen) cell.aux |= 220u;
    else if (material == Material::carbon_dioxide) cell.aux |= 180u;
    else if (material == Material::hydrogen) cell.aux |= 210u;

    if (is_block_material(material)) {
        cell.aux |= fill_aux_structural | fill_aux_supported;
        cell.aux = (cell.aux & ~fill_aux_state_mask) | 255u;
    }
    return cell;
}
'''
replace_once(anchor, helper)

anchor = '''    void upload_scene_cells(const std::span<const SceneCell> cells) {
'''
methods = r'''    [[nodiscard]] std::vector<SceneCell> download_scene_cells() {
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(fill readback)");
        std::memcpy(cells.data(), mapped, cells.size() * sizeof(SceneCell));
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return cells;
    }

    void fill_connected_region(SharedState& state) {
        auto cells = download_scene_cells();
        const auto [cursor_x, cursor_y] = grid_cursor(state);
        if (cursor_x < 0 || cursor_y < 0 ||
            cursor_x >= static_cast<std::int32_t>(config.grid_width) ||
            cursor_y >= static_cast<std::int32_t>(config.grid_height)) return;

        const auto replacement_id =
            state.selected_material.load(std::memory_order_relaxed) % material_count;
        const auto replacement = static_cast<Material>(replacement_id);
        if (replacement == Material::bee || replacement == Material::queen_bee ||
            replacement == Material::bee_nest) {
            startup_log("Fill ignored for colony agents/prefab; place Bee nest to build a live colony.");
            return;
        }

        const auto start = static_cast<std::uint32_t>(cursor_y) * config.grid_width +
                           static_cast<std::uint32_t>(cursor_x);
        const auto target = cells[start].material;
        if (target == replacement_id) return;

        std::vector<std::uint8_t> visited(cells.size(), 0u);
        std::vector<std::uint32_t> queue(cells.size());
        std::size_t head = 0;
        std::size_t tail = 0;
        visited[start] = 1u;
        queue[tail++] = start;

        while (head < tail) {
            const auto index = queue[head++];
            const auto x = index % config.grid_width;
            const auto y = index / config.grid_width;
            const auto enqueue = [&](const std::uint32_t candidate) {
                if (visited[candidate] == 0u && cells[candidate].material == target) {
                    visited[candidate] = 1u;
                    queue[tail++] = candidate;
                }
            };
            if (x > 0u) enqueue(index - 1u);
            if (x + 1u < config.grid_width) enqueue(index + 1u);
            if (y > 0u) enqueue(index - config.grid_width);
            if (y + 1u < config.grid_height) enqueue(index + config.grid_width);
        }

        std::size_t changed = 0;
        if (is_block_material(replacement)) {
            constexpr std::uint32_t block = 8u;
            const auto tile_columns = divide_round_up(config.grid_width, block);
            const auto tile_rows = divide_round_up(config.grid_height, block);
            std::vector<std::uint8_t> touched(
                static_cast<std::size_t>(tile_columns) * tile_rows, 0u);
            for (std::uint32_t index = 0; index < visited.size(); ++index) {
                if (visited[index] == 0u) continue;
                const auto x = index % config.grid_width;
                const auto y = index / config.grid_width;
                touched[(y / block) * tile_columns + (x / block)] = 1u;
            }
            for (std::uint32_t tile_y = 0; tile_y < tile_rows; ++tile_y) {
                for (std::uint32_t tile_x = 0; tile_x < tile_columns; ++tile_x) {
                    if (touched[tile_y * tile_columns + tile_x] == 0u) continue;
                    bool complete = true;
                    for (std::uint32_t local_y = 0; local_y < block && complete; ++local_y) {
                        for (std::uint32_t local_x = 0; local_x < block; ++local_x) {
                            const auto x = tile_x * block + local_x;
                            const auto y = tile_y * block + local_y;
                            if (x >= config.grid_width || y >= config.grid_height ||
                                visited[y * config.grid_width + x] == 0u) {
                                complete = false;
                                break;
                            }
                        }
                    }
                    if (!complete) continue;
                    for (std::uint32_t local_y = 0; local_y < block; ++local_y) {
                        for (std::uint32_t local_x = 0; local_x < block; ++local_x) {
                            const auto index = (tile_y * block + local_y) * config.grid_width +
                                               tile_x * block + local_x;
                            cells[index] = make_fill_cell(replacement_id, index);
                            ++changed;
                        }
                    }
                }
            }
        } else {
            for (std::uint32_t index = 0; index < visited.size(); ++index) {
                if (visited[index] == 0u) continue;
                cells[index] = make_fill_cell(replacement_id, index);
                ++changed;
            }
        }

        if (changed == 0u) {
            startup_log("Fill found no complete aligned bricks in the connected region.");
            return;
        }
        upload_scene_cells(cells);
        startup_log("Filled connected region: " + std::to_string(changed) + " cells.");
    }

''' + anchor
replace_once(anchor, methods)

replace_once(
    '''        const bool explicit_load = state.load_scene_image.exchange(false, std::memory_order_acq_rel);
        const bool reset_requested = needs_reset || state.reset.exchange(false, std::memory_order_acq_rel);
''',
    '''        const bool explicit_load = state.load_scene_image.exchange(false, std::memory_order_acq_rel);
        if (state.fill_region.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) fill_connected_region(state);
            else startup_log("Fill skipped until the initial scene exists.");
        }
        const bool reset_requested = needs_reset || state.reset.exchange(false, std::memory_order_acq_rel);
''')

PATH.write_text(text, encoding="utf-8", newline="\n")
print("Applied Fix35 connected fill and live-colony paint dispatch patch.")
