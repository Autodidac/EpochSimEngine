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

static_assert(world_save_format_version == 2u);
static_assert(world_save_min_format_version == 1u);
static_assert(world_save_chunk_edge == 64u);
static_assert(world_save_actor_bytes == 80u);
static_assert(world_dimensions(WorldSizePreset::compact).width == 5120u);
static_assert(world_dimensions(WorldSizePreset::standard).width == 7680u);
static_assert(world_dimensions(WorldSizePreset::large).width == 10240u);

[[nodiscard]] bool same_cells(const std::vector<SceneCell>& left,
                              const std::vector<SceneCell>& right) {
    return std::equal(left.begin(), left.end(), right.begin(),
        [](const SceneCell& a, const SceneCell& b) {
            return a.material == b.material && a.age == b.age &&
                   a.temperature == b.temperature && a.aux == b.aux;
        });
}

[[nodiscard]] bool same_actor(const WorldSaveActorState& a,
                              const WorldSaveActorState& b) {
    return a.x == b.x && a.y == b.y && a.velocity_y == b.velocity_y &&
           a.enabled == b.enabled && a.gold == b.gold && a.iron == b.iron &&
           a.ammo == b.ammo && a.shot_timer == b.shot_timer &&
           a.move_cooldown == b.move_cooldown && a.grounded == b.grounded &&
           a.health == b.health && a.oxygen == b.oxygen &&
           a.hit_x == b.hit_x && a.hit_y == b.hit_y && a.scene == b.scene &&
           a.exposure_ticks == b.exposure_ticks && a.aluminum == b.aluminum &&
           a.copper == b.copper && a.unlocks == b.unlocks &&
           a.drill_level == b.drill_level;
}

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

    const WorldSaveActorState first_actor{
        .x = 2088,
        .y = 747,
        .velocity_y = -2,
        .enabled = 1u,
        .gold = 7u,
        .iron = 8u,
        .ammo = 9u,
        .shot_timer = 3u,
        .move_cooldown = 2u,
        .grounded = 1u,
        .health = 201u,
        .oxygen = 187u,
        .hit_x = 2112,
        .hit_y = 734,
        .scene = static_cast<std::uint32_t>(world_scene),
        .exposure_ticks = 4u,
        .aluminum = 5u,
        .copper = 6u,
        .unlocks = 15u,
        .drill_level = 2u,
    };
    const WorldSaveOwners first_owners{
        .actor_present = true,
        .actor = first_actor,
    };

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
    if (!save_world(root, metadata, "slot 1", first, first_owners, error)) return 6;

    WorldSaveMetadata read_metadata{};
    if (!read_world_save_metadata(
            world_save_path(root, WorldSizePreset::compact, Scene::volcano, "slot 1"),
            read_metadata, error)) return 7;
    if (read_metadata.format_version != 2u ||
        read_metadata.width != dimensions.width ||
        read_metadata.height != dimensions.height ||
        read_metadata.scene != Scene::volcano ||
        read_metadata.world_size != WorldSizePreset::compact ||
        read_metadata.owner_payload_bytes != world_save_actor_bytes + 16u) return 8;

    std::vector<SceneCell> loaded(first.size());
    WorldSaveOwners loaded_owners{};
    WorldSaveMetadata loaded_metadata{};
    if (!load_world(root, WorldSizePreset::compact, dimensions.width, dimensions.height,
                    Scene::volcano, "slot 1", loaded, loaded_owners,
                    loaded_metadata, error)) return 9;
    if (!same_cells(first, loaded)) return 10;
    if (!loaded_owners.actor_present ||
        !same_actor(loaded_owners.actor, first_actor)) return 20;
    if (loaded[half_water_index].material != static_cast<std::uint32_t>(Material::water) ||
        loaded[half_water_index].age != 37u ||
        loaded[half_water_index].temperature != 73 ||
        loaded[half_water_index].aux != half_water_aux) return 18;

    auto second = first;
    second.front().material = static_cast<std::uint32_t>(Material::stone);
    auto second_owners = first_owners;
    second_owners.actor.gold = 11u;
    if (!save_world(root, metadata, "slot 1", second, second_owners, error)) return 11;
    const auto primary = world_save_path(
        root, WorldSizePreset::compact, Scene::volcano, "slot 1");
    {
        std::ofstream corrupt{primary, std::ios::binary | std::ios::trunc};
        corrupt << "bad";
    }
    std::fill(loaded.begin(), loaded.end(), SceneCell{});
    loaded_owners = WorldSaveOwners{};
    if (!load_world(root, WorldSizePreset::compact, dimensions.width, dimensions.height,
                    Scene::volcano, "slot 1", loaded, loaded_owners,
                    loaded_metadata, error)) return 12;
    if (error.find("loaded backup") == std::string::npos) return 13;
    if (loaded.front().material != first.front().material) return 14;
    if (!loaded_owners.actor_present ||
        !same_actor(loaded_owners.actor, first_actor)) return 21;
    if (loaded[half_water_index].aux != half_water_aux ||
        loaded[half_water_index].temperature != 73) return 19;

    auto sentinel = loaded;
    const auto sentinel_owners = loaded_owners;
    if (load_world(root, WorldSizePreset::compact, dimensions.width + 1u, dimensions.height,
                   Scene::volcano, "slot 1", loaded, loaded_owners,
                   loaded_metadata, error)) return 15;
    if (!same_cells(loaded, sentinel) ||
        !same_actor(loaded_owners.actor, sentinel_owners.actor) ||
        loaded_owners.actor_present != sentinel_owners.actor_present) return 16;

    auto invalid_owners = first_owners;
    invalid_owners.actor.gold = 10000u;
    if (save_world(root, metadata, "invalid", first, invalid_owners, error)) return 22;

    if (!save_world(root, metadata, "legacy", first, error)) return 23;
    const auto legacy = world_save_path(
        root, WorldSizePreset::compact, Scene::volcano, "legacy");
    {
        std::fstream stream{legacy, std::ios::binary | std::ios::in | std::ios::out};
        if (!stream) return 24;
        stream.seekp(8);
        const char version_one[4]{1, 0, 0, 0};
        stream.write(version_one, 4);
        if (!stream) return 25;
    }
    std::fill(loaded.begin(), loaded.end(), SceneCell{});
    loaded_owners = first_owners;
    if (!load_world(root, WorldSizePreset::compact, dimensions.width, dimensions.height,
                    Scene::volcano, "legacy", loaded, loaded_owners,
                    loaded_metadata, error)) return 26;
    if (loaded_metadata.format_version != 1u || loaded_owners.actor_present ||
        !same_cells(first, loaded)) return 27;

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
