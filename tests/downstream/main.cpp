#include <sandhybrid/library.hpp>

int main() {
    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{0, 0}, 2u, 2u, 4u);
    return schedule.assignment_count == 4u && sandhybrid::library_api_version == 1u ? 0 : 1;
}
