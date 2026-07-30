#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one literal match, found {count}")
    write(path, text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{path}: expected one regex match, found {count}")
    write(path, updated)


write("shaders/bee_swarm.glsl", '#ifndef EPOCH_SAND_BEE_SWARM_GLSL\n#define EPOCH_SAND_BEE_SWARM_GLSL\n\nconst uint BEE_FORMATION_COUNT = 200u;\nconst uint BEE_TARGET_NONE = 0xffffu;\nconst uint BEE_AUX_QUEEN = 0x40000000u;\nconst uint BEE_AUX_POLLEN = 0x20000000u;\nconst uint BEE_AUX_FED = 0x10000000u;\nconst uint BEE_AUX_SWARM = 0x08000000u;\nconst uint BEE_AUX_MIGRATING = 0x02000000u;\nconst uint BEE_METADATA_MASK = 0x00ffffffu;\n\nconst uint BEE_BIOHAZARD_PACKED[BEE_FORMATION_COUNT] = uint[](\n    3002u, 3005u, 3007u, 3009u, 3012u, 3014u, 3129u, 3144u, 3255u, 3273u,\n    3382u, 3403u, 3508u, 3532u, 3634u, 3662u, 3761u, 3919u, 4016u, 4048u,\n    4143u, 4305u, 4398u, 4434u, 4653u, 4691u, 4909u, 4947u, 5293u, 5331u,\n    5421u, 5460u, 5567u, 5572u, 5715u, 5805u, 5951u, 5956u, 5971u, 6061u,\n    6226u, 6318u, 6334u, 6339u, 6482u, 6575u, 6704u, 6718u, 6723u, 6737u,\n    6864u, 6961u, 6976u, 7090u, 7100u, 7103u, 7106u, 7119u, 7123u, 7208u,\n    7211u, 7213u, 7215u, 7217u, 7227u, 7236u, 7238u, 7245u, 7247u, 7249u,\n    7254u, 7256u, 7332u, 7338u, 7355u, 7364u, 7380u, 7386u, 7462u, 7465u,\n    7475u, 7490u, 7496u, 7508u, 7517u, 7591u, 7594u, 7604u, 7619u, 7625u,\n    7637u, 7646u, 7720u, 7723u, 7733u, 7748u, 7754u, 7766u, 7775u, 7849u,\n    7852u, 7862u, 7877u, 7883u, 7895u, 7904u, 7978u, 7981u, 7991u, 8006u,\n    8012u, 8024u, 8033u, 8111u, 8121u, 8136u, 8142u, 8154u, 8163u, 8241u,\n    8251u, 8266u, 8272u, 8284u, 8293u, 8371u, 8381u, 8396u, 8402u, 8414u,\n    8423u, 8501u, 8511u, 8526u, 8532u, 8544u, 8553u, 8631u, 8641u, 8656u,\n    8662u, 8674u, 8683u, 8761u, 8771u, 8786u, 8792u, 8804u, 8813u, 8891u,\n    8901u, 8916u, 8922u, 8934u, 8943u, 9021u, 9031u, 9046u, 9052u, 9064u,\n    9073u, 9151u, 9161u, 9176u, 9182u, 9194u, 9203u, 9281u, 9291u, 9306u,\n    9312u, 9324u, 9333u, 9411u, 9421u, 9436u, 9442u, 9454u, 9463u, 9541u,\n    9551u, 9566u, 9572u, 9584u, 9593u, 9671u, 9681u, 9696u, 9702u, 9714u,\n    9723u, 9801u, 9811u, 9826u, 9832u, 9844u, 9853u, 9931u, 9941u, 9956u,\n    9962u, 9974u, 9983u, 10061u, 10071u, 10086u, 10092u, 10104u, 10113u, 10191u,\n    10201u, 10216u, 10222u, 10234u, 10243u, 10321u, 10331u, 10346u, 10352u, 10364u,\n    10373u, 10451u, 10461u, 10476u, 10482u, 10494u, 10503u, 10581u, 10591u, 10606u,\n    10612u, 10624u, 10633u, 10711u, 10721u, 10736u, 10742u, 10754u, 10763u, 10841u,\n    10851u, 10866u, 10872u, 10884u, 10893u, 10971u, 10981u, 10996u, 11002u, 11014u,\n    11023u, 11101u, 11111u, 11126u, 11132u, 11144u, 11153u, 11231u, 11241u, 11256u,\n    11262u, 11274u, 11283u, 11361u, 11371u, 11386u, 11392u, 11404u, 11413u, 11491u,\n    11501u, 11516u, 11522u, 11534u, 11543u, 11621u, 11631u, 11646u, 11652u, 11664u,\n    11673u, 11751u, 11761u, 11776u, 11782u, 11794u, 11803u, 11881u, 11891u, 11906u,\n    11912u, 11924u, 11933u, 12011u, 12021u, 12036u, 12042u, 12054u, 12063u, 12141u,\n    12151u, 12166u, 12172u, 12184u, 12193u\n);\n\nivec2 beeFormationOffset(uint slot) {\n    uint packed = BEE_BIOHAZARD_PACKED[min(slot, BEE_FORMATION_COUNT - 1u)];\n    return ivec2(int(packed & 127u) - 64, int(packed >> 7u) - 64);\n}\n\nint beeFormationSlotFromOffset(ivec2 offset) {\n    if (offset.x < -64 || offset.x > 63 || offset.y < -64 || offset.y > 63) return -1;\n    uint key = (uint(offset.y + 64) << 7u) | uint(offset.x + 64);\n    int low = 0;\n    int high = int(BEE_FORMATION_COUNT) - 1;\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    if (low <= high) {\n        int middle = (low + high) / 2;\n        uint middleKey = BEE_BIOHAZARD_PACKED[middle];\n        if (key == middleKey) return middle;\n        if (key < middleKey) high = middle - 1;\n        else low = middle + 1;\n    }\n    return -1;\n}\n\nuint beeFormationSlotFromAux(uint aux) { return (aux >> 15u) & 255u; }\n\nivec2 beeHomeCenterFromAux(uint aux) {\n    return ivec2(int(aux & 255u) * 4, int((aux >> 8u) & 127u) * 4);\n}\n\nuint beePackMetadata(uint aux, ivec2 homeCenter, uint slot) {\n    uint homeX = uint(clamp(homeCenter.x / 4, 0, 255));\n    uint homeY = uint(clamp(homeCenter.y / 4, 0, 127));\n    uint metadata = homeX | (homeY << 8u) | ((slot & 255u) << 15u);\n    return (aux & ~BEE_METADATA_MASK) | metadata;\n}\n\nuint beeTimerFromAge(uint age) { return age & 0xffffu; }\nuint beeTargetTileFromAge(uint age) { return age >> 16u; }\nuint beePackAge(uint timer, uint targetTile) {\n    return min(timer, 0xffffu) | (min(targetTile, BEE_TARGET_NONE) << 16u);\n}\n\nbool beeIsForager(uint aux) {\n    uint slot = beeFormationSlotFromAux(aux);\n    return ((slot * 37u + 11u) % 10u) == 0u;\n}\n\nivec2 beeRotateOffset(ivec2 offset, uint phase) {\n    switch (phase & 15u) {\n    case 0u: return ivec2((offset.x * 256 - offset.y * 0) / 256, (offset.x * 0 + offset.y * 256) / 256);\n    case 1u: return ivec2((offset.x * 237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * 237) / 256);\n    case 2u: return ivec2((offset.x * 181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * 181) / 256);\n    case 3u: return ivec2((offset.x * 98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * 98) / 256);\n    case 4u: return ivec2((offset.x * 0 - offset.y * 256) / 256, (offset.x * 256 + offset.y * 0) / 256);\n    case 5u: return ivec2((offset.x * -98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * -98) / 256);\n    case 6u: return ivec2((offset.x * -181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * -181) / 256);\n    case 7u: return ivec2((offset.x * -237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * -237) / 256);\n    case 8u: return ivec2((offset.x * -256 - offset.y * 0) / 256, (offset.x * 0 + offset.y * -256) / 256);\n    case 9u: return ivec2((offset.x * -237 - offset.y * -98) / 256, (offset.x * -98 + offset.y * -237) / 256);\n    case 10u: return ivec2((offset.x * -181 - offset.y * -181) / 256, (offset.x * -181 + offset.y * -181) / 256);\n    case 11u: return ivec2((offset.x * -98 - offset.y * -237) / 256, (offset.x * -237 + offset.y * -98) / 256);\n    case 12u: return ivec2((offset.x * 0 - offset.y * -256) / 256, (offset.x * -256 + offset.y * 0) / 256);\n    case 13u: return ivec2((offset.x * 98 - offset.y * -237) / 256, (offset.x * -237 + offset.y * 98) / 256);\n    case 14u: return ivec2((offset.x * 181 - offset.y * -181) / 256, (offset.x * -181 + offset.y * 181) / 256);\n    case 15u: return ivec2((offset.x * 237 - offset.y * -98) / 256, (offset.x * -98 + offset.y * 237) / 256);\n    }\n    return offset;\n}\n\nivec2 beeOrbitTarget(uint aux, uint step) {\n    uint phase = step / 8u;\n    return beeHomeCenterFromAux(aux) +\n           beeRotateOffset(beeFormationOffset(beeFormationSlotFromAux(aux)), phase);\n}\n\nivec2 beeLandingOffset(uint slot) {\n    switch (slot & 15u) {\n    case 0u: return ivec2(13, 0);\n    case 1u: return ivec2(12, 5);\n    case 2u: return ivec2(9, 9);\n    case 3u: return ivec2(5, 12);\n    case 4u: return ivec2(0, 13);\n    case 5u: return ivec2(-5, 12);\n    case 6u: return ivec2(-9, 9);\n    case 7u: return ivec2(-12, 5);\n    case 8u: return ivec2(-13, 0);\n    case 9u: return ivec2(-12, -5);\n    case 10u: return ivec2(-9, -9);\n    case 11u: return ivec2(-5, -12);\n    case 12u: return ivec2(0, -13);\n    case 13u: return ivec2(5, -12);\n    case 14u: return ivec2(9, -9);\n    case 15u: return ivec2(12, -5);\n    }\n    return ivec2(13, 0);\n}\n\nint beeAxisSign(int value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }\n\nivec2 beeApproachPosition(ivec2 occupiedPosition, ivec2 fromPosition) {\n    ivec2 delta = fromPosition - occupiedPosition;\n    ivec2 direction = ivec2(beeAxisSign(delta.x), beeAxisSign(delta.y));\n    if (all(equal(direction, ivec2(0)))) direction = ivec2(1, 0);\n    return occupiedPosition + direction;\n}\n\nivec2 beeMigrationSite(ivec2 flowerPosition, uint width, uint height) {\n    ivec2 site = flowerPosition + ivec2(0, -16);\n    site = ivec2((site.x / 4) * 4, (site.y / 4) * 4);\n    return clamp(site, ivec2(16), ivec2(int(width) - 17, int(height) - 17));\n}\n\n#endif\n')

replace_once(
    "CMakeLists.txt",
    '            DEPENDS "${SHADER_INPUT}" "${SHADER_SOURCE_DIR}/materials.glsl"\n',
    '            DEPENDS "${SHADER_INPUT}" "${SHADER_SOURCE_DIR}/materials.glsl"\n'
    '                    "${SHADER_SOURCE_DIR}/bee_swarm.glsl"\n',
)

replace_once(
    "include/epoch/sand/window.hpp",
    "    void set_title(std::string_view title);\n",
    "    void set_title(std::string_view title);\n"
    "    void show_startup_message(std::string_view message);\n",
)

replace_once(
    "src/app.cpp",
    '    NativeWindow window{"SandHybrid - Loading", 1280, 720};\n'
    '    std::fprintf(stderr, "[EpochSand] Native window created.\\n");\n',
    '    NativeWindow window{"SandHybrid", 1280, 720};\n'
    '    window.show_startup_message("Compiling Shaders...");\n'
    '    std::fprintf(stderr, "[EpochSand] Native window created.\\n");\n',
)
replace_once(
    "src/app.cpp",
    '            window.set_title("SandHybrid");\n'
    '            ready_title_applied = true;\n',
    '            window.show_startup_message("");\n'
    '            window.set_title("SandHybrid");\n'
    '            ready_title_applied = true;\n',
)

replace_once(
    "src/window_win32.cpp",
    "    std::uint32_t height{};\n",
    "    std::uint32_t height{};\n"
    "    std::wstring startup_message;\n",
)
replace_once(
    "src/window_win32.cpp",
    "        switch (message) {\n"
    "        case WM_CLOSE:\n",
    """        switch (message) {
        case WM_ERASEBKGND:
            if (!self->startup_message.empty()) return 1;
            break;
        case WM_PAINT:
            if (!self->startup_message.empty()) {
                PAINTSTRUCT paint{};
                HDC deviceContext = BeginPaint(hwnd, &paint);
                RECT client{};
                GetClientRect(hwnd, &client);
                FillRect(deviceContext, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                SetBkMode(deviceContext, TRANSPARENT);
                SetTextColor(deviceContext, RGB(255, 255, 255));
                DrawTextW(deviceContext, self->startup_message.c_str(), -1, &client,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                EndPaint(hwnd, &paint);
                return 0;
            }
            break;
        case WM_CLOSE:
""",
)
replace_once(
    "src/window_win32.cpp",
    "    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));\n"
    "    window_class.lpszClassName = window_class_name;\n",
    "    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));\n"
    "    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));\n"
    "    window_class.lpszClassName = window_class_name;\n",
)
replace_once(
    "src/window_win32.cpp",
    """void NativeWindow::set_title(const std::string_view title) {
    const auto wide_title = widen(title);
    SetWindowTextW(impl_->handle, wide_title.c_str());
}
""",
    """void NativeWindow::set_title(const std::string_view title) {
    const auto wide_title = widen(title);
    SetWindowTextW(impl_->handle, wide_title.c_str());
}

void NativeWindow::show_startup_message(const std::string_view message) {
    impl_->startup_message = widen(message);
    if (impl_->startup_message.empty()) return;
    InvalidateRect(impl_->handle, nullptr, TRUE);
    UpdateWindow(impl_->handle);
}
""",
)

replace_once(
    "src/window_xcb.cpp",
    "    xcb_window_t window{};\n",
    "    xcb_window_t window{};\n"
    "    xcb_gcontext_t startup_gc{};\n"
    "    std::string startup_message;\n",
)
replace_once(
    "src/window_xcb.cpp",
    "    ~Impl() {\n"
    "        if (connection != nullptr && window != 0) {\n",
    """    void draw_startup_message() {
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
""",
)
replace_once(
    "src/window_xcb.cpp",
    '    impl_->wm_protocols = intern_atom(impl_->connection, "WM_PROTOCOLS");\n',
    """    impl_->startup_gc = xcb_generate_id(impl_->connection);
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
""",
)
replace_once(
    "src/window_xcb.cpp",
    "        switch (type) {\n"
    "        case XCB_CLIENT_MESSAGE: {\n",
    """        switch (type) {
        case XCB_EXPOSE:
            impl_->draw_startup_message();
            break;
        case XCB_CLIENT_MESSAGE: {
""",
)
replace_once(
    "src/window_xcb.cpp",
    """void NativeWindow::set_title(const std::string_view title) {
""",
    """void NativeWindow::show_startup_message(const std::string_view message) {
    impl_->startup_message.assign(message.data(), message.size());
    impl_->draw_startup_message();
}

void NativeWindow::set_title(const std::string_view title) {
""",
)

replace_once(
    "shaders/tiles.glsl",
    "const uint TILE_DAMAGED = 0x00000080u;\n",
    "const uint TILE_DAMAGED = 0x00000080u;\n"
    'const uint TILE_HAS_QUEEN = 0x00000100u;\nconst uint TILE_HAS_HIVE = 0x00000200u;\nconst uint TILE_HAS_FLOWER = 0x00000400u;\nconst uint TILE_HAS_HONEY = 0x00000800u;\nconst uint TILE_HAS_BEES = 0x00001000u;\nconst uint TILE_HAS_MIGRATING_QUEEN = 0x00002000u;\nconst uint TILE_BEE_HAZARD = 0x00004000u;\n',
)
replace_once(
    "shaders/tiles.glsl",
    "bool tileHas(TileState state, uint flag) { return (state.flags & flag) != 0u; }\n",
    "bool tileHas(TileState state, uint flag) { return (state.flags & flag) != 0u; }\n"
    '\nconst uint TILE_OCCUPANCY_MASK = 0x0000007fu;\nconst uint TILE_QUEEN_X_SHIFT = 7u;\nconst uint TILE_QUEEN_Y_SHIFT = 10u;\nconst uint TILE_FLOWER_X_SHIFT = 13u;\nconst uint TILE_FLOWER_Y_SHIFT = 16u;\nconst uint TILE_HONEY_X_SHIFT = 19u;\nconst uint TILE_HONEY_Y_SHIFT = 22u;\nconst uint TILE_BEE_COUNT_SHIFT = 25u;\n\nuint tileOccupancy(TileState state) { return state.occupancy & TILE_OCCUPANCY_MASK; }\nivec2 tileQueenLocal(TileState state) {\n    return ivec2(int((state.occupancy >> TILE_QUEEN_X_SHIFT) & 7u),\n                 int((state.occupancy >> TILE_QUEEN_Y_SHIFT) & 7u));\n}\nivec2 tileFlowerLocal(TileState state) {\n    return ivec2(int((state.occupancy >> TILE_FLOWER_X_SHIFT) & 7u),\n                 int((state.occupancy >> TILE_FLOWER_Y_SHIFT) & 7u));\n}\nivec2 tileHoneyLocal(TileState state) {\n    return ivec2(int((state.occupancy >> TILE_HONEY_X_SHIFT) & 7u),\n                 int((state.occupancy >> TILE_HONEY_Y_SHIFT) & 7u));\n}\nuint tileBeeCount(TileState state) { return (state.occupancy >> TILE_BEE_COUNT_SHIFT) & 127u; }\n\nuint packTileOccupancy(uint occupancy, ivec2 queenLocal, ivec2 flowerLocal,\n                       ivec2 honeyLocal, uint beeCount) {\n    return min(occupancy, 64u) |\n           (uint(clamp(queenLocal.x, 0, 7)) << TILE_QUEEN_X_SHIFT) |\n           (uint(clamp(queenLocal.y, 0, 7)) << TILE_QUEEN_Y_SHIFT) |\n           (uint(clamp(flowerLocal.x, 0, 7)) << TILE_FLOWER_X_SHIFT) |\n           (uint(clamp(flowerLocal.y, 0, 7)) << TILE_FLOWER_Y_SHIFT) |\n           (uint(clamp(honeyLocal.x, 0, 7)) << TILE_HONEY_X_SHIFT) |\n           (uint(clamp(honeyLocal.y, 0, 7)) << TILE_HONEY_Y_SHIFT) |\n           (min(beeCount, 127u) << TILE_BEE_COUNT_SHIFT);\n}\n\nivec2 tileOriginFromIndex(uint index, uint width) {\n    uint columns = tileColumns(width);\n    return ivec2(int(index % columns), int(index / columns)) * int(TILE_SIZE);\n}\nivec2 tileQueenPosition(uint index, uint width, TileState state) {\n    return tileOriginFromIndex(index, width) + tileQueenLocal(state);\n}\nivec2 tileFlowerPosition(uint index, uint width, TileState state) {\n    return tileOriginFromIndex(index, width) + tileFlowerLocal(state);\n}\nivec2 tileHoneyPosition(uint index, uint width, TileState state) {\n    return tileOriginFromIndex(index, width) + tileHoneyLocal(state);\n}\n',
)

replace_once(
    "shaders/tiles.comp",
    "    bool activeContent = false;\n",
    """    bool activeContent = false;
    bool hasQueen = false;
    bool hasHive = false;
    bool hasFlower = false;
    bool hasHoney = false;
    bool hasMigratingQueen = false;
    bool beeHazard = false;
    ivec2 queenLocal = ivec2(0);
    ivec2 flowerLocal = ivec2(0);
    ivec2 honeyLocal = ivec2(0);
    uint beeCount = 0u;
""",
)
replace_once(
    "shaders/tiles.comp",
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
""",
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
            ivec2 local = ivec2(x, y);
            if (cell.material == MAT_QUEEN_BEE) {
                if (!hasQueen) queenLocal = local;
                hasQueen = true;
            }
            if (cell.material == MAT_BEE_NEST) hasHive = true;
            if (cell.material == MAT_FLOWER) {
                if (!hasFlower) flowerLocal = local;
                hasFlower = true;
            }
            if (cell.material == MAT_HONEY) {
                if (!hasHoney) honeyLocal = local;
                hasHoney = true;
            }
            if (cell.material == MAT_BEE) {
                ++beeCount;
                if ((cell.aux & AUX_CHARGED) != 0u) {
                    if (!hasMigratingQueen) queenLocal = local;
                    hasMigratingQueen = true;
                }
            }
            beeHazard = beeHazard || cell.material == MAT_FIRE || cell.material == MAT_LAVA ||
                        cell.material == MAT_ACID || cell.material == MAT_SMOKE ||
                        cell.material == MAT_DIRTY_STEAM || cell.material == MAT_LIGHTNING ||
                        cell.material == MAT_RADIATION;
""",
)
replace_once(
    "shaders/tiles.comp",
    """            bool activeAgent = cell.material == MAT_BEE || cell.material == MAT_ANT ||
                                cell.material == MAT_BEETLE || cell.material == MAT_SEED ||
                                cell.material == MAT_POLLEN;
""",
    """            bool activeAgent = cell.material == MAT_BEE || cell.material == MAT_QUEEN_BEE ||
                                cell.material == MAT_BEE_NEST || cell.material == MAT_ANT ||
                                cell.material == MAT_BEETLE || cell.material == MAT_SEED ||
                                cell.material == MAT_POLLEN;
""",
)
replace_once(
    "shaders/tiles.comp",
    "                             previous.occupancy >= TILE_STABILITY_OCCUPANCY;\n",
    "                             tileOccupancy(previous) >= TILE_STABILITY_OCCUPANCY;\n",
)
replace_once(
    "shaders/tiles.comp",
    """    if (damaged) flags |= TILE_DAMAGED;

    uint stableCells = structuralTile ? dominantCount : occupied;
""",
    """    if (damaged) flags |= TILE_DAMAGED;
    if (hasQueen) flags |= TILE_HAS_QUEEN;
    if (hasHive) flags |= TILE_HAS_HIVE;
    if (hasFlower) flags |= TILE_HAS_FLOWER;
    if (hasHoney) flags |= TILE_HAS_HONEY;
    if (beeCount > 0u) flags |= TILE_HAS_BEES;
    if (hasMigratingQueen) flags |= TILE_HAS_MIGRATING_QUEEN;
    if (beeHazard) flags |= TILE_BEE_HAZARD;

    uint stableCells = structuralTile ? dominantCount : occupied;
""",
)
replace_once(
    "shaders/tiles.comp",
    """    tiles[index] = TileState(dominant, structuralTile ? dominantCount : occupied,
                              flags, packTileCounters(stableTicks, cooldown));
""",
    """    uint packedOccupancy = packTileOccupancy(
        structuralTile ? dominantCount : occupied,
        queenLocal, flowerLocal, honeyLocal, beeCount);
    tiles[index] = TileState(dominant, packedOccupancy,
                             flags, packTileCounters(stableTicks, cooldown));
""",
)

replace_once(
    "shaders/reset.comp",
    '#include "materials.glsl"\n',
    '#include "materials.glsl"\n#include "bee_swarm.glsl"\n',
)
replace_regex(
    "shaders/reset.comp",
    r"\nconst uint BEE_FORMATION_COUNT = 200u;.*?\nuint ecosystemMaterial",
    "\nuint ecosystemMaterial",
)
replace_regex(
    "shaders/reset.comp",
    r"""    // Canonical hive and exactly 200 deterministic bee slots\..*?
    int formationSlot = beeFormationSlot\(q\);
    if \(material == MAT_EMPTY && formationSlot >= 0\) material = MAT_BEE;
""",
    """    // Keep the compact Fix29 hive. The swarm is a separate 200-bee
    // biohazard silhouette, so hive geometry and bee motion remain independent.
    int queenX = width - 104;
    int queenGroundY = floorY - 54 + int(hash32(uint(queenX) * 31u) % 5u);
    ivec2 queen = ivec2(queenX, queenGroundY - 72);
    ivec2 q = p - queen;
    int q2 = q.x * q.x + q.y * q.y;
    if (q2 >= 28 && q2 < 108) material = MAT_BEE_NEST;
    if (q2 == 0) material = MAT_QUEEN_BEE;
    else if (q.x >= 1 && q.x <= 12 && abs(q.y) <= 1) material = MAT_EMPTY;
    else if (q2 < 28) {
        uint chamber = hash32(indexOf(p) ^ pc.seed ^ 0xb33u);
        material = (chamber & 3u) == 0u ? MAT_EMPTY
            : ((chamber & 4u) == 0u ? MAT_HONEY : MAT_POLLEN);
    }

    int formationSlot = beeFormationSlotFromOffset(q);
    if (material == MAT_EMPTY && formationSlot >= 0) material = MAT_BEE;
""",
)
replace_regex(
    "shaders/reset.comp",
    r"""    if \(scene == SCENE_ECOSYSTEM && material == MAT_BEE\) \{.*?
    \}

    // Machine controllers""",
    """    if (scene == SCENE_ECOSYSTEM && material == MAT_BEE) {
        int queenX = int(pc.width) - 104;
        int queenGroundY = int(pc.height) - 64 +
            int(hash32(uint(queenX) * 31u) % 5u);
        ivec2 queen = ivec2(queenX, queenGroundY - 72);
        int slot = beeFormationSlotFromOffset(p - queen);
        if (slot >= 0) {
            cell.aux |= BEE_AUX_SWARM | BEE_AUX_FED;
            cell.aux = beePackMetadata(cell.aux, queen, uint(slot));
            cell.age = beePackAge(uint((slot * 17) % 900), BEE_TARGET_NONE);
        }
    }

    // Machine controllers""",
)

replace_once(
    "shaders/chemistry.comp",
    '#include "tiles.glsl"\n',
    '#include "tiles.glsl"\n#include "bee_swarm.glsl"\n',
)
replace_regex(
    "shaders/chemistry.comp",
    r"bool hasHungryBee\(ivec2 p\) \{.*?\nint neighborTemperature",
    '\nuint beeTileCount() {\n    return tileColumns(pc.width) * ((pc.height + TILE_SIZE - 1u) / TILE_SIZE);\n}\n\nbool beeTileIndexValid(uint index) { return index < beeTileCount(); }\n\nbool beeTileMatches(uint index, uint requiredFlags, uint forbiddenFlags) {\n    if (!beeTileIndexValid(index)) return false;\n    TileState state = tiles[index];\n    return (state.flags & requiredFlags) == requiredFlags &&\n           (state.flags & forbiddenFlags) == 0u;\n}\n\nuint nearestBeeTile(ivec2 originPosition, uint requiredFlags, uint forbiddenFlags,\n                    int minimumTileDistance, int maximumTileDistance) {\n    uint columns = tileColumns(pc.width);\n    uint rows = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;\n    ivec2 originTile = ivec2(tileCoordinate(originPosition));\n    uint bestIndex = BEE_TARGET_NONE;\n    int bestDistance = 0x3fffffff;\n    for (uint y = 0u; y < rows; ++y) {\n        for (uint x = 0u; x < columns; ++x) {\n            ivec2 delta = ivec2(int(x), int(y)) - originTile;\n            int tileDistance = abs(delta.x) + abs(delta.y);\n            if (tileDistance < minimumTileDistance || tileDistance > maximumTileDistance) continue;\n            uint candidateIndex = y * columns + x;\n            if (!beeTileMatches(candidateIndex, requiredFlags, forbiddenFlags)) continue;\n            int distanceSquared = delta.x * delta.x + delta.y * delta.y;\n            if (distanceSquared < bestDistance ||\n                (distanceSquared == bestDistance && candidateIndex < bestIndex)) {\n                bestDistance = distanceSquared;\n                bestIndex = candidateIndex;\n            }\n        }\n    }\n    return bestIndex;\n}\n\nbool beeTileAreaHasFlag(uint centerIndex, uint wantedFlag, int radius) {\n    if (!beeTileIndexValid(centerIndex)) return false;\n    uint columns = tileColumns(pc.width);\n    uint rows = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;\n    ivec2 center = ivec2(int(centerIndex % columns), int(centerIndex / columns));\n    for (int y = -radius; y <= radius; ++y) {\n        for (int x = -radius; x <= radius; ++x) {\n            ivec2 candidate = center + ivec2(x, y);\n            if (candidate.x < 0 || candidate.y < 0 ||\n                candidate.x >= int(columns) || candidate.y >= int(rows)) continue;\n            TileState state = tiles[uint(candidate.y) * columns + uint(candidate.x)];\n            if (tileHas(state, wantedFlag)) return true;\n        }\n    }\n    return false;\n}\n\nuint beePopulationAroundTile(uint centerIndex, int radius) {\n    if (!beeTileIndexValid(centerIndex)) return 0u;\n    uint columns = tileColumns(pc.width);\n    uint rows = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;\n    ivec2 center = ivec2(int(centerIndex % columns), int(centerIndex / columns));\n    uint count = 0u;\n    for (int y = -radius; y <= radius; ++y) {\n        for (int x = -radius; x <= radius; ++x) {\n            ivec2 candidate = center + ivec2(x, y);\n            if (candidate.x < 0 || candidate.y < 0 ||\n                candidate.x >= int(columns) || candidate.y >= int(rows)) continue;\n            count += tileBeeCount(tiles[uint(candidate.y) * columns + uint(candidate.x)]);\n        }\n    }\n    return count;\n}\n\nuint refreshedMigratingQueenTile(uint previousTarget, ivec2 searchOrigin) {\n    if (beeTileIndexValid(previousTarget) &&\n        (tiles[previousTarget].flags & (TILE_HAS_MIGRATING_QUEEN | TILE_HAS_QUEEN)) != 0u)\n        return previousTarget;\n    return nearestBeeTile(searchOrigin, TILE_HAS_MIGRATING_QUEEN, 0u, 0, 64);\n}\n\nuint nearestFlowerTileForColony(ivec2 homeCenter) {\n    return nearestBeeTile(homeCenter, TILE_HAS_FLOWER,\n                          TILE_HAS_HIVE | TILE_HAS_QUEEN | TILE_HAS_MIGRATING_QUEEN |\n                          TILE_BEE_HAZARD,\n                          0, 48);\n}\n\nuint nearestHoneyTileForColony(ivec2 homeCenter) {\n    return nearestBeeTile(homeCenter, TILE_HAS_HONEY, TILE_BEE_HAZARD, 0, 12);\n}\n\nuint migrationDestinationTile(ivec2 queenPosition) {\n    return nearestBeeTile(queenPosition, TILE_HAS_FLOWER,\n                          TILE_HAS_HIVE | TILE_HAS_QUEEN | TILE_HAS_MIGRATING_QUEEN |\n                          TILE_BEE_HAZARD,\n                          8, 56);\n}\n\nbool beeHomeHasQueen(Cell bee) {\n    uint homeTile = tileIndex(beeHomeCenterFromAux(bee.aux), pc.width);\n    return beeTileAreaHasFlag(homeTile, TILE_HAS_QUEEN, 1);\n}\n\nbool hungryBeeTargetsHoney(ivec2 honeyPosition) {\n    uint honeyTile = tileIndex(honeyPosition, pc.width);\n    for (uint i = 0u; i < 8u; ++i) {\n        ivec2 beePosition = honeyPosition - neighborOffsets[i];\n        Cell bee = at(beePosition);\n        if (bee.material == MAT_BEE &&\n            (bee.aux & BEE_AUX_POLLEN) == 0u &&\n            (bee.aux & BEE_AUX_FED) == 0u &&\n            beeTargetTileFromAge(bee.age) == honeyTile &&\n            beeTimerFromAge(bee.age) == 0u) return true;\n    }\n    return false;\n}\n\nbool beeBesideTargetMaterial(ivec2 beePosition, uint targetTile, uint material) {\n    if (!beeTileIndexValid(targetTile)) return false;\n    for (uint i = 0u; i < 8u; ++i) {\n        ivec2 candidate = beePosition + neighborOffsets[i];\n        if (at(candidate).material == material &&\n            tileIndex(candidate, pc.width) == targetTile) return true;\n    }\n    return false;\n}\n' + "\nint neighborTemperature",
)
replace_once(
    "shaders/chemistry.comp",
    "        tile.occupancy >= TILE_MIN_COHESIVE_CELLS &&\n"
    "        tile.occupancy < TILE_STABILITY_OCCUPANCY) {\n",
    "        tileOccupancy(tile) >= TILE_MIN_COHESIVE_CELLS &&\n"
    "        tileOccupancy(tile) < TILE_STABILITY_OCCUPANCY) {\n",
)
replace_once(
    "shaders/chemistry.comp",
    "        uint limitedHealth = max(64u, tile.occupancy * 255u / TILE_STABILITY_OCCUPANCY);\n",
    "        uint limitedHealth = max(64u, tileOccupancy(tile) * 255u / TILE_STABILITY_OCCUPANCY);\n",
)
replace_once(
    "shaders/chemistry.comp",
    """                    result = makeCell(MAT_BEE);
                    handledNest = true;
""",
    """                    result = makeCell(MAT_BEE);
                    uint newbornSlot = hash32(indexOf(p) ^ pc.seed ^ result.aux) % BEE_FORMATION_COUNT;
                    result.aux |= BEE_AUX_SWARM | BEE_AUX_FED;
                    result.aux = beePackMetadata(result.aux, p + queenOffset, newbornSlot);
                    result.age = beePackAge(uint(newbornSlot * 13u) % 900u, BEE_TARGET_NONE);
                    handledNest = true;
""",
)
replace_regex(
    "shaders/chemistry.comp",
    r"""    // Exactly 200 authored bees retain stable slots\..*?
    \} else if \(source\.material == MAT_BEESWAX\) \{""",
    '\n    // Authored colonies use explicit targets stored in each bee. Orbiting bees do\n    // no searches; only a lifecycle transition scans compact 8x8 tile metadata.\n    if (source.material == MAT_BEE) {\n        bool authoredBee = (source.aux & BEE_AUX_SWARM) != 0u;\n        bool queenCarrier = (source.aux & BEE_AUX_QUEEN) != 0u;\n        bool migratingFollower = (source.aux & BEE_AUX_MIGRATING) != 0u;\n        uint timer = min(beeTimerFromAge(source.age) + 1u, 0xffffu);\n        uint targetTile = beeTargetTileFromAge(source.age);\n        result.age = beePackAge(timer, targetTile);\n\n        if (nearFire || nearLava || hasNeighbor(p, MAT_LIGHTNING)) {\n            result = makeCell(MAT_ASH);\n        } else if (nearAcid && (randomValue & 3u) == 0u) {\n            result = makeCell(MAT_WASTE);\n        } else if ((nearSmoke || hasNeighbor(p, MAT_DIRTY_STEAM) ||\n                    hasNeighbor(p, MAT_RADIATION)) && (randomValue & 127u) == 0u) {\n            result = makeCell(MAT_WASTE);\n        } else if (queenCarrier) {\n            if (!beeTileMatches(targetTile, TILE_HAS_FLOWER,\n                                TILE_HAS_HIVE | TILE_HAS_QUEEN |\n                                TILE_HAS_MIGRATING_QUEEN | TILE_BEE_HAZARD)) {\n                targetTile = migrationDestinationTile(p);\n                result.age = beePackAge(0u, targetTile);\n                timer = 0u;\n            }\n            if (beeTileIndexValid(targetTile)) {\n                ivec2 destination = beeMigrationSite(\n                    tileFlowerPosition(targetTile, pc.width, tiles[targetTile]),\n                    pc.width, pc.height);\n                ivec2 delta = p - destination;\n                bool colonyArrived = delta.x * delta.x + delta.y * delta.y <= 4 &&\n                                      countWithin(p, MAT_BEE, 8) >= 10u;\n                if (colonyArrived && timer >= 180u &&\n                    !hasWithin(p, MAT_QUEEN_BEE, 12) &&\n                    !hasWithin(p, MAT_BEE_NEST, 10)) {\n                    result = makeCell(MAT_QUEEN_BEE);\n                    setStateValue(result, 0u);\n                    result.age = 0u;\n                }\n            }\n        } else {\n            ivec2 nearbyQueen = nearestOffset(p, MAT_QUEEN_BEE, 6);\n            if (any(notEqual(nearbyQueen, ivec2(0)))) {\n                ivec2 queenPosition = p + nearbyQueen;\n                uint slot = authoredBee ? beeFormationSlotFromAux(source.aux)\n                                        : hash32(indexOf(p) ^ source.aux) % BEE_FORMATION_COUNT;\n                result.aux = beePackMetadata(\n                    result.aux | BEE_AUX_SWARM | BEE_AUX_FED,\n                    queenPosition, slot);\n                result.aux &= ~BEE_AUX_MIGRATING;\n                result.age = beePackAge(0u, BEE_TARGET_NONE);\n                authoredBee = true;\n                migratingFollower = false;\n                timer = 0u;\n                targetTile = BEE_TARGET_NONE;\n            }\n\n            if (authoredBee && !migratingFollower && !beeHomeHasQueen(result) &&\n                (timer & 63u) == 0u) {\n                uint queenTile = refreshedMigratingQueenTile(BEE_TARGET_NONE, p);\n                if (queenTile != BEE_TARGET_NONE) {\n                    result.aux |= BEE_AUX_MIGRATING;\n                    result.age = beePackAge(0u, queenTile);\n                    migratingFollower = true;\n                    targetTile = queenTile;\n                    timer = 0u;\n                }\n            } else if (migratingFollower) {\n                uint queenTile = refreshedMigratingQueenTile(targetTile, p);\n                if (queenTile != BEE_TARGET_NONE) {\n                    result.age = beePackAge(timer, queenTile);\n                    targetTile = queenTile;\n                }\n            }\n\n            if (authoredBee && !migratingFollower) {\n                bool carryingPollen = (source.aux & BEE_AUX_POLLEN) != 0u;\n                bool fed = (source.aux & BEE_AUX_FED) != 0u;\n                uint slot = beeFormationSlotFromAux(source.aux);\n                ivec2 homeCenter = beeHomeCenterFromAux(source.aux);\n\n                if (fed && !carryingPollen && targetTile == BEE_TARGET_NONE &&\n                    beeIsForager(source.aux) &&\n                    timer >= 1200u + ((slot * 29u) % 600u)) {\n                    uint flowerTile = nearestFlowerTileForColony(homeCenter);\n                    if (flowerTile != BEE_TARGET_NONE) {\n                        result.age = beePackAge(0u, flowerTile);\n                        targetTile = flowerTile;\n                        timer = 0u;\n                    }\n                }\n\n                if (fed && !carryingPollen && targetTile != BEE_TARGET_NONE) {\n                    if (!beeTileMatches(targetTile, TILE_HAS_FLOWER, TILE_BEE_HAZARD)) {\n                        result.age = beePackAge(0u, BEE_TARGET_NONE);\n                        targetTile = BEE_TARGET_NONE;\n                    } else if (beeBesideTargetMaterial(p, targetTile, MAT_FLOWER)) {\n                        result.aux |= BEE_AUX_POLLEN;\n                        result.aux &= ~BEE_AUX_FED;\n                        result.age = beePackAge(0u, BEE_TARGET_NONE);\n                        carryingPollen = true;\n                        fed = false;\n                        targetTile = BEE_TARGET_NONE;\n                        timer = 0u;\n                    }\n                }\n\n                if (carryingPollen &&\n                    (hasNeighbor(p, MAT_QUEEN_BEE) || hasNeighbor(p, MAT_BEE_NEST))) {\n                    ivec2 depositTarget = beeDepositTarget(p, source);\n                    uint depositMaterial = at(depositTarget).material;\n                    if (depositMaterial == MAT_EMPTY || depositMaterial == MAT_POLLEN) {\n                        result.aux &= ~(BEE_AUX_POLLEN | BEE_AUX_FED);\n                        uint honeyTile = nearestHoneyTileForColony(homeCenter);\n                        result.age = beePackAge(0u, honeyTile);\n                        carryingPollen = false;\n                        fed = false;\n                        targetTile = honeyTile;\n                        timer = 0u;\n                    }\n                }\n\n                if (!carryingPollen && !fed) {\n                    if (!beeTileMatches(targetTile, TILE_HAS_HONEY, TILE_BEE_HAZARD)) {\n                        targetTile = nearestHoneyTileForColony(homeCenter);\n                        result.age = beePackAge(0u, targetTile);\n                        timer = 0u;\n                    }\n                    bool besideTargetHoney =\n                        beeBesideTargetMaterial(p, targetTile, MAT_HONEY);\n                    if (besideTargetHoney) {\n                        if (timer == 0u) {\n                            result.age = beePackAge(1u, targetTile);\n                        } else {\n                            result.aux |= BEE_AUX_FED;\n                            result.age = beePackAge(0u, BEE_TARGET_NONE);\n                        }\n                    }\n                }\n            }\n\n            if (!authoredBee) {\n                // A painted or orphaned bee without a queen does not wander.\n                result.aux &= ~AUX_MOVED;\n            }\n        }\n    } else if (source.material == MAT_POLLEN) {\n        if ((hasNeighbor(p, MAT_QUEEN_BEE) || hasNeighbor(p, MAT_BEE_NEST)) &&\n            source.age > 90u && (randomValue & 63u) == 0u) {\n            result = makeCell(MAT_HONEY);\n            setStateValue(result, 255u);\n        }\n    } else if (source.material == MAT_HONEY) {\n        if (stateValue(result) == 0u) setStateValue(result, 255u);\n        if (hungryBeeTargetsHoney(p)) {\n            uint remaining = stateValue(result);\n            uint portion = min(remaining, 26u);\n            if (remaining <= portion) result = makeCell(MAT_EMPTY);\n            else setStateValue(result, remaining - portion);\n        } else if (hasNeighbor(p, MAT_BEE) && source.age > 7200u &&\n                   (randomValue & 32767u) == 0u) {\n            result = makeCell(MAT_BEESWAX);\n        }\n    } else if (source.material == MAT_BEESWAX) {\n',
)
replace_regex(
    "shaders/chemistry.comp",
    r"""    \} else if \(source\.material == MAT_QUEEN_BEE\) \{.*?
    \} else if \(source\.material == MAT_BEE_NEST\) \{""",
    '    } else if (source.material == MAT_QUEEN_BEE) {\n        if (nearLava || nearFire || nearAcid || result.temperature > 160) {\n            result = makeCell(MAT_ASH);\n        } else {\n            uint stress = stateValue(source);\n            uint queenTile = tileIndex(p, pc.width);\n            uint localBees = beePopulationAroundTile(queenTile, 6);\n            bool localHoney = beeTileAreaHasFlag(queenTile, TILE_HAS_HONEY, 2);\n            bool reproductiveSwarm = localBees >= 180u && localHoney;\n            bool resourceFailure = !localHoney;\n\n            if ((randomValue & 255u) == 0u) {\n                if (reproductiveSwarm || resourceFailure) stress = min(255u, stress + 8u);\n                else stress = stress > 0u ? stress - 1u : 0u;\n            }\n            setStateValue(result, stress);\n\n            uint destinationTile = BEE_TARGET_NONE;\n            if (stress >= 232u && source.age > 36000u)\n                destinationTile = migrationDestinationTile(p);\n            if (stress >= 240u && destinationTile != BEE_TARGET_NONE) {\n                result = makeCell(MAT_BEE);\n                result.aux |= BEE_AUX_SWARM | BEE_AUX_QUEEN | BEE_AUX_FED;\n                result.aux = beePackMetadata(result.aux, p, 0u);\n                result.age = beePackAge(0u, destinationTile);\n            } else if (source.age > 900000u && (randomValue & 16383u) == 0u) {\n                result = makeCell(MAT_WASTE);\n            }\n        }\n    } else if (source.material == MAT_BEE_NEST) {',
)

replace_once(
    "shaders/move.comp",
    '#include "tiles.glsl"\n',
    '#include "tiles.glsl"\n#include "bee_swarm.glsl"\n',
)
replace_regex(
    "shaders/move.comp",
    r"int regionalTargetSignal\(ivec2 p, uint first, uint second\) \{.*?\nbool magneticMoveAllowed",
    '\nuint beeTargetTileCount() {\n    return tileColumns(movePc.width) * ((movePc.height + TILE_SIZE - 1u) / TILE_SIZE);\n}\n\nbool beeTargetTileValid(uint index, uint requiredFlags) {\n    return index < beeTargetTileCount() && tileHas(tiles[index], requiredFlags);\n}\n\nivec2 beeStoredFlowerTarget(Cell bee) {\n    uint targetTile = beeTargetTileFromAge(bee.age);\n    if (!beeTargetTileValid(targetTile, TILE_HAS_FLOWER)) return beeOrbitTarget(bee.aux, movePc.step);\n    TileState targetState = tiles[targetTile];\n    ivec2 flower = tileFlowerPosition(targetTile, movePc.width, targetState);\n    return beeApproachPosition(flower, beeHomeCenterFromAux(bee.aux));\n}\n\nivec2 beeStoredHoneyTarget(Cell bee) {\n    uint targetTile = beeTargetTileFromAge(bee.age);\n    if (!beeTargetTileValid(targetTile, TILE_HAS_HONEY))\n        return beeHomeCenterFromAux(bee.aux) + beeLandingOffset(beeFormationSlotFromAux(bee.aux));\n    TileState targetState = tiles[targetTile];\n    ivec2 honey = tileHoneyPosition(targetTile, movePc.width, targetState);\n    return beeApproachPosition(honey, beeHomeCenterFromAux(bee.aux));\n}\n\nivec2 beeStoredMigratingQueen(Cell bee) {\n    uint targetTile = beeTargetTileFromAge(bee.age);\n    if (!beeTargetTileValid(targetTile, TILE_HAS_MIGRATING_QUEEN) &&\n        !beeTargetTileValid(targetTile, TILE_HAS_QUEEN))\n        return beeHomeCenterFromAux(bee.aux);\n    TileState targetState = tiles[targetTile];\n    return tileQueenPosition(targetTile, movePc.width, targetState);\n}\n\nivec2 beeMovementTarget(Cell bee) {\n    uint slot = beeFormationSlotFromAux(bee.aux);\n    if ((bee.aux & BEE_AUX_QUEEN) != 0u) {\n        uint targetTile = beeTargetTileFromAge(bee.age);\n        if (!beeTargetTileValid(targetTile, TILE_HAS_FLOWER))\n            return beeHomeCenterFromAux(bee.aux);\n        TileState targetState = tiles[targetTile];\n        return beeMigrationSite(tileFlowerPosition(targetTile, movePc.width, targetState),\n                                movePc.width, movePc.height);\n    }\n    if ((bee.aux & BEE_AUX_MIGRATING) != 0u) {\n        ivec2 queen = beeStoredMigratingQueen(bee);\n        return queen + beeRotateOffset(beeFormationOffset(slot) / 2, movePc.step / 8u);\n    }\n    if ((bee.aux & BEE_AUX_POLLEN) != 0u)\n        return beeHomeCenterFromAux(bee.aux) + beeLandingOffset(slot);\n    if ((bee.aux & BEE_AUX_FED) == 0u)\n        return beeStoredHoneyTarget(bee);\n    if (beeTargetTileFromAge(bee.age) != BEE_TARGET_NONE)\n        return beeStoredFlowerTarget(bee);\n    return beeOrbitTarget(bee.aux, movePc.step);\n}\n\nbool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {\n    if ((bee.aux & AUX_MOVED) != 0u) return false;\n    bool sourceHazard = adjacentHazard(sourcePosition);\n    bool targetHazard = adjacentHazard(targetPosition);\n    if (sourceHazard != targetHazard) return sourceHazard;\n    if ((bee.aux & BEE_AUX_SWARM) == 0u) return false;\n\n    ivec2 destination = beeMovementTarget(bee);\n    ivec2 sourceDelta = sourcePosition - destination;\n    ivec2 targetDelta = targetPosition - destination;\n    int sourceDistance = sourceDelta.x * sourceDelta.x + sourceDelta.y * sourceDelta.y;\n    int targetDistance = targetDelta.x * targetDelta.x + targetDelta.y * targetDelta.y;\n    return targetDistance < sourceDistance;\n}\n\nbool magneticMoveAllowed',
)

replace_once(
    "shaders/fullscreen.frag",
    "        color.rgb = mix(color.rgb, overlay, alpha * float(tile.occupancy) / 64.0);\n",
    "        color.rgb = mix(color.rgb, overlay, alpha * float(tileOccupancy(tile)) / 64.0);\n",
)

print("Applied Fix31 startup screen, biohazard colony formation, sparse tile targets, and bounded migration lifecycle.")
