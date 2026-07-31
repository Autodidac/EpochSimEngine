#include "epoch/sand/section_scheduler.hpp"

#include <array>
#include <cstddef>

int main() {
    using namespace epoch::sand;

    constexpr SectionCoordinate center{5, 5};
    static_assert(section_in_starburst(center, center));
    static_assert(section_in_starburst({7, 5}, center));
    static_assert(section_in_starburst({7, 7}, center));
    static_assert(!section_in_starburst({6, 7}, center));
    static_assert(!section_in_starburst({8, 5}, center));

    const auto full = make_section_schedule(center, 12u, 12u, 24u);
    if (full.assignment_count != 17u || full.worker_count != 17u) return 1;
    std::array<bool, 17> seen{};
    for (const auto assignment : full.active()) {
        if (assignment.worker >= 17u || seen[assignment.worker] ||
            !section_in_starburst(assignment.coordinate, center)) return 2;
        seen[assignment.worker] = true;
    }

    const auto constrained = make_section_schedule(center, 12u, 12u, 8u);
    if (constrained.assignment_count != 17u || constrained.worker_count != 7u) return 3;
    for (std::size_t index = 0; index < constrained.assignment_count; ++index)
        if (constrained.assignments[index].worker >= constrained.worker_count) return 4;
    for (std::size_t index = 9u; index < constrained.assignment_count; ++index)
        if (constrained.assignments[index].ring != 2u) return 5;

    const auto corner = make_section_schedule({0, 0}, 4u, 4u, 24u);
    if (corner.assignment_count != 7u || corner.worker_count != 7u) return 6;

    const auto main_only = make_section_schedule(center, 12u, 12u, 1u);
    if (main_only.assignment_count != 17u || main_only.worker_count != 0u) return 7;
    return 0;
}
