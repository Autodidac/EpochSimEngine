#include "sandhybrid/world_save.hpp"

#include "sandhybrid/material.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace sandhybrid {
namespace {

constexpr std::array<std::uint8_t, 8> save_magic{
    'S', 'H', 'W', 'R', 'L', 'D', '1', 0,
};
constexpr std::array<std::uint8_t, 8> owner_magic{
    'S', 'H', 'O', 'W', 'N', 'R', '1', 0,
};
constexpr std::uint32_t save_header_bytes = 72u;
constexpr std::uint32_t chunk_header_bytes = 32u;
constexpr std::uint32_t owner_header_bytes = 16u;
constexpr std::uint32_t owner_record_bytes = owner_header_bytes + world_save_actor_bytes;
constexpr std::uint32_t encoding_raw = 0u;
constexpr std::uint32_t encoding_run_length = 1u;
constexpr std::uint64_t fnv_offset_64 = 14695981039346656037ull;
constexpr std::uint64_t fnv_prime_64 = 1099511628211ull;
constexpr std::uint32_t fnv_offset_32 = 2166136261u;
constexpr std::uint32_t fnv_prime_32 = 16777619u;

[[nodiscard]] bool same_cell(const SceneCell& first, const SceneCell& second) noexcept {
    return first.material == second.material && first.age == second.age &&
           first.temperature == second.temperature && first.aux == second.aux;
}

void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

void append_u64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

void append_i32(std::vector<std::uint8_t>& bytes, const std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_cell(std::vector<std::uint8_t>& bytes, const SceneCell& cell) {
    append_u32(bytes, cell.material);
    append_u32(bytes, cell.age);
    append_i32(bytes, cell.temperature);
    append_u32(bytes, cell.aux);
}

void append_actor(std::vector<std::uint8_t>& bytes,
                  const WorldSaveActorState& actor) {
    append_i32(bytes, actor.x);
    append_i32(bytes, actor.y);
    append_i32(bytes, actor.velocity_y);
    append_u32(bytes, actor.enabled);
    append_u32(bytes, actor.gold);
    append_u32(bytes, actor.iron);
    append_u32(bytes, actor.ammo);
    append_u32(bytes, actor.shot_timer);
    append_u32(bytes, actor.move_cooldown);
    append_u32(bytes, actor.grounded);
    append_u32(bytes, actor.health);
    append_u32(bytes, actor.oxygen);
    append_i32(bytes, actor.hit_x);
    append_i32(bytes, actor.hit_y);
    append_u32(bytes, actor.scene);
    append_u32(bytes, actor.exposure_ticks);
    append_u32(bytes, actor.aluminum);
    append_u32(bytes, actor.copper);
    append_u32(bytes, actor.unlocks);
    append_u32(bytes, actor.drill_level);
}
[[nodiscard]] bool read_u32(const std::span<const std::uint8_t> bytes,
                            std::size_t& offset,
                            std::uint32_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 4u) return false;
    value = 0u;
    for (std::uint32_t index = 0u; index < 4u; ++index)
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8u);
    offset += 4u;
    return true;
}

[[nodiscard]] bool read_u64(const std::span<const std::uint8_t> bytes,
                            std::size_t& offset,
                            std::uint64_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 8u) return false;
    value = 0u;
    for (std::uint32_t index = 0u; index < 8u; ++index)
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8u);
    offset += 8u;
    return true;
}

[[nodiscard]] bool read_cell(const std::span<const std::uint8_t> bytes,
                             std::size_t& offset,
                             SceneCell& cell) noexcept {
    std::uint32_t temperature{};
    return read_u32(bytes, offset, cell.material) &&
           read_u32(bytes, offset, cell.age) &&
           read_u32(bytes, offset, temperature) &&
           read_u32(bytes, offset, cell.aux) &&
           ((cell.temperature = static_cast<std::int32_t>(temperature)), true);
}

[[nodiscard]] bool read_actor(const std::span<const std::uint8_t> bytes,
                              std::size_t& offset,
                              WorldSaveActorState& actor) noexcept {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t velocity_y{};
    std::uint32_t hit_x{};
    std::uint32_t hit_y{};
    return read_u32(bytes, offset, x) &&
           read_u32(bytes, offset, y) &&
           read_u32(bytes, offset, velocity_y) &&
           read_u32(bytes, offset, actor.enabled) &&
           read_u32(bytes, offset, actor.gold) &&
           read_u32(bytes, offset, actor.iron) &&
           read_u32(bytes, offset, actor.ammo) &&
           read_u32(bytes, offset, actor.shot_timer) &&
           read_u32(bytes, offset, actor.move_cooldown) &&
           read_u32(bytes, offset, actor.grounded) &&
           read_u32(bytes, offset, actor.health) &&
           read_u32(bytes, offset, actor.oxygen) &&
           read_u32(bytes, offset, hit_x) &&
           read_u32(bytes, offset, hit_y) &&
           read_u32(bytes, offset, actor.scene) &&
           read_u32(bytes, offset, actor.exposure_ticks) &&
           read_u32(bytes, offset, actor.aluminum) &&
           read_u32(bytes, offset, actor.copper) &&
           read_u32(bytes, offset, actor.unlocks) &&
           read_u32(bytes, offset, actor.drill_level) &&
           ((actor.x = static_cast<std::int32_t>(x)),
            (actor.y = static_cast<std::int32_t>(y)),
            (actor.velocity_y = static_cast<std::int32_t>(velocity_y)),
            (actor.hit_x = static_cast<std::int32_t>(hit_x)),
            (actor.hit_y = static_cast<std::int32_t>(hit_y)), true);
}

[[nodiscard]] bool actor_state_valid(const WorldSaveActorState& actor,
                                     const std::uint32_t width,
                                     const std::uint32_t height) noexcept {
    constexpr std::uint32_t inventory_limit = 9999u;
    return actor.enabled <= 1u && actor.grounded <= 1u &&
           actor.gold <= inventory_limit && actor.iron <= inventory_limit &&
           actor.ammo <= inventory_limit && actor.aluminum <= inventory_limit &&
           actor.copper <= inventory_limit && actor.health <= 255u &&
           actor.oxygen <= 255u && actor.scene < legacy_scene_count &&
           (actor.unlocks & ~15u) == 0u && actor.drill_level <= 2u &&
           (actor.enabled == 0u ||
            (actor.x >= 0 && actor.y >= 0 &&
             actor.x < static_cast<std::int32_t>(width) &&
             actor.y < static_cast<std::int32_t>(height)));
}

[[nodiscard]] std::uint64_t hash64(const std::span<const std::uint8_t> bytes) noexcept {
    auto value = fnv_offset_64;
    for (const auto byte : bytes) {
        value ^= byte;
        value *= fnv_prime_64;
    }
    return value;
}

[[nodiscard]] std::uint32_t hash32(const std::span<const std::uint8_t> bytes) noexcept {
    auto value = fnv_offset_32;
    for (const auto byte : bytes) {
        value ^= byte;
        value *= fnv_prime_32;
    }
    return value;
}

[[nodiscard]] bool write_bytes(const std::filesystem::path& path,
                               const std::span<const std::uint8_t> first,
                               const std::span<const std::uint8_t> second,
                               std::string& error) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        error = "unable to write " + path.string();
        return false;
    }
    stream.write(reinterpret_cast<const char*>(first.data()),
                 static_cast<std::streamsize>(first.size()));
    stream.write(reinterpret_cast<const char*>(second.data()),
                 static_cast<std::streamsize>(second.size()));
    stream.flush();
    if (!stream) {
        error = "failed while writing " + path.string();
        return false;
    }
    stream.close();
    if (!stream) error = "failed while closing " + path.string();
    return static_cast<bool>(stream);
}

[[nodiscard]] bool publish_file(const std::filesystem::path& temporary,
                                const std::filesystem::path& destination,
                                std::string& error) {
#ifdef _WIN32
    // std::filesystem::rename cannot replace an existing file on Windows.
    if (MoveFileExW(temporary.c_str(), destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    const std::error_code filesystem_error{
        static_cast<int>(GetLastError()), std::system_category()};
#else
    std::error_code filesystem_error;
    std::filesystem::rename(temporary, destination, filesystem_error);
    if (!filesystem_error) return true;
#endif
    error = "unable to publish save: " + filesystem_error.message();
    return false;
}

[[nodiscard]] bool write_manifest(const std::filesystem::path& directory,
                                  const WorldSaveMetadata& metadata,
                                  const std::string_view slot,
                                  std::string& error) {
    const auto temporary = directory / "manifest.tmp";
    const auto destination = directory / "manifest.txt";
    std::ofstream stream{temporary, std::ios::trunc};
    if (!stream) {
        error = "unable to write save manifest";
        return false;
    }
    stream << "format=SandHybridWorld\n"
           << "version=" << metadata.format_version << '\n'
           << "slot=" << normalize_world_slot(slot) << '\n'
           << "size=" << world_size_name(metadata.world_size) << '\n'
           << "width=" << metadata.width << '\n'
           << "height=" << metadata.height << '\n'
           << "scene=" << scene_save_name(metadata.scene) << '\n'
           << "cell_count=" << metadata.cell_count << '\n'
           << "chunk_edge=" << metadata.chunk_edge << '\n'
           << "chunk_count=" << metadata.chunk_count << '\n'
           << "payload_bytes=" << metadata.payload_bytes << '\n'
           << "payload_hash=" << metadata.payload_hash << '\n'
           << "owner_payload_bytes=" << metadata.owner_payload_bytes << '\n'
           << "actor_owner=" << (metadata.owner_payload_bytes != 0u ? "present" : "absent") << '\n';
    stream.flush();
    if (!stream) {
        error = "failed while writing save manifest";
        return false;
    }
    stream.close();
    if (!stream) {
        error = "failed while closing save manifest";
        return false;
    }
    return publish_file(temporary, destination, error);
}

[[nodiscard]] bool read_file(const std::filesystem::path& path,
                             std::vector<std::uint8_t>& bytes,
                             std::string& error) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        error = "save not found: " + path.string();
        return false;
    }
    const auto end = stream.tellg();
    if (end < 0) {
        error = "unable to determine save size: " + path.string();
        return false;
    }
    const auto size = static_cast<std::uint64_t>(end);
    if (size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        error = "save file is too large for this platform";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        error = "save file is truncated: " + path.string();
        return false;
    }
    return true;
}

[[nodiscard]] bool decode_header(const std::span<const std::uint8_t> bytes,
                                 WorldSaveMetadata& metadata,
                                 std::size_t& offset,
                                 std::string& error) {
    if (bytes.size() < save_header_bytes ||
        !std::equal(save_magic.begin(), save_magic.end(), bytes.begin())) {
        error = "save magic is invalid";
        return false;
    }
    offset = save_magic.size();
    std::uint32_t header_bytes{};
    std::uint32_t preset{};
    std::uint32_t scene{};
    if (!read_u32(bytes, offset, metadata.format_version) ||
        !read_u32(bytes, offset, header_bytes) ||
        !read_u32(bytes, offset, metadata.width) ||
        !read_u32(bytes, offset, metadata.height) ||
        !read_u32(bytes, offset, preset) ||
        !read_u32(bytes, offset, scene) ||
        !read_u32(bytes, offset, metadata.chunk_edge) ||
        !read_u32(bytes, offset, metadata.chunk_count) ||
        !read_u64(bytes, offset, metadata.cell_count) ||
        !read_u64(bytes, offset, metadata.payload_bytes) ||
        !read_u64(bytes, offset, metadata.payload_hash) ||
        !read_u64(bytes, offset, metadata.owner_payload_bytes)) {
        error = "save header is truncated";
        return false;
    }
    if (metadata.format_version < world_save_min_format_version ||
        metadata.format_version > world_save_format_version ||
        header_bytes != save_header_bytes || metadata.chunk_edge != world_save_chunk_edge) {
        error = "save format version is unsupported";
        return false;
    }
    if ((metadata.format_version == 1u && metadata.owner_payload_bytes != 0u) ||
        (metadata.owner_payload_bytes != 0u &&
         metadata.owner_payload_bytes != owner_record_bytes) ||
        metadata.owner_payload_bytes > metadata.payload_bytes) {
        error = "save owner payload size is invalid";
        return false;
    }
    if (preset > static_cast<std::uint32_t>(WorldSizePreset::large) ||
        scene >= legacy_scene_count) {
        error = "save metadata contains an invalid size or scene";
        return false;
    }
    metadata.world_size = static_cast<WorldSizePreset>(preset);
    metadata.scene = static_cast<Scene>(scene);
    if (metadata.width == 0u || metadata.height == 0u ||
        metadata.cell_count != static_cast<std::uint64_t>(metadata.width) * metadata.height) {
        error = "save dimensions are invalid";
        return false;
    }
    if (metadata.payload_bytes != bytes.size() - save_header_bytes ||
        hash64(bytes.subspan(save_header_bytes)) != metadata.payload_hash) {
        error = "save payload checksum failed";
        return false;
    }
    return true;
}

[[nodiscard]] bool decode_world_file(const std::filesystem::path& path,
                                     const WorldSizePreset expected_size,
                                     const std::uint32_t expected_width,
                                     const std::uint32_t expected_height,
                                     const Scene expected_scene,
                                     const std::span<SceneCell> cells,
                                     WorldSaveOwners& owners,
                                     WorldSaveMetadata& output_metadata,
                                     std::string& error,
                                     const bool validate_only = false) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes, error)) return false;
    std::size_t offset{};
    WorldSaveMetadata metadata{};
    if (!decode_header(bytes, metadata, offset, error)) return false;
    if (metadata.world_size != expected_size || metadata.width != expected_width ||
        metadata.height != expected_height || metadata.scene != expected_scene) {
        error = "save belongs to " + std::string{world_size_name(metadata.world_size)} + " " +
                std::to_string(metadata.width) + "x" + std::to_string(metadata.height) +
                " scene " + std::string{scene_save_name(metadata.scene)};
        return false;
    }
    if (!validate_only && cells.size() != metadata.cell_count) {
        error = "destination cell span does not match the save dimensions";
        return false;
    }
    const auto cell_payload_end = bytes.size() -
        static_cast<std::size_t>(metadata.owner_payload_bytes);
    if (cell_payload_end < offset) {
        error = "save owner payload overlaps the cell payload";
        return false;
    }

    const auto chunk_columns = 1u + (metadata.width - 1u) / metadata.chunk_edge;
    const auto chunk_rows = 1u + (metadata.height - 1u) / metadata.chunk_edge;
    if (metadata.chunk_count != static_cast<std::uint64_t>(chunk_columns) * chunk_rows ||
        metadata.chunk_count > (cell_payload_end - offset) / chunk_header_bytes) {
        error = "save chunk count is invalid";
        return false;
    }

    std::vector<SceneCell> decoded(validate_only ? 0u : cells.size());
    for (std::uint32_t chunk_index = 0u; chunk_index < metadata.chunk_count; ++chunk_index) {
        std::uint32_t chunk_x{};
        std::uint32_t chunk_y{};
        std::uint32_t chunk_width{};
        std::uint32_t chunk_height{};
        std::uint32_t encoding{};
        std::uint32_t decoded_count{};
        std::uint32_t payload_size{};
        std::uint32_t payload_checksum{};
        if (!read_u32(bytes, offset, chunk_x) || !read_u32(bytes, offset, chunk_y) ||
            !read_u32(bytes, offset, chunk_width) || !read_u32(bytes, offset, chunk_height) ||
            !read_u32(bytes, offset, encoding) || !read_u32(bytes, offset, decoded_count) ||
            !read_u32(bytes, offset, payload_size) || !read_u32(bytes, offset, payload_checksum)) {
            error = "save chunk header is truncated";
            return false;
        }
        const auto expected_x = (chunk_index % chunk_columns) * metadata.chunk_edge;
        const auto expected_y = (chunk_index / chunk_columns) * metadata.chunk_edge;
        const auto expected_chunk_width = (std::min)(metadata.chunk_edge, metadata.width - expected_x);
        const auto expected_chunk_height = (std::min)(metadata.chunk_edge, metadata.height - expected_y);
        const auto expected_count = expected_chunk_width * expected_chunk_height;
        if (chunk_x != expected_x || chunk_y != expected_y ||
            chunk_width != expected_chunk_width || chunk_height != expected_chunk_height ||
            decoded_count != expected_count || offset > cell_payload_end ||
            cell_payload_end - offset < payload_size) {
            error = "save chunk layout is invalid";
            return false;
        }
        const auto payload = std::span<const std::uint8_t>{bytes}.subspan(offset, payload_size);
        if (hash32(payload) != payload_checksum) {
            error = "save chunk checksum failed";
            return false;
        }
        offset += payload_size;

        std::vector<SceneCell> chunk_cells;
        chunk_cells.reserve(expected_count);
        std::size_t payload_offset{};
        if (encoding == encoding_raw) {
            if (payload_size != expected_count * sizeof(SceneCell)) {
                error = "raw save chunk has the wrong byte count";
                return false;
            }
            while (chunk_cells.size() < expected_count) {
                SceneCell cell{};
                if (!read_cell(payload, payload_offset, cell)) {
                    error = "raw save chunk is truncated";
                    return false;
                }
                chunk_cells.push_back(cell);
            }
        } else if (encoding == encoding_run_length) {
            while (payload_offset < payload.size() && chunk_cells.size() < expected_count) {
                std::uint32_t run_length{};
                SceneCell cell{};
                if (!read_u32(payload, payload_offset, run_length) || run_length == 0u ||
                    !read_cell(payload, payload_offset, cell) ||
                    run_length > expected_count - chunk_cells.size()) {
                    error = "run-length save chunk is invalid";
                    return false;
                }
                chunk_cells.insert(chunk_cells.end(), run_length, cell);
            }
            if (chunk_cells.size() != expected_count || payload_offset != payload.size()) {
                error = "run-length save chunk does not decode to its declared size";
                return false;
            }
        } else {
            error = "save chunk encoding is unsupported";
            return false;
        }

        for (const auto& cell : chunk_cells) {
            if (cell.material >= material_count) {
                error = "save contains an unknown material id";
                return false;
            }
        }
        if (validate_only) continue;
        std::size_t source_index{};
        for (std::uint32_t local_y = 0u; local_y < chunk_height; ++local_y) {
            const auto destination = static_cast<std::size_t>(chunk_y + local_y) * metadata.width + chunk_x;
            std::copy_n(chunk_cells.begin() + static_cast<std::ptrdiff_t>(source_index),
                        chunk_width,
                        decoded.begin() + static_cast<std::ptrdiff_t>(destination));
            source_index += chunk_width;
        }
    }
    if (offset != cell_payload_end) {
        error = "save cell payload contains trailing data";
        return false;
    }

    WorldSaveOwners decoded_owners{};
    if (metadata.owner_payload_bytes != 0u) {
        if (bytes.size() - offset != owner_record_bytes ||
            !std::equal(owner_magic.begin(), owner_magic.end(),
                        bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
            error = "save owner record magic or size is invalid";
            return false;
        }
        offset += owner_magic.size();
        std::uint32_t actor_version{};
        std::uint32_t actor_bytes{};
        if (!read_u32(bytes, offset, actor_version) ||
            !read_u32(bytes, offset, actor_bytes) ||
            actor_version != world_save_actor_version ||
            actor_bytes != world_save_actor_bytes ||
            !read_actor(bytes, offset, decoded_owners.actor) ||
            offset != bytes.size()) {
            error = "save actor owner record is invalid";
            return false;
        }
        if (!actor_state_valid(decoded_owners.actor, metadata.width, metadata.height)) {
            error = "save actor owner state is out of range";
            return false;
        }
        decoded_owners.actor_present = true;
    } else if (offset != bytes.size()) {
        error = "save contains trailing data";
        return false;
    }

    std::copy(decoded.begin(), decoded.end(), cells.begin());
    owners = decoded_owners;
    output_metadata = metadata;
    return true;
}

[[nodiscard]] bool replace_atomically(const std::filesystem::path& temporary,
                                      const std::filesystem::path& destination,
                                      const std::filesystem::path& backup,
                                      const WorldSaveMetadata& metadata,
                                      std::string& error) {
    // A recovered corrupt primary is not a backup generation. Validate every
    // chunk/material/owner without allocating another resident cell field.
    WorldSaveMetadata previous_metadata{};
    WorldSaveOwners previous_owners{};
    std::string validation_error;
    if (decode_world_file(destination, metadata.world_size, metadata.width,
                          metadata.height, metadata.scene, {}, previous_owners,
                          previous_metadata, validation_error, true)) {
        const auto staged_backup = backup.parent_path() / "world.bak.tmp";
        std::error_code filesystem_error;
        std::filesystem::copy_file(destination, staged_backup,
            std::filesystem::copy_options::overwrite_existing, filesystem_error);
        if (filesystem_error) {
            error = "unable to stage save backup: " + filesystem_error.message();
            return false;
        }
        // Neither the previous backup nor primary is removed before its
        // replacement is ready. Failure leaves at least the old primary intact.
        if (!publish_file(staged_backup, backup, error)) return false;
    }
    return publish_file(temporary, destination, error);
}

} // namespace

std::optional<WorldSizePreset> parse_world_size(const std::string_view value) noexcept {
    std::string normalized;
    normalized.reserve(value.size());
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isspace(byte))
            normalized.push_back(static_cast<char>(std::tolower(byte)));
    }
    if (normalized == "compact" || normalized == "small") return WorldSizePreset::compact;
    if (normalized == "standard" || normalized == "medium") return WorldSizePreset::standard;
    if (normalized == "large") return WorldSizePreset::large;
    return std::nullopt;
}

std::optional<WorldSizePreset> world_size_from_dimensions(
    const std::uint32_t width, const std::uint32_t height) noexcept {
    for (const auto preset : {WorldSizePreset::compact,
                              WorldSizePreset::standard,
                              WorldSizePreset::large}) {
        const auto dimensions = world_dimensions(preset);
        if (dimensions.width == width && dimensions.height == height) return preset;
    }
    return std::nullopt;
}

std::string normalize_world_slot(const std::string_view slot) {
    std::string normalized;
    normalized.reserve((std::min)(slot.size(), std::size_t{32u}));
    for (const auto character : slot) {
        if (normalized.size() == 32u) break;
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '-' || character == '_')
            normalized.push_back(character);
        else if (character == ' ' || character == '.')
            normalized.push_back('_');
    }
    while (!normalized.empty() && normalized.front() == '_') normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();
    return normalized.empty() ? std::string{default_world_save_slot} : normalized;
}

std::string_view scene_save_name(const Scene scene) noexcept {
    switch (scene) {
    case Scene::sandbox: return "world";
    case Scene::blank: return "blank";
    case Scene::volcano: return "volcano";
    case Scene::waterworks: return "waterworks";
    case Scene::ecosystem: return "ecosystem";
    case Scene::engineering_lab: return "engineering_lab";
    case Scene::gold_mine: return "platformer";
    case Scene::demolition: return "demolition";
    case Scene::frontier_base: return "frontier_base";
    case Scene::count: break;
    }
    return "unknown";
}

std::filesystem::path world_save_directory(
    const std::filesystem::path& application_directory,
    const WorldSizePreset preset,
    const Scene scene,
    const std::string_view slot) {
    return application_directory / "saves" / "worlds" /
           world_size_name(preset) / scene_save_name(scene) / normalize_world_slot(slot);
}

std::filesystem::path world_save_path(
    const std::filesystem::path& application_directory,
    const WorldSizePreset preset,
    const Scene scene,
    const std::string_view slot) {
    return world_save_directory(application_directory, preset, scene, slot) / "world.shw";
}

std::filesystem::path world_save_backup_path(
    const std::filesystem::path& application_directory,
    const WorldSizePreset preset,
    const Scene scene,
    const std::string_view slot) {
    return world_save_directory(application_directory, preset, scene, slot) / "world.bak";
}

bool read_world_save_metadata(const std::filesystem::path& path,
                              WorldSaveMetadata& metadata,
                              std::string& error) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes, error)) return false;
    std::size_t offset{};
    WorldSaveMetadata decoded{};
    if (!decode_header(bytes, decoded, offset, error)) return false;
    metadata = decoded;
    error.clear();
    return true;
}

bool save_world(const std::filesystem::path& application_directory,
                const WorldSaveMetadata& requested_metadata,
                const std::string_view slot,
                const std::span<const SceneCell> cells,
                const WorldSaveOwners& owners,
                std::string& error) {
    error.clear();
    WorldSaveMetadata metadata = requested_metadata;
    metadata.format_version = world_save_format_version;
    metadata.chunk_edge = world_save_chunk_edge;
    metadata.cell_count = static_cast<std::uint64_t>(metadata.width) * metadata.height;
    if (metadata.width == 0u || metadata.height == 0u || cells.size() != metadata.cell_count ||
        static_cast<std::uint32_t>(metadata.scene) >= legacy_scene_count ||
        static_cast<std::uint32_t>(metadata.world_size) >
            static_cast<std::uint32_t>(WorldSizePreset::large)) {
        error = "world save metadata does not match the supplied cell span";
        return false;
    }
    if (owners.actor_present &&
        !actor_state_valid(owners.actor, metadata.width, metadata.height)) {
        error = "world save actor owner state is out of range";
        return false;
    }

    const auto chunk_columns = (metadata.width + metadata.chunk_edge - 1u) / metadata.chunk_edge;
    const auto chunk_rows = (metadata.height + metadata.chunk_edge - 1u) / metadata.chunk_edge;
    metadata.chunk_count = chunk_columns * chunk_rows;

    std::vector<std::uint8_t> body;
    body.reserve(static_cast<std::size_t>(metadata.chunk_count) * chunk_header_bytes +
                 cells.size() * sizeof(SceneCell) / 8u);
    for (std::uint32_t chunk_y = 0u; chunk_y < metadata.height; chunk_y += metadata.chunk_edge) {
        const auto chunk_height = (std::min)(metadata.chunk_edge, metadata.height - chunk_y);
        for (std::uint32_t chunk_x = 0u; chunk_x < metadata.width; chunk_x += metadata.chunk_edge) {
            const auto chunk_width = (std::min)(metadata.chunk_edge, metadata.width - chunk_x);
            const auto chunk_cells = chunk_width * chunk_height;
            std::vector<std::uint8_t> raw;
            raw.reserve(static_cast<std::size_t>(chunk_cells) * sizeof(SceneCell));
            std::vector<std::uint8_t> run_length;
            run_length.reserve(raw.capacity() / 4u);

            SceneCell current{};
            std::uint32_t run{};
            bool have_current = false;
            for (std::uint32_t local_y = 0u; local_y < chunk_height; ++local_y) {
                const auto row = static_cast<std::size_t>(chunk_y + local_y) * metadata.width + chunk_x;
                for (std::uint32_t local_x = 0u; local_x < chunk_width; ++local_x) {
                    const auto& cell = cells[row + local_x];
                    if (cell.material >= material_count) {
                        error = "world save contains an unknown material id";
                        return false;
                    }
                    append_cell(raw, cell);
                    if (!have_current) {
                        current = cell;
                        run = 1u;
                        have_current = true;
                    } else if (same_cell(current, cell) && run != (std::numeric_limits<std::uint32_t>::max)()) {
                        ++run;
                    } else {
                        append_u32(run_length, run);
                        append_cell(run_length, current);
                        current = cell;
                        run = 1u;
                    }
                }
            }
            if (have_current) {
                append_u32(run_length, run);
                append_cell(run_length, current);
            }
            const bool use_run_length = run_length.size() < raw.size();
            const auto& payload = use_run_length ? run_length : raw;
            append_u32(body, chunk_x);
            append_u32(body, chunk_y);
            append_u32(body, chunk_width);
            append_u32(body, chunk_height);
            append_u32(body, use_run_length ? encoding_run_length : encoding_raw);
            append_u32(body, chunk_cells);
            append_u32(body, static_cast<std::uint32_t>(payload.size()));
            append_u32(body, hash32(payload));
            body.insert(body.end(), payload.begin(), payload.end());
        }
    }

    metadata.owner_payload_bytes = 0u;
    if (owners.actor_present) {
        body.insert(body.end(), owner_magic.begin(), owner_magic.end());
        append_u32(body, world_save_actor_version);
        append_u32(body, world_save_actor_bytes);
        append_actor(body, owners.actor);
        metadata.owner_payload_bytes = owner_record_bytes;
    }

    metadata.payload_bytes = body.size();
    metadata.payload_hash = hash64(body);
    std::vector<std::uint8_t> header;
    header.reserve(save_header_bytes);
    header.insert(header.end(), save_magic.begin(), save_magic.end());
    append_u32(header, metadata.format_version);
    append_u32(header, save_header_bytes);
    append_u32(header, metadata.width);
    append_u32(header, metadata.height);
    append_u32(header, static_cast<std::uint32_t>(metadata.world_size));
    append_u32(header, static_cast<std::uint32_t>(metadata.scene));
    append_u32(header, metadata.chunk_edge);
    append_u32(header, metadata.chunk_count);
    append_u64(header, metadata.cell_count);
    append_u64(header, metadata.payload_bytes);
    append_u64(header, metadata.payload_hash);
    append_u64(header, metadata.owner_payload_bytes);
    if (header.size() != save_header_bytes) {
        error = "internal world-save header size mismatch";
        return false;
    }

    const auto directory = world_save_directory(
        application_directory, metadata.world_size, metadata.scene, slot);
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        error = "unable to create save directory: " + filesystem_error.message();
        return false;
    }
    const auto temporary = directory / "world.tmp";
    const auto destination = directory / "world.shw";
    const auto backup = directory / "world.bak";
    if (!write_bytes(temporary, header, body, error)) return false;
    if (!replace_atomically(temporary, destination, backup, metadata, error)) return false;
    return write_manifest(directory, metadata, slot, error);
}

bool save_world(const std::filesystem::path& application_directory,
                const WorldSaveMetadata& metadata,
                const std::string_view slot,
                const std::span<const SceneCell> cells,
                std::string& error) {
    return save_world(application_directory, metadata, slot, cells,
                      WorldSaveOwners{}, error);
}

bool load_world(const std::filesystem::path& application_directory,
                const WorldSizePreset expected_size,
                const std::uint32_t expected_width,
                const std::uint32_t expected_height,
                const Scene expected_scene,
                const std::string_view slot,
                const std::span<SceneCell> cells,
                WorldSaveOwners& owners,
                WorldSaveMetadata& metadata,
                std::string& error) {
    error.clear();
    const auto primary = world_save_path(
        application_directory, expected_size, expected_scene, slot);
    std::string primary_error;
    if (decode_world_file(primary, expected_size, expected_width, expected_height,
                          expected_scene, cells, owners, metadata, primary_error))
        return true;

    const auto backup = world_save_backup_path(
        application_directory, expected_size, expected_scene, slot);
    std::string backup_error;
    if (decode_world_file(backup, expected_size, expected_width, expected_height,
                          expected_scene, cells, owners, metadata, backup_error)) {
        error = "primary save failed (" + primary_error + "); loaded backup";
        return true;
    }
    error = primary_error + "; backup failed: " + backup_error;
    return false;
}

bool load_world(const std::filesystem::path& application_directory,
                const WorldSizePreset expected_size,
                const std::uint32_t expected_width,
                const std::uint32_t expected_height,
                const Scene expected_scene,
                const std::string_view slot,
                const std::span<SceneCell> cells,
                WorldSaveMetadata& metadata,
                std::string& error) {
    WorldSaveOwners ignored{};
    return load_world(application_directory, expected_size, expected_width,
                      expected_height, expected_scene, slot, cells, ignored,
                      metadata, error);
}

} // namespace sandhybrid
