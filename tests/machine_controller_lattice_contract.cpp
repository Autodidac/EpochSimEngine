#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace actual {
#include "../shaders/machine_controller_lattice.glsl"
}

namespace {
constexpr int radius = 6; // All production nearestAcceptingMachine callers.
static_assert(std::numeric_limits<int>::digits == 31 && (-1 & 7) == 7);
std::uint64_t assertions{}, coordinate_pairs{}, admission_masks{}, tied_masks{};

void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
struct Candidate {
    int x, y, dx, dy;
    int distance() const { return dx * dx + dy * dy; }
    bool operator==(const Candidate& other) const {
        return x == other.x && y == other.y && dx == other.dx && dy == other.dy;
    }
};
using Candidates = std::vector<Candidate>;

Candidates original(int resource_x, int resource_y) {
    Candidates candidates;
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x) {
            const int candidate_x = resource_x + x, candidate_y = resource_y + y;
            if ((candidate_x & 7) != 3 || (candidate_y & 7) != 3) continue;
            candidates.push_back({candidate_x, candidate_y, x, y});
        }
    return candidates;
}
Candidates lattice(int resource_x, int resource_y) {
    Candidates candidates;
    const int first_x = actual::machineControllerFirstOffset(resource_x, radius);
    const int first_y = actual::machineControllerFirstOffset(resource_y, radius);
    require(first_x >= -radius && first_x <= 1 && first_y >= -radius && first_y <= 1,
            "first controller offset escaped the one-period bound");
    for (int y = first_y; y <= radius; y += 8)
        for (int x = first_x; x <= radius; x += 8) {
            const int candidate_x = resource_x + x, candidate_y = resource_y + y;
            // Retained production defense, not used to hide a bad helper.
            require((candidate_x & 7) == 3 && (candidate_y & 7) == 3,
                    "helper emitted a non-controller coordinate");
            candidates.push_back({candidate_x, candidate_y, x, y});
        }
    return candidates;
}
Candidate winner(const Candidates& candidates, unsigned mask, int resource_x, int resource_y,
                 bool resource_active = true) {
    Candidate result{resource_x, resource_y, 0, 0};
    int best = 0x3fffffff;
    if (!resource_active) return result;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if ((mask & (1u << i)) == 0u) continue;
        const auto& candidate = candidates[i];
        if (candidate.distance() < best) {
            best = candidate.distance();
            result = candidate;
        }
    }
    return result;
}
struct Rectangle {
    int left, top, right, bottom;
    bool contains(int x, int y) const {
        return x >= left && y >= top && x < right && y < bottom;
    }
};
unsigned clipped_mask(const Candidates& candidates, const Rectangle& rectangle) {
    unsigned mask{};
    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (rectangle.contains(candidates[i].x, candidates[i].y)) mask |= 1u << i;
    return mask;
}
}

int main() {
    try {
        std::set<int> coordinates;
        for (int value = -40; value <= 40; ++value) coordinates.insert(value);
        for (int boundary : {64, 128, 360, 640, 1280, 1440, 5120, 10240})
            for (int delta = -8; delta <= 8; ++delta) coordinates.insert(boundary + delta);
        for (int delta = 0; delta < 8; ++delta) {
            coordinates.insert(std::numeric_limits<int>::min() + radius + delta);
            coordinates.insert(std::numeric_limits<int>::max() - 21 + delta);
            coordinates.insert(std::numeric_limits<int>::max() - radius - delta);
        }
        require(coordinates.size() == 241, "coordinate coverage changed unexpectedly");
        // Every Boolean combination subsumes clipping, material/recipe mismatch,
        // asleep/stale witnesses, and the unchanged final admission predicate.
        // This proves enumeration equivalence, not those predicates themselves.
        for (int resource_x : coordinates)
            for (int resource_y : coordinates) {
                const auto before = original(resource_x, resource_y);
                const auto after = lattice(resource_x, resource_y);
                ++coordinate_pairs;
                require(before == after, "controller order/offsets differ from the old square scan");
                require(before.size() <= 4, "radius-six candidate bound exceeded");
                for (unsigned mask = 0; mask < (1u << before.size()); ++mask) {
                    ++admission_masks;
                    const auto expected = winner(before, mask, resource_x, resource_y);
                    require(expected == winner(after, mask, resource_x, resource_y),
                            "strict-distance winner changed under an admission mask");
                    int equal_best{};
                    for (std::size_t i = 0; i < before.size(); ++i)
                        if ((mask & (1u << i)) && before[i].distance() == expected.distance()) ++equal_best;
                    if (equal_best > 1) ++tied_masks;
                }
                require(winner(after, 15u, resource_x, resource_y, false) ==
                            Candidate{resource_x, resource_y, 0, 0},
                        "inactive resource gained a recipient");
            }
        require(coordinate_pairs == 58081 && admission_masks == 501778,
                "exhaustive coordinate/admission-mask coverage changed unexpectedly");

        constexpr std::array<Rectangle, 7> rectangles{{
            {0, 0, 640, 360}, {0, 0, 5120, 1440}, {0, 0, 10240, 1440},
            {640, 360, 1280, 720}, {1, 1, 640, 360}, {635, 355, 644, 364},
            {0, 360, 640, 361}}};
        constexpr std::array<int, 38> edges{{
            0, 1, 6, 7, 8, 14, 15, 16, 31, 32, 63, 64, 65, 127, 128,
            359, 360, 361, 639, 640, 641, 719, 720, 735, 736, 1039, 1040,
            1041, 1279, 1280, 1438, 1439, 1440, 5119, 5120, 10238, 10239, 10240}};
        std::uint64_t clipped_cases{};
        for (const auto& rectangle : rectangles)
            for (int x : edges)
                for (int y : edges) {
                    const auto before = original(x, y), after = lattice(x, y);
                    const bool source_active = rectangle.contains(x, y);
                    require(winner(before, clipped_mask(before, rectangle), x, y, source_active) ==
                                winner(after, clipped_mask(after, rectangle), x, y, source_active),
                            "clipped/world-boundary recipient changed");
                    ++clipped_cases;
                }
        require(clipped_cases == 10108, "clipped controls changed unexpectedly");
        const auto tied = lattice(7, 7);
        require(winner(tied, 15u, 7, 7) == Candidate{3, 3, -4, -4}, "first row-major tie changed");
        require(winner(tied, 14u, 7, 7) == Candidate{11, 3, 4, -4}, "masked row-major tie changed");
        require(winner(tied, 0u, 7, 7) == Candidate{7, 7, 0, 0}, "empty admission fallback changed");

        std::array<unsigned, 5> histogram{};
        unsigned candidate_total{};
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const auto count = lattice(x, y).size();
                ++histogram[count];
                candidate_total += static_cast<unsigned>(count);
            }
        require(histogram == std::array<unsigned, 5>{0, 9, 30, 0, 25} && candidate_total == 169,
                "controller iteration work distribution changed");
        std::printf("machine controller lattice: %llu assertions, %llu coordinate pairs, "
                    "%llu admission masks (%llu tied), %llu clipped cases passed; "
                    "1/2/4 iterations in 9/30/25 of 64 residues, mean 2.640625 vs 169. "
                    "Work count only, not a GPU/JIT timing result.\n",
                    static_cast<unsigned long long>(assertions),
                    static_cast<unsigned long long>(coordinate_pairs),
                    static_cast<unsigned long long>(admission_masks),
                    static_cast<unsigned long long>(tied_masks),
                    static_cast<unsigned long long>(clipped_cases));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
