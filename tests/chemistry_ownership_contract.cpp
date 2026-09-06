#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>

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
// In particular, resources belong to machinery, but Conveyor/Factory Core/
// Magnet and Ant/Beetle remain bulk; hive content belongs to Bees. Empty and
// Atmosphere own destinations; the explicit water/gas/heat IDs own phases.
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
    {actual::MAT_SMELTER, 51u, 2u},
    {actual::MAT_ASSEMBLER, 52u, 2u},
    {actual::MAT_INSECT_HABITAT, 53u, 2u},
    {actual::MAT_POWER_CELL, 54u, 2u},
    {actual::MAT_PLASMA_AMMO, 55u, 2u},
    {actual::MAT_ANT, 56u, 0u},
    {actual::MAT_BEETLE, 57u, 0u},
    {actual::MAT_PLANT_STEM, 58u, 0u},
    {actual::MAT_FACTORY_CORE, 59u, 0u},
    {actual::MAT_SILT, 60u, 2u},
    {actual::MAT_FERTILIZER, 61u, 2u},
    {actual::MAT_FOOD, 62u, 2u},
    {actual::MAT_WASTE, 63u, 2u},
    {actual::MAT_HYDROGEN, 64u, 4u},
    {actual::MAT_SLUICE_BOX, 65u, 2u},
    {actual::MAT_ATMOSPHERE, 66u, 3u},
    {actual::MAT_CLOUD, 67u, 4u},
}};

constexpr std::array<Word, 16> unknown_ids{
    68u, 69u, 127u, 128u, 255u, 256u, 65535u, 65536u,
    0x007fff00u, 0x00800000u, 0x7fffffffu, 0x80000000u,
    0xffff0000u, 0xfffffffdu, 0xfffffffeu, 0xffffffffu};
constexpr std::size_t domain_size = materials.size() + unknown_ids.size();
using Cells = std::array<Cell, domain_size>;
using Order = std::array<Word, 5>;
constexpr auto orders = [] {
    std::array<Order, 120> permutations{};
    Order order{0u, 1u, 2u, 3u, 4u};
    std::size_t index{};
    do {
        permutations[index++] = order;
    } while (std::next_permutation(order.begin(), order.end()));
    return permutations;
}();
constexpr Cell poison{0xdeadbeefu, 0xfeedc0deu, 0xabcdef01u, 0x98765432u};

std::uint64_t assertions{}, ownership_cases{}, transition_cases{}, composition_cases{};
std::uint64_t mutable_source_cases{}, duplicate_controls{};

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
    for (Word stage = 0u; stage < 5u; ++stage) {
        const bool owns = owner == stage;
        require(owns == (expected_owner(material) == stage), "pass membership changed");
        if (owns) ++memberships;
    }
    require(memberships == 1u, "source does not have exactly one chemistry owner");
    require(owner != 5u && owner != std::numeric_limits<Word>::max(),
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
}

int main() {
    try {
        require(actual::MATERIAL_COUNT == 68u, "canonical material table changed without a new oracle");
        require(actual::CHEMISTRY_OWNER_BULK == 0u && actual::CHEMISTRY_OWNER_BEES == 1u &&
                    actual::CHEMISTRY_OWNER_MACHINERY == 2u &&
                    actual::CHEMISTRY_OWNER_DESTINATIONS == 3u &&
                    actual::CHEMISTRY_OWNER_PHASES == 4u,
                "host-visible chemistry stage numbers changed");
        std::array<Word, 5> totals{};
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto& material = materials[i];
            require(material.saved_id == i, "independent material oracle has a gap or duplicate");
            require(material.shader_id == material.saved_id, "canonical saved material ID changed");
            verify_owner(material.shader_id);
            ++totals[material.owner];
        }
        require(totals == std::array<Word, 5>{25u, 6u, 18u, 2u, 17u},
                "canonical partition sizes changed");

        std::array<Order, 5> position_totals{};
        for (std::size_t i = 0; i < orders.size(); ++i) {
            Order sorted = orders[i];
            std::sort(sorted.begin(), sorted.end());
            require(sorted == Order{0u, 1u, 2u, 3u, 4u}, "stage order lost or duplicated an owner");
            require(i == 0u || orders[i - 1u] < orders[i], "stage orders are not unique and exhaustive");
            for (std::size_t position = 0; position < orders[i].size(); ++position)
                ++position_totals[orders[i][position]][position];
        }
        for (const auto& positions : position_totals)
            require(positions == Order{24u, 24u, 24u, 24u, 24u},
                    "every owner must occupy every stage position in all 120 orders");

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
        std::array<std::uint64_t, 25> transition_totals{};
        for (std::size_t shift = 0; shift < domain_size; ++shift) {
            for (std::size_t i = 0; i < domain_size; ++i) {
                const Word result = domain_id((i + shift) % domain_size);
                ++transition_cases;
                ++transition_totals[expected_owner(source[i][0]) * 5u + expected_owner(result)];
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
        }
        require(source == initial, "stage composition mutated the immutable canonical source");
        require(transition_totals == std::array<std::uint64_t, 25>{
                    1681u, 246u, 738u, 82u, 697u,
                    246u, 36u, 108u, 12u, 102u,
                    738u, 108u, 324u, 36u, 306u,
                    82u, 12u, 36u, 4u, 34u,
                    697u, 102u, 306u, 34u, 289u},
                "all 25 source/result ownership transitions were not exhausted");
        require(ownership_cases == 65620u && transition_cases == 7056u &&
                    composition_cases == 60480u && mutable_source_cases == 846720u &&
                    duplicate_controls == 283320u,
                "exhaustive partition/composition coverage changed unexpectedly");
        std::printf("chemistry ownership: %llu assertions; %llu source IDs; "
                    "%llu source/result pairs; %llu compositions (all 120 stage orders, "
                    "two invocation orders, full/clipped/empty scopes); "
                    "%llu mutable-source controls (%llu duplicated-write witnesses). "
                    "Actual shared partition helper and synthetic CPU composition only; "
                    "not full shader numerical equivalence, GPU synchronization, "
                    "runtime acceptance, or JIT/performance proof.\n",
                    static_cast<unsigned long long>(assertions),
                    static_cast<unsigned long long>(ownership_cases),
                    static_cast<unsigned long long>(transition_cases),
                    static_cast<unsigned long long>(composition_cases),
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
