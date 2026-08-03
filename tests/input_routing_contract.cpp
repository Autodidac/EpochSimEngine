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
static_assert(edge_pan_direction(0, 100, 0, 0, 640, 360) == EdgePanDirection{-1, 0});
static_assert(edge_pan_direction(639, 359, 0, 0, 640, 360) == EdgePanDirection{1, 1});
static_assert(edge_pan_direction(320, 180, 0, 0, 640, 360) == EdgePanDirection{});

int main() { return 0; }
