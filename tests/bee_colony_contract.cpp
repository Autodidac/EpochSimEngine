#include <epochsimengine/bee_colony.hpp>
#include <sandhybrid/bee_colony.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace epochsimengine;
std::uint64_t checks{};
std::uint64_t failures{};

void check(const bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        if (failures <= 16u) std::cerr << message << '\n';
    }
}

// Independent legacy district layout: literal field widths, not the packer.
std::uint32_t old_district_payload(const std::uint32_t aux,
    const std::uint32_t local_x, const std::uint32_t local_y,
    const std::uint32_t slot, const std::uint32_t district) {
    return (aux & 0xff000000u) | (local_x / 8u) | ((local_y / 8u) << 7u) |
        ((slot & 127u) << 13u) | (district << 20u);
}

void legacy_district_contract() {
    constexpr std::array widths{5120u, 7680u, 10240u};
    constexpr std::array<BeeHome, 7> locals{{
        {0u, 0u}, {639u, 359u}, {7u, 7u}, {8u, 8u},
        {512u, 234u}, {512u, 232u}, {321u, 173u}}};
    for (const auto width : widths) {
        for (std::uint32_t district = 0u; district < 8u; ++district) {
            const auto ox = persistent_world_district_origin_x(width, district);
            const auto oy = persistent_world_district_origin_y(1440u, district);
            for (const auto local : locals) {
                const BeeHome home{ox + local.x, oy + local.y};
                const auto address = bee_district_address(home, width, 1440u);
                check(address.valid && address.district == district && address.local == local,
                    "district address changed");
                for (std::uint32_t slot = 0u; slot < 128u; ++slot) {
                    constexpr auto aux = 0xfbabcdefu;
                    const auto packed = pack_bee_home_metadata(aux, home, slot, width, 1440u);
                    check(packed == old_district_payload(aux, local.x, local.y, slot, district),
                        "legacy district bytes changed");
                    check(!bee_uses_global_home(packed, width, 1440u),
                        "legacy district acquired global discriminator");
                    check(bee_home_from_metadata(packed, width, 1440u) ==
                            BeeHome{ox + (local.x / 8u) * 8u, oy + (local.y / 8u) * 8u},
                        "legacy district home changed");
                    check(bee_slot_from_metadata(packed, width, 1440u) == slot,
                        "legacy slot bits changed");
                    check(bee_home_alignment(packed, width, 1440u) == 8u,
                        "legacy home search alignment changed");
                }
            }
        }
    }
}

void global_home_contract() {
    constexpr std::array widths{5120u, 7680u, 10240u, 16384u};
    for (const auto width : widths) {
        for (const auto height : {1440u, 2048u}) {
            const std::array<BeeHome, 7> homes{{
                {0u, 0u}, {width - 1u, 0u}, {width - 1u, height - 1u},
                {700u, 200u}, {1000u, 536u}, {width / 2u, height - 1u},
                {640u, 900u}}};
            for (const auto home : homes) {
                if (bee_district_address(home, width, height).valid) continue;
                check(bee_home_encodable(home, width, height), "valid world home rejected");
                for (std::uint32_t slot = 0u; slot < 60u; ++slot) {
                    constexpr auto aux = 0xb7ffffffu;
                    const auto packed = pack_bee_home_metadata(aux, home, slot, width, height);
                    const auto expected = 0xb7800000u | (home.x / 16u) |
                        ((home.y / 16u) << 10u) | (slot << 17u);
                    check(packed == expected, "global home payload differs from literal field layout");
                    check(bee_uses_global_home(packed, width, height), "global discriminator missing");
                    check(bee_slot_from_metadata(packed, width, height) == slot,
                        "global slot decoded incorrectly");
                    check(bee_home_from_metadata(packed, width, height) ==
                            BeeHome{(home.x / 16u) * 16u, (home.y / 16u) * 16u},
                        "global home decoded incorrectly");
                    check(bee_home_alignment(packed, width, height) == 16u,
                        "global home search alignment incorrect");
                }
            }
        }
    }
    // All lifecycle flag combinations survive packing; no metadata bit leaks
    // into the preserved high byte, and old district packing clears bit 23.
    for (std::uint32_t flags = 0u; flags < 256u; ++flags) {
        const auto packed = pack_bee_home_metadata(flags << 24u,
            {10239u, 1439u}, 59u, 10240u, 1440u);
        check((packed & 0xff000000u) == (flags << 24u), "lifecycle flag altered");
        const auto district = pack_bee_home_metadata(packed, {512u, 954u}, 59u, 10240u, 1440u);
        check((district & 0x00800000u) == 0u, "repacking a district retains global discriminator");
    }
    check(!bee_home_encodable({10240u, 22u}, 10240u, 1440u), "right exterior accepted");
    check(!bee_home_encodable({20u, 1440u}, 10240u, 1440u), "bottom exterior accepted");
    check(!bee_home_encodable({16384u, 200u}, 16385u, 1440u), "global X overflow accepted");
    check(!bee_home_encodable({700u, 2048u}, 10240u, 2049u), "global Y overflow accepted");
}

void legacy_small_world_contract() {
    check(bee_home_encodable({512u, 234u}, 640u, 360u),
        "canonical zero-origin legacy home rejected");
    for (std::uint32_t slot = 0u; slot < 128u; ++slot) {
        const auto packed = pack_bee_home_metadata(0xdfffffff, {512u, 234u}, slot, 640u, 360u);
        const auto expected = 0xdf000000u | 128u | (58u << 8u) | ((slot | 128u) << 15u);
        check(packed == expected, "legacy PPM constructor bytes changed");
        check(bee_home_from_metadata(packed, 640u, 360u) == BeeHome{512u, 232u},
            "legacy small-world home changed");
        check(bee_slot_from_metadata(packed, 640u, 360u) == slot,
            "legacy small-world slot changed");
        check(bee_home_alignment(packed, 640u, 360u) == 4u,
            "legacy small-world alignment changed");
    }
    check(!bee_uses_global_home(0x00800000u, 640u, 360u),
        "persistent global discriminator used in a legacy small world");
}

void legacy_offset_admission_contract() {
    // The old constructor intentionally remains byte-compatible for imports.
    // Its authored flag plus absolute coordinates does not describe a newly
    // placed home on an offset legacy canvas; admission must reject that use.
    constexpr BeeHome home{512u, 234u};
    constexpr std::uint32_t aux = 0xb0ffffffu;
    constexpr std::uint32_t slot = 59u;
    constexpr auto legacy_payload = 0xb0000000u | 128u | (58u << 8u) |
        ((slot | 128u) << 15u);
    check(!bee_home_encodable(home, 1280u, 360u),
        "offset1280x360 home incorrectly admitted");
    const auto horizontal = pack_bee_home_metadata(aux, home, slot, 1280u, 360u);
    check(horizontal == legacy_payload, "offset legacy import bytes changed");
    check(bee_home_from_metadata(horizontal, 1280u, 360u) == BeeHome{832u, 232u},
        "historical horizontal-offset decode changed");
    check(!bee_home_encodable(home, 640u, 1080u),
        "vertically offset legacy home incorrectly admitted");
    const auto vertical = pack_bee_home_metadata(aux, home, slot, 640u, 1080u);
    check(vertical == legacy_payload, "vertical legacy import bytes changed");
    check(bee_home_from_metadata(vertical, 640u, 1080u) == BeeHome{512u, 952u},
        "historical vertical-offset decode changed");
    for (const auto width : {5120u, 7680u, 10240u}) {
        check(bee_home_encodable(home, width, 1440u),
            "persistent high-sky home rejected by legacy offset guard");
        check(bee_home_encodable({512u, 954u}, width, 1440u),
            "persistent authored home rejected by legacy offset guard");
    }
}

void footprint_contract() {
    constexpr auto footprint = beehive_placement_footprint();
    static_assert(footprint.min_x == -20 && footprint.max_x == 20);
    static_assert(footprint.min_y == -22 && footprint.max_y == 14);
    check(beehive_placement_fits(20, 22, 41u, 37u), "exact minimal footprint rejected");
    check(!beehive_placement_fits(19, 22, 41u, 37u), "left partial formation accepted");
    check(!beehive_placement_fits(21, 22, 41u, 37u), "right partial formation accepted");
    check(!beehive_placement_fits(20, 21, 41u, 37u), "top partial formation accepted");
    check(!beehive_placement_fits(20, 23, 41u, 37u), "bottom partial formation accepted");
    for (const auto width : {5120u, 7680u, 10240u}) {
        check(beehive_placement_fits(20, 22, width, 1440u), "valid near-edge sky rejected");
        check(beehive_placement_fits(static_cast<std::int32_t>(width) - 21,
                1425, width, 1440u), "valid bottom/right edge rejected");
        check(beehive_placement_fits(700, 200, width, 1440u), "valid sky rejected");
        check(beehive_placement_fits(650, 900, width, 1440u), "district edge/gap rejected");
        check(!beehive_placement_fits(static_cast<std::int32_t>(width) - 20,
                1425, width, 1440u), "partial right edge accepted");
        check(!beehive_placement_fits(20, 1426, width, 1440u), "partial bottom edge accepted");
    }
    check(!beehive_placement_fits(-1, 30, 10240u, 1440u), "negative center accepted");
    check(!beehive_placement_fits((std::numeric_limits<std::int32_t>::max)(),
            (std::numeric_limits<std::int32_t>::max)(), 10240u, 1440u),
        "overflow-sized center accepted");
    check(!beehive_placement_fits(20, 22, 0u, 0u), "empty world accepted");
}
} // namespace

int main() {
    static_assert(std::is_same_v<epochsimengine::BeeHome, sandhybrid::BeeHome>);
    legacy_district_contract();
    global_home_contract();
    legacy_small_world_contract();
    legacy_offset_admission_contract();
    footprint_contract();
    std::cout << checks << " bee colony metadata/placement assertions; " << failures << " failures\n";
    return failures == 0u ? 0 : 1;
}
