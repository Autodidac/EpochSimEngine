#include "sandhybrid/section_scheduler.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace sandhybrid {

SectionSchedule make_section_schedule(
    const SectionCoordinate center,
    const std::uint32_t section_columns,
    const std::uint32_t section_rows,
    const std::uint32_t hardware_threads) noexcept {
    SectionSchedule result{};
    result.hardware_threads = hardware_threads;
    result.origin = active_window_origin(center, section_columns, section_rows);

    // Nearest map-footprint regions are listed first, but the represented set is
    // always one contiguous clipped 4x4 rectangle. This is cheaper and more
    // predictable than the old sparse two-ring starburst.
    for (std::uint8_t priority = 0u; priority <= 3u; ++priority) {
        for (std::int32_t y = 0; y < active_window_rows; ++y) {
            for (std::int32_t x = 0; x < active_window_columns; ++x) {
                const SectionCoordinate candidate{result.origin.x + x, result.origin.y + y};
                if (candidate.x < 0 || candidate.y < 0 ||
                    candidate.x >= static_cast<std::int32_t>(section_columns) ||
                    candidate.y >= static_cast<std::int32_t>(section_rows)) {
                    continue;
                }
                const auto dx = candidate.x - center.x;
                const auto dy = candidate.y - center.y;
                const auto ax = dx < 0 ? -dx : dx;
                const auto ay = dy < 0 ? -dy : dy;
                const auto candidate_priority = static_cast<std::uint8_t>((std::max)(ax, ay));
                if (candidate_priority != priority) continue;
                result.assignments[result.assignment_count++] = SectionAssignment{
                    .coordinate = candidate,
                    .priority = priority,
                    .worker = std::numeric_limits<std::uint8_t>::max(),
                };
            }
        }
    }

    const auto available_workers = hardware_threads > 1u ? hardware_threads - 1u : 0u;
    result.worker_count = (std::min)(
        static_cast<std::size_t>(available_workers), result.assignment_count);
    if (result.worker_count == 0u) return result;

    for (std::size_t index = 0; index < result.worker_count; ++index)
        result.assignments[index].worker = static_cast<std::uint8_t>(index);

    std::array<std::uint8_t, active_window_section_capacity> worker_load{};
    for (std::size_t index = 0; index < result.worker_count; ++index) worker_load[index] = 1u;

    // Attach the farthest remaining jobs first, balancing deterministic load
    // across the dedicated simulation workers. Thread zero remains outside this
    // assignment model for windowing, input, and render coordination.
    for (std::size_t reverse = result.assignment_count; reverse > result.worker_count; --reverse) {
        const auto assignment_index = reverse - 1u;
        std::size_t best_worker = 0u;
        for (std::size_t worker = 1u; worker < result.worker_count; ++worker)
            if (worker_load[worker] < worker_load[best_worker]) best_worker = worker;
        result.assignments[assignment_index].worker = static_cast<std::uint8_t>(best_worker);
        ++worker_load[best_worker];
    }
    return result;
}

} // namespace sandhybrid
