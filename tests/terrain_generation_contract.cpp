#include "sandhybrid/terrain_generation.hpp"

#include <cstddef>
#include <cstdint>

using namespace sandhybrid;

static_assert(terrain::tile_size == 8u);
static_assert(terrain::host_material(Material::sand));
static_assert(!terrain::host_material(Material::water));

int main() {
    std::size_t structural_cells = 0u;
    std::size_t structural_partial_tiles = 0u;
    std::size_t loose_resources = 0u;
    std::size_t loose_host_cells = 0u;
    std::size_t trap_cells = 0u;
    std::size_t rubble_cells = 0u;
    std::size_t deposits[4]{};

    for (std::uint32_t tile_y = 0u; tile_y < 72u; ++tile_y) {
        for (std::uint32_t tile_x = 0u; tile_x < 220u; ++tile_x) {
  std::size_t tile_structural_deposit = 0u;
  Material tile_deposit = Material::empty;
  for (std::uint32_t local_y = 0u; local_y < terrain::tile_size; ++local_y) {
      for (std::uint32_t local_x = 0u; local_x < terrain::tile_size; ++local_x) {
          const auto x = tile_x * terrain::tile_size + local_x;
          const auto y = tile_y * terrain::tile_size + local_y;
          const auto value = terrain::sample(Material::sand, x, y, 120u + y);
          const bool deposit = value.material == Material::iron_ore ||
              value.material == Material::copper || value.material == Material::aluminum ||
              value.material == Material::uranium;
          if (deposit && value.structural) {
              ++tile_structural_deposit;
              tile_deposit = value.material;
          }
          if (deposit && value.deliberate_loose) ++loose_resources;
          if ((value.material == Material::sand || value.material == Material::silt) &&
              value.deliberate_loose) ++loose_host_cells;
          if (value.sand_trap) ++trap_cells;
          if (value.deliberate_loose && !value.sand_trap) ++rubble_cells;
      }
  }
  if (tile_structural_deposit != 0u) {
      structural_cells += tile_structural_deposit;
      if (tile_structural_deposit != 64u) ++structural_partial_tiles;
      if (tile_deposit == Material::iron_ore) ++deposits[0];
      else if (tile_deposit == Material::copper) ++deposits[1];
      else if (tile_deposit == Material::aluminum) ++deposits[2];
      else if (tile_deposit == Material::uranium) ++deposits[3];
  }
        }
    }
    if (structural_cells == 0u || structural_partial_tiles != 0u) return 1;
    for (const auto count : deposits) if (count == 0u) return 2;
    if (loose_resources == 0u || loose_host_cells == 0u) return 3;
    if (trap_cells == 0u || rubble_cells == 0u) return 4;
    if (rubble_cells * 3u > structural_cells) return 5;
    return 0;
}
