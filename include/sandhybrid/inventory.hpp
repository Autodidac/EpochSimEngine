#pragma once

#include "sandhybrid/material.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace sandhybrid {

[[nodiscard]] constexpr std::size_t material_index(
    const Material material) noexcept {
    return static_cast<std::size_t>(material);
}

struct MaterialInventory final {
    std::array<std::uint32_t, static_cast<std::size_t>(Material::count)> amounts{};
    std::uint32_t capacity{};

    [[nodiscard]] constexpr std::uint32_t total() const noexcept {
        std::uint32_t result = 0u;
        for (const auto amount : amounts) {
            result += amount;
        }
        return result;
    }

    [[nodiscard]] constexpr std::uint32_t available_capacity() const noexcept {
        const auto used = total();
        return used < capacity ? capacity - used : 0u;
    }

    [[nodiscard]] constexpr std::uint32_t count(
        const Material material) const noexcept {
        return amounts[material_index(material)];
    }

    [[nodiscard]] constexpr bool can_add(
        const std::uint32_t amount) const noexcept {
        return amount <= available_capacity();
    }

    [[nodiscard]] constexpr bool can_remove(
        const Material material,
        const std::uint32_t amount) const noexcept {
        return count(material) >= amount;
    }

    [[nodiscard]] constexpr bool add(
        const Material material,
        const std::uint32_t amount) noexcept {
        if (!can_add(amount)) return false;
        amounts[material_index(material)] += amount;
        return true;
    }

    [[nodiscard]] constexpr bool remove(
        const Material material,
        const std::uint32_t amount) noexcept {
        if (!can_remove(material, amount)) return false;
        amounts[material_index(material)] -= amount;
        return true;
    }
};

} // namespace sandhybrid
