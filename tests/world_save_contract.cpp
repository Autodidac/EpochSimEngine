#include "sandhybrid/material.hpp"
#include "sandhybrid/world_save.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sandhybrid;

static_assert(world_save_format_version == 1u);
static_assert(world_save_chunk_edge == 64u);
static_assert(world_dimensions(WorldSizePreset::compact).width == 5120u);
static_assert(world_dimensions(WorldSizePreset::standard).width == 7680u);
static_assert(world_dimensions(WorldSizePreset::large).width == 10240u);

int main() {
    if (parse_world_size("small") != WorldSizePreset::compact) return 1;
    if (parse_world_size("MEDIUM") != WorldSizePreset::standard) return 2;
    if (parse_world_size("large") != WorldSizePreset::large) return 3;
    if (parse_world_size("wrong").has_value()) return 4;
    if (normalize_world_slot("../../ My World ") != "My_World") return 5;
    if (scene_save_name(world_scene) != "world") return 17;

    const auto dimensions = world_dimensions(WorldSizePreset::compact);
    std::vector<SceneCell> first(
        static_cast<std::size_t>(dimensions.width) * dimensions.height,
        SceneCell{static_cast<std::uint32_t>(Material::atmosphere), 0u, 20, 54u});
    for (std::uint32_t y = 500u; y < 510u; ++y) {
        for (std::uint32_t x = 1100u; x < 1180u; ++x) {
            auto& cell = first[static_cast<std::size_t>(y) * dimensions.width + x];
            cell = SceneCell{static_cast<std::uint32_t>(Material::water), x + y, 18, 96u};
        }
    }
    constexpr std::uint32_t half_water_aux =
        0x00800000u |
        ((static_cast<std::uint32_t>(Material::atmosphere) & 0x7fu) << 8u) |
        121u;
    const auto half_water_index =
        static_cast<std::size_t>(505u) * dimensions.width + 1140u;
    first[half_water_index] = SceneCell{
        static_cast<std::uint32_t>(Material::water), 37u, 73, half_water_aux};

    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::temp_directory_path() /
        ("sandhybrid-world-save-contract-" + suffix);
    std::string error;
    WorldSaveMetadata metadata{
        .world_size = WorldSizePreset::compact,
        .width = dimensions.width,
        .height = dimensions.height,
        .scene = Scene::volcano,
    };
    if (!save_world(root, metadata, "slot 1", first, error)) return 6;

    WorldSaveMetadata read_metadata{};
    if (!read_world_save_metadata(
            world_save_path(root, WorldSizePreset::compact, Scene::volcano, "slot 1"),
            read_metadata, error)) return 7;
    if (read_metadata.width != dimensions.width ||
        read_metadata.height != dimensions.height ||
        read_metadata.scene != Scene::volcano ||
        read_metadata.world_size != WorldSizePreset::compact) return 8;

    std::vector<SceneCell> loaded(first.size());
    WorldSaveMetadata loaded_metadata{};
    if (!load_world(root, WorldSizePreset::compact, dimensions.width, dimensions.height,
                    Scene::volcano, "slot 1", loaded, loaded_metadata, error)) return 9;
    if (!std::equal(first.begin(), first.end(), loaded.begin(), [](const SceneCell& left,
                                                                  const SceneCell& right) {
            return left.material == right.material && left.age == right.age &&
                   left.temperature == right.temperature && left.aux == right.aux;
        })) return 10;
    if (loaded[half_water_index].material != static_cast<std::uint32_t>(Material::water) ||
        loaded[half_water_index].age != 37u ||
        loaded[half_water_index].temperature != 73 ||
        loaded[half_water_index].aux != half_water_aux) return 18;

    auto second = first;
    second.front().material = static_cast<std::uint32_t>(Material::stone);
    if (!save_world(root, metadata, "slot 1", second, error)) return 11;
    const auto primary = world_save_path(
        root, WorldSizePreset::compact, Scene::volcano, "slot 1");
    {
        std::ofstream corrupt{primary, std::ios::binary | std::ios::trunc};
        corrupt << "bad";
    }
    std::fill(loaded.begin(), loaded.end(), SceneCell{});
    if (!load_world(root, WorldSizePreset::compact, dimensions.width, dimensions.height,
                    Scene::volcano, "slot 1", loaded, loaded_metadata, error)) return 12;
    if (error.find("loaded backup") == std::string::npos) return 13;
    if (loaded.front().material != first.front().material) return 14;
    if (loaded[half_water_index].aux != half_water_aux ||
        loaded[half_water_index].temperature != 73) return 19;

    auto sentinel = loaded;
    if (load_world(root, WorldSizePreset::compact, dimensions.width + 1u, dimensions.height,
                   Scene::volcano, "slot 1", loaded, loaded_metadata, error)) return 15;
    if (!std::equal(loaded.begin(), loaded.end(), sentinel.begin(), [](const SceneCell& left,
                                                                      const SceneCell& right) {
            return left.material == right.material && left.age == right.age &&
                   left.temperature == right.temperature && left.aux == right.aux;
        })) return 16;

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
