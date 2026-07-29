#pragma once

#include <cstdint>
#include <string_view>

namespace epochengine::gui_lib
{
    struct Vec2
    {
        float x{};
        float y{};
    };

    struct Rect
    {
        Vec2 position{};
        Vec2 size{};
    };

    [[nodiscard]] inline bool contains(Rect rect, Vec2 point) noexcept
    {
        return point.x >= rect.position.x
            && point.x <= rect.position.x + rect.size.x
            && point.y >= rect.position.y
            && point.y <= rect.position.y + rect.size.y;
    }

    class LayoutController
    {
    public:
        virtual ~LayoutController() = default;
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    };
}
