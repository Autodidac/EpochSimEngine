#include "sandhybrid/world_layout.hpp"

#include <array>
#include <cstddef>

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
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1408u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1416u) == Material::lava);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);

int main() {
    std::array<std::size_t, 4> deposits{};
    std::size_t gold = 0u;
    constexpr auto scene_bottom = 1080u;
    constexpr auto geology_end = 1408u;
    for (std::uint32_t y = scene_bottom; y < geology_end; ++y) {
        for (std::uint32_t x = resident_world_shell_cells;
   x < resident_world_width - resident_world_shell_cells; ++x) {
  switch (resident_substrate_material(resident_world_width, resident_world_height, x, y)) {
  case Material::iron_ore: ++deposits[0]; break;
  case Material::copper: ++deposits[1]; break;
  case Material::aluminum: ++deposits[2]; break;
  case Material::uranium: ++deposits[3]; break;
  case Material::gold: ++gold; break;
  default: break;
  }
        }
    }
    for (const auto count : deposits) {
        if (count == 0u) return 1;
    }
    if (gold != 0u) return 2;
    if (deposits[0] <= deposits[1] || deposits[1] <= deposits[2] ||
        deposits[2] <= deposits[3]) return 3;
    return 0;
}
