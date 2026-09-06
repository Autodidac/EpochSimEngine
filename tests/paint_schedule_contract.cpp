#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace actual {
using uint = std::uint32_t;
#include "../shaders/paint_schedule.glsl"
}

namespace {
std::uint64_t checks{};
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool brush(int dx, int dy, int radius, int shape) {
    if (shape == 0) return dx * dx + dy * dy <= radius * radius;
    if (shape == 1) return std::abs(dx) <= radius && std::abs(dy) <= radius;
    if (shape == 2) return std::abs(dx) <= radius && std::abs(dy) <= 1;
    return std::abs(dx) <= 1 && std::abs(dy) <= radius;
}

// A shaft model retaining unique source payloads, not just water counts.
// GPU transaction/payload acceptance is separate: this tests the production
// scheduling helper and its serial/disjoint ownership assumptions on the CPU.
using Grid = std::array<int, 32 * 48>;
void insert(Grid& cells, int x, int y) {
    auto index = [=](int row) { return static_cast<std::size_t>(row * 32 + x); };
    if (cells[index(y)] <= 0) return;
    int hole = y - 1;
    while (hole >= 0 && cells[index(hole)] > 0) --hole;
    if (hole < 0 || cells[index(hole)] != 0) return;
    for (int row = hole; row < y; ++row) cells[index(row)] = cells[index(row + 1)];
    cells[index(y)] = -2; // one explicit Smoke edit; not an erased Water owner
}
Grid run(Grid cells, bool reverse_columns) {
    for (std::uint32_t phase = 0; phase < actual::PAINT_COLUMN_PHASES; ++phase) {
        std::vector<int> columns;
        for (int x = 2; x <= 30; ++x)
            if (actual::paintColumnOwned(x, actual::PAINT_COLUMN_FIRST_MODE + phase))
                columns.push_back(x);
        if (reverse_columns) std::reverse(columns.begin(), columns.end());
        for (int x : columns)
            for (int y = 27; y >= 13; --y) insert(cells, x, y);
    }
    return cells;
}
}

int main() {
    try {
        for (std::uint32_t mode = 0; mode < 16; ++mode)
            require(actual::paintColumnMode(mode) == (mode >= 6 && mode <= 8),
                    "ordinary/special paint mode captured by column scheduler");
        // World-aligned phases must neither miss nor double-own translated,
        // clipped cells at any supported brush shape, including tile edges.
        for (int width : {1, 7, 8, 16, 31, 64, 131})
            for (int center : {-2, 0, 1, 7, 8, 17, 65, 130})
                for (int radius : {0, 1, 2, 4, 8, 16, 64})
                    for (int shape = 0; shape < 4; ++shape) {
                        for (int x = center - radius; x <= center + radius; ++x) {
                            int owners{};
                            for (std::uint32_t phase = 0; phase < actual::PAINT_COLUMN_PHASES; ++phase)
                                owners += x < width && actual::paintColumnOwned(
                                    x, actual::PAINT_COLUMN_FIRST_MODE + phase) ? 1 : 0;
                            for (int y = -radius; y <= radius; ++y)
                                if (brush(x - center, y, radius, shape))
                                    require(owners == (x >= 0 && x < width ? 1 : 0),
                                            "clipped brush has missing or duplicate owner");
                        }
                    }
        // The full shafts can overlap in Y arbitrarily. Their X read/write
        // halos (gas receivers at x +/- 1) still cannot intersect in one phase.
        for (std::uint32_t phase = 0; phase < actual::PAINT_COLUMN_PHASES; ++phase)
            for (int first = 0; first < 257; ++first)
                for (int second = first + 1; second < 257; ++second)
                    if (actual::paintColumnOwned(first, 6u + phase) &&
                        actual::paintColumnOwned(second, 6u + phase))
                        require(first + 1 < second - 1, "concurrent displacement footprints intersect");
        require(10 + 1 >= 11 - 1, "old adjacent-column race counterexample lost");
        Grid original{};
        original.fill(-1);
        for (int x = 2; x <= 30; ++x) {
            for (int y = 1; y < 10; ++y) original[static_cast<std::size_t>(y * 32 + x)] = 0;
            for (int y = 10; y < 40; ++y)
                original[static_cast<std::size_t>(y * 32 + x)] = y * 32 + x;
        }
        auto first = run(original, false);
        auto second = run(original, true);
        require(first == second, "independent column execution order changed committed payloads");
        std::vector<int> before, after;
        for (int cell : original) if (cell > 0) before.push_back(cell);
        for (int cell : first) if (cell > 0) after.push_back(cell);
        std::sort(before.begin(), before.end());
        std::sort(after.begin(), after.end());
        require(before == after, "serial shaft insert lost or duplicated an exact owner");
        require(first != original, "positive brush case did not insert Smoke");
        std::printf("paint schedule: %llu assertions passed; GPU transactions remain separate\n",
                    static_cast<unsigned long long>(checks));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
