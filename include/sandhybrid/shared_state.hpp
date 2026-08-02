#pragma once

#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"

#include <atomic>
#include <cstdint>

namespace sandhybrid {

inline constexpr std::uint32_t default_brush_radius = 2u;
inline constexpr std::uint32_t default_brush_shape = 1u; // square

struct SharedState final {
    std::atomic_bool quit{false};
    std::atomic_bool paused{false};
    std::atomic_bool single_step{false};
    std::atomic_bool reset{false};
    std::atomic_bool save_scene_image{false};
    std::atomic_bool load_scene_image{false};
    std::atomic_bool fill_region{false};

    std::atomic_uint32_t window_width{1280};
    std::atomic_uint32_t window_height{720};
    std::atomic_bool resized{false};

    std::atomic_int mouse_x{0};
    std::atomic_int mouse_y{0};
    std::atomic_bool primary_down{false};
    std::atomic_bool secondary_down{false};
    std::atomic_bool inspect_material{false};
    std::atomic_bool debug_visualization{false};
    std::atomic_bool camera_controls{false};

    std::atomic_uint32_t selected_material{static_cast<std::uint32_t>(Material::sand)};
    std::atomic_uint32_t selected_group{static_cast<std::uint32_t>(MaterialGroup::ground)};
    std::atomic_uint32_t hovered_material{material_count};
    std::atomic_uint32_t hovered_group{material_group_count};
    std::atomic_uint32_t selected_scene{static_cast<std::uint32_t>(Scene::ecosystem)};
    std::atomic_uint32_t brush_radius{default_brush_radius};
    std::atomic_uint32_t brush_shape{default_brush_shape};
    std::atomic_uint32_t placement_mode{0}; // 0 = cells, 1 = aligned 8x8 tile
    std::atomic_uint32_t camera_zoom{4};
    std::atomic_int camera_center_x{1280};
    std::atomic_int camera_center_y{1260};
    std::atomic_uint32_t section_worker_count{0};
    std::atomic_uint32_t active_section_count{0};
    std::atomic_uint32_t active_scope_mode{1}; // 1 = 17 map-area starburst, 2 = camera-visible fallback
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

} // namespace sandhybrid
