#!/usr/bin/env python3
from __future__ import annotations

import subprocess
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file_path = Path(path)
    text = file_path.read_text(encoding="utf-8")
    if old not in text:
        raise SystemExit(f"expected block missing from {path}")
    file_path.write_text(text.replace(old, new, 1), encoding="utf-8")


Path("include/epoch/sand/input_routing.hpp").write_text(
    '''#pragma once

#include <cstdint>

namespace epoch::sand {

struct DirectionalInputRouting final {
    std::int32_t camera_x{};
    std::int32_t camera_y{};
    std::int32_t player_x{};
    std::int32_t player_y{};

    friend constexpr bool operator==(const DirectionalInputRouting&,
                                     const DirectionalInputRouting&) = default;
};

[[nodiscard]] constexpr DirectionalInputRouting route_directional_input(
    const bool player_present,
    const bool move_left,
    const bool move_right,
    const bool move_up,
    const bool move_down) noexcept {
    const auto horizontal = static_cast<std::int32_t>(move_right) -
                            static_cast<std::int32_t>(move_left);
    const auto vertical = static_cast<std::int32_t>(move_down) -
                          static_cast<std::int32_t>(move_up);

    if (player_present) {
        return {0, 0, horizontal, vertical};
    }
    return {horizontal, vertical, 0, 0};
}

} // namespace epoch::sand
''',
    encoding="utf-8",
)

replace_once(
    "src/app.cpp",
    '#include "epoch/sand/material.hpp"\n',
    '#include "epoch/sand/input_routing.hpp"\n#include "epoch/sand/material.hpp"\n',
)
replace_once(
    "src/app.cpp",
    '''        int camera_direction_x = 0;
        int camera_direction_y = 0;
        // Camera navigation is universal. Character scenes continue forwarding the
        // same input to the actor below, so W/A/S/D moves both the player and view.
        camera_direction_x += (input.move_right ? 1 : 0) - (input.move_left ? 1 : 0);
        camera_direction_y += (input.move_down ? 1 : 0) - (input.move_up ? 1 : 0);
''',
    '''        const bool player_controls = scene_has_character(scene);
        const auto directional_input = route_directional_input(
            player_controls,
            input.move_left,
            input.move_right,
            input.move_up,
            input.move_down);
        int camera_direction_x = directional_input.camera_x;
        int camera_direction_y = directional_input.camera_y;
''',
)
replace_once(
    "src/app.cpp",
    '''        const bool player_controls = scene_has_character(scene);
        shared_state.move_x.store(player_controls
            ? (input.move_right ? 1 : 0) - (input.move_left ? 1 : 0) : 0,
            std::memory_order_relaxed);
        shared_state.move_y.store(player_controls
            ? (input.move_down ? 1 : 0) - (input.move_up ? 1 : 0) : 0,
            std::memory_order_relaxed);
''',
    '''        shared_state.move_x.store(directional_input.player_x, std::memory_order_relaxed);
        shared_state.move_y.store(directional_input.player_y, std::memory_order_relaxed);
''',
)

replace_once(
    "tests/behavior_contract.cpp",
    '#include "epoch/sand/material.hpp"\n',
    '#include "epoch/sand/input_routing.hpp"\n#include "epoch/sand/material.hpp"\n',
)
replace_once(
    "tests/behavior_contract.cpp",
    '[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {',
    '''[[nodiscard]] constexpr bool directional_input_routes_by_scene() noexcept {
    using epoch::sand::DirectionalInputRouting;
    using epoch::sand::route_directional_input;

    constexpr auto camera_scene = route_directional_input(false, false, true, true, false);
    constexpr auto player_scene = route_directional_input(true, false, true, true, false);
    constexpr auto neutral_scene = route_directional_input(true, true, true, true, true);

    return camera_scene == DirectionalInputRouting{1, -1, 0, 0} &&
           player_scene == DirectionalInputRouting{0, 0, 1, -1} &&
           neutral_scene == DirectionalInputRouting{0, 0, 0, 0};
}

[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {''',
)
replace_once(
    "tests/behavior_contract.cpp",
    'static_assert(terrain_stability_preserves_representation());\n',
    'static_assert(terrain_stability_preserves_representation());\nstatic_assert(directional_input_routes_by_scene());\n',
)
replace_once(
    "tests/behavior_contract.cpp",
    '''    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&
           half_water_medium_exchange_preserves_volume() &&
           breathing_requires_explicit_oxygen() &&
           terrain_stability_preserves_representation() ? 0 : 1;
''',
    '''    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&
           half_water_medium_exchange_preserves_volume() &&
           breathing_requires_explicit_oxygen() &&
           terrain_stability_preserves_representation() &&
           directional_input_routes_by_scene() ? 0 : 1;
''',
)

replace_once(
    "README.md",
    '''### Camera navigation

- `W` / `A` / `S` / `D`: pan camera in every scene
- Character scenes also continue forwarding `W` / `A` / `S` / `D` to the player

### Character scenes

- `A` / `D`: walk
- `W`: jump
''',
    '''### Camera navigation

- `W` / `A` / `S` / `D`: pan the camera only in scenes without a player
- Mouse-edge scrolling and middle-mouse drag remain available in every scene

### Character scenes

- `W` / `A` / `S` / `D`: control the player exclusively; these keys do not pan the camera
''',
)

replace_once(
    "tools/validate_shader_contracts.py",
    '''    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    window_hpp = (ROOT / "include/epoch/sand/window.hpp").read_text(encoding="utf-8")
''',
    '''    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    input_routing_hpp = (ROOT / "include/epoch/sand/input_routing.hpp").read_text(encoding="utf-8")
    window_hpp = (ROOT / "include/epoch/sand/window.hpp").read_text(encoding="utf-8")
''',
)
replace_once(
    "tools/validate_shader_contracts.py",
    '''    for token in (
        "edge_band_pixels = 28",
        "(input.move_right ? 1 : 0) - (input.move_left ? 1 : 0)",
        "layout.placement_cells",
        "layout.placement_tiles",
    ):
        if token not in app_cpp:
            errors.append(f"camera/placement input contract missing {token!r}")
    if "if (!player_scene)" in app_cpp:
        errors.append("camera WASD is incorrectly disabled on player scenes")
''',
    '''    for token in (
        "edge_band_pixels = 28",
        "route_directional_input(",
        "camera_direction_x = directional_input.camera_x",
        "camera_direction_y = directional_input.camera_y",
        "shared_state.move_x.store(directional_input.player_x",
        "shared_state.move_y.store(directional_input.player_y",
        "layout.placement_cells",
        "layout.placement_tiles",
    ):
        if token not in app_cpp:
            errors.append(f"camera/placement input contract missing {token!r}")
    for token in (
        "struct DirectionalInputRouting final",
        "if (player_present)",
        "return {0, 0, horizontal, vertical};",
        "return {horizontal, vertical, 0, 0};",
    ):
        if token not in input_routing_hpp:
            errors.append(f"context-sensitive directional routing contract missing {token!r}")
    for forbidden in (
        "Camera navigation is universal",
        "W/A/S/D moves both the player and view",
        "camera_direction_x += (input.move_right ? 1 : 0)",
        "camera_direction_y += (input.move_down ? 1 : 0)",
    ):
        if forbidden in app_cpp:
            errors.append(f"player-scene camera duplication remains: {forbidden!r}")
''',
)

replace_once(
    "missioncache.md",
    '''| MC-075 | PARTIAL | Complete camera navigation and scope HUD | Middle-mouse drag, mouse-edge scrolling, and camera reset work at every zoom. W/A/S/D pans the camera in every scene; character scenes simultaneously retain W/A/S/D player controls. The sidebar always shows current zoom, active-region mode, and active-region count, while debug draws map-area boundaries clearly. Static implementation is present; runtime scene verification remains required. |
''',
    '''| MC-075 | PARTIAL | Camera navigation and scope HUD | Middle-mouse drag, mouse-edge scrolling, and camera reset work at every zoom. Keyboard camera panning is permitted only when the current scene has no player; MC-076 owns that routing contract. The sidebar always shows current zoom, active-region mode, and active-region count, while debug draws map-area boundaries clearly. Drag, edge, reset, HUD, and boundary rendering remain runtime-unverified. |
| MC-076 | PARTIAL | Context-sensitive W/A/S/D ownership | Directional keyboard input has exactly one owner. When a scene has a player, W/A/S/D controls the player and contributes zero camera motion. When no player exists, W/A/S/D pans the camera and contributes zero actor motion. Mouse-edge and middle-mouse camera controls remain independent. A shared constexpr router and behavior contract are implemented; Windows/Linux CI evidence is pending. |
''',
)
replace_once(
    "missioncache.md",
    '- World expansion, zoom, pausing, streaming, and concurrency do not change deterministic reference results.\n',
    '- World expansion, zoom, pausing, streaming, and concurrency do not change deterministic reference results.\n- Directional keyboard input has exactly one owner: the player when present, otherwise the camera. Mouse-edge and middle-mouse camera input remain independent.\n',
)
replace_once(
    "missioncache.md",
    '''`MC-075` contains the universal W/A/S/D implementation and regression contract. Runtime scene verification remains active; unrelated open, partial, regressed, and deferred missions carry forward under their existing IDs.
''',
    '''`MC-075` was later corrected: player scenes must not route W/A/S/D to the camera. The incorrect simultaneous-routing behavior from this release is superseded by MC-076 and must not be restored.
''',
)

canonical_workflow = subprocess.run(
    ["git", "show", "origin/main:.github/workflows/source-export.yml"],
    check=True,
    capture_output=True,
).stdout
Path(".github/workflows/source-export.yml").write_bytes(canonical_workflow)
Path(__file__).unlink()
