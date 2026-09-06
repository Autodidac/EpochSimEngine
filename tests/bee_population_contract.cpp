#include <sandhybrid/bee_colony.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
std::uint64_t checks{};
std::uint64_t failures{};

void check(const bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        if (failures <= 16u) std::cerr << message << '\n';
    }
}

struct Dimensions final {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
};

struct DecodedHome final {
    std::int64_t x;
    std::int64_t y;
    friend bool operator==(const DecodedHome&, const DecodedHome&) = default;
};

// Independent literal decoder for the pre-index GLSL beeOwnsHome domain.
// In short persistent worlds, historical GLSL district origins may be negative;
// do not silently substitute the CPU import decoder's clamped origins.
DecodedHome original_home(const std::uint32_t aux, const Dimensions world) {
    if (world.width >= 5120u && world.height >= 360u) {
        if ((aux & 0x00800000u) != 0u)
            return {static_cast<std::int64_t>(aux & 1023u) * 16,
                    static_cast<std::int64_t>((aux >> 10u) & 127u) * 16};
        constexpr std::array<std::int64_t, 8u> surface_rows{40, 37, 42, 17, 22, 42, 42, 41};
        const auto district = (aux >> 20u) & 7u;
        const auto gap = static_cast<std::int64_t>((world.width - 5120u) / 56u) * 8;
        const auto surface = world.height >= 1080u ? 1040 : 320;
        return {static_cast<std::int64_t>(district) * (640 + gap) +
                    static_cast<std::int64_t>(aux & 127u) * 8,
                surface - surface_rows[district] * 8 +
                    static_cast<std::int64_t>((aux >> 7u) & 63u) * 8};
    }
    const bool authored = (aux & 0x00400000u) != 0u;
    const auto origin_x = world.width >= 1920u ? 1280u :
        (world.width > 640u ? (world.width - 640u) / 2u : 0u);
    const auto origin_y = world.height >= 1080u ? 720u : 0u;
    return {static_cast<std::int64_t>(aux & 255u) * 4 + (authored ? origin_x : 0u),
            static_cast<std::int64_t>((aux >> 8u) & 127u) * 4 + (authored ? origin_y : 0u)};
}

bool interior(const DecodedHome home, const Dimensions world) {
    return home.x >= 0 && home.y >= 0 && home.x < world.width && home.y < world.height;
}

std::uint32_t actual_key(const DecodedHome home, const Dimensions world) {
    return sandhybrid::bee_population_key(
        {static_cast<std::uint32_t>(home.x), static_cast<std::uint32_t>(home.y)},
        world.width, world.height);
}

void sizing_contract() {
    constexpr std::array worlds{
        Dimensions{0u, 0u, 4u}, Dimensions{1u, 1u, 4u},
        Dimensions{640u, 360u, 4u}, Dimensions{641u, 361u, 4u},
        Dimensions{642u, 361u, 1u}, Dimensions{643u, 1079u, 1u},
        Dimensions{644u, 1080u, 2u}, Dimensions{645u, 1081u, 2u},
        Dimensions{646u, 1081u, 1u}, Dimensions{647u, 1081u, 1u},
        Dimensions{648u, 1081u, 4u}, Dimensions{1279u, 1440u, 1u},
        Dimensions{1280u, 1440u, 4u}, Dimensions{1919u, 1440u, 1u},
        Dimensions{1920u, 1440u, 4u}, Dimensions{5119u, 1440u, 4u},
        Dimensions{5120u, 359u, 4u}, Dimensions{5120u, 360u, 8u},
        Dimensions{5121u, 361u, 8u}, Dimensions{5120u, 1079u, 8u},
        Dimensions{5120u, 1080u, 8u}, Dimensions{5120u, 1440u, 8u},
        Dimensions{7680u, 1440u, 8u}, Dimensions{10240u, 1440u, 8u},
        Dimensions{16384u, 2048u, 8u},
    };
    for (const auto world : worlds) {
        check(sandhybrid::bee_population_stride(world.width, world.height) == world.stride,
              "population stride differs from independent dimension table");
        const auto columns = (static_cast<std::uint64_t>(world.width) + world.stride - 1u) / world.stride;
        const auto rows = (static_cast<std::uint64_t>(world.height) + world.stride - 1u) / world.stride;
        check(sandhybrid::bee_population_slot_count(world.width, world.height) == columns * rows,
              "population allocation does not use exact ceiling dimensions");
        for (std::uint32_t y = 0u; y < world.height; y += world.stride) {
            for (std::uint32_t x = 0u; x < world.width; x += world.stride) {
                const auto expected = (y / world.stride) * columns + x / world.stride;
                check(actual_key({x, y}, world) == expected,
                      "distinct lattice coordinates do not map injectively in row-major order");
            }
        }
        for (const auto outside : std::array<DecodedHome, 4u>{{
                 {-1, 0}, {0, -1}, {world.width, 0}, {0, world.height}}})
            check(actual_key(outside, world) == sandhybrid::bee_population_invalid_key,
                  "exterior home aliases an interior population slot");
        if (world.stride > 1u && world.width > 1u && world.height > 1u) {
            check(actual_key({1, 0}, world) == sandhybrid::bee_population_invalid_key,
                  "non-lattice X was floored onto another home");
            check(actual_key({0, 1}, world) == sandhybrid::bee_population_invalid_key,
                  "non-lattice Y was floored onto another home");
        }
    }
    check(sandhybrid::bee_population_slot_count(10240u, 1440u) == 230400u,
          "Large population buffer must contain 230400 words");
    check(sandhybrid::bee_population_slot_count(0u, 2048u) == 0u &&
          sandhybrid::bee_population_slot_count(16384u, 0u) == 0u,
          "zero-area population allocation is nonzero");
    constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
    constexpr std::uint64_t wide_axis = 536870912u;
    check(sandhybrid::bee_population_slot_count(maximum, maximum) == wide_axis * wide_axis,
          "population sizing overflowed before host allocation validation");
    check(sandhybrid::bee_population_slot_count(maximum, maximum) > maximum,
          "oversized population allocation cannot be rejected before narrowing");
    check(sandhybrid::bee_population_key({maximum - 7u, maximum - 7u}, maximum, maximum) ==
              sandhybrid::bee_population_invalid_key,
          "oversized key narrowed into a valid shader index");
}

void encoding_contract() {
    constexpr std::array worlds{
        Dimensions{640u, 360u, 4u}, Dimensions{642u, 1081u, 1u},
        Dimensions{644u, 1081u, 2u}, Dimensions{648u, 1081u, 4u},
        Dimensions{1919u, 1440u, 1u}, Dimensions{1920u, 1440u, 4u},
        Dimensions{5120u, 360u, 8u}, Dimensions{5121u, 361u, 8u},
        Dimensions{5120u, 1440u, 8u}, Dimensions{7680u, 1440u, 8u},
        Dimensions{10240u, 1440u, 8u}, Dimensions{16384u, 2048u, 8u},
    };
    for (const auto world : worlds) {
        const auto count = sandhybrid::bee_population_slot_count(world.width, world.height);
        std::vector<std::uint64_t> owners(static_cast<std::size_t>(count),
                                        (std::numeric_limits<std::uint64_t>::max)());
        const auto inspect = [&](const std::uint32_t aux) {
            const auto home = original_home(aux, world);
            const auto key = actual_key(home, world);
            if (!interior(home, world)) {
                check(key == sandhybrid::bee_population_invalid_key,
                      "invalid encoded home acquired a valid query key");
                return;
            }
            check(home.x % world.stride == 0 && home.y % world.stride == 0,
                  "admitted decoded home does not belong to the proposed lattice");
            check(key < count, "admitted decoded home escaped its population allocation");
            if (key >= count) return;
            const auto identity = (static_cast<std::uint64_t>(home.y) << 32u) |
                                  static_cast<std::uint64_t>(home.x);
            check(owners[key] == (std::numeric_limits<std::uint64_t>::max)() ||
                  owners[key] == identity, "unequal decoded homes collide in population key");
            owners[key] = identity;
            // Existing CPU import semantics agree on normal persistent and
            // all legacy canvases; the short-world GLSL distinction stays explicit.
            if (world.width < 5120u || world.height >= 1080u) {
                const auto cpu = sandhybrid::bee_home_from_metadata(aux, world.width, world.height);
                check(cpu.x == home.x && cpu.y == home.y,
                      "normal decoded-home oracle differs from production CPU metadata");
            }
        };
        if (world.width >= 5120u && world.height >= 360u) {
            for (std::uint32_t district = 0u; district < 8u; ++district)
                for (std::uint32_t y = 0u; y < 64u; ++y)
                    for (std::uint32_t x = 0u; x < 128u; ++x)
                        inspect(x | (y << 7u) | (district << 20u));
            for (std::uint32_t y = 0u; y < 128u; ++y)
                for (std::uint32_t x = 0u; x < 1024u; ++x)
                    inspect(0x00800000u | x | (y << 10u));
        } else {
            for (const auto authored : {0u, 0x00400000u})
                for (std::uint32_t y = 0u; y < 128u; ++y)
                    for (std::uint32_t x = 0u; x < 256u; ++x)
                        inspect(authored | x | (y << 8u));
        }
        // Slot values beyond59 still contribute to the old population scan.
        // Flags, slot, pollen, newborn age and migration never refine home equality.
        for (std::uint32_t flags = 0u; flags < 256u; ++flags) {
            for (std::uint32_t slot = 0u; slot < 128u; ++slot) {
                if (world.width >= 5120u && world.height >= 360u) {
                    inspect((flags << 24u) | (slot << 13u) | 40u | (12u << 7u));
                    inspect((flags << 24u) | 0x00800000u | ((slot & 63u) << 17u) |
                            20u | (51u << 10u));
                } else {
                    inspect((flags << 24u) | (slot << 15u) | 20u | (12u << 8u));
                    inspect((flags << 24u) | 0x00400000u | (slot << 15u) | 20u | (12u << 8u));
                }
            }
        }
    }
}

struct CanonicalCell final {
    std::uint32_t material;
    std::uint32_t aux;
};

struct CandidateTile final {
    std::uint32_t cached_count{};
    std::uint32_t flags{};
    // Unlisted canonical cells are Empty. Explicit records are distinct cell
    // owners, not a substitute payload or a cached per-home count.
    std::vector<CanonicalCell> cells;
};

bool candidate(const CandidateTile& tile) {
    return tile.cached_count != 0u || (tile.flags & 0x00001300u) != 0u;
}

std::uint32_t original_population(const std::vector<CandidateTile>& tiles,
                                  const Dimensions world, const DecodedHome query) {
    std::uint32_t count = 0u;
    for (const auto& tile : tiles) {
        if (!candidate(tile)) continue;
        for (const auto cell : tile.cells)
            if (cell.material == 18u && (cell.aux & 0x08000000u) != 0u &&
                original_home(cell.aux, world) == query) ++count;
    }
    return count;
}

std::vector<std::uint32_t> reduction(const std::vector<CandidateTile>& tiles,
                                   const Dimensions world, const std::uint32_t lanes,
                                   const bool reversed) {
    std::vector<std::uint32_t> counts(static_cast<std::size_t>(
        sandhybrid::bee_population_slot_count(world.width, world.height)));
    // Simulate independent tile-invocation scheduling and atomic integer adds.
    // This proves reduction/key equivalence, not GPU execution or visibility.
    for (std::uint32_t lane = 0u; lane < lanes; ++lane) {
        for (std::size_t offset = lane; offset < tiles.size(); offset += lanes) {
            const auto tile_index = reversed ? tiles.size() - 1u - offset : offset;
            const auto& tile = tiles[tile_index];
            if (!candidate(tile)) continue;
            for (auto cell = tile.cells.rbegin(); cell != tile.cells.rend(); ++cell) {
                if (cell->material != 18u || (cell->aux & 0x08000000u) == 0u) continue;
                const auto key = actual_key(original_home(cell->aux, world), world);
                if (key != sandhybrid::bee_population_invalid_key) ++counts[key];
            }
        }
    }
    return counts;
}

void reduction_contract() {
    constexpr Dimensions world{10240u, 1440u, 8u};
    std::vector<CandidateTile> tiles(230400u);
    constexpr std::uint32_t swarm = 0x08000000u;
    constexpr auto district_home = 40u | (12u << 7u); // Decodes to(320,816).
    constexpr auto global_alias = 0x00800000u | 20u | (51u << 10u);
    constexpr auto other_home = 0x00800000u | 400u | (70u << 10u);
    constexpr auto exterior_home = 0x00800000u | 1023u | (127u << 10u);
    // More than65535 nonempty OTHER-home candidate tiles before the query's
    // resident row expose both empty-scan and sparsity-dependent loop repairs.
    for (std::size_t tile = 0u; tile < 70000u; ++tile) {
        tiles[tile].cached_count = 1u;
        tiles[tile].cells.push_back({18u, swarm | other_home});
    }
    auto& colony = tiles[130600u];
    colony.cached_count = 0u;
    colony.flags = 0x00001000u; // Published candidate survives stale zero count.
    for (std::uint32_t slot = 0u; slot < 59u; ++slot)
        colony.cells.push_back({18u, swarm | district_home | (slot << 13u)});
    auto& distant = tiles.back();
    distant.flags = 0x00000100u;
    distant.cells.push_back({18u, 0xfa000000u | global_alias | (63u << 17u)});
    distant.cells.push_back({0u, swarm | district_home});
    distant.cells.push_back({18u, district_home}); // Not an owned swarm Bee.
    distant.cells.push_back({18u, swarm | exterior_home});
    auto& hive_candidate = tiles[180000u];
    hive_candidate.flags = 0x00000200u;
    hive_candidate.cells.push_back({18u, swarm | other_home});
    tiles[180001u].flags = 0x80000000u; // Unrelated flags are not a candidate.
    tiles[180001u].cells.push_back({18u, swarm | district_home});
    constexpr std::array queries{DecodedHome{320, 816}, DecodedHome{6400, 1120},
                                 DecodedHome{0, 0}, DecodedHome{10232, 1432}};
    check(original_population(tiles, world, queries[0]) == 60u,
          "off-camera alias/unknown-slot owner was not included in strict60 fixture");
    check(original_population(tiles, world, queries[1]) == 70001u,
          "dense other-home fixture does not exceed the historical loop budget");
    for (const auto lanes : {1u, 2u, 7u, 64u}) {
        for (const auto reversed : {false, true}) {
            const auto counts = reduction(tiles, world, lanes, reversed);
            for (const auto query : queries)
                check(counts[actual_key(query, world)] == original_population(tiles, world, query),
                      "per-home tile reduction differs from immutable whole-world source scan");
            check(counts[actual_key(queries[0], world)] >= 60u,
                  "per-home replacement cap lost the distant sixtieth owner");
        }
    }
    distant.cells.push_back({18u, swarm | district_home | (127u << 13u)});
    const auto overfull = reduction(tiles, world, 13u, true);
    check(overfull[actual_key(queries[0], world)] == 61u &&
          original_population(tiles, world, queries[0]) == 61u,
          "population reduction saturated, ignored malformed slots, or lost an over-cap owner");
    distant.cells.erase(distant.cells.begin());
    distant.cells.pop_back();
    const auto missing = reduction(tiles, world, 31u, false);
    check(missing[actual_key(queries[0], world)] == 59u &&
          original_population(tiles, world, queries[0]) == 59u,
          "59-owner replacement admission cannot distinguish a true missing owner");
}
} // namespace

int main() {
    sizing_contract();
    encoding_contract();
    reduction_contract();
    std::cout << checks << " bee population lattice/reduction assertions; " << failures
              << " failures (CPU equivalence, not GPU visibility or lifecycle acceptance)\n";
    return failures == 0u ? 0 : 1;
}
