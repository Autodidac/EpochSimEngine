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
    if (layout.simulation.size.y <= 0.0f || layout.debug_toggle.size.x < 60.0f) return 3;

    const auto viewport = sandhybrid::ui::make_simulation_viewport(layout, 640u, 360u);
    if (static_cast<std::uint32_t>(viewport.rect.size.x) % 80u != 0u ||
        static_cast<std::uint32_t>(viewport.rect.size.y) % 45u != 0u) return 4;
    if (viewport.rect.size.x / 80.0f != viewport.rect.size.y / 45.0f) return 5;
    if (viewport.rect.position.x < 0.0f ||
        viewport.rect.position.y < layout.simulation.position.y) return 6;

    const auto compact = sandhybrid::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f || compact.status.size.x < 300.0f) return 7;
    if (compact.previous_scene.size.x <= 0.0f || compact.next_scene.size.x <= 0.0f ||
        compact.reset_scene.size.x <= 0.0f) return 8;
    if (compact.mode_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 9;
    return 0;
}
