#pragma once

#include "sandhybrid/material.hpp"

#include <cstdint>

namespace sandhybrid::terrain {

inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t vein_cluster_tiles_x = 20u;
inline constexpr std::uint32_t vein_cluster_tiles_y = 12u;
inline constexpr std::uint32_t trap_cluster_tiles_x = 28u;
inline constexpr std::uint32_t trap_cluster_tiles_y = 16u;

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

[[nodiscard]] constexpr std::int32_t absolute(const std::int32_t value) noexcept {
    return value < 0 ? -value : value;
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
    if (depth >= 160u && (hash(cluster_x, cluster_y, 0x711du) & 4095u) < 100u)
        return Material::uranium;
    if ((hash(cluster_x, cluster_y, 0xa16fu) & 4095u) < 220u)
        return Material::aluminum;
    if ((hash(cluster_x, cluster_y, 0xc077u) & 4095u) < 420u)
        return Material::copper;
    if ((hash(cluster_x, cluster_y, 0x1a0fu) & 4095u) < 900u)
        return Material::iron_ore;
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
    const auto center_x = 6 + static_cast<std::int32_t>((seed >> 7u) % 8u);
    const auto center_y = 3 + static_cast<std::int32_t>((seed >> 13u) % 6u);
    const auto slope = static_cast<std::int32_t>((seed >> 19u) % 5u) - 2;
    const auto length = 6 + static_cast<std::int32_t>((seed >> 23u) % 4u);
    const auto width = 1 + static_cast<std::int32_t>((seed >> 27u) % 2u);
    const bool vertical = (seed & 1u) != 0u;

    std::int32_t along{};
    std::int32_t cross{};
    if (vertical) {
        const auto bend = static_cast<std::int32_t>(
  hash(cluster_x + static_cast<std::uint32_t>(local_y / 3 + 1),
       cluster_y, 0x52b1u) % 3u) - 1;
        const auto line_x = center_x + ((local_y - center_y) * slope) / 5 + bend;
        along = absolute(local_y - center_y);
        cross = absolute(local_x - line_x);
    } else {
        const auto bend = static_cast<std::int32_t>(
  hash(cluster_x, cluster_y + static_cast<std::uint32_t>(local_x / 3 + 1),
       0x52b1u) % 3u) - 1;
        const auto line_y = center_y + ((local_x - center_x) * slope) / 5 + bend;
        along = absolute(local_x - center_x);
        cross = absolute(local_y - line_y);
    }

    const auto edge_noise = static_cast<std::int32_t>(hash(tile_x, tile_y, seed) % 3u) - 1;
    return along <= length && cross <= width + edge_noise;
}

[[nodiscard]] constexpr bool vein_core_tile_signed(
    const std::int32_t tile_x,
    const std::int32_t tile_y,
    const std::uint32_t depth) noexcept {
    return tile_x >= 0 && tile_y >= 0 &&
 vein_core_tile(static_cast<std::uint32_t>(tile_x),
                static_cast<std::uint32_t>(tile_y), depth);
}

[[nodiscard]] constexpr Material neighboring_vein_material(
    const std::uint32_t tile_x,
    const std::uint32_t tile_y,
    const std::uint32_t depth) noexcept {
    for (std::int32_t offset_y = -1; offset_y <= 1; ++offset_y) {
        for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
  if (offset_x == 0 && offset_y == 0) continue;
  const auto neighbor_x = static_cast<std::int32_t>(tile_x) + offset_x;
  const auto neighbor_y = static_cast<std::int32_t>(tile_y) + offset_y;
  if (!vein_core_tile_signed(neighbor_x, neighbor_y, depth)) continue;
  return cluster_deposit(
      static_cast<std::uint32_t>(neighbor_x) / vein_cluster_tiles_x,
      static_cast<std::uint32_t>(neighbor_y) / vein_cluster_tiles_y,
      depth);
        }
    }
    return Material::empty;
}

[[nodiscard]] constexpr bool rubble_pocket_selected(
    const std::uint32_t tile_x,
    const std::uint32_t tile_y) noexcept {
    return (hash(tile_x, tile_y, 0xb04du) % 4u) == 0u;
}

[[nodiscard]] constexpr std::uint32_t rubble_distance(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t tile_x,
    const std::uint32_t tile_y) noexcept {
    const auto seed = hash(tile_x, tile_y, 0x8a21u);
    const auto center_x = 2 + static_cast<std::int32_t>((seed >> 5u) % 4u);
    const auto center_y = 2 + static_cast<std::int32_t>((seed >> 11u) % 4u);
    const auto local_x = static_cast<std::int32_t>(x % tile_size);
    const auto local_y = static_cast<std::int32_t>(y % tile_size);
    const auto dx = local_x - center_x;
    const auto dy = local_y - center_y;
    return static_cast<std::uint32_t>(dx * dx * 3 + dy * dy * 4);
}

[[nodiscard]] constexpr Material trap_resource(
    const std::uint32_t trap_cluster_x,
    const std::uint32_t trap_cluster_y,
    const std::uint32_t depth) noexcept {
    const auto roll = hash(trap_cluster_x, trap_cluster_y, 0x3a71u) & 1023u;
    if (depth >= 160u && roll < 38u) return Material::uranium;
    if (roll < 220u) return Material::copper;
    if (roll < 360u) return Material::aluminum;
    return Material::iron_ore;
}

[[nodiscard]] constexpr bool trap_selected(
    const std::uint32_t cluster_x,
    const std::uint32_t cluster_y,
    const std::uint32_t depth) noexcept {
    if (depth < 72u || depth > 320u) return false;
    return (hash(cluster_x, cluster_y, 0x6f31u) & 1023u) < 160u ||
           ((cluster_x + cluster_y * 3u) % 17u) == 0u;
}

[[nodiscard]] constexpr Sample sample(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    if (!host_material(base_material)) return {base_material, false, false, false};

    const auto core_tile_x = x / tile_size;
    const auto core_tile_y = y / tile_size;
    const auto core_depth = (depth / tile_size) * tile_size;
    const auto core_deposit = cluster_deposit(
        core_tile_x / vein_cluster_tiles_x,
        core_tile_y / vein_cluster_tiles_y,
        core_depth);
    if (core_deposit != Material::empty &&
        vein_core_tile(core_tile_x, core_tile_y, core_depth))
        return {core_deposit, true, false, false};

    constexpr auto trap_width_cells = trap_cluster_tiles_x * tile_size;
    constexpr auto trap_height_cells = trap_cluster_tiles_y * tile_size;
    const auto trap_cluster_x = x / trap_width_cells;
    const auto trap_cluster_y = y / trap_height_cells;
    if (base_material == Material::sand &&
        trap_selected(trap_cluster_x, trap_cluster_y, depth)) {
        const auto seed = hash(trap_cluster_x, trap_cluster_y, 0x7135u);
        const auto local_x = static_cast<std::int32_t>(x % trap_width_cells);
        const auto local_y = static_cast<std::int32_t>(y % trap_height_cells);
        const auto center_x = 72 + static_cast<std::int32_t>(seed % 80u);
        const auto center_y = 48 + static_cast<std::int32_t>((seed >> 8u) % 28u);
        const auto radius_x = 28 + static_cast<std::int32_t>((seed >> 15u) % 18u);
        const auto radius_y = 16 + static_cast<std::int32_t>((seed >> 21u) % 10u);
        const auto dx = local_x - center_x;
        const auto dy = local_y - center_y;
        const auto ellipse = dx * dx * radius_y * radius_y +
                   dy * dy * radius_x * radius_x;
        const auto limit = radius_x * radius_x * radius_y * radius_y;
        if (ellipse <= limit) return {Material::empty, false, false, true};

        const auto roof_y = center_y - radius_y +
  (dx * dx * 5) / (radius_x * radius_x);
        const bool loose_roof = absolute(dx) <= radius_x &&
                      local_y >= roof_y - 4 && local_y <= roof_y + 3;
        if (loose_roof) {
  const bool concentrated = absolute(dx) * 3 < radius_x * 2;
  const bool resource_cell = concentrated &&
      (hash(x, y, 0x10c5u) % 11u) == 0u;
  return {resource_cell
              ? trap_resource(trap_cluster_x, trap_cluster_y, depth)
              : Material::sand,
          false, true, true};
        }
    }

    const auto tile_x = x / tile_size;
    const auto tile_y = y / tile_size;
    const auto tile_depth = (depth / tile_size) * tile_size;
    const auto deposit = cluster_deposit(
        tile_x / vein_cluster_tiles_x, tile_y / vein_cluster_tiles_y, tile_depth);
    if (deposit != Material::empty && vein_core_tile(tile_x, tile_y, tile_depth))
        return {deposit, true, false, false};

    const auto neighboring_deposit = neighboring_vein_material(
        tile_x, tile_y, tile_depth);
    if ((base_material == Material::sand || base_material == Material::silt) &&
        neighboring_deposit != Material::empty && rubble_pocket_selected(tile_x, tile_y)) {
        const auto distance = rubble_distance(x, y, tile_x, tile_y);
        if (distance <= 11u) {
  const bool resource_cell = (hash(x, y, 0x441du) % 5u) == 0u;
  return {resource_cell ? neighboring_deposit : Material::empty,
          false, true, false};
        }
        if (distance <= 28u) return {base_material, false, true, false};
    }

    return {base_material, true, false, false};
}

} // namespace sandhybrid::terrain
