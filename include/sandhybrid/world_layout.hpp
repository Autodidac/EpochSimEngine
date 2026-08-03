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
    return resident_ground_host_material(material) ||
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

[[nodiscard]] constexpr terrain::Sample resident_substrate_sample(
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    if (x >= world_width || y >= world_height || world_width == 0u || world_height == 0u)
        return {Material::empty, false, false, false};

    const auto scene_top = authored_scene_origin_y(world_height);
    const auto scene_bottom = (std::min)(
        world_height, scene_top + pre_expansion_world_height);
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
        return {Material::empty, false, false, false};
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
    const auto boundary_a = 106 + resident_ground_boundary_offset(x, zone * 17u + 3u, 18);
    const auto boundary_b = 232 + resident_ground_boundary_offset(x, zone * 19u + 7u, 24);
    const auto boundary_c = 304 + resident_ground_boundary_offset(x, zone * 23u + 11u, 16);

    Material geology = Material::stone;
    if (zone == 0u) {
        geology = local_y < boundary_a ? Material::dirt
            : (local_y < boundary_b ? Material::sand : Material::silt);
    } else if (zone == 1u) {
        geology = local_y < boundary_a ? Material::sand
            : (local_y < boundary_b ? Material::silt : Material::dirt);
    } else {
        geology = local_y < boundary_a ? Material::silt
            : (local_y < boundary_b ? Material::dirt : Material::stone);
    }

    const auto nearest = (std::min)(
        (std::abs)(local_y - boundary_a),
        (std::min)((std::abs)(local_y - boundary_b), (std::abs)(local_y - boundary_c)));
    if (nearest <= 7 && resident_ground_rough_edge_cell(x, y, zone * 31u + 13u)) {
        if (geology == Material::dirt) geology = Material::sand;
        else if (geology == Material::sand) geology = Material::silt;
        else if (geology == Material::silt) geology = Material::dirt;
    }

    const auto mud_seed = resident_ground_hash(x / 24u, y / 18u + zone * 97u);
    const auto mud_local_x = static_cast<std::int32_t>(x % 24u) - 12;
    const auto mud_local_y = static_cast<std::int32_t>(y % 18u) - 9;
    if ((mud_seed & 31u) == 0u &&
        mud_local_x * mud_local_x * 9 + mud_local_y * mud_local_y * 16 < 900) {
        geology = Material::mud;
    }

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
