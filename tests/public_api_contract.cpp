#include <epochsimengine/actor_medium.hpp>
#include <epochsimengine/atmosphere.hpp>
#include <epochsimengine/blueprint.hpp>
#include <epochsimengine/camera_policy.hpp>
#include <epochsimengine/hive_recovery.hpp>
#include <epochsimengine/input_routing.hpp>
#include <epochsimengine/inventory.hpp>
#include <epochsimengine/library.hpp>
#include <epochsimengine/machinery.hpp>
#include <epochsimengine/material.hpp>
#include <epochsimengine/material_color.hpp>
#include <epochsimengine/packet_transaction.hpp>
#include <epochsimengine/scene.hpp>
#include <epochsimengine/scene_image.hpp>
#include <epochsimengine/scene_spawn.hpp>
#include <epochsimengine/section_grid.hpp>
#include <epochsimengine/section_scheduler.hpp>
#include <epochsimengine/simulation_policy.hpp>
#include <epochsimengine/terrain_generation.hpp>
#include <epochsimengine/world_layout.hpp>
#include <epochsimengine/world_save.hpp>

#include <sandhybrid/library.hpp>

#include <type_traits>

static_assert(sandhybrid::library_api_version == 4u);
static_assert(epochsimengine::library_api_version == 4u);
static_assert(epochsimengine::library_name == "EpochSimEngine");
static_assert(sandhybrid::library_name == epochsimengine::library_name);
static_assert(epochsimengine::legacy_library_name == "SandHybrid");
static_assert(std::is_same_v<epochsimengine::SceneCell, sandhybrid::SceneCell>);
static_assert(std::is_same_v<epochsimengine::Material, sandhybrid::Material>);
static_assert(std::is_same_v<epochsimengine::WorldSaveActorState, sandhybrid::WorldSaveActorState>);
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
    const auto schedule = epochsimengine::make_section_schedule(
        epochsimengine::SectionCoordinate{1, 1}, 4u, 4u, 12u);
    if (schedule.assignment_count == 0u || schedule.worker_count == 0u) return 1;

    const auto path =
        epochsimengine::scene_image_path("scenes", epochsimengine::Scene::sandbox);
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
