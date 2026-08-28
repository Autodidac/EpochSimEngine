#include "sandhybrid/scene_spawn.hpp"
#include "sandhybrid/section_scheduler.hpp"

using namespace sandhybrid;

static_assert(player_body_height_cells == 23);
static_assert(player_body_height_cells < 3 * static_cast<std::int32_t>(authored_scene_foundation_cells));
static_assert(player_top_offset_cells == -22);
static_assert(player_half_width_cells == 4);
static_assert(player_head_center_offset_cells >= player_top_offset_cells);
static_assert(player_tool_origin_offset_cells > player_top_offset_cells);
static_assert(authored_scene_origin_x(resident_world_width) == 1280u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(scene_world_spawn(Scene::sandbox, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1039, 12u, true});
static_assert(scene_world_spawn(Scene::blank, resident_world_width, resident_world_height) ==
              SceneSpawn{1360, 1039, 12u, true});
static_assert(scene_world_spawn(Scene::volcano, resident_world_width, resident_world_height) ==
              SceneSpawn{1672, 895, 12u, true});
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
static_assert(persistent_world_spawn(resident_world_width, resident_world_height) ==
              SceneSpawn{4272, 1111, 24u, true});

constexpr std::uint32_t large_startup_district_overlap_count() {
    constexpr auto spawn =
        persistent_world_spawn(resident_world_width, resident_world_height);
    constexpr SectionCoordinate center{
        spawn.x / active_region_width_cells,
        spawn.y / active_region_height_cells,
    };
    constexpr auto origin = active_window_origin(
        center, resident_world_footprint_columns, resident_world_footprint_rows);
    constexpr auto active_left =
        static_cast<std::uint32_t>(origin.x * active_region_width_cells);
    constexpr auto active_right = active_left +
        static_cast<std::uint32_t>(active_window_columns * active_region_width_cells);
    std::uint32_t overlaps = 0u;
    for (std::uint32_t district = 0u;
         district < persistent_world_district_count; ++district) {
        const auto left =
            persistent_world_district_origin_x(resident_world_width, district);
        const auto right = left + pre_expansion_world_width;
        overlaps += left < active_right && right > active_left ? 1u : 0u;
    }
    return overlaps;
}

static_assert(large_startup_district_overlap_count() == 3u);

int main() {
    if (!scene_has_character(world_scene) ||
        !persistent_world_spawn(resident_world_width, resident_world_height).enabled)
        return 1;
    for (std::uint32_t index = 0u; index < legacy_scene_count; ++index) {
        const auto scene = static_cast<Scene>(index);
        if (!scene_world_spawn(scene, resident_world_width, resident_world_height).enabled)
            return 2;
    }
    return 0;
}