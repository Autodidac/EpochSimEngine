#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sandhybrid {

inline constexpr std::int32_t active_region_width_cells = 640;
inline constexpr std::int32_t active_region_height_cells = 360;
inline constexpr std::int32_t active_window_columns = 4;
inline constexpr std::int32_t active_window_rows = 4;
inline constexpr std::size_t active_window_section_capacity =
    static_cast<std::size_t>(active_window_columns * active_window_rows);

struct SectionCoordinate final {
    std::int32_t x{};
    std::int32_t y{};

    friend constexpr bool operator==(SectionCoordinate, SectionCoordinate) noexcept = default;
};

struct SectionAssignment final {
    SectionCoordinate coordinate{};
    std::uint8_t priority{};
    std::uint8_t worker{};
};

struct SectionSchedule final {
    std::array<SectionAssignment, active_window_section_capacity> assignments{};
    SectionCoordinate origin{};
    std::size_t assignment_count{};
    std::size_t worker_count{};
    std::uint32_t hardware_threads{};

    [[nodiscard]] constexpr std::span<const SectionAssignment> active() const noexcept {
        return {assignments.data(), assignment_count};
    }
};

[[nodiscard]] constexpr SectionCoordinate active_window_origin(
    const SectionCoordinate center,
    const std::uint32_t section_columns,
    const std::uint32_t section_rows) noexcept {
    const auto max_x = section_columns > static_cast<std::uint32_t>(active_window_columns)
        ? static_cast<std::int32_t>(section_columns) - active_window_columns : 0;
    const auto max_y = section_rows > static_cast<std::uint32_t>(active_window_rows)
        ? static_cast<std::int32_t>(section_rows) - active_window_rows : 0;
    const auto desired_x = center.x - 1;
    const auto desired_y = center.y - 1;
    return {
        desired_x < 0 ? 0 : (desired_x > max_x ? max_x : desired_x),
        desired_y < 0 ? 0 : (desired_y > max_y ? max_y : desired_y),
    };
}

[[nodiscard]] constexpr bool section_in_active_window(
    const SectionCoordinate candidate,
    const SectionCoordinate center,
    const std::uint32_t section_columns,
    const std::uint32_t section_rows) noexcept {
    const auto origin = active_window_origin(center, section_columns, section_rows);
    return candidate.x >= origin.x && candidate.y >= origin.y &&
           candidate.x < origin.x + active_window_columns &&
           candidate.y < origin.y + active_window_rows &&
           candidate.x < static_cast<std::int32_t>(section_columns) &&
           candidate.y < static_cast<std::int32_t>(section_rows);
}

[[nodiscard]] SectionSchedule make_section_schedule(
    SectionCoordinate center,
    std::uint32_t section_columns,
    std::uint32_t section_rows,
    std::uint32_t hardware_threads) noexcept;

} // namespace sandhybrid
