#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old[:80]!r}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


replace_once(
    "include/epoch/sand/shared_state.hpp",
    "    std::atomic_bool load_scene_image{false};\n",
    "    std::atomic_bool load_scene_image{false};\n"
    "    std::atomic_bool fill_region{false};\n")

replace_once(
    "include/epoch/sand/window.hpp",
    "    bool primary_down{};\n    bool secondary_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n",
    "    bool primary_down{};\n    bool secondary_down{};\n    bool middle_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n    bool middle_pressed{};\n")
replace_once(
    "include/epoch/sand/window.hpp",
    "    bool reset{};\n    bool save_scene{};\n",
    "    bool reset{};\n    bool fill{};\n    bool save_scene{};\n")

replace_once(
    "src/window_win32.cpp",
    "    bool primary_down{};\n    bool secondary_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n",
    "    bool primary_down{};\n    bool secondary_down{};\n    bool middle_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n    bool middle_pressed{};\n")
replace_once(
    "src/window_win32.cpp",
    "    bool reset{};\n    bool save_scene{};\n",
    "    bool reset{};\n    bool fill{};\n    bool save_scene{};\n")
replace_once(
    "src/window_win32.cpp",
    '''        case WM_RBUTTONDOWN:
            if (!self->secondary_down) self->secondary_pressed = true;
            self->secondary_down = true;
            SetCapture(hwnd);
            return 0;
''',
    '''        case WM_MBUTTONDOWN:
            if (!self->middle_down) self->middle_pressed = true;
            self->middle_down = true;
            SetCapture(hwnd);
            return 0;
        case WM_MBUTTONUP:
            self->middle_down = false;
            if (!self->primary_down && !self->secondary_down) ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            if (!self->secondary_down) self->secondary_pressed = true;
            self->secondary_down = true;
            SetCapture(hwnd);
            return 0;
''')
replace_once(
    "src/window_win32.cpp",
    "            case 'R':\n                self->reset = true;\n                return 0;\n",
    "            case 'R':\n                self->reset = true;\n                return 0;\n"
    "            case 'F':\n                self->fill = true;\n                return 0;\n")
replace_once(
    "src/window_win32.cpp",
    "    impl_->primary_pressed = false;\n    impl_->secondary_pressed = false;\n",
    "    impl_->primary_pressed = false;\n    impl_->secondary_pressed = false;\n    impl_->middle_pressed = false;\n")
replace_once(
    "src/window_win32.cpp",
    "    impl_->reset = false;\n    impl_->save_scene = false;\n",
    "    impl_->reset = false;\n    impl_->fill = false;\n    impl_->save_scene = false;\n")
replace_once(
    "src/window_win32.cpp",
    "        .primary_down = impl_->primary_down,\n        .secondary_down = impl_->secondary_down,\n"
    "        .primary_pressed = impl_->primary_pressed,\n        .secondary_pressed = impl_->secondary_pressed,\n",
    "        .primary_down = impl_->primary_down,\n        .secondary_down = impl_->secondary_down,\n"
    "        .middle_down = impl_->middle_down,\n        .primary_pressed = impl_->primary_pressed,\n"
    "        .secondary_pressed = impl_->secondary_pressed,\n        .middle_pressed = impl_->middle_pressed,\n")
replace_once(
    "src/window_win32.cpp",
    "        .reset = impl_->reset,\n        .save_scene = impl_->save_scene,\n",
    "        .reset = impl_->reset,\n        .fill = impl_->fill,\n        .save_scene = impl_->save_scene,\n")

replace_once(
    "src/window_xcb.cpp",
    "constexpr std::uint32_t keysym_r = 0x0072u;\n"
    "constexpr std::uint32_t keysym_upper_r = 0x0052u;\n",
    "constexpr std::uint32_t keysym_r = 0x0072u;\n"
    "constexpr std::uint32_t keysym_upper_r = 0x0052u;\n"
    "constexpr std::uint32_t keysym_f = 0x0066u;\n"
    "constexpr std::uint32_t keysym_upper_f = 0x0046u;\n")
replace_once(
    "src/window_xcb.cpp",
    "    bool primary_down{};\n    bool secondary_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n",
    "    bool primary_down{};\n    bool secondary_down{};\n    bool middle_down{};\n"
    "    bool primary_pressed{};\n    bool secondary_pressed{};\n    bool middle_pressed{};\n")
replace_once(
    "src/window_xcb.cpp",
    "    bool reset{};\n    bool save_scene{};\n",
    "    bool reset{};\n    bool fill{};\n    bool save_scene{};\n")
replace_once(
    "src/window_xcb.cpp",
    '''            if (button->detail == 1) {
                if (!impl_->primary_down) impl_->primary_pressed = true;
                impl_->primary_down = true;
            } else if (button->detail == 3) {
''',
    '''            if (button->detail == 1) {
                if (!impl_->primary_down) impl_->primary_pressed = true;
                impl_->primary_down = true;
            } else if (button->detail == 2) {
                if (!impl_->middle_down) impl_->middle_pressed = true;
                impl_->middle_down = true;
            } else if (button->detail == 3) {
''')
replace_once(
    "src/window_xcb.cpp",
    '''            if (button->detail == 1) {
                impl_->primary_down = false;
            } else if (button->detail == 3) {
''',
    '''            if (button->detail == 1) {
                impl_->primary_down = false;
            } else if (button->detail == 2) {
                impl_->middle_down = false;
            } else if (button->detail == 3) {
''')
replace_once(
    "src/window_xcb.cpp",
    '''            } else if (keysym == keysym_r || keysym == keysym_upper_r) {
                impl_->reset = true;
            } else if (keysym == keysym_right_bracket) {
''',
    '''            } else if (keysym == keysym_r || keysym == keysym_upper_r) {
                impl_->reset = true;
            } else if (keysym == keysym_f || keysym == keysym_upper_f) {
                impl_->fill = true;
            } else if (keysym == keysym_right_bracket) {
''')
replace_once(
    "src/window_xcb.cpp",
    "    impl_->primary_pressed = false;\n    impl_->secondary_pressed = false;\n",
    "    impl_->primary_pressed = false;\n    impl_->secondary_pressed = false;\n    impl_->middle_pressed = false;\n")
replace_once(
    "src/window_xcb.cpp",
    "    impl_->reset = false;\n    impl_->save_scene = false;\n",
    "    impl_->reset = false;\n    impl_->fill = false;\n    impl_->save_scene = false;\n")
replace_once(
    "src/window_xcb.cpp",
    "        .primary_down = impl_->primary_down,\n        .secondary_down = impl_->secondary_down,\n"
    "        .primary_pressed = impl_->primary_pressed,\n        .secondary_pressed = impl_->secondary_pressed,\n",
    "        .primary_down = impl_->primary_down,\n        .secondary_down = impl_->secondary_down,\n"
    "        .middle_down = impl_->middle_down,\n        .primary_pressed = impl_->primary_pressed,\n"
    "        .secondary_pressed = impl_->secondary_pressed,\n        .middle_pressed = impl_->middle_pressed,\n")
replace_once(
    "src/window_xcb.cpp",
    "        .reset = impl_->reset,\n        .save_scene = impl_->save_scene,\n",
    "        .reset = impl_->reset,\n        .fill = impl_->fill,\n        .save_scene = impl_->save_scene,\n")

replace_once(
    "src/app.cpp",
    "    WindowInput input{};\n    bool ready_title_applied = false;\n",
    "    WindowInput input{};\n"
    "    bool pan_dragging = false;\n"
    "    std::int32_t pan_last_x = 0;\n"
    "    std::int32_t pan_last_y = 0;\n"
    "    std::int64_t pan_remainder_x = 0;\n"
    "    std::int64_t pan_remainder_y = 0;\n"
    "    bool ready_title_applied = false;\n")
replace_once(
    "src/app.cpp",
    "        if (input.save_scene) shared_state.save_scene_image.store(true, std::memory_order_release);\n"
    "        if (input.load_scene) shared_state.load_scene_image.store(true, std::memory_order_release);\n",
    "        if (input.save_scene) shared_state.save_scene_image.store(true, std::memory_order_release);\n"
    "        if (input.load_scene) shared_state.load_scene_image.store(true, std::memory_order_release);\n"
    "        if (input.fill) shared_state.fill_region.store(true, std::memory_order_release);\n")
replace_once(
    "src/app.cpp",
    '''        if (input.wheel_delta != 0 && over_simulation)
            zoom_at_pointer(shared_state, simulation_config, simulation_viewport,
                            input.mouse_x, input.mouse_y, input.wheel_delta);

''',
    '''        if (input.wheel_delta != 0 && over_simulation)
            zoom_at_pointer(shared_state, simulation_config, simulation_viewport,
                            input.mouse_x, input.mouse_y, input.wheel_delta);

        const auto zoom = shared_state.camera_zoom.load(std::memory_order_relaxed);
        if (zoom > 1u && input.middle_down && (over_simulation || pan_dragging)) {
            if (!pan_dragging) {
                pan_dragging = true;
                pan_last_x = input.mouse_x;
                pan_last_y = input.mouse_y;
                pan_remainder_x = 0;
                pan_remainder_y = 0;
            } else {
                const auto view = camera_view(shared_state, simulation_config, zoom);
                const auto viewport_width = (std::max)(
                    static_cast<std::int64_t>(simulation_viewport.rect.size.x), 1ll);
                const auto viewport_height = (std::max)(
                    static_cast<std::int64_t>(simulation_viewport.rect.size.y), 1ll);
                const auto dx = input.mouse_x - pan_last_x;
                const auto dy = input.mouse_y - pan_last_y;
                pan_last_x = input.mouse_x;
                pan_last_y = input.mouse_y;

                // Four-to-one damping keeps zoomed panning deliberate instead of jumpy.
                pan_remainder_x += -static_cast<std::int64_t>(dx) * view.width;
                pan_remainder_y += -static_cast<std::int64_t>(dy) * view.height;
                const auto denominator_x = viewport_width * 4;
                const auto denominator_y = viewport_height * 4;
                const auto shift_x = pan_remainder_x / denominator_x;
                const auto shift_y = pan_remainder_y / denominator_y;
                pan_remainder_x %= denominator_x;
                pan_remainder_y %= denominator_y;

                const auto half_width = static_cast<int>(view.width / 2u);
                const auto half_height = static_cast<int>(view.height / 2u);
                const auto min_x = half_width;
                const auto min_y = half_height;
                const auto max_x = static_cast<int>(simulation_config.grid_width) -
                                   static_cast<int>(view.width - view.width / 2u);
                const auto max_y = static_cast<int>(simulation_config.grid_height) -
                                   static_cast<int>(view.height - view.height / 2u);
                shared_state.camera_center_x.store(std::clamp(
                    shared_state.camera_center_x.load(std::memory_order_relaxed) +
                        static_cast<int>(shift_x), min_x, max_x), std::memory_order_relaxed);
                shared_state.camera_center_y.store(std::clamp(
                    shared_state.camera_center_y.load(std::memory_order_relaxed) +
                        static_cast<int>(shift_y), min_y, max_y), std::memory_order_relaxed);
            }
        } else {
            pan_dragging = false;
            pan_remainder_x = 0;
            pan_remainder_y = 0;
        }

''')

replace_once(
    "tools/generate_ui_text.py",
    '    "CURSOR", "CIRCLE", "SQUARE", "H LINE", "V LINE", "SIZE", "ZOOM", "BEE ACTIVE",\n',
    '    "CURSOR", "CIRCLE", "SQUARE", "H LINE", "V LINE", "SIZE", "ZOOM", "BEE ACTIVE",\n'
    '    "MMB DRAG PAN", "F FILL",\n')
replace_once(
    "shaders/fullscreen.frag",
    '''        uint keymapTop = eraserBottom + 3u;
        uint keymapBottom = keymapTop + 108u;
''',
    '''        uint keymapTop = eraserBottom + 3u;
        uint keymapBottom = keymapTop + 126u;
''')
replace_once(
    "shaders/fullscreen.frag",
    '''            uint leftIds[6] = uint[6](62u, 63u, 64u, 69u, 60u, 61u);
            uint rightIds[5] = uint[5](70u, 71u, 72u, 73u, 74u);
''',
    '''            uint leftIds[7] = uint[7](62u, 63u, 64u, 69u, 60u, 61u, 106u);
            uint rightIds[6] = uint[6](70u, 71u, 72u, 73u, 74u, 107u);
''')
replace_once(
    "shaders/fullscreen.frag",
    '''            for (uint i = 0u; i < 6u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 13u)), 1, leftIds[i]);
            for (uint i = 0u; i < 5u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(columnMiddle + 8u), int(keymapTop + 25u + i * 13u)), 1, rightIds[i]);
''',
    '''            for (uint i = 0u; i < 7u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 14u)), 1, leftIds[i]);
            for (uint i = 0u; i < 6u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(columnMiddle + 8u), int(keymapTop + 25u + i * 14u)), 1, rightIds[i]);
''')

print("Applied Fix35 middle-drag pan, F fill input, and readable keymap patch.")
