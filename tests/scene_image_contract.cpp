#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"
#include "epoch/sand/scene_image.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t aux_structural = 0x04000000u;
constexpr std::uint32_t aux_supported = 0x02000000u;
constexpr std::uint32_t aux_moved = 0x01000000u;
constexpr std::uint32_t aux_state_mask = 0x000000ffu;
}

int main() {
    constexpr std::uint32_t width = 16u;
    constexpr std::uint32_t height = 8u;
    std::vector<epoch::sand::SceneCell> source(width * height);

    // 31 stone cells: below the cohesive minimum, so the imported pixels crumble.
    for (std::uint32_t index = 0u; index < 31u; ++index)
        source[index].material = static_cast<std::uint32_t>(epoch::sand::Material::stone);

    // 40 glass cells in the second aligned region: coherent but deliberately weakened.
    for (std::uint32_t index = 0u; index < 40u; ++index)
        source[8u + (index / 8u) * width + index % 8u].material =
            static_cast<std::uint32_t>(epoch::sand::Material::glass);

    const auto root = std::filesystem::temp_directory_path() / "sandhybrid_scene_image_contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::string error;
    const auto path = epoch::sand::scene_image_path(root, epoch::sand::Scene::sandbox);
    if (!epoch::sand::save_scene_ppm(path, width, height, source, error)) return 1;

    std::vector<epoch::sand::SceneCell> loaded(width * height);
    if (!epoch::sand::load_scene_ppm(path, width, height, loaded, error)) return 2;

    const auto weak_stone = loaded[0u];
    if ((weak_stone.aux & aux_structural) != 0u ||
        (weak_stone.aux & aux_supported) != 0u ||
        (weak_stone.aux & aux_moved) == 0u ||
        (weak_stone.aux & aux_state_mask) != 48u) return 3;

    const auto reduced_glass = loaded[8u];
    const auto glass_health = reduced_glass.aux & aux_state_mask;
    if ((reduced_glass.aux & aux_structural) == 0u ||
        (reduced_glass.aux & aux_supported) == 0u ||
        glass_health < 64u || glass_health >= 255u) return 4;

    if (!epoch::sand::write_scene_material_key(root, error)) return 5;
    if (!std::filesystem::is_regular_file(root / "material_key.txt") ||
        !std::filesystem::is_regular_file(root / "material_key.ppm")) return 6;

    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
