#pragma once

#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"

#include <atomic>
#include <cstdint>

namespace epoch::sand {

struct SharedState final {
    std::atomic_bool quit{false};
    std::atomic_bool paused{false};
    std::atomic_bool single_step{false};
    std::atomic_bool reset{false};
    std::atomic_bool save_scene_image{false};
    std::atomic_bool load_scene_image{false};

    std::atomic_uint32_t window_width{1280};
    std::atomic_uint32_t window_height{720};
    std::atomic_bool resized{false};

    std::atomic_int mouse_x{0};
    std::atomic_int mouse_y{0};
    std::atomic_bool primary_down{false};
    std::atomic_bool secondary_down{false};
    std::atomic_bool inspect_material{false};
    std::atomic_bool debug_visualization{false};

    std::atomic_uint32_t selected_material{static_cast<std::uint32_t>(Material::sand)};
    std::atomic_uint32_t selected_group{static_cast<std::uint32_t>(MaterialGroup::ground)};
    std::atomic_uint32_t hovered_material{material_count};
    std::atomic_uint32_t hovered_group{material_group_count};
    std::atomic_uint32_t selected_scene{static_cast<std::uint32_t>(Scene::ecosystem)};
    std::atomic_uint32_t brush_radius{6};
    std::atomic_uint32_t brush_shape{0};
    std::atomic_uint32_t camera_zoom{1};
    std::atomic_int camera_center_x{320};
    std::atomic_int camera_center_y{180};
    std::atomic_uint32_t steps_per_frame{1};
    std::atomic_uint32_t frames_per_second{0};

    std::atomic_int move_x{0};
    std::atomic_int move_y{0};
    std::atomic_bool jump{false};
    std::atomic_bool mining_mode{false};
    std::atomic_bool fire_tool{false};
    std::atomic_bool fire_tool_pressed{false};
    std::atomic_bool deposit_resource{false};
    std::atomic_bool deposit_resource_pressed{false};
};

} // namespace epoch::sand
