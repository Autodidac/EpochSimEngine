#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace epoch::sand {

inline constexpr std::int32_t active_region_width_cells = 640;
inline constexpr std::int32_t active_region_height_cells = 360;
inline constexpr std::size_t starburst_section_capacity = 17;

struct SectionCoordinate final {
    std::int32_t x{};
    std::int32_t y{};

    friend constexpr bool operator==(SectionCoordinate, SectionCoordinate) noexcept = default;
};

struct SectionAssignment final {
    SectionCoordinate coordinate{};
    std::uint8_t ring{};
    std::uint8_t worker{};
};

struct SectionSchedule final {
    std::array<SectionAssignment, starburst_section_capacity> assignments{};
    std::size_t assignment_count{};
    std::size_t worker_count{};
    std::uint32_t hardware_threads{};

    [[nodiscard]] constexpr std::span<const SectionAssignment> active() const noexcept {
        return {assignments.data(), assignment_count};
    }
};

[[nodiscard]] constexpr bool section_in_starburst(
    const SectionCoordinate candidate, const SectionCoordinate center) noexcept {
    const auto dx = candidate.x - center.x;
    const auto dy = candidate.y - center.y;
    const auto ax = dx < 0 ? -dx : dx;
    const auto ay = dy < 0 ? -dy : dy;
    const auto radius = ax > ay ? ax : ay;
    if (radius == 0) return true;
    if (radius == 1) return true;
    if (radius != 2) return false;
    return dx == 0 || dy == 0 || ax == ay;
}

[[nodiscard]] SectionSchedule make_section_schedule(
    SectionCoordinate center,
    std::uint32_t section_columns,
    std::uint32_t section_rows,
    std::uint32_t hardware_threads) noexcept;

} // namespace epoch::sand
