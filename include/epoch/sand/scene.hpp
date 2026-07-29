#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace epoch::sand {

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

inline constexpr auto scene_count = static_cast<std::uint32_t>(Scene::count);

inline constexpr std::array<std::string_view, scene_count> scene_names{
    "Sandbox", "Blank", "Volcano", "Waterworks", "Ecosystem", "Engineering lab",
    "Gold mine", "Demolition", "Frontier base"
};

[[nodiscard]] constexpr std::string_view scene_name(const Scene scene) noexcept {
    const auto index = static_cast<std::uint32_t>(scene);
    return index < scene_names.size() ? scene_names[index] : "Unknown";
}

[[nodiscard]] constexpr Scene next_scene(const Scene scene) noexcept {
    return static_cast<Scene>((static_cast<std::uint32_t>(scene) + 1u) % scene_count);
}

[[nodiscard]] constexpr Scene previous_scene(const Scene scene) noexcept {
    return static_cast<Scene>((static_cast<std::uint32_t>(scene) + scene_count - 1u) % scene_count);
}

[[nodiscard]] constexpr bool scene_has_character(const Scene scene) noexcept {
    return scene == Scene::gold_mine || scene == Scene::demolition || scene == Scene::frontier_base;
}

} // namespace epoch::sand
