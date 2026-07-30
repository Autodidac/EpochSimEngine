#include "epochwater/canvas.hpp"
#include "epochwater/simulation.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace fastfreddy::testbed
{
    namespace
    {
        constexpr std::uint32_t canvas_width = 960U;
        constexpr std::uint32_t canvas_height = 540U;
        constexpr std::uint32_t grid_width = 128U;
        constexpr std::uint32_t grid_height = 72U;
        constexpr std::int32_t cell_pixels = 6;
        constexpr std::int32_t world_x = 16;
        constexpr std::int32_t world_y = 16;
        constexpr std::int32_t world_width = static_cast<std::int32_t>(grid_width) * cell_pixels;
        constexpr std::int32_t world_height = static_cast<std::int32_t>(grid_height) * cell_pixels;

        constexpr Color background{8U, 13U, 22U, 255U};
        constexpr Color world_background{15U, 24U, 37U, 255U};
        constexpr Color panel_fill{19U, 29U, 44U, 242U};
        constexpr Color panel_border{53U, 76U, 104U, 255U};
        constexpr Color button_fill{29U, 43U, 63U, 255U};
        constexpr Color selected_fill{43U, 78U, 111U, 255U};
        constexpr Color text_color{224U, 235U, 245U, 255U};
        constexpr Color muted_text{142U, 164U, 186U, 255U};
        constexpr Color stone_base{95U, 101U, 112U, 255U};
        constexpr Color stone_dark{67U, 72U, 82U, 255U};
        constexpr Color water_full{30U, 132U, 240U, 244U};
        constexpr Color water_half{30U, 132U, 240U, 112U};

        enum class Tool : std::uint8_t
        {
            water,
            stone,
            eraser
        };

        struct Button final
        {
            std::int32_t x{};
            std::int32_t y{};
            std::int32_t width{};
            std::int32_t height{};
            Tool tool{};
            std::string_view label{};
        };

        constexpr std::array tool_buttons{
            Button{812, 84, 120, 44, Tool::water, "1 WATER"},
            Button{812, 138, 120, 44, Tool::stone, "2 STONE"},
            Button{812, 192, 120, 44, Tool::eraser, "3 ERASER"}
        };

        [[nodiscard]] bool contains(const Button& button, const float x, const float y) noexcept
        {
            return x >= static_cast<float>(button.x) && y >= static_cast<float>(button.y) &&
                   x < static_cast<float>(button.x + button.width) &&
                   y < static_cast<float>(button.y + button.height);
        }

        [[nodiscard]] bool inside_world(const float x, const float y) noexcept
        {
            return x >= static_cast<float>(world_x) && y >= static_cast<float>(world_y) &&
                   x < static_cast<float>(world_x + world_width) &&
                   y < static_cast<float>(world_y + world_height);
        }

        void paint_at(Simulation& simulation, const Tool tool, const float x, const float y, const bool force_erase)
        {
            if (!inside_world(x, y))
                return;

            const auto grid_x = static_cast<std::uint32_t>((static_cast<std::int32_t>(x) - world_x) / cell_pixels);
            const auto grid_y = static_cast<std::uint32_t>((static_cast<std::int32_t>(y) - world_y) / cell_pixels);
            if (force_erase || tool == Tool::eraser)
            {
                simulation.erase(grid_x, grid_y, 2U);
                return;
            }
            if (tool == Tool::stone)
            {
                simulation.paint_stone_block(grid_x, grid_y);
                return;
            }
            simulation.add_water(grid_x, grid_y, Simulation::full_water);
        }

        void render_world(Canvas& canvas, const Simulation& simulation)
        {
            canvas.fill_rect(world_x, world_y, world_width, world_height, world_background);
            for (std::uint32_t y = 0; y < simulation.height(); ++y)
            {
                for (std::uint32_t x = 0; x < simulation.width(); ++x)
                {
                    const Cell& value = simulation.cell(x, y);
                    if (value.is_empty())
                        continue;

                    const std::int32_t px = world_x + static_cast<std::int32_t>(x) * cell_pixels;
                    const std::int32_t py = world_y + static_cast<std::int32_t>(y) * cell_pixels;
                    if (value.is_stone())
                    {
                        const std::uint32_t block_x = x / Simulation::terrain_block_size;
                        const std::uint32_t block_y = y / Simulation::terrain_block_size;
                        const std::uint8_t variation = static_cast<std::uint8_t>((block_x * 17U + block_y * 29U) % 15U);
                        const Color stone{
                            static_cast<std::uint8_t>(stone_base.red + variation),
                            static_cast<std::uint8_t>(stone_base.green + variation),
                            static_cast<std::uint8_t>(stone_base.blue + variation),
                            255U
                        };
                        canvas.fill_rect(px, py, cell_pixels, cell_pixels, stone);
                        if ((x % Simulation::terrain_block_size) == 0U)
                            canvas.fill_rect(px, py, 1, cell_pixels, stone_dark);
                        if ((y % Simulation::terrain_block_size) == 0U)
                            canvas.fill_rect(px, py, cell_pixels, 1, stone_dark);
                    }
                    else if (value.is_water())
                    {
                        canvas.fill_rect(
                            px,
                            py,
                            cell_pixels,
                            cell_pixels,
                            value.water_units == Simulation::half_water ? water_half : water_full);
                    }
                }
            }
        }

        void render_button(Canvas& canvas, const Button& button, const Tool selected)
        {
            const bool active = selected == button.tool;
            canvas.draw_rounded_rect(
                static_cast<float>(button.x),
                static_cast<float>(button.y),
                static_cast<float>(button.width),
                static_cast<float>(button.height),
                10.0F,
                active ? selected_fill : button_fill,
                active ? Color{90U, 170U, 225U, 255U} : panel_border,
                2.0F);
            canvas.draw_text(button.x + 12, button.y + 14, button.label, text_color, 2U);
        }

        void render_ui(Canvas& canvas, const Simulation& simulation, const Tool selected,
                       const bool paused, const double fps)
        {
            canvas.draw_rounded_rect(800.0F, 16.0F, 144.0F, 432.0F, 14.0F, panel_fill, panel_border, 2.0F);
            canvas.draw_text(814, 34, "WATER TESTBED", text_color, 2U);
            canvas.draw_text(814, 61, "VULKAN", muted_text, 1U);

            for (const Button& button : tool_buttons)
                render_button(canvas, button, selected);

            canvas.draw_rounded_rect(812.0F, 254.0F, 120.0F, 70.0F, 10.0F, button_fill, panel_border, 2.0F);
            canvas.draw_text(824, 268, "R RESET", text_color, 1U);
            canvas.draw_text(824, 284, "SPACE PAUSE", text_color, 1U);
            canvas.draw_text(824, 300, "RIGHT ERASE", text_color, 1U);

            canvas.draw_rounded_rect(812.0F, 338.0F, 120.0F, 94.0F, 10.0F, button_fill, panel_border, 2.0F);
            canvas.draw_text(824, 352, "WATER MASS", text_color, 1U);
            canvas.draw_text(824, 370, "HALF = FAINT", muted_text, 1U);
            canvas.draw_text(824, 386, "2 HALF = FULL", muted_text, 1U);
            canvas.draw_text(824, 402, "EDGE NEEDS 1.5", muted_text, 1U);
            canvas.draw_text(824, 418, "HALF MOVES 2X", muted_text, 1U);

            canvas.draw_rounded_rect(16.0F, 464.0F, 928.0F, 60.0F, 12.0F, panel_fill, panel_border, 2.0F);
            const std::string status =
                std::string{paused ? "PAUSED" : "RUNNING"} +
                "  WATER HALF-UNITS " + std::to_string(simulation.total_water_units()) +
                "  TICK " + std::to_string(simulation.tick()) +
                "  FPS " + std::to_string(static_cast<int>(fps + 0.5));
            canvas.draw_text(32, 482, status, text_color, 2U);
            canvas.draw_text(32, 508, "FULL WATER HANGS AT A LIP UNTIL A TRAILING HALF ARRIVES", muted_text, 1U);
        }

        void print_renderer_diagnostics(SDL_Renderer* renderer)
        {
            const char* renderer_name = SDL_GetRendererName(renderer);
            std::cout << "Renderer: " << (renderer_name != nullptr ? renderer_name : "unknown") << '\n';

            const SDL_PropertiesID properties = SDL_GetRendererProperties(renderer);
            auto* physical_device = static_cast<VkPhysicalDevice>(
                SDL_GetPointerProperty(properties, SDL_PROP_RENDERER_VULKAN_PHYSICAL_DEVICE_POINTER, nullptr));
            if (physical_device != VK_NULL_HANDLE)
            {
                VkPhysicalDeviceProperties device_properties{};
                vkGetPhysicalDeviceProperties(physical_device, &device_properties);
                std::cout << "Vulkan device: " << device_properties.deviceName << '\n';
                std::cout << "Vulkan API: "
                          << VK_API_VERSION_MAJOR(device_properties.apiVersion) << '.'
                          << VK_API_VERSION_MINOR(device_properties.apiVersion) << '.'
                          << VK_API_VERSION_PATCH(device_properties.apiVersion) << '\n';
            }
        }
    }
}

int main(int, char**)
{
    using namespace fastfreddy::testbed;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
    SDL_Window* window = SDL_CreateWindow(
        "FastFreddy Vulkan Water Testbed",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, "vulkan");
    if (renderer == nullptr)
    {
        std::cerr << "SDL Vulkan renderer creation failed: " << SDL_GetError() << '\n';
        std::cerr << "Available SDL renderers:\n";
        for (int index = 0; index < SDL_GetNumRenderDrivers(); ++index)
            std::cerr << "  " << SDL_GetRenderDriver(index) << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const char* renderer_name = SDL_GetRendererName(renderer);
    if (renderer_name == nullptr || std::string_view{renderer_name} != "vulkan")
    {
        std::cerr << "The selected renderer is not Vulkan. Got: "
                  << (renderer_name != nullptr ? renderer_name : "unknown") << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!SDL_SetRenderLogicalPresentation(
            renderer,
            static_cast<int>(canvas_width),
            static_cast<int>(canvas_height),
            SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        std::cerr << "SDL_SetRenderLogicalPresentation failed: " << SDL_GetError() << '\n';
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        static_cast<int>(canvas_width),
        static_cast<int>(canvas_height));
    if (texture == nullptr)
    {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    print_renderer_diagnostics(renderer);

    Simulation simulation{grid_width, grid_height};
    Canvas canvas{canvas_width, canvas_height};
    Tool selected_tool = Tool::water;
    bool paused = false;
    bool running = true;
    bool left_down = false;
    bool right_down = false;
    float mouse_x = -1.0F;
    float mouse_y = -1.0F;

    using clock = std::chrono::steady_clock;
    constexpr auto simulation_step = std::chrono::duration<double>{1.0 / 60.0};
    auto previous = clock::now();
    auto fps_window_start = previous;
    std::chrono::duration<double> accumulator{};
    std::uint32_t frames{};
    double fps{};

    while (running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat)
                    break;
                switch (event.key.key)
                {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_1: selected_tool = Tool::water; break;
                case SDLK_2: selected_tool = Tool::stone; break;
                case SDLK_3: selected_tool = Tool::eraser; break;
                case SDLK_R: simulation.reset_demo_scene(); break;
                case SDLK_SPACE: paused = !paused; break;
                default: break;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                mouse_x = event.button.x;
                mouse_y = event.button.y;
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_down = true;
                    bool selected_button = false;
                    for (const Button& button : tool_buttons)
                    {
                        if (contains(button, mouse_x, mouse_y))
                        {
                            selected_tool = button.tool;
                            selected_button = true;
                            break;
                        }
                    }
                    if (!selected_button)
                        paint_at(simulation, selected_tool, mouse_x, mouse_y, false);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    right_down = true;
                    paint_at(simulation, selected_tool, mouse_x, mouse_y, true);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT)
                    left_down = false;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    right_down = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                if (left_down)
                    paint_at(simulation, selected_tool, mouse_x, mouse_y, false);
                if (right_down)
                    paint_at(simulation, selected_tool, mouse_x, mouse_y, true);
                break;
            default:
                break;
            }
        }

        const auto now = clock::now();
        const auto elapsed = now - previous;
        previous = now;
        accumulator += elapsed;
        accumulator = std::min(accumulator, std::chrono::duration<double>{0.25});

        while (accumulator >= simulation_step)
        {
            if (!paused)
                simulation.step();
            accumulator -= simulation_step;
        }

        canvas.clear(background);
        render_world(canvas, simulation);
        render_ui(canvas, simulation, selected_tool, paused, fps);

        if (!SDL_UpdateTexture(texture, nullptr, canvas.bytes().data(), static_cast<int>(canvas.pitch())))
        {
            std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << '\n';
            running = false;
            continue;
        }

        SDL_SetRenderDrawColor(renderer, background.red, background.green, background.blue, background.alpha);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        ++frames;
        const auto fps_elapsed = now - fps_window_start;
        if (fps_elapsed >= std::chrono::seconds{1})
        {
            fps = static_cast<double>(frames) / std::chrono::duration<double>(fps_elapsed).count();
            frames = 0U;
            fps_window_start = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
