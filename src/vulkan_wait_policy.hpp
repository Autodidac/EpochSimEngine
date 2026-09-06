#pragma once

#include <cstdint>

namespace sandhybrid {

// Presentation, acquire, and capture waits share one device-class budget.
// Report modes may request captures, but never shorten a software device's wait.
[[nodiscard]] constexpr std::uint64_t presentation_wait_timeout_ns(
    const bool software_vulkan) noexcept {
    return software_vulkan ? 60'000'000'000ull : 5'000'000'000ull;
}

} // namespace sandhybrid
