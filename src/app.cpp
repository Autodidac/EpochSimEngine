#include "sandhybrid/app.hpp"

#include "sandhybrid/input_routing.hpp"
#include "sandhybrid/world_layout.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"
#include "sandhybrid/section_scheduler.hpp"
#include "sandhybrid/shared_state.hpp"
#include "sandhybrid/ui_layout.hpp"
#include "sandhybrid/vulkan_renderer.hpp"
#include "sandhybrid/window.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <thread>
#include <string>
#include <utility>

namespace sandhybrid {

namespace {

struct CameraView final {
    std::uint32_t origin_x{};
    std::uint32_t origin_y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

[[nodiscard]] CameraView camera_view_from(const SimulationConfig& config,
                                     const std::uint32_t requested_zoom,
                                     const int requested_center_x,
                                     const int requested_center_y,
                                     const bool map_view) noexcept {
    const auto zoom = map_view
        ? std::clamp(requested_zoom, map_zoom_min, map_zoom_max)
        : std::clamp(requested_zoom, camera_zoom_min, camera_zoom_max);
    const auto visible_width = (std::min)(config.grid_width, map_view
        ? map_view_width(config.grid_width, zoom) : camera_view_width(zoom));
    const auto visible_height = (std::min)(config.grid_height, map_view
        ? map_view_height(config.grid_height, zoom) : camera_view_height(zoom));
    const auto center_x = std::clamp(requested_center_x,
                                     0, static_cast<int>(config.grid_width - 1u));
    const auto center_y = std::clamp(requested_center_y,
                                     0, static_cast<int>(config.grid_height - 1u));
    const auto max_x = config.grid_width - visible_width;
    const auto max_y = config.grid_height - visible_height;
    const auto origin_x = static_cast<std::uint32_t>(std::clamp(
        center_x - static_cast<int>(visible_width / 2u), 0, static_cast<int>(max_x)));
    const auto origin_y = static_cast<std::uint32_t>(std::clamp(
        center_y - static_cast<int>(visible_height / 2u), 0, static_cast<int>(max_y)));
    return {origin_x, origin_y, visible_width, visible_height};
}

[[nodiscard]] CameraView camera_view(const SharedState& state, const SimulationConfig& config,
                                     const bool map_view) noexcept {
    return camera_view_from(
        config,
        map_view ? state.map_zoom.load(std::memory_order_relaxed)
                 : state.camera_zoom.load(std::memory_order_relaxed),
        map_view ? state.map_center_x.load(std::memory_order_relaxed)
                 : state.camera_center_x.load(std::memory_order_relaxed),
        map_view ? state.map_center_y.load(std::memory_order_relaxed)
                 : state.camera_center_y.load(std::memory_order_relaxed),
        map_view);
}

[[nodiscard]] std::pair<std::int32_t, std::int32_t> pointer_grid(
    const SharedState& state, const SimulationConfig& config,
    const ui::SimulationViewport& viewport, const std::int32_t mouse_x,
    const std::int32_t mouse_y, const bool map_view) noexcept {
    const auto view = camera_view(state, config, map_view);
    const auto viewport_width = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(mouse_x - static_cast<int>(viewport.rect.position.x),
                                    0, static_cast<int>(viewport_width - 1u));
    const auto local_y = std::clamp(mouse_y - static_cast<int>(viewport.rect.position.y),
                                    0, static_cast<int>(viewport_height - 1u));
    return {
        static_cast<std::int32_t>(view.origin_x +
            static_cast<std::uint64_t>(local_x) * view.width / viewport_width),
        static_cast<std::int32_t>(view.origin_y +
            static_cast<std::uint64_t>(local_y) * view.height / viewport_height),
    };
}

struct DesignerView final {
    std::uint32_t origin_x{};
    std::uint32_t origin_y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

[[nodiscard]] DesignerView designer_view(const SharedState& state) noexcept {
    const auto zoom = std::clamp(state.designer_zoom.load(std::memory_order_relaxed), 1u, 4u);
    const auto width = (std::max)(8u, designer_grid_columns / zoom);
    const auto height = (std::max)(8u, designer_grid_rows / zoom);
    return {(designer_grid_columns - width) / 2u,
            (designer_grid_rows - height) / 2u, width, height};
}

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> pointer_designer_grid(
    const SharedState& state, const ui::SimulationViewport& viewport,
    const std::int32_t mouse_x, const std::int32_t mouse_y) noexcept {
    const auto view = designer_view(state);
    const auto viewport_width = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(mouse_x - static_cast<int>(viewport.rect.position.x),
                                    0, static_cast<int>(viewport_width - 1u));
    const auto local_y = std::clamp(mouse_y - static_cast<int>(viewport.rect.position.y),
                                    0, static_cast<int>(viewport_height - 1u));
    return {
        (std::min)(designer_grid_columns - 1u, view.origin_x +
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(local_x) * view.width / viewport_width)),
        (std::min)(designer_grid_rows - 1u, view.origin_y +
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(local_y) * view.height / viewport_height)),
    };
}

void paint_designer_grid(SharedState& state, const ui::SimulationViewport& viewport,
                         const std::int32_t mouse_x, const std::int32_t mouse_y) noexcept {
    if (state.designer_mode.load(std::memory_order_relaxed) != 0u) return;
    const auto [center_x, center_y] = pointer_designer_grid(state, viewport, mouse_x, mouse_y);
    const auto material = state.designer_selected_material.load(std::memory_order_relaxed) % material_count;
    const auto placement = state.designer_placement_mode.load(std::memory_order_relaxed) & 1u;
    if (placement != 0u) {
        const auto start_x = center_x & ~7u;
        const auto start_y = center_y & ~7u;
        for (std::uint32_t y = start_y; y < (std::min)(start_y + 8u, designer_grid_rows); ++y)
            for (std::uint32_t x = start_x; x < (std::min)(start_x + 8u, designer_grid_columns); ++x)
                state.designer_cells[y * designer_grid_columns + x].store(material, std::memory_order_relaxed);
        state.designer_dirty.store(true, std::memory_order_release);
        return;
    }

    const auto radius = std::clamp(state.designer_brush_radius.load(std::memory_order_relaxed), 1u, 12u);
    const auto shape = state.designer_brush_shape.load(std::memory_order_relaxed) & 3u;
    const auto extent = static_cast<std::int32_t>(radius - 1u);
    for (std::int32_t dy = -extent; dy <= extent; ++dy) {
        for (std::int32_t dx = -extent; dx <= extent; ++dx) {
            bool include = true;
            if (shape == 0u) include = dx * dx + dy * dy <= extent * extent;
            else if (shape == 2u) include = dy == 0;
            else if (shape == 3u) include = dx == 0;
            if (!include) continue;
            const auto x = static_cast<std::int32_t>(center_x) + dx;
            const auto y = static_cast<std::int32_t>(center_y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(designer_grid_columns) ||
                y >= static_cast<std::int32_t>(designer_grid_rows)) continue;
            state.designer_cells[static_cast<std::uint32_t>(y) * designer_grid_columns +
                                 static_cast<std::uint32_t>(x)].store(material, std::memory_order_relaxed);
        }
    }
    state.designer_dirty.store(true, std::memory_order_release);
}

void zoom_at_pointer(SharedState& state, const SimulationConfig& config,
                     const ui::SimulationViewport& viewport, const std::int32_t mouse_x,
                     const std::int32_t mouse_y, const int delta,
                     const bool map_view) noexcept {
    auto& zoom_state = map_view ? state.map_zoom : state.camera_zoom;
    auto& center_x_state = map_view ? state.map_center_x : state.camera_center_x;
    auto& center_y_state = map_view ? state.map_center_y : state.camera_center_y;
    const auto minimum_zoom = map_view ? map_zoom_min : camera_zoom_min;
    const auto maximum_zoom = map_view ? map_zoom_max : camera_zoom_max;
    const auto old_zoom = std::clamp(zoom_state.load(std::memory_order_relaxed),
                                     minimum_zoom, maximum_zoom);
    const auto new_zoom = static_cast<std::uint32_t>(std::clamp(
        static_cast<int>(old_zoom) + delta,
        static_cast<int>(minimum_zoom), static_cast<int>(maximum_zoom)));
    if (new_zoom == old_zoom) return;
    const auto [target_x, target_y] = pointer_grid(
        state, config, viewport, mouse_x, mouse_y, map_view);
    const auto viewport_width = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.x));
    const auto viewport_height = (std::max)(1u, static_cast<std::uint32_t>(viewport.rect.size.y));
    const auto local_x = std::clamp(mouse_x - static_cast<int>(viewport.rect.position.x),
                                    0, static_cast<int>(viewport_width - 1u));
    const auto local_y = std::clamp(mouse_y - static_cast<int>(viewport.rect.position.y),
                                    0, static_cast<int>(viewport_height - 1u));
    const auto new_width = (std::min)(config.grid_width, map_view
        ? map_view_width(config.grid_width, new_zoom) : camera_view_width(new_zoom));
    const auto new_height = (std::min)(config.grid_height, map_view
        ? map_view_height(config.grid_height, new_zoom) : camera_view_height(new_zoom));
    const auto desired_origin_x = target_x - static_cast<int>(
        static_cast<std::uint64_t>(local_x) * new_width / viewport_width);
    const auto desired_origin_y = target_y - static_cast<int>(
        static_cast<std::uint64_t>(local_y) * new_height / viewport_height);
    const auto origin_x = std::clamp(
        desired_origin_x, 0, static_cast<int>(config.grid_width - new_width));
    const auto origin_y = std::clamp(
        desired_origin_y, 0, static_cast<int>(config.grid_height - new_height));
    zoom_state.store(new_zoom, std::memory_order_relaxed);
    center_x_state.store(origin_x + static_cast<int>(new_width / 2u),
                         std::memory_order_relaxed);
    center_y_state.store(origin_y + static_cast<int>(new_height / 2u),
                         std::memory_order_relaxed);
}

void set_camera_center_clamped(std::atomic_int& center_x_state,
                               std::atomic_int& center_y_state,
                               const SimulationConfig& config, const CameraView& view,
                               const int center_x, const int center_y) noexcept {
    const auto half_width = static_cast<int>(view.width / 2u);
    const auto half_height = static_cast<int>(view.height / 2u);
    const auto min_x = half_width;
    const auto min_y = half_height;
    const auto max_x = static_cast<int>(config.grid_width) -
                       static_cast<int>(view.width - view.width / 2u);
    const auto max_y = static_cast<int>(config.grid_height) -
                       static_cast<int>(view.height - view.height / 2u);
    center_x_state.store(std::clamp(center_x, min_x, max_x),
                         std::memory_order_relaxed);
    center_y_state.store(std::clamp(center_y, min_y, max_y),
                         std::memory_order_relaxed);
}

void pan_camera_cells(SharedState& state, const SimulationConfig& config,
                      const int delta_x, const int delta_y,
                      const bool map_view) noexcept {
    if (delta_x == 0 && delta_y == 0) return;
    auto& center_x_state = map_view ? state.map_center_x : state.camera_center_x;
    auto& center_y_state = map_view ? state.map_center_y : state.camera_center_y;
    const auto view = camera_view(state, config, map_view);
    set_camera_center_clamped(
        center_x_state, center_y_state, config, view,
        center_x_state.load(std::memory_order_relaxed) + delta_x,
        center_y_state.load(std::memory_order_relaxed) + delta_y);
}

void reset_camera_to_zero(SharedState& state, const SimulationConfig& config) noexcept {
    const auto visible_width = (std::min)(config.grid_width,
        camera_view_width(camera_zoom_default));
    const auto visible_height = (std::min)(config.grid_height,
        camera_view_height(camera_zoom_default));
    const auto map_origin_x = authored_scene_origin_x(config.grid_width);
    const auto map_origin_y = authored_scene_origin_y(config.grid_height);
    state.camera_zoom.store(camera_zoom_default, std::memory_order_relaxed);
    const CameraView view{map_origin_x, map_origin_y, visible_width, visible_height};
    set_camera_center_clamped(state.camera_center_x, state.camera_center_y, config, view,
                              static_cast<int>(map_origin_x + visible_width / 2u),
                              static_cast<int>(map_origin_y + visible_height / 2u));
}

void reset_map_view(SharedState& state, const SimulationConfig& config) noexcept {
    state.map_zoom.store(map_zoom_default, std::memory_order_relaxed);
    const auto view = camera_view_from(
        config, map_zoom_default, static_cast<int>(config.grid_width / 2u),
        static_cast<int>(config.grid_height / 2u), true);
    set_camera_center_clamped(state.map_center_x, state.map_center_y, config, view,
                              static_cast<int>(config.grid_width / 2u),
                              static_cast<int>(config.grid_height / 2u));
}

} // namespace

int run_application(const ApplicationOptions& options) {
    std::fprintf(stderr, "[SandHybrid] Creating native window...\n");
    NativeWindow window{"SandHybrid", 1280, 720};
    window.show_startup_message("Compiling Shaders...");
    std::fprintf(stderr, "[SandHybrid] Native window created.\n");
    SharedState shared_state{};
    const auto world = world_dimensions(options.world_size);
    const SimulationConfig simulation_config{
        .grid_width = world.width,
        .grid_height = world.height,
        .frames_in_flight = 2u,
        .max_frames_per_second = 120u,
        .world_size = options.world_size,
    };
    reset_camera_to_zero(shared_state, simulation_config);
    reset_map_view(shared_state, simulation_config);
    std::atomic_bool renderer_ready{false};

    std::exception_ptr render_error;
    std::mutex render_error_mutex;

    // Keep driver initialization off the native event thread. Startup remains
    // responsive, while stage-by-stage console logging identifies any driver or
    // pipeline failure instead of leaving a silent frozen window.
    std::atomic_bool stop_renderer{false};
    std::thread render_thread([&] {
        try {
            std::fprintf(stderr, "[SandHybrid] Vulkan initialization started.\n");
            VulkanRenderer renderer{window, simulation_config, options.save_slot};
            renderer_ready.store(true, std::memory_order_release);
            std::fprintf(stderr, "[SandHybrid] Vulkan renderer ready.\n");
            renderer.run(stop_renderer, shared_state);
        } catch (...) {
            {
                const std::scoped_lock lock{render_error_mutex};
                render_error = std::current_exception();
            }
            shared_state.quit.store(true, std::memory_order_release);
        }
    });

    WindowInput input{};
    bool pan_dragging = false;
    bool pan_drag_map = false;
    std::int32_t pan_last_x = 0;
    std::int32_t pan_last_y = 0;
    std::int64_t pan_remainder_x = 0;
    std::int64_t pan_remainder_y = 0;
    double camera_key_remainder_x = 0.0;
    double camera_key_remainder_y = 0.0;
    auto last_camera_update = std::chrono::steady_clock::now();
    bool ready_title_applied = false;
    while (!shared_state.quit.load(std::memory_order_acquire) && window.poll(input)) {
        if (!ready_title_applied && renderer_ready.load(std::memory_order_acquire)) {
            window.show_startup_message("");
            window.set_title("SandHybrid - " +
      std::string{world_size_name(options.world_size)});
            ready_title_applied = true;
        }
        shared_state.window_width.store(input.width, std::memory_order_relaxed);
        shared_state.window_height.store(input.height, std::memory_order_relaxed);
        shared_state.mouse_x.store(input.mouse_x, std::memory_order_relaxed);
        shared_state.mouse_y.store(input.mouse_y, std::memory_order_relaxed);

        if (input.resized) {
            shared_state.resized.store(true, std::memory_order_release);
        }
        if (input.toggle_pause) {
            const auto paused = shared_state.paused.load(std::memory_order_relaxed);
            shared_state.paused.store(!paused, std::memory_order_release);
        }
        if (input.single_step) {
            shared_state.single_step.store(true, std::memory_order_release);
        }
        shared_state.inspect_material.store(input.inspect_material, std::memory_order_relaxed);
        if (input.toggle_debug) {
            const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);
            shared_state.debug_visualization.store(!debug, std::memory_order_release);
        }
        if (input.toggle_map) {
            const bool map = shared_state.map_view.load(std::memory_order_relaxed);
            shared_state.map_view.store(!map, std::memory_order_release);
        }

        auto scene = static_cast<Scene>(
            shared_state.selected_scene.load(std::memory_order_relaxed) % scene_count);
        if (input.toggle_mining) {
            const bool current_mining = shared_state.mining_mode.load(std::memory_order_relaxed);
            shared_state.mining_mode.store(!current_mining, std::memory_order_release);
        }

        if (input.next_scene) {
            scene = next_scene(scene);
            shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
            shared_state.reset.store(true, std::memory_order_release);
            shared_state.mining_mode.store(scene_has_character(scene),
                                           std::memory_order_release);
            reset_camera_to_zero(shared_state, simulation_config);
            reset_map_view(shared_state, simulation_config);
        } else if (input.previous_scene) {
            scene = previous_scene(scene);
            shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
            shared_state.reset.store(true, std::memory_order_release);
            shared_state.mining_mode.store(scene_has_character(scene),
                                           std::memory_order_release);
            reset_camera_to_zero(shared_state, simulation_config);
            reset_map_view(shared_state, simulation_config);
        } else if (input.reset) {
            shared_state.reset.store(true, std::memory_order_release);
            reset_camera_to_zero(shared_state, simulation_config);
            reset_map_view(shared_state, simulation_config);
        }
        if (input.reset_camera) reset_camera_to_zero(shared_state, simulation_config);
        if (input.save_scene) shared_state.save_scene_image.store(true, std::memory_order_release);
        if (input.load_scene) shared_state.load_scene_image.store(true, std::memory_order_release);

        const auto layout = ui::make_layout(input.width, input.height);
        const bool map_view_enabled =
            shared_state.map_view.load(std::memory_order_relaxed);
        const auto visible_view = camera_view(shared_state, simulation_config, false);
        const auto map_view = camera_view(shared_state, simulation_config, true);
        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, visible_view.width, visible_view.height);
        const auto map_overlay_viewport = ui::make_map_overlay_viewport(
            layout, map_view.width, map_view.height);
        const epochengine::gui_lib::Vec2 pointer{
            static_cast<float>(input.mouse_x),
            static_cast<float>(input.mouse_y),
        };
        const bool primary_pressed = input.primary_pressed;
        const auto selected_workspace = shared_state.selected_workspace.load(
            std::memory_order_relaxed) % ui::workspace_tab_count;
        const bool editor_workspace = selected_workspace == 1u;
        const bool designer_workspace = selected_workspace == 3u;
        const bool material_workspace = editor_workspace || designer_workspace;
        const bool over_simulation = epochengine::gui_lib::contains(
            simulation_viewport.rect, pointer);
        const bool over_map = map_view_enabled && epochengine::gui_lib::contains(
            map_overlay_viewport.rect, pointer);
        const bool over_world = over_simulation && !over_map;
        if (input.wheel_delta != 0) {
            if (over_map) {
                zoom_at_pointer(shared_state, simulation_config, map_overlay_viewport,
                                input.mouse_x, input.mouse_y, input.wheel_delta, true);
            } else if (over_world && designer_workspace) {
                const auto zoom = static_cast<int>(
                    shared_state.designer_zoom.load(std::memory_order_relaxed));
                shared_state.designer_zoom.store(static_cast<std::uint32_t>(
                    std::clamp(zoom + input.wheel_delta, 1, 4)), std::memory_order_relaxed);
            } else if (over_world) {
                zoom_at_pointer(shared_state, simulation_config, simulation_viewport,
                                input.mouse_x, input.mouse_y, input.wheel_delta, false);
            }
        }

        const bool scene_player_present = scene_has_character(scene);
        const bool camera_controls_enabled =
            shared_state.camera_controls.load(std::memory_order_relaxed);
        const bool player_controls = !designer_workspace &&
            player_wasd_enabled(scene_player_present, camera_controls_enabled);
        const bool pan_button_down = input.secondary_down;
        if (pan_button_down && (((over_simulation && (!designer_workspace || over_map))) || pan_dragging)) {
            if (!pan_dragging) {
                pan_dragging = true;
                pan_drag_map = over_map;
                pan_last_x = input.mouse_x;
                pan_last_y = input.mouse_y;
                pan_remainder_x = 0;
                pan_remainder_y = 0;
            } else {
                const auto drag_view = camera_view(
                    shared_state, simulation_config, pan_drag_map);
                const auto& drag_viewport = pan_drag_map
                    ? map_overlay_viewport : simulation_viewport;
                const auto viewport_width = (std::max)(
                    static_cast<std::int64_t>(drag_viewport.rect.size.x), std::int64_t{1});
                const auto viewport_height = (std::max)(
                    static_cast<std::int64_t>(drag_viewport.rect.size.y), std::int64_t{1});
                const auto dx = input.mouse_x - pan_last_x;
                const auto dy = input.mouse_y - pan_last_y;
                pan_last_x = input.mouse_x;
                pan_last_y = input.mouse_y;

                pan_remainder_x += -static_cast<std::int64_t>(dx) * drag_view.width;
                pan_remainder_y += -static_cast<std::int64_t>(dy) * drag_view.height;
                const auto shift_x = pan_remainder_x / viewport_width;
                const auto shift_y = pan_remainder_y / viewport_height;
                pan_remainder_x %= viewport_width;
                pan_remainder_y %= viewport_height;

                pan_camera_cells(shared_state, simulation_config,
                                 static_cast<int>(shift_x), static_cast<int>(shift_y),
                                 pan_drag_map);
            }
        } else {
            pan_dragging = false;
            pan_drag_map = false;
            pan_remainder_x = 0;
            pan_remainder_y = 0;
        }

        const auto camera_now = std::chrono::steady_clock::now();
        const auto camera_seconds = std::clamp(
            std::chrono::duration<double>(camera_now - last_camera_update).count(), 0.0, 0.05);
        last_camera_update = camera_now;
        const auto directional_input = route_directional_input(
            player_controls,
            input.move_left,
            input.move_right,
            input.move_up,
            input.move_down);
        const bool edge_targets_map = pan_button_down && pan_dragging && pan_drag_map;
        int camera_direction_x = designer_workspace ? 0 :
            (edge_targets_map ? 0 : directional_input.camera_x);
        int camera_direction_y = designer_workspace ? 0 :
            (edge_targets_map ? 0 : directional_input.camera_y);
        if (pan_button_down) {
            const auto& edge_viewport = edge_targets_map
                ? map_overlay_viewport : simulation_viewport;
            const auto edge = edge_pan_direction(
                input.mouse_x, input.mouse_y,
                static_cast<std::int32_t>(edge_viewport.rect.position.x),
                static_cast<std::int32_t>(edge_viewport.rect.position.y),
                static_cast<std::int32_t>(edge_viewport.rect.size.x),
                static_cast<std::int32_t>(edge_viewport.rect.size.y));
            camera_direction_x = std::clamp(camera_direction_x + edge.x, -1, 1);
            camera_direction_y = std::clamp(camera_direction_y + edge.y, -1, 1);
        }
        camera_direction_x = std::clamp(camera_direction_x, -1, 1);
        camera_direction_y = std::clamp(camera_direction_y, -1, 1);
        if (camera_direction_x != 0 || camera_direction_y != 0) {
            const auto active_view = camera_view(
                shared_state, simulation_config, edge_targets_map);
            const double cells_per_second = (std::max)(
                120.0, static_cast<double>((std::max)(active_view.width, active_view.height)) * 0.85);
            camera_key_remainder_x += static_cast<double>(camera_direction_x) *
                                      cells_per_second * camera_seconds;
            camera_key_remainder_y += static_cast<double>(camera_direction_y) *
                                      cells_per_second * camera_seconds;
            const int camera_shift_x = static_cast<int>(std::trunc(camera_key_remainder_x));
            const int camera_shift_y = static_cast<int>(std::trunc(camera_key_remainder_y));
            camera_key_remainder_x -= static_cast<double>(camera_shift_x);
            camera_key_remainder_y -= static_cast<double>(camera_shift_y);
            pan_camera_cells(shared_state, simulation_config, camera_shift_x, camera_shift_y,
                             edge_targets_map);
        } else {
            camera_key_remainder_x = 0.0;
            camera_key_remainder_y = 0.0;
        }

        const auto center_section = SectionCoordinate{
            shared_state.camera_center_x.load(std::memory_order_relaxed) / active_region_width_cells,
            shared_state.camera_center_y.load(std::memory_order_relaxed) / active_region_height_cells,
        };
        const auto section_schedule = make_section_schedule(
            center_section,
            (simulation_config.grid_width +
             static_cast<std::uint32_t>(active_region_width_cells) - 1u) /
                static_cast<std::uint32_t>(active_region_width_cells),
            (simulation_config.grid_height +
             static_cast<std::uint32_t>(active_region_height_cells) - 1u) /
                static_cast<std::uint32_t>(active_region_height_cells),
            std::thread::hardware_concurrency());
        shared_state.section_worker_count.store(
            static_cast<std::uint32_t>(section_schedule.worker_count), std::memory_order_relaxed);
        shared_state.active_section_count.store(
            static_cast<std::uint32_t>(section_schedule.assignment_count), std::memory_order_relaxed);
        shared_state.active_window_origin_x.store(
            section_schedule.origin.x, std::memory_order_relaxed);
        shared_state.active_window_origin_y.store(
            section_schedule.origin.y, std::memory_order_relaxed);
        shared_state.active_scope_mode.store(1u, std::memory_order_relaxed);

        const auto adjust_zoom_centered = [&shared_state](const int delta) {
            const auto current = static_cast<int>(
                shared_state.camera_zoom.load(std::memory_order_relaxed));
            shared_state.camera_zoom.store(static_cast<std::uint32_t>(std::clamp(
                current + delta, static_cast<int>(camera_zoom_min),
                static_cast<int>(camera_zoom_max))), std::memory_order_relaxed);
        };

        auto& active_selected_group = designer_workspace
            ? shared_state.designer_selected_group : shared_state.selected_group;
        auto& active_selected_material = designer_workspace
            ? shared_state.designer_selected_material : shared_state.selected_material;
        auto& active_hovered_group = designer_workspace
            ? shared_state.designer_hovered_group : shared_state.hovered_group;
        auto& active_hovered_material = designer_workspace
            ? shared_state.designer_hovered_material : shared_state.hovered_material;

        const auto hovered_group = material_workspace
            ? ui::group_at(layout, pointer) : material_group_count;
        active_hovered_group.store(hovered_group, std::memory_order_relaxed);
        if (designer_workspace) shared_state.hovered_group.store(material_group_count, std::memory_order_relaxed);
        else shared_state.designer_hovered_group.store(material_group_count, std::memory_order_relaxed);

        const auto selected_group_index =
            active_selected_group.load(std::memory_order_relaxed) % material_group_count;
        const auto selected_group = static_cast<MaterialGroup>(selected_group_index);
        const auto hovered_material = material_workspace
            ? ui::palette_material_at(layout, selected_group, pointer) : Material::count;
        active_hovered_material.store(
            static_cast<std::uint32_t>(hovered_material), std::memory_order_relaxed);
        if (designer_workspace) shared_state.hovered_material.store(material_count, std::memory_order_relaxed);
        else shared_state.designer_hovered_material.store(material_count, std::memory_order_relaxed);
        const bool hovered_ignite_air = editor_workspace &&
            ui::ignite_air_action_at(layout, selected_group, pointer);

        if (primary_pressed) {
            const auto workspace = ui::workspace_at(layout, pointer);
            if (workspace < ui::workspace_tab_count) {
                shared_state.selected_workspace.store(workspace, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.previous_scene, pointer)) {
                scene = previous_scene(scene);
                shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
                shared_state.reset.store(true, std::memory_order_release);
                shared_state.mining_mode.store(scene_has_character(scene),
                                               std::memory_order_release);
                reset_camera_to_zero(shared_state, simulation_config);
                reset_map_view(shared_state, simulation_config);
            } else if (epochengine::gui_lib::contains(layout.next_scene, pointer)) {
                scene = next_scene(scene);
                shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
                shared_state.reset.store(true, std::memory_order_release);
                shared_state.mining_mode.store(scene_has_character(scene),
                                               std::memory_order_release);
                reset_camera_to_zero(shared_state, simulation_config);
                reset_map_view(shared_state, simulation_config);
            } else if (epochengine::gui_lib::contains(layout.reset_scene, pointer)) {
                shared_state.reset.store(true, std::memory_order_release);
                reset_camera_to_zero(shared_state, simulation_config);
                reset_map_view(shared_state, simulation_config);
            } else if (epochengine::gui_lib::contains(layout.save_scene, pointer)) {
                shared_state.save_scene_image.store(true, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.load_scene, pointer)) {
                shared_state.load_scene_image.store(true, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.mode_toggle, pointer)) {
                const bool current_mining = shared_state.mining_mode.load(std::memory_order_relaxed);
                shared_state.mining_mode.store(!current_mining, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.pause_toggle, pointer)) {
                const bool paused = shared_state.paused.load(std::memory_order_relaxed);
                shared_state.paused.store(!paused, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.camera_controls_toggle, pointer)) {
                const bool camera_controls =
                    shared_state.camera_controls.load(std::memory_order_relaxed);
                shared_state.camera_controls.store(!camera_controls,
                                                   std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.map_toggle, pointer)) {
                const bool map = shared_state.map_view.load(std::memory_order_relaxed);
                shared_state.map_view.store(!map, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {
                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);
                shared_state.debug_visualization.store(!debug, std::memory_order_release);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.designer_static_model, pointer)) {
                shared_state.designer_mode.store(0u, std::memory_order_relaxed);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.designer_map_chunk, pointer)) {
                shared_state.designer_mode.store(1u, std::memory_order_relaxed);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.designer_inventory, pointer)) {
                shared_state.designer_pane.store(0u, std::memory_order_relaxed);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.designer_blueprints, pointer)) {
                shared_state.designer_pane.store(1u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.placement_cells, pointer)) {
                (designer_workspace ? shared_state.designer_placement_mode : shared_state.placement_mode)
                    .store(0u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.placement_tiles, pointer)) {
                (designer_workspace ? shared_state.designer_placement_mode : shared_state.placement_mode)
                    .store(1u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.cursor_circle, pointer)) {
                (designer_workspace ? shared_state.designer_brush_shape : shared_state.brush_shape)
                    .store(0u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.cursor_square, pointer)) {
                (designer_workspace ? shared_state.designer_brush_shape : shared_state.brush_shape)
                    .store(1u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.cursor_horizontal, pointer)) {
                (designer_workspace ? shared_state.designer_brush_shape : shared_state.brush_shape)
                    .store(2u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.cursor_vertical, pointer)) {
                (designer_workspace ? shared_state.designer_brush_shape : shared_state.brush_shape)
                    .store(3u, std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.brush_smaller, pointer)) {
                auto& radius_state = designer_workspace
                    ? shared_state.designer_brush_radius : shared_state.brush_radius;
                const auto radius = static_cast<int>(radius_state.load(std::memory_order_relaxed));
                radius_state.store(static_cast<std::uint32_t>(std::clamp(
                    radius - 1, 1, designer_workspace ? 12 : 48)), std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.brush_larger, pointer)) {
                auto& radius_state = designer_workspace
                    ? shared_state.designer_brush_radius : shared_state.brush_radius;
                const auto radius = static_cast<int>(radius_state.load(std::memory_order_relaxed));
                radius_state.store(static_cast<std::uint32_t>(std::clamp(
                    radius + 1, 1, designer_workspace ? 12 : 48)), std::memory_order_relaxed);
            } else if (editor_workspace && epochengine::gui_lib::contains(layout.zoom_out, pointer)) {
                adjust_zoom_centered(-1);
            } else if (editor_workspace && epochengine::gui_lib::contains(layout.zoom_in, pointer)) {
                adjust_zoom_centered(1);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.zoom_out, pointer)) {
                const auto zoom = static_cast<int>(shared_state.designer_zoom.load(std::memory_order_relaxed));
                shared_state.designer_zoom.store(static_cast<std::uint32_t>(
                    std::clamp(zoom - 1, 1, 4)), std::memory_order_relaxed);
            } else if (designer_workspace && epochengine::gui_lib::contains(layout.zoom_in, pointer)) {
                const auto zoom = static_cast<int>(shared_state.designer_zoom.load(std::memory_order_relaxed));
                shared_state.designer_zoom.store(static_cast<std::uint32_t>(
                    std::clamp(zoom + 1, 1, 4)), std::memory_order_relaxed);
            } else if (material_workspace && epochengine::gui_lib::contains(layout.atmosphere, pointer)) {
                active_selected_material.store(
                    static_cast<std::uint32_t>(Material::atmosphere), std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.fill, pointer)) {
                // World Fill remains click-confirmed. Designer Fill will reuse
                // the bounded selection model when MC-149 lands.
            } else if (material_workspace && epochengine::gui_lib::contains(layout.eraser, pointer)) {
                active_selected_material.store(static_cast<std::uint32_t>(Material::empty),
                                               std::memory_order_relaxed);
            } else if (material_workspace && hovered_group < material_group_count) {
                active_selected_group.store(hovered_group, std::memory_order_relaxed);
            } else if (hovered_ignite_air) {
                shared_state.ignite_air.store(true, std::memory_order_release);
            } else if (material_workspace && hovered_material != Material::count) {
                active_selected_material.store(
                    static_cast<std::uint32_t>(hovered_material), std::memory_order_relaxed);
            }
        }

        const bool world_paused = shared_state.paused.load(std::memory_order_relaxed);
        const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
        const bool inspecting = input.inspect_material;
        const bool fill_click = editor_workspace && input.fill_modifier && primary_pressed &&
                                over_world && !pan_button_down && !world_paused;
        if (fill_click) shared_state.fill_region.store(true, std::memory_order_release);

        const bool designer_paint_active = designer_workspace && over_world &&
                                           input.primary_down && !inspecting &&
                                           !input.fill_modifier && !pan_button_down &&
                                           !world_paused;
        if (designer_paint_active)
            paint_designer_grid(shared_state, simulation_viewport, input.mouse_x, input.mouse_y);

        const bool player_build = scene_player_present && !mining;
        const bool paint_active = editor_workspace && over_world && !scene_player_present && !mining &&
                                  !inspecting && !input.fill_modifier && !pan_button_down &&
                                  !world_paused;
        shared_state.primary_down.store(input.primary_down && paint_active,
                                         std::memory_order_relaxed);
        // Right mouse is camera-only. Erasing is an explicit left-click Eraser
        // selection, never an implicit Oxygen write.
        shared_state.secondary_down.store(false, std::memory_order_relaxed);

        const bool tool_active = editor_workspace && over_world && scene_player_present && mining &&
                                 !inspecting && !input.fill_modifier && !pan_button_down &&
                                 !world_paused;
        shared_state.fire_tool.store(input.primary_down && tool_active,
                                     std::memory_order_relaxed);
        if (primary_pressed && tool_active)
            shared_state.fire_tool_pressed.store(true, std::memory_order_release);

        const bool build_active = editor_workspace && over_world && player_build && !inspecting &&
                                  !input.fill_modifier && !pan_button_down && !world_paused;
        shared_state.deposit_resource.store(input.primary_down && build_active,
                                             std::memory_order_relaxed);
        if (primary_pressed && build_active)
            shared_state.deposit_resource_pressed.store(true, std::memory_order_release);

        shared_state.move_x.store(world_paused ? 0 : directional_input.player_x,
                                  std::memory_order_relaxed);
        shared_state.move_y.store(world_paused ? 0 : directional_input.player_y,
                                  std::memory_order_relaxed);
        shared_state.jump.store(!world_paused && player_controls && input.jump,
                                std::memory_order_relaxed);


        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::fprintf(stderr, "[SandHybrid] Shutting down.\n");
    shared_state.quit.store(true, std::memory_order_release);
    stop_renderer.store(true, std::memory_order_release);
    if (render_thread.joinable()) render_thread.join();

    {
        const std::scoped_lock lock{render_error_mutex};
        if (render_error) {
            std::rethrow_exception(render_error);
        }
    }

    return 0;
}

} // namespace sandhybrid
