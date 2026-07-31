#pragma once

#include "epoch/sand/shared_state.hpp"
#include "epoch/sand/window.hpp"

#include <cstdint>
#include <memory>
#include <atomic>

namespace epoch::sand {

inline constexpr std::uint32_t pre_expansion_world_width = 640u;
inline constexpr std::uint32_t pre_expansion_world_height = 360u;
inline constexpr std::uint32_t world_dimension_scale = 8u;
inline constexpr std::uint32_t camera_zoom_min = 4u;
inline constexpr std::uint32_t camera_zoom_default = 8u;
inline constexpr std::uint32_t camera_zoom_max = 64u;

struct SimulationConfig final {
    std::uint32_t grid_width{pre_expansion_world_width * world_dimension_scale};
    std::uint32_t grid_height{pre_expansion_world_height * world_dimension_scale};
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
