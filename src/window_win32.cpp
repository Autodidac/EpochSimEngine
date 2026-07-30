#include "epoch/sand/window.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace epoch::sand {

namespace {

constexpr wchar_t window_class_name[] = L"EpochSandNativeWindow";

std::wstring widen(const std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("Failed to convert UTF-8 window title.");
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), size);
    return result;
}

} // namespace

struct NativeWindow::Impl final {
    HINSTANCE instance{GetModuleHandleW(nullptr)};
    HWND handle{};
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

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (self == nullptr) {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }

        switch (message) {
        case WM_CLOSE:
            self->close_requested = true;
            return 0;
        case WM_DESTROY:
            self->close_requested = true;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            self->width = static_cast<std::uint32_t>(LOWORD(lparam));
            self->height = static_cast<std::uint32_t>(HIWORD(lparam));
            self->resized = true;
            return 0;
        case WM_MOUSEMOVE:
            self->mouse_x = GET_X_LPARAM(lparam);
            self->mouse_y = GET_Y_LPARAM(lparam);
            return 0;
        case WM_LBUTTONDOWN:
            if (!self->primary_down) self->primary_pressed = true;
            self->primary_down = true;
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
            self->primary_down = false;
            if (!self->secondary_down) {
                ReleaseCapture();
            }
            return 0;
        case WM_RBUTTONDOWN:
            if (!self->secondary_down) self->secondary_pressed = true;
            self->secondary_down = true;
            SetCapture(hwnd);
            return 0;
        case WM_RBUTTONUP:
            self->secondary_down = false;
            if (!self->primary_down) {
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEWHEEL:
            self->wheel_delta += GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            return 0;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            if ((lparam & (1u << 30u)) != 0) {
                return 0;
            }
            switch (wparam) {
            case VK_MENU:
                self->inspect_material = true;
                return 0;
            case VK_F3:
                self->toggle_debug = true;
                return 0;
            case VK_F5:
                self->save_scene = true;
                return 0;
            case VK_F9:
                self->load_scene = true;
                return 0;
            case VK_ESCAPE:
                self->close_requested = true;
                return 0;
            case VK_SPACE:
                self->jump = true;
                return 0;
            case 'P':
                self->toggle_pause = true;
                return 0;
            case 'N':
                self->single_step = true;
                return 0;
            case 'R':
                self->reset = true;
                return 0;
            case VK_OEM_6:
                self->next_scene = true;
                return 0;
            case VK_OEM_4:
                self->previous_scene = true;
                return 0;
            case 'A': self->move_left = true; return 0;
            case 'D': self->move_right = true; return 0;
            case 'W': self->move_up = true; return 0;
            case 'S': self->move_down = true; return 0;
            case 'M': self->toggle_mining = true; return 0;
            default:
                break;
            }
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            switch (wparam) {
            case VK_MENU: self->inspect_material = false; return 0;
            case 'A': self->move_left = false; return 0;
            case 'D': self->move_right = false; return 0;
            case VK_SPACE: self->jump = false; return 0;
            case 'W': self->move_up = false; return 0;
            case 'S': self->move_down = false; return 0;
            default: break;
            }
            break;
        case WM_SYSCHAR:
            if (self->inspect_material) return 0;
            break;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
};

NativeWindow::NativeWindow(const std::string_view title, const std::uint32_t width,
                           const std::uint32_t height)
    : impl_(std::make_unique<Impl>()) {
    impl_->width = width;
    impl_->height = height;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = Impl::window_proc;
    window_class.hInstance = impl_->instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = window_class_name;

    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("RegisterClassExW failed.");
    }

    RECT rectangle{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRectEx(&rectangle, WS_OVERLAPPEDWINDOW, FALSE, 0);

    const auto wide_title = widen(title);
    impl_->handle = CreateWindowExW(
        0,
        window_class_name,
        wide_title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        nullptr,
        nullptr,
        impl_->instance,
        impl_.get());

    if (impl_->handle == nullptr) {
        throw std::runtime_error("CreateWindowExW failed.");
    }
}

NativeWindow::~NativeWindow() {
    if (impl_ && impl_->handle != nullptr) {
        DestroyWindow(impl_->handle);
        impl_->handle = nullptr;
    }
}

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

    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (message.message == WM_QUIT) {
            impl_->close_requested = true;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    // Refresh continuous movement from physical key state. This prevents focus,
    // capture, or message coalescing from dropping a key-up/down transition and
    // silently disabling player movement.
    const bool focused = GetForegroundWindow() == impl_->handle;
    const auto key_down = [focused](const int key) noexcept {
        return focused && (GetAsyncKeyState(key) & 0x8000) != 0;
    };
    impl_->move_left = key_down('A') || key_down(VK_LEFT);
    impl_->move_right = key_down('D') || key_down(VK_RIGHT);
    impl_->move_up = key_down('W') || key_down(VK_UP);
    impl_->move_down = key_down('S') || key_down(VK_DOWN);
    impl_->jump = key_down(VK_SPACE);

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

void NativeWindow::set_title(const std::string_view title) {
    const auto wide_title = widen(title);
    SetWindowTextW(impl_->handle, wide_title.c_str());
}

std::vector<const char*> NativeWindow::required_instance_extensions() const {
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
}

VkSurfaceKHR NativeWindow::create_surface(const VkInstance instance) const {
    const VkWin32SurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = impl_->instance,
        .hwnd = impl_->handle,
    };

    VkSurfaceKHR surface{};
    if (vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed.");
    }
    return surface;
}

} // namespace epoch::sand
