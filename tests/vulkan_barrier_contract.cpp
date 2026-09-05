#include "vulkan_barriers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <type_traits>

namespace {

// Non-dispatchable handles may be pointer- or integer-shaped in Vulkan headers.
// These inert values are compared only; this test never calls a Vulkan API.
template<typename Handle>
Handle inert_handle(const std::uintptr_t value) noexcept {
    if constexpr (std::is_pointer_v<Handle>) return reinterpret_cast<Handle>(value);
    else return static_cast<Handle>(value);
}

std::size_t checks = 0;

void require(const bool condition, const char* const message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

template<std::size_t N>
void check_batch(const std::array<VkBuffer, N>& buffers) {
    const auto original = buffers;
    const auto barriers = sandhybrid::detail::compute_storage_barriers(buffers);
    static_assert(noexcept(sandhybrid::detail::compute_storage_barriers(buffers)));
    static_assert(std::is_same_v<decltype(barriers),
                                 const std::array<VkBufferMemoryBarrier, N>>);
    require(buffers == original, "helper modified input handles");
    require(barriers.size() == N, "helper changed batch extent");
    for (std::size_t index = 0; index < N; ++index) {
        const auto& barrier = barriers[index];
        require(barrier.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                "incorrect structure type");
        require(barrier.pNext == nullptr, "unexpected extension chain");
        require(barrier.srcAccessMask == VK_ACCESS_SHADER_WRITE_BIT,
                "incorrect source access mask");
        require(barrier.dstAccessMask ==
                    (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
                "incorrect destination access mask");
        require(barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED,
                "unexpected source queue-family ownership");
        require(barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED,
                "unexpected destination queue-family ownership");
        require(barrier.buffer == buffers[index], "buffer identity/order changed");
        require(barrier.offset == 0, "nonzero buffer offset");
        require(barrier.size == VK_WHOLE_SIZE, "buffer coverage narrowed");
    }
}

} // namespace

int main() {
    try {
        const auto first = inert_handle<VkBuffer>(0x1110u);
        const auto second = inert_handle<VkBuffer>(0x2220u);
        const auto third = inert_handle<VkBuffer>(0x3330u);
        check_batch(std::array<VkBuffer, 2>{first, second});
        check_batch(std::array<VkBuffer, 3>{third, first, second});
        // The data helper preserves entries verbatim; it neither filters nor
        // deduplicates handles. Vulkan validity remains the caller's contract.
        check_batch(std::array<VkBuffer, 3>{second, VK_NULL_HANDLE, second});
        std::printf("Vulkan barrier data contract: %zu checks passed (no GPU calls).\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Vulkan barrier data contract failed: %s\n", error.what());
        return 1;
    }
}
