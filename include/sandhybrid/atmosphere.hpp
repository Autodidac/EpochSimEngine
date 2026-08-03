#pragma once

#include "sandhybrid/material.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace sandhybrid {

inline constexpr std::uint32_t atmosphere_capacity = 65'535u;

enum class AtmosphereComponent : std::uint8_t {
    nitrogen = 0,
    oxygen,
    argon,
    carbon_dioxide,
    neon,
    hydrogen,
    helium,
    water_vapor,
    contaminants,
    count
};

inline constexpr auto atmosphere_component_count =
    static_cast<std::size_t>(AtmosphereComponent::count);

[[nodiscard]] constexpr std::size_t atmosphere_index(
    const AtmosphereComponent component) noexcept {
    return static_cast<std::size_t>(component);
}

struct PackedAtmosphere final {
    std::array<std::uint32_t, atmosphere_component_count> components{};
    std::int16_t temperature_c{20};

    [[nodiscard]] constexpr std::uint32_t amount(
        const AtmosphereComponent component) const noexcept {
        return components[atmosphere_index(component)];
    }

    [[nodiscard]] constexpr std::uint32_t pressure_units() const noexcept {
        std::uint32_t result = 0u;
        for (const auto component : components) {
            result += component;
        }
        return result;
    }

    [[nodiscard]] constexpr std::uint32_t available_capacity() const noexcept {
        const auto pressure = pressure_units();
        return pressure < atmosphere_capacity ? atmosphere_capacity - pressure : 0u;
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        return pressure_units() <= atmosphere_capacity;
    }
};

[[nodiscard]] constexpr PackedAtmosphere make_earth_atmosphere(
    const std::int16_t temperature_c = 20) noexcept {
    PackedAtmosphere result{};
    result.temperature_c = temperature_c;

    const auto oxygen = atmosphere_capacity * 54u / 255u;
    const auto argon = atmosphere_capacity * 9u / 1'000u;
    const auto carbon_dioxide = atmosphere_capacity * 4u / 10'000u;
    const auto neon = 1u;
    const auto nitrogen =
        atmosphere_capacity - oxygen - argon - carbon_dioxide - neon;

    result.components[atmosphere_index(AtmosphereComponent::nitrogen)] = nitrogen;
    result.components[atmosphere_index(AtmosphereComponent::oxygen)] = oxygen;
    result.components[atmosphere_index(AtmosphereComponent::argon)] = argon;
    result.components[atmosphere_index(AtmosphereComponent::carbon_dioxide)] =
        carbon_dioxide;
    result.components[atmosphere_index(AtmosphereComponent::neon)] = neon;
    return result;
}

[[nodiscard]] constexpr std::uint32_t oxygen_per_mille(
    const PackedAtmosphere& atmosphere) noexcept {
    const auto pressure = atmosphere.pressure_units();
    if (pressure == 0u) return 0u;

    const auto oxygen = atmosphere.amount(AtmosphereComponent::oxygen);
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(oxygen) * 1'000u) / pressure);
}

[[nodiscard]] constexpr bool is_breathable(
    const PackedAtmosphere& atmosphere,
    const std::uint32_t minimum_oxygen_per_mille = 160u) noexcept {
    return oxygen_per_mille(atmosphere) >= minimum_oxygen_per_mille;
}

[[nodiscard]] constexpr std::uint32_t enrich_atmosphere(
    PackedAtmosphere& atmosphere,
    const AtmosphereComponent component,
    const std::uint32_t requested_amount) noexcept {
    if (requested_amount == 0u) return 0u;

    const auto target = atmosphere_index(component);
    auto remaining = requested_amount;

    const auto added = (std::min)(remaining, atmosphere.available_capacity());
    atmosphere.components[target] += added;
    remaining -= added;

    for (std::size_t index = 0u;
         index < atmosphere_component_count && remaining > 0u;
         ++index) {
        if (index == target) continue;

        const auto replaced =
            (std::min)(remaining, atmosphere.components[index]);
        atmosphere.components[index] -= replaced;
        atmosphere.components[target] += replaced;
        remaining -= replaced;
    }

    return requested_amount - remaining;
}

struct AtmosphereTransferResult final {
    std::uint32_t moved{};
    bool committed{};
};

[[nodiscard]] constexpr AtmosphereTransferResult transfer_atmosphere(
    PackedAtmosphere& source,
    PackedAtmosphere& destination,
    const std::uint32_t requested_amount) noexcept {
    const auto source_total = source.pressure_units();
    const auto accepted = (std::min)(
        requested_amount,
        (std::min)(source_total, destination.available_capacity()));

    if (accepted == 0u) return {};

    std::array<std::uint32_t, atmosphere_component_count> moved{};
    std::array<std::uint64_t, atmosphere_component_count> remainders{};

    std::uint32_t base_total = 0u;
    for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
        const auto product =
            static_cast<std::uint64_t>(source.components[index]) * accepted;
        moved[index] = static_cast<std::uint32_t>(product / source_total);
        remainders[index] = product % source_total;
        base_total += moved[index];
    }

    auto remainder_units = accepted - base_total;
    while (remainder_units > 0u) {
        std::size_t selected = atmosphere_component_count;
        std::uint64_t selected_remainder = 0u;

        for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
            if (source.components[index] <= moved[index]) continue;
            if (selected == atmosphere_component_count ||
                remainders[index] > selected_remainder) {
                selected = index;
                selected_remainder = remainders[index];
            }
        }

        if (selected == atmosphere_component_count) break;
        ++moved[selected];
        remainders[selected] = 0u;
        --remainder_units;
    }

    for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
        source.components[index] -= moved[index];
        destination.components[index] += moved[index];
    }

    return {accepted, true};
}

[[nodiscard]] constexpr std::uint32_t respire(
    PackedAtmosphere& atmosphere,
    const std::uint32_t requested_oxygen) noexcept {
    const auto oxygen_index = atmosphere_index(AtmosphereComponent::oxygen);
    const auto carbon_index =
        atmosphere_index(AtmosphereComponent::carbon_dioxide);
    const auto consumed =
        (std::min)(requested_oxygen, atmosphere.components[oxygen_index]);

    atmosphere.components[oxygen_index] -= consumed;
    atmosphere.components[carbon_index] += consumed;
    return consumed;
}

[[nodiscard]] constexpr std::optional<AtmosphereComponent>
atmosphere_component_for_material(const Material material) noexcept {
    switch (material) {
        case Material::oxygen:
            return AtmosphereComponent::oxygen;
        case Material::carbon_dioxide:
            return AtmosphereComponent::carbon_dioxide;
        case Material::hydrogen:
            return AtmosphereComponent::hydrogen;
        case Material::steam:
        case Material::dirty_steam:
            return AtmosphereComponent::water_vapor;
        case Material::smoke:
            return AtmosphereComponent::contaminants;
        default:
            return std::nullopt;
    }
}

} // namespace sandhybrid
