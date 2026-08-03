#include <sandhybrid/machinery.hpp>

int main() {
    sandhybrid::MaterialInventory input{};
    input.capacity = 8u;
    if (!input.add(sandhybrid::Material::iron_ore, 2u)) return 1;

    sandhybrid::MaterialInventory output{};
    output.capacity = 1u;
    if (!output.add(sandhybrid::Material::stone, 1u)) return 2;

    const sandhybrid::MachineRecipe smelt{
        sandhybrid::Material::iron_ore,
        1u,
        sandhybrid::Material::iron,
        1u,
        30u,
        true,
        false
    };

    const auto input_before = input;
    const auto output_before = output;
    const auto blocked = sandhybrid::transact_machine(
        input, output, smelt, true, true,
        sandhybrid::PortDirection::right);
    if (blocked.committed() ||
        blocked.status != sandhybrid::MachineTransactionStatus::blocked_output) {
        return 3;
    }
    if (input.amounts != input_before.amounts ||
        output.amounts != output_before.amounts) return 4;

    output = {};
    output.capacity = 8u;
    const auto committed = sandhybrid::transact_machine(
        input, output, smelt, true, true,
        sandhybrid::PortDirection::left);
    if (!committed.committed() ||
        committed.output_direction != sandhybrid::PortDirection::left) return 5;
    if (input.count(sandhybrid::Material::iron_ore) != 1u ||
        output.count(sandhybrid::Material::iron) != 1u) return 6;

    sandhybrid::MaterialInventory feed{};
    feed.capacity = 4u;
    if (!feed.add(sandhybrid::Material::sand, 1u)) return 7;
    sandhybrid::MaterialInventory water{};
    water.capacity = 4u;
    if (!water.add(sandhybrid::Material::water, 1u)) return 8;
    sandhybrid::MaterialInventory solid_output{};
    solid_output.capacity = 4u;
    sandhybrid::MaterialInventory water_output{};
    water_output.capacity = 4u;

    const auto dry = sandhybrid::transact_sluice(
        feed, water, solid_output, water_output,
        sandhybrid::Material::sand, false, 7u, 1u,
        sandhybrid::PortDirection::left,
        sandhybrid::PortDirection::right);
    if (dry.committed() ||
        dry.status != sandhybrid::SluiceTransactionStatus::dry_feed) return 9;
    if (feed.count(sandhybrid::Material::sand) != 1u ||
        water.count(sandhybrid::Material::water) != 1u) return 10;

    const auto sluiced = sandhybrid::transact_sluice(
        feed, water, solid_output, water_output,
        sandhybrid::Material::sand, true, 7u, 1u,
        sandhybrid::PortDirection::left,
        sandhybrid::PortDirection::right);
    if (!sluiced.committed()) return 11;
    if (solid_output.total() != 1u ||
        water_output.count(sandhybrid::Material::water) != 1u) return 12;
    if (sluiced.solid_direction == sluiced.water_direction) return 13;

    constexpr sandhybrid::InsectHabitatPolicy ant_habitat{};
    static_assert(ant_habitat.valid());
    constexpr sandhybrid::InsectHabitatPolicy invalid_habitat{
        sandhybrid::Material::bee, 100u, 1u, 1u, 1u, 600u
    };
    static_assert(!invalid_habitat.valid());

    if (sandhybrid::opposite_direction(
            sandhybrid::PortDirection::left) !=
        sandhybrid::PortDirection::right) return 14;

    return 0;
}
