#pragma once

#include "sandhybrid/material.hpp"

#include <cstdint>

namespace sandhybrid::terrain {

inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t vein_cluster_tiles_x = 12u;
inline constexpr std::uint32_t vein_cluster_tiles_y = 8u;
inline constexpr std::uint32_t trap_cluster_tiles_x = 24u;
inline constexpr std::uint32_t trap_cluster_tiles_y = 14u;

struct Sample final {
    Material material{Material::empty};
    bool structural{};
    bool deliberate_loose{};
    bool sand_trap{};
};

[[nodiscard]] constexpr std::uint32_t hash(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t salt = 0u) noexcept {
    auto value = x * 0x9e3779b9u ^ y * 0x85ebca6bu ^ salt * 0xc2b2ae35u;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

[[nodiscard]] constexpr bool host_material(const Material material) noexcept {
    return material == Material::sand || material == Material::dirt ||
           material == Material::silt || material == Material::mud ||
           material == Material::stone;
}

[[nodiscard]] constexpr Material cluster_deposit(
    const std::uint32_t cluster_x,
    const std::uint32_t cluster_y,
    const std::uint32_t depth) noexcept {
    const auto roll = hash(cluster_x, cluster_y, 0x51e1u) & 4095u;
    if (depth >= 160u && roll < 18u) return Material::uranium;
    if (roll < 50u) return Material::aluminum;
    if (roll < 160u) return Material::copper;
    if (roll < 460u) return Material::iron_ore;
    return Material::empty;
}

[[nodiscard]] constexpr bool vein_core_tile(
    const std::uint32_t tile_x,
    const std::uint32_t tile_y,
    const std::uint32_t depth) noexcept {
    const auto cluster_x = tile_x / vein_cluster_tiles_x;
    const auto cluster_y = tile_y / vein_cluster_tiles_y;
    if (cluster_deposit(cluster_x, cluster_y, depth) == Material::empty) return false;
    const auto seed = hash(cluster_x, cluster_y, 0x9b7du);
    const auto local_x = static_cast<std::int32_t>(tile_x % vein_cluster_tiles_x);
    const auto local_y = static_cast<std::int32_t>(tile_y % vein_cluster_tiles_y);
    const auto center_x = 3 + static_cast<std::int32_t>((seed >> 8u) % 6u);
    const auto center_y = 2 + static_cast<std::int32_t>((seed >> 14u) % 4u);
    const auto radius_x = 2 + static_cast<std::int32_t>((seed >> 20u) % 3u);
    const auto radius_y = 1 + static_cast<std::int32_t>((seed >> 24u) % 3u);
    const auto dx = local_x - center_x;
    const auto dy = local_y - center_y;
    const auto ellipse = dx * dx * radius_y * radius_y +
                         dy * dy * radius_x * radius_x;
    const auto limit = radius_x * radius_x * radius_y * radius_y;
    const auto rough = static_cast<std::int32_t>(hash(tile_x, tile_y, seed) & 3u) - 1;
    return ellipse <= limit + rough * radius_x;
}

[[nodiscard]] constexpr bool loose_inclusion_cell(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    if (base_material != Material::sand) return false;
    const auto tile_x = x / tile_size;
    const auto tile_y = y / tile_size;
    const auto cluster_x = tile_x / vein_cluster_tiles_x;
    const auto cluster_y = tile_y / vein_cluster_tiles_y;
    if (cluster_deposit(cluster_x, cluster_y, depth) == Material::empty) return false;
    if (vein_core_tile(tile_x, tile_y, depth)) return false;
    bool touches_core = false;
    for (std::int32_t oy = -1; oy <= 1; ++oy) {
        for (std::int32_t ox = -1; ox <= 1; ++ox) {
            if (ox == 0 && oy == 0) continue;
            const auto nx = static_cast<std::int32_t>(tile_x) + ox;
            const auto ny = static_cast<std::int32_t>(tile_y) + oy;
            if (nx >= 0 && ny >= 0 && vein_core_tile(
                    static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny), depth)) {
                touches_core = true;
            }
        }
    }
    return touches_core && (hash(x, y, 0x10c5u) % 73u) == 0u;
}

[[nodiscard]] constexpr bool trap_selected(
    const std::uint32_t tile_x,
    const std::uint32_t tile_y,
    const std::uint32_t depth) noexcept {
    if (depth < 72u || depth > 300u) return false;
    const auto cluster_x = tile_x / trap_cluster_tiles_x;
    const auto cluster_y = tile_y / trap_cluster_tiles_y;
    return ((cluster_x + cluster_y * 3u) % 11u) == 0u;
}

[[nodiscard]] constexpr Sample sample(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    if (!host_material(base_material)) return {base_material, false, false, false};

    const auto tile_x = x / tile_size;
    const auto tile_y = y / tile_size;
    if (base_material == Material::sand && trap_selected(tile_x, tile_y, depth)) {
        const auto local_x = tile_x % trap_cluster_tiles_x;
        const auto local_y = tile_y % trap_cluster_tiles_y;
        const auto seed = hash(tile_x / trap_cluster_tiles_x, tile_y / trap_cluster_tiles_y, 0x7135u);
        const auto center_x = 8u + (seed % 8u);
        const auto roof_y = 4u + ((seed >> 8u) % 4u);
        const bool chamber = local_x + 3u >= center_x && local_x <= center_x + 3u &&
                             local_y > roof_y && local_y <= roof_y + 3u;
        const bool loose_roof = local_x + 2u >= center_x && local_x <= center_x + 2u &&
                                local_y == roof_y;
        if (chamber) return {Material::empty, false, false, true};
        if (loose_roof) return {Material::sand, false, true, true};
    }

    const auto cluster_x = tile_x / vein_cluster_tiles_x;
    const auto cluster_y = tile_y / vein_cluster_tiles_y;
    const auto deposit = cluster_deposit(cluster_x, cluster_y, depth);
    if (deposit != Material::empty && vein_core_tile(tile_x, tile_y, depth))
        return {deposit, true, false, false};
    if (deposit != Material::empty && loose_inclusion_cell(base_material, x, y, depth))
        return {deposit, false, true, false};
    return {base_material, true, false, false};
}

} // namespace sandhybrid::terrain
