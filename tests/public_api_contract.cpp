#include <sandhybrid/library.hpp>

#include <cstdint>

int main() {
    if (sandhybrid::library_api_version != 1u) return 1;
    if (sandhybrid::library_name != "SandHybrid") return 2;
    if (!sandhybrid::core_library_capabilities.native_startup_owned_by_consumer) return 3;
    if (sandhybrid::core_library_capabilities.windowing_required) return 4;
    if (sandhybrid::core_library_capabilities.vulkan_required) return 5;

    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{1, 1}, 4u, 4u, 12u);
    if (schedule.assignment_count == 0u || schedule.worker_count == 0u) return 6;

    const auto path = sandhybrid::scene_image_path("scenes", sandhybrid::Scene::sandbox);
    if (path.empty()) return 7;
    return 0;
}
