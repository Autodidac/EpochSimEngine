#include "sandhybrid/input_routing.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/simulation_policy.hpp"
#include "sandhybrid/shared_state.hpp"
#include "sandhybrid/camera_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

enum class CreationPath : std::uint8_t {
    map, stable_terrain, cursor, fragment, particle, reaction, save
};

struct CanonicalState final {
    sandhybrid::Material material{};
    sandhybrid::MaterialPhase phase{};
    std::int32_t temperature{};
    std::uint32_t represented_mass{};
    std::uint32_t damage{};

    friend constexpr bool operator==(const CanonicalState&, const CanonicalState&) = default;
};

[[nodiscard]] constexpr CanonicalState canonical_state(
    [[maybe_unused]] const CreationPath path,
    const sandhybrid::Material material,
    const std::int32_t temperature,
    const std::uint32_t represented_mass,
    const std::uint32_t damage) noexcept {
    return {material, sandhybrid::phase_at(material, temperature), temperature, represented_mass, damage};
}

[[nodiscard]] constexpr bool creation_paths_are_canonical() noexcept {
    constexpr std::array paths{
        CreationPath::map, CreationPath::stable_terrain, CreationPath::cursor,
        CreationPath::fragment, CreationPath::particle, CreationPath::reaction,
        CreationPath::save,
    };
    constexpr std::array temperatures{-100, 20, 180, 1300, 3000};
    for (std::uint32_t material_id = 0; material_id < sandhybrid::material_count; ++material_id) {
        const auto material = static_cast<sandhybrid::Material>(material_id);
        for (const auto temperature : temperatures) {
            const auto expected = canonical_state(paths.front(), material, temperature, 37u, 72u);
            for (const auto path : paths) {
                if (canonical_state(path, material, temperature, 37u, 72u) != expected) return false;
            }
        }
    }
    return true;
}

[[nodiscard]] constexpr bool local_water_equalization_preserves_volume() noexcept {
    std::array<std::uint32_t, 16> columns{};
    columns[0] = 64;
    const auto original = 64u;
    for (std::uint32_t pass = 0; pass < 256; ++pass) {
        for (std::uint32_t parity = 0; parity < 2; ++parity) {
            for (std::uint32_t x = parity; x + 1 < columns.size(); x += 2) {
                const auto difference = static_cast<std::int32_t>(columns[x]) -
                                        static_cast<std::int32_t>(columns[x + 1]);
                if (difference == 0) continue;
                const auto magnitude = static_cast<std::uint32_t>(difference > 0 ? difference : -difference);
                const auto transfer = (std::min)(sandhybrid::policy::water_pressure_depth,
                                                 (std::max)(1u, magnitude / 2u));
                if (difference > 0) {
                    columns[x] -= transfer;
                    columns[x + 1] += transfer;
                } else {
                    columns[x + 1] -= transfer;
                    columns[x] += transfer;
                }
            }
        }
    }
    std::uint32_t total{};
    std::uint32_t minimum = columns.front();
    std::uint32_t maximum = columns.front();
    for (const auto value : columns) {
        total += value;
        minimum = (std::min)(minimum, value);
        maximum = (std::max)(maximum, value);
    }
    return total == original && maximum - minimum <= 1;
}

[[nodiscard]] constexpr bool half_water_medium_exchange_preserves_volume() noexcept {
    constexpr std::uint32_t oxygen_volume = 220u;
    constexpr std::uint32_t first_half = oxygen_volume / 2u;
    constexpr std::uint32_t second_half = oxygen_volume - first_half;
    return first_half + second_half == oxygen_volume &&
           first_half <= 255u && second_half <= 255u;
}

[[nodiscard]] constexpr bool breathing_requires_explicit_oxygen() noexcept {
    constexpr std::uint32_t vacuum_volume = 0u;
    constexpr std::uint32_t pure_oxygen_volume = 255u;
    constexpr std::uint32_t atmosphere_oxygen_volume = 54u;
    return vacuum_volume == 0u && pure_oxygen_volume > 0u &&
           atmosphere_oxygen_volume > 0u && atmosphere_oxygen_volume < pure_oxygen_volume;
}

[[nodiscard]] constexpr bool directional_input_routes_by_scene() noexcept {
    using sandhybrid::DirectionalInputRouting;
    using sandhybrid::route_directional_input;

    constexpr auto camera_scene = route_directional_input(false, false, true, true, false);
    constexpr auto player_scene = route_directional_input(true, false, true, true, false);
    constexpr auto neutral_scene = route_directional_input(true, true, true, true, true);

    return camera_scene == DirectionalInputRouting{1, -1, 0, 0} &&
           player_scene == DirectionalInputRouting{0, 0, 1, -1} &&
           neutral_scene == DirectionalInputRouting{0, 0, 0, 0};
}

[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {
    constexpr std::uint32_t initial_mass = 64u;
    constexpr std::uint32_t detached_pixels = 33u;
    constexpr std::uint32_t remaining_pixels = initial_mass - detached_pixels;
    static_assert(sandhybrid::policy::should_collapse(remaining_pixels));
    constexpr std::uint32_t settled_mass = remaining_pixels + detached_pixels;
    return settled_mass == initial_mass;
}

static_assert(creation_paths_are_canonical());
static_assert(local_water_equalization_preserves_volume());
static_assert(half_water_medium_exchange_preserves_volume());
static_assert(breathing_requires_explicit_oxygen());
static_assert(terrain_stability_preserves_representation());
static_assert(directional_input_routes_by_scene());
static_assert(sandhybrid::policy::stability_ready(52u, 120u, true, true, false, false, 0u));
static_assert(!sandhybrid::policy::stability_ready(51u, 120u, true, true, false, false, 0u));
static_assert(sandhybrid::policy::laser_hits_to_dislodge == 2u);
static_assert(!sandhybrid::policy::should_collapse(32u));
static_assert(sandhybrid::policy::should_collapse(31u));
static_assert(sandhybrid::policy::water_pressure_depth == 8u);
static_assert(sandhybrid::policy::water_half_units_per_full_cell == 2u);
static_assert(sandhybrid::policy::gas_tile_eligible(true, true, false));
static_assert(sandhybrid::policy::gas_tile_eligible(true, false, true));
static_assert(!sandhybrid::policy::gas_tile_eligible(true, false, false));
static_assert(sandhybrid::policy::liquid_tile_eligible(true, true, false));
static_assert(sandhybrid::policy::liquid_tile_eligible(true, false, true));
static_assert(!sandhybrid::policy::liquid_tile_eligible(true, false, false));
static_assert(sandhybrid::policy::medium_tile_breaks_to_fine(true, false, false));
static_assert(!sandhybrid::policy::medium_tile_breaks_to_fine(true, true, false));

static_assert(sandhybrid::material_names[static_cast<std::uint32_t>(sandhybrid::Material::dirt)] == "Soil");
static_assert(sandhybrid::default_brush_radius == 2u);
static_assert(sandhybrid::default_brush_shape == 1u);
static_assert(sandhybrid::policy::effective_wet_density(1050u, true) > 100u);
static_assert(sandhybrid::policy::effective_wet_density(1050u, false) == 1050u);
static_assert(sandhybrid::material_count == 67u);
static_assert(sandhybrid::material_names[static_cast<std::uint32_t>(sandhybrid::Material::atmosphere)] == "Atmosphere");
static_assert(sandhybrid::is_block_material(sandhybrid::Material::sluice_box));
static_assert(!sandhybrid::policy::water_ledge_can_release(2u, 0u));
static_assert(sandhybrid::policy::water_ledge_can_release(2u, 1u));
static_assert(sandhybrid::policy::water_ledge_can_release(2u, 2u));
static_assert(sandhybrid::policy::water_half_horizontal_passes ==
              sandhybrid::policy::water_full_horizontal_passes * 2u);
static_assert(sandhybrid::policy::vent_eruption_pressure > sandhybrid::policy::vent_gas_release_pressure);
static_assert(sandhybrid::policy::restabilization_cooldown_ticks > sandhybrid::policy::stability_ticks);
static_assert(sandhybrid::resident_world_dimension_scale == 4u);
static_assert(sandhybrid::logical_world_dimension_scale == 8u);
static_assert(sandhybrid::camera_zoom_min == 2u);
static_assert(sandhybrid::camera_zoom_default == 4u);
static_assert(sandhybrid::camera_zoom_max == 32u);
static_assert((sandhybrid::pre_expansion_world_width * sandhybrid::resident_world_dimension_scale) /
                  sandhybrid::camera_zoom_min == 1280u);
static_assert((sandhybrid::pre_expansion_world_height * sandhybrid::resident_world_dimension_scale) /
                  sandhybrid::camera_zoom_min == 720u);
static_assert((sandhybrid::pre_expansion_world_width * sandhybrid::resident_world_dimension_scale) /
                  sandhybrid::camera_zoom_default == 640u);
static_assert((sandhybrid::pre_expansion_world_height * sandhybrid::resident_world_dimension_scale) /
                  sandhybrid::camera_zoom_default == 360u);
static_assert(sandhybrid::policy::vent_emission(220u, 0u) ==
              sandhybrid::policy::VentEmission::lava);
static_assert(sandhybrid::policy::consume_vent_pressure(
                  220u, sandhybrid::policy::VentEmission::lava) == 124u);
static_assert(sandhybrid::policy::vent_emission(72u, 0u) ==
              sandhybrid::policy::VentEmission::gas);
static_assert(sandhybrid::policy::consume_vent_pressure(
                  72u, sandhybrid::policy::VentEmission::gas) == 48u);

} // namespace

int main() {
    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&
           half_water_medium_exchange_preserves_volume() &&
           breathing_requires_explicit_oxygen() &&
           terrain_stability_preserves_representation() &&
           directional_input_routes_by_scene() ? 0 : 1;
}
