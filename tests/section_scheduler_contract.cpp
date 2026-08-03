#include "sandhybrid/section_scheduler.hpp"

#include <array>
#include <cstddef>

int main() {
    using namespace sandhybrid;

    static_assert(active_region_width_cells == 640);
    static_assert(active_region_height_cells == 360);
    static_assert(active_window_columns == 4);
    static_assert(active_window_rows == 4);
    static_assert(active_window_section_capacity == 16u);

    constexpr SectionCoordinate center{5, 2};
    static_assert(active_window_origin(center, 16u, 4u) == SectionCoordinate{4, 0});
    static_assert(section_in_active_window({4, 0}, center, 16u, 4u));
    static_assert(section_in_active_window({7, 3}, center, 16u, 4u));
    static_assert(!section_in_active_window({3, 0}, center, 16u, 4u));
    static_assert(!section_in_active_window({8, 3}, center, 16u, 4u));

    const auto full = make_section_schedule(center, 16u, 4u, 24u);
    if (full.origin != SectionCoordinate{4, 0} ||
        full.assignment_count != 16u || full.worker_count != 16u) return 1;
    std::array<bool, 16> seen{};
    for (const auto assignment : full.active()) {
        if (assignment.worker >= 16u || seen[assignment.worker] ||
            !section_in_active_window(assignment.coordinate, center, 16u, 4u)) return 2;
        seen[assignment.worker] = true;
    }

    const auto constrained = make_section_schedule(center, 16u, 4u, 8u);
    if (constrained.assignment_count != 16u || constrained.worker_count != 7u) return 3;
    for (std::size_t index = 0; index < constrained.assignment_count; ++index)
        if (constrained.assignments[index].worker >= constrained.worker_count) return 4;

    const auto left_edge = make_section_schedule({0, 2}, 16u, 4u, 24u);
    if (left_edge.origin != SectionCoordinate{0, 0} || left_edge.assignment_count != 16u) return 5;
    const auto right_edge = make_section_schedule({15, 2}, 16u, 4u, 24u);
    if (right_edge.origin != SectionCoordinate{12, 0} || right_edge.assignment_count != 16u) return 6;

    const auto tiny = make_section_schedule({0, 0}, 2u, 2u, 24u);
    if (tiny.origin != SectionCoordinate{0, 0} ||
        tiny.assignment_count != 4u || tiny.worker_count != 4u) return 7;

    const auto main_only = make_section_schedule(center, 16u, 4u, 1u);
    if (main_only.assignment_count != 16u || main_only.worker_count != 0u) return 8;
    return 0;
}
