#pragma once

#include <cstdint>

namespace epoch::sand::policy {

inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t tile_cells = tile_size * tile_size;
inline constexpr std::uint32_t stability_occupancy = 52u;
inline constexpr std::uint32_t collapse_occupancy = tile_cells / 2u;
inline constexpr std::uint32_t stability_ticks = 120u;
inline constexpr std::uint32_t restabilization_cooldown_ticks = 240u;
inline constexpr std::uint32_t terrain_cell_integrity = 255u;
inline constexpr std::uint32_t laser_damage_per_hit = 144u;
inline constexpr std::uint32_t laser_hits_to_dislodge =
    (terrain_cell_integrity + laser_damage_per_hit - 1u) / laser_damage_per_hit;
inline constexpr std::uint32_t water_pressure_depth = 8u;
inline constexpr std::uint32_t sunlight_update_interval = 4u;
inline constexpr std::uint32_t vent_eruption_pressure = 220u;
inline constexpr std::uint32_t vent_gas_release_pressure = 72u;

[[nodiscard]] constexpr bool stability_ready(
    const std::uint32_t occupancy,
    const std::uint32_t settled_ticks,
    const bool compatible,
    const bool stable_phase,
    const bool moving,
    const bool reacting,
    const std::uint32_t cooldown) noexcept {
    return occupancy >= stability_occupancy && settled_ticks >= stability_ticks &&
           compatible && stable_phase && !moving && !reacting && cooldown == 0u;
}

[[nodiscard]] constexpr bool should_collapse(const std::uint32_t represented_cells) noexcept {
    return represented_cells < collapse_occupancy;
}

[[nodiscard]] constexpr std::uint32_t update_vent_pressure(
    const std::uint32_t pressure, const bool blocked, const bool open) noexcept {
    if (blocked) return pressure >= 252u ? 255u : pressure + 3u;
    if (open) return pressure > 4u ? pressure - 4u : 0u;
    return pressure == 255u ? 255u : pressure + 1u;
}

} // namespace epoch::sand::policy
