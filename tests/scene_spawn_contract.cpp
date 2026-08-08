#include "sandhybrid/scene_spawn.hpp"

using namespace sandhybrid;

static_assert(authored_scene_origin_x(resident_world_width) == 1280u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(scene_world_spawn(Scene::sandbox, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1039, 12u, true});
static_assert(scene_world_spawn(Scene::blank, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1039, 12u, true});
static_assert(scene_world_spawn(Scene::volcano, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1063, 12u, true});
static_assert(scene_world_spawn(Scene::waterworks, resident_world_width, resident_world_height) ==
              SceneSpawn{1320, 1031, 12u, true});
static_assert(scene_world_spawn(Scene::ecosystem, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1015, 12u, true});
static_assert(scene_world_spawn(Scene::engineering_lab, resident_world_width, resident_world_height) ==
              SceneSpawn{1480, 1055, 18u, true});
static_assert(scene_world_spawn(Scene::gold_mine, resident_world_width, resident_world_height) ==
              SceneSpawn{1358, 847, 18u, true});
static_assert(scene_world_spawn(Scene::demolition, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1047, 12u, true});
static_assert(scene_world_spawn(Scene::frontier_base, resident_world_width, resident_world_height) ==
              SceneSpawn{1448, 927, 24u, true});
static_assert(!scene_world_spawn(Scene::count, resident_world_width, resident_world_height).enabled);

int main() {
    for (std::uint32_t index = 0u; index < scene_count; ++index) {
        const auto scene = static_cast<Scene>(index);
        if (!scene_has_character(scene) ||
            !scene_world_spawn(scene, resident_world_width, resident_world_height).enabled)
            return 1;
    }
    return 0;
}