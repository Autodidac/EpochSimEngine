#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"
#include "sandhybrid/scene_image.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t aux_bee_fed = 0x10000000u;
constexpr std::uint32_t aux_bee_swarm = 0x08000000u;
constexpr std::uint32_t aux_structural = 0x04000000u;
constexpr std::uint32_t aux_supported = 0x02000000u;
constexpr std::uint32_t aux_moved = 0x01000000u;
constexpr std::uint32_t aux_water_half = 0x00800000u;
constexpr std::uint32_t aux_state_mask = 0x000000ffu;
constexpr std::uint32_t bee_target_none = 0xffffu;
constexpr std::uint32_t bee_authored_home_slot_bit = 0x80u;
}

int main() {
    constexpr std::uint32_t width = 32u;
    constexpr std::uint32_t height = 16u;
    std::vector<sandhybrid::SceneCell> source(width * height);

    // 31 stone cells: below the cohesive minimum, so the imported pixels crumble.
    for (std::uint32_t index = 0u; index < 31u; ++index)
        source[index].material = static_cast<std::uint32_t>(sandhybrid::Material::stone);

    source[31u].material = static_cast<std::uint32_t>(sandhybrid::Material::water);

    // 40 glass cells in the second aligned region: coherent but deliberately weakened.
    for (std::uint32_t index = 0u; index < 40u; ++index)
        source[8u + (index / 8u) * width + index % 8u].material =
            static_cast<std::uint32_t>(sandhybrid::Material::glass);

    constexpr std::uint32_t queen_x = 20u;
    constexpr std::uint32_t queen_y = 12u;
    source[queen_y * width + queen_x].material =
        static_cast<std::uint32_t>(sandhybrid::Material::queen_bee);
    for (const auto x : {17u, 18u, 22u, 23u})
        source[queen_y * width + x].material = static_cast<std::uint32_t>(sandhybrid::Material::bee);

    const auto root = std::filesystem::temp_directory_path() / "sandhybrid_scene_image_contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::string error;
    const auto path = sandhybrid::scene_image_path(root, sandhybrid::Scene::sandbox);
    if (!sandhybrid::save_scene_ppm(path, width, height, source, error)) return 1;

    std::vector<sandhybrid::SceneCell> loaded(width * height);
    if (!sandhybrid::load_scene_ppm(path, width, height, loaded, error)) return 2;

    const auto weak_stone = loaded[0u];
    if ((weak_stone.aux & aux_structural) != 0u ||
        (weak_stone.aux & aux_supported) != 0u ||
        (weak_stone.aux & aux_moved) == 0u ||
        (weak_stone.aux & aux_state_mask) != 48u) return 3;

    const auto imported_water = loaded[31u];
    if (imported_water.material != static_cast<std::uint32_t>(sandhybrid::Material::water) ||
        (imported_water.aux & aux_water_half) != 0u) return 14;
    if (loaded[(queen_y - 2u) * width + queen_x - 3u].material !=
        static_cast<std::uint32_t>(sandhybrid::Material::honey)) return 15;
    if (loaded[(queen_y - 4u) * width + queen_x - 2u].material !=
        static_cast<std::uint32_t>(sandhybrid::Material::pollen)) return 16;
    if (loaded[(queen_y - 4u) * width + queen_x - 1u].material !=
        static_cast<std::uint32_t>(sandhybrid::Material::empty)) return 17;

    const auto reduced_glass = loaded[8u];
    const auto glass_health = reduced_glass.aux & aux_state_mask;
    if ((reduced_glass.aux & aux_structural) == 0u ||
        (reduced_glass.aux & aux_supported) == 0u ||
        glass_health < 64u || glass_health >= 255u) return 4;

    std::uint32_t expected_slot = 0u;
    for (const auto x : {17u, 18u, 22u, 23u}) {
        const auto bee = loaded[queen_y * width + x];
        if ((bee.aux & (aux_bee_fed | aux_bee_swarm)) != (aux_bee_fed | aux_bee_swarm)) return 5;
        const auto home_x = (bee.aux & 255u) * 4u;
        const auto home_y = ((bee.aux >> 8u) & 127u) * 4u;
        const auto packed_slot = (bee.aux >> 15u) & 255u;
        if ((packed_slot & bee_authored_home_slot_bit) == 0u) return 6;
        const auto slot = packed_slot & 127u;
        if (home_x != queen_x || home_y != queen_y || slot != expected_slot) return 6;
        if ((bee.age >> 16u) != bee_target_none) return 7;
        ++expected_slot;
    }

    if (!sandhybrid::write_scene_material_key(root, error)) return 8;
    if (!std::filesystem::is_regular_file(root / "material_key.txt") ||
        !std::filesystem::is_regular_file(root / "material_key.ppm")) return 9;

    // Legacy scene images used Empty for open sky. Only boundary-connected
    // Empty migrates to Atmosphere; the sealed vacuum pocket remains Empty.
    constexpr std::uint32_t legacy_width = 16u;
    constexpr std::uint32_t legacy_height = 16u;
    std::vector<sandhybrid::SceneCell> legacy(legacy_width * legacy_height);
    for (std::uint32_t y = 5u; y <= 10u; ++y) {
        for (std::uint32_t x = 5u; x <= 10u; ++x) {
            const bool wall = x == 5u || x == 10u || y == 5u || y == 10u;
            legacy[y * legacy_width + x].material = static_cast<std::uint32_t>(
                wall ? sandhybrid::Material::stone : sandhybrid::Material::empty);
        }
    }
    const auto legacy_path = root / "legacy.ppm";
    if (!sandhybrid::save_scene_ppm(legacy_path, legacy_width, legacy_height, legacy, error)) return 10;
    std::vector<sandhybrid::SceneCell> migrated(legacy_width * legacy_height);
    if (!sandhybrid::load_scene_ppm(legacy_path, legacy_width, legacy_height, migrated, error)) return 11;
    if (migrated[0u].material != static_cast<std::uint32_t>(sandhybrid::Material::atmosphere)) return 12;
    if (migrated[7u * legacy_width + 7u].material != static_cast<std::uint32_t>(sandhybrid::Material::empty)) return 13;

    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
