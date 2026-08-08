#include "sandhybrid/world_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using namespace sandhybrid;

static_assert(resident_world_width == 10240u);
static_assert(resident_world_height == 1440u);
static_assert(resident_world_footprint_columns == 16u);
static_assert(resident_world_footprint_rows == 4u);
static_assert(authored_scene_origin_x(resident_world_width) == 1280u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(authored_scene_sky_footprint_rows == 2u);
static_assert(authored_scene_origin_x(resident_world_width) % pre_expansion_world_width == 0u);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 0u, 0u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 100u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1039u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1040u) == Material::grass);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1041u) == Material::dirt);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1071u) == Material::dirt);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1079u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 5000u, 1068u) != Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 3000u, 1068u) != Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1500u, 1068u) != Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 0u) == Material::stone);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1416u) == Material::lava);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);
static_assert(resident_substrate_is_structural(Material::grass));
static_assert(resident_substrate_is_structural(Material::mud));
static_assert(resident_substrate_is_structural(Material::iron_ore));
static_assert(resident_substrate_is_structural(Material::copper));
static_assert(resident_substrate_is_structural(Material::aluminum));
static_assert(resident_substrate_is_structural(Material::uranium));


static_assert(world_dimensions(WorldSizePreset::compact).footprint_columns == 4u);
static_assert(world_dimensions(WorldSizePreset::standard).footprint_columns == 8u);
static_assert(world_dimensions(WorldSizePreset::large).footprint_columns == 16u);
static_assert(world_dimensions(WorldSizePreset::compact).height == resident_world_height);

int main() {
    constexpr auto scene_bottom = 1080u;
    constexpr auto geology_end = 1408u;
    std::array<std::size_t, 4> deposits{};
    std::array<std::uint64_t, 2> depth_totals{};
    std::array<std::size_t, 2> depth_counts{};
    std::size_t gold = 0u;
    std::size_t deposit_cells = 0u;
    std::size_t adjacent_deposit_cells = 0u;
    std::array<bool, resident_world_width / 8u> deposit_tiles{};
    std::array<bool, resident_world_width / 8u> mixed_tiles{};
    std::size_t varied_boundaries = 0u;
    int previous_transition = -1;

    for (std::uint32_t x = resident_world_shell_cells;
         x < resident_world_width - resident_world_shell_cells; ++x) {
        int transition = -1;
        for (std::uint32_t y = scene_bottom; y < scene_bottom + 180u; ++y) {
            const auto material = resident_substrate_material(
                resident_world_width, resident_world_height, x, y);
            if (material == Material::dirt) {
                depth_totals[0] += y - scene_bottom;
                ++depth_counts[0];
            } else if (material == Material::sand) {
                depth_totals[1] += y - scene_bottom;
                ++depth_counts[1];
                if (transition < 0) transition = static_cast<int>(y - scene_bottom);
            }
        }
        if (previous_transition >= 0 && transition >= 0 && transition != previous_transition)
            ++varied_boundaries;
        if (transition >= 0) previous_transition = transition;
    }

    for (std::uint32_t y = scene_bottom; y < geology_end; ++y) {
        for (std::uint32_t x = resident_world_shell_cells;
             x < resident_world_width - resident_world_shell_cells; ++x) {
            const auto material = resident_substrate_material(
                resident_world_width, resident_world_height, x, y);
            std::size_t deposit_index = 4u;
            switch (material) {
                case Material::iron_ore: deposit_index = 0u; break;
                case Material::copper: deposit_index = 1u; break;
                case Material::aluminum: deposit_index = 2u; break;
                case Material::uranium: deposit_index = 3u; break;
                case Material::gold: ++gold; break;
                default: break;
            }
            if (deposit_index == 4u) continue;
            const auto sample = resident_substrate_sample(
                resident_world_width, resident_world_height, x, y);
            if (sample.structural && (x % 8u != 0u || y % 8u != 0u)) {
                const auto tile_origin = resident_substrate_sample(
                    resident_world_width, resident_world_height, x - x % 8u, y - y % 8u);
                if (tile_origin.material != material || !tile_origin.structural) return 7;
            }
            ++deposits[deposit_index];
            ++deposit_cells;
            const auto tile_x = x / 8u;
            deposit_tiles[tile_x] = true;
            const auto right = resident_substrate_material(
                resident_world_width, resident_world_height, x + 1u, y);
            const auto down = resident_substrate_material(
                resident_world_width, resident_world_height, x, y + 1u);
            if (right == material || down == material) ++adjacent_deposit_cells;
            if (right != material || down != material) mixed_tiles[tile_x] = true;
        }
    }

    for (const auto count : deposits) if (count == 0u) return 1;
    if (gold != 0u) return 2;
    if (deposits[0] <= deposits[1] || deposits[1] <= deposits[2] ||
        deposits[2] <= deposits[3]) return 3;
    if (depth_counts[0] == 0u || depth_counts[1] == 0u ||
        depth_totals[0] / depth_counts[0] >= depth_totals[1] / depth_counts[1]) return 4;
    if (varied_boundaries < 100u) return 5;
    if (deposit_cells == 0u || adjacent_deposit_cells * 4u < deposit_cells * 3u) return 6;
    std::size_t loose_resource_cells = 0u;
    std::size_t trap_cells = 0u;
    for (std::uint32_t y = scene_bottom; y < geology_end; ++y) {
        for (std::uint32_t x = resident_world_shell_cells;
             x < resident_world_width - resident_world_shell_cells; ++x) {
            const auto sample = resident_substrate_sample(
                resident_world_width, resident_world_height, x, y);
            if (sample.deliberate_loose && sample.material != Material::sand) ++loose_resource_cells;
            if (sample.sand_trap) ++trap_cells;
        }
    }
    if (loose_resource_cells == 0u) return 8;
    if (trap_cells == 0u) return 9;
    return 0;
}
