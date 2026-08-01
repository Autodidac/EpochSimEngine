#pragma once

#include <cstdint>

namespace sandhybrid::policy {

// Fine cells remain the canonical simulation state. An aligned 8x8 region may
// be transferred as one macro-cell only while every represented cell can make
// the same move. Any mixed or partially blocked region falls back to pixels.
inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t tile_cells = tile_size * tile_size;
inline constexpr std::uint32_t macro_tile_size = tile_size;
inline constexpr std::uint32_t macro_tile_cells = tile_cells;

// Eight macro tiles form one 64x64-cell scheduling chunk. Chunks never replace
// cell storage; they cache activity so large inactive areas can be rejected by
// one lookup and their tile scans can be skipped until explicitly dirtied.
inline constexpr std::uint32_t chunk_tiles_per_axis = 8u;
inline constexpr std::uint32_t chunk_size = tile_size * chunk_tiles_per_axis;
inline constexpr std::uint32_t chunk_tile_count =
    chunk_tiles_per_axis * chunk_tiles_per_axis;
inline constexpr std::uint32_t chunk_sleep_ticks = 30u;

inline constexpr std::uint32_t stability_occupancy = 52u;
inline constexpr std::uint32_t collapse_occupancy = tile_cells / 2u;
inline constexpr std::uint32_t stability_ticks = 120u;
inline constexpr std::uint32_t restabilization_cooldown_ticks = 240u;
inline constexpr std::uint32_t terrain_cell_integrity = 255u;
inline constexpr std::uint32_t laser_damage_per_hit = 144u;
inline constexpr std::uint32_t laser_hits_to_dislodge =
    (terrain_cell_integrity + laser_damage_per_hit - 1u) / laser_damage_per_hit;
inline constexpr std::uint32_t water_pressure_depth = 8u;
inline constexpr std::uint32_t water_half_units_per_full_cell = 2u;
inline constexpr std::uint32_t water_ledge_release_units = 3u;
inline constexpr std::uint32_t water_full_horizontal_passes = 4u;
inline constexpr std::uint32_t water_half_horizontal_passes = 8u;
inline constexpr std::uint32_t sunlight_update_interval = 4u;
inline constexpr std::uint32_t vent_eruption_pressure = 220u;
inline constexpr std::uint32_t vent_gas_release_pressure = 72u;

[[nodiscard]] constexpr bool water_ledge_can_release(
    const std::uint32_t edge_units,
    const std::uint32_t trailing_units) noexcept {
    return edge_units == water_half_units_per_full_cell &&
           edge_units + trailing_units >= water_ledge_release_units;
}

[[nodiscard]] constexpr bool bulk_region_eligible(
    const std::uint32_t represented_cells,
    const bool uniform_material,
    const bool structural,
    const bool reacting) noexcept {
    return represented_cells == macro_tile_cells && uniform_material &&
 !structural && !reacting;
}


// A full gas/liquid region is a temporary 8x8 transfer packet at exposed
// boundaries. Once packet motion ends, only a completely compatible perimeter
// may retain coarse ownership; otherwise the canonical fine cells take over.
[[nodiscard]] constexpr bool gas_tile_eligible(
    const bool full_region,
    const bool moving,
    const bool perimeter_is_all_gas) noexcept {
    return full_region && (moving || perimeter_is_all_gas);
}

[[nodiscard]] constexpr bool liquid_tile_eligible(
    const bool full_region,
    const bool moving,
    const bool perimeter_has_only_liquid_or_solid) noexcept {
    return full_region && (moving || perimeter_has_only_liquid_or_solid);
}

[[nodiscard]] constexpr bool medium_tile_breaks_to_fine(
    const bool full_region,
    const bool moving,
    const bool perimeter_compatible) noexcept {
    return full_region && !moving && !perimeter_compatible;
}

[[nodiscard]] constexpr bool chunk_can_sleep(
    const std::uint32_t sleeping_tiles,
    const std::uint32_t present_tiles,
    const bool dirty,
    const std::uint32_t quiet_ticks) noexcept {
    return present_tiles != 0u && sleeping_tiles == present_tiles && !dirty &&
 quiet_ticks >= chunk_sleep_ticks;
}

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

enum class VentEmission : std::uint8_t { none, gas, lava };

[[nodiscard]] constexpr VentEmission vent_emission(
    const std::uint32_t pressure, const std::uint32_t random_value) noexcept {
    if (pressure >= vent_eruption_pressure && (random_value & 3u) == 0u)
        return VentEmission::lava;
    if (pressure >= vent_gas_release_pressure && (random_value & 15u) == 0u)
        return VentEmission::gas;
    return VentEmission::none;
}

[[nodiscard]] constexpr std::uint32_t vent_emission_cost(
    const VentEmission emission) noexcept {
    return emission == VentEmission::lava ? 96u :
           emission == VentEmission::gas ? 24u : 0u;
}

[[nodiscard]] constexpr std::uint32_t consume_vent_pressure(
    const std::uint32_t pressure, const VentEmission emission) noexcept {
    const auto cost = vent_emission_cost(emission);
    return pressure > cost ? pressure - cost : 0u;
}

[[nodiscard]] constexpr std::uint32_t update_vent_pressure(
    const std::uint32_t pressure, const bool blocked, const bool open) noexcept {
    if (blocked) return pressure >= 250u ? 255u : pressure + 5u;
    if (open) return pressure > 4u ? pressure - 4u : 0u;
    return pressure == 255u ? 255u : pressure + 1u;
}

} // namespace sandhybrid::policy
