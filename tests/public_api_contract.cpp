#include <sandhybrid/library.hpp>

static_assert(sandhybrid::library_api_version == 1u);
static_assert(sandhybrid::library_name == "SandHybrid");
static_assert(sandhybrid::core_library_capabilities.native_startup_owned_by_consumer);
static_assert(!sandhybrid::core_library_capabilities.windowing_required);
static_assert(!sandhybrid::core_library_capabilities.vulkan_required);

int main() {
    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{1, 1}, 4u, 4u, 12u);
    if (schedule.assignment_count == 0u || schedule.worker_count == 0u) return 1;

    const auto path = sandhybrid::scene_image_path("scenes", sandhybrid::Scene::sandbox);
    if (path.empty()) return 2;
    return 0;
}
