#include <sandhybrid/atmosphere.hpp>

#include <array>
#include <cstddef>

int main() {
    auto source = sandhybrid::make_earth_atmosphere();
    if (!source.valid()) return 1;
    if (source.pressure_units() != sandhybrid::atmosphere_capacity) return 2;
    if (sandhybrid::oxygen_per_mille(source) < 210u ||
        sandhybrid::oxygen_per_mille(source) > 212u) return 3;

    const auto inspection = sandhybrid::inspect_atmosphere(source);
    if (!inspection.breathable || inspection.pressure_units != sandhybrid::atmosphere_capacity) return 4;
    if (inspection.component_per_mille[
            sandhybrid::atmosphere_index(sandhybrid::AtmosphereComponent::oxygen)] < 210u) return 5;

    sandhybrid::PackedAtmosphere destination{};
    const auto total_before = source.pressure_units() + destination.pressure_units();
    const auto moved = sandhybrid::transfer_atmosphere(source, destination, 12'345u);
    if (!moved.committed || moved.moved != 12'345u) return 6;
    if (source.pressure_units() + destination.pressure_units() != total_before) return 7;

    for (std::size_t index = 0u; index < sandhybrid::atmosphere_component_count; ++index) {
        const auto original = sandhybrid::make_earth_atmosphere().components[index];
        if (source.components[index] + destination.components[index] != original) return 8;
    }

    const auto equalized = sandhybrid::equalize_pressure(source, destination);
    if (!equalized.committed) return 9;
    const auto pressure_gap = source.pressure_units() > destination.pressure_units()
        ? source.pressure_units() - destination.pressure_units()
        : destination.pressure_units() - source.pressure_units();
    if (pressure_gap > 1u) return 10;

    const auto pressure_before_respiration = destination.pressure_units();
    const auto consumed = sandhybrid::respire(destination, 50u);
    if (consumed != 50u || destination.pressure_units() != pressure_before_respiration) return 11;
    const auto burned = sandhybrid::combust(destination, 25u);
    if (burned != 25u || destination.pressure_units() != pressure_before_respiration) return 12;

    auto enriched = sandhybrid::make_earth_atmosphere();
    const auto enriched_pressure = enriched.pressure_units();
    if (sandhybrid::enrich_atmosphere(
            enriched, sandhybrid::AtmosphereComponent::hydrogen, 500u) != 500u) return 13;
    if (enriched.pressure_units() != enriched_pressure) return 14;
    if (sandhybrid::reabsorb_excess(
            enriched, sandhybrid::make_earth_atmosphere(),
            sandhybrid::AtmosphereComponent::hydrogen) == 0u) return 15;
    if (enriched.pressure_units() != enriched_pressure) return 16;

    sandhybrid::PackedAtmosphere smoke{};
    smoke.components[sandhybrid::atmosphere_index(
        sandhybrid::AtmosphereComponent::contaminants)] = sandhybrid::atmosphere_capacity;
    if (sandhybrid::mixture_density_milli(smoke) >= 1'000u) return 17;
    const sandhybrid::PackedAtmosphere vacuum{};
    if (sandhybrid::choose_gas_motion(
            smoke, vacuum, sandhybrid::GasBoundary::open,
            vacuum, sandhybrid::GasBoundary::open,
            vacuum, sandhybrid::GasBoundary::open) != sandhybrid::GasMotion::up) return 18;
    if (!sandhybrid::gas_wall_allows_tangential_motion(
            sandhybrid::GasBoundary::solid, sandhybrid::GasBoundary::open)) return 19;
    if (sandhybrid::gas_wall_allows_tangential_motion(
            sandhybrid::GasBoundary::solid, sandhybrid::GasBoundary::paused)) return 20;

    if (!sandhybrid::corner_pressure_is_physical(
            vacuum, vacuum, vacuum, true, true)) return 21;
    sandhybrid::HalfWaterAmbient ambient{};
    if (!ambient.valid()) return 22;
    ambient.hidden_pressure_units = 1u;
    if (ambient.valid()) return 23;

    std::array<sandhybrid::Material, 12> fill_cells{
        sandhybrid::Material::water, sandhybrid::Material::water, sandhybrid::Material::stone,
        sandhybrid::Material::water, sandhybrid::Material::stone, sandhybrid::Material::stone,
        sandhybrid::Material::atmosphere, sandhybrid::Material::atmosphere, sandhybrid::Material::stone,
        sandhybrid::Material::atmosphere, sandhybrid::Material::stone, sandhybrid::Material::atmosphere};
    if (sandhybrid::fill_connected_with_air(fill_cells, 3u, 4u, 0u) != 3u) return 24;
    if (fill_cells[0] != sandhybrid::Material::atmosphere ||
        fill_cells[2] != sandhybrid::Material::stone) return 25;
    if (sandhybrid::ignite_upper_left_air_region(fill_cells, 3u, 4u) != 6u) return 26;
    if (fill_cells[0] != sandhybrid::Material::fire ||
        fill_cells[11] != sandhybrid::Material::atmosphere) return 27;

    const auto oxygen_component = sandhybrid::atmosphere_component_for_material(
        sandhybrid::Material::oxygen);
    if (!oxygen_component.has_value() ||
        *oxygen_component != sandhybrid::AtmosphereComponent::oxygen) return 28;
    if (sandhybrid::atmosphere_component_for_material(
            sandhybrid::Material::stone).has_value()) return 29;
    return 0;
}
