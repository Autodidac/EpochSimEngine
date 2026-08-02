#pragma once

#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/material.hpp"

#include <algorithm>
#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t authored_scene_foundation_cells = 8u;
inline constexpr std::uint32_t resident_world_shell_cells = 8u;
inline constexpr std::uint32_t resident_world_lava_cells = 16u;
inline constexpr std::uint32_t resident_world_geology_patch_cells = 64u;
inline constexpr std::uint32_t authored_scene_sky_footprint_rows = 2u;
inline constexpr std::uint32_t subterranean_zone_count =
    resident_world_dimension_scale - 1u;

static_assert(subterranean_zone_count == 3u);
static_assert(authored_scene_foundation_cells == 8u);
static_assert(authored_scene_sky_footprint_rows == 2u);
static_assert(resident_world_lava_cells == 2u * authored_scene_foundation_cells);

[[nodiscard]] constexpr std::uint32_t authored_scene_origin_x(
    const std::uint32_t world_width) noexcept {
    return world_width > pre_expansion_world_width
        ? (world_width - pre_expansion_world_width) / 2u
        : 0u;
}

[[nodiscard]] constexpr std::uint32_t authored_scene_origin_y(
    const std::uint32_t world_height) noexcept {
    const auto sky_height = pre_expansion_world_height * authored_scene_sky_footprint_rows;
    return world_height >= sky_height + pre_expansion_world_height ? sky_height : 0u;
}

[[nodiscard]] constexpr bool resident_substrate_is_structural(
    const Material material) noexcept {
    return material == Material::stone || material == Material::dirt ||
           material == Material::sand || material == Material::silt;
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

[[nodiscard]] constexpr Material resident_ground_deposit_material(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    if (!resident_ground_host_material(base_material)) return base_material;
    const auto roll = resident_ground_hash(x, y) & 4095u;
    if (roll < 24u) return Material::iron_ore;
    if (roll < 32u) return Material::copper;
    if (roll < 38u) return Material::aluminum;
    if (depth >= 160u && roll < 41u) return Material::uranium;
    return base_material;
}

[[nodiscard]] constexpr Material resident_substrate_material(
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    if (x >= world_width || y >= world_height || world_width == 0u || world_height == 0u)
        return Material::empty;

    const auto scene_top = authored_scene_origin_y(world_height);
    const auto scene_bottom = (std::min)(
        world_height, scene_top + pre_expansion_world_height);
    const auto horizontal_shell = (std::min)(world_width, resident_world_shell_cells);
    const bool side_shell = x < horizontal_shell || x >= world_width - horizontal_shell;

    if (y < scene_top) return Material::empty;

    const auto scene_height = scene_bottom - scene_top;
    const auto foundation = (std::min)(scene_height, authored_scene_foundation_cells);
    const auto foundation_start = scene_bottom - foundation;
    if (y < scene_bottom) {
        if (y >= foundation_start || side_shell) return Material::stone;
        return Material::empty;
    }

    if (side_shell) return Material::stone;

    const auto bottom_shell = (std::min)(world_height, resident_world_shell_cells);
    const auto bottom_shell_start = world_height - bottom_shell;
    if (y >= bottom_shell_start) return Material::stone;

    const auto lava_room = bottom_shell_start > scene_bottom
        ? bottom_shell_start - scene_bottom
        : 0u;
    const auto lava_thickness = (std::min)(lava_room, resident_world_lava_cells);
    const auto lava_start = bottom_shell_start - lava_thickness;
    if (y >= lava_start) return Material::lava;

    const auto cap_room = lava_start > scene_bottom ? lava_start - scene_bottom : 0u;
    const auto cap_thickness = (std::min)(cap_room, resident_world_shell_cells);
    const auto lava_cap_start = lava_start - cap_thickness;
    if (y >= lava_cap_start) return Material::stone;

    const auto depth = y - scene_bottom;
    const auto zone_height = pre_expansion_world_height;
    const auto zone = (std::min)(depth / zone_height, subterranean_zone_count - 1u);
    const auto local_y = depth % zone_height;
    const auto patch_x = x / resident_world_geology_patch_cells;
    const auto patch_y = y / resident_world_geology_patch_cells;

    Material geology = Material::stone;
    if (zone == 0u) {
    if (local_y < 96u) geology = Material::sand;
    else if (local_y < 232u) geology = Material::dirt;
    else if (((patch_x + patch_y * 3u) % 11u) == 0u) geology = Material::mud;
    else geology = Material::silt;
} else if (zone == 1u) {
    if (local_y < 104u) geology = Material::dirt;
    else if (local_y < 248u) {
        geology = ((patch_x * 5u + patch_y) % 13u) == 0u
            ? Material::mud : Material::silt;
    } else {
        geology = ((patch_x + patch_y) % 7u) == 0u
            ? Material::stone : Material::sand;
    }
} else if (local_y < 80u) {
    geology = ((patch_x * 3u + patch_y) % 9u) == 0u
        ? Material::mud : Material::silt;
} else if (local_y < 176u) {
    geology = Material::dirt;
} else if (local_y < 224u && ((patch_x + patch_y * 2u) % 5u) == 0u) {
    geology = Material::sand;
}
    return resident_ground_deposit_material(geology, x, y, depth);
}

} // namespace sandhybrid
