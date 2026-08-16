#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace sandhybrid {

enum class Scene : std::uint32_t {
    sandbox = 0,
    blank,
    volcano,
    waterworks,
    ecosystem,
    engineering_lab,
    gold_mine,
    demolition,
    frontier_base,
    count
};

inline constexpr auto legacy_scene_count = static_cast<std::uint32_t>(Scene::count);
inline constexpr std::uint32_t scene_count = 1u;
inline constexpr Scene world_scene = Scene::sandbox;

inline constexpr std::array<std::string_view, legacy_scene_count> scene_names{
    "Sandbox", "Blank", "Volcano", "Waterworks", "Ecosystem", "Engineering lab",
    "Gold Mine", "Demolition", "Frontier base"
};

[[nodiscard]] constexpr std::string_view scene_name(const Scene scene) noexcept {
    const auto index = static_cast<std::uint32_t>(scene);
    return index < scene_names.size() ? scene_names[index] : "Unknown";
}

[[nodiscard]] constexpr Scene next_scene(const Scene) noexcept {
    return world_scene;
}

[[nodiscard]] constexpr Scene previous_scene(const Scene) noexcept {
    return world_scene;
}

[[nodiscard]] constexpr bool scene_has_character(const Scene scene) noexcept {
    return scene == world_scene;
}

[[nodiscard]] constexpr bool valid_legacy_scene(const Scene scene) noexcept {
    return static_cast<std::uint32_t>(scene) < legacy_scene_count;
}

} // namespace sandhybrid
