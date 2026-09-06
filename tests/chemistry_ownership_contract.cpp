#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

namespace actual {
using uint = std::uint32_t;
#include "../shaders/material_ids.glsl"
#include "../shaders/chemistry_ownership.glsl"
}

namespace {
using Word = std::uint32_t;
using Cell = std::array<Word, 4>;
using Counters = std::array<std::uint64_t, 4>;

struct Material {
    Word shader_id;
    Word saved_id;
    Word owner;
};

// Independent saved-ID and owner oracle, not generated from the GLSL helper.
// Industrial inputs, compost donors, controllers and harvest donors have
// separate owners. Conveyor/Factory Core/Magnet and Ant/Beetle remain bulk;
// hive content belongs to Bees. Empty/Atmosphere own destinations, and the
// explicit water/gas/heat IDs own phases.
constexpr std::array<Material, 68> materials{{
    {actual::MAT_EMPTY, 0u, 3u},
    {actual::MAT_SAND, 1u, 2u},
    {actual::MAT_WATER, 2u, 4u},
    {actual::MAT_DIRT, 3u, 0u},
    {actual::MAT_STONE, 4u, 0u},
    {actual::MAT_CRYSTAL, 5u, 0u},
    {actual::MAT_MUD, 6u, 0u},
    {actual::MAT_ACID, 7u, 0u},
    {actual::MAT_GRASS, 8u, 0u},
    {actual::MAT_SMOKE, 9u, 4u},
    {actual::MAT_STEAM, 10u, 4u},
    {actual::MAT_FIRE, 11u, 4u},
    {actual::MAT_LAVA, 12u, 4u},
    {actual::MAT_OIL, 13u, 0u},
    {actual::MAT_WOOD, 14u, 0u},
    {actual::MAT_PLASTIC, 15u, 0u},
    {actual::MAT_ACID_RESISTANT_PLASTIC, 16u, 0u},
    {actual::MAT_HONEY, 17u, 1u},
    {actual::MAT_BEE, 18u, 1u},
    {actual::MAT_SALT, 19u, 4u},
    {actual::MAT_ICE, 20u, 4u},
    {actual::MAT_ALUMINUM, 21u, 2u},
    {actual::MAT_ASH, 22u, 0u},
    {actual::MAT_EMBER, 23u, 4u},
    {actual::MAT_GLASS, 24u, 0u},
    {actual::MAT_GUNPOWDER, 25u, 0u},
    {actual::MAT_SNOW, 26u, 4u},
    {actual::MAT_SEED, 27u, 0u},
    {actual::MAT_BEESWAX, 28u, 1u},
    {actual::MAT_FLOWER, 29u, 0u},
    {actual::MAT_SALTWATER, 30u, 4u},
    {actual::MAT_BEEHIVE, 31u, 1u},
    {actual::MAT_DIRTY_STEAM, 32u, 4u},
    {actual::MAT_DIRTY_WATER, 33u, 4u},
    {actual::MAT_POLLEN, 34u, 1u},
    {actual::MAT_QUEEN_BEE, 35u, 1u},
    {actual::MAT_IRON, 36u, 2u},
    {actual::MAT_COPPER, 37u, 2u},
    {actual::MAT_MAGNET, 38u, 0u},
    {actual::MAT_INSULATOR, 39u, 0u},
    {actual::MAT_LIGHTNING, 40u, 0u},
    {actual::MAT_MAGMA_VENT, 41u, 4u},
    {actual::MAT_URANIUM, 42u, 0u},
    {actual::MAT_RADIATION, 43u, 0u},
    {actual::MAT_ALUMINUM_SHAVINGS, 44u, 2u},
    {actual::MAT_GOLD, 45u, 2u},
    {actual::MAT_OXYGEN, 46u, 4u},
    {actual::MAT_CARBON_DIOXIDE, 47u, 4u},
    {actual::MAT_IRON_ORE, 48u, 2u},
    {actual::MAT_STEEL, 49u, 2u},
    {actual::MAT_CONVEYOR, 50u, 0u},
    {actual::MAT_SMELTER, 51u, 6u},
    {actual::MAT_ASSEMBLER, 52u, 6u},
    {actual::MAT_INSECT_HABITAT, 53u, 6u},
    {actual::MAT_POWER_CELL, 54u, 2u},
    {actual::MAT_PLASMA_AMMO, 55u, 2u},
    {actual::MAT_ANT, 56u, 0u},
    {actual::MAT_BEETLE, 57u, 0u},
    {actual::MAT_PLANT_STEM, 58u, 0u},
    {actual::MAT_FACTORY_CORE, 59u, 0u},
    {actual::MAT_SILT, 60u, 5u},
    {actual::MAT_FERTILIZER, 61u, 7u},
    {actual::MAT_FOOD, 62u, 7u},
    {actual::MAT_WASTE, 63u, 5u},
    {actual::MAT_HYDROGEN, 64u, 4u},
    {actual::MAT_SLUICE_BOX, 65u, 6u},
    {actual::MAT_ATMOSPHERE, 66u, 3u},
    {actual::MAT_CLOUD, 67u, 4u},
}};

constexpr std::array<Word, 16> unknown_ids{
    68u, 69u, 127u, 128u, 255u, 256u, 65535u, 65536u,
    0x007fff00u, 0x00800000u, 0x7fffffffu, 0x80000000u,
    0xffff0000u, 0xfffffffdu, 0xfffffffeu, 0xffffffffu};
constexpr std::size_t domain_size = materials.size() + unknown_ids.size();
using Cells = std::array<Cell, domain_size>;
constexpr Word owner_count = 8u;
using Order = std::array<Word, owner_count>;
constexpr Order canonical_order{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
constexpr Order production_order{0u, 1u, 2u, 5u, 6u, 7u, 3u, 4u};

// A bounded, explicitly non-exhaustive order suite: all distinct leading
// pairs with both tail directions, both cyclic directions, and production
// order/reverse. Deduplication gives 123 of 40,320 possible orders. Separate tests exhaust every
// owner pair's commutativity, without factorial full-composition work.
std::vector<Order> sampled_orders() {
    std::vector<Order> result;
    for (Word first = 0u; first < owner_count; ++first)
        for (Word second = 0u; second < owner_count; ++second) {
            if (first == second) continue;
            Order order{first, second};
            std::size_t index = 2u;
            for (const Word owner : canonical_order)
                if (owner != first && owner != second) order[index++] = owner;
            result.push_back(order);
            std::reverse(order.begin() + 2, order.end());
            result.push_back(order);
        }
    for (Word first = 0u; first < owner_count; ++first)
        for (const bool reverse : {false, true}) {
            Order order{};
            for (Word position = 0u; position < owner_count; ++position)
                order[position] = reverse ? (first + owner_count - position) % owner_count
                                          : (first + position) % owner_count;
            result.push_back(order);
        }
    result.push_back(production_order);
    Order reverse_production = production_order;
    std::reverse(reverse_production.begin(), reverse_production.end());
    result.push_back(reverse_production);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
constexpr Cell poison{0xdeadbeefu, 0xfeedc0deu, 0xabcdef01u, 0x98765432u};

std::uint64_t assertions{}, ownership_cases{}, transition_cases{}, composition_cases{};
std::uint64_t mutable_source_cases{}, duplicate_controls{};
std::uint64_t pair_composition_cases{};

void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}

Word expected_owner(Word material) {
    return material < materials.size() ? materials[material].owner : 0u;
}

Word domain_id(std::size_t index) {
    return index < materials.size() ? materials[index].saved_id
                                   : unknown_ids[index - materials.size()];
}

void verify_owner(Word material) {
    ++ownership_cases;
    const Word owner = actual::chemistrySourceOwner(material);
    require(owner == expected_owner(material), "immutable source mapped to wrong owner");
    Word memberships{};
    for (Word stage = 0u; stage < owner_count; ++stage) {
        const bool owns = owner == stage;
        require(owns == (expected_owner(material) == stage), "pass membership changed");
        if (owns) ++memberships;
    }
    require(memberships == 1u, "source does not have exactly one chemistry owner");
    require(owner < owner_count && owner != std::numeric_limits<Word>::max(),
            "unsupported stage owns source");
}

void add(Counters& target, const Counters& value) {
    for (std::size_t i = 0; i < target.size(); ++i) target[i] += value[i];
}

struct Proposal {
    Cell cell;
    Counters events;
};

// Synthetic four-word chemistry with immutable neighbor reads and additive
// events. This is deliberately NOT a copy or reference implementation of the
// physical shader rules. It proves composition given a per-index proposal.
Proposal synthetic_rule(const Cells& source, std::size_t index, Word result_material) {
    const auto& cell = source[index];
    const auto& left = source[(index + source.size() - 1u) % source.size()];
    const auto& right = source[(index + 1u) % source.size()];
    return {{result_material, cell[1] + 1u,
             cell[2] ^ left[3] ^ right[0], cell[3] + left[0] + right[1]},
            {1u, cell[0] == result_material ? 0u : 1u,
             static_cast<std::uint64_t>(cell[0]),
             static_cast<std::uint64_t>(result_material)}};
}

std::size_t stage_position(const Order& order, Word stage) {
    for (std::size_t i = 0; i < order.size(); ++i)
        if (order[i] == stage) return i;
    throw std::runtime_error("fixture stage order is incomplete");
}

void verify_mutable_source_control(Word source, Word result, const Order& order) {
    ++mutable_source_cases;
    // Deliberately wrong: allowing a committed result to change ownership can
    // execute common chemistry/events twice when its new owner comes later.
    Word mutable_material = source;
    Word writes{};
    for (const Word stage : order)
        if (actual::chemistrySourceOwner(mutable_material) == stage) {
            ++writes;
            mutable_material = result;
        }
    const Word source_owner = expected_owner(source);
    const Word result_owner = expected_owner(result);
    const bool duplicate = source_owner != result_owner &&
        stage_position(order, source_owner) < stage_position(order, result_owner);
    require(writes == (duplicate ? 2u : 1u), "mutable-source negative control changed");
    if (duplicate) ++duplicate_controls;
}

void verify_composition(const Cells& source, std::size_t shift, const Order& order,
                        bool reverse, std::size_t begin, std::size_t end) {
    ++composition_cases;
    Cells expected;
    expected.fill(poison);
    Counters expected_events{};
    for (std::size_t i = begin; i < end; ++i) {
        const auto proposal = synthetic_rule(source, i, domain_id((i + shift) % domain_size));
        expected[i] = proposal.cell;
        add(expected_events, proposal.events);
    }

    Cells scratch;
    scratch.fill(poison);
    std::array<Word, domain_size> writes{}, common_executions{};
    std::array<Word, domain_size> writer;
    writer.fill(std::numeric_limits<Word>::max());
    Counters events{};
    for (const Word stage : order)
        for (std::size_t invocation = 0; invocation < source.size(); ++invocation) {
            const std::size_t i = reverse ? source.size() - 1u - invocation : invocation;
            if (i < begin || i >= end || actual::chemistrySourceOwner(source[i][0]) != stage)
                continue;
            require(writes[i] == 0u, "two chemistry passes wrote the same scratch index");
            ++common_executions[i];
            const auto proposal = synthetic_rule(source, i, domain_id((i + shift) % domain_size));
            scratch[i] = proposal.cell;
            writer[i] = stage;
            ++writes[i];
            add(events, proposal.events);
        }

    for (std::size_t i = 0; i < source.size(); ++i) {
        const bool inside = i >= begin && i < end;
        require(writes[i] == (inside ? 1u : 0u), "missing write or out-of-scope scratch mutation");
        require(common_executions[i] == (inside ? 1u : 0u), "common rule ran zero or multiple times");
        require(writer[i] == (inside ? expected_owner(source[i][0])
                                    : std::numeric_limits<Word>::max()),
                "result material stole the immutable source's writer");
        require(scratch[i] == expected[i], "composed four-word payload or sentinel changed");
    }
    require(events == expected_events, "partition duplicated or omitted additive events");
}

void verify_pair_commutativity(const Cells& source, std::size_t shift,
                               Word first, Word second, bool reverse_invocations,
                               std::size_t begin, std::size_t end) {
    ++pair_composition_cases;
    Cells expected;
    expected.fill(poison);
    Counters expected_events{};
    for (std::size_t i = begin; i < end; ++i) {
        const Word owner = expected_owner(source[i][0]);
        if (owner != first && owner != second) continue;
        const auto proposal = synthetic_rule(source, i, domain_id((i + shift) % domain_size));
        expected[i] = proposal.cell;
        add(expected_events, proposal.events);
    }
    for (const bool reverse_owners : {false, true}) {
        Cells scratch;
        scratch.fill(poison);
        Counters events{};
        std::array<Word, domain_size> writes{};
        const std::array pair = reverse_owners ? std::array{second, first}
                                              : std::array{first, second};
        for (const Word owner : pair)
            for (std::size_t invocation = 0u; invocation < domain_size; ++invocation) {
                const std::size_t i = reverse_invocations ? domain_size - 1u - invocation : invocation;
                if (i < begin || i >= end || actual::chemistrySourceOwner(source[i][0]) != owner)
                    continue;
                const auto proposal = synthetic_rule(source, i, domain_id((i + shift) % domain_size));
                scratch[i] = proposal.cell;
                ++writes[i];
                add(events, proposal.events);
            }
        for (std::size_t i = 0u; i < domain_size; ++i) {
            const Word owner = expected_owner(source[i][0]);
            const bool selected = i >= begin && i < end && (owner == first || owner == second);
            require(writes[i] == (selected ? 1u : 0u), "owner pair duplicated or omitted a source write");
            require(scratch[i] == expected[i], "owner pair did not commute at an exact four-word payload");
        }
        require(events == expected_events, "owner pair did not commute at additive event counters");
    }
}
}

int main() {
    try {
        require(actual::MATERIAL_COUNT == 68u, "canonical material table changed without a new oracle");
        require(actual::CHEMISTRY_OWNER_BULK == 0u && actual::CHEMISTRY_OWNER_BEES == 1u &&
                    actual::CHEMISTRY_OWNER_MACHINERY == 2u &&
                    actual::CHEMISTRY_OWNER_DESTINATIONS == 3u &&
                    actual::CHEMISTRY_OWNER_PHASES == 4u &&
                    actual::CHEMISTRY_OWNER_ECOLOGY_DONORS == 5u &&
                    actual::CHEMISTRY_OWNER_CONTROLLERS == 6u &&
                    actual::CHEMISTRY_OWNER_HARVEST_DONORS == 7u,
                "host-visible chemistry stage numbers changed");
        Order totals{};
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto& material = materials[i];
            require(material.saved_id == i, "independent material oracle has a gap or duplicate");
            require(material.shader_id == material.saved_id, "canonical saved material ID changed");
            verify_owner(material.shader_id);
            ++totals[material.owner];
        }
        require(totals == Order{25u, 6u, 10u, 2u, 17u, 2u, 4u, 2u},
                "canonical partition sizes changed");

        const auto orders = sampled_orders();
        require(orders.size() == 123u, "bounded sampled stage-order coverage changed");
        require(std::binary_search(orders.begin(), orders.end(), production_order),
                "bounded suite omitted the actual production owner order");
        std::array<Order, owner_count> position_totals{}, ordered_pair_totals{}, leading_pair_totals{};
        for (std::size_t i = 0; i < orders.size(); ++i) {
            Order sorted = orders[i];
            std::sort(sorted.begin(), sorted.end());
            require(sorted == canonical_order, "stage order lost or duplicated an owner");
            require(i == 0u || orders[i - 1u] < orders[i], "sampled stage orders are not unique");
            for (std::size_t position = 0; position < orders[i].size(); ++position)
                ++position_totals[orders[i][position]][position];
            ++leading_pair_totals[orders[i][0]][orders[i][1]];
            for (std::size_t before = 0; before < owner_count; ++before)
                for (std::size_t after = before + 1u; after < owner_count; ++after)
                    ++ordered_pair_totals[orders[i][before]][orders[i][after]];
        }
        for (const auto& positions : position_totals)
            for (const Word count : positions)
                require(count > 0u, "sampled suite must put every owner at every stage position");
        for (Word before = 0u; before < owner_count; ++before)
            for (Word after = 0u; after < owner_count; ++after)
                require(before == after ? ordered_pair_totals[before][after] == 0u
                                        : ordered_pair_totals[before][after] > 0u &&
                                          leading_pair_totals[before][after] >= 2u,
                        "sampled suite must include each ordered leading pair with both tail directions");

        // Exhaust every 16-bit ID, including the entire invalid 68..65535
        // range. Full-width probes separately cover sign/high-bit boundaries.
        for (Word material = 0u; material < 65536u; ++material) verify_owner(material);
        for (const Word material : unknown_ids) {
            require(material >= 68u, "unknown-ID probe became a canonical material");
            verify_owner(material);
        }

        Cells initial{};
        for (std::size_t i = 0; i < initial.size(); ++i)
            initial[i] = {domain_id(i), static_cast<Word>(i) * 17u + 3u,
                          0x80000000u ^ static_cast<Word>(i),
                          0x00800000u | (static_cast<Word>(i) * 257u)};
        const Cells source = initial;
        std::array<std::uint64_t, owner_count * owner_count> transition_totals{};
        for (std::size_t shift = 0; shift < domain_size; ++shift) {
            for (std::size_t i = 0; i < domain_size; ++i) {
                const Word result = domain_id((i + shift) % domain_size);
                ++transition_cases;
                ++transition_totals[expected_owner(source[i][0]) * owner_count + expected_owner(result)];
                for (const auto& order : orders)
                    verify_mutable_source_control(source[i][0], result, order);
            }
            for (const auto& order : orders)
                for (const bool reverse : {false, true}) {
                    // Full, clipped, and empty dispatch models retain poison
                    // outside scope. These are not GPU rectangle/barrier tests.
                    verify_composition(source, shift, order, reverse, 0u, domain_size);
                    verify_composition(source, shift, order, reverse, 1u, domain_size - 1u);
                    verify_composition(source, shift, order, reverse, domain_size / 2u, domain_size / 2u);
                }
            for (Word first = 0u; first < owner_count; ++first)
                for (Word second = first + 1u; second < owner_count; ++second)
                    for (const bool reverse : {false, true}) {
                        verify_pair_commutativity(source, shift, first, second, reverse, 0u, domain_size);
                        verify_pair_commutativity(source, shift, first, second, reverse, 1u, domain_size - 1u);
                        verify_pair_commutativity(source, shift, first, second, reverse,
                                                  domain_size / 2u, domain_size / 2u);
                    }
        }
        require(source == initial, "stage composition mutated the immutable canonical source");
        constexpr Order expected_domain_totals{41u, 6u, 10u, 2u, 17u, 2u, 4u, 2u};
        std::uint64_t expected_duplicates{};
        for (Word from = 0u; from < owner_count; ++from)
            for (Word to = 0u; to < owner_count; ++to) {
                const auto pairs = static_cast<std::uint64_t>(expected_domain_totals[from]) *
                                   expected_domain_totals[to];
                require(transition_totals[from * owner_count + to] == pairs,
                        "all 64 source/result ownership transitions were not exhausted");
                expected_duplicates += pairs * ordered_pair_totals[from][to];
            }
        require(ownership_cases == 65620u && transition_cases == 7056u &&
                    composition_cases == 61992u && mutable_source_cases == 867888u &&
                    pair_composition_cases == 14112u && duplicate_controls == expected_duplicates &&
                    duplicate_controls > 0u,
                "partition, pair-commutation or bounded composition coverage changed unexpectedly");
        std::printf("chemistry ownership: %llu assertions; %llu source IDs; "
                    "%llu source/result pairs; %llu compositions (123 sampled stage orders of 40,320, "
                    "two invocation orders, full/clipped/empty scopes); "
                    "%llu pair compositions (all 28 owner pairs in both execution orders); "
                    "%llu mutable-source controls (%llu duplicated-write witnesses). "
                    "Actual shared partition helper and synthetic CPU composition only; "
                    "not full shader numerical equivalence, GPU synchronization, "
                    "runtime acceptance, or JIT/performance proof.\n",
                    static_cast<unsigned long long>(assertions),
                    static_cast<unsigned long long>(ownership_cases),
                    static_cast<unsigned long long>(transition_cases),
                    static_cast<unsigned long long>(composition_cases),
                    static_cast<unsigned long long>(pair_composition_cases),
                    static_cast<unsigned long long>(mutable_source_cases),
                    static_cast<unsigned long long>(duplicate_controls));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "chemistry ownership after %llu assertions, composition %llu: %s\n",
                     static_cast<unsigned long long>(assertions),
                     static_cast<unsigned long long>(composition_cases), error.what());
        return 1;
    }
}
