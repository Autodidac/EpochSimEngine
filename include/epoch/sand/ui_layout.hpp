#pragma once
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
