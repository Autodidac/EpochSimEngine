#include <sandhybrid/library.hpp>

static_assert(sandhybrid::library_api_version == 4u);

int main() {
    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{0, 0}, 2u, 2u, 4u);
    return schedule.assignment_count == 4u ? 0 : 1;
}
