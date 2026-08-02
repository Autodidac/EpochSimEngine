#include "sandhybrid/world_layout.hpp"

using namespace sandhybrid;

static_assert(authored_scene_origin_x(resident_world_width) == 960u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(authored_scene_origin_y(pre_expansion_world_height) == 0u);
static_assert(authored_scene_sky_footprint_rows == 2u);
static_assert(subterranean_zone_count == 3u);
static_assert(authored_scene_foundation_cells == 8u);
static_assert(resident_world_lava_cells == 16u);

static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 0u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 0u, 719u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 720u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 0u, 900u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1072u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1080u) == Material::sand);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1200u) == Material::dirt);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1330u) == Material::silt);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1408u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1416u) == Material::lava);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);

static_assert(resident_substrate_is_structural(Material::stone));
static_assert(resident_substrate_is_structural(Material::dirt));
static_assert(resident_substrate_is_structural(Material::sand));
static_assert(resident_substrate_is_structural(Material::silt));
static_assert(!resident_substrate_is_structural(Material::mud));
static_assert(!resident_substrate_is_structural(Material::lava));

int main() { return 0; }
