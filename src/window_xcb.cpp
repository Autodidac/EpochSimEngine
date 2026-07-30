#include "epoch/sand/window.hpp"

#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace epoch::sand {

namespace {

xcb_atom_t intern_atom(xcb_connection_t* connection, const char* name) {
    const auto cookie = xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
        throw std::runtime_error(std::string{"Unable to intern XCB atom: "} + name);
    }
    const auto atom = reply->atom;
    std::free(reply);
    return atom;
}

constexpr std::uint32_t keysym_escape = 0xFF1Bu;
constexpr std::uint32_t keysym_space = 0x0020u;
constexpr std::uint32_t keysym_p = 0x0070u;
constexpr std::uint32_t keysym_upper_p = 0x0050u;
constexpr std::uint32_t keysym_n = 0x006Eu;
constexpr std::uint32_t keysym_upper_n = 0x004Eu;
constexpr std::uint32_t keysym_r = 0x0072u;
constexpr std::uint32_t keysym_upper_r = 0x0052u;
constexpr std::uint32_t keysym_left_bracket = 0x005Bu;
constexpr std::uint32_t keysym_right_bracket = 0x005Du;
constexpr std::uint32_t keysym_a = 0x0061u;
constexpr std::uint32_t keysym_d = 0x0064u;
constexpr std::uint32_t keysym_w = 0x0077u;
constexpr std::uint32_t keysym_s = 0x0073u;
constexpr std::uint32_t keysym_m = 0x006du;
constexpr std::uint32_t keysym_upper_a = 0x0041u;
constexpr std::uint32_t keysym_upper_d = 0x0044u;
constexpr std::uint32_t keysym_upper_w = 0x0057u;
constexpr std::uint32_t keysym_upper_s = 0x0053u;
constexpr std::uint32_t keysym_upper_m = 0x004du;
constexpr std::uint32_t keysym_alt_l = 0xFFE9u;
constexpr std::uint32_t keysym_alt_r = 0xFFEAu;
constexpr std::uint32_t keysym_f3 = 0xFFC0u;
constexpr std::uint32_t keysym_f5 = 0xFFC2u;
constexpr std::uint32_t keysym_f9 = 0xFFC6u;

std::uint32_t lookup_keysym(xcb_connection_t* connection, const xcb_keycode_t keycode) {
    const auto cookie = xcb_get_keyboard_mapping(connection, keycode, 1);
    xcb_get_keyboard_mapping_reply_t* reply = xcb_get_keyboard_mapping_reply(connection, cookie, nullptr);
    if (reply == nullptr || reply->keysyms_per_keycode == 0) {
        std::free(reply);
        return 0u;
    }

    const auto* keysyms = xcb_get_keyboard_mapping_keysyms(reply);
    std::uint32_t result{};
    for (std::uint8_t index = 0; index < reply->keysyms_per_keycode; ++index) {
        if (keysyms[index] != 0u) {
            result = keysyms[index];
            break;
        }
    }
    std::free(reply);
    return result;
}

} // namespace

struct NativeWindow::Impl final {
    xcb_connection_t* connection{};
    xcb_screen_t* screen{};
    xcb_window_t window{};
    xcb_gcontext_t startup_gc{};
    std::string startup_message;
    xcb_atom_t wm_protocols{};
    xcb_atom_t wm_delete_window{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::int32_t mouse_x{};
    std::int32_t mouse_y{};
    std::int32_t wheel_delta{};
    bool primary_down{};
    bool secondary_down{};
    bool primary_pressed{};
    bool secondary_pressed{};
    bool close_requested{};
    bool resized{};
    bool toggle_pause{};
    bool single_step{};
    bool reset{};
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

    void draw_startup_message() {
        if (connection == nullptr || window == 0 || startup_gc == 0 || startup_message.empty()) return;
        xcb_clear_area(connection, 0, window, 0, 0,
                       static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
        const int textWidth = static_cast<int>(startup_message.size()) * 6;
        const std::int16_t textX = static_cast<std::int16_t>(
            width > static_cast<std::uint32_t>(textWidth)
                ? (width - static_cast<std::uint32_t>(textWidth)) / 2u : 4u);
        const std::int16_t textY = static_cast<std::int16_t>(height / 2u + 4u);
        xcb_image_text_8(connection, static_cast<std::uint8_t>(startup_message.size()),
                         window, startup_gc, textX, textY, startup_message.c_str());
        xcb_flush(connection);
    }

    ~Impl() {
        if (connection != nullptr && startup_gc != 0) {
            xcb_free_gc(connection, startup_gc);
            startup_gc = 0;
        }
        if (connection != nullptr && window != 0) {
            xcb_destroy_window(connection, window);
            xcb_flush(connection);
        }
        if (connection != nullptr) {
            xcb_disconnect(connection);
        }
    }
};

NativeWindow::NativeWindow(const std::string_view title, const std::uint32_t width,
                           const std::uint32_t height)
    : impl_(std::make_unique<Impl>()) {
    int screen_index{};
    impl_->connection = xcb_connect(nullptr, &screen_index);
    if (impl_->connection == nullptr || xcb_connection_has_error(impl_->connection) != 0) {
        throw std::runtime_error("xcb_connect failed. Ensure DISPLAY is available.");
    }

    const auto* setup = xcb_get_setup(impl_->connection);
    auto iterator = xcb_setup_roots_iterator(setup);
    for (int index = 0; index < screen_index; ++index) {
        xcb_screen_next(&iterator);
    }
    impl_->screen = iterator.data;
    if (impl_->screen == nullptr) {
        throw std::runtime_error("Unable to acquire the XCB screen.");
    }

    impl_->width = width;
    impl_->height = height;
    impl_->window = xcb_generate_id(impl_->connection);

    const std::uint32_t event_mask =
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE;
    const std::uint32_t values[] = {impl_->screen->black_pixel, event_mask};

    const auto create_cookie = xcb_create_window_checked(
        impl_->connection,
        XCB_COPY_FROM_PARENT,
        impl_->window,
        impl_->screen->root,
        0,
        0,
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        impl_->screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        values);
    if (xcb_generic_error_t* error = xcb_request_check(impl_->connection, create_cookie)) {
        const auto code = error->error_code;
        std::free(error);
        throw std::runtime_error("xcb_create_window failed with X11 error " + std::to_string(code));
    }

    impl_->startup_gc = xcb_generate_id(impl_->connection);
    const std::uint32_t gcValues[] = {impl_->screen->white_pixel, impl_->screen->black_pixel};
    const auto gcCookie = xcb_create_gc_checked(
        impl_->connection, impl_->startup_gc, impl_->window,
        XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gcValues);
    if (xcb_generic_error_t* error = xcb_request_check(impl_->connection, gcCookie)) {
        const auto code = error->error_code;
        std::free(error);
        throw std::runtime_error("xcb_create_gc failed with X11 error " + std::to_string(code));
    }

    impl_->wm_protocols = intern_atom(impl_->connection, "WM_PROTOCOLS");
    impl_->wm_delete_window = intern_atom(impl_->connection, "WM_DELETE_WINDOW");
    xcb_change_property(
        impl_->connection,
        XCB_PROP_MODE_REPLACE,
        impl_->window,
        impl_->wm_protocols,
        XCB_ATOM_ATOM,
        32,
        1,
        &impl_->wm_delete_window);

    set_title(title);
    xcb_map_window(impl_->connection, impl_->window);
    xcb_flush(impl_->connection);
}

NativeWindow::~NativeWindow() = default;

NativeWindow::NativeWindow(NativeWindow&&) noexcept = default;
NativeWindow& NativeWindow::operator=(NativeWindow&&) noexcept = default;

bool NativeWindow::poll(WindowInput& input) {
    impl_->wheel_delta = 0;
    impl_->primary_pressed = false;
    impl_->secondary_pressed = false;
    impl_->resized = false;
    impl_->toggle_pause = false;
    impl_->single_step = false;
    impl_->reset = false;
    impl_->save_scene = false;
    impl_->load_scene = false;
    impl_->next_scene = false;
    impl_->previous_scene = false;
    impl_->toggle_mining = false;
    impl_->toggle_debug = false;

    while (xcb_generic_event_t* event = xcb_poll_for_event(impl_->connection)) {
        const auto type = static_cast<std::uint8_t>(event->response_type & ~0x80u);
        switch (type) {
        case XCB_EXPOSE:
            impl_->draw_startup_message();
            break;
        case XCB_CLIENT_MESSAGE: {
            const auto* client = reinterpret_cast<xcb_client_message_event_t*>(event);
            if (client->type == impl_->wm_protocols && client->data.data32[0] == impl_->wm_delete_window) {
                impl_->close_requested = true;
            }
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
            const auto new_width = static_cast<std::uint32_t>(configure->width);
            const auto new_height = static_cast<std::uint32_t>(configure->height);
            impl_->resized = new_width != impl_->width || new_height != impl_->height;
            impl_->width = new_width;
            impl_->height = new_height;
            break;
        }
        case XCB_MOTION_NOTIFY: {
            const auto* motion = reinterpret_cast<xcb_motion_notify_event_t*>(event);
            impl_->mouse_x = motion->event_x;
            impl_->mouse_y = motion->event_y;
            break;
        }
        case XCB_BUTTON_PRESS: {
            const auto* button = reinterpret_cast<xcb_button_press_event_t*>(event);
            impl_->mouse_x = button->event_x;
            impl_->mouse_y = button->event_y;
            if (button->detail == 1) {
                if (!impl_->primary_down) impl_->primary_pressed = true;
                impl_->primary_down = true;
            } else if (button->detail == 3) {
                if (!impl_->secondary_down) impl_->secondary_pressed = true;
                impl_->secondary_down = true;
            } else if (button->detail == 4) {
                ++impl_->wheel_delta;
            } else if (button->detail == 5) {
                --impl_->wheel_delta;
            }
            break;
        }
        case XCB_BUTTON_RELEASE: {
            const auto* button = reinterpret_cast<xcb_button_release_event_t*>(event);
            if (button->detail == 1) {
                impl_->primary_down = false;
            } else if (button->detail == 3) {
                impl_->secondary_down = false;
            }
            break;
        }
        case XCB_KEY_PRESS: {
            const auto* key = reinterpret_cast<xcb_key_press_event_t*>(event);
            const auto keysym = lookup_keysym(impl_->connection, key->detail);
            if (keysym == keysym_alt_l || keysym == keysym_alt_r) {
                impl_->inspect_material = true;
            } else if (keysym == keysym_f3) {
                impl_->toggle_debug = true;
            } else if (keysym == keysym_f5) {
                impl_->save_scene = true;
            } else if (keysym == keysym_f9) {
                impl_->load_scene = true;
            } else if (keysym == keysym_escape) {
                impl_->close_requested = true;
            } else if (keysym == keysym_space) {
                impl_->jump = true;
            } else if (keysym == keysym_p || keysym == keysym_upper_p) {
                impl_->toggle_pause = true;
            } else if (keysym == keysym_n || keysym == keysym_upper_n) {
                impl_->single_step = true;
            } else if (keysym == keysym_r || keysym == keysym_upper_r) {
                impl_->reset = true;
            } else if (keysym == keysym_right_bracket) {
                impl_->next_scene = true;
            } else if (keysym == keysym_left_bracket) {
                impl_->previous_scene = true;
            } else if (keysym == keysym_a || keysym == keysym_upper_a) {
                impl_->move_left = true;
            } else if (keysym == keysym_d || keysym == keysym_upper_d) {
                impl_->move_right = true;
            } else if (keysym == keysym_w || keysym == keysym_upper_w) {
                impl_->move_up = true;
            } else if (keysym == keysym_s || keysym == keysym_upper_s) {
                impl_->move_down = true;
            } else if (keysym == keysym_m || keysym == keysym_upper_m) {
                impl_->toggle_mining = true;
            }
            break;
        }
        case XCB_KEY_RELEASE: {
            const auto* key = reinterpret_cast<xcb_key_release_event_t*>(event);
            const auto keysym = lookup_keysym(impl_->connection, key->detail);
            if (keysym == keysym_alt_l || keysym == keysym_alt_r) impl_->inspect_material = false;
            else if (keysym == keysym_a || keysym == keysym_upper_a) impl_->move_left = false;
            else if (keysym == keysym_d || keysym == keysym_upper_d) impl_->move_right = false;
            else if (keysym == keysym_space) impl_->jump = false;
            else if (keysym == keysym_w || keysym == keysym_upper_w) impl_->move_up = false;
            else if (keysym == keysym_s || keysym == keysym_upper_s) impl_->move_down = false;
            break;
        }
        default:
            break;
        }
        std::free(event);
    }

    if (xcb_connection_has_error(impl_->connection) != 0) {
        impl_->close_requested = true;
    }

    input = WindowInput{
        .width = impl_->width,
        .height = impl_->height,
        .mouse_x = impl_->mouse_x,
        .mouse_y = impl_->mouse_y,
        .wheel_delta = impl_->wheel_delta,
        .primary_down = impl_->primary_down,
        .secondary_down = impl_->secondary_down,
        .primary_pressed = impl_->primary_pressed,
        .secondary_pressed = impl_->secondary_pressed,
        .close_requested = impl_->close_requested,
        .resized = impl_->resized,
        .toggle_pause = impl_->toggle_pause,
        .single_step = impl_->single_step,
        .reset = impl_->reset,
        .save_scene = impl_->save_scene,
        .load_scene = impl_->load_scene,
        .next_scene = impl_->next_scene,
        .previous_scene = impl_->previous_scene,
        .move_left = impl_->move_left,
        .move_right = impl_->move_right,
        .move_up = impl_->move_up,
        .move_down = impl_->move_down,
        .jump = impl_->jump,
        .toggle_mining = impl_->toggle_mining,
        .inspect_material = impl_->inspect_material,
        .toggle_debug = impl_->toggle_debug,
    };
    return !impl_->close_requested;
}

void NativeWindow::show_startup_message(const std::string_view message) {
    impl_->startup_message.assign(message.data(), message.size());
    impl_->draw_startup_message();
}

void NativeWindow::set_title(const std::string_view title) {
    xcb_change_property(
        impl_->connection,
        XCB_PROP_MODE_REPLACE,
        impl_->window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        static_cast<std::uint32_t>(title.size()),
        title.data());
    xcb_flush(impl_->connection);
}

std::vector<const char*> NativeWindow::required_instance_extensions() const {
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME};
}

VkSurfaceKHR NativeWindow::create_surface(const VkInstance instance) const {
    const VkXcbSurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .connection = impl_->connection,
        .window = impl_->window,
    };

    VkSurfaceKHR surface{};
    if (vkCreateXcbSurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateXcbSurfaceKHR failed.");
    }
    return surface;
}

} // namespace epoch::sand
