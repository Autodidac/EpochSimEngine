#pragma once

#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t pre_expansion_world_width = 640u;
inline constexpr std::uint32_t pre_expansion_world_height = 360u;

// The logical world remains 8x8 authored-map footprints, while the bounded
// resident GPU window remains 4x4 until deterministic far-section streaming
// exists. Camera scale is defined from authored-map footprints, not incidental
// resident allocation size, so memory policy cannot silently change framing.
inline constexpr std::uint32_t logical_world_dimension_scale = 8u;
inline constexpr std::uint32_t resident_world_dimension_scale = 4u;

inline constexpr std::uint32_t camera_zoom_min = resident_world_dimension_scale / 2u;
inline constexpr std::uint32_t camera_zoom_default = resident_world_dimension_scale;
inline constexpr std::uint32_t camera_zoom_max = resident_world_dimension_scale * 8u;

inline constexpr std::uint32_t resident_world_width =
    pre_expansion_world_width * resident_world_dimension_scale;
inline constexpr std::uint32_t resident_world_height =
    pre_expansion_world_height * resident_world_dimension_scale;

[[nodiscard]] constexpr std::uint32_t camera_view_width(
    const std::uint32_t zoom) noexcept {
    return resident_world_width / zoom;
}

[[nodiscard]] constexpr std::uint32_t camera_view_height(
    const std::uint32_t zoom) noexcept {
    return resident_world_height / zoom;
}

static_assert(camera_view_width(camera_zoom_min) == 1280u);
static_assert(camera_view_height(camera_zoom_min) == 720u);
static_assert(camera_view_width(camera_zoom_default) == 640u);
static_assert(camera_view_height(camera_zoom_default) == 360u);

} // namespace sandhybrid
