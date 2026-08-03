#include <sandhybrid/atmosphere.hpp>

#include <cstddef>

int main() {
    auto source = sandhybrid::make_earth_atmosphere();
    if (!source.valid()) return 1;
    if (source.pressure_units() != sandhybrid::atmosphere_capacity) return 2;
    if (sandhybrid::oxygen_per_mille(source) < 210u ||
        sandhybrid::oxygen_per_mille(source) > 212u) return 3;

    sandhybrid::PackedAtmosphere destination{};
    const auto total_before =
        source.pressure_units() + destination.pressure_units();
    const auto moved =
        sandhybrid::transfer_atmosphere(source, destination, 12'345u);
    if (!moved.committed || moved.moved != 12'345u) return 4;
    if (source.pressure_units() + destination.pressure_units() != total_before) {
        return 5;
    }

    for (std::size_t index = 0u;
         index < sandhybrid::atmosphere_component_count;
         ++index) {
        const auto original =
            sandhybrid::make_earth_atmosphere().components[index];
        if (source.components[index] + destination.components[index] != original) {
            return 6;
        }
    }

    const auto pressure_before_respiration = destination.pressure_units();
    const auto oxygen_before = destination.amount(
        sandhybrid::AtmosphereComponent::oxygen);
    const auto carbon_before = destination.amount(
        sandhybrid::AtmosphereComponent::carbon_dioxide);
    const auto consumed = sandhybrid::respire(destination, 50u);
    if (consumed != 50u) return 7;
    if (destination.pressure_units() != pressure_before_respiration) return 8;
    if (destination.amount(sandhybrid::AtmosphereComponent::oxygen) + consumed !=
        oxygen_before) return 9;
    if (destination.amount(
            sandhybrid::AtmosphereComponent::carbon_dioxide) !=
        carbon_before + consumed) return 10;

    auto enriched = sandhybrid::make_earth_atmosphere();
    const auto enriched_pressure = enriched.pressure_units();
    const auto hydrogen_before = enriched.amount(
        sandhybrid::AtmosphereComponent::hydrogen);
    const auto changed = sandhybrid::enrich_atmosphere(
        enriched, sandhybrid::AtmosphereComponent::hydrogen, 500u);
    if (changed != 500u) return 11;
    if (enriched.pressure_units() != enriched_pressure) return 12;
    if (enriched.amount(sandhybrid::AtmosphereComponent::hydrogen) !=
        hydrogen_before + changed) return 13;

    const auto oxygen_component =
        sandhybrid::atmosphere_component_for_material(
            sandhybrid::Material::oxygen);
    if (!oxygen_component.has_value() ||
        *oxygen_component != sandhybrid::AtmosphereComponent::oxygen) return 14;
    if (sandhybrid::atmosphere_component_for_material(
            sandhybrid::Material::stone).has_value()) return 15;

    return 0;
}
