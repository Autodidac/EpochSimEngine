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
    std::size_t loose_trap_resources = 0u;
    std::size_t trap_cells = 0u;
    std::size_t loose_trap_roof_cells = 0u;

    for (std::uint32_t tile_y = 0u; tile_y < 64u; ++tile_y) {
        for (std::uint32_t tile_x = 0u; tile_x < 192u; ++tile_x) {
            std::size_t tile_structural_deposit = 0u;
            for (std::uint32_t local_y = 0u; local_y < terrain::tile_size; ++local_y) {
                for (std::uint32_t local_x = 0u; local_x < terrain::tile_size; ++local_x) {
                    const auto x = tile_x * terrain::tile_size + local_x;
                    const auto y = tile_y * terrain::tile_size + local_y;
                    const auto sample = terrain::sample(Material::sand, x, y, 120u + y);
                    const bool deposit = sample.material == Material::iron_ore ||
                        sample.material == Material::copper || sample.material == Material::aluminum ||
                        sample.material == Material::uranium;
                    if (deposit && sample.structural) ++tile_structural_deposit;
                    if (deposit && sample.deliberate_loose) {
                        if (!sample.sand_trap) return 3;
                        ++loose_trap_resources;
                    }
                    if (sample.sand_trap) {
                        ++trap_cells;
                        if (sample.material == Material::sand && sample.deliberate_loose)
                            ++loose_trap_roof_cells;
                    }
                }
            }
            if (tile_structural_deposit != 0u) {
                structural_cells += tile_structural_deposit;
                if (tile_structural_deposit != 64u) ++structural_partial_tiles;
            }
        }
    }
    if (structural_cells == 0u) return 1;
    if (structural_partial_tiles != 0u) return 2;
    if (loose_trap_resources == 0u) return 3;
    if (trap_cells == 0u || loose_trap_roof_cells == 0u) return 4;
    return 0;
}
