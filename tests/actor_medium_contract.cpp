#include <sandhybrid/actor_medium.hpp>

int main() {
    sandhybrid::MediumState medium{};
    medium.atmosphere = sandhybrid::make_earth_atmosphere();
    medium.liquid_per_mille = 100u;

    const auto pressure_before = medium.atmosphere.pressure_units();
    const sandhybrid::ActorOccupancy actor{42u, 600u};
    const auto interaction = sandhybrid::interact_actor_with_medium(
        medium, actor, 20u, 300, -200);

    if (interaction.drowning || interaction.suffocating) return 1;
    if (interaction.oxygen_consumed != 20u ||
        interaction.carbon_dioxide_produced != 20u) return 2;
    if (medium.atmosphere.pressure_units() != pressure_before) return 3;
    if (medium.liquid_per_mille != 100u) return 4;
    if (medium.impulse_x != 300 || medium.impulse_y != -200) return 5;

    medium.liquid_per_mille = 900u;
    const auto oxygen_before = medium.atmosphere.amount(
        sandhybrid::AtmosphereComponent::oxygen);
    const auto drowning = sandhybrid::interact_actor_with_medium(
        medium, actor, 20u, 10'000, -10'000);
    if (!drowning.drowning || !drowning.suffocating) return 6;
    if (medium.atmosphere.amount(
            sandhybrid::AtmosphereComponent::oxygen) != oxygen_before) return 7;
    if (medium.impulse_x != 4'096 || medium.impulse_y != -4'096) return 8;

    const sandhybrid::ActorOccupancy invalid_actor{};
    const auto invalid = sandhybrid::interact_actor_with_medium(
        medium, invalid_actor, 1u, 0, 0);
    if (!invalid.suffocating) return 9;

    return 0;
}
