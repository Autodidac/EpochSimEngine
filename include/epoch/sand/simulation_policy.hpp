#pragma once

#include <cstdint>

namespace epoch::sand::policy {

inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t tile_cells = tile_size * tile_size;
inline constexpr std::uint32_t reconstruction_occupancy = 52u;
inline constexpr std::uint32_t collapse_occupancy = tile_cells / 2u;
inline constexpr std::uint32_t reconstruction_stabilization_ticks = 120u;
inline constexpr std::uint32_t reconstruction_cooldown_ticks = 240u;
inline constexpr std::uint32_t water_pressure_depth = 8u;
inline constexpr std::uint32_t sunlight_update_interval = 4u;
inline constexpr std::uint32_t vent_eruption_pressure = 220u;
inline constexpr std::uint32_t vent_gas_release_pressure = 72u;

[[nodiscard]] constexpr bool reconstruction_ready(
    const std::uint32_t occupancy,
    const std::uint32_t stable_ticks,
    const bool compatible,
    const bool stable_phase,
    const bool moving,
    const bool reacting,
    const std::uint32_t cooldown) noexcept {
    return occupancy >= reconstruction_occupancy &&
           stable_ticks >= reconstruction_stabilization_ticks && compatible && stable_phase &&
           !moving && !reacting && cooldown == 0u;
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
