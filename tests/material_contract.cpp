#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"
#include "epoch/sand/simulation_policy.hpp"
#include "epoch/sand/ui_layout.hpp"

#include <array>
#include <cstdint>

using epoch::sand::Material;
using epoch::sand::MaterialGroup;
using epoch::sand::MaterialPhase;
using epoch::sand::Scene;

static_assert(static_cast<std::uint32_t>(Material::empty) == 0u);
static_assert(static_cast<std::uint32_t>(Material::waste) == 63u);
static_assert(static_cast<std::uint32_t>(Material::hydrogen) == 64u);
static_assert(epoch::sand::material_count == 65u);
static_assert(epoch::sand::material_profiles.size() == epoch::sand::material_count);

static_assert(epoch::sand::material_group_count == 2u);
static_assert(epoch::sand::material_slots_per_group == 10u);
static_assert(epoch::sand::material_group_size(MaterialGroup::ground) == 5u);
static_assert(epoch::sand::material_group_size(MaterialGroup::fluids) == 8u);
static_assert(epoch::sand::grouped_material(MaterialGroup::ground, 0u) == Material::empty);
static_assert(epoch::sand::grouped_material(MaterialGroup::ground, 4u) == Material::mud);
static_assert(epoch::sand::grouped_material(MaterialGroup::fluids, 0u) == Material::water);
static_assert(epoch::sand::grouped_material(MaterialGroup::fluids, 7u) == Material::oxygen);
static_assert(epoch::sand::grouped_material(MaterialGroup::fluids, 8u) == Material::count);

static_assert(epoch::sand::is_enabled_material(Material::sand));
static_assert(epoch::sand::is_enabled_material(Material::water));
static_assert(epoch::sand::is_enabled_material(Material::oxygen));
static_assert(!epoch::sand::is_enabled_material(Material::fire));
static_assert(epoch::sand::canonical_material(Material::smoke) == Material::oxygen);
static_assert(epoch::sand::canonical_material(Material::steam) == Material::oxygen);
static_assert(epoch::sand::canonical_material(Material::molten_copper) == Material::water);
static_assert(epoch::sand::canonical_material(Material::crystal) == Material::stone);
static_assert(epoch::sand::canonical_material(Material::ash) == Material::dirt);

static_assert(epoch::sand::scene_count == 9u);
static_assert(epoch::sand::next_scene(Scene::frontier_base) == Scene::sandbox);
static_assert(epoch::sand::scene_has_character(Scene::frontier_base));
static_assert(epoch::sand::scene_name(Scene::gold_mine) == "Platformer");
static_assert(epoch::sand::is_block_material(Material::stone));
static_assert(!epoch::sand::is_block_material(Material::mud));

// Canonical phase behavior remains property-driven and independent of placement source.
static_assert(epoch::sand::phase_at(Material::copper, 1300) == MaterialPhase::molten);
static_assert(epoch::sand::phase_at(Material::gold, 1070) == MaterialPhase::molten);
static_assert(epoch::sand::phase_at(Material::iron, 1300) == MaterialPhase::softened);
static_assert(epoch::sand::phase_at(Material::steel, 1300) == MaterialPhase::softened);
static_assert(epoch::sand::phase_at(Material::steel, 1300) != MaterialPhase::molten);
static_assert(epoch::sand::phase_at(Material::steel, 1300) != MaterialPhase::vapor);
static_assert(epoch::sand::phase_at(Material::steel, 1600) == MaterialPhase::molten);
static_assert(epoch::sand::phase_at(Material::plastic, 120) == MaterialPhase::softened);
static_assert(epoch::sand::phase_at(Material::plastic, 180) == MaterialPhase::molten);
static_assert(epoch::sand::phase_at(Material::plastic, 500) == MaterialPhase::vapor);
static_assert(epoch::sand::material_profile(Material::gold).melting_point <
              epoch::sand::material_profile(Material::steel).melting_point);
static_assert(epoch::sand::material_profile(Material::copper).melting_point <
              epoch::sand::material_profile(Material::steel).melting_point);
static_assert(epoch::sand::material_profile(Material::plastic).softening_point <
              epoch::sand::material_profile(Material::plastic).melting_point);
static_assert(epoch::sand::material_profile(Material::plastic).melting_point <
              epoch::sand::material_profile(Material::plastic).ignition_point);
static_assert(epoch::sand::material_profile(Material::plastic).conversions.find("SMOKE") != std::string_view::npos);
static_assert(epoch::sand::material_profile(Material::acid_resistant_plastic).acid_resistance >
              epoch::sand::material_profile(Material::plastic).acid_resistance);

static_assert(epoch::sand::policy::stability_ready(52u, 120u, true, true, false, false, 0u));
static_assert(!epoch::sand::policy::stability_ready(51u, 120u, true, true, false, false, 0u));
static_assert(epoch::sand::policy::laser_hits_to_dislodge == 2u);
static_assert(!epoch::sand::policy::should_collapse(32u));
static_assert(epoch::sand::policy::should_collapse(31u));
static_assert(epoch::sand::policy::update_vent_pressure(100u, true, false) == 103u);
static_assert(epoch::sand::policy::update_vent_pressure(100u, false, true) == 96u);

constexpr bool palette_materials_are_unique_and_enabled() {
    std::array<std::uint32_t, epoch::sand::material_count> counts{};
    for (std::uint32_t group_index = 0; group_index < epoch::sand::material_group_count; ++group_index) {
        const auto group = static_cast<MaterialGroup>(group_index);
        for (std::uint32_t slot = 0; slot < epoch::sand::material_group_size(group); ++slot) {
            const auto material = epoch::sand::grouped_material(group, slot);
            if (material == Material::count || !epoch::sand::is_enabled_material(material)) return false;
            auto& count = counts[static_cast<std::uint32_t>(material)];
            if (++count != 1u) return false;
        }
    }

    for (std::uint32_t index = 0; index < epoch::sand::material_count; ++index) {
        const auto material = static_cast<Material>(index);
        const auto expected = epoch::sand::is_enabled_material(material) ? 1u : 0u;
        if (counts[index] != expected) return false;
    }
    return true;
}
static_assert(palette_materials_are_unique_and_enabled());

int main() {
    const auto layout = epoch::sand::ui::make_layout(1280u, 720u);

    const auto fluids_tab = epoch::sand::ui::group_tab_rect(
        layout, static_cast<std::uint32_t>(MaterialGroup::fluids));
    const auto selected_group = epoch::sand::ui::group_at(
        layout,
        {fluids_tab.position.x + fluids_tab.size.x * 0.5f,
         fluids_tab.position.y + fluids_tab.size.y * 0.5f});
    if (selected_group != static_cast<std::uint32_t>(MaterialGroup::fluids)) return 1;

    const auto oxygen_slot = epoch::sand::ui::palette_item_rect(layout, MaterialGroup::fluids, 7u);
    const auto selected_material = epoch::sand::ui::palette_material_at(
        layout, MaterialGroup::fluids,
        {oxygen_slot.position.x + oxygen_slot.size.x * 0.5f,
         oxygen_slot.position.y + oxygen_slot.size.y * 0.5f});
    if (selected_material != Material::oxygen) return 2;
    if (layout.simulation.size.y <= 0.0f || layout.debug_toggle.size.x < 60.0f) return 3;

    const auto viewport = epoch::sand::ui::make_simulation_viewport(layout, 640u, 360u);
    if (static_cast<std::uint32_t>(viewport.rect.size.x) % 80u != 0u ||
        static_cast<std::uint32_t>(viewport.rect.size.y) % 45u != 0u) return 4;
    if (viewport.rect.size.x / 80.0f != viewport.rect.size.y / 45.0f) return 5;
    if (viewport.rect.position.x < 0.0f || viewport.rect.position.y < layout.simulation.position.y) return 6;

    const auto compact = epoch::sand::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f || compact.status.size.x < 300.0f) return 7;
    if (compact.previous_scene.size.x <= 0.0f || compact.next_scene.size.x <= 0.0f ||
        compact.reset_scene.size.x <= 0.0f) return 8;
    if (compact.mode_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 9;
    return 0;
}
