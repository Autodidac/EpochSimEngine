#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace fastfreddy::testbed
{
    enum class Material : std::uint8_t
    {
        empty,
        stone,
        water
    };

    struct Cell final
    {
        Material material{Material::empty};
        std::uint8_t water_units{};
        std::int8_t horizontal_direction{};
        bool falling{};

        [[nodiscard]] constexpr bool is_empty() const noexcept { return material == Material::empty; }
        [[nodiscard]] constexpr bool is_stone() const noexcept { return material == Material::stone; }
        [[nodiscard]] constexpr bool is_water() const noexcept
        {
            return material == Material::water && water_units != 0;
        }
    };

    class Simulation final
    {
    public:
        static constexpr std::uint32_t terrain_block_size = 8;
        static constexpr std::uint8_t half_water = 1;
        static constexpr std::uint8_t full_water = 2;
        static constexpr std::uint8_t ledge_release_units = 3;

        Simulation(std::uint32_t width, std::uint32_t height);

        void reset_demo_scene();
        void clear() noexcept;
        void step();

        void add_water(std::uint32_t x, std::uint32_t y, std::uint8_t units = half_water) noexcept;
        void paint_stone_block(std::uint32_t x, std::uint32_t y) noexcept;
        void erase(std::uint32_t x, std::uint32_t y, std::uint32_t radius = 0) noexcept;

        [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
        [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
        [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
        [[nodiscard]] std::uint64_t total_water_units() const noexcept;
        [[nodiscard]] std::span<const Cell> cells() const noexcept { return cells_; }
        [[nodiscard]] const Cell& cell(std::uint32_t x, std::uint32_t y) const noexcept;

        void set_cell(std::uint32_t x, std::uint32_t y, Cell value) noexcept;

    private:
        [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const noexcept;
        [[nodiscard]] bool inside(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] Cell& mutable_cell(std::uint32_t x, std::uint32_t y) noexcept;
        [[nodiscard]] std::uint8_t water_units_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] bool stone_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] bool supported(std::uint32_t x, std::uint32_t y) const noexcept;
        [[nodiscard]] bool eligible_for_substep(const Cell& cell, std::uint32_t substep) const noexcept;
        [[nodiscard]] bool can_release_over_ledge(std::uint32_t x, std::uint32_t y, std::int32_t direction) const noexcept;
        [[nodiscard]] std::int32_t infer_ledge_direction(std::uint32_t x, std::uint32_t y, const Cell& cell) const noexcept;

        bool transfer_down(std::uint32_t x, std::uint32_t y, std::uint32_t substep) noexcept;
        bool transfer_diagonal(std::uint32_t x, std::uint32_t y, std::int32_t direction, std::uint32_t substep) noexcept;
        bool transfer_horizontal(std::uint32_t x, std::uint32_t y, std::int32_t direction, std::uint32_t substep) noexcept;
        void vertical_pass(std::uint32_t substep) noexcept;
        void horizontal_pass(std::uint32_t substep) noexcept;
        void normalize_cell(Cell& cell) noexcept;

        std::uint32_t width_{};
        std::uint32_t height_{};
        std::uint64_t tick_{};
        std::vector<Cell> cells_{};
        std::vector<std::uint8_t> moved_{};
    };
}
