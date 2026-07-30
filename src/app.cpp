#include "epoch/sand/app.hpp"

#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"
#include "epoch/sand/shared_state.hpp"
#include "epoch/sand/ui_layout.hpp"
#include "epoch/sand/vulkan_renderer.hpp"
#include "epoch/sand/window.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <thread>

namespace epoch::sand {

int run_application() {
    std::fprintf(stderr, "[EpochSand] Creating native window...\n");
    NativeWindow window{"SandHybrid - Loading", 1280, 720};
    std::fprintf(stderr, "[EpochSand] Native window created.\n");
    SharedState shared_state{};
    const SimulationConfig simulation_config{};
    std::atomic_bool renderer_ready{false};

    std::exception_ptr render_error;
    std::mutex render_error_mutex;

    // Keep driver initialization off the native event thread. Startup remains
    // responsive, while stage-by-stage console logging identifies any driver or
    // pipeline failure instead of leaving a silent frozen window.
    std::atomic_bool stop_renderer{false};
    std::thread render_thread([&] {
        try {
            std::fprintf(stderr, "[EpochSand] Vulkan initialization started.\n");
            VulkanRenderer renderer{window, simulation_config};
            renderer_ready.store(true, std::memory_order_release);
            std::fprintf(stderr, "[EpochSand] Vulkan renderer ready.\n");
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
    bool ready_title_applied = false;
    while (!shared_state.quit.load(std::memory_order_acquire) && window.poll(input)) {
        if (!ready_title_applied && renderer_ready.load(std::memory_order_acquire)) {
            window.set_title("SandHybrid");
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

        auto scene = static_cast<Scene>(
            shared_state.selected_scene.load(std::memory_order_relaxed) % scene_count);
        if (input.toggle_mining) {
            const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
            shared_state.mining_mode.store(!mining, std::memory_order_release);
        }

        if (input.next_scene) {
            scene = next_scene(scene);
            shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
            shared_state.reset.store(true, std::memory_order_release);
            shared_state.mining_mode.store(scene_has_character(scene),
                                           std::memory_order_release);
        } else if (input.previous_scene) {
            scene = previous_scene(scene);
            shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
            shared_state.reset.store(true, std::memory_order_release);
            shared_state.mining_mode.store(scene_has_character(scene),
                                           std::memory_order_release);
        } else if (input.reset) {
            shared_state.reset.store(true, std::memory_order_release);
        }

        const auto layout = ui::make_layout(input.width, input.height);
        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, simulation_config.grid_width, simulation_config.grid_height);
        const epochengine::gui_lib::Vec2 pointer{
            static_cast<float>(input.mouse_x),
            static_cast<float>(input.mouse_y),
        };
        const bool primary_pressed = input.primary_pressed;
        const bool secondary_pressed = input.secondary_pressed;

        const auto hovered_group = ui::group_at(layout, pointer);
        shared_state.hovered_group.store(hovered_group, std::memory_order_relaxed);

        const auto selected_group_index =
            shared_state.selected_group.load(std::memory_order_relaxed) % material_group_count;
        const auto selected_group = static_cast<MaterialGroup>(selected_group_index);
        const auto hovered_material = ui::palette_material_at(layout, selected_group, pointer);
        shared_state.hovered_material.store(
            static_cast<std::uint32_t>(hovered_material), std::memory_order_relaxed);

        if (primary_pressed) {
            if (epochengine::gui_lib::contains(layout.previous_scene, pointer)) {
                scene = previous_scene(scene);
                shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
                shared_state.reset.store(true, std::memory_order_release);
                shared_state.mining_mode.store(scene_has_character(scene),
                                               std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.next_scene, pointer)) {
                scene = next_scene(scene);
                shared_state.selected_scene.store(static_cast<std::uint32_t>(scene), std::memory_order_relaxed);
                shared_state.reset.store(true, std::memory_order_release);
                shared_state.mining_mode.store(scene_has_character(scene),
                                               std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.reset_scene, pointer)) {
                shared_state.reset.store(true, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.mode_toggle, pointer)) {
                const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
                shared_state.mining_mode.store(!mining, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {
                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);
                shared_state.debug_visualization.store(!debug, std::memory_order_release);
            } else if (hovered_group < material_group_count) {
                shared_state.selected_group.store(hovered_group, std::memory_order_relaxed);
            } else if (hovered_material != Material::count) {
                shared_state.selected_material.store(
                    static_cast<std::uint32_t>(hovered_material), std::memory_order_relaxed);
            }
        }

        const bool over_simulation = epochengine::gui_lib::contains(simulation_viewport.rect, pointer);
        const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
        const bool inspecting = input.inspect_material;
        const bool paint_active = over_simulation && !mining && !inspecting;
        shared_state.primary_down.store(input.primary_down && paint_active,
                                         std::memory_order_relaxed);
        shared_state.secondary_down.store(input.secondary_down && paint_active,
                                           std::memory_order_relaxed);
        // MINE uses the player tool; BUILD paints or erases the selected material.
        // Character movement remains active in either mode.
        const bool tool_active = over_simulation && mining && !inspecting;
        shared_state.fire_tool.store(input.primary_down && tool_active, std::memory_order_relaxed);
        shared_state.deposit_resource.store(input.secondary_down && tool_active, std::memory_order_relaxed);
        if (primary_pressed && tool_active)
            shared_state.fire_tool_pressed.store(true, std::memory_order_release);
        if (secondary_pressed && tool_active)
            shared_state.deposit_resource_pressed.store(true, std::memory_order_release);
        shared_state.move_x.store((input.move_right ? 1 : 0) - (input.move_left ? 1 : 0),
                                  std::memory_order_relaxed);
        shared_state.move_y.store((input.move_down ? 1 : 0) - (input.move_up ? 1 : 0),
                                  std::memory_order_relaxed);
        shared_state.jump.store(input.jump, std::memory_order_relaxed);

        if (input.wheel_delta != 0 && over_simulation) {
            const auto current = static_cast<int>(shared_state.brush_radius.load(std::memory_order_relaxed));
            const auto next = std::clamp(current + input.wheel_delta, 1, 48);
            shared_state.brush_radius.store(static_cast<std::uint32_t>(next), std::memory_order_relaxed);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::fprintf(stderr, "[EpochSand] Shutting down.\n");
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

} // namespace epoch::sand
