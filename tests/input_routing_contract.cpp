#include "sandhybrid/input_routing.hpp"

using namespace sandhybrid;

static_assert(player_wasd_enabled(true, false));
static_assert(!player_wasd_enabled(true, true));
static_assert(!player_wasd_enabled(false, false));
static_assert(camera_wasd_enabled(true, true));
static_assert(camera_wasd_enabled(false, false));
static_assert(!camera_wasd_enabled(true, false));
static_assert(route_directional_input(true, true, false, false, false) ==
              DirectionalInputRouting{0, 0, -1, 0});
static_assert(route_directional_input(false, true, false, false, false) ==
              DirectionalInputRouting{-1, 0, 0, 0});

int main() { return 0; }
