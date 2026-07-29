#pragma once

#include "epoch/sand/shared_state.hpp"
#include "epoch/sand/window.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>

namespace epoch::sand {

struct SimulationConfig final {
    std::uint32_t grid_width{640};
    std::uint32_t grid_height{360};
    std::uint32_t frames_in_flight{2};
    std::uint32_t max_frames_per_second{120};
};

class VulkanRenderer final {
public:
    VulkanRenderer(const NativeWindow& window, SimulationConfig config);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void run(std::stop_token stop_token, SharedState& shared_state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace epoch::sand
