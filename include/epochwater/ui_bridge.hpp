#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace fastfreddy::testbed
{
    struct UiPoint final
    {
        float x{};
        float y{};
    };

    struct UiMesh final
    {
        std::vector<UiPoint> vertices{};
        std::vector<std::uint32_t> fill_indices{};
        std::vector<std::uint32_t> border_indices{};
        bool valid{};
    };

    [[nodiscard]] UiMesh make_epochgui_rounded_rect(
        float x,
        float y,
        float width,
        float height,
        float radius,
        float border_width = 0.0f,
        std::uint32_t segments_per_corner = 8U);

    [[nodiscard]] std::array<std::uint8_t, 7> epochgui_glyph_rows(char character) noexcept;
}
