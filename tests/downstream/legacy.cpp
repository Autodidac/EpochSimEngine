// A standalone old-only consumer: no canonical include or namespace is needed.
#include <sandhybrid/library.hpp>
#include <sandhybrid/world_save.hpp>

static_assert(sandhybrid::library_api_version == 4u);
static_assert(sandhybrid::library_name == "EpochSimEngine");
static_assert(!sandhybrid::core_library_capabilities.vulkan_required);
static_assert(sandhybrid::world_save_format_version == 2u);
static_assert(sizeof(sandhybrid::SceneCell) == 16u);

int main() {
    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{0, 0}, 2u, 2u, 4u);
    if (schedule.assignment_count != 4u) return 1;
    return sandhybrid::scene_image_path("scenes", sandhybrid::Scene::sandbox).empty() ? 2 : 0;
}
