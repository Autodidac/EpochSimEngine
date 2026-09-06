#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/ui_layout.hpp"

#include <array>
#include <cstdint>

int main() {
    using sandhybrid::Material;
    using sandhybrid::MaterialGroup;

    constexpr auto font_metrics =
        epochengine::gui_lib::font::make_bitmap_font_metrics({16.0F, 1.5F});
    static_assert(font_metrics.pixel_height == 24.0F);
    static_assert(font_metrics.glyph_height == 24.0F);
    static_assert(epochengine::gui_lib::font::resolved_pixel_height({12.0F, 2.0F}) ==
                  24.0F);

    const auto layout = sandhybrid::ui::make_layout(1280u, 720u);
    const auto industry_tab = sandhybrid::ui::group_tab_rect(
        layout, static_cast<std::uint32_t>(MaterialGroup::industry));
    const auto industry = sandhybrid::ui::group_at(
        layout,
        {industry_tab.position.x + industry_tab.size.x * 0.5f,
         industry_tab.position.y + industry_tab.size.y * 0.5f});
    if (industry != static_cast<std::uint32_t>(MaterialGroup::industry)) return 1;

    const auto bot_slot = sandhybrid::ui::palette_item_rect(layout, MaterialGroup::industry, 3u);
    const auto material = sandhybrid::ui::palette_material_at(
        layout, MaterialGroup::industry,
        {bot_slot.position.x + bot_slot.size.x * 0.5f,
         bot_slot.position.y + bot_slot.size.y * 0.5f});
    if (material != Material::factory_core) return 2;

    const auto beehive_slot = sandhybrid::ui::palette_item_rect(
        layout, MaterialGroup::colony, 3u);
    const auto beehive_button_material = sandhybrid::ui::palette_material_at(
        layout, MaterialGroup::colony,
        {beehive_slot.position.x + beehive_slot.size.x * 0.5f,
         beehive_slot.position.y + beehive_slot.size.y * 0.5f});
    if (beehive_button_material != Material::beehive) return 28;

    if (layout.reset_scene.position.y != layout.pause_toggle.position.y ||
        layout.reset_scene.size.y != layout.pause_toggle.size.y ||
        layout.reset_scene.position.x + layout.reset_scene.size.x >
            layout.pause_toggle.position.x) return 3;
    if (layout.previous_scene.size.x != 0.0f || layout.next_scene.size.x != 0.0f ||
        layout.save_scene.position.x + layout.save_scene.size.x > layout.load_scene.position.x ||
        layout.save_scene.size.x <= 0.0f || layout.load_scene.size.x <= 0.0f) return 4;
    if (layout.mode_toggle.position.x + layout.mode_toggle.size.x >
            layout.camera_controls_toggle.position.x ||
        layout.camera_controls_toggle.position.x + layout.camera_controls_toggle.size.x >
            layout.camera_home.position.x ||
        layout.camera_home.position.x + layout.camera_home.size.x >
            layout.map_toggle.position.x ||
        layout.map_toggle.position.x + layout.map_toggle.size.x >
            layout.debug_toggle.position.x) return 5;
    if (layout.atmosphere.position.x + layout.atmosphere.size.x > layout.eraser.position.x ||
        layout.eraser.position.x + layout.eraser.size.x > layout.fill.position.x ||
        layout.group_tabs.position.y < layout.fill.position.y + layout.fill.size.y) return 6;
    if (layout.status.size.x < 420.0f ||
        layout.cursor_editor.position.y < layout.keymap.position.y + layout.keymap.size.y ||
        layout.material_card.position.y <
            layout.cursor_editor.position.y + layout.cursor_editor.size.y) return 15;

    const auto viewport = sandhybrid::ui::make_simulation_viewport(layout, 640u, 360u);
    if (static_cast<std::uint32_t>(viewport.rect.size.x) % 80u != 0u ||
        static_cast<std::uint32_t>(viewport.rect.size.y) % 45u != 0u) return 7;
    if (viewport.rect.size.x / 80.0f != viewport.rect.size.y / 45.0f) return 8;
    if (viewport.rect.position.x < 0.0f ||
        viewport.rect.position.y < layout.simulation.position.y) return 9;
    const auto grid_top_left = sandhybrid::ui::pointer_to_grid(
        viewport, 1280u, 720u, 640u, 360u,
        static_cast<std::int32_t>(viewport.rect.position.x),
        static_cast<std::int32_t>(viewport.rect.position.y));
    const auto grid_bottom_right = sandhybrid::ui::pointer_to_grid(
        viewport, 1280u, 720u, 640u, 360u,
        static_cast<std::int32_t>(viewport.rect.position.x + viewport.rect.size.x - 1.0f),
        static_cast<std::int32_t>(viewport.rect.position.y + viewport.rect.size.y - 1.0f));
    if (grid_top_left != std::pair<std::int32_t, std::int32_t>{1280, 720} ||
        grid_bottom_right != std::pair<std::int32_t, std::int32_t>{1919, 1079}) return 23;
    const auto hidpi_layout = sandhybrid::ui::make_layout(2560u, 1440u);
    const auto hidpi_viewport = sandhybrid::ui::make_simulation_viewport(
        hidpi_layout, 640u, 360u);
    const auto logical_center = sandhybrid::ui::pointer_to_grid(
        viewport, 1280u, 720u, 640u, 360u,
        static_cast<std::int32_t>(viewport.rect.position.x + viewport.rect.size.x * 0.5f),
        static_cast<std::int32_t>(viewport.rect.position.y + viewport.rect.size.y * 0.5f));
    const auto framebuffer_center = sandhybrid::ui::pointer_to_grid(
        hidpi_viewport, 1280u, 720u, 640u, 360u,
        static_cast<std::int32_t>(hidpi_viewport.rect.position.x + hidpi_viewport.rect.size.x * 0.5f),
        static_cast<std::int32_t>(hidpi_viewport.rect.position.y + hidpi_viewport.rect.size.y * 0.5f));
    if (logical_center != framebuffer_center) return 24;
    const auto scaled_center = sandhybrid::ui::framebuffer_to_logical_pointer(
        600u, 540u, 1920u, 1080u, 1280u, 720u);
    if (scaled_center != std::pair<std::uint32_t, std::uint32_t>{400u, 360u}) return 25;
    const auto logical_sidebar_left = static_cast<std::uint32_t>(
        layout.simulation.position.x + layout.simulation.size.x);
    const auto scaled_sidebar_left = sandhybrid::ui::framebuffer_to_logical_pointer(
        logical_sidebar_left * 3u / 2u, 300u, 1920u, 1080u, 1280u, 720u);
    if (scaled_sidebar_left.first != logical_sidebar_left) return 26;

    const auto wide_map = sandhybrid::ui::make_simulation_viewport(
        layout, sandhybrid::resident_world_width, sandhybrid::resident_world_height);
    if (static_cast<std::uint32_t>(wide_map.rect.size.x) != 856u ||
        static_cast<std::uint32_t>(wide_map.rect.size.y) != 120u ||
        wide_map.tile_pixel_size != 0u) return 14;

    const auto map_overlay = sandhybrid::ui::make_map_overlay_viewport(
        layout, sandhybrid::resident_world_width, sandhybrid::resident_world_height);
    if (map_overlay.rect.position.x <= layout.simulation.position.x ||
        map_overlay.rect.position.y != 16.0f ||
        map_overlay.rect.size.x > layout.simulation.size.x * 0.71f ||
        map_overlay.rect.size.y > layout.simulation.size.y * 0.26f ||
        map_overlay.rect.size.x / map_overlay.rect.size.y < 7.0f) return 18;

    const auto designer_contains = [](const auto outer, const auto inner) constexpr {
        return inner.position.x >= outer.position.x &&
               inner.position.y >= outer.position.y &&
               inner.position.x + inner.size.x <= outer.position.x + outer.size.x &&
               inner.position.y + inner.size.y <= outer.position.y + outer.size.y;
    };
    const auto designer_nonoverlap = [](const auto a, const auto b) constexpr {
        return a.position.x + a.size.x <= b.position.x ||
               b.position.x + b.size.x <= a.position.x ||
               a.position.y + a.size.y <= b.position.y ||
               b.position.y + b.size.y <= a.position.y;
    };
    if (!designer_contains(layout.keymap, layout.designer_static_model) ||
        !designer_contains(layout.keymap, layout.designer_map_chunk) ||
        !designer_contains(layout.keymap, layout.designer_inventory) ||
        !designer_contains(layout.keymap, layout.designer_blueprints) ||
        !designer_nonoverlap(layout.designer_static_model, layout.designer_map_chunk) ||
        !designer_nonoverlap(layout.designer_inventory, layout.designer_blueprints)) return 19;
    for (std::uint32_t slot = 0u; slot < sandhybrid::blueprint_slot_count; ++slot) {
        const auto rect = sandhybrid::ui::designer_blueprint_slot_rect(layout, slot);
        if (!designer_contains(layout.keymap, rect) ||
            sandhybrid::ui::designer_blueprint_slot_at(
                layout, {rect.position.x + rect.size.x * 0.5f,
                         rect.position.y + rect.size.y * 0.5f}) != slot)
            return 25;
    }

    const auto compact = sandhybrid::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f || compact.status.size.x < 300.0f) return 10;
    const auto sidebar_left = layout.simulation.position.x + layout.simulation.size.x;
    const auto sidebar_right = layout.status.position.x + layout.status.size.x;
    if (layout.inventory_inventory.position.x < sidebar_left ||
        layout.inventory_blueprints.position.x < sidebar_left ||
        layout.inventory_blueprints.position.x + layout.inventory_blueprints.size.x > sidebar_right ||
        !designer_nonoverlap(layout.inventory_inventory, layout.inventory_blueprints)) return 20;
    if (layout.designer_grid.position.x < sidebar_left ||
        layout.designer_material_card.position.x < sidebar_left ||
        layout.designer_grid.position.x + layout.designer_grid.size.x > sidebar_right ||
        layout.designer_material_card.position.x + layout.designer_material_card.size.x > sidebar_right ||
        layout.designer_grid.position.y + layout.designer_grid.size.y >
            layout.designer_material_card.position.y) return 21;
    if (layout.inventory_inventory.position.y < layout.group_tabs.position.y ||
        layout.inventory_blueprints.position.y < layout.group_tabs.position.y ||
        layout.inventory_inventory.position.y + layout.inventory_inventory.size.y >
            layout.group_tabs.position.y + layout.group_tabs.size.y ||
        layout.inventory_blueprints.position.y + layout.inventory_blueprints.size.y >
            layout.group_tabs.position.y + layout.group_tabs.size.y) return 22;

    if (compact.reset_scene.size.x <= 0.0f || compact.pause_toggle.size.x <= 0.0f ||
        compact.atmosphere.size.x <= 0.0f || compact.eraser.size.x <= 0.0f ||
        compact.fill.size.x <= 0.0f) return 11;
    if (compact.mode_toggle.position.x < compact.simulation.size.x ||
        compact.pause_toggle.position.x < compact.simulation.size.x ||
        compact.camera_controls_toggle.position.x < compact.simulation.size.x ||
        compact.camera_home.position.x < compact.simulation.size.x ||
        compact.map_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 12;

    if (sandhybrid::ui::palette_item_count(MaterialGroup::fire_chemistry) !=
            sandhybrid::material_group_size(MaterialGroup::fire_chemistry) ||
        sandhybrid::ui::palette_item_count(MaterialGroup::ground) !=
            sandhybrid::material_group_size(MaterialGroup::ground)) return 16;
    const auto ignite_slot = layout.ignite_air;
    if (!sandhybrid::ui::ignite_air_action_at(
            layout,
            {ignite_slot.position.x + ignite_slot.size.x * 0.5f,
             ignite_slot.position.y + ignite_slot.size.y * 0.5f}) ||
        layout.actions.position.y + layout.actions.size.y > layout.keymap.position.y)
        return 17;

    if (!designer_contains(layout.settings_fps, layout.fps_30) ||
        !designer_contains(layout.settings_fps, layout.fps_60) ||
        !designer_contains(layout.settings_fps, layout.fps_120) ||
        !designer_contains(layout.settings_fps, layout.fps_unlimited) ||
        !designer_nonoverlap(layout.fps_30, layout.fps_60) ||
        !designer_nonoverlap(layout.fps_60, layout.fps_120) ||
        !designer_nonoverlap(layout.fps_120, layout.fps_unlimited) ||
        layout.settings_fps.position.y + layout.settings_fps.size.y > layout.keymap.position.y)
        return 27;

    for (std::uint32_t slot = 0u; slot < 4u; ++slot) {
        const auto rect = sandhybrid::ui::inventory_slot_rect(layout, 720u, slot);
        const auto hit = sandhybrid::ui::inventory_slot_at(
            layout, 720u, {rect.position.x + rect.size.x * 0.5f,
                           rect.position.y + rect.size.y * 0.5f});
        if (hit != slot) return 13;
    }

    // Independent logical-pixel oracle: height, group height, palette height,
    // canvas top, canvas height, material-card height. Include the requested
    // resize boundaries plus both actual compact-policy transition edges.
    constexpr std::array<std::array<std::uint32_t, 6>, 14> designer_cases{{
        {719u, 64u, 82u, 607u, 64u, 40u},
        {720u, 64u, 83u, 608u, 64u, 40u},
        {721u, 64u, 84u, 609u, 64u, 40u},
        {743u, 64u, 106u, 631u, 64u, 40u},
        {744u, 64u, 107u, 632u, 64u, 40u},
        {761u, 64u, 124u, 649u, 64u, 40u},
        {762u, 65u, 124u, 650u, 64u, 40u},
        {767u, 70u, 124u, 655u, 64u, 40u},
        {768u, 71u, 124u, 656u, 64u, 40u},
        {792u, 95u, 124u, 680u, 64u, 40u},
        {793u, 96u, 124u, 681u, 64u, 40u},
        {807u, 96u, 124u, 681u, 78u, 40u},
        {808u, 96u, 124u, 681u, 79u, 40u},
        {1080u, 96u, 124u, 681u, 207u, 184u},
    }};
    const auto same_rect = [](const auto a, const auto b) constexpr {
        return a.position.x == b.position.x && a.position.y == b.position.y &&
               a.size.x == b.size.x && a.size.y == b.size.y;
    };
    for (const auto& expected : designer_cases) {
        const auto height = expected[0];
        const auto designer = sandhybrid::ui::make_layout(1280u, height, 3u);
        const auto editor = sandhybrid::ui::make_layout(1280u, height, 1u);
        const epochengine::gui_lib::Rect sidebar{{856.0f, 0.0f}, {424.0f, float(height)}};
        if (designer.group_tabs.size.y != float(expected[1]) ||
            designer.palette.size.y != float(expected[2]) ||
            designer.designer_grid.position.y != float(expected[3]) ||
            designer.designer_grid.size.y != float(expected[4]) ||
            designer.designer_material_card.size.y != float(expected[5])) return 29;
        if (!same_rect(designer.simulation, editor.simulation) ||
            !same_rect(designer.status, editor.status) ||
            !same_rect(designer.fill, editor.fill) ||
            !same_rect(designer.workspace_designer, editor.workspace_designer) ||
            editor.group_tabs.size.y != 96.0f || editor.palette.size.y != 124.0f ||
            editor.actions.position.y != 439.0f || editor.ignite_air.position.y != 461.0f ||
            editor.keymap.position.y != 496.0f) return 30;
        for (std::uint32_t workspace = 0u; workspace < 3u; ++workspace) {
            const auto other = sandhybrid::ui::make_layout(1280u, height, workspace);
            if (!same_rect(other.group_tabs, editor.group_tabs) ||
                !same_rect(other.palette, editor.palette) ||
                !same_rect(other.ignite_air, editor.ignite_air) ||
                !same_rect(other.cursor_editor, editor.cursor_editor)) return 31;
        }
        if (designer.keymap.position.y != designer.palette.position.y + designer.palette.size.y + 3.0f ||
            designer.keymap.size.y != 124.0f || designer.cursor_editor.size.y != 112.0f ||
            designer.designer_grid.size.y < 64.0f || designer.designer_material_card.size.y < 40.0f)
            return 32;
        const std::array panels{designer.group_tabs, designer.palette, designer.keymap,
                                designer.cursor_editor, designer.designer_grid,
                                designer.designer_material_card};
        for (std::size_t panel = 0u; panel < panels.size(); ++panel) {
            if (!designer_contains(sidebar, panels[panel]) ||
                panels[panel].size.x <= 0.0f || panels[panel].size.y <= 0.0f ||
                (panel != 0u && !designer_nonoverlap(panels[panel - 1u], panels[panel]))) return 33;
        }
        const std::array designer_buttons{designer.designer_static_model, designer.designer_map_chunk,
                                         designer.designer_inventory, designer.designer_blueprints};
        for (const auto& button : designer_buttons)
            if (!designer_contains(designer.keymap, button) || button.size.y != 28.0f) return 34;
        const std::array cursor_buttons{designer.placement_cells, designer.placement_tiles,
            designer.cursor_circle, designer.cursor_square, designer.cursor_horizontal,
            designer.cursor_vertical, designer.brush_smaller, designer.brush_larger,
            designer.zoom_out, designer.zoom_in};
        for (std::size_t button = 0u; button < cursor_buttons.size(); ++button) {
            if (!designer_contains(designer.cursor_editor, cursor_buttons[button]) ||
                cursor_buttons[button].size.y < 24.0f) return 35;
            for (std::size_t other = button + 1u; other < cursor_buttons.size(); ++other)
                if (!designer_nonoverlap(cursor_buttons[button], cursor_buttons[other])) return 36;
        }
        for (std::uint32_t slot = 0u; slot < sandhybrid::blueprint_slot_count; ++slot) {
            const auto rect = sandhybrid::ui::designer_blueprint_slot_rect(designer, slot);
            const epochengine::gui_lib::Vec2 point{
                rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f};
            if (!designer_contains(designer.keymap, rect) || rect.size.y != 27.0f ||
                sandhybrid::ui::designer_blueprint_slot_at(designer, point) != slot ||
                !designer_nonoverlap(rect, designer.designer_inventory) ||
                !designer_nonoverlap(rect, designer.designer_blueprints)) return 37;
        }
        for (std::uint32_t group = 0u; group < sandhybrid::material_group_count; ++group) {
            const auto group_rect = sandhybrid::ui::group_tab_rect(designer, group);
            if (!designer_contains(designer.group_tabs, group_rect) ||
                sandhybrid::ui::group_at(designer,
                    {group_rect.position.x + group_rect.size.x * 0.5f,
                     group_rect.position.y + group_rect.size.y * 0.5f}) != group) return 38;
            // The fragment uses proportional row selection, not a truncated
            // pitch that can drift into a different button at odd heights.
            for (auto y = static_cast<std::uint32_t>(group_rect.position.y);
                 float(y) < group_rect.position.y + group_rect.size.y; ++y) {
                if (float(y) < group_rect.position.y) continue;
                const auto row = (y - static_cast<std::uint32_t>(designer.group_tabs.position.y)) * 4u /
                                 static_cast<std::uint32_t>(designer.group_tabs.size.y);
                if (row != group / 2u) return 42;
            }
            const auto material_group = static_cast<MaterialGroup>(group);
            for (std::uint32_t slot = 0u; slot < sandhybrid::material_group_size(material_group); ++slot) {
                const auto rect = sandhybrid::ui::palette_item_rect(designer, material_group, slot);
                if (!designer_contains(designer.palette, rect) ||
                    sandhybrid::ui::palette_slot_at(designer, material_group,
                        {rect.position.x + rect.size.x * 0.5f,
                         rect.position.y + rect.size.y * 0.5f}) != slot) return 39;
                const auto rows = (sandhybrid::material_group_size(material_group) + 1u) / 2u;
                for (auto y = static_cast<std::uint32_t>(rect.position.y);
                     float(y) < rect.position.y + rect.size.y; ++y) {
                    if (float(y) < rect.position.y) continue;
                    const auto row = (y - static_cast<std::uint32_t>(designer.palette.position.y)) * rows /
                                     static_cast<std::uint32_t>(designer.palette.size.y);
                    if (row != slot / 2u) return 43;
                }
            }
        }

        // Integer canvas bounds must agree with fragment sampling through
        // logical input conversion at 1x, fractional 1.5x, and 2x DPI. Exercise
        // every canvas pixel at each Designer zoom, including last-row/column.
        // This is a geometry contract, not packaged rendering/eye acceptance.
        const auto grid_left = static_cast<std::uint32_t>(designer.designer_grid.position.x);
        const auto grid_top = static_cast<std::uint32_t>(designer.designer_grid.position.y);
        const auto grid_width = static_cast<std::uint32_t>(designer.designer_grid.size.x);
        const auto grid_height = static_cast<std::uint32_t>(designer.designer_grid.size.y);
        const sandhybrid::ui::SimulationViewport grid{designer.designer_grid, 0u};
        for (std::uint32_t scale = 2u; scale <= 4u; ++scale) {
            const auto framebuffer_width = 1280u * scale / 2u;
            const auto framebuffer_height = height * scale / 2u;
            for (std::uint32_t y = 0u; y < grid_height; ++y)
                for (std::uint32_t x = 0u; x < grid_width; ++x) {
                    const auto logical_x = grid_left + x;
                    const auto logical_y = grid_top + y;
                    const auto physical_x = (logical_x * framebuffer_width + 1279u) / 1280u;
                    const auto physical_y = (logical_y * framebuffer_height + height - 1u) / height;
                    const auto logical = sandhybrid::ui::framebuffer_to_logical_pointer(
                        physical_x, physical_y, framebuffer_width, framebuffer_height, 1280u, height);
                    if (logical != std::pair<std::uint32_t, std::uint32_t>{logical_x, logical_y}) return 40;
                    for (std::uint32_t zoom = 1u; zoom <= 4u; ++zoom) {
                        const auto columns = (std::max)(8u, 64u / zoom);
                        const auto rows = (std::max)(8u, 32u / zoom);
                        const auto origin_x = (64u - columns) / 2u;
                        const auto origin_y = (32u - rows) / 2u;
                        const auto input_cell = sandhybrid::ui::pointer_to_grid(
                            grid, origin_x, origin_y, columns, rows,
                            static_cast<std::int32_t>(logical.first),
                            static_cast<std::int32_t>(logical.second));
                        const std::pair<std::int32_t, std::int32_t> fragment_cell{
                            static_cast<std::int32_t>(origin_x + x * columns / grid_width),
                            static_cast<std::int32_t>(origin_y + y * rows / grid_height)};
                        if (input_cell != fragment_cell) return 41;
                    }
                }
        }
    }
    return 0;
}
static_assert(sandhybrid::ui::workspace_at(
    sandhybrid::ui::make_layout(1920u, 1080u),
    {sandhybrid::ui::make_layout(1920u, 1080u).workspace_inventory.position.x + 2.0f,
     sandhybrid::ui::make_layout(1920u, 1080u).workspace_inventory.position.y + 2.0f}) == 0u);
static_assert(sandhybrid::ui::workspace_at(
    sandhybrid::ui::make_layout(1920u, 1080u),
    {sandhybrid::ui::make_layout(1920u, 1080u).workspace_designer.position.x + 2.0f,
     sandhybrid::ui::make_layout(1920u, 1080u).workspace_designer.position.y + 2.0f}) == 3u);
