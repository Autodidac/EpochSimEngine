#pragma once

#include "epoch/sand/shared_state.hpp"
#include "epoch/sand/window.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace epoch::sand {

inline constexpr std::uint32_t pre_expansion_world_width = 640u;
inline constexpr std::uint32_t pre_expansion_world_height = 360u;

// MC-077: the logical world remains 8x8 map footprints, but the entire 64x
// cell area must not be simultaneously resident until MC-063 implements
// deterministic far-section streaming. A 4x4 resident window cuts persistent
// cell, snapshot, staging, sunlight, hierarchy, and full-pass GPU work to 25%
// of the crashing fully resident configuration. Placement-mode changes never
// alter these dimensions or allocate/resize buffers.
inline constexpr std::uint32_t logical_world_dimension_scale = 8u;
inline constexpr std::uint32_t resident_world_dimension_scale = 4u;

// The v2.4.4 resident-window reduction accidentally reused the old numeric
// zoom limits against half-sized resident dimensions. That cut every camera
// view in half. Scale the zoom values with the resident window so the visual
// contract remains unchanged: 2x2 map footprints at maximum zoom-out, one
// complete 640x360 map at reset, and the original close-up limit.
inline constexpr std::uint32_t camera_zoom_min = resident_world_dimension_scale / 2u;
inline constexpr std::uint32_t camera_zoom_default = resident_world_dimension_scale;
inline constexpr std::uint32_t camera_zoom_max = resident_world_dimension_scale * 8u;

static_assert((pre_expansion_world_width * resident_world_dimension_scale) /
                  camera_zoom_min ==
              pre_expansion_world_width * 2u);
static_assert((pre_expansion_world_height * resident_world_dimension_scale) /
                  camera_zoom_min ==
              pre_expansion_world_height * 2u);
static_assert((pre_expansion_world_width * resident_world_dimension_scale) /
                  camera_zoom_default ==
              pre_expansion_world_width);
static_assert((pre_expansion_world_height * resident_world_dimension_scale) /
                  camera_zoom_default ==
              pre_expansion_world_height);

struct SimulationConfig final {
    std::uint32_t grid_width{pre_expansion_world_width * resident_world_dimension_scale};
    std::uint32_t grid_height{pre_expansion_world_height * resident_world_dimension_scale};
    std::uint32_t frames_in_flight{2};
    std::uint32_t max_frames_per_second{120};
};

class VulkanRenderer final {
public:
    VulkanRenderer(const NativeWindow& window, SimulationConfig config);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void run(const std::atomic_bool& stop_requested, SharedState& shared_state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace epoch::sand
