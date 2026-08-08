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
    if (medium.impulse_x != 300 || medium.impulse_y != -200) return 4;

    medium.liquid_per_mille = 900u;
    const auto drowning = sandhybrid::interact_actor_with_medium(
        medium, actor, 20u, 10'000, -10'000);
    if (!drowning.drowning || !drowning.suffocating) return 5;
    if (medium.impulse_x != 4'096 || medium.impulse_y != -4'096) return 6;

    sandhybrid::ActorComponent bee{
        7u, sandhybrid::ActorSpecies::bee, {10, 10}, {20, 20},
        sandhybrid::LifeStage::forage, 800u, false, false, true};
    if (!bee.valid() || sandhybrid::actor_is_material_record(bee.species)) return 7;
    auto bee_result = sandhybrid::advance_bee_lifecycle(
        bee, {.flower_available = true, .colony_population = 99u});
    if (!bee_result.collect_pollen || bee_result.next_stage != sandhybrid::LifeStage::carry) return 8;
    bee.carrying_pollen = true;
    bee_result = sandhybrid::advance_bee_lifecycle(
        bee, {.at_home = true, .colony_population = 100u});
    if (!bee_result.deposit_pollen || bee_result.next_stage != sandhybrid::LifeStage::deposit) return 9;
    if (sandhybrid::capped_bee_births(99u, 5u) != 1u ||
        sandhybrid::capped_bee_births(100u, 1u) != 0u) return 10;

    const auto formation_a = sandhybrid::biohazard_formation_offset(0u, 0u);
    const auto formation_b = sandhybrid::biohazard_formation_offset(0u, 120u);
    if (formation_a == formation_b) return 11;

    if (sandhybrid::choose_ant_intent({.hazard = true}) !=
        sandhybrid::AntIntent::avoid_hazard) return 12;
    if (sandhybrid::choose_ant_intent({.flooded = true}) !=
        sandhybrid::AntIntent::escape_flood) return 13;
    if (sandhybrid::choose_ant_intent({.carrying_food = true}) !=
        sandhybrid::AntIntent::return_home) return 14;
    if (sandhybrid::choose_ant_intent({.permitted_dig_cell = true}) !=
        sandhybrid::AntIntent::dig) return 15;

    if (sandhybrid::choose_beetle_intent({.hazard = true}) !=
        sandhybrid::BeetleIntent::escape_hazard) return 16;
    if (sandhybrid::choose_beetle_intent({.bright_light = true}) !=
        sandhybrid::BeetleIntent::avoid_light) return 17;
    if (sandhybrid::choose_beetle_intent({.forward_surface = true}) !=
        sandhybrid::BeetleIntent::crawl_forward) return 18;

    sandhybrid::HabitatState habitat{
        sandhybrid::ActorSpecies::ant, 4u, 5u, 1u, 1u, 0u, 0u};
    const auto birth = sandhybrid::transact_habitat_birth(habitat, 0u);
    if (!birth.committed || habitat.population != 5u || habitat.food != 0u ||
        habitat.water != 0u || habitat.waste != 1u) return 19;
    const auto blocked = sandhybrid::transact_habitat_birth(habitat, 600u);
    if (!blocked.blocked_capacity) return 20;

    if (sandhybrid::classify_pre_pr19_hive_cell(-40, -16) !=
        sandhybrid::HivePart::support) return 21;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 0) !=
        sandhybrid::HivePart::queen) return 22;
    if (sandhybrid::classify_pre_pr19_hive_cell(12, 1) !=
        sandhybrid::HivePart::exit) return 23;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 10) !=
        sandhybrid::HivePart::shell) return 24;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 1u) !=
        sandhybrid::HivePart::honey) return 25;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 5u) !=
        sandhybrid::HivePart::pollen) return 26;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 0u) !=
        sandhybrid::HivePart::chamber) return 27;
    if (sandhybrid::canonical_pre_pr19_hive_entropy(0, -3) != 0x0e8d5ce0u)
        return 28;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            0, -3, sandhybrid::canonical_pre_pr19_hive_entropy(0, -3)) !=
        sandhybrid::HivePart::chamber) return 29;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            -1, -5, sandhybrid::canonical_pre_pr19_hive_entropy(-1, -5)) !=
        sandhybrid::HivePart::honey) return 30;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            1, -5, sandhybrid::canonical_pre_pr19_hive_entropy(1, -5)) !=
        sandhybrid::HivePart::pollen) return 31;
    if (sandhybrid::hive_home_from_scene_origin({1280, 720}, {100, 50}) !=
        sandhybrid::GridPosition{1380, 770}) return 32;

    sandhybrid::LifeDebugCounters counters{};
    sandhybrid::account_actor(counters, bee);
    if (counters.species_counts[
            sandhybrid::species_index(sandhybrid::ActorSpecies::bee)] != 1u) return 33;
    return 0;
}
