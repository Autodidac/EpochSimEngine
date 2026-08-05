#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/ui_layout.hpp"

#include <cstdint>

int main() {
    using sandhybrid::Material;
    using sandhybrid::MaterialGroup;

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

    if (layout.reset_scene.position.y != layout.pause_toggle.position.y ||
        layout.reset_scene.size.y != layout.pause_toggle.size.y ||
        layout.reset_scene.position.x + layout.reset_scene.size.x >
            layout.pause_toggle.position.x) return 3;
    if (layout.previous_scene.position.x + layout.previous_scene.size.x > layout.next_scene.position.x ||
        layout.next_scene.position.x + layout.next_scene.size.x > layout.save_scene.position.x ||
        layout.save_scene.position.x + layout.save_scene.size.x > layout.load_scene.position.x) return 4;
    if (layout.mode_toggle.position.x + layout.mode_toggle.size.x >
            layout.camera_controls_toggle.position.x ||
        layout.camera_controls_toggle.position.x + layout.camera_controls_toggle.size.x >
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
        compact.map_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 12;

    if (sandhybrid::ui::palette_item_count(MaterialGroup::fire_chemistry) !=
            sandhybrid::material_group_size(MaterialGroup::fire_chemistry) + 1u ||
        sandhybrid::ui::palette_item_count(MaterialGroup::ground) !=
            sandhybrid::material_group_size(MaterialGroup::ground)) return 16;
    const auto ignite_slot = sandhybrid::ui::palette_item_rect(
        layout, MaterialGroup::fire_chemistry,
        sandhybrid::material_group_size(MaterialGroup::fire_chemistry));
    if (!sandhybrid::ui::ignite_air_action_at(
            layout, MaterialGroup::fire_chemistry,
            {ignite_slot.position.x + ignite_slot.size.x * 0.5f,
             ignite_slot.position.y + ignite_slot.size.y * 0.5f})) return 17;

    for (std::uint32_t slot = 0u; slot < 4u; ++slot) {
        const auto rect = sandhybrid::ui::inventory_slot_rect(layout, 720u, slot);
        const auto hit = sandhybrid::ui::inventory_slot_at(
            layout, 720u, {rect.position.x + rect.size.x * 0.5f,
                           rect.position.y + rect.size.y * 0.5f});
        if (hit != slot) return 13;
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
