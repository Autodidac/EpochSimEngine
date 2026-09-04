#pragma once

#include "sandhybrid/scene_image.hpp"
#include <cstdint>
#include <string_view>

namespace sandhybrid {

enum class PhantomHiveRecoveryStatus : std::uint8_t {
    not_applicable, no_candidate, signature_mismatch, repaired
};

struct PhantomHiveRecoveryResult final {
    PhantomHiveRecoveryStatus status{PhantomHiveRecoveryStatus::not_applicable};
    std::uint32_t queen_x{};
    std::uint32_t queen_y{};
    std::uint32_t body_cells{};
    std::uint32_t empty_cells{};
    std::uint32_t bee_cells{};
    std::uint32_t preserved_formation_cells{};
    std::uint32_t mismatches{};
    [[nodiscard]] constexpr std::uint32_t changed_cells() const noexcept {
        return body_cells + empty_cells + bee_cells;
    }
};

[[nodiscard]] constexpr std::string_view phantom_hive_recovery_status_name(
    const PhantomHiveRecoveryStatus status) noexcept {
    switch (status) {
    case PhantomHiveRecoveryStatus::not_applicable: return "not applicable";
    case PhantomHiveRecoveryStatus::no_candidate: return "no phantom candidate";
    case PhantomHiveRecoveryStatus::signature_mismatch:
        return "signature mismatch: no changes; explicit Reset required for cleanup";
    case PhantomHiveRecoveryStatus::repaired: return "exact v2.5.27 phantom repaired in memory";
    }
    return "unknown";
}

// Call on the completely validated staged save, before upload. No disk I/O,
// actor changes, broad hive search, or global Bee cleanup. Every rejection is
// byte-identical. Requires schema 2, World, supported dimensions, both real
// authored Queens, and the complete obsolete loader non-hole body fingerprint.
// Chamber/exit owners must be Empty or Atmosphere and remain byte-identical;
// gas relaxation makes their provenance ambiguous even for exact old Empty.
[[nodiscard]] PhantomHiveRecoveryResult recover_v2527_phantom_hive(
    std::span<SceneCell> cells, std::uint32_t width, std::uint32_t height,
    Scene scene, std::uint32_t format_version) noexcept;

struct LegacyHiveOwnerResult final {
    bool initialized{};
    std::uint32_t bee_cells{};
    std::uint32_t structural_cells{};
};

// Schema-1 only: call after normalize_pre_pr19_hives and conversion of changed
// material IDs to SceneCells at the SAME explicit scene origin. Requires the
// complete body and 60 formation Bees before any write. Persistent callers use
// actual district origins, never the obsolete single-map origin.
[[nodiscard]] LegacyHiveOwnerResult initialize_schema1_hive_owners(
    std::span<SceneCell> cells, std::uint32_t width, std::uint32_t height,
    std::uint32_t scene_origin_x, std::uint32_t scene_origin_y, Scene scene) noexcept;

} // namespace sandhybrid
