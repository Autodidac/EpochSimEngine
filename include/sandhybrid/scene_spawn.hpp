#pragma once

#include "sandhybrid/scene.hpp"
#include "sandhybrid/world_layout.hpp"

#include <cstdint>

namespace sandhybrid {

struct SceneSpawn final {
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t ammunition{};
    bool enabled{};

    friend constexpr bool operator==(const SceneSpawn&, const SceneSpawn&) = default;
};

[[nodiscard]] constexpr SceneSpawn scene_local_spawn(const Scene scene) noexcept {
    switch (scene) {
    case Scene::sandbox: return {80, 319, 12u, true};
    case Scene::blank: return {80, 319, 12u, true};
    case Scene::volcano: return {80, 343, 12u, true};
    case Scene::waterworks: return {40, 311, 12u, true};
    case Scene::ecosystem: return {80, 295, 12u, true};
    case Scene::engineering_lab: return {200, 335, 18u, true};
    case Scene::gold_mine: return {78, 127, 18u, true};
    case Scene::demolition: return {80, 327, 12u, true};
    case Scene::frontier_base: return {168, 207, 24u, true};
    case Scene::count: return {};
    }
    return {};
}

[[nodiscard]] constexpr SceneSpawn scene_world_spawn(
    const Scene scene,
    const std::uint32_t world_width,
    const std::uint32_t world_height) noexcept {
    auto spawn = scene_local_spawn(scene);
    if (!spawn.enabled) return spawn;
    spawn.x += static_cast<std::int32_t>(authored_scene_origin_x(world_width));
    spawn.y += static_cast<std::int32_t>(authored_scene_origin_y(world_height));
    return spawn;
}

} // namespace sandhybrid
