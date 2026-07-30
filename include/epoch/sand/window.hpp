#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace epoch::sand {

struct WindowInput final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::int32_t mouse_x{};
    std::int32_t mouse_y{};
    std::int32_t wheel_delta{};
    bool primary_down{};
    bool secondary_down{};
    bool middle_down{};
    bool primary_pressed{};
    bool secondary_pressed{};
    bool middle_pressed{};
    bool close_requested{};
    bool resized{};
    bool toggle_pause{};
    bool single_step{};
    bool reset{};
    bool fill{};
    bool save_scene{};
    bool load_scene{};
    bool next_scene{};
    bool previous_scene{};
    bool move_left{};
    bool move_right{};
    bool move_up{};
    bool move_down{};
    bool jump{};
    bool toggle_mining{};
    bool inspect_material{};
    bool toggle_debug{};
};

class NativeWindow final {
public:
    NativeWindow(std::string_view title, std::uint32_t width, std::uint32_t height);
    ~NativeWindow();

    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    NativeWindow(NativeWindow&&) noexcept;
    NativeWindow& operator=(NativeWindow&&) noexcept;

    [[nodiscard]] bool poll(WindowInput& input);
    void set_title(std::string_view title);
    void show_startup_message(std::string_view message);

    [[nodiscard]] std::vector<const char*> required_instance_extensions() const;
    [[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace epoch::sand
