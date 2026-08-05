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
// Granular stabilized terrain may release below this occupancy.
// Block-capable structural tile placements are explicitly exempt.
inline constexpr std::uint32_t destroyed_cells_to_crumble = 31u;
inline constexpr std::uint32_t collapse_occupancy =
    tile_cells - destroyed_cells_to_crumble + 1u;
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
inline constexpr std::uint32_t canonical_air_state = 54u;
inline constexpr bool half_water_stores_ambient_air_pressure = false;
inline constexpr std::uint32_t half_water_attraction_min_cells = 2u;
inline constexpr std::uint32_t half_water_attraction_max_cells = 4u;
inline constexpr std::uint32_t half_water_rest_ticks = 12u;
inline constexpr std::uint32_t sunlight_update_interval = 4u;
inline constexpr std::uint32_t day_cycle_steps = 21'600u; // six minutes at 60 simulation steps/s
inline constexpr std::uint32_t vent_eruption_pressure = 176u;
inline constexpr std::uint32_t vent_pulse_lava_pressure = 144u;
inline constexpr std::uint32_t vent_gas_release_pressure = 48u;
inline constexpr std::uint32_t vent_cycle_ticks = 10'800u;
inline constexpr std::uint32_t vent_pulse_ticks = 900u;
inline constexpr std::uint32_t vent_pulse_on_ticks = 420u;
inline constexpr std::uint32_t vent_major_start_tick = 10'200u;
inline constexpr std::uint32_t wet_density_bonus = 32u;


[[nodiscard]] constexpr bool half_water_attraction_distance(
    const std::uint32_t distance,
    const bool clear_path) noexcept {
    return clear_path && distance >= half_water_attraction_min_cells &&
           distance <= half_water_attraction_max_cells;
}

[[nodiscard]] constexpr bool half_water_can_sleep(
    const std::uint32_t age,
    const bool moving,
    const bool reacting,
    const bool hot,
    const bool attraction_pending) noexcept {
    return age >= half_water_rest_ticks && !moving && !reacting && !hot &&
           !attraction_pending;
}

[[nodiscard]] constexpr bool medium_packet_tries_macro(
    const bool full_region,
    const bool half_water,
    const bool structural,
    const bool reacting) noexcept {
    return full_region && !half_water && !structural && !reacting;
}

[[nodiscard]] constexpr bool medium_packet_needs_fine_fallback(
    const bool full_region,
    const bool macro_moved,
    const bool perimeter_compatible,
    const bool productive_move) noexcept {
    return full_region && !macro_moved &&
           (!perimeter_compatible || productive_move);
}

[[nodiscard]] constexpr bool simulation_clock_advances(
    const bool paused,
    const bool reset_this_frame,
    const bool simulation_tick,
    const bool single_step) noexcept {
    return !reset_this_frame && (single_step || (simulation_tick && !paused));
}

[[nodiscard]] constexpr bool map_snapshot_refresh_allowed(
    const bool paused,
    const bool reset_this_frame,
    const bool visible) noexcept {
    return visible && !paused && !reset_this_frame;
}

[[nodiscard]] constexpr std::uint32_t effective_wet_density(
    const std::uint32_t dry_density, const bool wet) noexcept {
    return dry_density + (wet ? wet_density_bonus : 0u);
}

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
    const bool reacting,
    const bool block_capable = false) noexcept {
    return represented_cells == macro_tile_cells && uniform_material &&
           !structural && !reacting && !block_capable;
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

[[nodiscard]] constexpr bool should_collapse(
    const std::uint32_t represented_cells,
    const bool durable_structural = false) noexcept {
    return !durable_structural && represented_cells < collapse_occupancy;
}

enum class VentEmission : std::uint8_t { none, gas, lava };

[[nodiscard]] constexpr bool vent_pulse_active(
    const std::uint32_t cycle_tick) noexcept {
    return cycle_tick % vent_pulse_ticks < vent_pulse_on_ticks;
}

[[nodiscard]] constexpr bool vent_major_active(
    const std::uint32_t cycle_tick) noexcept {
    return cycle_tick % vent_cycle_ticks >= vent_major_start_tick;
}

[[nodiscard]] constexpr VentEmission vent_emission(
    const std::uint32_t pressure, const std::uint32_t random_value,
    const std::uint32_t cycle_tick = 0u) noexcept {
    if (vent_major_active(cycle_tick) && pressure >= vent_eruption_pressure &&
        (random_value & 1u) == 0u)
        return VentEmission::lava;
    if (vent_pulse_active(cycle_tick) && pressure >= vent_pulse_lava_pressure &&
        (random_value & 3u) == 0u)
        return VentEmission::lava;
    if (vent_pulse_active(cycle_tick) && pressure >= vent_gas_release_pressure &&
        (random_value & 3u) != 0u)
        return VentEmission::gas;
    return VentEmission::none;
}

[[nodiscard]] constexpr std::uint32_t vent_emission_cost(
    const VentEmission emission) noexcept {
    return emission == VentEmission::lava ? 20u :
           emission == VentEmission::gas ? 6u : 0u;
}

[[nodiscard]] constexpr std::uint32_t consume_vent_pressure(
    const std::uint32_t pressure, const VentEmission emission) noexcept {
    const auto cost = vent_emission_cost(emission);
    return pressure > cost ? pressure - cost : 0u;
}

[[nodiscard]] constexpr std::uint32_t update_vent_pressure(
    const std::uint32_t pressure, const bool blocked, const bool open) noexcept {
    if (blocked) return pressure >= 236u ? 255u : pressure + 20u;
    if (open) return pressure >= 247u ? 255u : pressure + 8u;
    return pressure >= 231u ? 255u : pressure + 24u;
}

} // namespace sandhybrid::policy
