#pragma once

#include <algorithm>
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

struct ActiveCellDispatch final {
    std::uint32_t origin_x{};
    std::uint32_t origin_y{};
    std::uint32_t width{};
    std::uint32_t height{};

    friend constexpr bool operator==(
        ActiveCellDispatch, ActiveCellDispatch) noexcept = default;

    [[nodiscard]] constexpr std::uint64_t cell_count() const noexcept {
        return static_cast<std::uint64_t>(width) * height;
    }
};

[[nodiscard]] constexpr ActiveCellDispatch active_cell_dispatch(
    const std::uint32_t grid_width,
    const std::uint32_t grid_height,
    const SectionCoordinate origin) noexcept {
    const auto requested_x = origin.x > 0
        ? static_cast<std::uint64_t>(origin.x) * active_region_width_cells : 0u;
    const auto requested_y = origin.y > 0
        ? static_cast<std::uint64_t>(origin.y) * active_region_height_cells : 0u;
    const auto origin_x = static_cast<std::uint32_t>(
        requested_x < grid_width ? requested_x : grid_width);
    const auto origin_y = static_cast<std::uint32_t>(
        requested_y < grid_height ? requested_y : grid_height);
    const auto remaining_width = grid_width - origin_x;
    const auto remaining_height = grid_height - origin_y;
    constexpr auto window_width = static_cast<std::uint32_t>(
        active_window_columns * active_region_width_cells);
    constexpr auto window_height = static_cast<std::uint32_t>(
        active_window_rows * active_region_height_cells);
    return {
        origin_x,
        origin_y,
        remaining_width < window_width ? remaining_width : window_width,
        remaining_height < window_height ? remaining_height : window_height,
    };
}

[[nodiscard]] constexpr ActiveCellDispatch expanded_cell_dispatch(
    const ActiveCellDispatch dispatch,
    const std::uint32_t grid_width,
    const std::uint32_t grid_height,
    const std::uint32_t halo) noexcept {
    const auto origin_x = dispatch.origin_x > halo ? dispatch.origin_x - halo : 0u;
    const auto origin_y = dispatch.origin_y > halo ? dispatch.origin_y - halo : 0u;
    const auto far_x = (std::min)(
        static_cast<std::uint64_t>(grid_width),
        static_cast<std::uint64_t>(dispatch.origin_x) + dispatch.width + halo);
    const auto far_y = (std::min)(
        static_cast<std::uint64_t>(grid_height),
        static_cast<std::uint64_t>(dispatch.origin_y) + dispatch.height + halo);
    return {
        origin_x,
        origin_y,
        static_cast<std::uint32_t>(far_x - origin_x),
        static_cast<std::uint32_t>(far_y - origin_y),
    };
}

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
