#pragma once

#include "epoch/sand/material.hpp"

#include <gui/floating_window.hpp>
#include <gui/font.hpp>

#include <algorithm>
#include <cstdint>

namespace epoch::sand::ui {

inline constexpr std::uint32_t status_height = 72u;
inline constexpr std::uint32_t group_tabs_height = 48u;
inline constexpr std::uint32_t palette_items_height = 76u;
inline constexpr std::uint32_t palette_height = group_tabs_height + palette_items_height + 6u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;

struct Layout final {
    epochengine::gui_lib::Rect status{};
    epochengine::gui_lib::Rect simulation{};
    epochengine::gui_lib::Rect group_tabs{};
    epochengine::gui_lib::Rect palette{};
    epochengine::gui_lib::Rect previous_scene{};
    epochengine::gui_lib::Rect next_scene{};
    epochengine::gui_lib::Rect reset_scene{};
    epochengine::gui_lib::Rect mode_toggle{};
    epochengine::gui_lib::Rect debug_toggle{};
    epochengine::gui_lib::Rect material_card{};
};

[[nodiscard]] inline Layout make_layout(const std::uint32_t width, const std::uint32_t height) noexcept {
    const auto safe_width = (std::max)(width, 1u);
    const auto safe_height = (std::max)(height, 1u);
    const auto top = (std::min)(status_height, safe_height);
    const auto bottom = (std::min)(palette_height, safe_height - top);
    const auto simulation_height = safe_height - top - bottom;
    const auto tabs_height = (std::min)(group_tabs_height, bottom);
    const auto items_height = bottom - tabs_height - (bottom > tabs_height ? 6u : 0u);

    Layout layout{
        .status = {{0.0f, 0.0f}, {static_cast<float>(safe_width), static_cast<float>(top)}},
        .simulation = {{0.0f, static_cast<float>(top)},
                       {static_cast<float>(safe_width), static_cast<float>(simulation_height)}},
        .group_tabs = {{margin, static_cast<float>(top + simulation_height + 3u)},
                       {(std::max)(1.0f, static_cast<float>(safe_width) - margin * 2.0f),
                        static_cast<float>(tabs_height)}},
        .palette = {{margin, static_cast<float>(top + simulation_height + tabs_height + 3u)},
                    {(std::max)(1.0f, static_cast<float>(safe_width) - margin * 2.0f),
                     static_cast<float>(items_height)}},
    };

    const float right = static_cast<float>(safe_width) - margin;
    layout.debug_toggle = {{(std::max)(margin, right - 104.0f), 12.0f}, {104.0f, 48.0f}};
    layout.mode_toggle = {{(std::max)(margin, right - 244.0f), 12.0f}, {136.0f, 48.0f}};
    if (safe_width >= 1040u) {
        layout.reset_scene = {{right - 352.0f, 12.0f}, {104.0f, 48.0f}};
        layout.next_scene = {{right - 434.0f, 12.0f}, {78.0f, 48.0f}};
        layout.previous_scene = {{right - 516.0f, 12.0f}, {78.0f, 48.0f}};
    } else {
        constexpr float hidden = -4096.0f;
        layout.reset_scene = {{hidden, hidden}, {0.0f, 0.0f}};
        layout.next_scene = {{hidden, hidden}, {0.0f, 0.0f}};
        layout.previous_scene = {{hidden, hidden}, {0.0f, 0.0f}};
    }
    return layout;
}

[[nodiscard]] inline epochengine::gui_lib::Rect group_tab_rect(
    const Layout& layout, const std::uint32_t group_index) noexcept {
    const auto cell_width = layout.group_tabs.size.x / static_cast<float>(material_group_count);
    return {{layout.group_tabs.position.x + static_cast<float>(group_index) * cell_width + gap * 0.5f,
             layout.group_tabs.position.y + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap),
             (std::max)(1.0f, layout.group_tabs.size.y - gap)}};
}

[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(
    const Layout& layout, const MaterialGroup group, const std::uint32_t slot) noexcept {
    const auto slot_count = (std::max)(material_group_size(group), 1u);
    const auto cell_width = layout.palette.size.x / static_cast<float>(slot_count);
    return {{layout.palette.position.x + static_cast<float>(slot) * cell_width + gap * 0.5f,
             layout.palette.position.y + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap),
             (std::max)(1.0f, layout.palette.size.y - gap)}};
}

[[nodiscard]] inline std::uint32_t group_at(
    const Layout& layout, const epochengine::gui_lib::Vec2 point) noexcept {
    if (!epochengine::gui_lib::contains(layout.group_tabs, point)) return material_group_count;
    const auto local_x = (std::max)(0.0f, point.x - layout.group_tabs.position.x);
    return (std::min)(material_group_count - 1u,
        static_cast<std::uint32_t>(local_x * static_cast<float>(material_group_count) /
                                   (std::max)(layout.group_tabs.size.x, 1.0f)));
}

[[nodiscard]] inline std::uint32_t palette_slot_at(
    const Layout& layout, const MaterialGroup group,
    const epochengine::gui_lib::Vec2 point) noexcept {
    const auto slot_count = material_group_size(group);
    if (slot_count == 0u || !epochengine::gui_lib::contains(layout.palette, point)) return slot_count;
    const auto local_x = (std::max)(0.0f, point.x - layout.palette.position.x);
    return (std::min)(slot_count - 1u,
        static_cast<std::uint32_t>(local_x * static_cast<float>(slot_count) /
                                   (std::max)(layout.palette.size.x, 1.0f)));
}

[[nodiscard]] inline Material palette_material_at(
    const Layout& layout, const MaterialGroup group,
    const epochengine::gui_lib::Vec2 point) noexcept {
    const auto slot = palette_slot_at(layout, group, point);
    return slot < material_group_size(group) ? grouped_material(group, slot) : Material::count;
}

} // namespace epoch::sand::ui
