#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>

namespace sandhybrid::detail {

// Private command-recording data only. Callers retain the original dependency
// point and COMPUTE_SHADER -> COMPUTE_SHADER stage masks when batching entries.
template<std::size_t N>
[[nodiscard]] constexpr std::array<VkBufferMemoryBarrier, N> compute_storage_barriers(
    const std::array<VkBuffer, N>& buffers) noexcept {
    std::array<VkBufferMemoryBarrier, N> barriers{};
    for (std::size_t index = 0; index < N; ++index) {
        barriers[index] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffers[index],
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
    }
    return barriers;
}

} // namespace sandhybrid::detail
