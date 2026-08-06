#pragma once

#include "sandhybrid/world_save.hpp"

#include <string>

namespace sandhybrid {

struct ApplicationOptions final {
    WorldSizePreset world_size{WorldSizePreset::large};
    std::string save_slot{"quick"};
    std::string runtime_acceptance_report{};
};

int run_application(const ApplicationOptions& options);

} // namespace sandhybrid
