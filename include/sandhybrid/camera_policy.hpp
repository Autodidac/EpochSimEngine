#pragma once

#include <algorithm>
#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t pre_expansion_world_width = 640u;
inline constexpr std::uint32_t pre_expansion_world_height = 360u;

// The 64 authored-map footprints are arranged as a 16x4 resident world. This
// preserves the former total map count while extending travel to the right and
// retaining the original scene/camera coordinates.
inline constexpr std::uint32_t resident_world_footprint_columns = 16u;
inline constexpr std::uint32_t resident_world_footprint_rows = 4u;
inline constexpr std::uint32_t resident_world_footprint_count =
    resident_world_footprint_columns * resident_world_footprint_rows;

inline constexpr std::uint32_t camera_zoom_min = 2u;
inline constexpr std::uint32_t camera_zoom_default = 4u;
inline constexpr std::uint32_t camera_zoom_max = 32u;
inline constexpr std::uint32_t map_zoom_min = 1u;
inline constexpr std::uint32_t map_zoom_default = 1u;
inline constexpr std::uint32_t map_zoom_max = 16u;

inline constexpr std::uint32_t resident_world_width =
    pre_expansion_world_width * resident_world_footprint_columns;
inline constexpr std::uint32_t resident_world_height =
    pre_expansion_world_height * resident_world_footprint_rows;

[[nodiscard]] constexpr std::uint32_t camera_view_width(
    const std::uint32_t zoom) noexcept {
    const auto clamped = std::clamp(zoom, camera_zoom_min, camera_zoom_max);
    return (pre_expansion_world_width * camera_zoom_default) / clamped;
}

[[nodiscard]] constexpr std::uint32_t camera_view_height(
    const std::uint32_t zoom) noexcept {
    const auto clamped = std::clamp(zoom, camera_zoom_min, camera_zoom_max);
    return (pre_expansion_world_height * camera_zoom_default) / clamped;
}

[[nodiscard]] constexpr std::uint32_t map_view_width(
    const std::uint32_t world_width, const std::uint32_t zoom) noexcept {
    return (std::max)(8u, world_width / std::clamp(zoom, map_zoom_min, map_zoom_max));
}

[[nodiscard]] constexpr std::uint32_t map_view_height(
    const std::uint32_t world_height, const std::uint32_t zoom) noexcept {
    return (std::max)(8u, world_height / std::clamp(zoom, map_zoom_min, map_zoom_max));
}

static_assert(resident_world_footprint_count == 64u);
static_assert(camera_view_width(camera_zoom_min) == 1280u);
static_assert(camera_view_height(camera_zoom_min) == 720u);
static_assert(camera_view_width(camera_zoom_default) == 640u);
static_assert(camera_view_height(camera_zoom_default) == 360u);
static_assert(map_view_width(resident_world_width, map_zoom_default) == resident_world_width);
static_assert(map_view_height(resident_world_height, map_zoom_default) == resident_world_height);

} // namespace sandhybrid
