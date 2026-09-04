#pragma once

#include "sandhybrid/atmosphere.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace sandhybrid {

struct ActorOccupancy final {
    std::uint32_t actor_id{};
    std::uint16_t coverage_per_mille{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return actor_id != 0u && coverage_per_mille <= 1'000u;
    }
};

struct MediumState final {
    PackedAtmosphere atmosphere{};
    std::uint16_t liquid_per_mille{};
    std::int32_t impulse_x{};
    std::int32_t impulse_y{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return atmosphere.valid() && liquid_per_mille <= 1'000u;
    }
};

struct ActorMediumResult final {
    std::uint32_t oxygen_consumed{};
    std::uint32_t carbon_dioxide_produced{};
    std::int32_t applied_impulse_x{};
    std::int32_t applied_impulse_y{};
    bool drowning{};
    bool suffocating{};
};

[[nodiscard]] constexpr std::int32_t bounded_impulse_add(
    const std::int32_t current,
    const std::int32_t delta,
    const std::int32_t limit = 4'096) noexcept {
    const auto sum = static_cast<std::int64_t>(current) +
                     static_cast<std::int64_t>(delta);
    return static_cast<std::int32_t>((std::clamp)(
        sum, -static_cast<std::int64_t>(limit),
        static_cast<std::int64_t>(limit)));
}

[[nodiscard]] constexpr ActorMediumResult interact_actor_with_medium(
    MediumState& medium,
    const ActorOccupancy& occupancy,
    const std::uint32_t oxygen_demand,
    const std::int32_t impulse_x,
    const std::int32_t impulse_y) noexcept {
    ActorMediumResult result{};
    if (!occupancy.valid() || !medium.valid()) {
        result.suffocating = true;
        return result;
    }
    result.drowning = medium.liquid_per_mille >= 800u;
    const auto breathable_before = is_breathable(medium.atmosphere);
    if (!result.drowning && breathable_before) {
        result.oxygen_consumed = respire(medium.atmosphere, oxygen_demand);
        result.carbon_dioxide_produced = result.oxygen_consumed;
    }
    result.suffocating = result.drowning || !breathable_before ||
                         result.oxygen_consumed < oxygen_demand;
    const auto previous_x = medium.impulse_x;
    const auto previous_y = medium.impulse_y;
    medium.impulse_x = bounded_impulse_add(medium.impulse_x, impulse_x);
    medium.impulse_y = bounded_impulse_add(medium.impulse_y, impulse_y);
    result.applied_impulse_x = medium.impulse_x - previous_x;
    result.applied_impulse_y = medium.impulse_y - previous_y;
    return result;
}

enum class ActorSpecies : std::uint8_t {
    player = 0,
    bee,
    queen_bee,
    ant,
    beetle
};

enum class LifeStage : std::uint8_t {
    idle = 0,
    forage,
    carry,
    return_home,
    deposit,
    feed,
    migrate,
    hazard_escape,
    dead
};

struct GridPosition final {
    std::int32_t x{};
    std::int32_t y{};
    friend constexpr bool operator==(GridPosition, GridPosition) noexcept = default;
};

struct ActorComponent final {
    std::uint32_t actor_id{};
    ActorSpecies species{ActorSpecies::player};
    GridPosition position{};
    GridPosition home{};
    LifeStage stage{LifeStage::idle};
    std::uint16_t energy{1'000u};
    bool carrying_food{};
    bool carrying_pollen{};
    bool alive{true};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return actor_id != 0u && energy <= 1'000u;
    }
};

[[nodiscard]] constexpr bool actor_is_material_record(
    const ActorSpecies) noexcept {
    return false;
}

struct LifeDebugCounters final {
    std::uint64_t actor_moves{};
    std::uint64_t respiration_events{};
    std::uint64_t suffocations{};
    std::uint64_t drownings{};
    std::uint64_t births{};
    std::uint64_t deaths{};
    std::uint64_t hive_returns{};
    std::uint64_t pollen_deposits{};
    std::uint64_t honey_feedings{};
    std::uint64_t medium_displacements{};
    std::array<std::uint32_t, 5> species_counts{};
};

[[nodiscard]] constexpr std::size_t species_index(
    const ActorSpecies species) noexcept {
    return static_cast<std::size_t>(species);
}

constexpr void account_actor(
    LifeDebugCounters& counters,
    const ActorComponent& actor) noexcept {
    if (actor.alive && actor.valid()) {
        ++counters.species_counts[species_index(actor.species)];
    }
}

struct BeeLifecycleInput final {
    bool flower_available{};
    bool at_home{};
    bool honey_available{};
    bool hazard{};
    bool migration_required{};
    std::uint32_t colony_population{};
};

struct BeeLifecycleResult final {
    LifeStage next_stage{LifeStage::idle};
    bool collect_pollen{};
    bool deposit_pollen{};
    bool consume_honey{};
    bool die{};
};

inline constexpr std::uint32_t fix29_bee_colony_max = 60u;

[[nodiscard]] constexpr BeeLifecycleResult advance_bee_lifecycle(
    const ActorComponent& bee,
    const BeeLifecycleInput input) noexcept {
    if (!bee.alive || bee.species != ActorSpecies::bee) {
        return {LifeStage::dead, false, false, false, true};
    }
    if (input.hazard) return {LifeStage::hazard_escape, false, false, false, false};
    if (input.migration_required &&
        input.colony_population <= fix29_bee_colony_max) {
        return {LifeStage::migrate, false, false, false, false};
    }
    if (bee.energy < 250u && input.honey_available) {
        return {LifeStage::feed, false, false, true, false};
    }
    if (bee.carrying_pollen) {
        return input.at_home
            ? BeeLifecycleResult{LifeStage::deposit, false, true, false, false}
            : BeeLifecycleResult{LifeStage::return_home, false, false, false, false};
    }
    if (input.flower_available) {
        return {LifeStage::carry, true, false, false, false};
    }
    return {LifeStage::forage, false, false, false, false};
}

[[nodiscard]] constexpr std::uint32_t capped_bee_births(
    const std::uint32_t population,
    const std::uint32_t requested_births) noexcept {
    return population >= fix29_bee_colony_max
        ? 0u
        : (std::min)(requested_births, fix29_bee_colony_max - population);
}

struct FormationOffset final {
    std::int8_t x{};
    std::int8_t y{};
    friend constexpr bool operator==(FormationOffset, FormationOffset) noexcept = default;
};

[[nodiscard]] constexpr FormationOffset biohazard_formation_offset(
    const std::size_t index,
    const std::uint32_t tick) noexcept {
    constexpr std::array<FormationOffset, 12> formation{{
        {0, -4}, {1, -3}, {2, -2}, {4, 0}, {3, 1}, {2, 2},
        {0, 4}, {-1, 3}, {-2, 2}, {-4, 0}, {-3, -1}, {-2, -2}}};
    static_cast<void>(tick);
    return formation[index % formation.size()];
}

enum class AntIntent : std::uint8_t {
    forage = 0,
    follow_pheromone,
    return_home,
    avoid_hazard,
    escape_flood,
    dig
};

struct AntEnvironment final {
    bool carrying_food{};
    bool hazard{};
    bool flooded{};
    bool food_visible{};
    bool pheromone_visible{};
    bool permitted_dig_cell{};
};

[[nodiscard]] constexpr AntIntent choose_ant_intent(
    const AntEnvironment environment) noexcept {
    if (environment.hazard) return AntIntent::avoid_hazard;
    if (environment.flooded) return AntIntent::escape_flood;
    if (environment.carrying_food) return AntIntent::return_home;
    if (environment.food_visible) return AntIntent::forage;
    if (environment.pheromone_visible) return AntIntent::follow_pheromone;
    if (environment.permitted_dig_cell) return AntIntent::dig;
    return AntIntent::forage;
}

enum class BeetleIntent : std::uint8_t {
    crawl_forward = 0,
    turn_left,
    turn_right,
    seek_shelter,
    avoid_light,
    escape_hazard
};

struct BeetleEnvironment final {
    bool forward_surface{};
    bool left_surface{};
    bool right_surface{};
    bool bright_light{};
    bool shelter_visible{};
    bool hazard{};
};

[[nodiscard]] constexpr BeetleIntent choose_beetle_intent(
    const BeetleEnvironment environment) noexcept {
    if (environment.hazard) return BeetleIntent::escape_hazard;
    if (environment.bright_light) return BeetleIntent::avoid_light;
    if (environment.shelter_visible) return BeetleIntent::seek_shelter;
    if (environment.forward_surface) return BeetleIntent::crawl_forward;
    if (environment.left_surface) return BeetleIntent::turn_left;
    return environment.right_surface ? BeetleIntent::turn_right
                                     : BeetleIntent::turn_left;
}

struct HabitatState final {
    ActorSpecies species{ActorSpecies::ant};
    std::uint32_t population{};
    std::uint32_t capacity{100u};
    std::uint32_t food{};
    std::uint32_t water{};
    std::uint32_t waste{};
    std::uint32_t next_birth_tick{};
};

struct HabitatTransaction final {
    bool committed{};
    bool blocked_capacity{};
    bool missing_inputs{};
};

[[nodiscard]] constexpr HabitatTransaction transact_habitat_birth(
    HabitatState& habitat,
    const std::uint32_t tick,
    const std::uint32_t interval_ticks = 600u) noexcept {
    if (habitat.species != ActorSpecies::ant &&
        habitat.species != ActorSpecies::beetle) return {};
    if (habitat.population >= habitat.capacity) {
        return {false, true, false};
    }
    if (habitat.food == 0u || habitat.water == 0u || tick < habitat.next_birth_tick) {
        return {false, false, true};
    }
    --habitat.food;
    --habitat.water;
    ++habitat.waste;
    ++habitat.population;
    habitat.next_birth_tick = tick + interval_ticks;
    return {true, false, false};
}

inline constexpr std::uint32_t pre_pr19_hive_canonical_width = 640u;
inline constexpr std::int32_t pre_pr19_hive_canonical_queen_x = 512;
inline constexpr std::int32_t fix29_hive_sandbox_queen_y = 234;
inline constexpr std::int32_t fix29_hive_ecosystem_queen_y = 232;
inline constexpr std::int32_t pre_pr19_hive_canonical_queen_y =
    fix29_hive_ecosystem_queen_y;
inline constexpr std::uint32_t pre_pr19_hive_canonical_seed = 0xD17A5EEDu;
inline constexpr std::size_t fix29_bee_formation_count = 60u;
inline constexpr std::uint32_t fix29_bee_timer_bits = 14u;
inline constexpr std::uint32_t fix29_bee_timer_mask =
    (1u << fix29_bee_timer_bits) - 1u;
inline constexpr std::uint32_t fix29_bee_target_none =
    (1u << (32u - fix29_bee_timer_bits)) - 1u;
inline constexpr std::uint32_t fix29_bee_target_newborn =
    fix29_bee_target_none - 1u;
inline constexpr std::uint32_t fix29_bee_departure_base_ticks = 1200u;
inline constexpr std::uint32_t fix29_bee_departure_span_ticks = 600u;
inline constexpr std::uint32_t fix29_bee_forager_stagger_ticks = 12u;
inline constexpr std::uint32_t fix29_bee_forager_activation_window_ticks = 61u;
inline constexpr std::int32_t fix29_bee_formation_min_x = -20;
inline constexpr std::int32_t fix29_bee_formation_max_x = 20;
inline constexpr std::int32_t fix29_bee_formation_min_y = -22;
inline constexpr std::int32_t fix29_bee_formation_max_y = 14;

inline constexpr std::array<std::uint16_t, fix29_bee_formation_count>
    fix29_bee_formation_packed{{
        5437u, 5564u, 5572u, 5691u, 5701u, 5818u, 5830u, 5946u, 5958u, 6075u,
        6085u, 6204u, 6205u, 6211u, 6212u, 6334u, 6335u, 6336u, 6337u, 6338u,
        8495u, 8496u, 8497u, 8527u, 8528u, 8529u, 8626u, 8654u, 8755u, 8781u,
        8884u, 8908u, 9013u, 9035u, 9142u, 9162u, 9270u, 9290u, 9398u, 9418u,
        9525u, 9547u, 9644u, 9652u, 9676u, 9684u, 9773u, 9779u, 9805u, 9811u,
        9902u, 9906u, 9934u, 9938u, 10031u, 10032u, 10033u, 10063u, 10064u,
        10065u}};

[[nodiscard]] constexpr std::uint32_t fix29_bee_pack_age(
    const std::uint32_t timer, const std::uint32_t target_tile) noexcept {
    return (std::min)(timer, fix29_bee_timer_mask) |
           ((std::min)(target_tile, fix29_bee_target_none) <<
            fix29_bee_timer_bits);
}

[[nodiscard]] constexpr std::uint32_t fix29_bee_timer_from_age(
    const std::uint32_t age) noexcept {
    return age & fix29_bee_timer_mask;
}

[[nodiscard]] constexpr std::uint32_t fix29_bee_target_from_age(
    const std::uint32_t age) noexcept {
    return age >> fix29_bee_timer_bits;
}

[[nodiscard]] constexpr bool fix29_bee_forager_slot(
    const std::size_t slot) noexcept {
    return ((slot * 37u + 11u) % 10u) == 0u;
}

[[nodiscard]] constexpr std::uint32_t fix29_bee_forager_ordinal(
    const std::size_t slot) noexcept {
    return static_cast<std::uint32_t>(
        (std::min)(slot, fix29_bee_formation_count - 1u) / 10u);
}

[[nodiscard]] constexpr std::uint32_t fix29_bee_departure_threshold(
    const std::size_t slot) noexcept {
    return fix29_bee_departure_base_ticks +
           static_cast<std::uint32_t>(
               (slot * 29u) % fix29_bee_departure_span_ticks);
}

[[nodiscard]] constexpr std::uint32_t fix29_bee_initial_timer(
    const std::size_t slot) noexcept {
    if (!fix29_bee_forager_slot(slot))
        return static_cast<std::uint32_t>((slot * 17u) % 900u);
    const auto activation_tick =
        1u + fix29_bee_forager_ordinal(slot) *
                 fix29_bee_forager_stagger_ticks;
    return fix29_bee_departure_threshold(slot) - activation_tick;
}

[[nodiscard]] constexpr FormationOffset fix29_bee_formation_offset(
    const std::size_t slot) noexcept {
    const auto packed = fix29_bee_formation_packed[
        (std::min)(slot, fix29_bee_formation_count - 1u)];
    return {
        static_cast<std::int8_t>(static_cast<std::int32_t>(packed & 127u) - 64),
        static_cast<std::int8_t>(static_cast<std::int32_t>(packed >> 7u) - 64)};
}

[[nodiscard]] constexpr std::int32_t fix29_bee_formation_slot(
    const std::int32_t offset_x, const std::int32_t offset_y) noexcept {
    if (offset_x < -64 || offset_x > 63 || offset_y < -64 || offset_y > 63)
        return -1;
    const auto key = static_cast<std::uint16_t>(
        ((offset_y + 64) << 7) | (offset_x + 64));
    std::size_t low = 0u;
    std::size_t high = fix29_bee_formation_count;
    while (low < high) {
        const auto middle = low + (high - low) / 2u;
        if (fix29_bee_formation_packed[middle] < key) low = middle + 1u;
        else high = middle;
    }
    return low < fix29_bee_formation_count &&
                   fix29_bee_formation_packed[low] == key
        ? static_cast<std::int32_t>(low)
        : -1;
}

[[nodiscard]] constexpr std::uint32_t pre_pr19_hive_hash(
    std::uint32_t value) noexcept {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

[[nodiscard]] constexpr std::uint32_t fix29_hive_entropy(
    const std::int32_t queen_x,
    const std::int32_t queen_y,
    const std::int32_t dx,
    const std::int32_t dy) noexcept {
    const auto x = static_cast<std::uint32_t>(queen_x + dx);
    const auto y = static_cast<std::uint32_t>(queen_y + dy);
    return pre_pr19_hive_hash(
        (y * pre_pr19_hive_canonical_width + x) ^ pre_pr19_hive_canonical_seed);
}

[[nodiscard]] constexpr std::uint32_t canonical_pre_pr19_hive_entropy(
    const std::int32_t dx,
    const std::int32_t dy) noexcept {
    return fix29_hive_entropy(
        pre_pr19_hive_canonical_queen_x, pre_pr19_hive_canonical_queen_y, dx, dy);
}

enum class HivePart : std::uint8_t {
    empty = 0,
    shell,
    chamber,
    queen,
    exit,
    honey,
    pollen
};

[[nodiscard]] constexpr HivePart classify_pre_pr19_hive_cell(
    const std::int32_t dx,
    const std::int32_t dy,
    const std::uint32_t entropy = 1u,
    const std::int32_t queen_x = pre_pr19_hive_canonical_queen_x,
    const std::int32_t queen_y = pre_pr19_hive_canonical_queen_y) noexcept {
    if (dx == 0 && dy == 0) return HivePart::queen;
    if (dx >= 1 && dx <= 10 && dy >= -1 && dy <= 1) return HivePart::exit;
    const auto radius_squared = dx * dx + dy * dy;
    if (radius_squared >= 24 && radius_squared < 88) return HivePart::shell;
    if (radius_squared < 24) {
        if ((entropy & 3u) == 0u) return HivePart::chamber;
        return ((entropy >> 2u) & 3u) == 0u ? HivePart::pollen : HivePart::honey;
    }
    static_cast<void>(queen_x);
    static_cast<void>(queen_y);
    return HivePart::empty;
}
[[nodiscard]] constexpr GridPosition hive_home_from_scene_origin(
    const GridPosition scene_origin,
    const GridPosition local_queen) noexcept {
    return {scene_origin.x + local_queen.x, scene_origin.y + local_queen.y};
}

} // namespace sandhybrid
