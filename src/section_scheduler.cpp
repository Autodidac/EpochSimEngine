#include "sandhybrid/section_scheduler.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace sandhybrid {

namespace {

struct Offset final {
    std::int32_t x;
    std::int32_t y;
    std::uint8_t ring;
};

inline constexpr std::array<Offset, starburst_section_capacity> starburst_offsets{{
    { 0,  0, 0},
    { 0, -1, 1}, { 1, -1, 1}, { 1,  0, 1}, { 1,  1, 1},
    { 0,  1, 1}, {-1,  1, 1}, {-1,  0, 1}, {-1, -1, 1},
    { 0, -2, 2}, { 2, -2, 2}, { 2,  0, 2}, { 2,  2, 2},
    { 0,  2, 2}, {-2,  2, 2}, {-2,  0, 2}, {-2, -2, 2},
}};

} // namespace

SectionSchedule make_section_schedule(
    const SectionCoordinate center,
    const std::uint32_t section_columns,
    const std::uint32_t section_rows,
    const std::uint32_t hardware_threads) noexcept {
    SectionSchedule result{};
    result.hardware_threads = hardware_threads;

    for (const auto offset : starburst_offsets) {
        const SectionCoordinate candidate{center.x + offset.x, center.y + offset.y};
        if (candidate.x < 0 || candidate.y < 0 ||
            candidate.x >= static_cast<std::int32_t>(section_columns) ||
            candidate.y >= static_cast<std::int32_t>(section_rows)) {
            continue;
        }
        result.assignments[result.assignment_count++] = SectionAssignment{
            .coordinate = candidate,
            .ring = offset.ring,
            .worker = std::numeric_limits<std::uint8_t>::max(),
        };
    }

    // Thread zero is permanently reserved for windowing, input, render
    // coordination, and all non-section work. Unknown/one-thread systems have
    // no legal simulation worker and therefore leave the starburst paused.
    const auto available_workers = hardware_threads > 1u ? hardware_threads - 1u : 0u;
    result.worker_count = (std::min)(
        static_cast<std::size_t>(available_workers), result.assignment_count);
    if (result.worker_count == 0u) return result;

    // Nearest work receives dedicated workers first. Missing workers are then
    // compensated by attaching the furthest unassigned jobs first, balancing
    // deterministically across the existing workers. On a 24-core machine this
    // yields exactly 17 workers plus the untouched main thread.
    for (std::size_t index = 0; index < result.worker_count; ++index) {
        result.assignments[index].worker = static_cast<std::uint8_t>(index);
    }

    std::array<std::uint8_t, starburst_section_capacity> worker_load{};
    for (std::size_t index = 0; index < result.worker_count; ++index) worker_load[index] = 1u;

    for (std::size_t reverse = result.assignment_count; reverse > result.worker_count; --reverse) {
        const auto assignment_index = reverse - 1u;
        std::size_t best_worker = 0u;
        for (std::size_t worker = 1u; worker < result.worker_count; ++worker) {
            if (worker_load[worker] < worker_load[best_worker]) best_worker = worker;
        }
        result.assignments[assignment_index].worker = static_cast<std::uint8_t>(best_worker);
        ++worker_load[best_worker];
    }
    return result;
}

} // namespace sandhybrid
