#include "sandhybrid/material.hpp"
#include "sandhybrid/world_save.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace sandhybrid;

static_assert(world_save_format_version == 2u);
static_assert(world_save_min_format_version == 1u);
static_assert(requires_pre_pr19_hive_migration(1u));
static_assert(!requires_pre_pr19_hive_migration(2u));
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

[[nodiscard]] bool same_metadata(const WorldSaveMetadata& a, const WorldSaveMetadata& b) {
    return a.format_version == b.format_version && a.world_size == b.world_size &&
           a.width == b.width && a.height == b.height && a.scene == b.scene &&
           a.chunk_edge == b.chunk_edge && a.chunk_count == b.chunk_count &&
           a.cell_count == b.cell_count && a.payload_bytes == b.payload_bytes &&
           a.payload_hash == b.payload_hash && a.owner_payload_bytes == b.owner_payload_bytes;
}

[[nodiscard]] std::vector<char> file_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] bool rejected_saves_preserve_slot(const std::filesystem::path& root) {
    WorldSaveMetadata metadata{
        .world_size = WorldSizePreset::compact, .width = 69u, .height = 67u, .scene = world_scene};
    std::vector<SceneCell> cells(69u * 67u,
        SceneCell{static_cast<std::uint32_t>(Material::water), 11u, 73, 96u});
    std::string error;
    constexpr auto slot = "rejected_saves";
    if (!save_world(root, metadata, slot, cells, error)) return false;
    cells.back().temperature = 91;
    if (!save_world(root, metadata, slot, cells, error)) return false;
    const auto primary = world_save_path(root, metadata.world_size, metadata.scene, slot);
    const auto backup = world_save_backup_path(root, metadata.world_size, metadata.scene, slot);
    const auto manifest = primary.parent_path() / "manifest.txt";
    const auto original_primary = file_bytes(primary);
    const auto original_backup = file_bytes(backup);
    const auto original_manifest = file_bytes(manifest);
    const auto unchanged = [&] {
        return file_bytes(primary) == original_primary && file_bytes(backup) == original_backup &&
               file_bytes(manifest) == original_manifest;
    };
    cells.back().material = material_count;
    if (save_world(root, metadata, slot, cells, error) || !unchanged()) return false;
    cells.back().material = static_cast<std::uint32_t>(Material::water);
    auto invalid = metadata;
    invalid.world_size = static_cast<WorldSizePreset>(255u);
    if (save_world(root, invalid, slot, cells, error) || !unchanged()) return false;
    invalid = metadata;
    invalid.scene = static_cast<Scene>(255u);
    if (save_world(root, invalid, slot, cells, error) || !unchanged()) return false;

    // A backup-staging I/O failure must not remove either healthy generation.
    const auto obstruction = primary.parent_path() / "world.bak.tmp";
    std::filesystem::create_directory(obstruction);
    if (save_world(root, metadata, slot, cells, error) || !unchanged()) return false;
    std::filesystem::remove(obstruction);
    if (!save_world(root, metadata, slot, cells, error)) return false;

    auto decoded = cells;
    WorldSaveOwners owners{.actor_present = true, .actor = {.gold = 47u}};
    const auto original_owners = owners;
    WorldSaveMetadata output = metadata;
    output.payload_hash = 0xabcdefu;
    const auto original_output = output;
    if (load_world(root, metadata.world_size, metadata.width + 1u, metadata.height,
                    metadata.scene, slot, decoded, owners, output, error) ||
        !same_cells(decoded, cells) || !same_actor(owners.actor, original_owners.actor) ||
        owners.actor_present != original_owners.actor_present ||
        !same_metadata(output, original_output)) return false;
    // Header parsing also publishes metadata only on success.
    auto corrupt_header = file_bytes(primary);
    corrupt_header[8] = 127;
    {
        std::ofstream stream{primary, std::ios::binary | std::ios::trunc};
        stream.write(corrupt_header.data(), static_cast<std::streamsize>(corrupt_header.size()));
    }
    if (read_world_save_metadata(primary, output, error) ||
        !same_metadata(output, original_output)) return false;
    return true;
}

// Exercise recovery as a sequence, not just one load of the previous generation.
[[nodiscard]] bool repeated_backup_recovery(const std::filesystem::path& root) {
    WorldSaveMetadata metadata{
        .world_size = WorldSizePreset::compact,
        .width = 69u,
        .height = 67u,
        .scene = world_scene,
    };
    const std::vector<SceneCell> original(69u * 67u,
        SceneCell{static_cast<std::uint32_t>(Material::water), 11u, 73, 96u});
    auto newer = original;
    newer.back() = SceneCell{static_cast<std::uint32_t>(Material::stone), 9u, 101, 17u};
    WorldSaveOwners owners{.actor_present = true, .actor = {.gold = 37u}};
    std::string error;
    constexpr auto slot = "recovery_sequence";
    const auto primary = world_save_path(root, metadata.world_size, metadata.scene, slot);
    const auto backup = world_save_backup_path(root, metadata.world_size, metadata.scene, slot);
    if (!save_world(root, metadata, slot, original, owners, error) ||
        !save_world(root, metadata, slot, newer, owners, error)) return false;
    const auto corrupt_primary = [&] {
        std::ofstream corrupt{primary, std::ios::binary | std::ios::trunc};
        corrupt << "interrupted save";
    };
    const auto recover_original = [&] {
        auto decoded = newer;
        WorldSaveOwners decoded_owners{};
        WorldSaveMetadata decoded_metadata{};
        return load_world(root, metadata.world_size, metadata.width, metadata.height,
                          metadata.scene, slot, decoded, decoded_owners,
                          decoded_metadata, error) &&
               error.find("loaded backup") != std::string::npos &&
               same_cells(decoded, original) && decoded_owners.actor_present &&
               same_actor(decoded_owners.actor, owners.actor);
    };
    corrupt_primary();
    if (!recover_original()) return false;
    for (int iteration = 0; iteration < 3; ++iteration) {
        if (!save_world(root, metadata, slot, newer, owners, error)) return false;
        corrupt_primary();
        if (!recover_original()) {
            std::cerr << "good backup lost after recovery/save/corruption: " << error << '\n';
            return false;
        }
    }
    std::filesystem::remove(primary);
    if (!save_world(root, metadata, slot, newer, owners, error)) return false;
    corrupt_primary();
    if (!recover_original()) return false;

    // A later healthy generation must still rotate normally.
    if (!save_world(root, metadata, slot, newer, owners, error) ||
        !save_world(root, metadata, slot, original, owners, error)) return false;
    corrupt_primary();
    auto decoded = original;
    WorldSaveOwners decoded_owners{};
    WorldSaveMetadata decoded_metadata{};
    if (!load_world(root, metadata.world_size, metadata.width, metadata.height,
                    metadata.scene, slot, decoded, decoded_owners,
                    decoded_metadata, error) || !same_cells(decoded, newer)) return false;
    return std::filesystem::is_regular_file(backup);
}

// Deliberately repair the schema-2 wire checksums after fixture mutations. A
// rejected magic/hash would not exercise backup rotation's validate_only path
// through decoded materials, chunk layout, RLE runs, and actor-owner semantics.
namespace semantic_save_fixture {
constexpr std::size_t header_bytes = 72u;
constexpr std::size_t chunk_header_bytes = 32u;
constexpr std::size_t owner_header_bytes = 16u;
constexpr std::size_t payload_hash_offset = 56u;

[[nodiscard]] std::uint32_t u32_at(const std::vector<char>& bytes, const std::size_t offset) {
    std::uint32_t value{};
    for (std::size_t byte = 0u; byte < 4u; ++byte)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(offset + byte)))
                 << (byte * 8u);
    return value;
}

void put_u32(std::vector<char>& bytes, const std::size_t offset, const std::uint32_t value) {
    for (std::size_t byte = 0u; byte < 4u; ++byte)
        bytes.at(offset + byte) = static_cast<char>((value >> (byte * 8u)) & 0xffu);
}

void refresh_checksums(std::vector<char>& bytes) {
    auto offset = header_bytes;
    const auto chunk_count = u32_at(bytes, 36u);
    for (std::uint32_t chunk = 0u; chunk < chunk_count; ++chunk) {
        const auto payload_size = u32_at(bytes, offset + 24u);
        std::uint32_t chunk_hash = 2166136261u;
        for (const auto byte : std::span<const char>{bytes}.subspan(
                 offset + chunk_header_bytes, payload_size)) {
            chunk_hash ^= static_cast<unsigned char>(byte);
            chunk_hash *= 16777619u;
        }
        put_u32(bytes, offset + 28u, chunk_hash);
        offset += chunk_header_bytes + payload_size;
    }
    std::uint64_t payload_hash = 14695981039346656037ull;
    for (const auto byte : std::span<const char>{bytes}.subspan(header_bytes)) {
        payload_hash ^= static_cast<unsigned char>(byte);
        payload_hash *= 1099511628211ull;
    }
    for (std::size_t byte = 0u; byte < 8u; ++byte)
        bytes.at(payload_hash_offset + byte) =
            static_cast<char>((payload_hash >> (byte * 8u)) & 0xffu);
}

[[nodiscard]] bool write(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    return static_cast<bool>(stream);
}

enum class Corruption {
    raw_material, rle_material, actor_inventory, actor_position,
    chunk_origin, partial_chunk_extent, zero_rle_run,
};

struct Case {
    Corruption corruption;
    std::string_view slot;
    std::string_view expected_error;
};
} // namespace semantic_save_fixture

[[nodiscard]] bool semantic_corruption_preserves_backup(const std::filesystem::path& root) {
    using namespace semantic_save_fixture;
    constexpr std::array cases{
        Case{Corruption::raw_material, "semantic_raw_material", "save contains an unknown material id"},
        Case{Corruption::rle_material, "semantic_rle_material", "save contains an unknown material id"},
        Case{Corruption::actor_inventory, "semantic_actor_inventory", "save actor owner state is out of range"},
        Case{Corruption::actor_position, "semantic_actor_position", "save actor owner state is out of range"},
        Case{Corruption::chunk_origin, "semantic_chunk_origin", "save chunk layout is invalid"},
        Case{Corruption::partial_chunk_extent, "semantic_chunk_extent", "save chunk layout is invalid"},
        Case{Corruption::zero_rle_run, "semantic_zero_rle_run", "run-length save chunk is invalid"},
    };
    const WorldSaveMetadata metadata{
        .world_size = WorldSizePreset::compact, .width = 69u, .height = 67u, .scene = world_scene};
    const std::vector<SceneCell> original(69u * 67u,
        SceneCell{static_cast<std::uint32_t>(Material::water), 11u, 73, 96u});
    const WorldSaveOwners original_owners{
        .actor_present = true, .actor = {.x = 23, .y = 31, .enabled = 1u, .gold = 37u}};
    std::string error;
    for (const auto& test : cases) {
        const auto fail = [&](const std::string_view stage) {
            std::cerr << test.slot << ": " << stage << ": " << error << '\n';
            return false;
        };
        auto prior_primary = original;
        prior_primary.back().temperature = 91;
        if (test.corruption == Corruption::raw_material) {
            // Unique ages force raw encoding in the first complete 64x64 chunk.
            for (std::size_t index = 0u; index < prior_primary.size(); ++index)
                prior_primary[index].age = static_cast<std::uint32_t>(index);
        }
        auto primary_owners = original_owners;
        primary_owners.actor.gold = 47u;
        auto next_cells = original;
        next_cells.front() = SceneCell{static_cast<std::uint32_t>(Material::stone), 9u, 101, 17u};
        auto next_owners = original_owners;
        next_owners.actor.gold = 57u;
        if (!save_world(root, metadata, test.slot, original, original_owners, error) ||
            !save_world(root, metadata, test.slot, prior_primary, primary_owners, error))
            return fail("prepare two healthy generations");
        const auto primary = world_save_path(root, metadata.world_size, metadata.scene, test.slot);
        const auto backup = world_save_backup_path(root, metadata.world_size, metadata.scene, test.slot);
        const auto good_backup = file_bytes(backup);
        auto bytes = file_bytes(primary);
        if (good_backup.empty() || bytes.size() < header_bytes + chunk_header_bytes +
                owner_header_bytes + world_save_actor_bytes || u32_at(bytes, 36u) != 4u)
            return fail("fixture wire layout");
        const auto first_payload = header_bytes + chunk_header_bytes;
        const auto first_encoding = u32_at(bytes, header_bytes + 16u);
        auto last_chunk = header_bytes;
        for (std::uint32_t chunk = 1u; chunk < 4u; ++chunk)
            last_chunk += chunk_header_bytes + u32_at(bytes, last_chunk + 24u);
        const auto actor_payload = bytes.size() - world_save_actor_bytes;
        if (test.corruption == Corruption::raw_material) {
            // Prove the valid RAW happy path before damaging its payload. A
            // rejected unknown material alone cannot establish exact decoding.
            if (first_encoding != 0u) return fail("healthy RAW fixture encoding");
            auto raw_loaded = original;
            WorldSaveOwners raw_owners{};
            WorldSaveMetadata raw_metadata{};
            if (!load_world(root, metadata.world_size, metadata.width, metadata.height,
                            metadata.scene, test.slot, raw_loaded, raw_owners,
                            raw_metadata, error) || !error.empty() ||
                !same_cells(raw_loaded, prior_primary) || !raw_owners.actor_present ||
                !same_actor(raw_owners.actor, primary_owners.actor))
                return fail("healthy RAW primary round-trip");

            // Use an independent slot so all seven invalid-primary sequences
            // below still start with their original known-good Water backup.
            constexpr auto healthy_slot = "healthy_raw_rotation";
            if (!save_world(root, metadata, healthy_slot, prior_primary, primary_owners, error) ||
                !save_world(root, metadata, healthy_slot, next_cells, next_owners, error))
                return fail("rotate healthy RAW generation");
            const auto raw_primary = world_save_path(
                root, metadata.world_size, metadata.scene, healthy_slot);
            const auto raw_backup = world_save_backup_path(
                root, metadata.world_size, metadata.scene, healthy_slot);
            if (file_bytes(raw_backup) != bytes)
                return fail("healthy RAW backup was not byte-identical");
            raw_loaded = next_cells;
            raw_owners = next_owners;
            if (!write(raw_primary, {'b', 'a', 'd'}) ||
                !load_world(root, metadata.world_size, metadata.width, metadata.height,
                            metadata.scene, healthy_slot, raw_loaded, raw_owners,
                            raw_metadata, error) ||
                error.find("loaded backup") == std::string::npos ||
                !same_cells(raw_loaded, prior_primary) || !raw_owners.actor_present ||
                !same_actor(raw_owners.actor, primary_owners.actor) ||
                file_bytes(raw_backup) != bytes)
                return fail("healthy RAW backup recovery");
        }
        switch (test.corruption) {
        case Corruption::raw_material:
            if (first_encoding != 0u) return fail("raw fixture was not encoded raw");
            put_u32(bytes, first_payload, material_count);
            break;
        case Corruption::rle_material:
            if (first_encoding != 1u) return fail("RLE fixture was not encoded RLE");
            put_u32(bytes, first_payload + 4u, material_count);
            break;
        case Corruption::actor_inventory:
            put_u32(bytes, actor_payload + 4u * 4u, 10000u); // Gold owner exceeds 9999.
            break;
        case Corruption::actor_position:
            put_u32(bytes, actor_payload, metadata.width); // Enabled actor lies just outside.
            break;
        case Corruption::chunk_origin:
            put_u32(bytes, last_chunk, 0u); // Final x must be 64, not an overlapping x=0.
            break;
        case Corruption::partial_chunk_extent:
            if (u32_at(bytes, last_chunk + 8u) != 5u ||
                u32_at(bytes, last_chunk + 12u) != 3u)
                return fail("partial-edge fixture dimensions");
            put_u32(bytes, last_chunk + 8u, 4u); // Final chunk is exactly 5x3.
            break;
        case Corruption::zero_rle_run:
            if (first_encoding != 1u) return fail("RLE fixture was not encoded RLE");
            put_u32(bytes, first_payload, 0u);
            break;
        }
        refresh_checksums(bytes);
        if (!write(primary, bytes)) return fail("write checksummed semantic corruption");
        WorldSaveMetadata checked_header{};
        if (!read_world_save_metadata(primary, checked_header, error))
            return fail("corruption failed before semantic decode");

        auto recovered = next_cells;
        WorldSaveOwners recovered_owners{};
        WorldSaveMetadata recovered_metadata{};
        const auto recovers_original = [&] {
            return load_world(root, metadata.world_size, metadata.width, metadata.height,
                              metadata.scene, test.slot, recovered, recovered_owners,
                              recovered_metadata, error) &&
                   error.find("loaded backup") != std::string::npos &&
                   same_cells(recovered, original) && recovered_owners.actor_present &&
                   same_actor(recovered_owners.actor, original_owners.actor);
        };
        if (!recovers_original() || error.find(test.expected_error) == std::string::npos ||
            file_bytes(backup) != good_backup)
            return fail("semantic rejection and exact backup recovery");

        // save_world must perform the same semantic rejection with validate_only
        // rather than rotate the checksummed-but-invalid primary over this backup.
        if (!save_world(root, metadata, test.slot, next_cells, next_owners, error) ||
            file_bytes(backup) != good_backup)
            return fail("valid save rotated an invalid primary");
        if (!load_world(root, metadata.world_size, metadata.width, metadata.height,
                        metadata.scene, test.slot, recovered, recovered_owners,
                        recovered_metadata, error) || !error.empty() ||
            !same_cells(recovered, next_cells) || !recovered_owners.actor_present ||
            !same_actor(recovered_owners.actor, next_owners.actor))
            return fail("new primary was not published exactly");
        if (!write(primary, {'b', 'a', 'd'}) || !recovers_original() ||
            file_bytes(backup) != good_backup)
            return fail("good backup lost after semantic recovery/save/corruption");
    }
    return true;
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

    if (!repeated_backup_recovery(root)) return 28;
    if (!rejected_saves_preserve_slot(root)) return 29;
    if (!semantic_corruption_preserves_backup(root)) return 30;

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
