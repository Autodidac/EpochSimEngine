#pragma once

#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/atmosphere.hpp"
#include "sandhybrid/inventory.hpp"
#include "sandhybrid/machinery.hpp"
#include "sandhybrid/packet_transaction.hpp"
#include "sandhybrid/scene_image.hpp"
#include "sandhybrid/section_grid.hpp"
#include "sandhybrid/section_scheduler.hpp"
#include "sandhybrid/world_layout.hpp"

#include <cstdint>
#include <string_view>

namespace sandhybrid {

inline constexpr std::uint32_t library_api_version = 4u;
inline constexpr std::string_view library_name = "SandHybrid";

struct LibraryCapabilities final {
    bool native_startup_owned_by_consumer{true};
    bool windowing_required{false};
    bool vulkan_required{false};
    bool packed_atmosphere_available{true};
    bool transactional_packets_available{true};
    bool actor_medium_contracts_available{true};
    bool machinery_transactions_available{true};
    bool deterministic_gas_transport_available{true};
    bool ecology_actor_policies_available{true};
};

inline constexpr LibraryCapabilities core_library_capabilities{};

} // namespace sandhybrid
