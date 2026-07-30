#include "epochwater/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fastfreddy::testbed
{
    namespace
    {
        [[nodiscard]] float edge(const UiPoint a, const UiPoint b, const UiPoint p) noexcept
        {
            return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
        }
    }

    Canvas::Canvas(const std::uint32_t width, const std::uint32_t height)
        : width_{std::max(width, 1U)},
          height_{std::max(height, 1U)},
          pixels_(static_cast<std::size_t>(width_) * height_ * 4U)
    {
    }

    void Canvas::clear(const Color color) noexcept
    {
        for (std::uint32_t y = 0; y < height_; ++y)
        {
            for (std::uint32_t x = 0; x < width_; ++x)
                set_pixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), color);
        }
    }

    void Canvas::set_pixel(const std::int32_t x, const std::int32_t y, const Color color) noexcept
    {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width_) || y >= static_cast<std::int32_t>(height_))
            return;
        const std::size_t offset = (static_cast<std::size_t>(y) * width_ + static_cast<std::uint32_t>(x)) * 4U;
        pixels_[offset + 0U] = color.red;
        pixels_[offset + 1U] = color.green;
        pixels_[offset + 2U] = color.blue;
        pixels_[offset + 3U] = color.alpha;
    }

    void Canvas::blend_pixel(const std::int32_t x, const std::int32_t y, const Color color) noexcept
    {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width_) || y >= static_cast<std::int32_t>(height_))
            return;
        const std::size_t offset = (static_cast<std::size_t>(y) * width_ + static_cast<std::uint32_t>(x)) * 4U;
        const std::uint32_t alpha = color.alpha;
        const std::uint32_t inverse = 255U - alpha;
        pixels_[offset + 0U] = static_cast<std::uint8_t>((color.red * alpha + pixels_[offset + 0U] * inverse) / 255U);
        pixels_[offset + 1U] = static_cast<std::uint8_t>((color.green * alpha + pixels_[offset + 1U] * inverse) / 255U);
        pixels_[offset + 2U] = static_cast<std::uint8_t>((color.blue * alpha + pixels_[offset + 2U] * inverse) / 255U);
        pixels_[offset + 3U] = 255U;
    }

    void Canvas::fill_rect(
        const std::int32_t x,
        const std::int32_t y,
        const std::int32_t width,
        const std::int32_t height,
        const Color color) noexcept
    {
        const std::int32_t x0 = std::max(x, 0);
        const std::int32_t y0 = std::max(y, 0);
        const std::int32_t x1 = std::min(x + width, static_cast<std::int32_t>(width_));
        const std::int32_t y1 = std::min(y + height, static_cast<std::int32_t>(height_));
        for (std::int32_t py = y0; py < y1; ++py)
        {
            for (std::int32_t px = x0; px < x1; ++px)
                blend_pixel(px, py, color);
        }
    }

    void Canvas::draw_rounded_rect(
        const float x,
        const float y,
        const float width,
        const float height,
        const float radius,
        const Color fill,
        const Color border,
        const float border_width)
    {
        const UiMesh mesh = make_epochgui_rounded_rect(x, y, width, height, radius, border_width, 10U);
        if (!mesh.valid)
            return;
        draw_mesh_indices(mesh, mesh.fill_indices, fill);
        if (border_width > 0.0f)
            draw_mesh_indices(mesh, mesh.border_indices, border);
    }

    void Canvas::draw_text(
        const std::int32_t x,
        const std::int32_t y,
        const std::string_view text,
        const Color color,
        const std::uint32_t scale) noexcept
    {
        if (scale == 0U)
            return;

        std::int32_t pen_x = x;
        std::int32_t pen_y = y;
        for (const char character : text)
        {
            if (character == '\n')
            {
                pen_x = x;
                pen_y += static_cast<std::int32_t>(9U * scale);
                continue;
            }

            const auto rows = epochgui_glyph_rows(character);
            for (std::uint32_t row = 0; row < 7U; ++row)
            {
                for (std::uint32_t column = 0; column < 5U; ++column)
                {
                    const std::uint8_t mask = static_cast<std::uint8_t>(1U << (4U - column));
                    if ((rows[row] & mask) == 0U)
                        continue;
                    fill_rect(
                        pen_x + static_cast<std::int32_t>(column * scale),
                        pen_y + static_cast<std::int32_t>(row * scale),
                        static_cast<std::int32_t>(scale),
                        static_cast<std::int32_t>(scale),
                        color);
                }
            }
            pen_x += static_cast<std::int32_t>(6U * scale);
        }
    }

    void Canvas::fill_triangle(const UiPoint a, const UiPoint b, const UiPoint c, const Color color) noexcept
    {
        const float area = edge(a, b, c);
        if (std::abs(area) < 0.0001f)
            return;

        const std::int32_t min_x = std::max(0, static_cast<std::int32_t>(std::floor(std::min({a.x, b.x, c.x}))));
        const std::int32_t min_y = std::max(0, static_cast<std::int32_t>(std::floor(std::min({a.y, b.y, c.y}))));
        const std::int32_t max_x = std::min(
            static_cast<std::int32_t>(width_) - 1,
            static_cast<std::int32_t>(std::ceil(std::max({a.x, b.x, c.x}))));
        const std::int32_t max_y = std::min(
            static_cast<std::int32_t>(height_) - 1,
            static_cast<std::int32_t>(std::ceil(std::max({a.y, b.y, c.y}))));

        for (std::int32_t py = min_y; py <= max_y; ++py)
        {
            for (std::int32_t px = min_x; px <= max_x; ++px)
            {
                const UiPoint point{static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f};
                const float w0 = edge(b, c, point);
                const float w1 = edge(c, a, point);
                const float w2 = edge(a, b, point);
                const bool positive = w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
                const bool negative = w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f;
                if (positive || negative)
                    blend_pixel(px, py, color);
            }
        }
    }

    void Canvas::draw_mesh_indices(
        const UiMesh& mesh,
        const std::span<const std::uint32_t> indices,
        const Color color) noexcept
    {
        for (std::size_t i = 0; i + 2U < indices.size(); i += 3U)
        {
            const std::uint32_t ia = indices[i + 0U];
            const std::uint32_t ib = indices[i + 1U];
            const std::uint32_t ic = indices[i + 2U];
            if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
                continue;
            fill_triangle(mesh.vertices[ia], mesh.vertices[ib], mesh.vertices[ic], color);
        }
    }
}
