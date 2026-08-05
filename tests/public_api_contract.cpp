#include <sandhybrid/library.hpp>

static_assert(sandhybrid::library_api_version == 4u);
static_assert(sandhybrid::library_name == "SandHybrid");
static_assert(sandhybrid::core_library_capabilities.native_startup_owned_by_consumer);
static_assert(!sandhybrid::core_library_capabilities.windowing_required);
static_assert(!sandhybrid::core_library_capabilities.vulkan_required);
static_assert(sandhybrid::core_library_capabilities.packed_atmosphere_available);
static_assert(sandhybrid::core_library_capabilities.transactional_packets_available);
static_assert(sandhybrid::core_library_capabilities.actor_medium_contracts_available);
static_assert(sandhybrid::core_library_capabilities.machinery_transactions_available);
static_assert(sandhybrid::core_library_capabilities.deterministic_gas_transport_available);
static_assert(sandhybrid::core_library_capabilities.ecology_actor_policies_available);

int main() {
    const auto schedule = sandhybrid::make_section_schedule(
        sandhybrid::SectionCoordinate{1, 1}, 4u, 4u, 12u);
    if (schedule.assignment_count == 0u || schedule.worker_count == 0u) return 1;

    const auto path =
        sandhybrid::scene_image_path("scenes", sandhybrid::Scene::sandbox);
    if (path.empty()) return 2;

    const auto atmosphere = sandhybrid::make_earth_atmosphere();
    if (!atmosphere.valid() ||
        atmosphere.pressure_units() != sandhybrid::atmosphere_capacity) return 3;

    sandhybrid::MaterialInventory inventory{};
    inventory.capacity = 1u;
    if (!inventory.add(sandhybrid::Material::water, 1u)) return 4;

    constexpr sandhybrid::InsectHabitatPolicy habitat{};
    static_assert(habitat.valid());
    return 0;
}
