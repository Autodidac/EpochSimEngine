#include <sandhybrid/packet_transaction.hpp>

int main() {
    sandhybrid::MaterialInventory packet_source{};
    packet_source.capacity = sandhybrid::full_bulk_element_volume;
    if (!packet_source.add(
            sandhybrid::Material::water,
            sandhybrid::full_bulk_element_volume)) return 1;

    sandhybrid::MaterialInventory packet_destination{};
    packet_destination.capacity = sandhybrid::full_bulk_element_volume;

    auto fine_source = packet_source;
    auto fine_destination = packet_destination;
    for (std::uint32_t index = 0u;
         index < sandhybrid::full_bulk_element_volume;
         ++index) {
        if (!fine_source.remove(sandhybrid::Material::water, 1u)) return 2;
        if (!fine_destination.add(sandhybrid::Material::water, 1u)) return 3;
    }

    const auto packet_result = sandhybrid::transact_material_packet(
        packet_source,
        packet_destination,
        sandhybrid::Material::water,
        sandhybrid::full_bulk_element_volume,
        true,
        true);
    if (!packet_result.committed() || packet_result.fine_fallback) return 4;
    if (packet_source.amounts != fine_source.amounts) return 5;
    if (packet_destination.amounts != fine_destination.amounts) return 6;

    const auto source_before = packet_destination;
    sandhybrid::MaterialInventory blocked{};
    blocked.capacity = 1u;
    if (!blocked.add(sandhybrid::Material::stone, 1u)) return 7;
    const auto blocked_before = blocked;
    const auto rejected = sandhybrid::transact_material_packet(
        packet_destination,
        blocked,
        sandhybrid::Material::water,
        10u,
        true,
        true);
    if (rejected.committed() || !rejected.fine_fallback) return 8;
    if (packet_destination.amounts != source_before.amounts) return 9;
    if (blocked.amounts != blocked_before.amounts) return 10;

    return 0;
}
