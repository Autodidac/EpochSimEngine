#pragma once

#include "epochwater/ui_bridge.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace fastfreddy::testbed
{
    struct Color final
    {
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};
        std::uint8_t alpha{255U};
    };

    class Canvas final
    {
    public:
        Canvas(std::uint32_t width, std::uint32_t height);

        void clear(Color color) noexcept;
        void set_pixel(std::int32_t x, std::int32_t y, Color color) noexcept;
        void blend_pixel(std::int32_t x, std::int32_t y, Color color) noexcept;
        void fill_rect(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, Color color) noexcept;
        void draw_rounded_rect(float x, float y, float width, float height, float radius,
                               Color fill, Color border, float border_width = 0.0f);
        void draw_text(std::int32_t x, std::int32_t y, std::string_view text,
                       Color color, std::uint32_t scale = 1U) noexcept;

        [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
        [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
        [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return pixels_; }
        [[nodiscard]] std::uint32_t pitch() const noexcept { return width_ * 4U; }

    private:
        void fill_triangle(UiPoint a, UiPoint b, UiPoint c, Color color) noexcept;
        void draw_mesh_indices(const UiMesh& mesh, std::span<const std::uint32_t> indices, Color color) noexcept;

        std::uint32_t width_{};
        std::uint32_t height_{};
        std::vector<std::uint8_t> pixels_{};
    };
}
