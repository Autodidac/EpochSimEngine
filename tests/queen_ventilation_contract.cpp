#include <array>
#include <cstdint>
#include <iostream>

namespace shader_ventilation {
using uint = std::uint32_t;
uint hash32(uint value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}
// Compile the actual production admission helper, not a duplicate predicate.
#include "queen_ventilation.glsl"
}

namespace {
using uint = std::uint32_t;
constexpr uint roll_mask = (1u << 18u) - 1u;
constexpr uint bee_salt = 0xb33a71u;

// Invert the bijective 32-bit hash so every possible masked roll is tested,
// including all rare event boundaries, rather than hoping random inputs hit it.
constexpr uint undo_right_xor(const uint value, const uint shift) {
    uint result = value;
    for (uint bits = shift; bits < 32u; bits += shift) result ^= value >> bits;
    return result;
}
constexpr uint odd_inverse(const uint multiplier) {
    uint inverse = 1u;
    for (uint step = 0u; step < 5u; ++step)
        inverse *= 2u - multiplier * inverse;
    return inverse;
}
constexpr uint inverse_hash(uint value) {
    value = undo_right_xor(value, 16u);
    value *= odd_inverse(0x846ca68bu);
    value = undo_right_xor(value, 15u);
    value *= odd_inverse(0x7feb352du);
    return undo_right_xor(value, 16u);
}

bool exhaustive_required_breaths(std::uint64_t& cases) {
    constexpr std::array<uint, 4> high_bits{0u, 1u, 0x1555u, 0x3fffu};
    for (const uint high : high_bits) {
        for (uint roll = 0u; roll <= roll_mask; ++roll) {
            const uint hashed = (high << 18u) | roll;
            const uint random = inverse_hash(hashed) ^ bee_salt;
            if (shader_ventilation::hash32(random ^ bee_salt) != hashed) {
                std::cerr << "Invalid inverse-hash coverage oracle\n";
                return false;
            }
            for (uint bees = 0u; bees <= 8u; ++bees) {
                const uint expected = 1u + (bees != 0u && roll < bees ? 1u : 0u);
                const uint actual = shader_ventilation::queenVentRequiredBreaths(bees, random);
                ++cases;
                if (actual != expected) {
                    std::cerr << "Vent demand mismatch: bees=" << bees
                              << " roll=" << roll << " high=" << high << '\n';
                    return false;
                }
            }
        }
    }
    return true;
}

// Independent finite-medium oracle. This is deliberately not represented as
// execution of materials.glsl: the GPU fixtures must test its real Cell ABI,
// source-only endpoint agreement, and the production respirePackedMedium body.
enum class Component { absent, carbon, incompatible };
struct Medium {
    uint oxygen;
    uint stored;
    Component component;
    uint flags;
    uint age;
    std::int32_t temperature;
    bool operator==(const Medium&) const = default;
};

bool model_single_breath(Medium& medium) {
    if (medium.oxygen <= 1u || medium.stored >= 255u ||
        (medium.stored != 0u && medium.component != Component::carbon)) return false;
    --medium.oxygen;
    ++medium.stored;
    medium.component = Component::carbon;
    return true;
}

bool model_admission(const Medium& source, const uint breaths, Medium& committed) {
    Medium candidate = source;
    for (uint breath = 0u; breath < breaths; ++breath)
        if (!model_single_breath(candidate)) return false;
    committed = candidate;
    return true;
}

bool finite_budget_and_atomicity(std::uint64_t& cases) {
    constexpr std::array<uint, 5> rolls{0u, 1u, 7u, 8u, roll_mask};
    constexpr std::array<Component, 2> components{Component::carbon, Component::incompatible};
    for (uint oxygen = 0u; oxygen <= 255u; ++oxygen) {
        for (uint stored = 0u; stored <= 255u; ++stored) {
            for (const auto component : components) {
                for (uint bees = 0u; bees <= 8u; ++bees) {
                    for (const uint roll : rolls) {
                        const uint random = inverse_hash(roll) ^ bee_salt;
                        const uint breaths =
                            shader_ventilation::queenVentRequiredBreaths(bees, random);
                        const Medium source{oxygen, stored,
                            stored == 0u ? Component::absent : component,
                            0x81000000u, 0xfedcba98u, -37};
                        Medium committed = source;
                        const bool accepted = model_admission(source, breaths, committed);
                        const bool expected = oxygen > breaths &&
                            stored <= 255u - breaths &&
                            (stored == 0u || component == Component::carbon);
                        ++cases;
                        if (accepted != expected || (!accepted && !(committed == source))) {
                            std::cerr << "Finite vent admission or failure atomicity mismatch\n";
                            return false;
                        }
                        if (accepted && (committed.oxygen + breaths != source.oxygen ||
                            committed.stored != source.stored + breaths ||
                            committed.oxygen + committed.stored != source.oxygen + source.stored ||
                            committed.component != Component::carbon ||
                            committed.flags != source.flags || committed.age != source.age ||
                            committed.temperature != source.temperature)) {
                            std::cerr << "Finite vent changed pressure or unrelated owner payload\n";
                            return false;
                        }
                    }
                }
            }
        }
    }

    // Both owners have a real event. Two Oxygen units leave only one spendable
    // unit: the first modeled breath succeeds, the second fails, and neither
    // endpoint may commit the partially spent candidate as a funded Queen.
    const uint overlapping_event = inverse_hash(0u) ^ bee_salt;
    const uint two_breaths = shader_ventilation::queenVentRequiredBreaths(1u, overlapping_event);
    const Medium insufficient{2u, 3u, Component::carbon, 0x01000000u, 9u, 119};
    Medium result = insufficient;
    if (two_breaths != 2u || model_admission(insufficient, two_breaths, result) ||
        !(result == insufficient)) return false;
    const Medium sufficient{3u, 253u, Component::carbon, 0x01000000u, 9u, 119};
    result = sufficient;
    if (!model_admission(sufficient, two_breaths, result) ||
        result.oxygen != 1u || result.stored != 255u) return false;
    return true;
}
}

int main() {
    std::uint64_t demand_cases = 0u;
    std::uint64_t budget_cases = 0u;
    if (!exhaustive_required_breaths(demand_cases)) return 1;
    if (!finite_budget_and_atomicity(budget_cases)) return 2;
    std::cout << "Queen vent actual shared demand helper: " << demand_cases
              << " exhaustive roll/count/high-bit cases; " << budget_cases
              << " independent finite-budget/atomicity cases. "
                 "GPU endpoint and packed-Cell execution remain separate acceptance.\n";
    return 0;
}
