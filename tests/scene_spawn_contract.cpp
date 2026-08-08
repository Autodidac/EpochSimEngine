#include "sandhybrid/scene_spawn.hpp"

using namespace sandhybrid;

static_assert(authored_scene_origin_x(resident_world_width) == 1280u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(scene_world_spawn(Scene::gold_mine, resident_world_width, resident_world_height) ==
              SceneSpawn{1358, 847, 18u, true});
static_assert(scene_world_spawn(Scene::demolition, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1047, 12u, true});
static_assert(scene_world_spawn(Scene::frontier_base, resident_world_width, resident_world_height) ==
              SceneSpawn{1448, 927, 24u, true});

int main() { return 0; }
