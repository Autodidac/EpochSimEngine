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
        for (const auto component : components) result += component;
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
    const auto nitrogen = atmosphere_capacity - oxygen - argon - carbon_dioxide - neon;
    result.components[atmosphere_index(AtmosphereComponent::nitrogen)] = nitrogen;
    result.components[atmosphere_index(AtmosphereComponent::oxygen)] = oxygen;
    result.components[atmosphere_index(AtmosphereComponent::argon)] = argon;
    result.components[atmosphere_index(AtmosphereComponent::carbon_dioxide)] = carbon_dioxide;
    result.components[atmosphere_index(AtmosphereComponent::neon)] = neon;
    return result;
}

[[nodiscard]] constexpr std::uint32_t component_per_mille(
    const PackedAtmosphere& atmosphere,
    const AtmosphereComponent component) noexcept {
    const auto pressure = atmosphere.pressure_units();
    if (pressure == 0u) return 0u;
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(atmosphere.amount(component)) * 1'000u / pressure);
}

[[nodiscard]] constexpr std::uint32_t oxygen_per_mille(
    const PackedAtmosphere& atmosphere) noexcept {
    return component_per_mille(atmosphere, AtmosphereComponent::oxygen);
}

[[nodiscard]] constexpr bool is_breathable(
    const PackedAtmosphere& atmosphere,
    const std::uint32_t minimum_oxygen_per_mille = 160u) noexcept {
    return oxygen_per_mille(atmosphere) >= minimum_oxygen_per_mille;
}

struct AtmosphereInspection final {
    std::uint32_t pressure_units{};
    std::array<std::uint16_t, atmosphere_component_count> component_per_mille{};
    bool breathable{};
};

[[nodiscard]] constexpr AtmosphereInspection inspect_atmosphere(
    const PackedAtmosphere& atmosphere) noexcept {
    AtmosphereInspection result{};
    result.pressure_units = atmosphere.pressure_units();
    for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
        result.component_per_mille[index] = static_cast<std::uint16_t>(
            component_per_mille(atmosphere, static_cast<AtmosphereComponent>(index)));
    }
    result.breathable = is_breathable(atmosphere);
    return result;
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
    for (std::size_t index = 0u; index < atmosphere_component_count && remaining > 0u; ++index) {
        if (index == target) continue;
        const auto replaced = (std::min)(remaining, atmosphere.components[index]);
        atmosphere.components[index] -= replaced;
        atmosphere.components[target] += replaced;
        remaining -= replaced;
    }
    return requested_amount - remaining;
}

struct AtmosphereTransferResult final { std::uint32_t moved{}; bool committed{}; };

[[nodiscard]] constexpr AtmosphereTransferResult transfer_atmosphere(
    PackedAtmosphere& source,
    PackedAtmosphere& destination,
    const std::uint32_t requested_amount) noexcept {
    const auto source_total = source.pressure_units();
    const auto accepted = (std::min)(requested_amount, (std::min)(source_total, destination.available_capacity()));
    if (accepted == 0u) return {};
    std::array<std::uint32_t, atmosphere_component_count> moved{};
    std::array<std::uint64_t, atmosphere_component_count> remainders{};
    std::uint32_t base_total = 0u;
    for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
        const auto product = static_cast<std::uint64_t>(source.components[index]) * accepted;
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
            if (selected == atmosphere_component_count || remainders[index] > selected_remainder) {
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

[[nodiscard]] constexpr AtmosphereTransferResult equalize_pressure(
    PackedAtmosphere& first,
    PackedAtmosphere& second,
    const std::uint32_t maximum_transfer = atmosphere_capacity) noexcept {
    auto* source = &first;
    auto* destination = &second;
    if (second.pressure_units() > first.pressure_units()) {
        source = &second;
        destination = &first;
    }
    const auto difference = source->pressure_units() - destination->pressure_units();
    return transfer_atmosphere(*source, *destination, (std::min)(maximum_transfer, difference / 2u));
}

[[nodiscard]] constexpr std::uint32_t respire(
    PackedAtmosphere& atmosphere,
    const std::uint32_t requested_oxygen) noexcept {
    const auto oxygen_index = atmosphere_index(AtmosphereComponent::oxygen);
    const auto carbon_index = atmosphere_index(AtmosphereComponent::carbon_dioxide);
    const auto consumed = (std::min)(requested_oxygen, atmosphere.components[oxygen_index]);
    atmosphere.components[oxygen_index] -= consumed;
    atmosphere.components[carbon_index] += consumed;
    return consumed;
}

[[nodiscard]] constexpr std::uint32_t combust(
    PackedAtmosphere& atmosphere,
    const std::uint32_t requested_oxygen) noexcept {
    return respire(atmosphere, requested_oxygen);
}

[[nodiscard]] constexpr std::uint16_t component_density_milli(
    const AtmosphereComponent component) noexcept {
    switch (component) {
        case AtmosphereComponent::hydrogen: return 70u;
        case AtmosphereComponent::helium: return 140u;
        case AtmosphereComponent::water_vapor: return 620u;
        case AtmosphereComponent::neon: return 695u;
        case AtmosphereComponent::contaminants: return 760u;
        case AtmosphereComponent::nitrogen: return 967u;
        case AtmosphereComponent::oxygen: return 1'105u;
        case AtmosphereComponent::argon: return 1'380u;
        case AtmosphereComponent::carbon_dioxide: return 1'520u;
        case AtmosphereComponent::count: break;
    }
    return 1'000u;
}

[[nodiscard]] constexpr std::uint16_t mixture_density_milli(
    const PackedAtmosphere& atmosphere) noexcept {
    const auto pressure = atmosphere.pressure_units();
    if (pressure == 0u) return 0u;
    std::uint64_t weighted = 0u;
    for (std::size_t index = 0u; index < atmosphere_component_count; ++index) {
        weighted += static_cast<std::uint64_t>(atmosphere.components[index]) *
                    component_density_milli(static_cast<AtmosphereComponent>(index));
    }
    return static_cast<std::uint16_t>(weighted / pressure);
}

enum class GasMotion : std::uint8_t { none = 0, left, right, up, down };
enum class GasBoundary : std::uint8_t { open = 0, solid, liquid, paused, unloaded };

[[nodiscard]] constexpr bool gas_boundary_allows_crossing(const GasBoundary boundary) noexcept {
    return boundary == GasBoundary::open;
}

[[nodiscard]] constexpr bool gas_wall_allows_tangential_motion(
    const GasBoundary normal_boundary,
    const GasBoundary tangent_boundary) noexcept {
    return normal_boundary == GasBoundary::solid && tangent_boundary == GasBoundary::open;
}

[[nodiscard]] constexpr GasMotion choose_gas_motion(
    const PackedAtmosphere& source,
    const PackedAtmosphere& vertical_destination,
    const GasBoundary vertical_boundary,
    const PackedAtmosphere& left_destination,
    const GasBoundary left_boundary,
    const PackedAtmosphere& right_destination,
    const GasBoundary right_boundary) noexcept {
    if (source.pressure_units() == 0u) return GasMotion::none;
    const auto density = mixture_density_milli(source);
    const auto vertical = density < 1'000u ? GasMotion::up : GasMotion::down;
    if (gas_boundary_allows_crossing(vertical_boundary) &&
        vertical_destination.pressure_units() < source.pressure_units()) return vertical;
    const auto left_open = gas_boundary_allows_crossing(left_boundary) &&
                           left_destination.pressure_units() < source.pressure_units();
    const auto right_open = gas_boundary_allows_crossing(right_boundary) &&
                            right_destination.pressure_units() < source.pressure_units();
    if (left_open && right_open) {
        return left_destination.pressure_units() <= right_destination.pressure_units()
            ? GasMotion::left : GasMotion::right;
    }
    if (left_open) return GasMotion::left;
    if (right_open) return GasMotion::right;
    return GasMotion::none;
}

[[nodiscard]] constexpr std::uint32_t reabsorb_excess(
    PackedAtmosphere& atmosphere,
    const PackedAtmosphere& baseline,
    const AtmosphereComponent excess_component) noexcept {
    const auto pressure = atmosphere.pressure_units();
    if (pressure == 0u || baseline.pressure_units() == 0u) return 0u;
    const auto excess_index = atmosphere_index(excess_component);
    const auto desired = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(baseline.components[excess_index]) * pressure /
        baseline.pressure_units());
    if (atmosphere.components[excess_index] <= desired) return 0u;
    auto remaining = atmosphere.components[excess_index] - desired;
    std::uint32_t moved = 0u;
    for (std::size_t index = 0u; index < atmosphere_component_count && remaining > 0u; ++index) {
        if (index == excess_index) continue;
        const auto target = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(baseline.components[index]) * pressure /
            baseline.pressure_units());
        if (atmosphere.components[index] >= target) continue;
        const auto transfer = (std::min)(remaining, target - atmosphere.components[index]);
        atmosphere.components[excess_index] -= transfer;
        atmosphere.components[index] += transfer;
        remaining -= transfer;
        moved += transfer;
    }
    return moved;
}

[[nodiscard]] constexpr bool corner_pressure_is_physical(
    const PackedAtmosphere& corner,
    const PackedAtmosphere& horizontal,
    const PackedAtmosphere& vertical,
    const bool horizontal_open,
    const bool vertical_open) noexcept {
    if (!corner.valid() || !horizontal.valid() || !vertical.valid()) return false;
    if (!horizontal_open && !vertical_open) return true;
    const auto neighbor_min = (std::min)(horizontal.pressure_units(), vertical.pressure_units());
    return corner.pressure_units() <= neighbor_min + atmosphere_capacity / 16u;
}

struct HalfWaterAmbient final {
    bool balanced_air_marker{true};
    std::uint32_t hidden_pressure_units{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return balanced_air_marker && hidden_pressure_units == 0u;
    }
};

[[nodiscard]] constexpr std::optional<AtmosphereComponent>
atmosphere_component_for_material(const Material material) noexcept {
    switch (material) {
        case Material::oxygen: return AtmosphereComponent::oxygen;
        case Material::carbon_dioxide: return AtmosphereComponent::carbon_dioxide;
        case Material::hydrogen: return AtmosphereComponent::hydrogen;
        case Material::steam:
        case Material::dirty_steam: return AtmosphereComponent::water_vapor;
        case Material::smoke: return AtmosphereComponent::contaminants;
        default: return std::nullopt;
    }
}

template <std::size_t CellCount>
[[nodiscard]] constexpr std::size_t fill_connected_with_air(
    std::array<Material, CellCount>& cells,
    const std::size_t width,
    const std::size_t height,
    const std::size_t seed) noexcept {
    if (width == 0u || height == 0u || width * height > CellCount || seed >= width * height) return 0u;
    const auto target = cells[seed];
    if (target == Material::atmosphere) return 0u;
    std::array<std::size_t, CellCount> queue{};
    std::array<bool, CellCount> visited{};
    std::size_t head = 0u;
    std::size_t tail = 0u;
    queue[tail++] = seed;
    visited[seed] = true;
    std::size_t changed = 0u;
    while (head < tail) {
        const auto index = queue[head++];
        if (cells[index] != target) continue;
        cells[index] = Material::atmosphere;
        ++changed;
        const auto x = index % width;
        const auto y = index / width;
        const std::array<std::size_t, 4> neighbors{
            x > 0u ? index - 1u : index,
            x + 1u < width ? index + 1u : index,
            y > 0u ? index - width : index,
            y + 1u < height ? index + width : index};
        for (const auto neighbor : neighbors) {
            if (neighbor == index || visited[neighbor] || cells[neighbor] != target) continue;
            visited[neighbor] = true;
            queue[tail++] = neighbor;
        }
    }
    return changed;
}

template <std::size_t CellCount>
[[nodiscard]] constexpr std::size_t ignite_upper_left_air_region(
    std::array<Material, CellCount>& cells,
    const std::size_t width,
    const std::size_t height) noexcept {
    if (width == 0u || height == 0u || width * height > CellCount) return 0u;
    std::size_t seed = CellCount;
    for (std::size_t index = 0u; index < width * height; ++index) {
        if (cells[index] == Material::atmosphere) { seed = index; break; }
    }
    if (seed == CellCount) return 0u;
    std::array<std::size_t, CellCount> queue{};
    std::array<bool, CellCount> visited{};
    std::size_t head = 0u;
    std::size_t tail = 0u;
    queue[tail++] = seed;
    visited[seed] = true;
    std::size_t changed = 0u;
    while (head < tail) {
        const auto index = queue[head++];
        if (cells[index] != Material::atmosphere) continue;
        cells[index] = Material::fire;
        ++changed;
        const auto x = index % width;
        const auto y = index / width;
        const std::array<std::size_t, 4> neighbors{
            x > 0u ? index - 1u : index,
            x + 1u < width ? index + 1u : index,
            y > 0u ? index - width : index,
            y + 1u < height ? index + width : index};
        for (const auto neighbor : neighbors) {
            if (neighbor == index || visited[neighbor] || cells[neighbor] != Material::atmosphere) continue;
            visited[neighbor] = true;
            queue[tail++] = neighbor;
        }
    }
    return changed;
}

} // namespace sandhybrid
