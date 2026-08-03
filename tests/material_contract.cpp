#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"
#include "sandhybrid/simulation_policy.hpp"

#include <array>
#include <cstdint>
#include <string_view>

using sandhybrid::Material;
using sandhybrid::MaterialGroup;
using sandhybrid::MaterialPhase;
using sandhybrid::Scene;

static_assert(static_cast<std::uint32_t>(Material::empty) == 0u);
static_assert(static_cast<std::uint32_t>(Material::waste) == 63u);
static_assert(static_cast<std::uint32_t>(Material::hydrogen) == 64u);
static_assert(static_cast<std::uint32_t>(Material::beehive) == 31u);
static_assert(static_cast<std::uint32_t>(Material::iron_ore) == 48u);
static_assert(static_cast<std::uint32_t>(Material::atmosphere) == 66u);
static_assert(sandhybrid::material_count == 67u);
static_assert(sandhybrid::material_profiles.size() == sandhybrid::material_count);
static_assert(sandhybrid::material_names[static_cast<std::uint32_t>(Material::atmosphere)] == "Atmosphere");
static_assert(sandhybrid::material_profile(Material::atmosphere).base_phase == MaterialPhase::gas);
static_assert(sandhybrid::material_profile(Material::lava).base_phase == MaterialPhase::powder);
static_assert(sandhybrid::material_profile(Material::lava).density > sandhybrid::material_profile(Material::silt).density);
static_assert(sandhybrid::material_group_count == 8u);
static_assert(sandhybrid::material_slots_per_group == 10u);
static_assert(sandhybrid::material_group_size(MaterialGroup::ground) == 7u);
static_assert(sandhybrid::material_group_size(MaterialGroup::colony) == 8u);
static_assert(sandhybrid::material_group_size(MaterialGroup::engineering) == 8u);
static_assert(sandhybrid::scene_count == 9u);
static_assert(sandhybrid::next_scene(Scene::frontier_base) == Scene::sandbox);
static_assert(sandhybrid::scene_has_character(Scene::frontier_base));
static_assert(sandhybrid::scene_name(Scene::gold_mine) == "Platformer");
static_assert(sandhybrid::grouped_material(MaterialGroup::ground, 0u) == Material::sand);
static_assert(sandhybrid::grouped_material(MaterialGroup::industry, 3u) == Material::factory_core);
static_assert(sandhybrid::grouped_material(MaterialGroup::engineering, 6u) == Material::hydrogen);
static_assert(sandhybrid::material_names[31u] == "Beehive");
static_assert(sandhybrid::material_names[48u] == "Iron ore");
static_assert(sandhybrid::is_block_material(Material::stone));
static_assert(sandhybrid::is_block_material(Material::iron_ore));
static_assert(sandhybrid::is_block_material(Material::aluminum));
static_assert(sandhybrid::is_block_material(Material::iron));
static_assert(sandhybrid::is_block_material(Material::copper));
static_assert(sandhybrid::is_block_material(Material::gold));
static_assert(sandhybrid::is_block_material(Material::uranium));
static_assert(sandhybrid::is_block_material(Material::steel));
static_assert(!sandhybrid::is_block_material(Material::mud));

static_assert(sandhybrid::phase_at(Material::copper, 1300) == MaterialPhase::molten);
static_assert(sandhybrid::phase_at(Material::gold, 1070) == MaterialPhase::molten);
static_assert(sandhybrid::phase_at(Material::iron, 1300) == MaterialPhase::softened);
static_assert(sandhybrid::phase_at(Material::steel, 1300) == MaterialPhase::softened);
static_assert(sandhybrid::phase_at(Material::steel, 1300) != MaterialPhase::molten);
static_assert(sandhybrid::phase_at(Material::steel, 1600) == MaterialPhase::molten);
static_assert(sandhybrid::phase_at(Material::plastic, 120) == MaterialPhase::softened);
static_assert(sandhybrid::phase_at(Material::plastic, 180) == MaterialPhase::molten);
static_assert(sandhybrid::phase_at(Material::plastic, 500) == MaterialPhase::vapor);
static_assert(sandhybrid::material_profile(Material::gold).melting_point <
              sandhybrid::material_profile(Material::steel).melting_point);
static_assert(sandhybrid::material_profile(Material::copper).melting_point <
              sandhybrid::material_profile(Material::steel).melting_point);
static_assert(sandhybrid::material_profile(Material::plastic).softening_point <
              sandhybrid::material_profile(Material::plastic).melting_point);
static_assert(sandhybrid::material_profile(Material::plastic).melting_point <
              sandhybrid::material_profile(Material::plastic).ignition_point);
static_assert(sandhybrid::material_profile(Material::plastic).conversions.find("SMOKE") != std::string_view::npos);
static_assert(sandhybrid::material_profile(Material::acid_resistant_plastic).acid_resistance >
              sandhybrid::material_profile(Material::plastic).acid_resistance);

static_assert(sandhybrid::policy::stability_ready(52u, 120u, true, true, false, false, 0u));
static_assert(!sandhybrid::policy::stability_ready(51u, 120u, true, true, false, false, 0u));
static_assert(sandhybrid::policy::laser_hits_to_dislodge == 2u);
static_assert(!sandhybrid::policy::should_collapse(34u));
static_assert(sandhybrid::policy::should_collapse(33u));
static_assert(!sandhybrid::policy::should_collapse(1u, true));
static_assert(sandhybrid::policy::update_vent_pressure(100u, true, false) == 105u);
static_assert(sandhybrid::policy::update_vent_pressure(100u, false, true) == 96u);
static_assert(sandhybrid::policy::bulk_region_eligible(64u, true, false, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(64u, true, false, false, true));
static_assert(!sandhybrid::policy::bulk_region_eligible(63u, true, false, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(64u, false, false, false, false));
static_assert(sandhybrid::policy::chunk_can_sleep(64u, 64u, false, 30u));
static_assert(!sandhybrid::policy::chunk_can_sleep(63u, 64u, false, 30u));
static_assert(!sandhybrid::policy::chunk_can_sleep(64u, 64u, true, 30u));

constexpr bool palette_materials_are_unique() {
    std::array<std::uint32_t, sandhybrid::material_count> counts{};
    for (const auto& group : sandhybrid::material_groups) {
        for (const auto material : group) {
            if (material == Material::count) continue;
            auto& count = counts[static_cast<std::uint32_t>(material)];
            if (++count != 1u) return false;
        }
    }
    return counts[static_cast<std::uint32_t>(Material::aluminum_shavings)] == 1u &&
           counts[static_cast<std::uint32_t>(Material::iron_ore)] == 1u &&
           counts[static_cast<std::uint32_t>(Material::atmosphere)] == 0u;
}
static_assert(palette_materials_are_unique());

int main() {
    return 0;
}
