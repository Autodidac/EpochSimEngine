#pragma once

#include <algorithm>
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

struct EdgePanDirection final {
    std::int32_t x{};
    std::int32_t y{};

    friend constexpr bool operator==(const EdgePanDirection&,
                                     const EdgePanDirection&) = default;
};

[[nodiscard]] constexpr bool player_wasd_enabled(
    const bool player_present, const bool camera_controls) noexcept {
    return player_present && !camera_controls;
}

[[nodiscard]] constexpr bool camera_wasd_enabled(
    const bool player_present, const bool camera_controls) noexcept {
    return !player_present || camera_controls;
}

[[nodiscard]] constexpr DirectionalInputRouting route_directional_input(
    const bool route_to_player,
    const bool move_left,
    const bool move_right,
    const bool move_up,
    const bool move_down) noexcept {
    const auto horizontal = static_cast<std::int32_t>(move_right) -
                            static_cast<std::int32_t>(move_left);
    const auto vertical = static_cast<std::int32_t>(move_down) -
                          static_cast<std::int32_t>(move_up);

    if (route_to_player) {
        return {0, 0, horizontal, vertical};
    }
    return {horizontal, vertical, 0, 0};
}

[[nodiscard]] constexpr EdgePanDirection edge_pan_direction(
    const std::int32_t pointer_x,
    const std::int32_t pointer_y,
    const std::int32_t viewport_left,
    const std::int32_t viewport_top,
    const std::int32_t viewport_width,
    const std::int32_t viewport_height,
    const std::int32_t threshold = 28) noexcept {
    if (viewport_width <= 0 || viewport_height <= 0 || threshold <= 0) return {};
    const auto right = viewport_left + viewport_width;
    const auto bottom = viewport_top + viewport_height;
    const auto clamped_threshold = (std::min)(
        threshold, (std::min)(viewport_width / 2, viewport_height / 2));
    return {
        pointer_x <= viewport_left + clamped_threshold ? -1
            : (pointer_x >= right - clamped_threshold ? 1 : 0),
        pointer_y <= viewport_top + clamped_threshold ? -1
            : (pointer_y >= bottom - clamped_threshold ? 1 : 0),
    };
}

} // namespace sandhybrid
