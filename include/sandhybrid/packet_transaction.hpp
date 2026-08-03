#pragma once

#include "sandhybrid/inventory.hpp"

#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t bulk_element_width = 8u;
inline constexpr std::uint32_t bulk_element_height = 8u;
inline constexpr std::uint32_t bulk_element_cells =
    bulk_element_width * bulk_element_height;
inline constexpr std::uint32_t represented_cell_volume = 255u;
inline constexpr std::uint32_t full_bulk_element_volume =
    bulk_element_cells * represented_cell_volume;

enum class PacketTransactionStatus : std::uint8_t {
    committed = 0,
    empty_request,
    source_shortage,
    destination_blocked,
    ineligible,
    incompatible
};

struct PacketTransactionResult final {
    PacketTransactionStatus status{PacketTransactionStatus::empty_request};
    std::uint32_t moved{};
    bool fine_fallback{};

    [[nodiscard]] constexpr bool committed() const noexcept {
        return status == PacketTransactionStatus::committed;
    }
};

[[nodiscard]] constexpr PacketTransactionResult transact_material_packet(
    MaterialInventory& source,
    MaterialInventory& destination,
    const Material material,
    const std::uint32_t represented_amount,
    const bool packet_eligible,
    const bool destination_compatible) noexcept {
    if (represented_amount == 0u) {
        return {PacketTransactionStatus::empty_request, 0u, false};
    }
    if (!packet_eligible) {
        return {PacketTransactionStatus::ineligible, 0u, true};
    }
    if (!destination_compatible) {
        return {PacketTransactionStatus::incompatible, 0u, true};
    }
    if (!source.can_remove(material, represented_amount)) {
        return {PacketTransactionStatus::source_shortage, 0u, true};
    }
    if (!destination.can_add(represented_amount)) {
        return {PacketTransactionStatus::destination_blocked, 0u, true};
    }

    const auto removed = source.remove(material, represented_amount);
    const auto added = destination.add(material, represented_amount);
    if (!removed || !added) {
        return {PacketTransactionStatus::destination_blocked, 0u, true};
    }

    return {PacketTransactionStatus::committed, represented_amount, false};
}

} // namespace sandhybrid
