#pragma once
#include "sandhybrid/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>

namespace sandhybrid::ui {
inline constexpr std::uint32_t preferred_sidebar_width = 424u;
inline constexpr std::uint32_t minimum_sidebar_width = 320u;
inline constexpr std::uint32_t status_height = 208u;
inline constexpr std::uint32_t workspace_tab_count = 4u;
inline constexpr std::uint32_t group_tabs_height = 96u;
inline constexpr std::uint32_t palette_items_height = 124u;
inline constexpr std::uint32_t eraser_height = 34u;
inline constexpr std::uint32_t keymap_height = 124u;
inline constexpr std::uint32_t cursor_editor_height = 112u;
inline constexpr std::uint32_t palette_height = 0u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;

struct Layout final {
    epochengine::gui_lib::Rect status{}, simulation{}, group_tabs{}, palette{};
    epochengine::gui_lib::Rect workspace_inventory{}, workspace_editor{}, workspace_settings{}, workspace_designer{};
    epochengine::gui_lib::Rect previous_scene{}, next_scene{}, reset_scene{}, save_scene{}, load_scene{};
    epochengine::gui_lib::Rect mode_toggle{}, pause_toggle{}, camera_controls_toggle{}, map_toggle{}, debug_toggle{};
    epochengine::gui_lib::Rect atmosphere{}, fill{}, eraser{}, keymap{}, cursor_editor{}, material_card{};
    epochengine::gui_lib::Rect placement_cells{}, placement_tiles{};
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
    if (panel_width < tile_columns || panel_height < tile_rows) {
        const auto safe_width = (std::max)(grid_width, 1u);
        const auto safe_height = (std::max)(grid_height, 1u);
        std::uint32_t viewport_width = panel_width;
        std::uint32_t viewport_height = panel_height;
        if (static_cast<std::uint64_t>(panel_width) * safe_height <=
            static_cast<std::uint64_t>(panel_height) * safe_width) {
            viewport_height = (std::max)(1u, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(panel_width) * safe_height / safe_width));
        } else {
            viewport_width = (std::max)(1u, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(panel_height) * safe_width / safe_height));
        }
        const auto left = layout.simulation.position.x +
                          float((panel_width - viewport_width) / 2u);
        const auto top = layout.simulation.position.y +
                         float((panel_height - viewport_height) / 2u);
        return {{{left, top}, {float(viewport_width), float(viewport_height)}}, 0u};
    }
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
    const float content_left = left + margin;
    const float content_width = (std::max)(1.0f, side - margin * 2.0f);

    Layout layout{
        .status = {{left, 0.0f}, {side, float(status_height)}},
        .simulation = {{0.0f, 0.0f}, {float(simulation_width), float(screen_height)}},
        .group_tabs = {{content_left, float(status_height) + margin},
                       {content_width, float(group_tabs_height)}},
        .palette = {{content_left, float(status_height + group_tabs_height) + margin + gap},
                    {content_width, float(palette_items_height)}},
    };

    const float workspace_left = left + 8.0f;
    const float workspace_width = (std::max)(1.0f, side - 16.0f);
    const float workspace_cell = workspace_width / float(workspace_tab_count);
    layout.workspace_inventory = {{workspace_left, 4.0f}, {workspace_cell, 24.0f}};
    layout.workspace_editor = {{workspace_left + workspace_cell, 4.0f}, {workspace_cell, 24.0f}};
    layout.workspace_settings = {{workspace_left + workspace_cell * 2.0f, 4.0f}, {workspace_cell, 24.0f}};
    layout.workspace_designer = {{workspace_left + workspace_cell * 3.0f, 4.0f}, {workspace_width - workspace_cell * 3.0f, 24.0f}};

    constexpr float row_left_padding = 8.0f;
    constexpr float row_right_padding = 8.0f;
    constexpr float row_gap = 4.0f;
    const float row_left = left + row_left_padding;
    const float row_width = (std::max)(1.0f, side - row_left_padding - row_right_padding);

    // Scene files/navigation: PREV, NEXT, SAVE, LOAD.
    const float scene_width = (std::max)(1.0f, (row_width - row_gap * 3.0f) / 4.0f);
    layout.previous_scene = {{row_left, 70.0f}, {scene_width, 28.0f}};
    layout.next_scene = {{row_left + (scene_width + row_gap), 70.0f}, {scene_width, 28.0f}};
    layout.save_scene = {{row_left + (scene_width + row_gap) * 2.0f, 70.0f}, {scene_width, 28.0f}};
    layout.load_scene = {{row_left + (scene_width + row_gap) * 3.0f, 70.0f},
                         {row_width - scene_width * 3.0f - row_gap * 3.0f, 28.0f}};

    // Simulation actions are intentionally paired and equally prominent.
    const float action_width = (std::max)(1.0f, (row_width - row_gap) * 0.5f);
    layout.reset_scene = {{row_left, 102.0f}, {action_width, 30.0f}};
    layout.pause_toggle = {{row_left + action_width + row_gap, 102.0f},
                           {row_width - action_width - row_gap, 30.0f}};

    // View/input modes: MINE/BUILD, PLAYER/WASD PAN, MAP, DEBUG.
    const float top_control_width = (std::max)(1.0f, (row_width - row_gap * 3.0f) / 4.0f);
    layout.mode_toggle = {{row_left, 136.0f}, {top_control_width, 28.0f}};
    layout.camera_controls_toggle = {{row_left + (top_control_width + row_gap), 136.0f},
                                     {top_control_width, 28.0f}};
    layout.map_toggle = {{row_left + (top_control_width + row_gap) * 2.0f, 136.0f},
                         {top_control_width, 28.0f}};
    layout.debug_toggle = {{row_left + (top_control_width + row_gap) * 3.0f, 136.0f},
                           {row_width - top_control_width * 3.0f - row_gap * 3.0f, 28.0f}};

    // Primary tools stay visible above the material browser.
    const float utility_width = (std::max)(1.0f, (row_width - row_gap * 2.0f) / 3.0f);
    layout.atmosphere = {{row_left, 168.0f}, {utility_width, float(eraser_height)}};
    layout.eraser = {{row_left + utility_width + row_gap, 168.0f},
                     {utility_width, float(eraser_height)}};
    layout.fill = {{row_left + (utility_width + row_gap) * 2.0f, 168.0f},
                   {row_width - utility_width * 2.0f - row_gap * 2.0f,
                    float(eraser_height)}};

    const float keymap_top = layout.palette.position.y + layout.palette.size.y + gap;
    layout.keymap = {{content_left, keymap_top}, {content_width, float(keymap_height)}};
    const float cursor_top = keymap_top + float(keymap_height) + gap;
    layout.cursor_editor = {{content_left, cursor_top},
                            {content_width, float(cursor_editor_height)}};

    const float placement_top = cursor_top + 23.0f;
    const float placement_width = content_width / 2.0f;
    layout.placement_cells = {{content_left, placement_top}, {placement_width, 26.0f}};
    layout.placement_tiles = {{content_left + placement_width, placement_top},
                              {content_width - placement_width, 26.0f}};

    const float shape_top = cursor_top + 53.0f;
    const float shape_width = content_width / 4.0f;
    layout.cursor_circle = {{content_left, shape_top}, {shape_width, 24.0f}};
    layout.cursor_square = {{content_left + shape_width, shape_top}, {shape_width, 24.0f}};
    layout.cursor_horizontal = {{content_left + shape_width * 2.0f, shape_top}, {shape_width, 24.0f}};
    layout.cursor_vertical = {{content_left + shape_width * 3.0f, shape_top}, {shape_width, 24.0f}};

    const float control_top = cursor_top + 85.0f;
    const float half = content_width / 2.0f;
    constexpr float control_button_width = 44.0f;
    layout.brush_smaller = {{content_left + 4.0f, control_top}, {control_button_width, 24.0f}};
    layout.brush_larger = {{content_left + half - control_button_width - 4.0f, control_top},
                           {control_button_width, 24.0f}};
    layout.zoom_out = {{content_left + half + 4.0f, control_top}, {control_button_width, 24.0f}};
    layout.zoom_in = {{content_left + content_width - control_button_width - 4.0f, control_top},
                      {control_button_width, 24.0f}};

    const float card_top = cursor_top + float(cursor_editor_height) + gap;
    layout.material_card = {{content_left, card_top},
                            {content_width, (std::max)(1.0f, float(screen_height) - card_top - margin)}};
    return layout;
}

[[nodiscard]] inline constexpr std::uint32_t workspace_at(
    const Layout& layout, const epochengine::gui_lib::Vec2 point) noexcept {
    if (epochengine::gui_lib::contains(layout.workspace_inventory, point)) return 0u;
    if (epochengine::gui_lib::contains(layout.workspace_editor, point)) return 1u;
    if (epochengine::gui_lib::contains(layout.workspace_settings, point)) return 2u;
    if (epochengine::gui_lib::contains(layout.workspace_designer, point)) return 3u;
    return workspace_tab_count;
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

inline constexpr MaterialGroup ignite_air_group = MaterialGroup::fire_chemistry;

[[nodiscard]] inline constexpr std::uint32_t palette_item_count(
    const MaterialGroup group) noexcept {
    return material_group_size(group) + (group == ignite_air_group ? 1u : 0u);
}

[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(
    const Layout& layout, MaterialGroup group, std::uint32_t index) noexcept {
    constexpr std::uint32_t columns = 2u;
    const auto count = (std::max)(palette_item_count(group), 1u);
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
    const auto count = palette_item_count(group);
    for (std::uint32_t index = 0u; index < count; ++index)
        if (epochengine::gui_lib::contains(palette_item_rect(layout, group, index), point)) return index;
    return count;
}

[[nodiscard]] inline Material palette_material_at(
    const Layout& layout, MaterialGroup group, epochengine::gui_lib::Vec2 point) noexcept {
    const auto slot = palette_slot_at(layout, group, point);
    return slot < material_group_size(group) ? grouped_material(group, slot) : Material::count;
}

[[nodiscard]] inline bool ignite_air_action_at(
    const Layout& layout, const MaterialGroup group,
    const epochengine::gui_lib::Vec2 point) noexcept {
    return group == ignite_air_group &&
           palette_slot_at(layout, group, point) == material_group_size(group);
}

[[nodiscard]] inline epochengine::gui_lib::Rect inventory_slot_rect(
    const Layout& layout, const std::uint32_t window_height,
    const std::uint32_t index) noexcept {
    constexpr float slot_gap = 3.0f;
    constexpr float slot_height = 37.0f;
    const float left = layout.status.position.x + margin;
    const float width = (std::max)(1.0f, layout.status.size.x - margin * 2.0f);
    const float top = (std::max)(0.0f, static_cast<float>(window_height) - 88.0f);
    const float slot_width = (width - slot_gap) * 0.5f;
    const auto column = index % 2u;
    const auto row = index / 2u;
    return {{left + static_cast<float>(column) * (slot_width + slot_gap),
             top + static_cast<float>(row) * (slot_height + slot_gap)},
            {slot_width, slot_height}};
}

[[nodiscard]] inline std::uint32_t inventory_slot_at(
    const Layout& layout, const std::uint32_t window_height,
    const epochengine::gui_lib::Vec2 point) noexcept {
    for (std::uint32_t index = 0u; index < 4u; ++index)
        if (epochengine::gui_lib::contains(inventory_slot_rect(layout, window_height, index), point))
            return index;
    return 4u;
}
} // namespace sandhybrid::ui
