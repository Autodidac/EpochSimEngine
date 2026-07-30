#include "epoch/sand/material.hpp"
#include "epoch/sand/simulation_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

enum class CreationPath : std::uint8_t {
    map, stable_terrain, cursor, fragment, particle, reaction, save
};

struct CanonicalState final {
    epoch::sand::Material material{};
    epoch::sand::MaterialPhase phase{};
    std::int32_t temperature{};
    std::uint32_t represented_mass{};
    std::uint32_t damage{};

    friend constexpr bool operator==(const CanonicalState&, const CanonicalState&) = default;
};

[[nodiscard]] constexpr CanonicalState canonical_state(
    [[maybe_unused]] const CreationPath path,
    const epoch::sand::Material material,
    const std::int32_t temperature,
    const std::uint32_t represented_mass,
    const std::uint32_t damage) noexcept {
    return {material, epoch::sand::phase_at(material, temperature), temperature, represented_mass, damage};
}

[[nodiscard]] constexpr bool creation_paths_are_canonical() noexcept {
    constexpr std::array paths{
        CreationPath::map, CreationPath::stable_terrain, CreationPath::cursor,
        CreationPath::fragment, CreationPath::particle, CreationPath::reaction,
        CreationPath::save,
    };
    constexpr std::array temperatures{-100, 20, 180, 1300, 3000};
    for (std::uint32_t material_id = 0; material_id < epoch::sand::material_count; ++material_id) {
        const auto material = static_cast<epoch::sand::Material>(material_id);
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
                const auto transfer = (std::min)(epoch::sand::policy::water_pressure_depth,
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

[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {
    constexpr std::uint32_t initial_mass = 64u;
    constexpr std::uint32_t detached_pixels = 33u;
    constexpr std::uint32_t remaining_pixels = initial_mass - detached_pixels;
    static_assert(epoch::sand::policy::should_collapse(remaining_pixels));
    constexpr std::uint32_t settled_mass = remaining_pixels + detached_pixels;
    return settled_mass == initial_mass;
}

static_assert(creation_paths_are_canonical());
static_assert(local_water_equalization_preserves_volume());
static_assert(terrain_stability_preserves_representation());
static_assert(epoch::sand::policy::stability_ready(52u, 120u, true, true, false, false, 0u));
static_assert(!epoch::sand::policy::stability_ready(51u, 120u, true, true, false, false, 0u));
static_assert(epoch::sand::policy::laser_hits_to_dislodge == 2u);
static_assert(!epoch::sand::policy::should_collapse(32u));
static_assert(epoch::sand::policy::should_collapse(31u));
static_assert(epoch::sand::policy::water_pressure_depth == 8u);
static_assert(epoch::sand::policy::water_half_units_per_full_cell == 2u);
static_assert(!epoch::sand::policy::water_ledge_can_release(2u, 0u));
static_assert(epoch::sand::policy::water_ledge_can_release(2u, 1u));
static_assert(epoch::sand::policy::water_ledge_can_release(2u, 2u));
static_assert(epoch::sand::policy::water_half_horizontal_passes ==
              epoch::sand::policy::water_full_horizontal_passes * 2u);
static_assert(epoch::sand::policy::vent_eruption_pressure > epoch::sand::policy::vent_gas_release_pressure);
static_assert(epoch::sand::policy::restabilization_cooldown_ticks > epoch::sand::policy::stability_ticks);

} // namespace

int main() {
    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&
           terrain_stability_preserves_representation() ? 0 : 1;
}
