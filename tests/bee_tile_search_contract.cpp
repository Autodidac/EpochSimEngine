#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
// Match nearestBeeTile's legacy return domain and the production tile flags.
// This is an independent CPU equivalence oracle, not a GPU timing test.
constexpr std::uint32_t no_target = 0xffffu;
constexpr std::uint32_t queen = 0x00000100u;
constexpr std::uint32_t hive = 0x00000200u;
constexpr std::uint32_t flower = 0x00000400u;
constexpr std::uint32_t honey = 0x00000800u;
constexpr std::uint32_t migrating_queen = 0x00002000u;
constexpr std::uint32_t hazard = 0x00004000u;
constexpr std::uint32_t flower_forbidden = hive | queen | migrating_queen | hazard;

struct World {
    int columns;
    int rows;
    std::vector<std::uint32_t> flags;

    World(const int width, const int height)
        : columns(width), rows(height),
          flags(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

    std::uint32_t index(const int x, const int y) const {
        return static_cast<std::uint32_t>(y * columns + x);
    }
    void set(const int x, const int y, const std::uint32_t value) {
        flags[index(x, y)] = value;
    }
};

struct Query {
    int x;
    int y;
    int minimum;
    int maximum;
    std::uint32_t required;
    std::uint32_t forbidden;
};

struct Result {
    std::uint32_t target = no_target;
    int distance_squared = 0x3fffffff;
    std::uint64_t candidates = 0u;
};

// Historical row-major whole-resident-grid scan, including its explicit
// lowest-index squared-distance tie rule. Origins here are already tile
// coordinates; production tileCoordinate clamps negative cell inputs to zero.
Result original_search(const World& world, const Query query) {
    Result result;
    for (int y = 0; y < world.rows; ++y) {
        for (int x = 0; x < world.columns; ++x) {
            ++result.candidates;
            const int dx = x - query.x;
            const int dy = y - query.y;
            const int distance = std::abs(dx) + std::abs(dy);
            if (distance < query.minimum || distance > query.maximum) continue;
            const auto index = world.index(x, y);
            const auto flags = world.flags[index];
            if ((flags & query.required) != query.required ||
                (flags & query.forbidden) != 0u) continue;
            const int squared = dx * dx + dy * dy;
            if (squared < result.distance_squared ||
                (squared == result.distance_squared && index < result.target)) {
                result.distance_squared = squared;
                result.target = index;
            }
        }
    }
    return result;
}

// Proposed clipped rectangle. Keep Manhattan eligibility separate from the
// squared-distance ranking: a square alone would admit incorrect corners.
Result bounded_search(const World& world, const Query query) {
    Result result;
    const int first_x = (std::max)(query.x - query.maximum, 0);
    const int first_y = (std::max)(query.y - query.maximum, 0);
    const int last_x = (std::min)(query.x + query.maximum, world.columns - 1);
    const int last_y = (std::min)(query.y + query.maximum, world.rows - 1);
    for (int y = first_y; y <= last_y; ++y) {
        for (int x = first_x; x <= last_x; ++x) {
            ++result.candidates;
            const int dx = x - query.x;
            const int dy = y - query.y;
            const int distance = std::abs(dx) + std::abs(dy);
            if (distance < query.minimum || distance > query.maximum) continue;
            const auto index = world.index(x, y);
            const auto flags = world.flags[index];
            if ((flags & query.required) != query.required ||
                (flags & query.forbidden) != 0u) continue;
            const int squared = dx * dx + dy * dy;
            if (squared < result.distance_squared ||
                (squared == result.distance_squared && index < result.target)) {
                result.distance_squared = squared;
                result.target = index;
            }
        }
    }
    return result;
}

std::uint32_t random_word(std::uint32_t& state) {
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

bool compare(const World& world, const Query query, const std::string_view name,
             const std::optional<std::uint32_t> expected = std::nullopt) {
    const auto original = original_search(world, query);
    const auto bounded = bounded_search(world, query);
    if (original.target != bounded.target ||
        original.distance_squared != bounded.distance_squared ||
        bounded.candidates > original.candidates ||
        (expected && bounded.target != *expected)) {
        std::cerr << name << ": original=" << original.target
                  << " bounded=" << bounded.target << " grid=" << world.columns
                  << 'x' << world.rows << " origin=" << query.x << ',' << query.y
                  << " range=" << query.minimum << ".." << query.maximum << '\n';
        return false;
    }
    return true;
}
} // namespace

int main() {
    // Exact current chemistry callers: Honey, flower, migrating Queen, and
    // colony migration. Extra malformed ranges are defensive CPU cases only;
    // production callers use these positive bounded radii, not INT extremes.
    constexpr std::array<Query, 4> callers{{
        {0, 0, 0, 12, honey, hazard},
        {0, 0, 0, 48, flower, flower_forbidden},
        {0, 0, 0, 64, migrating_queen, 0u},
        {0, 0, 8, 56, flower, flower_forbidden},
    }};
    std::uint64_t checks = 0u;
    World targets(161, 145);
    targets.set(76, 73, flower); // Offset(-4,+3), squared25, later index.
    targets.set(83, 66, flower); // Offset(+3,-4), squared25, winning index.
    targets.set(80, 70, flower | hazard); // Closer but forbidden.
    targets.set(81, 70, flower | hive);
    targets.set(79, 70, flower | queen);
    targets.set(80, 69, flower | migrating_queen);
    if (!compare(targets, {80, 70, 0, 12, flower, flower_forbidden},
                 "lowest-index tie and forbidden flags", targets.index(83, 66))) return 1;
    ++checks;
    targets.set(80, 58, honey | queen);
    if (!compare(targets, {80, 70, 12, 12, honey | queen, hazard},
                 "inclusive radius and all required flags", targets.index(80, 58))) return 2;
    ++checks;
    targets.set(80, 57, honey);
    if (!compare(targets, {80, 70, 13, 13, honey | queen, 0u},
                 "partial required mask rejected", no_target)) return 3;
    ++checks;

    World corners(21, 21);
    corners.set(14, 14, flower); // Squared32 but Manhattan8: outside radius7.
    corners.set(10, 17, flower); // Squared49 but Manhattan7: eligible.
    if (!compare(corners, {10, 10, 0, 7, flower, 0u},
                 "Manhattan eligibility before squared ranking", corners.index(10, 17))) return 4;
    ++checks;
    corners.set(10, 10, honey);
    if (!compare(corners, {10, 10, 0, 0, honey, 0u},
                 "zero-radius origin", corners.index(10, 10)) ||
        !compare(corners, {10, 10, 1, 7, honey, 0u},
                 "minimum excludes origin", no_target)) return 5;
    checks += 2u;

    std::uint32_t random_state = 0x51a7b33fu;
    constexpr std::array<std::uint32_t, 6> feature_flags{
        queen, hive, flower, honey, migrating_queen, hazard};
    const auto random_flags = [&]() {
        const auto bits = random_word(random_state);
        std::uint32_t flags = bits & 0x80000001u; // Unrelated flags cannot affect selection.
        for (std::size_t bit = 0u; bit < feature_flags.size(); ++bit)
            if ((bits & (1u << (bit + 1u))) != 0u) flags |= feature_flags[bit];
        return flags;
    };

    for (const auto dimensions : std::array<std::array<int, 2>, 7>{{
             {0, 0}, {1, 1}, {1, 25}, {29, 1}, {13, 17}, {640, 180}, {1280, 180}}}) {
        World world(dimensions[0], dimensions[1]);
        for (auto& flags : world.flags) flags = random_flags();
        const std::array<std::array<int, 2>, 9> origins{{
            {0, 0}, {world.columns - 1, 0}, {0, world.rows - 1},
            {world.columns - 1, world.rows - 1}, {world.columns / 2, world.rows / 2},
            {-3, 2}, {2, -3}, {world.columns + 2, world.rows / 2},
            {world.columns / 2, world.rows + 2},
        }};
        for (const auto origin : origins) {
            for (auto query : callers) {
                query.x = origin[0];
                query.y = origin[1];
                if (!compare(world, query, "caller edge/outside origin")) return 6;
                ++checks;
            }
            for (const auto range : std::array<std::array<int, 2>, 5>{{
                     {0, -1}, {-5, -2}, {13, 12}, {-3, 12}, {0, 0}}}) {
                if (!compare(world, {origin[0], origin[1], range[0], range[1], 0u, 0u},
                             "defensive ranges")) return 7;
                ++checks;
            }
        }
    }

    // Deterministic sparse and dense random grids keep this CPU test bounded;
    // full resident dimensions above separately exercise indices above65535.
    for (std::uint32_t iteration = 0u; iteration < 1024u; ++iteration) {
        const int columns = 1 + static_cast<int>(random_word(random_state) % 97u);
        const int rows = 1 + static_cast<int>(random_word(random_state) % 83u);
        World world(columns, rows);
        for (auto& flags : world.flags) flags = random_flags();
        const int x = static_cast<int>(random_word(random_state) %
                                      static_cast<std::uint32_t>(world.columns + 24)) - 12;
        const int y = static_cast<int>(random_word(random_state) %
                                      static_cast<std::uint32_t>(world.rows + 24)) - 12;
        for (auto query : callers) {
            query.x = x;
            query.y = y;
            if (!compare(world, query, "deterministic caller random")) return 8;
            ++checks;
        }
        Query arbitrary{x, y,
            static_cast<int>(random_word(random_state) % 80u) - 8,
            static_cast<int>(random_word(random_state) % 88u) - 8,
            random_flags(), random_flags()};
        if (!compare(world, arbitrary, "deterministic arbitrary masks/ranges")) return 9;
        ++checks;
    }

    World large(1280, 180);
    large.set(700, 100, honey);
    if (!compare(large, {700, 100, 0, 12, honey, hazard},
                 "valid selected index above legacy sentinel", large.index(700, 100))) return 10;
    ++checks;
    std::cout << "Bee tile search: " << checks << " CPU equivalence cases passed.\n";
    for (const int columns : {640, 1280}) {
        World world(columns, 180);
        for (const auto query : callers) {
            const Query centered{columns / 2, 90, query.minimum, query.maximum,
                                 query.required, query.forbidden};
            const auto old = original_search(world, centered);
            const auto bounded = bounded_search(world, centered);
            const auto diameter = static_cast<std::uint64_t>(2 * query.maximum + 1);
            if (bounded.candidates != diameter * diameter ||
                old.candidates != static_cast<std::uint64_t>(columns) * 180u) return 11;
            std::cout << "Algorithmic candidate visits " << columns << "x180 radius"
                      << query.maximum << ": " << old.candidates << " -> "
                      << bounded.candidates << " (not measured GPU/frame speed).\n";
        }
    }
    return 0;
}
