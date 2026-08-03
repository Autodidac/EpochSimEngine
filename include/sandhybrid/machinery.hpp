#pragma once

#include "sandhybrid/inventory.hpp"

#include <cstdint>

namespace sandhybrid {

enum class PortDirection : std::uint8_t {
    left = 0,
    right,
    up,
    down
};

[[nodiscard]] constexpr PortDirection opposite_direction(
    const PortDirection direction) noexcept {
    switch (direction) {
        case PortDirection::left:
            return PortDirection::right;
        case PortDirection::right:
            return PortDirection::left;
        case PortDirection::up:
            return PortDirection::down;
        case PortDirection::down:
            return PortDirection::up;
    }
    return PortDirection::right;
}

struct MachineRecipe final {
    Material input{Material::empty};
    std::uint32_t input_amount{};
    Material output{Material::empty};
    std::uint32_t output_amount{};
    std::uint32_t latency_ticks{1u};
    bool requires_power{true};
    bool requires_medium{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return input != Material::empty &&
               output != Material::empty &&
               input_amount > 0u &&
               output_amount > 0u &&
               latency_ticks > 0u;
    }
};

enum class MachineTransactionStatus : std::uint8_t {
    committed = 0,
    invalid_recipe,
    no_power,
    missing_medium,
    missing_input,
    blocked_output
};

struct MachineTransactionResult final {
    MachineTransactionStatus status{MachineTransactionStatus::invalid_recipe};
    PortDirection output_direction{PortDirection::right};
    std::uint32_t consumed{};
    std::uint32_t produced{};

    [[nodiscard]] constexpr bool committed() const noexcept {
        return status == MachineTransactionStatus::committed;
    }
};

[[nodiscard]] constexpr MachineTransactionResult transact_machine(
    MaterialInventory& input_inventory,
    MaterialInventory& output_inventory,
    const MachineRecipe& recipe,
    const bool powered,
    const bool medium_available,
    const PortDirection output_direction) noexcept {
    if (!recipe.valid()) {
        return {MachineTransactionStatus::invalid_recipe, output_direction, 0u, 0u};
    }
    if (recipe.requires_power && !powered) {
        return {MachineTransactionStatus::no_power, output_direction, 0u, 0u};
    }
    if (recipe.requires_medium && !medium_available) {
        return {MachineTransactionStatus::missing_medium, output_direction, 0u, 0u};
    }
    if (!input_inventory.can_remove(recipe.input, recipe.input_amount)) {
        return {MachineTransactionStatus::missing_input, output_direction, 0u, 0u};
    }
    if (!output_inventory.can_add(recipe.output_amount)) {
        return {MachineTransactionStatus::blocked_output, output_direction, 0u, 0u};
    }

    const auto removed = input_inventory.remove(recipe.input, recipe.input_amount);
    const auto added = output_inventory.add(recipe.output, recipe.output_amount);
    if (!removed || !added) {
        return {MachineTransactionStatus::blocked_output, output_direction, 0u, 0u};
    }

    return {
        MachineTransactionStatus::committed,
        output_direction,
        recipe.input_amount,
        recipe.output_amount
    };
}

[[nodiscard]] constexpr std::uint64_t mix_transaction_seed(
    std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31u);
}

[[nodiscard]] constexpr bool sluice_gold_roll(
    const std::uint64_t seed,
    const std::uint64_t feed_sequence) noexcept {
    return mix_transaction_seed(seed ^ mix_transaction_seed(feed_sequence)) % 10u == 0u;
}

enum class SluiceTransactionStatus : std::uint8_t {
    committed = 0,
    invalid_feed,
    dry_feed,
    missing_feed,
    missing_water,
    blocked_solid_output,
    blocked_water_output,
    overlapping_outputs
};

struct SluiceTransactionResult final {
    SluiceTransactionStatus status{SluiceTransactionStatus::invalid_feed};
    Material solid_output{Material::empty};
    PortDirection solid_direction{PortDirection::left};
    PortDirection water_direction{PortDirection::right};
    bool gold_roll{};

    [[nodiscard]] constexpr bool committed() const noexcept {
        return status == SluiceTransactionStatus::committed;
    }
};

[[nodiscard]] constexpr SluiceTransactionResult transact_sluice(
    MaterialInventory& feed_inventory,
    MaterialInventory& process_water_inventory,
    MaterialInventory& solid_output_inventory,
    MaterialInventory& water_output_inventory,
    const Material feed_material,
    const bool wet_feed,
    const std::uint64_t seed,
    const std::uint64_t feed_sequence,
    const PortDirection solid_direction,
    const PortDirection water_direction) noexcept {
    if (feed_material != Material::sand && feed_material != Material::silt) {
        return {SluiceTransactionStatus::invalid_feed, Material::empty,
                solid_direction, water_direction, false};
    }
    if (!wet_feed) {
        return {SluiceTransactionStatus::dry_feed, Material::empty,
                solid_direction, water_direction, false};
    }
    if (solid_direction == water_direction) {
        return {SluiceTransactionStatus::overlapping_outputs, Material::empty,
                solid_direction, water_direction, false};
    }
    if (!feed_inventory.can_remove(feed_material, 1u)) {
        return {SluiceTransactionStatus::missing_feed, Material::empty,
                solid_direction, water_direction, false};
    }
    if (!process_water_inventory.can_remove(Material::water, 1u)) {
        return {SluiceTransactionStatus::missing_water, Material::empty,
                solid_direction, water_direction, false};
    }

    const auto won_gold = sluice_gold_roll(seed, feed_sequence);
    const auto product = won_gold ? Material::gold : feed_material;
    if (!solid_output_inventory.can_add(1u)) {
        return {SluiceTransactionStatus::blocked_solid_output, product,
                solid_direction, water_direction, won_gold};
    }
    if (!water_output_inventory.can_add(1u)) {
        return {SluiceTransactionStatus::blocked_water_output, product,
                solid_direction, water_direction, won_gold};
    }

    const auto feed_removed = feed_inventory.remove(feed_material, 1u);
    const auto water_removed =
        process_water_inventory.remove(Material::water, 1u);
    const auto solid_added = solid_output_inventory.add(product, 1u);
    const auto water_added =
        water_output_inventory.add(Material::water, 1u);

    if (!feed_removed || !water_removed || !solid_added || !water_added) {
        return {SluiceTransactionStatus::blocked_solid_output, product,
                solid_direction, water_direction, won_gold};
    }

    return {SluiceTransactionStatus::committed, product,
            solid_direction, water_direction, won_gold};
}

struct InsectHabitatPolicy final {
    Material species{Material::ant};
    std::uint32_t capacity{100u};
    std::uint32_t food_per_birth{1u};
    std::uint32_t water_per_birth{1u};
    std::uint32_t waste_per_birth{1u};
    std::uint32_t spawn_interval_ticks{600u};

    [[nodiscard]] constexpr bool valid() const noexcept {
        const auto supported_species =
            species == Material::ant || species == Material::beetle;
        return supported_species &&
               capacity > 0u &&
               food_per_birth > 0u &&
               water_per_birth > 0u &&
               spawn_interval_ticks > 0u;
    }
};

} // namespace sandhybrid
