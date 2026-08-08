#pragma once

#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/terrain_generation.hpp"

#include <algorithm>
#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t authored_scene_foundation_cells = 8u;
inline constexpr std::uint32_t resident_world_shell_cells = 8u;
inline constexpr std::uint32_t resident_world_lava_cells = 16u;
inline constexpr std::uint32_t resident_world_geology_patch_cells = 64u;
inline constexpr std::uint32_t authored_scene_sky_footprint_rows = 2u;
inline constexpr std::uint32_t subterranean_zone_count =
    resident_world_footprint_rows - 1u;

static_assert(subterranean_zone_count == 3u);
static_assert(authored_scene_foundation_cells == 8u);
static_assert(authored_scene_sky_footprint_rows == 2u);
static_assert(resident_world_lava_cells == 2u * authored_scene_foundation_cells);

[[nodiscard]] constexpr std::uint32_t authored_scene_origin_x(
    const std::uint32_t world_width) noexcept {
    // A complete authored scene occupies exactly one 640-cell camera region.
    // The 16x4 world still grows only to the right; the scene begins at region 2.
    constexpr auto preserved_origin = pre_expansion_world_width * 2u;
    return world_width >= preserved_origin + pre_expansion_world_width
        ? preserved_origin
        : (world_width > pre_expansion_world_width
            ? (world_width - pre_expansion_world_width) / 2u : 0u);
}

[[nodiscard]] constexpr std::uint32_t authored_scene_origin_y(
    const std::uint32_t world_height) noexcept {
    const auto sky_height = pre_expansion_world_height * authored_scene_sky_footprint_rows;
    return world_height >= sky_height + pre_expansion_world_height ? sky_height : 0u;
}

[[nodiscard]] constexpr std::uint32_t resident_ground_hash(
    const std::uint32_t x, const std::uint32_t y) noexcept {
    auto value = x * 0x9e3779b9u ^ y * 0x85ebca6bu ^ 0xc2b2ae35u;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

[[nodiscard]] constexpr bool resident_ground_host_material(
    const Material material) noexcept {
    return material == Material::sand || material == Material::dirt ||
           material == Material::silt || material == Material::mud ||
           material == Material::stone;
}

[[nodiscard]] constexpr bool resident_substrate_is_structural(
    const Material material) noexcept {
    return material == Material::grass || resident_ground_host_material(material) ||
           material == Material::iron_ore || material == Material::copper ||
           material == Material::aluminum || material == Material::uranium;
}

[[nodiscard]] constexpr std::int32_t resident_ground_boundary_offset(
    const std::uint32_t x,
    const std::uint32_t salt,
    const std::int32_t amplitude) noexcept {
    const auto coarse = resident_ground_hash(x / 24u, salt) %
        static_cast<std::uint32_t>(amplitude * 2 + 1);
    const auto fine = resident_ground_hash(x / 7u, salt ^ 0x51f2a3b7u) % 7u;
    return static_cast<std::int32_t>(coarse) - amplitude +
           static_cast<std::int32_t>(fine) - 3;
}

[[nodiscard]] constexpr bool resident_ground_rough_edge_cell(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t salt) noexcept {
    return (resident_ground_hash(x / 3u, y / 3u + salt) & 15u) == 0u;
}

[[nodiscard]] constexpr terrain::Sample resident_ground_sample(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    return terrain::sample(base_material, x, y, depth);
}

[[nodiscard]] constexpr Material resident_biome_material(
    const std::uint32_t biome_index) noexcept {
    const auto roll = resident_ground_hash(biome_index, 0x4b17u) % 3u;
    return roll == 0u ? Material::dirt :
 (roll == 1u ? Material::sand : Material::silt);
}

[[nodiscard]] constexpr Material resident_transition_material(
    const Material first,
    const Material second,
    const std::int32_t signed_distance,
    const std::int32_t half_width,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t salt) noexcept {
    if (signed_distance <= -half_width) return first;
    if (signed_distance >= half_width) return second;
    const auto numerator = static_cast<std::uint32_t>(signed_distance + half_width);
    const auto denominator = static_cast<std::uint32_t>(half_width * 2);
    const auto threshold = numerator * 256u / denominator;
    return (resident_ground_hash(x / 2u, y / 2u + salt) & 255u) < threshold
        ? second : first;
}

[[nodiscard]] constexpr Material resident_surface_biome(
    const std::uint32_t world_width,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    const auto span = (std::max)(world_width / 12u, 256u);
    const auto biome = x / span;
    const auto local = x % span;
    const auto first = resident_biome_material(biome);
    const auto second = resident_biome_material(biome + 1u);
    constexpr std::uint32_t blend_width = 96u;
    if (local + blend_width < span) return first;
    const auto signed_distance = static_cast<std::int32_t>(local) -
        static_cast<std::int32_t>(span - blend_width / 2u);
    return resident_transition_material(first, second, signed_distance,
                              static_cast<std::int32_t>(blend_width / 2u),
                              x, y, biome * 41u + 9u);
}

[[nodiscard]] constexpr Material resident_layer_material(
    const std::uint32_t zone,
    const std::int32_t local_y,
    const std::int32_t boundary_a,
    const std::int32_t boundary_b,
    const std::int32_t boundary_c,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    const Material first = zone == 0u ? Material::dirt :
        (zone == 1u ? Material::sand : Material::silt);
    const Material second = zone == 0u ? Material::sand :
        (zone == 1u ? Material::silt : Material::dirt);
    const Material third = zone == 0u ? Material::silt :
        (zone == 1u ? Material::dirt : Material::stone);
    const Material fourth = Material::stone;
    constexpr std::int32_t blend = 14;
    if (local_y < boundary_a + blend)
        return resident_transition_material(first, second, local_y - boundary_a,
                                  blend, x, y, zone * 31u + 3u);
    if (local_y < boundary_b + blend)
        return resident_transition_material(second, third, local_y - boundary_b,
                                  blend, x, y, zone * 31u + 11u);
    return resident_transition_material(third, fourth, local_y - boundary_c,
                              blend, x, y, zone * 31u + 19u);
}

[[nodiscard]] constexpr terrain::Sample resident_substrate_sample(
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    if (x >= world_width || y >= world_height || world_width == 0u || world_height == 0u)
        return {Material::empty, false, false, false};

    const auto scene_top = authored_scene_origin_y(world_height);
    const auto scene_bottom = (std::min)(world_height, scene_top + pre_expansion_world_height);
    const auto horizontal_shell = (std::min)(world_width, resident_world_shell_cells);
    const auto top_shell = (std::min)(world_height, resident_world_shell_cells);
    const bool side_shell = x < horizontal_shell || x >= world_width - horizontal_shell;
    if (side_shell || y < top_shell) return {Material::stone, true, false, false};
    if (y < scene_top) return {Material::empty, false, false, false};

    const auto scene_height = scene_bottom - scene_top;
    const auto foundation = (std::min)(scene_height, authored_scene_foundation_cells);
    const auto foundation_start = scene_bottom - foundation;
    if (y < scene_bottom) {
        if (y >= foundation_start) return {Material::stone, true, false, false};
        // The shared surface crosses the authored footprint instead of stopping
        // at its left/right edges. Authored non-empty cells are composited over
        // this substrate by reset/load code, preserving structures and basins
        // while every scene receives continuous ground like the map overview.
        // Continue the authored Sandbox plateau cleanly across every scene.
        // Four dirt rows sit between the grass top and the shared foundation.
        const auto surface_y = static_cast<std::int32_t>(scene_bottom) -
                               static_cast<std::int32_t>(foundation * 5u);
        if (static_cast<std::int32_t>(y) < surface_y)
          return {Material::empty, false, false, false};
        if (static_cast<std::int32_t>(y) == surface_y)
          return {Material::grass, true, false, false};
        if (static_cast<std::int32_t>(y) < surface_y +
                static_cast<std::int32_t>(foundation * 4u))
          return {Material::dirt, true, false, false};
        const auto biome = resident_surface_biome(world_width, x, y);
        return {biome, true, false, false};
    }

    const auto bottom_shell = (std::min)(world_height, resident_world_shell_cells);
    const auto bottom_shell_start = world_height - bottom_shell;
    if (y >= bottom_shell_start) return {Material::stone, true, false, false};

    const auto lava_room = bottom_shell_start > scene_bottom
        ? bottom_shell_start - scene_bottom : 0u;
    const auto lava_thickness = (std::min)(lava_room, resident_world_lava_cells);
    const auto lava_start = bottom_shell_start - lava_thickness;
    if (y >= lava_start) return {Material::lava, false, true, false};

    const auto cap_room = lava_start > scene_bottom ? lava_start - scene_bottom : 0u;
    const auto cap_thickness = (std::min)(cap_room, resident_world_shell_cells);
    const auto lava_cap_start = lava_start - cap_thickness;
    if (y >= lava_cap_start) return {Material::stone, true, false, false};

    const auto depth = y - scene_bottom;
    const auto zone_height = pre_expansion_world_height;
    const auto zone = (std::min)(depth / zone_height, subterranean_zone_count - 1u);
    const auto local_y = static_cast<std::int32_t>(depth % zone_height);
    const auto boundary_a = 96 + resident_ground_boundary_offset(x, zone * 17u + 3u, 22);
    const auto boundary_b = 218 + resident_ground_boundary_offset(x, zone * 19u + 7u, 28);
    const auto boundary_c = 302 + resident_ground_boundary_offset(x, zone * 23u + 11u, 20);
    auto geology = resident_layer_material(zone, local_y, boundary_a, boundary_b,
                                 boundary_c, x, y);

    const auto mud_seed = resident_ground_hash(x / 32u, y / 24u + zone * 97u);
    const auto mud_local_x = static_cast<std::int32_t>(x % 32u) - 16;
    const auto mud_local_y = static_cast<std::int32_t>(y % 24u) - 12;
    if ((mud_seed & 63u) == 0u &&
        mud_local_x * mud_local_x * 9 + mud_local_y * mud_local_y * 16 < 1600)
        geology = Material::mud;

    return resident_ground_sample(geology, x, y, depth);
}

[[nodiscard]] constexpr Material resident_substrate_material(
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    return resident_substrate_sample(world_width, world_height, x, y).material;
}


} // namespace sandhybrid
