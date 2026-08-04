#pragma once

#include "sandhybrid/camera_policy.hpp"
#include "sandhybrid/scene.hpp"
#include "sandhybrid/scene_image.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sandhybrid {

inline constexpr std::uint32_t world_save_format_version = 1u;
inline constexpr std::uint32_t world_save_chunk_edge = 64u;
inline constexpr std::string_view default_world_save_slot = "quick";

struct WorldSaveMetadata final {
    std::uint32_t format_version{world_save_format_version};
    WorldSizePreset world_size{WorldSizePreset::large};
    std::uint32_t width{};
    std::uint32_t height{};
    Scene scene{Scene::sandbox};
    std::uint32_t chunk_edge{world_save_chunk_edge};
    std::uint32_t chunk_count{};
    std::uint64_t cell_count{};
    std::uint64_t payload_bytes{};
    std::uint64_t payload_hash{};
};

[[nodiscard]] constexpr std::string_view world_size_name(
    const WorldSizePreset preset) noexcept {
    switch (preset) {
    case WorldSizePreset::compact: return "compact";
    case WorldSizePreset::standard: return "standard";
    case WorldSizePreset::large: return "large";
    }
    return "large";
}

[[nodiscard]] std::optional<WorldSizePreset> parse_world_size(std::string_view value) noexcept;
[[nodiscard]] std::optional<WorldSizePreset> world_size_from_dimensions(
    std::uint32_t width, std::uint32_t height) noexcept;
[[nodiscard]] std::string normalize_world_slot(std::string_view slot);
[[nodiscard]] std::string_view scene_save_name(Scene scene) noexcept;

[[nodiscard]] std::filesystem::path world_save_directory(
    const std::filesystem::path& application_directory,
    WorldSizePreset preset,
    Scene scene,
    std::string_view slot = default_world_save_slot);
[[nodiscard]] std::filesystem::path world_save_path(
    const std::filesystem::path& application_directory,
    WorldSizePreset preset,
    Scene scene,
    std::string_view slot = default_world_save_slot);
[[nodiscard]] std::filesystem::path world_save_backup_path(
    const std::filesystem::path& application_directory,
    WorldSizePreset preset,
    Scene scene,
    std::string_view slot = default_world_save_slot);

bool read_world_save_metadata(const std::filesystem::path& path,
                              WorldSaveMetadata& metadata,
                              std::string& error);

bool save_world(const std::filesystem::path& application_directory,
                const WorldSaveMetadata& metadata,
                std::string_view slot,
                std::span<const SceneCell> cells,
                std::string& error);

bool load_world(const std::filesystem::path& application_directory,
                WorldSizePreset expected_size,
                std::uint32_t expected_width,
                std::uint32_t expected_height,
                Scene expected_scene,
                std::string_view slot,
                std::span<SceneCell> cells,
                WorldSaveMetadata& metadata,
                std::string& error);

} // namespace sandhybrid
