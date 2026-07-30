#include "epochwater/simulation.hpp"

#include <algorithm>
#include <cassert>

namespace fastfreddy::testbed
{
    namespace
    {
        constexpr Cell empty_cell{};
    }

    Simulation::Simulation(const std::uint32_t width, const std::uint32_t height)
        : width_{std::max(width, 1U)},
          height_{std::max(height, 1U)},
          cells_(static_cast<std::size_t>(width_) * height_),
          moved_(cells_.size())
    {
        reset_demo_scene();
    }

    void Simulation::clear() noexcept
    {
        std::fill(cells_.begin(), cells_.end(), Cell{});
        std::fill(moved_.begin(), moved_.end(), std::uint8_t{});
        tick_ = 0;
    }

    void Simulation::reset_demo_scene()
    {
        clear();

        const std::uint32_t floor_y = height_ > 16U ? height_ - 16U : 0U;
        for (std::uint32_t by = floor_y; by < height_; by += terrain_block_size)
        {
            for (std::uint32_t bx = 0; bx < width_; bx += terrain_block_size)
                paint_stone_block(bx, by);
        }

        const std::uint32_t platform_y = height_ > 46U ? height_ - 46U : height_ / 2U;
        const std::uint32_t platform_end = std::min(width_ - 1U, 64U);
        for (std::uint32_t bx = 8U; bx <= platform_end; bx += terrain_block_size)
            paint_stone_block(bx, platform_y);

        if (height_ > 24U)
        {
            const std::uint32_t basin_y = height_ - 24U;
            for (std::uint32_t bx = 72U; bx < width_; bx += terrain_block_size)
                paint_stone_block(bx, basin_y);
        }

        const std::uint32_t water_bottom = platform_y == 0U ? 0U : platform_y - 1U;
        const std::uint32_t water_top = water_bottom > 22U ? water_bottom - 22U : 0U;
        for (std::uint32_t y = water_top; y <= water_bottom; ++y)
        {
            for (std::uint32_t x = 12U; x < std::min(platform_end, 52U); ++x)
                set_cell(x, y, Cell{Material::water, full_water, 0, false});
        }
    }

    void Simulation::step()
    {
        [[maybe_unused]] const std::uint64_t before = total_water_units();
        for (std::uint32_t substep = 0; substep < 2U; ++substep)
        {
            std::fill(moved_.begin(), moved_.end(), std::uint8_t{});
            vertical_pass(substep);
            horizontal_pass(substep);
        }
        ++tick_;
        assert(before == total_water_units());
    }

    void Simulation::add_water(const std::uint32_t x, const std::uint32_t y, const std::uint8_t units) noexcept
    {
        if (x >= width_ || y >= height_ || units == 0U)
            return;

        Cell& target = mutable_cell(x, y);
        if (target.is_stone())
            return;

        const std::uint16_t current = static_cast<std::uint16_t>(target.is_water() ? target.water_units : 0U);
        const std::uint16_t combined = static_cast<std::uint16_t>(current + static_cast<std::uint16_t>(units));
        target.material = Material::water;
        target.water_units = static_cast<std::uint8_t>(std::min<std::uint16_t>(combined, full_water));
        target.horizontal_direction = 0;
        target.falling = false;
    }

    void Simulation::paint_stone_block(const std::uint32_t x, const std::uint32_t y) noexcept
    {
        const std::uint32_t origin_x = (x / terrain_block_size) * terrain_block_size;
        const std::uint32_t origin_y = (y / terrain_block_size) * terrain_block_size;
        for (std::uint32_t local_y = 0; local_y < terrain_block_size; ++local_y)
        {
            const std::uint32_t cell_y = origin_y + local_y;
            if (cell_y >= height_)
                break;
            for (std::uint32_t local_x = 0; local_x < terrain_block_size; ++local_x)
            {
                const std::uint32_t cell_x = origin_x + local_x;
                if (cell_x >= width_)
                    break;
                mutable_cell(cell_x, cell_y) = Cell{Material::stone, 0, 0, false};
            }
        }
    }

    void Simulation::erase(const std::uint32_t x, const std::uint32_t y, const std::uint32_t radius) noexcept
    {
        const std::int32_t cx = static_cast<std::int32_t>(x);
        const std::int32_t cy = static_cast<std::int32_t>(y);
        const std::int32_t r = static_cast<std::int32_t>(radius);
        const std::int64_t radius_squared = static_cast<std::int64_t>(r) * r;
        for (std::int32_t oy = -r; oy <= r; ++oy)
        {
            for (std::int32_t ox = -r; ox <= r; ++ox)
            {
                if (static_cast<std::int64_t>(ox) * ox + static_cast<std::int64_t>(oy) * oy > radius_squared)
                    continue;
                const std::int32_t px = cx + ox;
                const std::int32_t py = cy + oy;
                if (inside(px, py))
                    mutable_cell(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py)) = Cell{};
            }
        }
    }

    std::uint64_t Simulation::total_water_units() const noexcept
    {
        std::uint64_t total{};
        for (const Cell& value : cells_)
        {
            if (value.is_water())
                total += value.water_units;
        }
        return total;
    }

    const Cell& Simulation::cell(const std::uint32_t x, const std::uint32_t y) const noexcept
    {
        if (x >= width_ || y >= height_)
            return empty_cell;
        return cells_[index(x, y)];
    }

    void Simulation::set_cell(const std::uint32_t x, const std::uint32_t y, Cell value) noexcept
    {
        if (x >= width_ || y >= height_)
            return;
        normalize_cell(value);
        mutable_cell(x, y) = value;
    }

    std::size_t Simulation::index(const std::uint32_t x, const std::uint32_t y) const noexcept
    {
        return static_cast<std::size_t>(y) * width_ + x;
    }

    bool Simulation::inside(const std::int32_t x, const std::int32_t y) const noexcept
    {
        return x >= 0 && y >= 0 && x < static_cast<std::int32_t>(width_) && y < static_cast<std::int32_t>(height_);
    }

    Cell& Simulation::mutable_cell(const std::uint32_t x, const std::uint32_t y) noexcept
    {
        return cells_[index(x, y)];
    }

    std::uint8_t Simulation::water_units_at(const std::int32_t x, const std::int32_t y) const noexcept
    {
        if (!inside(x, y))
            return 0U;
        const Cell& value = cell(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        return value.is_water() ? value.water_units : 0U;
    }

    bool Simulation::stone_at(const std::int32_t x, const std::int32_t y) const noexcept
    {
        if (!inside(x, y))
            return true;
        return cell(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).is_stone();
    }

    bool Simulation::supported(const std::uint32_t x, const std::uint32_t y) const noexcept
    {
        if (y + 1U >= height_)
            return true;
        const Cell& below = cell(x, y + 1U);
        return below.is_stone() || (below.is_water() && below.water_units == full_water);
    }

    bool Simulation::eligible_for_substep(const Cell& value, const std::uint32_t substep) const noexcept
    {
        if (!value.is_water())
            return false;
        return substep == 0U || value.water_units == half_water;
    }

    std::int32_t Simulation::infer_ledge_direction(const std::uint32_t x, const std::uint32_t y, const Cell& value) const noexcept
    {
        if (value.horizontal_direction != 0)
            return value.horizontal_direction;
        const bool support_left = stone_at(static_cast<std::int32_t>(x) - 1, static_cast<std::int32_t>(y) + 1);
        const bool support_right = stone_at(static_cast<std::int32_t>(x) + 1, static_cast<std::int32_t>(y) + 1);
        if (support_left != support_right)
            return support_left ? 1 : -1;
        return 0;
    }

    bool Simulation::can_release_over_ledge(const std::uint32_t x, const std::uint32_t y, const std::int32_t direction) const noexcept
    {
        const Cell& source = cell(x, y);
        if (!source.is_water())
            return false;
        if (source.falling)
            return true;
        if (source.water_units != full_water)
            return false;

        if (direction != 0)
        {
            const std::int32_t upstream_x = static_cast<std::int32_t>(x) - direction;
            return static_cast<std::uint32_t>(source.water_units) +
                       water_units_at(upstream_x, static_cast<std::int32_t>(y)) >= ledge_release_units;
        }

        const std::uint8_t upstream = std::max(
            water_units_at(static_cast<std::int32_t>(x) - 1, static_cast<std::int32_t>(y)),
            water_units_at(static_cast<std::int32_t>(x) + 1, static_cast<std::int32_t>(y)));
        return static_cast<std::uint32_t>(source.water_units) + upstream >= ledge_release_units;
    }

    bool Simulation::transfer_down(const std::uint32_t x, const std::uint32_t y, const std::uint32_t substep) noexcept
    {
        if (y + 1U >= height_)
            return false;
        const std::size_t source_index = index(x, y);
        const std::size_t target_index = index(x, y + 1U);
        Cell source = cells_[source_index];
        Cell target = cells_[target_index];
        if (!eligible_for_substep(source, substep) || moved_[source_index] != 0U || target.is_stone())
            return false;

        if (target.is_water())
        {
            if (target.water_units >= full_water)
                return false;
            const std::uint8_t transfer = std::min<std::uint8_t>(
                source.water_units, static_cast<std::uint8_t>(full_water - target.water_units));
            if (transfer == 0U)
                return false;
            source.water_units = static_cast<std::uint8_t>(source.water_units - transfer);
            target.water_units = static_cast<std::uint8_t>(target.water_units + transfer);
            target.material = Material::water;
            target.falling = false;
            target.horizontal_direction = source.horizontal_direction;
            normalize_cell(source);
            normalize_cell(target);
            cells_[source_index] = source;
            cells_[target_index] = target;
            moved_[source_index] = 1U;
            moved_[target_index] = 1U;
            return true;
        }

        if (!target.is_empty())
            return false;

        const bool ledge_start = !source.falling &&
            (stone_at(static_cast<std::int32_t>(x) - 1, static_cast<std::int32_t>(y) + 1) ||
             stone_at(static_cast<std::int32_t>(x) + 1, static_cast<std::int32_t>(y) + 1));
        const std::int32_t direction = infer_ledge_direction(x, y, source);
        if (ledge_start && !can_release_over_ledge(x, y, direction))
            return false;

        source.falling = true;
        cells_[target_index] = source;
        cells_[source_index] = Cell{};
        moved_[source_index] = 1U;
        moved_[target_index] = 1U;
        return true;
    }

    bool Simulation::transfer_diagonal(const std::uint32_t x, const std::uint32_t y,
                                       const std::int32_t direction, const std::uint32_t substep) noexcept
    {
        const std::int32_t tx_signed = static_cast<std::int32_t>(x) + direction;
        const std::int32_t ty_signed = static_cast<std::int32_t>(y) + 1;
        if (!inside(tx_signed, ty_signed))
            return false;

        const std::uint32_t tx = static_cast<std::uint32_t>(tx_signed);
        const std::uint32_t ty = static_cast<std::uint32_t>(ty_signed);
        const std::size_t source_index = index(x, y);
        const std::size_t target_index = index(tx, ty);
        Cell source = cells_[source_index];
        Cell target = cells_[target_index];
        if (!eligible_for_substep(source, substep) || moved_[source_index] != 0U || target.is_stone())
            return false;

        if (target.is_water())
        {
            if (target.water_units >= full_water)
                return false;
            const std::uint8_t transfer = std::min<std::uint8_t>(
                source.water_units, static_cast<std::uint8_t>(full_water - target.water_units));
            source.water_units = static_cast<std::uint8_t>(source.water_units - transfer);
            target.water_units = static_cast<std::uint8_t>(target.water_units + transfer);
            target.horizontal_direction = static_cast<std::int8_t>(direction);
            target.falling = false;
            normalize_cell(source);
            normalize_cell(target);
            cells_[source_index] = source;
            cells_[target_index] = target;
            moved_[source_index] = 1U;
            moved_[target_index] = 1U;
            return true;
        }

        if (!target.is_empty())
            return false;

        const bool beside_ledge =
            stone_at(static_cast<std::int32_t>(x) - 1, static_cast<std::int32_t>(y) + 1) ||
            stone_at(static_cast<std::int32_t>(x) + 1, static_cast<std::int32_t>(y) + 1);
        if (!source.falling && (supported(x, y) || beside_ledge) &&
            !can_release_over_ledge(x, y, direction))
            return false;

        source.horizontal_direction = static_cast<std::int8_t>(direction);
        source.falling = true;
        cells_[target_index] = source;
        cells_[source_index] = Cell{};
        moved_[source_index] = 1U;
        moved_[target_index] = 1U;
        return true;
    }

    bool Simulation::transfer_horizontal(const std::uint32_t x, const std::uint32_t y,
                                         const std::int32_t direction, const std::uint32_t substep) noexcept
    {
        const std::int32_t tx_signed = static_cast<std::int32_t>(x) + direction;
        if (!inside(tx_signed, static_cast<std::int32_t>(y)))
            return false;

        const std::uint32_t tx = static_cast<std::uint32_t>(tx_signed);
        const std::size_t source_index = index(x, y);
        const std::size_t target_index = index(tx, y);
        Cell source = cells_[source_index];
        Cell target = cells_[target_index];
        if (!eligible_for_substep(source, substep) || moved_[source_index] != 0U || moved_[target_index] != 0U)
            return false;
        if (source.falling || !supported(x, y) || target.is_stone())
            return false;

        if (target.is_empty())
        {
            if (source.water_units == full_water)
            {
                source.water_units = half_water;
                source.falling = false;
                target = Cell{Material::water, half_water, static_cast<std::int8_t>(direction), false};
            }
            else
            {
                target = source;
                target.horizontal_direction = static_cast<std::int8_t>(direction);
                target.falling = false;
                source = Cell{};
            }
        }
        else if (target.is_water() && target.water_units < source.water_units)
        {
            source.water_units = static_cast<std::uint8_t>(source.water_units - 1U);
            target.water_units = static_cast<std::uint8_t>(target.water_units + 1U);
            target.horizontal_direction = static_cast<std::int8_t>(direction);
            target.falling = false;
            normalize_cell(source);
        }
        else
        {
            return false;
        }

        normalize_cell(source);
        normalize_cell(target);
        cells_[source_index] = source;
        cells_[target_index] = target;
        moved_[source_index] = 1U;
        moved_[target_index] = 1U;
        return true;
    }

    void Simulation::vertical_pass(const std::uint32_t substep) noexcept
    {
        if (height_ < 2U)
            return;
        const bool left_to_right = ((tick_ + substep) & 1ULL) == 0ULL;
        for (std::uint32_t y = height_ - 1U; y-- > 0U;)
        {
            for (std::uint32_t offset = 0; offset < width_; ++offset)
            {
                const std::uint32_t x = left_to_right ? offset : width_ - 1U - offset;
                const Cell value = cell(x, y);
                if (!eligible_for_substep(value, substep) || moved_[index(x, y)] != 0U)
                    continue;
                if (transfer_down(x, y, substep))
                    continue;
                const std::int32_t first_direction = ((tick_ + x + y + substep) & 1ULL) == 0ULL ? -1 : 1;
                if (transfer_diagonal(x, y, first_direction, substep))
                    continue;
                static_cast<void>(transfer_diagonal(x, y, -first_direction, substep));
            }
        }
    }

    void Simulation::horizontal_pass(const std::uint32_t substep) noexcept
    {
        const bool left_to_right = ((tick_ + substep) & 1ULL) != 0ULL;
        for (std::uint32_t y = 0; y < height_; ++y)
        {
            for (std::uint32_t offset = 0; offset < width_; ++offset)
            {
                const std::uint32_t x = left_to_right ? offset : width_ - 1U - offset;
                const Cell value = cell(x, y);
                if (!eligible_for_substep(value, substep) || moved_[index(x, y)] != 0U)
                    continue;
                std::int32_t preferred = value.horizontal_direction;
                if (preferred == 0)
                    preferred = ((tick_ + x + y + substep) & 1ULL) == 0ULL ? -1 : 1;
                if (transfer_horizontal(x, y, preferred, substep))
                    continue;
                static_cast<void>(transfer_horizontal(x, y, -preferred, substep));
            }
        }
    }

    void Simulation::normalize_cell(Cell& value) noexcept
    {
        if (value.material != Material::water || value.water_units == 0U)
        {
            if (value.material != Material::stone)
                value = Cell{};
            else
            {
                value.water_units = 0U;
                value.horizontal_direction = 0;
                value.falling = false;
            }
            return;
        }
        value.water_units = std::min<std::uint8_t>(value.water_units, full_water);
        value.horizontal_direction = static_cast<std::int8_t>(std::clamp<int>(value.horizontal_direction, -1, 1));
    }
}
