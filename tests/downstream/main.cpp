#include <epochsimengine/library.hpp>
#include <epochsimengine/world_save.hpp>
namespace consumer = epochsimengine;

static_assert(consumer::library_api_version == 4u);
static_assert(consumer::library_name == "EpochSimEngine");
static_assert(!consumer::core_library_capabilities.vulkan_required);
static_assert(!consumer::core_library_capabilities.windowing_required);
static_assert(consumer::world_save_format_version == 2u);
static_assert(sizeof(consumer::SceneCell) == 16u);

int main() {
    const auto schedule = consumer::make_section_schedule(
        consumer::SectionCoordinate{0, 0}, 2u, 2u, 4u);
    if (schedule.assignment_count != 4u) return 1;
    // Invoke a second out-of-line library symbol through each public namespace.
    return consumer::scene_image_path("scenes", consumer::Scene::sandbox).empty() ? 2 : 0;
}
