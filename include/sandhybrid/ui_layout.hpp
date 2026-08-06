#pragma once
#include "sandhybrid/blueprint.hpp"
#include "sandhybrid/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>
#include <utility>

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
    epochengine::gui_lib::Rect inventory_inventory{}, inventory_blueprints{};
    epochengine::gui_lib::Rect previous_scene{}, next_scene{}, reset_scene{}, save_scene{}, load_scene{};
    epochengine::gui_lib::Rect mode_toggle{}, pause_toggle{}, camera_controls_toggle{}, map_toggle{}, debug_toggle{};
    epochengine::gui_lib::Rect atmosphere{}, fill{}, eraser{}, keymap{}, cursor_editor{}, material_card{};
    epochengine::gui_lib::Rect placement_cells{}, placement_tiles{};
    epochengine::gui_lib::Rect designer_static_model{}, designer_map_chunk{};
    epochengine::gui_lib::Rect designer_inventory{}, designer_blueprints{};
    epochengine::gui_lib::Rect designer_grid{}, designer_material_card{};
    epochengine::gui_lib::Rect cursor_circle{}, cursor_square{}, cursor_horizontal{}, cursor_vertical{};
    epochengine::gui_lib::Rect brush_smaller{}, brush_larger{}, zoom_out{}, zoom_in{};
};
struct SimulationViewport final { epochengine::gui_lib::Rect rect{}; std::uint32_t tile_pixel_size{}; };

[[nodiscard]] inline constexpr std::pair<std::uint32_t, std::uint32_t>
framebuffer_to_logical_pointer(
    const std::uint32_t framebuffer_x,
    const std::uint32_t framebuffer_y,
    const std::uint32_t framebuffer_width,
    const std::uint32_t framebuffer_height,
    const std::uint32_t logical_width,
    const std::uint32_t logical_height) noexcept {
    const auto safe_framebuffer_width = (std::max)(framebuffer_width, 1u);
    const auto safe_framebuffer_height = (std::max)(framebuffer_height, 1u);
    const auto safe_logical_width = (std::max)(logical_width, 1u);
    const auto safe_logical_height = (std::max)(logical_height, 1u);
    return {
        (std::min)(safe_logical_width - 1u, static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(framebuffer_x) * safe_logical_width /
            safe_framebuffer_width)),
        (std::min)(safe_logical_height - 1u, static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(framebuffer_y) * safe_logical_height /
            safe_framebuffer_height)),
    };
}
[[nodiscard]] inline constexpr std::pair<std::int32_t, std::int32_t> pointer_to_grid(
    const SimulationViewport& viewport,
    const std::uint32_t view_origin_x, const std::uint32_t view_origin_y,
    const std::uint32_t view_width, const std::uint32_t view_height,
    const std::int32_t pointer_x, const std::int32_t pointer_y) noexcept {
    const auto viewport_width = (std::max)(
        1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(
        1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(
        pointer_x - static_cast<std::int32_t>(viewport.rect.position.x),
        0, static_cast<std::int32_t>(viewport_width - 1u));
    const auto local_y = std::clamp(
        pointer_y - static_cast<std::int32_t>(viewport.rect.position.y),
        0, static_cast<std::int32_t>(viewport_height - 1u));
    return {
        static_cast<std::int32_t>(view_origin_x +
            static_cast<std::uint64_t>(local_x) * view_width / viewport_width),
        static_cast<std::int32_t>(view_origin_y +
            static_cast<std::uint64_t>(local_y) * view_height / viewport_height),
    };
}

[[nodiscard]] inline SimulationViewport make_map_overlay_viewport(
    const Layout& layout, std::uint32_t map_width, std::uint32_t map_height) noexcept {
    const auto panel_width = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.x));
    const auto panel_height = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.y));
    const auto safe_width = (std::max)(map_width, 1u);
    const auto safe_height = (std::max)(map_height, 1u);

    const auto maximum_width = (std::max)(160u, panel_width * 7u / 10u);
    const auto maximum_height = (std::max)(72u, panel_height / 4u);
    std::uint32_t viewport_width = (std::min)(panel_width > 24u ? panel_width - 24u : panel_width,
                                               maximum_width);
    std::uint32_t viewport_height = (std::max)(1u, static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(viewport_width) * safe_height / safe_width));
    if (viewport_height > maximum_height) {
        viewport_height = maximum_height;
        viewport_width = (std::max)(1u, static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(viewport_height) * safe_width / safe_height));
    }
    viewport_width = (std::min)(viewport_width, panel_width);
    viewport_height = (std::min)(viewport_height, panel_height);

    const auto left = layout.simulation.position.x +
                      float((panel_width - viewport_width) / 2u);
    constexpr float top_margin = 16.0f;
    const auto top = layout.simulation.position.y +
                     (panel_height > viewport_height + static_cast<std::uint32_t>(top_margin)
                          ? top_margin : 0.0f);
    return {{{left, top}, {float(viewport_width), float(viewport_height)}}, 0u};
}

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

[[nodiscard]] inline constexpr Layout make_layout(std::uint32_t width, std::uint32_t height) noexcept {
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
    const float inventory_subtab_top = layout.group_tabs.position.y + 25.0f;
    const float inventory_subtab_width = content_width / 2.0f;
    layout.inventory_inventory = {{content_left, inventory_subtab_top},
                                  {inventory_subtab_width, 28.0f}};
    layout.inventory_blueprints = {{content_left + inventory_subtab_width, inventory_subtab_top},
                                   {content_width - inventory_subtab_width, 28.0f}};

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
    const float designer_button_width = content_width / 2.0f;
    layout.designer_static_model = {{content_left, keymap_top + 25.0f},
                                    {designer_button_width, 28.0f}};
    layout.designer_map_chunk = {{content_left + designer_button_width, keymap_top + 25.0f},
                                 {content_width - designer_button_width, 28.0f}};
    layout.designer_inventory = {{content_left, keymap_top + 59.0f},
                                  {designer_button_width, 28.0f}};
    layout.designer_blueprints = {{content_left + designer_button_width, keymap_top + 59.0f},
                                   {content_width - designer_button_width, 28.0f}};
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
    const float designer_grid_height = (std::min)(
        content_width * 0.5f, (std::max)(1.0f, layout.material_card.size.y * 0.5f));
    layout.designer_grid = {{content_left, card_top},
                            {content_width, designer_grid_height}};
    const float designer_card_top = card_top + designer_grid_height + gap;
    layout.designer_material_card = {
        {content_left, designer_card_top},
        {content_width, (std::max)(1.0f, float(screen_height) - designer_card_top - margin)}};
    return layout;
}

[[nodiscard]] inline constexpr bool contains_workspace_point(
    const epochengine::gui_lib::Rect& rect,
    const epochengine::gui_lib::Vec2 point) noexcept {
    return point.x >= rect.position.x &&
           point.x <= rect.position.x + rect.size.x &&
           point.y >= rect.position.y &&
           point.y <= rect.position.y + rect.size.y;
}

[[nodiscard]] inline constexpr std::uint32_t workspace_at(
    const Layout& layout, const epochengine::gui_lib::Vec2 point) noexcept {
    if (contains_workspace_point(layout.workspace_inventory, point)) return 0u;
    if (contains_workspace_point(layout.workspace_editor, point)) return 1u;
    if (contains_workspace_point(layout.workspace_settings, point)) return 2u;
    if (contains_workspace_point(layout.workspace_designer, point)) return 3u;
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
    (void)window_height;
    constexpr float slot_gap = 5.0f;
    constexpr float slot_height = 52.0f;
    const float left = layout.inventory_inventory.position.x;
    const float width = layout.inventory_inventory.size.x + layout.inventory_blueprints.size.x;
    const float top = layout.inventory_inventory.position.y + layout.inventory_inventory.size.y + gap;
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
[[nodiscard]] inline epochengine::gui_lib::Rect designer_blueprint_slot_rect(
    const Layout& layout, const std::uint32_t index) noexcept {
    constexpr float slot_gap = 4.0f;
    const float left = layout.keymap.position.x;
    const float width = layout.keymap.size.x;
    const float top = layout.keymap.position.y + 92.0f;
    const float bottom = layout.keymap.position.y + layout.keymap.size.y - 5.0f;
    const float slot_width = (std::max)(1.0f, (width - slot_gap * 3.0f) / 4.0f);
    const auto clamped = (std::min)(index, blueprint_slot_count - 1u);
    const float slot_left =
        left + static_cast<float>(clamped) * (slot_width + slot_gap);
    const float slot_right = clamped == blueprint_slot_count - 1u
        ? left + width : slot_left + slot_width;
    return {{slot_left, top},
            {(std::max)(1.0f, slot_right - slot_left),
             (std::max)(1.0f, bottom - top)}};
}

[[nodiscard]] inline std::uint32_t designer_blueprint_slot_at(
    const Layout& layout, const epochengine::gui_lib::Vec2 point) noexcept {
    for (std::uint32_t index = 0u; index < blueprint_slot_count; ++index)
        if (epochengine::gui_lib::contains(
                designer_blueprint_slot_rect(layout, index), point))
            return index;
    return blueprint_slot_count;
}
} // namespace sandhybrid::ui
