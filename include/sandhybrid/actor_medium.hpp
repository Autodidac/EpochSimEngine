#pragma once

#include "sandhybrid/atmosphere.hpp"

#include <algorithm>
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
    return static_cast<std::int32_t>(
        (std::clamp)(sum,
                     -static_cast<std::int64_t>(limit),
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

    result.suffocating =
        result.drowning || !breathable_before ||
        result.oxygen_consumed < oxygen_demand;

    const auto previous_x = medium.impulse_x;
    const auto previous_y = medium.impulse_y;
    medium.impulse_x = bounded_impulse_add(medium.impulse_x, impulse_x);
    medium.impulse_y = bounded_impulse_add(medium.impulse_y, impulse_y);
    result.applied_impulse_x = medium.impulse_x - previous_x;
    result.applied_impulse_y = medium.impulse_y - previous_y;

    return result;
}

} // namespace sandhybrid
