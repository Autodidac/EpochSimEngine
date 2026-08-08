#pragma once

#include "sandhybrid/scene.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sandhybrid {

struct SceneCell final {
    std::uint32_t material{};
    std::uint32_t age{};
    std::int32_t temperature{20};
    std::uint32_t aux{};
};
static_assert(sizeof(SceneCell) == 16u);

[[nodiscard]] std::filesystem::path scene_image_path(
    const std::filesystem::path& directory, Scene scene);
[[nodiscard]] bool scene_image_exists(const std::filesystem::path& directory, Scene scene);

bool load_scene_ppm(const std::filesystem::path& path,
                    const Scene scene,
                    std::uint32_t width,
                    std::uint32_t height,
                    std::span<SceneCell> cells,
                    std::string& error);

bool save_scene_ppm(const std::filesystem::path& path,
                    std::uint32_t width,
                    std::uint32_t height,
                    std::span<const SceneCell> cells,
                    std::string& error);

void normalize_pre_pr19_hives(std::vector<std::uint32_t>& materials,
                             std::uint32_t width,
                             std::uint32_t height,
                             std::uint32_t scene_origin_x,
                             std::uint32_t scene_origin_y,
                             Scene scene);

bool write_scene_material_key(const std::filesystem::path& directory, std::string& error);

} // namespace sandhybrid
