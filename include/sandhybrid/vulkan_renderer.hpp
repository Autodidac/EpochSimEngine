#pragma once

#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/shared_state.hpp"
#include "sandhybrid/window.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace sandhybrid {

struct SimulationConfig final {
    std::uint32_t grid_width{resident_world_width};
    std::uint32_t grid_height{resident_world_height};
    std::uint32_t frames_in_flight{2};
    std::uint32_t max_frames_per_second{120};
    WorldSizePreset world_size{WorldSizePreset::large};
};

class VulkanRenderer final {
public:
    VulkanRenderer(const NativeWindow& window, SimulationConfig config,
                   std::string save_slot);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void run(const std::atomic_bool& stop_requested, SharedState& shared_state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sandhybrid
