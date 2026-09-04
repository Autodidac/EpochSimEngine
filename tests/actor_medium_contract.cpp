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
        bee, {.flower_available = true, .colony_population = 59u});
    if (!bee_result.collect_pollen || bee_result.next_stage != sandhybrid::LifeStage::carry) return 8;
    bee.carrying_pollen = true;
    bee_result = sandhybrid::advance_bee_lifecycle(
        bee, {.at_home = true, .colony_population = 60u});
    if (!bee_result.deposit_pollen || bee_result.next_stage != sandhybrid::LifeStage::deposit) return 9;
    if (sandhybrid::capped_bee_births(59u, 5u) != 1u ||
        sandhybrid::capped_bee_births(60u, 1u) != 0u) return 10;

    const auto formation_a = sandhybrid::biohazard_formation_offset(0u, 0u);
    const auto formation_b = sandhybrid::biohazard_formation_offset(0u, 120u);
    if (formation_a != formation_b) return 11;
    std::uint32_t forager_slots = 0u;
    std::uint32_t previous_activation_tick = 0u;
    std::uint32_t latest_activation_tick = 0u;
    for (std::size_t slot = 0u;
         slot < sandhybrid::fix29_bee_formation_count; ++slot) {
        if (!sandhybrid::fix29_bee_forager_slot(slot)) {
            if (sandhybrid::fix29_bee_initial_timer(slot) !=
                (slot * 17u) % 900u) return 46;
            continue;
        }
        const auto threshold =
            sandhybrid::fix29_bee_departure_threshold(slot);
        const auto initial_timer =
            sandhybrid::fix29_bee_initial_timer(slot);
        if (initial_timer >= threshold) return 47;
        const auto activation_tick = threshold - initial_timer;
        const auto expected_tick =
            1u + forager_slots *
                     sandhybrid::fix29_bee_forager_stagger_ticks;
        if (sandhybrid::fix29_bee_forager_ordinal(slot) != forager_slots ||
            activation_tick != expected_tick ||
            activation_tick <= previous_activation_tick) return 48;
        previous_activation_tick = activation_tick;
        latest_activation_tick = activation_tick;
        ++forager_slots;
    }
    if (forager_slots != 6u) return 36;
    if (latest_activation_tick !=
            sandhybrid::fix29_bee_forager_activation_window_ticks)
        return 49;
    std::uint32_t formation_fingerprint = 2166136261u;
    for (std::size_t slot = 0u;
         slot < sandhybrid::fix29_bee_formation_count; ++slot) {
        const auto packed = sandhybrid::fix29_bee_formation_packed[slot];
        formation_fingerprint ^= packed & 0xffu;
        formation_fingerprint *= 16777619u;
        formation_fingerprint ^= packed >> 8u;
        formation_fingerprint *= 16777619u;
        const auto point = sandhybrid::fix29_bee_formation_offset(slot);
        if (sandhybrid::classify_pre_pr19_hive_cell(point.x, point.y) !=
            sandhybrid::HivePart::empty) return 40;
        if (sandhybrid::fix29_bee_formation_slot(point.x, point.y) !=
            static_cast<std::int32_t>(slot)) return 37;
        bool connected = false;
        for (std::size_t other = 0u;
             other < sandhybrid::fix29_bee_formation_count; ++other) {
            if (slot == other) continue;
            const auto neighbor =
                sandhybrid::fix29_bee_formation_offset(other);
            const auto dx = point.x - neighbor.x;
            const auto dy = point.y - neighbor.y;
            if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
                connected = true;
                break;
            }
        }
        if (!connected) return 38;
    }
    if (formation_fingerprint != 0xbeb650bdu) return 39;
    std::array<bool, sandhybrid::fix29_bee_formation_count> visited{};
    std::array<std::size_t, sandhybrid::fix29_bee_formation_count> stack{};
    std::uint32_t components = 0u;
    for (std::size_t start = 0u;
         start < sandhybrid::fix29_bee_formation_count; ++start) {
        if (visited[start]) continue;
        std::size_t stack_size = 0u;
        std::uint32_t component_size = 0u;
        stack[stack_size++] = start;
        visited[start] = true;
        while (stack_size > 0u) {
            const auto current = stack[--stack_size];
            ++component_size;
            const auto point =
                sandhybrid::fix29_bee_formation_offset(current);
            for (std::size_t other = 0u;
                 other < sandhybrid::fix29_bee_formation_count; ++other) {
                if (visited[other]) continue;
                const auto neighbor =
                    sandhybrid::fix29_bee_formation_offset(other);
                const auto dx = point.x - neighbor.x;
                const auto dy = point.y - neighbor.y;
                if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
                    visited[other] = true;
                    stack[stack_size++] = other;
                }
            }
        }
        if (component_size != 20u) return 41;
        ++components;
    }
    if (components != 3u) return 42;
    // The golden hive is the central biohazard ring. Each 20-bee outer
    // crescent must face its opening away from that body.
    if (sandhybrid::fix29_bee_formation_slot(0, -22) >= 0 ||
        sandhybrid::fix29_bee_formation_slot(0, -15) < 0) return 43;
    if (sandhybrid::fix29_bee_formation_slot(-20, 8) >= 0 ||
        sandhybrid::fix29_bee_formation_slot(-10, 8) < 0) return 44;
    if (sandhybrid::fix29_bee_formation_slot(20, 8) >= 0 ||
        sandhybrid::fix29_bee_formation_slot(10, 8) < 0) return 45;

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
        sandhybrid::HivePart::empty) return 21;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 0) !=
        sandhybrid::HivePart::queen) return 22;
    if (sandhybrid::classify_pre_pr19_hive_cell(10, 1) !=
        sandhybrid::HivePart::exit) return 23;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 9) !=
        sandhybrid::HivePart::shell) return 24;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 1u) !=
        sandhybrid::HivePart::pollen) return 25;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 5u) !=
        sandhybrid::HivePart::honey) return 26;
    if (sandhybrid::classify_pre_pr19_hive_cell(0, 1, 0u) !=
        sandhybrid::HivePart::chamber) return 27;
    if (sandhybrid::fix29_hive_entropy(512, 232, 0, -3) != 0x1c707b05u)
        return 28;
    if (sandhybrid::fix29_hive_entropy(512, 234, 0, -3) != 0x04572a8au)
        return 29;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            -1, -3, sandhybrid::fix29_hive_entropy(512, 232, -1, -3)) !=
        sandhybrid::HivePart::chamber) return 30;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            -1, -3, sandhybrid::fix29_hive_entropy(512, 234, -1, -3)) !=
        sandhybrid::HivePart::honey) return 31;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            -1, -1, sandhybrid::fix29_hive_entropy(512, 232, -1, -1)) !=
        sandhybrid::HivePart::honey) return 32;
    if (sandhybrid::classify_pre_pr19_hive_cell(
            -1, -1, sandhybrid::fix29_hive_entropy(512, 234, -1, -1)) !=
        sandhybrid::HivePart::honey) return 33;
    if (sandhybrid::hive_home_from_scene_origin({1280, 720}, {100, 50}) !=
        sandhybrid::GridPosition{1380, 770}) return 34;

    sandhybrid::LifeDebugCounters counters{};
    sandhybrid::account_actor(counters, bee);
    if (counters.species_counts[
            sandhybrid::species_index(sandhybrid::ActorSpecies::bee)] != 1u) return 35;
    return 0;
}
    constexpr auto large_world_last_tile = 1280u * 180u - 1u;
    static_assert(large_world_last_tile < sandhybrid::fix29_bee_target_none);
    constexpr auto packed_large_target = sandhybrid::fix29_bee_pack_age(
        1'701u, large_world_last_tile);
    static_assert(sandhybrid::fix29_bee_timer_from_age(packed_large_target) == 1'701u);
    static_assert(sandhybrid::fix29_bee_target_from_age(packed_large_target) ==
                  large_world_last_tile);
