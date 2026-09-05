#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
// Independent CPU model of DERIVED rain membership, not a replacement weather
// simulation or evidence of GPU/barrier correctness. No production helper is
// included: dense coordinate sets and cell scans supply independent oracles.
//
// Proposed std430 uint layout, for W columns and R = ceil(H / 32) row words:
//   [0, W)             exact live tagged-drop counts
//   [W, 2W)            summary bits for row words 0..31
//   [2W, 3W)           summary bits for row words 32..63
//   [3W, (3 + R)W)     interleaved rows: 3W + rowWord * W + column
//   [(3 + R)W, (4 + R)W) immutable admission counts copied before producers
// A set row bit has exactly one canonical Water/Dirty Water owner. Summaries
// may retain stale set bits, but must NEVER omit a nonempty row word. Two
// summary uints support H <= 2048; a taller world requires a new checked layout,
// not a shift >= 32 or silent truncation. Large 10240x1440 uses 2,007,040 bytes.
// The appended admission plane preserves every prior row/summary address.
// Scheduled emission requires an empty pre-producer snapshot AND a successful
// live-count CAS from zero to one. A same-pass phase removal must not open a
// previously occupied column to emission merely by reaching its atomic first.
//
// Registration/removal model atomicOr/atomicAnd's returned-old-bit semantics;
// count changes only when the bit actually changes. Parallel removers leave
// summary bits set. Only an exclusive column owner may prune stale summaries
// after prior writers are visible. Invariants are checked at completed
// transaction/pass boundaries, not between the component atomic operations.

std::uint64_t checks = 0u;

void require(const bool condition, const char* const message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Layout {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t row_words;
    std::size_t words;
    std::size_t bytes;

    static std::optional<Layout> checked(
        const std::uint64_t width, const std::uint64_t height,
        const std::uint64_t byte_limit = std::numeric_limits<std::size_t>::max()) {
        constexpr auto uint_limit = std::numeric_limits<std::uint32_t>::max();
        constexpr auto cell_limit = std::numeric_limits<std::int32_t>::max();
        if (width == 0u || height == 0u || width > uint_limit || height > 2048u)
            return std::nullopt;
        // Division/remainder avoids overflow from height + 31. Validate both
        // canonical cell addressing and every derived uint index before cast.
        const auto row_words = height / 32u + (height % 32u != 0u ? 1u : 0u);
        if (width > static_cast<std::uint64_t>(cell_limit) / height ||
            width > uint_limit / (4u + row_words))
            return std::nullopt;
        const auto words = width * (4u + row_words);
        const auto max_bytes = (std::min)(byte_limit,
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
        if (words > max_bytes / sizeof(std::uint32_t)) return std::nullopt;
        return Layout{static_cast<std::uint32_t>(width),
                      static_cast<std::uint32_t>(height),
                      static_cast<std::uint32_t>(row_words),
                      static_cast<std::size_t>(words),
                      static_cast<std::size_t>(words * sizeof(std::uint32_t))};
    }

    std::size_t summary(const std::uint32_t x, const std::uint32_t half) const {
        if (x >= width || half >= 2u) throw std::out_of_range("summary address");
        return static_cast<std::size_t>(half + 1u) * width + x;
    }

    std::size_t row(const std::uint32_t x, const std::uint32_t word) const {
        if (x >= width || word >= row_words) throw std::out_of_range("row address");
        return static_cast<std::size_t>(3u + word) * width + x;
    }

    std::size_t snapshot(const std::uint32_t x) const {
        if (x >= width) throw std::out_of_range("snapshot address");
        return words - width + x;
    }
};

struct Index {
    Layout layout;
    std::vector<std::uint32_t> data;

    explicit Index(const Layout value) : layout(value), data(value.words, 0u) {}

    bool valid(const std::uint32_t x, const std::uint32_t y) const {
        return x < layout.width && y < layout.height;
    }

    bool contains(const std::uint32_t x, const std::uint32_t y) const {
        return valid(x, y) &&
            (data[layout.row(x, y / 32u)] & (1u << (y % 32u))) != 0u;
    }

    bool add(const std::uint32_t x, const std::uint32_t y) {
        if (!valid(x, y)) return false;
        auto& row = data[layout.row(x, y / 32u)];
        const auto mask = 1u << (y % 32u);
        const auto old = row; // atomicOr returns the value before this write.
        row |= mask;
        if ((old & mask) == 0u) ++data[x];
        data[layout.summary(x, y / 1024u)] |= 1u << ((y / 32u) % 32u);
        return (old & mask) == 0u;
    }

    bool remove(const std::uint32_t x, const std::uint32_t y) {
        if (!valid(x, y)) return false;
        auto& row = data[layout.row(x, y / 32u)];
        const auto mask = 1u << (y % 32u);
        const auto old = row; // atomicAnd returns the value before this write.
        row &= ~mask;
        if ((old & mask) != 0u) --data[x];
        // Clearing the summary here could hide a simultaneous same-word add.
        return (old & mask) != 0u;
    }

    void snapshot_admission_counts() {
        // A completed producer pass must precede this exclusive copy. The
        // copied plane is then immutable until all next-pass producers end.
        for (std::uint32_t x = 0u; x < layout.width; ++x)
            data[layout.snapshot(x)] = data[x];
    }

    void prune_exclusive(const std::uint32_t x) {
        // Precondition: all producers have completed and their row/count/
        // summary writes are visible through the production producer barrier.
        // No registration/removal may overlap this exclusive column pass.
        // These tests deliberately do not claim concurrent-prune support.
        std::array<std::uint32_t, 2> exact{};
        for (std::uint32_t word = 0u; word < layout.row_words; ++word)
            if (data[layout.row(x, word)] != 0u)
                exact[word / 32u] |= 1u << (word % 32u);
        for (std::uint32_t half = 0u; half < 2u; ++half)
            data[layout.summary(x, half)] = exact[half];
    }

    template<class Function>
    void original_bottom_up(const std::uint32_t x, Function visit) {
        if (x >= layout.width || data[x] == 0u) return;
        // Freeze both summary words, then freeze each row word before visiting
        // its bits. Downward insertions target an already visited higher word
        // or an already copied row word, so a moved owner is never revisited.
        const std::array<std::uint32_t, 2> summaries{
            data[layout.summary(x, 0u)], data[layout.summary(x, 1u)]};
        for (int half = 1; half >= 0; --half) {
            auto words = summaries[static_cast<std::size_t>(half)];
            while (words != 0u) {
                const auto bit = 31u - static_cast<std::uint32_t>(std::countl_zero(words));
                words &= ~(1u << bit);
                const auto word = static_cast<std::uint32_t>(half) * 32u + bit;
                if (word >= layout.row_words) continue; // Conservative stale summary.
                auto original = data[layout.row(x, word)];
                while (original != 0u) {
                    const auto row_bit = 31u -
                        static_cast<std::uint32_t>(std::countl_zero(original));
                    original &= ~(1u << row_bit);
                    const auto y = word * 32u + row_bit;
                    if (y < layout.height) visit(y);
                }
            }
        }
        prune_exclusive(x);
    }
};

using Coordinate = std::pair<std::uint32_t, std::uint32_t>;
using DenseSet = std::set<Coordinate>;

void compare_index(const Index& index, const DenseSet& expected) {
    const auto& layout = index.layout;
    for (std::uint32_t x = 0u; x < layout.width; ++x) {
        std::uint32_t expected_count = 0u;
        std::uint32_t bit_count = 0u;
        for (std::uint32_t y = 0u; y < layout.height; ++y) {
            const bool present = expected.contains({x, y});
            expected_count += present ? 1u : 0u;
            require(index.contains(x, y) == present, "row bit differs from dense membership");
        }
        for (std::uint32_t word = 0u; word < layout.row_words; ++word) {
            const auto bits = index.data[layout.row(x, word)];
            bit_count += static_cast<std::uint32_t>(std::popcount(bits));
            const auto summary = index.data[layout.summary(x, word / 32u)];
            require(bits == 0u || (summary & (1u << (word % 32u))) != 0u,
                    "nonempty row word omitted by summary");
            if (word + 1u == layout.row_words && layout.height % 32u != 0u)
                require((bits >> (layout.height % 32u)) == 0u, "padding row bit set");
        }
        require(bit_count == expected_count && index.data[x] == expected_count,
                "live count differs from row popcount or dense membership");
    }
}

std::uint32_t random_word(std::uint32_t& state) {
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

void allocation_contract() {
    // Golden addresses are written out, not derived using the allocator or
    // the same interleaving formula. They reject a mutually wrong allocator
    // and address helper even if their sizes still agree with one another.
    const auto golden = *Layout::checked(5u, 65u);
    require(golden.words == 35u && golden.bytes == 140u &&
            golden.summary(0u, 0u) == 5u && golden.summary(4u, 0u) == 9u &&
            golden.summary(0u, 1u) == 10u && golden.summary(4u, 1u) == 14u &&
            golden.row(0u, 0u) == 15u && golden.row(4u, 0u) == 19u &&
            golden.row(0u, 1u) == 20u && golden.row(4u, 1u) == 24u &&
            golden.row(0u, 2u) == 25u && golden.row(4u, 2u) == 29u &&
            golden.snapshot(0u) == 30u && golden.snapshot(4u) == 34u,
            "golden small-world layout addresses differ");
    Index literal_bits(golden);
    for (const auto y : {31u, 32u, 63u, 64u}) (void)literal_bits.add(4u, y);
    literal_bits.snapshot_admission_counts();
    require(literal_bits.data[4u] == 4u && literal_bits.data[9u] == 7u &&
            literal_bits.data[14u] == 0u && literal_bits.data[19u] == 0x80000000u &&
            literal_bits.data[24u] == 0x80000001u && literal_bits.data[29u] == 1u &&
            literal_bits.data[30u] == 0u && literal_bits.data[34u] == 4u,
            "golden row31/32/63/64 packed words differ");
    Index tall(*Layout::checked(3u, 2048u));
    for (const auto y : {1023u, 1024u, 2047u}) (void)tall.add(2u, y);
    tall.snapshot_admission_counts();
    require(tall.layout.words == 204u && tall.layout.bytes == 816u &&
            tall.data[2u] == 3u && tall.data[5u] == 0x80000000u &&
            tall.data[8u] == 0x80000001u && tall.data[104u] == 0x80000000u &&
            tall.data[107u] == 1u && tall.data[200u] == 0x80000000u &&
            tall.layout.snapshot(0u) == 201u && tall.layout.snapshot(2u) == 203u &&
            tall.data[201u] == 0u && tall.data[203u] == 3u,
            "golden second-summary and final-row addresses differ");

    for (const auto dimensions : std::array<std::array<std::uint32_t, 2>, 9>{{
             {1u, 1u}, {3u, 31u}, {4u, 32u}, {7u, 33u}, {4u, 63u},
             {4u, 64u}, {5u, 65u}, {3u, 1440u}, {2u, 2048u}}}) {
        const auto layout = Layout::checked(dimensions[0], dimensions[1]);
        require(layout.has_value(), "valid layout rejected");
        // Build each row-word address by visiting real cells, independently of
        // ceil(H/32) and the allocator's multiplication formula.
        std::set<std::size_t> addresses;
        for (std::uint32_t x = 0u; x < layout->width; ++x) {
            addresses.insert(x);
            addresses.insert(layout->summary(x, 0u));
            addresses.insert(layout->summary(x, 1u));
            addresses.insert(layout->snapshot(x));
            for (std::uint32_t y = 0u; y < layout->height; ++y) {
                const auto address = layout->row(x, y / 32u);
                require(address >= 3u * layout->width &&
                        address < layout->words - layout->width,
                        "row word aliases metadata or escapes allocation");
                addresses.insert(address);
            }
        }
        require(addresses.size() == layout->words && *addresses.begin() == 0u &&
                *addresses.rbegin() + 1u == layout->words, "layout has alias, gap or wrong extent");
        require(layout->bytes == addresses.size() * sizeof(std::uint32_t),
                "byte allocation does not cover exact addresses");
        require(Layout::checked(layout->width, layout->height, layout->bytes).has_value(),
                "exact allocation limit rejected");
        require(!Layout::checked(layout->width, layout->height, layout->bytes - 1u),
                "allocation accepted one byte beyond limit");
        bool row_rejected = false;
        bool column_rejected = false;
        bool summary_rejected = false;
        bool snapshot_rejected = false;
        try { (void)layout->row(0u, layout->row_words); }
        catch (const std::out_of_range&) { row_rejected = true; }
        try { (void)layout->row(layout->width, 0u); }
        catch (const std::out_of_range&) { column_rejected = true; }
        try { (void)layout->summary(0u, 2u); }
        catch (const std::out_of_range&) { summary_rejected = true; }
        try { (void)layout->snapshot(layout->width); }
        catch (const std::out_of_range&) { snapshot_rejected = true; }
        require(row_rejected && column_rejected && summary_rejected && snapshot_rejected,
                "out-of-range derived address accepted");
    }

    for (const auto width : {5120u, 7680u, 10240u}) {
        const auto layout = Layout::checked(width, 1440u);
        require(layout && layout->row_words == 45u, "current preset rejected");
        require(layout->bytes == static_cast<std::size_t>(width) * 196u,
                "current preset byte count incorrect");
    }
    require(Layout::checked(10240u, 1440u)->bytes == 2007040u,
            "Large footprint witness differs");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const auto dimensions : std::array<std::array<std::uint64_t, 2>, 8>{{
             {0u, 1u}, {1u, 0u}, {0u, 0u}, {1u, 2049u},
             {1u, maximum}, {maximum, 1u}, {maximum, maximum},
             {std::numeric_limits<std::uint32_t>::max(), 2048u}}})
        require(!Layout::checked(dimensions[0], dimensions[1]), "invalid/overflow layout accepted");
    // Exercise a 32-bit byte-address ceiling even when the host is 64-bit.
    constexpr std::uint64_t narrow_limit = 0xffffffffu;
    const auto fitting_width = narrow_limit / (5u * 4u);
    require(Layout::checked(fitting_width, 1u, narrow_limit).has_value(),
            "largest narrow byte allocation rejected");
    require(!Layout::checked(fitting_width + 1u, 1u, narrow_limit),
            "narrow byte allocation wrapped");
    const auto canonical_limit = std::numeric_limits<std::int32_t>::max() / 2048u;
    require(Layout::checked(canonical_limit, 2048u).has_value(),
            "largest canonical cell-address width rejected");
    require(!Layout::checked(static_cast<std::uint64_t>(canonical_limit) + 1u, 2048u),
            "canonical cell index multiplication overflow accepted");
}

void membership_contract() {
    Index index(*Layout::checked(4u, 2048u));
    DenseSet expected;
    constexpr std::array<std::uint32_t, 13> rows{
        0u, 1u, 30u, 31u, 32u, 33u, 63u, 64u, 1023u, 1024u, 2015u, 2016u, 2047u};
    for (const auto x : {0u, 1u, 3u}) {
        for (const auto y : rows) {
            require(index.add(x, y), "fresh registration failed");
            expected.insert({x, y});
            const auto once = index.data;
            require(!index.add(x, y) && once == index.data, "duplicate registration changed index");
        }
    }
    compare_index(index, expected);
    const auto valid = index.data;
    for (const auto& invalid : std::array<Coordinate, 4>{{
             {4u, 0u}, {0u, 2048u}, {0xffffffffu, 0u}, {0u, 0xffffffffu}}}) {
        require(!index.add(invalid.first, invalid.second) &&
                !index.remove(invalid.first, invalid.second) &&
                !index.contains(invalid.first, invalid.second) && index.data == valid,
                "out-of-bounds membership touched storage");
    }
    for (const auto y : rows) {
        require(index.remove(1u, y), "existing membership removal failed");
        expected.erase({1u, y});
        const auto once = index.data;
        require(!index.remove(1u, y) && once == index.data, "duplicate removal underflowed count");
    }
    require(index.data[1u] == 0u && index.data[index.layout.summary(1u, 0u)] != 0u,
            "removal did not retain permitted conservative summary");
    compare_index(index, expected);
    index.prune_exclusive(1u);
    require(index.data[index.layout.summary(1u, 0u)] == 0u &&
            index.data[index.layout.summary(1u, 1u)] == 0u, "exclusive pruning failed");

    // Both atomic-equivalent orders of removing one bit and adding another in
    // the same word must leave the newcomer discoverable. Duplicate adds from
    // a load rebuild/correction pass must never create another counted owner.
    for (const bool remove_first : {false, true}) {
        Index race(*Layout::checked(2u, 65u));
        (void)race.add(1u, 31u);
        if (remove_first) (void)race.remove(1u, 31u);
        (void)race.add(1u, 30u);
        (void)race.add(1u, 30u);
        if (!remove_first) (void)race.remove(1u, 31u);
        compare_index(race, {{1u, 30u}});
    }

    Index random(*Layout::checked(7u, 97u));
    DenseSet oracle;
    std::uint32_t seed = 0x71a19e23u;
    for (std::uint32_t operation = 0u; operation < 8192u; ++operation) {
        const auto x = random_word(seed) % random.layout.width;
        const auto y = random_word(seed) % random.layout.height;
        if ((random_word(seed) & 1u) != 0u) {
            const auto inserted = oracle.insert({x, y}).second;
            require(random.add(x, y) == inserted, "random add result differs from set");
        } else {
            const auto removed = oracle.erase({x, y}) != 0u;
            require(random.remove(x, y) == removed, "random remove result differs from set");
        }
        if (operation % 17u == 0u) random.prune_exclusive(x);
        compare_index(random, oracle);
    }

    Index rebuilt(random.layout);
    // Reverse scan order and re-register the complete set twice: the result
    // must be independent of CAS winner/order, unlike the old one-Y tracker.
    for (auto it = oracle.rbegin(); it != oracle.rend(); ++it)
        (void)rebuilt.add(it->first, it->second);
    const auto first = rebuilt.data;
    for (const auto& coordinate : oracle) (void)rebuilt.add(coordinate.first, coordinate.second);
    require(rebuilt.data == first, "repeated rebuild inflated membership");
    compare_index(rebuilt, oracle);
    std::fill(rebuilt.data.begin(), rebuilt.data.end(), 0u);
    compare_index(rebuilt, {});
}

struct AtomicOperation {
    bool addition;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t stage = 0u;
    bool changed = false;

    bool complete() const { return stage == (addition ? 3u : 2u); }
};

struct ScheduleState {
    Index index;
    DenseSet oracle;
    std::vector<AtomicOperation> operations;
};

struct ScheduleResult {
    std::uint64_t completed = 0u;
    std::uint32_t final_presence = 0u;
};

ScheduleResult atomic_schedules(const Layout layout, const DenseSet& initial,
                                const std::vector<AtomicOperation>& operations,
                                const Coordinate witness) {
    ScheduleState start{Index(layout), initial, operations};
    for (const auto& coordinate : initial)
        (void)start.index.add(coordinate.first, coordinate.second);
    ScheduleResult result;
    const auto enumerate = [&](auto&& self, const ScheduleState& current) -> void {
        bool finished = true;
        for (std::size_t choice = 0u; choice < current.operations.size(); ++choice) {
            if (current.operations[choice].complete()) continue;
            finished = false;
            auto next = current;
            auto& operation = next.operations[choice];
            auto& storage = next.index.data;
            const auto x = operation.x;
            const auto y = operation.y;
            if (operation.stage == 0u) {
                auto& row = storage[layout.row(x, y / 32u)];
                const auto mask = 1u << (y % 32u);
                const auto old = row;
                // One linearized row atomic, separately scheduled from its
                // count and summary writes. The set oracle records only this
                // membership linearization; it has no counters or summaries.
                if (operation.addition) {
                    row |= mask;
                    operation.changed = (old & mask) == 0u;
                    next.oracle.insert({x, y});
                } else {
                    row &= ~mask;
                    operation.changed = (old & mask) != 0u;
                    next.oracle.erase({x, y});
                }
            } else if (operation.stage == 1u) {
                if (operation.changed) {
                    // Unsigned atomic-add arithmetic may transiently wrap if
                    // a remove's count update precedes an earlier add's count
                    // update. Do not inspect/accept/prune incomplete passes.
                    if (operation.addition) ++storage[x];
                    else --storage[x];
                }
            } else {
                storage[layout.summary(x, y / 1024u)] |= 1u << ((y / 32u) % 32u);
            }
            ++operation.stage;
            self(self, next);
        }
        if (finished) {
            // This is the modeled producer barrier: every row/count/summary
            // event is complete. Only now assert count=popcount and absence
            // of summary false negatives. False positives remain admissible.
            compare_index(current.index, current.oracle);
            result.final_presence |= current.oracle.contains(witness) ? 2u : 1u;
            ++result.completed;
        }
    };
    enumerate(enumerate, start);
    require(result.completed != 0u, "atomic schedule enumeration was empty");
    return result;
}

void atomic_event_contract() {
    const auto layout = *Layout::checked(3u, 65u);
    const auto duplicate_add = atomic_schedules(layout, {},
        {{true, 1u, 31u}, {true, 1u, 31u}, {true, 1u, 31u}}, {1u, 31u});
    require(duplicate_add.completed == 1680u && duplicate_add.final_presence == 2u,
            "duplicate atomic adds missed schedules or counted another owner");
    const auto duplicate_remove = atomic_schedules(layout, {{1u, 31u}},
        {{false, 1u, 31u}, {false, 1u, 31u}, {false, 1u, 31u}}, {1u, 31u});
    require(duplicate_remove.completed == 90u && duplicate_remove.final_presence == 1u,
            "duplicate atomic removals missed schedules or retained owner");
    // Both final memberships are legal depending on the last ROW atomic;
    // count/summary event ordering must not change that linearized result.
    const auto mixed = atomic_schedules(layout, {},
        {{true, 1u, 31u}, {true, 1u, 31u}, {false, 1u, 31u}}, {1u, 31u});
    require(mixed.completed == 560u && mixed.final_presence == 3u,
            "same-bit add/remove interleavings missed a legal final membership");
    const auto replacement = atomic_schedules(layout, {{1u, 31u}},
        {{false, 1u, 31u}, {false, 1u, 31u}, {true, 1u, 31u}}, {1u, 31u});
    require(replacement.completed == 210u && replacement.final_presence == 3u,
            "same-bit removal/replacement interleavings differ");
    const auto same_word = atomic_schedules(layout, {{1u, 31u}},
        {{false, 1u, 31u}, {true, 1u, 30u}, {true, 1u, 30u}}, {1u, 30u});
    require(same_word.completed == 560u && same_word.final_presence == 2u,
            "same-word removal hid duplicate registered neighbor");
    const auto different_words = atomic_schedules(layout, {{1u, 0u}},
        {{true, 1u, 31u}, {true, 1u, 32u}, {false, 1u, 0u}}, {1u, 32u});
    require(different_words.completed == 560u && different_words.final_presence == 2u,
            "row-word boundary atomic schedules differ");
    const auto different_columns = atomic_schedules(layout, {{1u, 31u}},
        {{true, 0u, 31u}, {true, 2u, 31u}, {false, 1u, 31u}}, {2u, 31u});
    require(different_columns.completed == 560u && different_columns.final_presence == 2u,
            "interleaved columns share a membership counter or word");
    const auto summary_boundary = atomic_schedules(*Layout::checked(1u, 1025u), {{0u, 0u}},
        {{true, 0u, 1023u}, {true, 0u, 1024u}, {false, 0u, 0u}}, {0u, 1024u});
    require(summary_boundary.completed == 560u && summary_boundary.final_presence == 2u,
            "second-summary atomic schedules differ");
}

struct AdmissionOperation {
    bool admission;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t stage = 0u;
    bool accepted = false;
    bool removed = false;

    bool complete() const { return stage == (admission ? 3u : 2u); }
};

struct AdmissionState {
    Index index;
    DenseSet oracle;
    std::vector<std::uint32_t> frozen_counts;
    std::vector<AdmissionOperation> operations;
};

AdmissionState admission_start(const Layout layout, const DenseSet& initial,
                               const std::vector<AdmissionOperation>& operations) {
    AdmissionState result{Index(layout), initial,
        std::vector<std::uint32_t>(layout.width, 0u), operations};
    for (const auto& coordinate : initial) {
        (void)result.index.add(coordinate.first, coordinate.second);
        ++result.frozen_counts.at(coordinate.first); // Independent set-derived counts.
    }
    const auto before = result.index.data;
    result.index.snapshot_admission_counts();
    require(std::equal(before.begin(), before.end() - layout.width,
                       result.index.data.begin()),
            "admission snapshot copy changed live counts, summaries or row words");
    for (const auto& operation : operations)
        require(result.index.valid(operation.x, operation.y),
                "admission schedule contains an invalid coordinate");
    return result;
}

void admission_event(AdmissionState& state, const std::size_t choice) {
    auto& operation = state.operations.at(choice);
    require(!operation.complete(), "completed admission event was repeated");
    auto& index = state.index;
    const auto x = operation.x;
    const auto y = operation.y;
    const auto mask = 1u << (y % 32u);
    if (operation.admission) {
        if (operation.stage == 0u) {
            // Immutable snapshot read followed by the live atomicCompSwap.
            // Their combination is one modeled event because no producer may
            // write the snapshot. A rejected candidate never reserves a count.
            if (index.data[index.layout.snapshot(x)] == 0u && index.data[x] == 0u) {
                index.data[x] = 1u;
                operation.accepted = true;
            }
        } else if (operation.accepted && operation.stage == 1u) {
            index.data[index.layout.row(x, y / 32u)] |= mask;
            state.oracle.insert({x, y});
            // The successful count CAS already paid for this new membership.
            // Calling ordinary add here would double-count the admitted drop.
        } else if (operation.accepted && operation.stage == 2u) {
            index.data[index.layout.summary(x, y / 1024u)] |=
                1u << ((y / 32u) % 32u);
        }
    } else if (operation.stage == 0u) {
        auto& row = index.data[index.layout.row(x, y / 32u)];
        operation.removed = (row & mask) != 0u;
        row &= ~mask;
        state.oracle.erase({x, y});
    } else if (operation.removed) {
        --index.data[x];
    }
    ++operation.stage;
}

void compare_admission_pass(const AdmissionState& state) {
    // Only completed producer passes are consumed. A successful admission CAS
    // transiently reserves a count before its row/summary writes; that is not
    // a count=popcount boundary and must not overlap pruning or rain iteration.
    for (const auto& operation : state.operations)
        require(operation.complete(), "incomplete admission pass was consumed");
    compare_index(state.index, state.oracle);
    std::vector<std::uint32_t> admissions(state.index.layout.width, 0u);
    for (const auto& operation : state.operations)
        if (operation.accepted) ++admissions[operation.x];
    for (std::uint32_t x = 0u; x < state.index.layout.width; ++x) {
        require(state.index.data[state.index.layout.snapshot(x)] == state.frozen_counts[x],
                "producer changed immutable pre-pass admission count");
        require(admissions[x] <= 1u &&
                (state.frozen_counts[x] == 0u || admissions[x] == 0u),
                "admission ignored preoccupied snapshot or admitted two column owners");
    }
}

struct AdmissionResult {
    std::uint64_t completed = 0u;
    std::uint32_t minimum_admissions = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t maximum_admissions = 0u;
    DenseSet winners;
};

AdmissionResult admission_schedules(const AdmissionState& start) {
    AdmissionResult result;
    const auto enumerate = [&](auto&& self, const AdmissionState& current) -> void {
        bool finished = true;
        for (std::size_t choice = 0u; choice < current.operations.size(); ++choice) {
            if (current.operations[choice].complete()) continue;
            finished = false;
            auto next = current;
            admission_event(next, choice);
            self(self, next);
        }
        if (finished) {
            compare_admission_pass(current);
            std::uint32_t admitted = 0u;
            for (const auto& operation : current.operations) {
                if (!operation.accepted) continue;
                ++admitted;
                result.winners.insert({operation.x, operation.y});
            }
            result.minimum_admissions = (std::min)(result.minimum_admissions, admitted);
            result.maximum_admissions = (std::max)(result.maximum_admissions, admitted);
            ++result.completed;
        }
    };
    enumerate(enumerate, start);
    return result;
}

void admission_snapshot_contract() {
    const auto layout = *Layout::checked(3u, 65u);
    const auto occupied = admission_start(layout, {{1u, 31u}},
        {{false, 1u, 31u}, {true, 1u, 64u}});
    // Explicit complete removal->admission and admission->removal witnesses:
    // both must reject emission even though the first order reaches live zero.
    for (const bool removal_first : {false, true}) {
        auto ordered = occupied;
        const std::array<std::size_t, 2> order = removal_first
            ? std::array<std::size_t, 2>{0u, 1u} : std::array<std::size_t, 2>{1u, 0u};
        for (const auto choice : order)
            while (!ordered.operations[choice].complete()) admission_event(ordered, choice);
        compare_admission_pass(ordered);
        require(ordered.oracle.empty() && ordered.index.data[1u] == 0u &&
                !ordered.operations[1u].accepted &&
                ordered.index.data[ordered.index.layout.snapshot(1u)] == 1u,
                "tagged removal/admission ordering changed scheduled emission");
    }
    const auto occupied_schedules = admission_schedules(occupied);
    require(occupied_schedules.completed == 10u &&
            occupied_schedules.minimum_admissions == 0u &&
            occupied_schedules.maximum_admissions == 0u,
            "preoccupied column admitted emission in a primitive-event interleaving");

    // The same gate must not permanently close an emptied column: only the
    // next exclusive snapshot may expose it to scheduled admission again.
    auto next_pass = occupied;
    for (std::size_t choice = 0u; choice < next_pass.operations.size(); ++choice)
        while (!next_pass.operations[choice].complete()) admission_event(next_pass, choice);
    next_pass.index.snapshot_admission_counts();
    next_pass.frozen_counts.assign(layout.width, 0u);
    next_pass.operations = {{true, 1u, 64u}};
    while (!next_pass.operations[0u].complete()) admission_event(next_pass, 0u);
    compare_admission_pass(next_pass);
    require(next_pass.operations[0u].accepted && next_pass.index.data[1u] == 1u &&
            next_pass.oracle == DenseSet{{1u, 64u}},
            "next-pass empty snapshot did not admit exactly one new drop");

    const auto competing = admission_schedules(admission_start(layout, {},
        {{true, 1u, 31u}, {true, 1u, 32u}, {true, 1u, 64u}}));
    require(competing.completed == 1680u && competing.minimum_admissions == 1u &&
            competing.maximum_admissions == 1u && competing.winners.size() == 3u,
            "empty-column candidates lost a legal winner or double-counted admission");
    const auto duplicate = admission_schedules(admission_start(layout, {},
        {{true, 1u, 31u}, {true, 1u, 31u}, {true, 1u, 31u}}));
    require(duplicate.completed == 1680u && duplicate.minimum_admissions == 1u &&
            duplicate.maximum_admissions == 1u && duplicate.winners == DenseSet{{1u, 31u}},
            "duplicate empty-column emission candidates created two counted owners");
}

enum class Material : std::uint32_t { vacuum, gas, water, dirty_water, cloud, stone };
constexpr std::uint32_t rain_flag = 0x10000000u;
constexpr std::uint32_t moved_flag = 0x01000000u;

struct Cell {
    Material material = Material::vacuum;
    std::uint32_t age = 0u;
    std::int32_t temperature = 0;
    std::uint32_t aux = 0u;
    bool operator==(const Cell&) const = default;
};

bool tracked(const Cell cell) {
    return (cell.material == Material::water || cell.material == Material::dirty_water) &&
           (cell.aux & rain_flag) != 0u;
}

struct World {
    std::uint32_t width;
    std::uint32_t height;
    std::vector<Cell> cells;

    World(const std::uint32_t w, const std::uint32_t h)
        : width(w), height(h), cells(static_cast<std::size_t>(w) * h) {}
    Cell& at(const std::uint32_t x, const std::uint32_t y) {
        return cells.at(static_cast<std::size_t>(y) * width + x);
    }
    const Cell& at(const std::uint32_t x, const std::uint32_t y) const {
        return cells.at(static_cast<std::size_t>(y) * width + x);
    }
};

DenseSet scan_tags(const World& world) {
    DenseSet result;
    for (std::uint32_t y = 0u; y < world.height; ++y)
        for (std::uint32_t x = 0u; x < world.width; ++x)
            if (tracked(world.at(x, y))) result.insert({x, y});
    return result;
}

std::array<std::uint32_t, 6> material_counts(const World& world) {
    std::array<std::uint32_t, 6> result{};
    for (const auto& cell : world.cells) ++result[static_cast<std::size_t>(cell.material)];
    return result;
}

Index rebuild(const World& world) {
    Index index(*Layout::checked(world.width, world.height));
    for (const auto& coordinate : scan_tags(world))
        (void)index.add(coordinate.first, coordinate.second);
    return index;
}

void indexed_step(World& world, Index& index, const std::uint32_t tick,
                  const bool paused = false) {
    if (paused) return;
    for (std::uint32_t x = 0u; x < world.width; ++x) {
        if (((tick + x) & 3u) != 0u) continue;
        index.original_bottom_up(x, [&](const std::uint32_t y) {
            auto& source = world.at(x, y);
            if (!tracked(source)) {
                (void)index.remove(x, y);
                return;
            }
            if (y + 1u < world.height) {
                auto& target = world.at(x, y + 1u);
                if (target.material == Material::gas || target.material == Material::vacuum) {
                    auto drop = source;
                    ++drop.age;
                    drop.aux |= rain_flag | moved_flag;
                    source = target;
                    target = drop;
                    (void)index.remove(x, y);
                    (void)index.add(x, y + 1u);
                    return;
                }
            }
            source.aux = (source.aux & ~rain_flag) | moved_flag;
            (void)index.remove(x, y);
        });
    }
}

void dense_step(World& world, const std::uint32_t tick) {
    // Independent row-major original-set oracle. It intentionally ignores the
    // count, summaries and bitset completely, and never sees newly tagged rows.
    const auto before = world.cells;
    for (std::uint32_t row = world.height; row > 0u; --row) {
        const auto y = row - 1u;
        for (std::uint32_t x = 0u; x < world.width; ++x) {
            const auto source = static_cast<std::size_t>(y) * world.width + x;
            if (!tracked(before[source]) || tick % 4u != (4u - x % 4u) % 4u) continue;
            const auto next = source + world.width;
            if (y + 1u == world.height ||
                (world.cells[next].material != Material::vacuum &&
                 world.cells[next].material != Material::gas)) {
                world.cells[source].aux &= ~rain_flag;
                world.cells[source].aux |= moved_flag;
            } else {
                std::swap(world.cells[source], world.cells[next]);
                world.cells[next].age += 1u;
                world.cells[next].aux |= moved_flag;
            }
        }
    }
}

bool reconcile_edited_membership(const World& world, Index& index,
                                 const std::vector<Coordinate>& changed) {
    // A completed editor/actor/Blueprint transaction supplies its exact changed
    // endpoints. This pass reads canonical cells and changes DERIVED data only;
    // it does not advance rain, simulation age, or any world clock while paused.
    if (world.width != index.layout.width || world.height != index.layout.height)
        return false;
    for (const auto& coordinate : changed)
        if (!index.valid(coordinate.first, coordinate.second)) return false;
    for (const auto& coordinate : changed) {
        if (tracked(world.at(coordinate.first, coordinate.second)))
            (void)index.add(coordinate.first, coordinate.second);
        else
            (void)index.remove(coordinate.first, coordinate.second);
    }
    // No prune: producers may conservatively OR summaries for other endpoints.
    return true;
}

void stale_membership_contract() {
    World edited(4u, 65u);
    for (std::uint32_t y = 0u; y < edited.height; ++y)
        for (std::uint32_t x = 0u; x < edited.width; ++x) {
            const auto identity = y * edited.width + x;
            edited.at(x, y) = {Material::gas, identity + 10u,
                static_cast<std::int32_t>(identity) - 80, identity};
        }
    for (const auto y : {20u, 30u, 31u, 63u})
        edited.at(1u, y) = {Material::water, y + 9u, 37, rain_flag | y};
    edited.at(2u, 10u) = {Material::dirty_water, 71u, -6, rain_flag | 0x29u};
    for (std::uint32_t x = 0u; x < edited.width; ++x)
        edited.at(x, 50u) = {Material::water, 811u, 21, x}; // Untagged pool.
    auto stale = rebuild(edited);
    // Explicit user erase and an exact cross-column actor/editor relocation
    // have committed, but the derived index still names the former endpoints.
    edited.at(1u, 30u) = {Material::vacuum, 123u, -45, 0xabcdu};
    std::swap(edited.at(1u, 31u), edited.at(3u, 31u));
    const auto edited_cells = edited.cells;
    require(stale.contains(1u, 30u) && stale.contains(1u, 31u) &&
            !stale.contains(3u, 31u) && !tracked(edited.at(1u, 30u)) &&
            !tracked(edited.at(1u, 31u)) && tracked(edited.at(3u, 31u)),
            "erase/relocation fixture did not create the intended stale membership");
    const auto before_reconcile = stale;
    indexed_step(edited, stale, 3u, true);
    require(edited.cells == edited_cells && stale.data == before_reconcile.data,
            "paused simulation advanced a stale drop or index");

    const std::vector<Coordinate> changed{{1u, 30u}, {1u, 31u}, {3u, 31u}};
    require(reconcile_edited_membership(edited, stale, changed),
            "paused derived-only reconciliation rejected valid edit endpoints");
    require(edited.cells == edited_cells && stale.data[1u] == 2u &&
            stale.data[2u] == 1u && stale.data[3u] == 1u &&
            stale.contains(1u, 20u) && stale.contains(1u, 63u),
            "paused reconciliation changed payload or stranded valid same-column drops");
    compare_index(stale, scan_tags(edited));
    const auto repaired = stale.data;
    require(reconcile_edited_membership(edited, stale,
                {{3u, 31u}, {1u, 31u}, {1u, 30u}, {3u, 31u}}) &&
            stale.data == repaired && edited.cells == edited_cells,
            "reordered/repeated reconciliation changed exact membership or canonical state");
    require(!reconcile_edited_membership(edited, stale, {{1u, 20u}, {4u, 31u}}) &&
            stale.data == repaired && edited.cells == edited_cells,
            "invalid reconciliation footprint partially mutated data");

    // Defensive stale-entry cleanup must not stop after the first invalid Y
    // or erase a different live owner in the same column. The missing new
    // destination still requires explicit reconciliation: iteration is not a
    // replacement full-world tag scan and must not pretend to discover it.
    auto moving = edited;
    auto oracle = edited;
    auto defensive = before_reconcile;
    indexed_step(moving, defensive, 3u);
    dense_step(oracle, 3u);
    require(moving.cells == oracle.cells && !defensive.contains(1u, 30u) &&
            !defensive.contains(1u, 31u) && defensive.contains(1u, 21u) &&
            defensive.contains(1u, 64u) && defensive.data[1u] == 2u &&
            !defensive.contains(3u, 31u),
            "stale-entry cleanup lost valid same-column rain or invented destination discovery");
    require(moving.at(1u, 30u) == edited.at(1u, 30u) &&
            moving.at(1u, 31u) == edited.at(1u, 31u) &&
            moving.at(3u, 31u) == edited.at(3u, 31u),
            "stale-entry cleanup changed erased/displaced/relocated canonical payload");
    const auto moved_cells = moving.cells;
    require(reconcile_edited_membership(moving, defensive, changed) &&
            moving.cells == moved_cells, "post-edit reconciliation advanced material state");
    compare_index(defensive, scan_tags(moving));
    require(material_counts(moving) == material_counts(edited),
            "stale-index recovery changed material counts after the explicit edit");
}

void movement_contract() {
    World initial(5u, 2048u);
    for (std::uint32_t y = 0u; y < initial.height; ++y)
        for (std::uint32_t x = 0u; x < initial.width; ++x) {
            const auto identity = y * initial.width + x;
            initial.at(x, y) = {identity % 3u == 0u ? Material::vacuum : Material::gas,
                identity + 7u, static_cast<std::int32_t>(identity % 155u) - 40,
                identity & 0x000fffffu};
        }
    constexpr std::array<std::uint32_t, 13> starts{
        0u, 30u, 31u, 32u, 62u, 63u, 64u, 1022u, 1023u, 1024u, 2015u, 2016u, 2047u};
    for (std::uint32_t x = 0u; x < initial.width; ++x) {
        for (const auto y : starts)
            initial.at(x, y) = {(y & 1u) != 0u ? Material::dirty_water : Material::water,
                100u + y, static_cast<std::int32_t>(y % 101u) - 20,
                rain_flag | (y * initial.width + x)};
        initial.at(x, 100u) = {Material::water, 901u, 23, 0x43u}; // Settled pool.
        initial.at(x, 99u) = {Material::dirty_water, 7u, 17, rain_flag | 0x19u};
        initial.at(x, 150u) = {Material::cloud, 8000u, 14, rain_flag | 0x99u};
        initial.at(x, 170u) = {Material::stone, 31u, 86, rain_flag | 0x77u};
        initial.at(x, 180u) = {Material::water, 93u, 12, 0x23u}; // Untagged isolated Water.
    }
    auto world = initial;
    auto oracle = initial;
    auto index = rebuild(world);
    const auto initial_materials = material_counts(initial);
    compare_index(index, scan_tags(world));
    const auto frozen_cells = world.cells;
    const auto frozen_index = index.data;
    indexed_step(world, index, 0u, true);
    require(world.cells == frozen_cells && index.data == frozen_index,
            "paused index iteration moved canonical state");

    // Deliberately retain every possible false-positive summary bit, including
    // words beyond a short height in the separate partial-word fixture below.
    for (std::uint32_t x = 0u; x < world.width; ++x)
        for (std::uint32_t half = 0u; half < 2u; ++half)
            index.data[index.layout.summary(x, half)] = 0xffffffffu;
    for (std::uint32_t tick = 0u; tick < 24u; ++tick) {
        indexed_step(world, index, tick);
        dense_step(oracle, tick);
        require(world.cells == oracle.cells, "indexed one-step movement differs from dense original set");
        require(material_counts(world) == initial_materials,
                "drop movement changed Water/Dirty Water/gas/Vacuum/Cloud/solid totals");
        compare_index(index, scan_tags(world));
        for (std::uint32_t x = 0u; x < world.width; ++x) {
            for (const auto y : {100u, 150u, 170u, 180u})
                require(world.at(x, y) == initial.at(x, y),
                        "rain disturbed pool, Cloud, solid or untracked Water");
        }
    }
    // One four-tick interval gives each column exactly one opportunity. Tight
    // chains at 30/31/32 and 62/63/64 must all move once, including 31->32,
    // 63->64 and 1023->1024 across both row and summary word boundaries.
    auto once = initial;
    auto once_index = rebuild(once);
    for (std::uint32_t tick = 0u; tick < 4u; ++tick) indexed_step(once, once_index, tick);
    for (std::uint32_t x = 0u; x < once.width; ++x) {
        for (const auto y : starts) {
            auto expected = initial.at(x, y);
            if (y + 1u == once.height) expected.aux = (expected.aux & ~rain_flag) | moved_flag;
            else { ++expected.age; expected.aux |= moved_flag; }
            require(once.at(x, (std::min)(y + 1u, once.height - 1u)) == expected,
                    "explicit drop witness did not move exactly once or preserve payload");
        }
        auto landed = initial.at(x, 99u);
        landed.aux = (landed.aux & ~rain_flag) | moved_flag;
        require(once.at(x, 99u) == landed && !once_index.contains(x, 99u),
                "landing did not clear only rain ownership");
    }

    World short_world(3u, 65u);
    short_world.at(0u, 31u) = {Material::dirty_water, 22u, -7, rain_flag | 0x31u};
    short_world.at(0u, 32u) = {Material::gas, 44u, 91, 0x32u};
    short_world.at(1u, 63u) = {Material::water, 28u, 63, rain_flag | 0x63u};
    short_world.at(1u, 64u) = {Material::vacuum, 87u, -43, 0x64u};
    short_world.at(2u, 64u) = {Material::water, 11u, 13, rain_flag | 0x12u};
    const auto short_before = short_world;
    auto short_index = rebuild(short_world);
    for (std::uint32_t x = 0u; x < 3u; ++x)
        for (std::uint32_t half = 0u; half < 2u; ++half)
            short_index.data[short_index.layout.summary(x, half)] = 0xffffffffu;
    for (std::uint32_t tick = 0u; tick < 4u; ++tick) indexed_step(short_world, short_index, tick);
    require(short_world.at(0u, 31u) == short_before.at(0u, 32u) &&
            short_world.at(1u, 63u) == short_before.at(1u, 64u),
            "displaced gas/Vacuum lost exact age, temperature or aux");
    require(short_index.contains(0u, 32u) && short_index.contains(1u, 64u) &&
            !short_index.contains(2u, 64u), "edge/padding membership incorrect");
    compare_index(short_index, scan_tags(short_world));

    // Persist canonical cells only. Rebuilding from their tag payload must
    // preserve several same-column owners without serializing derived state.
    auto loaded = world;
    auto loaded_index = rebuild(loaded);
    require(loaded.cells == world.cells && loaded_index.data[0u] > 1u,
            "multi-drop derived rebuild changed canonical payload or lost same-column owners");
    for (std::uint32_t tick = 24u; tick < 36u; ++tick) {
        indexed_step(world, index, tick);
        indexed_step(loaded, loaded_index, tick);
    }
    require(loaded.cells == world.cells, "canonical reload changed subsequent drop behavior");
    compare_index(loaded_index, scan_tags(loaded));
}
} // namespace

int main() {
    try {
        allocation_contract();
        membership_contract();
        atomic_event_contract();
        admission_snapshot_contract();
        stale_membership_contract();
        movement_contract();
        std::cout << "Rain membership: " << checks << " CPU contract assertions passed.\n"
                  << "Scope: golden layout, atomic-event interleavings, conservative summaries, "
                     "paused edit reconciliation, and once-only bottom-up drop transactions.\n"
                  << "No GPU memory-order, dispatch integration, weather-cycle, or speed claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Rain membership contract failed after " << checks
                  << " assertions: " << error.what() << '\n';
        return 1;
    }
}
