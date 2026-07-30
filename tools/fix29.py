#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def one(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def rx(text: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected one regex match, found {count}")
    return result


write("include/epoch/sand/shared_state.hpp", r'''#pragma once

#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"

#include <atomic>
#include <cstdint>

namespace epoch::sand {

struct SharedState final {
    std::atomic_bool quit{false};
    std::atomic_bool paused{false};
    std::atomic_bool single_step{false};
    std::atomic_bool reset{false};
    std::atomic_bool save_scene_image{false};
    std::atomic_bool load_scene_image{false};

    std::atomic_uint32_t window_width{1280};
    std::atomic_uint32_t window_height{720};
    std::atomic_bool resized{false};

    std::atomic_int mouse_x{0};
    std::atomic_int mouse_y{0};
    std::atomic_bool primary_down{false};
    std::atomic_bool secondary_down{false};
    std::atomic_bool inspect_material{false};
    std::atomic_bool debug_visualization{false};

    std::atomic_uint32_t selected_material{static_cast<std::uint32_t>(Material::sand)};
    std::atomic_uint32_t selected_group{static_cast<std::uint32_t>(MaterialGroup::ground)};
    std::atomic_uint32_t hovered_material{material_count};
    std::atomic_uint32_t hovered_group{material_group_count};
    std::atomic_uint32_t selected_scene{static_cast<std::uint32_t>(Scene::ecosystem)};
    std::atomic_uint32_t brush_radius{6};
    std::atomic_uint32_t brush_shape{0};
    std::atomic_uint32_t camera_zoom{1};
    std::atomic_int camera_center_x{320};
    std::atomic_int camera_center_y{180};
    std::atomic_uint32_t steps_per_frame{1};
    std::atomic_uint32_t frames_per_second{0};

    std::atomic_int move_x{0};
    std::atomic_int move_y{0};
    std::atomic_bool jump{false};
    std::atomic_bool mining_mode{false};
    std::atomic_bool fire_tool{false};
    std::atomic_bool fire_tool_pressed{false};
    std::atomic_bool deposit_resource{false};
    std::atomic_bool deposit_resource_pressed{false};
};

} // namespace epoch::sand
''')

write("include/epoch/sand/ui_layout.hpp", r'''#pragma once
#include "epoch/sand/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>

namespace epoch::sand::ui {
inline constexpr std::uint32_t preferred_sidebar_width = 384u;
inline constexpr std::uint32_t minimum_sidebar_width = 300u;
inline constexpr std::uint32_t status_height = 126u;
inline constexpr std::uint32_t group_tabs_height = 112u;
inline constexpr std::uint32_t palette_items_height = 136u;
inline constexpr std::uint32_t eraser_height = 24u;
inline constexpr std::uint32_t keymap_height = 76u;
inline constexpr std::uint32_t cursor_editor_height = 92u;
inline constexpr std::uint32_t palette_height = 0u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;

struct Layout final {
    epochengine::gui_lib::Rect status{}, simulation{}, group_tabs{}, palette{};
    epochengine::gui_lib::Rect previous_scene{}, next_scene{}, reset_scene{}, save_scene{}, load_scene{};
    epochengine::gui_lib::Rect mode_toggle{}, debug_toggle{}, eraser{}, keymap{}, cursor_editor{}, material_card{};
    epochengine::gui_lib::Rect cursor_circle{}, cursor_square{}, cursor_horizontal{}, cursor_vertical{};
    epochengine::gui_lib::Rect brush_smaller{}, brush_larger{}, zoom_out{}, zoom_in{};
};
struct SimulationViewport final { epochengine::gui_lib::Rect rect{}; std::uint32_t tile_pixel_size{}; };

[[nodiscard]] inline SimulationViewport make_simulation_viewport(
    const Layout& layout, std::uint32_t grid_width, std::uint32_t grid_height) noexcept {
    constexpr std::uint32_t cells_per_tile = 8u;
    const auto tile_columns = (std::max)(1u, (grid_width + cells_per_tile - 1u) / cells_per_tile);
    const auto tile_rows = (std::max)(1u, (grid_height + cells_per_tile - 1u) / cells_per_tile);
    const auto panel_width = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.x));
    const auto panel_height = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.y));
    if (panel_width < tile_columns || panel_height < tile_rows) return {layout.simulation, 0u};
    const auto tile_pixels = (std::max)(1u, (std::min)(panel_width / tile_columns, panel_height / tile_rows));
    const auto viewport_width = tile_columns * tile_pixels;
    const auto viewport_height = tile_rows * tile_pixels;
    const auto left = layout.simulation.position.x + float((panel_width - viewport_width) / 2u);
    const auto top = layout.simulation.position.y + float((panel_height - viewport_height) / 2u);
    return {{{left, top}, {float(viewport_width), float(viewport_height)}}, tile_pixels};
}

[[nodiscard]] inline Layout make_layout(std::uint32_t width, std::uint32_t height) noexcept {
    const auto screen_width = (std::max)(width, 1u);
    const auto screen_height = (std::max)(height, 1u);
    const auto requested = (std::max)(minimum_sidebar_width, screen_width / 3u);
    const auto sidebar = screen_width > minimum_sidebar_width + 160u
        ? (std::min)(preferred_sidebar_width, requested)
        : (std::min)(screen_width, minimum_sidebar_width);
    const auto simulation_width = screen_width > sidebar ? screen_width - sidebar : 1u;
    const float left = float(simulation_width);
    const float side = float(screen_width - simulation_width);

    Layout layout{
        .status = {{left, 0.0f}, {side, float(status_height)}},
        .simulation = {{0.0f, 0.0f}, {float(simulation_width), float(screen_height)}},
        .group_tabs = {{left + margin, float(status_height) + margin},
                       {(std::max)(1.0f, side - margin * 2.0f), float(group_tabs_height)}},
        .palette = {{left + margin, float(status_height + group_tabs_height) + margin + gap},
                    {(std::max)(1.0f, side - margin * 2.0f), float(palette_items_height)}},
    };

    const float scene_left = left + 8.0f;
    const float scene_gap = 3.0f;
    const float scene_width = (std::max)(1.0f, (side - 16.0f - scene_gap * 4.0f) / 5.0f);
    layout.previous_scene = {{scene_left, 70.0f}, {scene_width, 26.0f}};
    layout.next_scene = {{scene_left + (scene_width + scene_gap), 70.0f}, {scene_width, 26.0f}};
    layout.reset_scene = {{scene_left + (scene_width + scene_gap) * 2.0f, 70.0f}, {scene_width, 26.0f}};
    layout.save_scene = {{scene_left + (scene_width + scene_gap) * 3.0f, 70.0f}, {scene_width, 26.0f}};
    layout.load_scene = {{scene_left + (scene_width + scene_gap) * 4.0f, 70.0f}, {scene_width, 26.0f}};

    layout.mode_toggle = {{left + 8.0f, 100.0f}, {(std::max)(112.0f, side * 0.46f), 22.0f}};
    layout.debug_toggle = {{layout.mode_toggle.position.x + layout.mode_toggle.size.x + 4.0f, 100.0f},
                           {(std::max)(1.0f, side - layout.mode_toggle.size.x - 24.0f), 22.0f}};

    const float content_left = left + margin;
    const float content_width = (std::max)(1.0f, side - margin * 2.0f);
    const float eraser_top = layout.palette.position.y + layout.palette.size.y + gap;
    layout.eraser = {{content_left, eraser_top}, {content_width, float(eraser_height)}};
    const float keymap_top = eraser_top + float(eraser_height) + gap;
    layout.keymap = {{content_left, keymap_top}, {content_width, float(keymap_height)}};
    const float cursor_top = keymap_top + float(keymap_height) + gap;
    layout.cursor_editor = {{content_left, cursor_top}, {content_width, float(cursor_editor_height)}};

    const float shape_top = cursor_top + 23.0f;
    const float shape_width = content_width / 4.0f;
    layout.cursor_circle = {{content_left, shape_top}, {shape_width, 24.0f}};
    layout.cursor_square = {{content_left + shape_width, shape_top}, {shape_width, 24.0f}};
    layout.cursor_horizontal = {{content_left + shape_width * 2.0f, shape_top}, {shape_width, 24.0f}};
    layout.cursor_vertical = {{content_left + shape_width * 3.0f, shape_top}, {shape_width, 24.0f}};

    const float control_top = cursor_top + 60.0f;
    const float half = content_width / 2.0f;
    layout.brush_smaller = {{content_left + 4.0f, control_top}, {30.0f, 26.0f}};
    layout.brush_larger = {{content_left + half - 34.0f, control_top}, {30.0f, 26.0f}};
    layout.zoom_out = {{content_left + half + 4.0f, control_top}, {30.0f, 26.0f}};
    layout.zoom_in = {{content_left + content_width - 34.0f, control_top}, {30.0f, 26.0f}};

    const float card_top = cursor_top + float(cursor_editor_height) + gap;
    layout.material_card = {{content_left, card_top},
                            {content_width, (std::max)(1.0f, float(screen_height) - card_top - margin)}};
    return layout;
}

[[nodiscard]] inline epochengine::gui_lib::Rect group_tab_rect(
    const Layout& layout, std::uint32_t index) noexcept {
    constexpr std::uint32_t columns = 2u;
    const auto rows = (material_group_count + columns - 1u) / columns;
    const auto column = index % columns;
    const auto row = index / columns;
    const float cell_width = layout.group_tabs.size.x / float(columns);
    const float cell_height = layout.group_tabs.size.y / float((std::max)(rows, 1u));
    return {{layout.group_tabs.position.x + float(column) * cell_width + gap * 0.5f,
             layout.group_tabs.position.y + float(row) * cell_height + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap), (std::max)(1.0f, cell_height - gap)}};
}

[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(
    const Layout& layout, MaterialGroup group, std::uint32_t index) noexcept {
    constexpr std::uint32_t columns = 2u;
    const auto count = (std::max)(material_group_size(group), 1u);
    const auto rows = (count + columns - 1u) / columns;
    const auto column = index % columns;
    const auto row = index / columns;
    const float cell_width = layout.palette.size.x / float(columns);
    const float cell_height = layout.palette.size.y / float((std::max)(rows, 1u));
    return {{layout.palette.position.x + float(column) * cell_width + gap * 0.5f,
             layout.palette.position.y + float(row) * cell_height + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap), (std::max)(1.0f, cell_height - gap)}};
}

[[nodiscard]] inline std::uint32_t group_at(const Layout& layout, epochengine::gui_lib::Vec2 point) noexcept {
    for (std::uint32_t index = 0u; index < material_group_count; ++index)
        if (epochengine::gui_lib::contains(group_tab_rect(layout, index), point)) return index;
    return material_group_count;
}

[[nodiscard]] inline std::uint32_t palette_slot_at(
    const Layout& layout, MaterialGroup group, epochengine::gui_lib::Vec2 point) noexcept {
    const auto count = material_group_size(group);
    for (std::uint32_t index = 0u; index < count; ++index)
        if (epochengine::gui_lib::contains(palette_item_rect(layout, group, index), point)) return index;
    return count;
}

[[nodiscard]] inline Material palette_material_at(
    const Layout& layout, MaterialGroup group, epochengine::gui_lib::Vec2 point) noexcept {
    const auto slot = palette_slot_at(layout, group, point);
    return slot < material_group_size(group) ? grouped_material(group, slot) : Material::count;
}
} // namespace epoch::sand::ui
''')

write("shaders/paint.comp", r'''#version 450
#extension GL_GOOGLE_include_directive : require
#include "materials.glsl"
#include "conservation.glsl"

layout(local_size_x = 16, local_size_y = 16) in;
layout(std430, binding = 0) buffer CurrentCells { Cell cells[]; };

void main() {
    ivec2 local = ivec2(gl_GlobalInvocationID.xy);
    int diameter = int(pc.radius * 2u + 1u);
    if (local.x >= diameter || local.y >= diameter) return;

    ivec2 center = ivec2(pc.brushX, pc.brushY);
    ivec2 p = center + local - ivec2(int(pc.radius));
    if (!inside(p)) return;

    uint material = pc.material & 0xffffu;
    uint brushShape = (pc.material >> 16u) & 3u;
    material = material < MATERIAL_COUNT ? material : MAT_EMPTY;

    if (isBlockCapable(material)) {
        ivec2 blockOrigin = (center / int(STRUCTURAL_BLOCK_SIZE)) * int(STRUCTURAL_BLOCK_SIZE);
        ivec2 blockEnd = blockOrigin + ivec2(int(STRUCTURAL_BLOCK_SIZE));
        if (p.x < blockOrigin.x || p.y < blockOrigin.y || p.x >= blockEnd.x || p.y >= blockEnd.y) return;

        bool anchored = blockEnd.y >= int(pc.height) - 1;
        Cell cell = makeStructuralCell(material, anchored);
        bool machineController = (material == MAT_SMELTER || material == MAT_ASSEMBLER ||
                                  material == MAT_INSECT_HABITAT) &&
                                 (p.x & 7) == 3 && (p.y & 7) == 3;
        cell.aux = (cell.aux & ~AUX_RANDOM_MASK) |
                   (cellHash(p, uint(local.x + local.y * diameter)) & AUX_RANDOM_MASK) |
                   (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                                AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED | AUX_STATE_MASK));
        if (machineController) {
            setMachineInventory(cell, uvec4(0u));
            cell.aux |= AUX_CHARGED;
        }
        uint index = indexOf(p);
        Cell previous = cells[index];
        recordConservation(previous, cell);
        cells[index] = cell;
        return;
    }

    // Seeds and queens are precision tools: brush size never duplicates them.
    if ((material == MAT_SEED || material == MAT_QUEEN_BEE) && any(notEqual(p, center))) return;

    ivec2 delta = p - center;
    int distanceSquared = delta.x * delta.x + delta.y * delta.y;
    int radius = int(pc.radius);
    bool insideBrush = false;
    if (brushShape == 0u) insideBrush = distanceSquared <= radius * radius;
    else if (brushShape == 1u) insideBrush = abs(delta.x) <= radius && abs(delta.y) <= radius;
    else if (brushShape == 2u) insideBrush = abs(delta.x) <= radius && abs(delta.y) <= 1;
    else insideBrush = abs(delta.x) <= 1 && abs(delta.y) <= radius;
    if (!insideBrush) return;

    Cell cell;
    if (material == MAT_BEE_NEST) {
        int nestRadius = max(radius, 4);
        int innerRadius = max(nestRadius - 3, 1);
        if (all(equal(p, center))) {
            cell = makeCell(MAT_QUEEN_BEE);
        } else if (delta.x >= 1 && delta.x <= nestRadius && abs(delta.y) <= 1) {
            cell = makeCell(MAT_EMPTY);
        } else if (distanceSquared <= innerRadius * innerRadius) {
            uint chamberPattern = cellHash(p, 0xb33u);
            if ((chamberPattern & 3u) == 0u) cell = makeCell(MAT_EMPTY);
            else cell = makeCell(((chamberPattern >> 2u) & 3u) == 0u ? MAT_POLLEN : MAT_HONEY);
        } else if (distanceSquared <= nestRadius * nestRadius) {
            cell = makeCell(MAT_BEE_NEST);
        } else {
            return;
        }
    } else {
        cell = makeCell(material);
    }

    cell.aux = (cell.aux & ~AUX_RANDOM_MASK) |
               (cellHash(p, uint(local.x + local.y * diameter)) & AUX_RANDOM_MASK) |
               (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                            AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED | AUX_STATE_MASK));
    uint index = indexOf(p);
    Cell previous = cells[index];
    recordConservation(previous, cell);
    cells[index] = cell;
}
''')

app = read("src/app.cpp")
app = one(app, '#include <mutex>\n#include <thread>\n', '#include <mutex>\n#include <thread>\n#include <utility>\n', 'app utility include')
app = one(app, 'namespace epoch::sand {\n\nint run_application() {', r'''namespace epoch::sand {

namespace {

struct CameraView final {
    std::uint32_t origin_x{};
    std::uint32_t origin_y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

[[nodiscard]] CameraView camera_view(const SharedState& state, const SimulationConfig& config,
                                     const std::uint32_t requested_zoom) noexcept {
    const auto zoom = std::clamp(requested_zoom, 1u, 8u);
    const auto visible_width = (std::max)(8u, config.grid_width / zoom);
    const auto visible_height = (std::max)(8u, config.grid_height / zoom);
    const auto center_x = std::clamp(state.camera_center_x.load(std::memory_order_relaxed),
                                     0, static_cast<int>(config.grid_width - 1u));
    const auto center_y = std::clamp(state.camera_center_y.load(std::memory_order_relaxed),
                                     0, static_cast<int>(config.grid_height - 1u));
    const auto max_x = config.grid_width - visible_width;
    const auto max_y = config.grid_height - visible_height;
    const auto origin_x = static_cast<std::uint32_t>(std::clamp(
        center_x - static_cast<int>(visible_width / 2u), 0, static_cast<int>(max_x)));
    const auto origin_y = static_cast<std::uint32_t>(std::clamp(
        center_y - static_cast<int>(visible_height / 2u), 0, static_cast<int>(max_y)));
    return {origin_x, origin_y, visible_width, visible_height};
}

[[nodiscard]] std::pair<std::int32_t, std::int32_t> pointer_grid(
    const SharedState& state, const SimulationConfig& config,
    const ui::SimulationViewport& viewport, const std::int32_t mouse_x,
    const std::int32_t mouse_y) noexcept {
    const auto zoom = state.camera_zoom.load(std::memory_order_relaxed);
    const auto view = camera_view(state, config, zoom);
    const auto viewport_width = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(mouse_x - static_cast<int>(viewport.rect.position.x),
                                    0, static_cast<int>(viewport_width - 1u));
    const auto local_y = std::clamp(mouse_y - static_cast<int>(viewport.rect.position.y),
                                    0, static_cast<int>(viewport_height - 1u));
    return {
        static_cast<std::int32_t>(view.origin_x +
            static_cast<std::uint64_t>(local_x) * view.width / viewport_width),
        static_cast<std::int32_t>(view.origin_y +
            static_cast<std::uint64_t>(local_y) * view.height / viewport_height),
    };
}

void zoom_at_pointer(SharedState& state, const SimulationConfig& config,
                     const ui::SimulationViewport& viewport, const std::int32_t mouse_x,
                     const std::int32_t mouse_y, const int delta) noexcept {
    const auto old_zoom = std::clamp(state.camera_zoom.load(std::memory_order_relaxed), 1u, 8u);
    const auto new_zoom = static_cast<std::uint32_t>(std::clamp(static_cast<int>(old_zoom) + delta, 1, 8));
    if (new_zoom == old_zoom) return;
    const auto [target_x, target_y] = pointer_grid(state, config, viewport, mouse_x, mouse_y);
    const auto viewport_width = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(mouse_x - static_cast<int>(viewport.rect.position.x),
                                    0, static_cast<int>(viewport_width - 1u));
    const auto local_y = std::clamp(mouse_y - static_cast<int>(viewport.rect.position.y),
                                    0, static_cast<int>(viewport_height - 1u));
    const auto new_width = (std::max)(8u, config.grid_width / new_zoom);
    const auto new_height = (std::max)(8u, config.grid_height / new_zoom);
    const auto desired_origin_x = target_x - static_cast<int>(
        static_cast<std::uint64_t>(local_x) * new_width / viewport_width);
    const auto desired_origin_y = target_y - static_cast<int>(
        static_cast<std::uint64_t>(local_y) * new_height / viewport_height);
    const auto origin_x = std::clamp(desired_origin_x, 0, static_cast<int>(config.grid_width - new_width));
    const auto origin_y = std::clamp(desired_origin_y, 0, static_cast<int>(config.grid_height - new_height));
    state.camera_zoom.store(new_zoom, std::memory_order_relaxed);
    state.camera_center_x.store(origin_x + static_cast<int>(new_width / 2u), std::memory_order_relaxed);
    state.camera_center_y.store(origin_y + static_cast<int>(new_height / 2u), std::memory_order_relaxed);
}

} // namespace

int run_application() {''', 'app camera helpers')
app = one(app, '    const SimulationConfig simulation_config{};\n    std::atomic_bool renderer_ready{false};', '    const SimulationConfig simulation_config{};\n    shared_state.camera_center_x.store(static_cast<int>(simulation_config.grid_width / 2u), std::memory_order_relaxed);\n    shared_state.camera_center_y.store(static_cast<int>(simulation_config.grid_height / 2u), std::memory_order_relaxed);\n    std::atomic_bool renderer_ready{false};', 'app camera init')
app = one(app, '''        const bool primary_pressed = input.primary_pressed;
        const bool secondary_pressed = input.secondary_pressed;
''', '''        const bool primary_pressed = input.primary_pressed;
        const bool secondary_pressed = input.secondary_pressed;
        const bool over_simulation = epochengine::gui_lib::contains(simulation_viewport.rect, pointer);
        if (input.wheel_delta != 0 && over_simulation)
            zoom_at_pointer(shared_state, simulation_config, simulation_viewport,
                            input.mouse_x, input.mouse_y, input.wheel_delta);

        const auto adjust_zoom_centered = [&shared_state](const int delta) {
            const auto current = static_cast<int>(shared_state.camera_zoom.load(std::memory_order_relaxed));
            shared_state.camera_zoom.store(static_cast<std::uint32_t>(std::clamp(current + delta, 1, 8)),
                                           std::memory_order_relaxed);
        };
''', 'app wheel zoom')
app = one(app, '''            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {
                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);
                shared_state.debug_visualization.store(!debug, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {
''', '''            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {
                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);
                shared_state.debug_visualization.store(!debug, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.cursor_circle, pointer)) {
                shared_state.brush_shape.store(0u, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.cursor_square, pointer)) {
                shared_state.brush_shape.store(1u, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.cursor_horizontal, pointer)) {
                shared_state.brush_shape.store(2u, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.cursor_vertical, pointer)) {
                shared_state.brush_shape.store(3u, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.brush_smaller, pointer)) {
                const auto radius = static_cast<int>(shared_state.brush_radius.load(std::memory_order_relaxed));
                shared_state.brush_radius.store(static_cast<std::uint32_t>(std::clamp(radius - 1, 1, 48)),
                                                std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.brush_larger, pointer)) {
                const auto radius = static_cast<int>(shared_state.brush_radius.load(std::memory_order_relaxed));
                shared_state.brush_radius.store(static_cast<std::uint32_t>(std::clamp(radius + 1, 1, 48)),
                                                std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.zoom_out, pointer)) {
                adjust_zoom_centered(-1);
            } else if (epochengine::gui_lib::contains(layout.zoom_in, pointer)) {
                adjust_zoom_centered(1);
            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {
''', 'app cursor editor clicks')
app = one(app, '        const bool over_simulation = epochengine::gui_lib::contains(simulation_viewport.rect, pointer);\n        const bool mining =', '        const bool mining =', 'app duplicate over simulation')
app = rx(app, r'''\n        if \(input\.wheel_delta != 0 && over_simulation\) \{\n            const auto current = static_cast<int>\(shared_state\.brush_radius\.load\(std::memory_order_relaxed\)\);\n            const auto next = std::clamp\(current \+ input\.wheel_delta, 1, 48\);\n            shared_state\.brush_radius\.store\(static_cast<std::uint32_t>\(next\), std::memory_order_relaxed\);\n        \}\n''', '\n', 'remove old wheel brush')
write("src/app.cpp", app)

renderer = read("src/vulkan_renderer.cpp")
renderer = one(renderer, '''    std::uint32_t viewport_width{};
    std::uint32_t viewport_height{};
};
static_assert(sizeof(RenderPush) == 124);
''', '''    std::uint32_t viewport_width{};
    std::uint32_t viewport_height{};
    std::uint32_t view_origin_x{};
    std::uint32_t view_origin_y{};
    std::uint32_t view_width{};
    std::uint32_t view_height{};
    std::uint32_t brush_shape{};
};
static_assert(sizeof(RenderPush) == 144);
''', 'renderer render push')
renderer = rx(renderer, r'''    std::pair<std::int32_t, std::int32_t> grid_cursor\(const SharedState& state\) const \{.*?\n    \}\n\n    void record_paint''', r'''    struct GridView final {
        std::uint32_t origin_x{};
        std::uint32_t origin_y{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    [[nodiscard]] GridView grid_view(const SharedState& state) const {
        const auto zoom = std::clamp(state.camera_zoom.load(std::memory_order_relaxed), 1u, 8u);
        const auto visible_width = (std::max)(8u, config.grid_width / zoom);
        const auto visible_height = (std::max)(8u, config.grid_height / zoom);
        const auto center_x = std::clamp(state.camera_center_x.load(std::memory_order_relaxed),
                                         0, static_cast<int>(config.grid_width - 1u));
        const auto center_y = std::clamp(state.camera_center_y.load(std::memory_order_relaxed),
                                         0, static_cast<int>(config.grid_height - 1u));
        const auto origin_x = static_cast<std::uint32_t>(std::clamp(
            center_x - static_cast<int>(visible_width / 2u), 0,
            static_cast<int>(config.grid_width - visible_width)));
        const auto origin_y = static_cast<std::uint32_t>(std::clamp(
            center_y - static_cast<int>(visible_height / 2u), 0,
            static_cast<int>(config.grid_height - visible_height)));
        return {origin_x, origin_y, visible_width, visible_height};
    }

    std::pair<std::int32_t, std::int32_t> grid_cursor(const SharedState& state) const {
        const auto width = (std::max)(state.window_width.load(std::memory_order_relaxed), 1u);
        const auto height = (std::max)(state.window_height.load(std::memory_order_relaxed), 1u);
        const auto layout = ui::make_layout(width, height);
        const auto viewport = ui::make_simulation_viewport(layout, config.grid_width, config.grid_height);
        const auto viewport_width = (std::max)(static_cast<std::uint32_t>(viewport.rect.size.x), 1u);
        const auto viewport_height = (std::max)(static_cast<std::uint32_t>(viewport.rect.size.y), 1u);
        const auto mouse_x = std::clamp(
            state.mouse_x.load(std::memory_order_relaxed) - static_cast<int>(viewport.rect.position.x),
            0, static_cast<int>(viewport_width - 1u));
        const auto mouse_y = std::clamp(
            state.mouse_y.load(std::memory_order_relaxed) - static_cast<int>(viewport.rect.position.y),
            0, static_cast<int>(viewport_height - 1u));
        const auto view = grid_view(state);
        const auto grid_x = static_cast<std::int32_t>(view.origin_x +
            static_cast<std::uint64_t>(mouse_x) * view.width / viewport_width);
        const auto grid_y = static_cast<std::int32_t>(view.origin_y +
            static_cast<std::uint64_t>(mouse_y) * view.height / viewport_height);
        return {grid_x, grid_y};
    }

    void record_paint''', 'renderer camera cursor')
renderer = one(renderer, '''        SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .brush_x = grid_x,
            .brush_y = grid_y,
            .radius = radius,
            .material = material,
        };
''', '''        const auto shape = is_block_material(selected)
            ? 1u : state.brush_shape.load(std::memory_order_relaxed) % 4u;
        const auto packed_material = material | (shape << 16u);
        SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .brush_x = grid_x,
            .brush_y = grid_y,
            .radius = radius,
            .material = packed_material,
        };
''', 'renderer packed brush shape')
renderer = one(renderer, '''        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, config.grid_width, config.grid_height);
''', '''        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, config.grid_width, config.grid_height);
        const auto view = grid_view(state);
''', 'renderer render view')
renderer = one(renderer, '''            .viewport_width = static_cast<std::uint32_t>(simulation_viewport.rect.size.x),
            .viewport_height = static_cast<std::uint32_t>(simulation_viewport.rect.size.y),
        };
''', '''            .viewport_width = static_cast<std::uint32_t>(simulation_viewport.rect.size.x),
            .viewport_height = static_cast<std::uint32_t>(simulation_viewport.rect.size.y),
            .view_origin_x = view.origin_x,
            .view_origin_y = view.origin_y,
            .view_width = view.width,
            .view_height = view.height,
            .brush_shape = [&state]() {
                const auto material_id = state.selected_material.load(std::memory_order_relaxed);
                const auto material = static_cast<Material>(material_id < material_count ? material_id : 0u);
                return is_block_material(material) ? 1u : state.brush_shape.load(std::memory_order_relaxed) % 4u;
            }(),
        };
''', 'renderer push camera')
write("src/vulkan_renderer.cpp", renderer)

fullscreen = read("shaders/fullscreen.frag")
fullscreen = one(fullscreen, '''    uint viewportWidth;
    uint viewportHeight;
} renderPc;
''', '''    uint viewportWidth;
    uint viewportHeight;
    uint viewOriginX;
    uint viewOriginY;
    uint viewWidth;
    uint viewHeight;
    uint brushShape;
} renderPc;
''', 'fullscreen render push')
fullscreen = one(fullscreen, '        uint keymapBottom = keymapTop + 100u;', '        uint keymapBottom = keymapTop + 76u;', 'fullscreen keymap height')
fullscreen = one(fullscreen, '''        uint cardTop = keymapBottom + 3u;
        uint actorPanel = actor.enabled != 0u ? 102u : 5u;
''', r'''        uint cursorTop = keymapBottom + 3u;
        uint cursorBottom = cursorTop + 92u;
        if (y >= cursorTop && y < cursorBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, cursorTop, contentLeft + contentWidth, cursorBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool cursorText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(cursorTop + 5u)), 2, 99u);
            uint shapeTop = cursorTop + 23u;
            uint shapeWidth = max(contentWidth / 4u, 1u);
            uint shapeIds[4] = uint[4](100u, 101u, 102u, 103u);
            for (uint shape = 0u; shape < 4u; ++shape) {
                uint left = contentLeft + shape * shapeWidth;
                uint right = shape == 3u ? contentLeft + contentWidth : left + shapeWidth;
                if (x >= left && x < right && y >= shapeTop && y < shapeTop + 24u) {
                    color = shape == renderPc.brushShape ? vec3(0.14, 0.30, 0.45) : vec3(0.055, 0.07, 0.09);
                    if (borderPixel(x, y, left, shapeTop, right, shapeTop + 24u)) color *= 0.55;
                }
                int labelWidth = int(fixedTextLength(shapeIds[shape])) * 6 - 1;
                cursorText = cursorText || fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2, int(shapeTop + 8u)), 1, shapeIds[shape]);
            }
            uint controlTop = cursorTop + 60u;
            uint half = contentWidth / 2u;
            cursorText = cursorText || fixedPixel(pixel, ivec2(int(contentLeft + 39u), int(controlTop + 2u)), 1, 104u) ||
                numberPixel(pixel, ivec2(int(contentLeft + half / 2u - 8u), int(controlTop + 13u)), 1, renderPc.brushRadius) ||
                fixedPixel(pixel, ivec2(int(contentLeft + half + 39u), int(controlTop + 2u)), 1, 105u) ||
                numberPixel(pixel, ivec2(int(contentLeft + half + half / 2u - 8u), int(controlTop + 13u)), 1,
                            max(renderPc.gridWidth / max(renderPc.viewWidth, 1u), 1u));
            bool minusLeft = glyphPixel(pixel, ivec2(int(contentLeft + 13u), int(controlTop + 9u)), 2, 45u);
            bool plusLeft = glyphPixel(pixel, ivec2(int(contentLeft + half - 25u), int(controlTop + 9u)), 2, 43u);
            bool minusRight = glyphPixel(pixel, ivec2(int(contentLeft + half + 13u), int(controlTop + 9u)), 2, 45u);
            bool plusRight = glyphPixel(pixel, ivec2(int(contentLeft + contentWidth - 25u), int(controlTop + 9u)), 2, 43u);
            if (minusLeft || plusLeft || minusRight || plusRight) cursorText = true;
            if (cursorText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        uint cardTop = cursorBottom + 3u;
        uint actorPanel = actor.enabled != 0u ? 102u : 5u;
''', 'fullscreen cursor panel')
fullscreen = one(fullscreen, '''    uint gridX = min(renderPc.gridWidth - 1u,
                      simulationX * renderPc.gridWidth / max(renderPc.viewportWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u,
                      simulationY * renderPc.gridHeight / simulationHeight);
''', '''    uint gridX = min(renderPc.gridWidth - 1u, renderPc.viewOriginX +
                      simulationX * max(renderPc.viewWidth, 1u) / max(renderPc.viewportWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u, renderPc.viewOriginY +
                      simulationY * max(renderPc.viewHeight, 1u) / simulationHeight);
''', 'fullscreen camera mapping')
fullscreen = one(fullscreen, '        if (local.x == 0 || local.y == 0) color.rgb *= 0.45;', '        if (local.x == 0 || local.y == 0) color.rgb *= renderPc.viewWidth < renderPc.gridWidth ? 0.72 : 0.88;', 'fullscreen subtle grid')
fullscreen = one(fullscreen, '''        } else {
            int outer = int(renderPc.brushRadius * renderPc.brushRadius);
            int innerRadius = max(int(renderPc.brushRadius) - 1, 0);
            if (distanceSquared <= outer && distanceSquared >= innerRadius * innerRadius)
                color.rgb = vec3(1.0) - color.rgb;
        }
''', '''        } else {
            int radius = int(renderPc.brushRadius);
            bool cursorEdge = false;
            if (renderPc.brushShape == 0u) {
                int outer = radius * radius;
                int innerRadius = max(radius - 1, 0);
                cursorEdge = distanceSquared <= outer && distanceSquared >= innerRadius * innerRadius;
            } else if (renderPc.brushShape == 1u) {
                cursorEdge = max(abs(delta.x), abs(delta.y)) == radius;
            } else if (renderPc.brushShape == 2u) {
                cursorEdge = abs(delta.x) <= radius && abs(delta.y) <= 1;
            } else {
                cursorEdge = abs(delta.x) <= 1 && abs(delta.y) <= radius;
            }
            if (cursorEdge) color.rgb = vec3(1.0) - color.rgb;
        }
''', 'fullscreen brush cursor shape')
write("shaders/fullscreen.frag", fullscreen)

move = read("shaders/move.comp")
move = one(move, 'const uint AUX_BEE_FED = 0x10000000u;\nconst uint AUX_STRUCTURAL', 'const uint AUX_BEE_FED = 0x10000000u;\nconst uint AUX_BEE_SWARM = 0x08000000u;\nconst uint AUX_STRUCTURAL', 'move swarm flag')
move = rx(move, r'''bool beeMoveAllowed\(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue\) \{.*?\n\}\n\nbool magneticMoveAllowed''', r'''bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;

    ivec2 delta = targetPosition - sourcePosition;
    if ((bee.aux & AUX_BEE_SWARM) != 0u) {
        int sourceNest = regionalTargetSignal(sourcePosition, MAT_QUEEN_BEE, MAT_BEE_NEST);
        int targetNest = regionalTargetSignal(targetPosition, MAT_QUEEN_BEE, MAT_BEE_NEST);
        uint lane = (bee.aux >> 8u) % 3u;
        int low = lane == 0u ? 3600 : (lane == 1u ? 1250 : 320);
        int high = lane == 0u ? 9000 : (lane == 1u ? 4200 : 1700);
        if (sourceNest < low && targetNest != sourceNest) return targetNest > sourceNest;
        if (sourceNest > high && targetNest != sourceNest) return targetNest < sourceNest;
        if (targetNest < low || targetNest > high) return false;

        int east = regionalTargetSignal(sourcePosition + ivec2(1, 0), MAT_QUEEN_BEE, MAT_BEE_NEST);
        int west = regionalTargetSignal(sourcePosition + ivec2(-1, 0), MAT_QUEEN_BEE, MAT_BEE_NEST);
        int south = regionalTargetSignal(sourcePosition + ivec2(0, 1), MAT_QUEEN_BEE, MAT_BEE_NEST);
        int north = regionalTargetSignal(sourcePosition + ivec2(0, -1), MAT_QUEEN_BEE, MAT_BEE_NEST);
        ivec2 gradient = ivec2(sign(east - west), sign(south - north));
        bool clockwise = ((bee.aux >> 10u) & 1u) != 0u;
        ivec2 tangent = clockwise ? ivec2(-gradient.y, gradient.x)
                                  : ivec2(gradient.y, -gradient.x);
        int tangentScore = dot(delta, tangent) * 8;
        int waveVertical = beeWaveVertical(bee, sourcePosition);
        int waveHorizontal = beeWaveHorizontal(bee, sourcePosition);
        if (delta.y == waveVertical) tangentScore += 3;
        if (delta.x == waveHorizontal) tangentScore += 2;
        return tangentScore > 0 || (tangentScore == 0 && (randomValue & 63u) == 0u);
    }

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

bool magneticMoveAllowed''', 'move orbit controller')
write("shaders/move.comp", move)

reset = read("shaders/reset.comp")
reset = one(reset, '''        if ((innerWave || middleWave || outerWave) && (swarm & 3u) != 0u)
            material = MAT_BEE;
        else if (radius < 58.0 && (swarm % 53u) == 0u)
            material = MAT_BEE;
''', '''        if ((innerWave || middleWave || outerWave) && (swarm & 7u) == 0u)
            material = MAT_BEE;
        else if (radius < 58.0 && (swarm % 211u) == 0u)
            material = MAT_BEE;
''', 'reset swarm population')
reset = one(reset, '''    cell.aux = (cell.aux & ~AUX_RANDOM_MASK) | (hash32(indexOf(p) ^ pc.seed) & AUX_RANDOM_MASK) |
               (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                            AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED |
                            AUX_STATE_MASK));

    // Machine controllers''', '''    cell.aux = (cell.aux & ~AUX_RANDOM_MASK) | (hash32(indexOf(p) ^ pc.seed) & AUX_RANDOM_MASK) |
               (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                            AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED |
                            AUX_STATE_MASK));

    if (scene == SCENE_ECOSYSTEM && material == MAT_BEE) {
        ivec2 queen = ivec2(int(pc.width) - 104, int(pc.height) - 10 - 54 - 72);
        ivec2 q = p - queen;
        float radius = length(vec2(q));
        float angle = atan(float(q.y), float(q.x));
        float wavePhase = angle * 3.0 + radius * 0.14;
        bool authoredWave = abs(radius - (18.0 + sin(wavePhase) * 3.5)) < 1.15 ||
                            abs(radius - (30.0 + sin(wavePhase + 1.7) * 5.0)) < 1.25 ||
                            abs(radius - (44.0 + sin(wavePhase + 3.2) * 7.0)) < 1.35;
        if (authoredWave) {
            cell.aux |= AUX_PLANT_STEM | AUX_BEE_FED;
            setStateValue(cell, 255u);
        }
    }

    // Machine controllers''', 'reset mark swarm bees')
write("shaders/reset.comp", reset)

chem = read("shaders/chemistry.comp")
chem = one(chem, '''                if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared >= 7 &&
                    queenDistanceSquared <= 21 && (randomValue & 7u) != 0u) {
                    result = makeCell(MAT_BEE_NEST);
                    handledNest = true;
                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            countWithin(p, MAT_BEE, 6) < 12u && (randomValue & 4095u) == 0u) {
                    result = makeCell(MAT_BEE);
                    handledNest = true;
                }
''', '''                if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared >= 7 &&
                    queenDistanceSquared <= 21 && countWithin(p, MAT_BEE_NEST, 5) < 36u &&
                    (randomValue & 4095u) == 0u) {
                    result = makeCell(MAT_BEE_NEST);
                    handledNest = true;
                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            countWithin(p, MAT_BEE, 6) < 4u && (randomValue & 32767u) == 0u) {
                    result = makeCell(MAT_BEE);
                    handledNest = true;
                }
''', 'chem bounded hive growth')
chem = rx(chem, r'''    // Bee cycle:.*?    \} else if \(source\.material == MAT_POLLEN\) \{''', r'''    // Bee cycle: authored swarm bees remain in the three orbit lanes; worker
    // bees forage, carry pollen home, feed, rest, and repeat without consuming flowers.
    if (source.material == MAT_BEE) {
        bool swarmBee = (source.aux & AUX_PLANT_STEM) != 0u;
        uint restTimer = stateValue(source);
        if (!swarmBee && restTimer > 0u) setStateValue(result, restTimer - 1u);

        if (nearFire || nearLava || hasNeighbor(p, MAT_LIGHTNING)) {
            result = makeCell(MAT_ASH);
        } else if (nearAcid && (randomValue & 3u) == 0u) {
            result = makeCell(MAT_WASTE);
        } else if ((nearSmoke || hasNeighbor(p, MAT_DIRTY_STEAM) || hasNeighbor(p, MAT_RADIATION)) &&
                   (randomValue & 127u) == 0u) {
            result = makeCell(MAT_WASTE);
        } else if (swarmBee) {
            result.aux |= AUX_PLANT_STEM | AUX_BEE_FED;
            result.aux &= ~AUX_BEE_POLLEN;
            setStateValue(result, 255u);
        } else {
            if ((source.aux & AUX_BEE_POLLEN) == 0u && hasNeighbor(p, MAT_FLOWER)) {
                result.aux |= AUX_BEE_POLLEN;
                result.aux &= ~AUX_BEE_FED;
                setStateValue(result, 0u);
                result.age = min(result.age, 1200u);
            }
            if ((source.aux & AUX_BEE_POLLEN) != 0u &&
                (hasNeighbor(p, MAT_QUEEN_BEE) || hasNeighbor(p, MAT_BEE_NEST))) {
                ivec2 target = beeDepositTarget(p, source);
                uint targetMaterial = at(target).material;
                if (targetMaterial == MAT_EMPTY || targetMaterial == MAT_POLLEN)
                    result.aux &= ~AUX_BEE_POLLEN;
            }
            if ((result.aux & AUX_BEE_POLLEN) == 0u && hasNeighbor(p, MAT_HONEY)) {
                result.aux |= AUX_BEE_FED;
                setStateValue(result, 240u);
                result.age = min(result.age, 600u);
            }
            if (source.age > 220000u && (randomValue & 8191u) == 0u)
                result = makeCell(MAT_WASTE);
        }
    } else if (source.material == MAT_POLLEN) {''', 'chem swarm lifecycle')
chem = one(chem, 'if (hasHungryBee(p) && (randomValue & 2047u) == 0u)', 'if (hasHungryBee(p) && (randomValue & 8191u) == 0u)', 'chem slow honey use')
write("shaders/chemistry.comp", chem)

gen = read("tools/generate_ui_text.py")
gen = one(gen, '"SAVE", "LOAD", "ERASER", "KEYMAP", "WHEEL BRUSH", "N STEP", "R RESET",', '"SAVE", "LOAD", "ERASER", "KEYMAP", "WHEEL ZOOM", "N STEP", "R RESET",', 'ui wheel label')
gen = one(gen, '"BEES", "BEE MOVES", "QUEENS", "NESTS", "FLOWERS", "HONEY", "ANTS",', '"BEES", "BEE MOVES", "QUEENS", "HIVE CELLS", "FLOWERS", "HONEY", "ANTS",', 'ui hive label')
gen = one(gen, '    "STRUCT", "LIQUID", "GAS", "POLLEN", "ACTIVE TILES",\n]', '    "STRUCT", "LIQUID", "GAS", "POLLEN", "ACTIVE TILES",\n    "CURSOR", "CIRCLE", "SQUARE", "H LINE", "V LINE", "SIZE", "ZOOM", "BEE ACTIVE",\n]', 'ui cursor labels')
write("tools/generate_ui_text.py", gen)

print("Fix29 camera, cursor editor, and bee controller applied.")
