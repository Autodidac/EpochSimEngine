#pragma once

#include "sandhybrid/world_save.hpp"

#include <string>

namespace sandhybrid {

struct ApplicationOptions final {
    WorldSizePreset world_size{WorldSizePreset::large};
    std::string save_slot{"quick"};
    std::string runtime_acceptance_report{};
    std::string long_cycle_acceptance_report{};
    std::string interactive_acceptance_report{};
    std::string simulation_profile_report{};
    bool simulation_profile_stage_trace{false};
};

int run_application(const ApplicationOptions& options);

} // namespace sandhybrid
