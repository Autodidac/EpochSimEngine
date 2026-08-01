#pragma once

#include <cstdint>

namespace sandhybrid {

struct DirectionalInputRouting final {
    std::int32_t camera_x{};
    std::int32_t camera_y{};
    std::int32_t player_x{};
    std::int32_t player_y{};

    friend constexpr bool operator==(const DirectionalInputRouting&,
                                     const DirectionalInputRouting&) = default;
};

[[nodiscard]] constexpr DirectionalInputRouting route_directional_input(
    const bool player_present,
    const bool move_left,
    const bool move_right,
    const bool move_up,
    const bool move_down) noexcept {
    const auto horizontal = static_cast<std::int32_t>(move_right) -
                            static_cast<std::int32_t>(move_left);
    const auto vertical = static_cast<std::int32_t>(move_down) -
                          static_cast<std::int32_t>(move_up);

    if (player_present) {
        return {0, 0, horizontal, vertical};
    }
    return {horizontal, vertical, 0, 0};
}

} // namespace sandhybrid
