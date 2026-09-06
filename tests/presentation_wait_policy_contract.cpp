#include "vulkan_wait_policy.hpp"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using sandhybrid::presentation_wait_timeout_ns;

static_assert(std::is_same_v<decltype(&presentation_wait_timeout_ns),
                             std::uint64_t (*)(bool) noexcept>);
static_assert(presentation_wait_timeout_ns(false) == 5'000'000'000ull);
static_assert(presentation_wait_timeout_ns(true) == 60'000'000'000ull);

unsigned checks = 0u;

void require(const bool condition, const char* const message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        const auto hardware = presentation_wait_timeout_ns(false);
        const auto software = presentation_wait_timeout_ns(true);
        require(hardware == 5'000'000'000ull, "hardware wait must remain five seconds");
        require(software == 60'000'000'000ull, "software wait must allow sixty seconds");
        require(hardware > (std::numeric_limits<std::uint32_t>::max)(),
                "hardware nanoseconds must not narrow to 32 bits");
        require(software > (std::numeric_limits<std::uint32_t>::max)(),
                "software nanoseconds must not narrow to 32 bits");
        require(hardware < (std::numeric_limits<std::uint64_t>::max)(),
                "hardware wait must remain finite");
        require(software < (std::numeric_limits<std::uint64_t>::max)(),
                "software wait must remain finite");
        std::printf("Presentation wait policy: %u native checks passed; no Vulkan calls.\n",
                    checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Presentation wait policy failed: %s\n", error.what());
        return 1;
    }
}
