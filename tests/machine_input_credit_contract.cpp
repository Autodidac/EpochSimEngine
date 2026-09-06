#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace actual {
using uint = std::uint32_t;
#include "../shaders/machine_input_credit.glsl"
}

namespace {
using Count = std::uint32_t;
constexpr std::size_t controllers = 4, slots = 4, footprint = 13 * 13;
using Credits = std::array<Count, controllers * slots>;

// The unchanged spatial/material/awake election supplies these immutable
// inputs. This test proves accounting equivalence, not the GPU predicates or
// lattice election themselves. An unselected/full owner never gets rerouted.
struct Donor {
    Count row_major_index{};
    int owner{-1};
    int slot{-1};
    bool active{true};
    bool awake{true};
    bool compatible{true};
};
struct Snapshot {
    std::array<Donor, footprint> donors{};
    std::size_t size{};
    Credits capacity{}; // 15 minus immutable source inventory, not output state.
    std::array<bool, controllers> controller_active{true, true, true, true};
};
struct Outcome {
    Credits credits{};
    std::array<bool, footprint> accepted{};
    Count accepted_count{};
};

std::uint64_t assertions{}, cases{}, scalar_cases{}, donor_visits{}, rank_visits{};
std::uint64_t short_masks{}, mixed_masks{}, dense_cases{}, output_cases{}, output_controls{};

void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}

int eligible_key(const Snapshot& snapshot, const Donor& donor) {
    if (!donor.active || !donor.awake || !donor.compatible || donor.owner < 0 ||
        donor.owner >= static_cast<int>(controllers) || donor.slot < 0 ||
        donor.slot >= static_cast<int>(slots)) return -1;
    if (!snapshot.controller_active[static_cast<std::size_t>(donor.owner)]) return -1;
    return donor.owner * static_cast<int>(slots) + donor.slot;
}

Outcome original_rank_oracle(const Snapshot& snapshot) {
    Outcome result;
    for (std::size_t i = 0; i < snapshot.size; ++i) {
        const auto& donor = snapshot.donors[i];
        const int key = eligible_key(snapshot, donor);
        if (key < 0) continue;
        Count rank{};
        // Deliberately retain the old independent per-donor recount. Do not
        // replace this oracle with min(total, capacity) or an online counter.
        for (std::size_t other = 0; other < snapshot.size; ++other) {
            ++rank_visits;
            const auto& candidate = snapshot.donors[other];
            if (candidate.row_major_index < donor.row_major_index &&
                eligible_key(snapshot, candidate) == key) ++rank;
        }
        if (rank < snapshot.capacity[static_cast<std::size_t>(key)]) {
            ++result.credits[static_cast<std::size_t>(key)];
            result.accepted[i] = true;
            ++result.accepted_count;
        }
    }
    return result;
}

Outcome single_pass_actual(const Snapshot& snapshot) {
    Outcome result;
    for (std::size_t i = 0; i < snapshot.size; ++i) {
        ++donor_visits;
        const int key = eligible_key(snapshot, snapshot.donors[i]);
        if (key < 0) continue;
        const auto index = static_cast<std::size_t>(key);
        const Count before = result.credits[index];
        result.credits[index] = actual::machineInputCreditIncrement(
            before, snapshot.capacity[index]);
        if (result.credits[index] != before) {
            result.accepted[i] = true;
            ++result.accepted_count;
        }
    }
    return result;
}

Outcome verify(const Snapshot& snapshot) {
    ++cases;
    require(snapshot.size <= footprint, "donor footprint exceeded radius-six bound");
    bool ordered = true;
    for (std::size_t i = 1; i < snapshot.size; ++i)
        ordered = ordered && snapshot.donors[i - 1].row_major_index <
            snapshot.donors[i].row_major_index;
    require(ordered, "fixture is not a unique row-major immutable donor stream");
    const auto expected = original_rank_oracle(snapshot);
    const auto observed = single_pass_actual(snapshot);
    require(observed.credits == expected.credits, "per-controller-slot credit differs from original rank");
    require(observed.accepted == expected.accepted, "accepted donor owners/order changed");
    require(observed.accepted_count == expected.accepted_count, "accepted donor count changed");
    Count credited{};
    for (std::size_t key = 0; key < observed.credits.size(); ++key) {
        require(snapshot.capacity[key] <= 15u, "fixture capacity exceeds packed nibble");
        require(observed.credits[key] <= snapshot.capacity[key], "source capacity exceeded");
        credited += observed.credits[key];
    }
    require(credited == observed.accepted_count, "recipient credits lack matching exact donor debits");
    return observed;
}

Snapshot stream(std::size_t size) {
    Snapshot snapshot;
    snapshot.size = size;
    snapshot.capacity.fill(15u);
    for (std::size_t i = 0; i < size; ++i)
        snapshot.donors[i] = {1000u + static_cast<Count>(i) * 17u, 0, 0};
    return snapshot;
}

void reject(Donor& donor, unsigned reason) {
    switch (reason % 7u) {
    case 0u: donor.active = false; break;
    case 1u: donor.awake = false; break;
    case 2u: donor.compatible = false; break;
    case 3u: donor.owner = -1; break;
    case 4u: donor.owner = static_cast<int>(controllers); break;
    case 5u: donor.slot = -1; break;
    default: donor.slot = static_cast<int>(slots); break;
    }
}
}

int main() {
    try {
        for (Count capacity = 0; capacity <= 15u; ++capacity)
            for (Count current = 0; current <= footprint; ++current) {
                ++scalar_cases;
                const Count expected = current < capacity ? current + 1u : current;
                require(actual::machineInputCreditIncrement(current, capacity) == expected,
                        "shared GLSL scalar increment changed");
            }

        // Every admission mask for eight donors, every controller/slot, and
        // every nibble capacity. Rejected records exercise all seven reasons.
        for (int owner = 0; owner < static_cast<int>(controllers); ++owner)
            for (int slot = 0; slot < static_cast<int>(slots); ++slot)
                for (Count capacity = 0; capacity <= 15u; ++capacity)
                    for (unsigned mask = 0; mask < 256u; ++mask) {
                        auto snapshot = stream(8);
                        snapshot.capacity[static_cast<std::size_t>(owner * 4 + slot)] = capacity;
                        for (unsigned i = 0; i < 8u; ++i) {
                            snapshot.donors[i].owner = owner;
                            snapshot.donors[i].slot = slot;
                            if ((mask & (1u << i)) == 0u) reject(snapshot.donors[i], i);
                        }
                        verify(snapshot);
                        ++short_masks;
                    }

        // Two row-major donors for each of all 16 controller/slot keys. Every
        // mask chooses the first donor's eligibility; the second stays valid.
        // The four capacity nibbles also exhaust all 16^4 combinations.
        for (unsigned mask = 0; mask < 65536u; ++mask) {
            auto snapshot = stream(32);
            for (unsigned key = 0; key < 16u; ++key) {
                snapshot.capacity[key] = (mask >> ((key % 4u) * 4u)) & 15u;
                for (unsigned member = 0; member < 2u; ++member) {
                    auto& donor = snapshot.donors[key * 2u + member];
                    donor.owner = static_cast<int>(key / 4u);
                    donor.slot = static_cast<int>(key % 4u);
                    if (member == 0u && (mask & (1u << key)) == 0u) reject(donor, key);
                }
            }
            verify(snapshot);
            ++mixed_masks;
        }

        // Dense 169-donor overflow at every controller/slot and capacity.
        for (int owner = 0; owner < static_cast<int>(controllers); ++owner)
            for (int slot = 0; slot < static_cast<int>(slots); ++slot)
                for (Count capacity = 0; capacity <= 15u; ++capacity) {
                    auto snapshot = stream(footprint);
                    snapshot.capacity.fill(capacity);
                    for (auto& donor : snapshot.donors) {
                        donor.owner = owner;
                        donor.slot = slot;
                    }
                    verify(snapshot);
                    ++dense_cases;
                }

        // Alternating slots/owners, incomplete recipes, rejected candidates,
        // and a full nearest owner beside a spare competitor. Each pattern
        // covers all 16 controller-active masks and all capacity values.
        for (unsigned pattern = 0; pattern < 6u; ++pattern)
            for (Count capacity = 0; capacity <= 15u; ++capacity)
                for (unsigned active_mask = 0; active_mask < 16u; ++active_mask) {
                    auto snapshot = stream(footprint);
                    for (unsigned owner = 0; owner < 4u; ++owner)
                        snapshot.controller_active[owner] = (active_mask & (1u << owner)) != 0u;
                    for (unsigned key = 0; key < 16u; ++key)
                        snapshot.capacity[key] = (capacity + key * 3u) & 15u;
                    for (unsigned i = 0; i < footprint; ++i) {
                        auto& donor = snapshot.donors[i];
                        donor.owner = pattern == 0u ? 0 : static_cast<int>(i % 4u);
                        donor.slot = static_cast<int>((pattern == 1u ? i : i / 4u) % 4u);
                        if (pattern == 2u && i % 3u != 0u) reject(donor, i);
                        if (pattern == 3u) {
                            constexpr std::array<int, 4> recipe_slots{2, 2, 4, 3};
                            donor.compatible = donor.slot < recipe_slots[static_cast<std::size_t>(donor.owner)];
                        }
                        if (pattern == 4u) {
                            donor.owner = i % 3u == 0u ? 1 : 0;
                            donor.slot = 0;
                            snapshot.capacity[0] = 0u;
                            snapshot.capacity[4] = 15u;
                        }
                        if (pattern == 5u) {
                            donor.owner = static_cast<int>(i % 6u) - 1;
                            donor.slot = static_cast<int>((i / 6u) % 6u) - 1;
                            donor.active = i % 7u != 0u;
                            donor.awake = i % 11u != 0u;
                        }
                    }
                    verify(snapshot);
                    ++dense_cases;
                }

        // Output can spend only source inventory. It cannot enlarge this
        // tick's incoming capacity, even if every refused donor is available.
        for (unsigned key = 0; key < 16u; ++key)
            for (Count capacity = 0; capacity <= 15u; ++capacity)
                for (Count output = 0; output <= 15u - capacity; ++output) {
                    auto snapshot = stream(16);
                    snapshot.capacity[key] = capacity;
                    for (std::size_t i = 0; i < snapshot.size; ++i) {
                        snapshot.donors[i].owner = static_cast<int>(key / 4u);
                        snapshot.donors[i].slot = static_cast<int>(key % 4u);
                    }
                    const auto observed = verify(snapshot);
                    const Count source_inventory = 15u - capacity;
                    require(source_inventory + observed.credits[key] - output == 15u - output,
                            "same-tick output changed source-capacity admission");
                    if (output != 0u) {
                        auto incorrectly_expanded = snapshot;
                        incorrectly_expanded.capacity[key] += output;
                        require(single_pass_actual(incorrectly_expanded).credits[key] > observed.credits[key],
                                "output control no longer detects incorrectly expanded capacity");
                        ++output_controls;
                    }
                    ++output_cases;
                }

        require(scalar_cases == 2720u && short_masks == 65536u && mixed_masks == 65536u &&
                    dense_cases == 1792u && output_cases == 2176u && output_controls == 1920u &&
                    cases == 135040u,
                "bounded exhaustive accounting coverage changed unexpectedly");
        std::printf("machine input credit: %llu assertions; %llu scalar cases; "
                    "%llu eight-donor masks; %llu mixed 4-controller x 4-slot masks; "
                    "%llu full-169-donor cases; %llu same-tick-output cases (%llu negative controls); "
                    "%llu total snapshot cases, %llu single-pass donor visits, %llu oracle rank visits. "
                    "Shared scalar accounting only, not GPU predicate/payload or JIT timing proof.\n",
                    static_cast<unsigned long long>(assertions),
                    static_cast<unsigned long long>(scalar_cases),
                    static_cast<unsigned long long>(short_masks),
                    static_cast<unsigned long long>(mixed_masks),
                    static_cast<unsigned long long>(dense_cases),
                    static_cast<unsigned long long>(output_cases),
                    static_cast<unsigned long long>(output_controls),
                    static_cast<unsigned long long>(cases),
                    static_cast<unsigned long long>(donor_visits),
                    static_cast<unsigned long long>(rank_visits));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "machine input credit case %llu: %s\n",
                     static_cast<unsigned long long>(cases), error.what());
        return 1;
    }
}
