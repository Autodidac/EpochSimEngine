import epoch.gui.font;
import epoch.gui.rounded_rect;

#include "epochwater/ui_bridge.hpp"

namespace fastfreddy::testbed
{
    UiMesh make_epochgui_rounded_rect(
        const float x,
        const float y,
        const float width,
        const float height,
        const float radius,
        const float border_width,
        const std::uint32_t segments_per_corner)
    {
        namespace gui = epochengine::gui_lib;
        namespace rounded = epochengine::gui_lib::rounded_rect;

        const rounded::RoundedRectMesh source = rounded::make_rounded_rect_mesh({
            .bounds = gui::Rect{{x, y}, {width, height}},
            .radii = rounded::CornerRadii{radius, radius, radius, radius},
            .border_width = border_width,
            .segments_per_corner = segments_per_corner
        });

        UiMesh result{};
        result.vertices.reserve(source.vertices.size());
        for (const gui::Vec2 vertex : source.vertices)
            result.vertices.push_back(UiPoint{vertex.x, vertex.y});
        result.fill_indices = source.fill_indices;
        result.border_indices = source.border_indices;
        result.valid = source.valid;
        return result;
    }

    std::array<std::uint8_t, 7> epochgui_glyph_rows(const char character) noexcept
    {
        return epochengine::gui_lib::font::default_glyph(character).rows;
    }
}
