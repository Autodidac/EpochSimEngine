#include <array>
#include <cstdint>
#include <iostream>

#include "epochsimengine/material.hpp"
#include "epochsimengine/simulation_policy.hpp"

namespace shader_appearance {
using uint = std::uint32_t;
constexpr uint PHASE_LIQUID =
    static_cast<uint>(sandhybrid::MaterialPhase::liquid);
// Compile the actual production selector, not a duplicate rendering rule.
#include "material_visual_phase.glsl"
}

namespace {
using sandhybrid::Material;
using sandhybrid::MaterialPhase;
using shader_appearance::materialPresentationPhase;
using uint = std::uint32_t;

constexpr uint half_water = 0x00800000u;
constexpr uint carrier_mask = 0x000000ffu;

uint presentation_phase(Material material, std::int32_t temperature, uint aux) {
    const bool half = material == Material::water && (aux & half_water) != 0u;
    const uint physical = static_cast<uint>(sandhybrid::phase_at(material, temperature));
    return materialPresentationPhase(half, physical);
}

bool check_half_water_carriers() {
    constexpr std::array<std::int32_t, 14> temperatures{
        -200, -101, -2, -1, 0, 20, 99, 100, 101, 120, 154, 180, 500, 5000};
    // Entropy, move/rain/wet flags and encoded displaced gas identities must
    // not route a Half Water owner into gas-density or vapor-color shading.
    constexpr std::array<uint, 5> payloads{
        0u, 0x00012300u, 0x007fff00u, 0x11045600u, 0x80078900u};
    for (const auto temperature : temperatures) {
        for (const uint payload : payloads) {
            for (uint carrier = 0u; carrier <= carrier_mask; ++carrier) {
                const uint aux = half_water | payload | carrier;
                if (presentation_phase(Material::water, temperature, aux) !=
                    shader_appearance::PHASE_LIQUID) {
                    std::cerr << "Half Water changed presentation phase at "
                              << temperature << " C, carrier " << carrier << '\n';
                    return false;
                }
            }
        }
    }

    // The old selector routed both same-temperature owners as vapor, whose
    // shader then interpreted carrier 0 vs 255 as gas density.
    const auto physical = sandhybrid::phase_at(Material::water, 120);
    if (physical != MaterialPhase::vapor ||
        sandhybrid::policy::decode_half_water_medium_temperature(0u) != 20 ||
        sandhybrid::policy::decode_half_water_medium_temperature(255u) != 154)
        return false;
    return presentation_phase(Material::water, 120, half_water) ==
           presentation_phase(Material::water, 120, half_water | 255u);
}

bool check_non_half_phases_unchanged() {
    // Every physical result passes through exactly for non-Half owners. The
    // visual helper may not redefine full Water, real Steam, metals or life.
    for (uint phase = static_cast<uint>(MaterialPhase::empty);
         phase <= static_cast<uint>(MaterialPhase::vapor); ++phase) {
        if (materialPresentationPhase(false, phase) != phase) return false;
    }
    constexpr std::array<std::int32_t, 11> temperatures{
        -200, -21, -2, 0, 20, 99, 100, 101, 102, 120, 5000};
    constexpr std::array<uint, 4> payloads{0u, 255u, half_water, 0xffffffffu};
    for (uint id = 0u; id < sandhybrid::material_count; ++id) {
        const auto material = static_cast<Material>(id);
        for (const auto temperature : temperatures) {
            const uint physical = static_cast<uint>(sandhybrid::phase_at(material, temperature));
            for (uint aux : payloads) {
                // For the Water control only, deliberately construct full
                // Water; the same bit on another material is not a half flag.
                if (material == Material::water) aux &= ~half_water;
                if (presentation_phase(material, temperature, aux) != physical)
                    return false;
            }
        }
    }
    return presentation_phase(Material::water, 99, 0u) ==
               static_cast<uint>(MaterialPhase::liquid) &&
           presentation_phase(Material::water, 100, 0u) ==
               static_cast<uint>(MaterialPhase::vapor) &&
           presentation_phase(Material::steam, 120, half_water | 255u) ==
               static_cast<uint>(MaterialPhase::gas) &&
           presentation_phase(Material::dirty_water, 100, half_water | 255u) ==
               static_cast<uint>(MaterialPhase::vapor);
}
}

int main() {
    if (!check_half_water_carriers()) return 1;
    if (!check_non_half_phases_unchanged()) return 2;
    std::cout << "Material appearance phase: all 256 Half Water carrier codes "
                 "remain liquid; all non-Half physical phases pass through unchanged. "
                 "CPU selector coverage only; presented pixels remain a GPU check.\n";
    return 0;
}
