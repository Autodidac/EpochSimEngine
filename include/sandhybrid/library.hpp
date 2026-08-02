#pragma once

#include "sandhybrid/scene_image.hpp"
#include "sandhybrid/section_grid.hpp"
#include "sandhybrid/section_scheduler.hpp"

#include <cstdint>
#include <string_view>

namespace sandhybrid {

inline constexpr std::uint32_t library_api_version = 2u;
inline constexpr std::string_view library_name = "SandHybrid";

struct LibraryCapabilities final {
    bool native_startup_owned_by_consumer{true};
    bool windowing_required{false};
    bool vulkan_required{false};
};

inline constexpr LibraryCapabilities core_library_capabilities{};

} // namespace sandhybrid
