#include <array>
#include <cstdint>
#include <iostream>

namespace {
// Independent Boolean/coordinate proof for the proposed chemistry early gates.
// This does not execute the shader, prove complete reactions, or measure speed.
struct Demand {
    std::uint32_t bees;
    std::uint32_t other_life;
};

struct Respiration {
    bool bees = false;
    bool other_life = false;
    bool embers = false;
    bool any = false;
    std::uint32_t demand_scans = 0u;
    std::uint32_t ember_scans = 0u;
};

Respiration original_respiration(const Demand demand, const std::uint32_t bee_roll,
                                const std::uint32_t life_roll,
                                const std::uint32_t ember_roll,
                                const bool nearby_ember, const bool fire_event) {
    Respiration result;
    result.demand_scans = 1u;
    result.bees = demand.bees > 0u && bee_roll < demand.bees;
    result.other_life = demand.other_life > 0u && life_roll < demand.other_life;
    result.ember_scans = 1u;
    result.embers = nearby_ember && ember_roll == 0u;
    result.any = result.bees || result.other_life || fire_event || result.embers;
    return result;
}

Respiration gated_respiration(const Demand demand, const std::uint32_t bee_roll,
                             const std::uint32_t life_roll,
                             const std::uint32_t ember_roll,
                             const bool nearby_ember, const bool fire_event) {
    Respiration result;
    if (bee_roll < 8u || life_roll < 8u) {
        result.demand_scans = 1u;
        result.bees = demand.bees > 0u && bee_roll < demand.bees;
        result.other_life = demand.other_life > 0u && life_roll < demand.other_life;
    }
    if (ember_roll == 0u) {
        result.ember_scans = 1u;
        result.embers = nearby_ember;
    }
    result.any = result.bees || result.other_life || fire_event || result.embers;
    return result;
}

bool same_events(const Respiration old, const Respiration gated) {
    return old.bees == gated.bees && old.other_life == gated.other_life &&
           old.embers == gated.embers && old.any == gated.any;
}

struct Point { int x; int y; };
struct Grid { int width; int height; };
enum class Medium { vacuum, atmosphere, water, stone };

bool inside(const Point point, const Grid grid) {
    return point.x >= 0 && point.y >= 0 &&
           point.x < grid.width && point.y < grid.height;
}

std::uint32_t residue(const int coordinate) {
    return static_cast<std::uint32_t>(coordinate) & 7u;
}

bool controller_residue(const Point controller) {
    return residue(controller.x) == 3u && residue(controller.y) == 3u;
}

bool old_machine_owner(const Point position, const Grid grid, const bool vent,
                       const Medium medium, const bool machine,
                       const bool remaining_transition_conditions) {
    // main() rejects outside cells. at() returns non-machine Stone for an
    // outside controller; model that explicitly even when machine=true.
    if (!inside(position, grid)) return false;
    if (vent ? medium != Medium::atmosphere
             : medium != Medium::vacuum && medium != Medium::atmosphere) return false;
    const Point controller{position.x - 5, position.y + (vent ? 1 : 0)};
    if (!inside(controller, grid) || !machine || !controller_residue(controller)) return false;
    const Point output{controller.x + 5, controller.y};
    const Point owner{output.x, output.y - (vent ? 1 : 0)};
    return owner.x == position.x && owner.y == position.y && remaining_transition_conditions;
}

bool gated_machine_owner(const Point position, const Grid grid, const bool vent,
                         const Medium medium, const bool machine,
                         const bool remaining_transition_conditions) {
    if (!inside(position, grid)) return false;
    if (vent ? medium != Medium::atmosphere
             : medium != Medium::vacuum && medium != Medium::atmosphere) return false;
    if (residue(position.x) != 0u || residue(position.y) != (vent ? 2u : 3u)) return false;
    const Point controller{position.x - 5, position.y + (vent ? 1 : 0)};
    // The residue check does not establish bounds: x=0 maps to controller-5,
    // and a bottom-row vent may map to a controller just below the grid.
    if (!inside(controller, grid) || !machine || !controller_residue(controller)) return false;
    return remaining_transition_conditions;
}
} // namespace

int main() {
    std::uint64_t checks = 0u;

    // Enumerate every eight-neighbor category assignment: non-respiring, Bee/
    // Queen, or other breathing life. The production if/else count partitions
    // eight positions, so each demand and their sum are bounded by eight.
    for (std::uint32_t arrangement = 0u; arrangement < 6561u; ++arrangement) {
        auto encoded = arrangement;
        Demand demand{};
        for (std::uint32_t neighbor = 0u; neighbor < 8u; ++neighbor) {
            const auto kind = encoded % 3u;
            encoded /= 3u;
            if (kind == 1u) ++demand.bees;
            else if (kind == 2u) ++demand.other_life;
        }
        if (encoded != 0u || demand.bees > 8u || demand.other_life > 8u ||
            demand.bees + demand.other_life > 8u) return 1;
        ++checks;
    }

    // Exhaust both complete masked roll domains, not only small values. This
    // also includes zero demand and the strict threshold at rolls7 and8.
    for (const std::uint32_t mask : {0x0003ffffu, 0x0000ffffu}) {
        for (std::uint32_t demand = 0u; demand <= 8u; ++demand) {
            for (std::uint32_t roll = 0u; roll <= mask; ++roll) {
                const bool old = demand > 0u && roll < demand;
                const bool gated = roll < 8u && demand > 0u && roll < demand;
                if (old != gated) return 2;
                ++checks;
            }
        }
    }

    constexpr std::array<std::uint32_t, 16> bee_rolls{
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 15u, 127u, 255u, 65535u, 65536u, 262143u};
    constexpr std::array<std::uint32_t, 14> life_rolls{
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 15u, 127u, 255u, 65535u};
    for (std::uint32_t bees = 0u; bees <= 8u; ++bees) {
        for (std::uint32_t life = 0u; life <= 8u; ++life) {
            // Including impossible sums>8 strengthens the independent bound
            // proof: the optimization relies only on each demand<=8.
            for (const auto bee_roll : bee_rolls) {
                for (const auto life_roll : life_rolls) {
                    for (const auto ember_roll : {0u, 1u, 2047u}) {
                        for (std::uint32_t flags = 0u; flags < 4u; ++flags) {
                            const bool nearby_ember = (flags & 1u) != 0u;
                            const bool fire_event = (flags & 2u) != 0u;
                            const auto old = original_respiration({bees, life}, bee_roll,
                                life_roll, ember_roll, nearby_ember, fire_event);
                            const auto gated = gated_respiration({bees, life}, bee_roll,
                                life_roll, ember_roll, nearby_ember, fire_event);
                            if (!same_events(old, gated) ||
                                gated.demand_scans != (bee_roll < 8u || life_roll < 8u ? 1u : 0u) ||
                                gated.ember_scans != (ember_roll == 0u ? 1u : 0u)) return 3;
                            ++checks;
                        }
                    }
                }
            }
        }
    }
    for (std::uint32_t ember_roll = 0u; ember_roll <= 2047u; ++ember_roll) {
        for (const bool nearby : {false, true}) {
            if ((nearby && ember_roll == 0u) != (ember_roll == 0u && nearby)) return 4;
            ++checks;
        }
    }

    std::array<bool, 64> output_residues{};
    std::array<bool, 64> vent_residues{};
    for (const auto grid : std::array<Grid, 8>{{
             {0, 0}, {1, 1}, {4, 4}, {8, 8}, {9, 11}, {17, 19}, {25, 26}, {33, 35}}}) {
        for (int y = -8; y <= grid.height + 8; ++y) {
            for (int x = -8; x <= grid.width + 8; ++x) {
                for (const bool vent : {false, true}) {
                    for (const auto medium : {Medium::vacuum, Medium::atmosphere,
                                              Medium::water, Medium::stone}) {
                        for (std::uint32_t flags = 0u; flags < 4u; ++flags) {
                            const bool machine = (flags & 1u) != 0u;
                            const bool remaining = (flags & 2u) != 0u;
                            const auto old = old_machine_owner({x, y}, grid, vent,
                                                               medium, machine, remaining);
                            const auto gated = gated_machine_owner({x, y}, grid, vent,
                                                                   medium, machine, remaining);
                            if (old != gated) {
                                std::cerr << "Machine gate mismatch at " << x << ',' << y
                                          << " in " << grid.width << 'x' << grid.height << '\n';
                                return 5;
                            }
                            ++checks;
                        }
                    }
                    (vent ? vent_residues : output_residues)[residue(y) * 8u + residue(x)] = true;
                }
            }
        }
    }
    for (std::uint32_t residue_index = 0u; residue_index < 64u; ++residue_index)
        if (!output_residues[residue_index] || !vent_residues[residue_index]) return 6;

    // Explicit witnesses prevent a mutually wrong model from passing solely
    // by agreeing. Valid owner(8,3) derives controller(3,3); vent is(8,2).
    if (!gated_machine_owner({8, 3}, {16, 16}, false, Medium::vacuum, true, true) ||
        !gated_machine_owner({8, 2}, {16, 16}, true, Medium::atmosphere, true, true) ||
        gated_machine_owner({0, 3}, {16, 16}, false, Medium::atmosphere, true, true) ||
        gated_machine_owner({0, 2}, {16, 16}, true, Medium::atmosphere, true, true) ||
        gated_machine_owner({8, 10}, {16, 11}, true, Medium::atmosphere, true, true) ||
        gated_machine_owner({8, 2}, {16, 16}, true, Medium::vacuum, true, true) ||
        gated_machine_owner({7, 3}, {16, 16}, false, Medium::atmosphere, true, true)) return 7;

    std::cout << "Chemistry early gates: " << checks
              << " CPU predicate equivalence cases passed; all64 coordinate residues covered.\n"
              << "Proof scope: bounded neighbor demand, hash-first Ember lookup, and machine owner gates.\n"
              << "No GPU execution, complete reaction equivalence, or measured speed is claimed.\n";
    return 0;
}
