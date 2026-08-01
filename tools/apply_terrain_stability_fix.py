#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.write_text(content, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    if old not in text:
        if new in text:
            return
        raise RuntimeError(f"Expected terrain-stability source block missing from {path}")
    target.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


write(
    "shaders/tiles.glsl",
    """#ifndef SANDHYBRID_TILES_GLSL
#define SANDHYBRID_TILES_GLSL

const uint TILE_SIZE = 8u;
const uint TILE_CELL_COUNT = 64u;
const uint TILE_STABILITY_OCCUPANCY = 52u;
const uint TILE_COLLAPSE_OCCUPANCY = 32u;
const uint TILE_STABILIZE_TICKS = 120u;
const uint TILE_RESTABILIZE_COOLDOWN = 240u;

const uint TILE_STRUCTURAL = 0x00000001u;
const uint TILE_SUPPORTED = 0x00000002u;
const uint TILE_SLEEPING = 0x00000004u;
const uint TILE_ACTIVE = 0x00000008u;
const uint TILE_CANDIDATE = 0x00000010u;
const uint TILE_STABLE = 0x00000020u;
const uint TILE_COLLAPSING = 0x00000040u;
const uint TILE_DAMAGED = 0x00000080u;

struct TileState {
    uint material;
    uint occupancy;
    uint flags;
    uint counters; // low 16: stability ticks, high 16: restabilization cooldown
};

uint tileColumns(uint width) { return (width + TILE_SIZE - 1u) / TILE_SIZE; }
uvec2 tileCoordinate(ivec2 p) { return uvec2(max(p, ivec2(0))) / TILE_SIZE; }
uint tileIndex(ivec2 p, uint width) {
    uvec2 tile = tileCoordinate(p);
    return tile.y * tileColumns(width) + tile.x;
}
uint tileStableTicks(TileState state) { return state.counters & 0xffffu; }
uint tileCooldown(TileState state) { return state.counters >> 16u; }
uint packTileCounters(uint stableTicks, uint cooldown) {
    return min(stableTicks, 0xffffu) | (min(cooldown, 0xffffu) << 16u);
}
bool tileHas(TileState state, uint flag) { return (state.flags & flag) != 0u; }

#endif
""",
)

write(
    "shaders/tiles.comp",
    """#version 450
#extension GL_GOOGLE_include_directive : require
#include "materials.glsl"
#include "tiles.glsl"

layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 4) buffer Tiles { TileState tiles[]; };

bool tileInside(ivec2 p) {
    return p.x >= 0 && p.y >= 0 && p.x < int(pc.width) && p.y < int(pc.height);
}

void main() {
    uvec2 tile = gl_GlobalInvocationID.xy;
    uint columns = tileColumns(pc.width);
    uint rows = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;
    if (tile.x >= columns || tile.y >= rows) return;

    uint index = tile.y * columns + tile.x;
    TileState previous = tiles[index];
    ivec2 origin = ivec2(tile * TILE_SIZE);

    uint counts[MATERIAL_COUNT];
    for (uint i = 0u; i < MATERIAL_COUNT; ++i) counts[i] = 0u;
    uint occupied = 0u;
    uint structural = 0u;
    uint stable = 0u;
    uint healthSum = 0u;
    bool hot = false;
    bool moving = false;
    bool reacting = false;

    for (uint y = 0u; y < TILE_SIZE; ++y) {
        for (uint x = 0u; x < TILE_SIZE; ++x) {
            ivec2 p = origin + ivec2(x, y);
            if (!tileInside(p)) continue;
            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY || isCellGas(cell) || isCellLiquid(cell)) continue;
            ++occupied;
            if (cell.material < MATERIAL_COUNT) ++counts[cell.material];
            if (isStructural(cell)) {
                ++structural;
                uint health = stateValue(cell);
                healthSum += health == 0u ? 255u : health;
            }
            uint phase = cellPhase(cell);
            bool cellStable = cell.age >= 30u && (cell.aux & AUX_MOVED) == 0u &&
                              phase != PHASE_SOFTENED && phase != PHASE_MOLTEN && phase != PHASE_VAPOR;
            if (cellStable) ++stable;
            moving = moving || (cell.aux & AUX_MOVED) != 0u || cell.age < 8u;
            hot = hot || abs(cell.temperature - 20) > 80;
            reacting = reacting || cell.material == MAT_FIRE || cell.material == MAT_EMBER ||
                       cell.material == MAT_GUNPOWDER || cell.material == MAT_ACID;
        }
    }

    uint dominant = MAT_EMPTY;
    uint dominantCount = 0u;
    for (uint material = 1u; material < MATERIAL_COUNT; ++material) {
        if (counts[material] > dominantCount) {
            dominantCount = counts[material];
            dominant = material;
        }
    }

    uint cooldown = tileCooldown(previous);
    if (cooldown > 0u) --cooldown;
    bool uniformMaterial = dominant != MAT_EMPTY && dominantCount == occupied;
    bool stabilizable = uniformMaterial && isReconstructableMaterial(dominant);
    bool structuralTile = structural > 0u;

    // An aligned region never fills gaps, synthesizes pixels, or becomes a second
    // tile object. Existing represented cells may only qualify as coherent terrain.
    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&
                           previous.occupancy >= TILE_STABILITY_OCCUPANCY;
    bool denseStructural = structural >= TILE_STABILITY_OCCUPANCY || previouslyDense;
    bool damaged = structuralTile && denseStructural &&
                   (structural < TILE_CELL_COUNT ||
                    (structural > 0u && healthSum / structural < 240u));
    bool candidate = !structuralTile && cooldown == 0u && stabilizable &&
                     occupied >= TILE_STABILITY_OCCUPANCY && stable == occupied &&
                     !moving && !hot && !reacting;
    uint stableTicks = candidate ? min(tileStableTicks(previous) + 1u, TILE_STABILIZE_TICKS) : 0u;
    bool newlyStable = candidate && stableTicks >= TILE_STABILIZE_TICKS;

    // More than half dislodged means fewer than 32 represented structural cells.
    // The chemistry pass releases every remaining cell in the same simulation tick.
    bool collapsing = structuralTile && previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY;
    bool coherent = (structuralTile || newlyStable) && !collapsing;
    bool anchored = origin.y + int(TILE_SIZE) >= int(pc.height);
    bool supported = coherent || anchored;

    uint flags = 0u;
    if (structuralTile || newlyStable) flags |= TILE_STRUCTURAL;
    if (supported) flags |= TILE_SUPPORTED;
    if (candidate) flags |= TILE_CANDIDATE;
    if (coherent) flags |= TILE_STABLE;
    if (collapsing) flags |= TILE_COLLAPSING;
    if (damaged) flags |= TILE_DAMAGED;

    uint coherentCells = structuralTile ? structural : occupied;
    bool sleeping = coherent && !damaged && !moving && !hot && !reacting &&
                    dominantCount == coherentCells;
    if (sleeping) flags |= TILE_SLEEPING;
    else flags |= TILE_ACTIVE;

    if (collapsing || newlyStable) cooldown = TILE_RESTABILIZE_COOLDOWN;
    tiles[index] = TileState(dominant, structuralTile ? structural : occupied,
                             flags, packTileCounters(stableTicks, cooldown));
}
""",
)

write(
    "include/sandhybrid/simulation_policy.hpp",
    """#pragma once

#include <cstdint>

namespace sandhybrid::policy {

inline constexpr std::uint32_t tile_size = 8u;
inline constexpr std::uint32_t tile_cells = tile_size * tile_size;
inline constexpr std::uint32_t stability_occupancy = 52u;
inline constexpr std::uint32_t collapse_occupancy = tile_cells / 2u;
inline constexpr std::uint32_t stability_ticks = 120u;
inline constexpr std::uint32_t restabilization_cooldown_ticks = 240u;
inline constexpr std::uint32_t terrain_cell_integrity = 255u;
inline constexpr std::uint32_t laser_damage_per_hit = 144u;
inline constexpr std::uint32_t laser_hits_to_dislodge =
    (terrain_cell_integrity + laser_damage_per_hit - 1u) / laser_damage_per_hit;
inline constexpr std::uint32_t water_pressure_depth = 8u;
inline constexpr std::uint32_t sunlight_update_interval = 4u;
inline constexpr std::uint32_t vent_eruption_pressure = 220u;
inline constexpr std::uint32_t vent_gas_release_pressure = 72u;

[[nodiscard]] constexpr bool stability_ready(
    const std::uint32_t occupancy,
    const std::uint32_t settled_ticks,
    const bool compatible,
    const bool stable_phase,
    const bool moving,
    const bool reacting,
    const std::uint32_t cooldown) noexcept {
    return occupancy >= stability_occupancy && settled_ticks >= stability_ticks &&
           compatible && stable_phase && !moving && !reacting && cooldown == 0u;
}

[[nodiscard]] constexpr bool should_collapse(const std::uint32_t represented_cells) noexcept {
    return represented_cells < collapse_occupancy;
}

[[nodiscard]] constexpr std::uint32_t update_vent_pressure(
    const std::uint32_t pressure, const bool blocked, const bool open) noexcept {
    if (blocked) return pressure >= 252u ? 255u : pressure + 3u;
    if (open) return pressure > 4u ? pressure - 4u : 0u;
    return pressure == 255u ? 255u : pressure + 1u;
}

} // namespace sandhybrid::policy
""",
)

write(
    "tests/behavior_contract.cpp",
    """#include "sandhybrid/material.hpp"
#include "sandhybrid/simulation_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

enum class CreationPath : std::uint8_t {
    map, stable_terrain, cursor, fragment, particle, reaction, save
};

struct CanonicalState final {
    sandhybrid::Material material{};
    sandhybrid::MaterialPhase phase{};
    std::int32_t temperature{};
    std::uint32_t represented_mass{};
    std::uint32_t damage{};

    friend constexpr bool operator==(const CanonicalState&, const CanonicalState&) = default;
};

[[nodiscard]] constexpr CanonicalState canonical_state(
    [[maybe_unused]] const CreationPath path,
    const sandhybrid::Material material,
    const std::int32_t temperature,
    const std::uint32_t represented_mass,
    const std::uint32_t damage) noexcept {
    return {material, sandhybrid::phase_at(material, temperature), temperature, represented_mass, damage};
}

[[nodiscard]] constexpr bool creation_paths_are_canonical() noexcept {
    constexpr std::array paths{
        CreationPath::map, CreationPath::stable_terrain, CreationPath::cursor,
        CreationPath::fragment, CreationPath::particle, CreationPath::reaction,
        CreationPath::save,
    };
    constexpr std::array temperatures{-100, 20, 180, 1300, 3000};
    for (std::uint32_t material_id = 0; material_id < sandhybrid::material_count; ++material_id) {
        const auto material = static_cast<sandhybrid::Material>(material_id);
        for (const auto temperature : temperatures) {
            const auto expected = canonical_state(paths.front(), material, temperature, 37u, 72u);
            for (const auto path : paths) {
                if (canonical_state(path, material, temperature, 37u, 72u) != expected) return false;
            }
        }
    }
    return true;
}

[[nodiscard]] constexpr bool local_water_equalization_preserves_volume() noexcept {
    std::array<std::uint32_t, 16> columns{};
    columns[0] = 64;
    const auto original = 64u;
    for (std::uint32_t pass = 0; pass < 256; ++pass) {
        for (std::uint32_t parity = 0; parity < 2; ++parity) {
            for (std::uint32_t x = parity; x + 1 < columns.size(); x += 2) {
                const auto difference = static_cast<std::int32_t>(columns[x]) -
                                        static_cast<std::int32_t>(columns[x + 1]);
                if (difference == 0) continue;
                const auto magnitude = static_cast<std::uint32_t>(difference > 0 ? difference : -difference);
                const auto transfer = (std::min)(sandhybrid::policy::water_pressure_depth,
                                                 (std::max)(1u, magnitude / 2u));
                if (difference > 0) {
                    columns[x] -= transfer;
                    columns[x + 1] += transfer;
                } else {
                    columns[x + 1] -= transfer;
                    columns[x] += transfer;
                }
            }
        }
    }
    std::uint32_t total{};
    std::uint32_t minimum = columns.front();
    std::uint32_t maximum = columns.front();
    for (const auto value : columns) {
        total += value;
        minimum = (std::min)(minimum, value);
        maximum = (std::max)(maximum, value);
    }
    return total == original && maximum - minimum <= 1;
}

[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {
    constexpr std::uint32_t initial_mass = 64u;
    constexpr std::uint32_t detached_pixels = 33u;
    constexpr std::uint32_t remaining_pixels = initial_mass - detached_pixels;
    static_assert(sandhybrid::policy::should_collapse(remaining_pixels));
    constexpr std::uint32_t settled_mass = remaining_pixels + detached_pixels;
    return settled_mass == initial_mass;
}

static_assert(creation_paths_are_canonical());
static_assert(local_water_equalization_preserves_volume());
static_assert(terrain_stability_preserves_representation());
static_assert(sandhybrid::policy::stability_ready(52u, 120u, true, true, false, false, 0u));
static_assert(!sandhybrid::policy::stability_ready(51u, 120u, true, true, false, false, 0u));
static_assert(sandhybrid::policy::laser_hits_to_dislodge == 2u);
static_assert(!sandhybrid::policy::should_collapse(32u));
static_assert(sandhybrid::policy::should_collapse(31u));
static_assert(sandhybrid::policy::water_pressure_depth == 8u);
static_assert(sandhybrid::policy::vent_eruption_pressure > sandhybrid::policy::vent_gas_release_pressure);
static_assert(sandhybrid::policy::restabilization_cooldown_ticks > sandhybrid::policy::stability_ticks);

} // namespace

int main() {
    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&
           terrain_stability_preserves_representation() ? 0 : 1;
}
""",
)

write(
    "VALIDATION.md",
    """# SandHybrid Fix22 validation matrix

The project separates three validation levels:

- **Contract:** deterministic C++23 tests for IDs, phase thresholds, terrain stability policy, local water conservation, UI hit testing, and source-independent canonical state.
- **Static shader/interface:** generated-file reproducibility, include resolution, delimiter checks, reserved identifiers, material/card mappings, required rule tokens, and exact C++/GLSL push-constant layouts.
- **Windows Vulkan runtime:** actual MSVC compilation, `glslc` compilation, Vulkan execution, visual behavior, conservation logging, and GPU-load observation. Run `validate_windows.bat Release`, then execute the listed runtime scene checks.

| # | Requirement | Automated coverage | Runtime check |
|---:|---|---|---|
| 1 | Copper melts in sufficiently hot lava | `phase_at(copper, 1300) == molten`; generated copper threshold | Place copper beside a hot/pressurized vent or hotter lava source and inspect with `Alt`. |
| 2 | Steel survives lava below melting point | Steel at 1300 C is softened, not molten or vapor | Place steel in ordinary lava; confirm mass remains and card phase is solid/softened. |
| 3 | Lower-melting metals melt before steel | Gold/copper thresholds asserted below steel | Heat gold, copper, and steel together. |
| 4 | Plastic softens, melts, burns, or decomposes | Plastic threshold ordering and conversion text asserted; chemistry rules statically required | Heat plastic gradually, then expose it to fire. |
| 5 | Plastic reacts with lava and produces configured byproducts | Canonical chemistry includes plastic ignition/decomposition outputs | Drop both plastic types into lava and inspect products/counters. |
| 6 | Blocked thermal vent builds toward eruption | `update_vent_pressure` and eruption threshold contracts | Seal the Volcano vent and watch pressure/gas/magma escalation. |
| 7 | Open vent releases pressure without automatic major eruption | Open-pressure decay contract | Open the vent path and confirm pressure falls through gas/lava release. |
| 8 | Water fills/equalizes a basin quickly without volume loss | Bounded local equalization test preserves 64/64 units | Use Waterworks/Blank, alter a basin, and compare conservation counters. |
| 9 | `Alt` shows the exact material under cursor | Direct cursor-to-cell render path and input suppression statically checked | Hold `Alt` and move across cell boundaries, gases, liquids, and damaged terrain. |
| 10 | Dense settled cells qualify for stability without reconstruction | 52/64 occupancy and 120-tick stability contracts | Fill one 8x8 region above threshold; verify existing cells stop falling and empty positions stay empty. |
| 11 | Incomplete regions remain loose | 51/64 stability rejection asserted | Leave a region below 52 cells and confirm it remains simulated. |
| 12 | Stability/break cycles conserve mass | Representation conservation test | Repeatedly break and settle terrain while watching counters. |
| 13 | Pre-placed metal survives partial destruction | Creation paths canonical; structural damage releases same material | Damage pre-placed metal without heating it past vaporization. |
| 14 | Cursor-painted metal survives after losing more than half | Creation paths canonical; no provenance destruction | Paint a metal block, remove over half, and inspect all remaining fragments. |
| 15 | Stabilized metal matches other placement paths | Seven creation paths resolve to identical canonical state | Compare card phase/thresholds for map, painted, and broken metal. |
| 16 | Damaged terrain collapses without disappearing | 32-cell threshold and same-pass release are statically checked | Shoot a hanging block until 31 pixels remain; verify the remainder drops together. |
| 17 | Stable regions sleep and reduce GPU load | Tile sleeping flags and chemistry/movement early-outs required by validator | Enable `F3`, wait for green sleeping regions, compare GPU load against active water/fire. |
| 18 | Stability does not oscillate | Restabilization cooldown exceeds qualification time | Repeatedly disturb a candidate region and confirm cooldown prevents flicker. |
| 19 | Normal rendering hides raw square grid | Grid rendering is required to remain inside debug branch | Run with `F3` off. |
| 20 | Debug reveals structure/simulation state | Tile boundary, candidate, stable, sleep, active, damage tokens statically required | Toggle `F3` and inspect each overlay state. |
| 21 | CO2 is visually distinct | Static validator requires the violet CO2 presentation | Compare CO2 against smoke, darkness, stone, and water. |
| 22 | UI is aligned, responsive, unobtrusive | Wide/compact EpochGui hit-box contracts | Resize through compact and wide layouts; verify no overlaps. |
| 23 | Colors remain distinct during reactions | One generated palette/card catalog | Inspect common water/fire/smoke/steam/CO2 and acid/material combinations. |
| 24 | Gas rendering supports future shader presentation | Static validator requires `gasPresentation` boundary | Confirm current gas opacity does not obscure terrain; later shader work stays isolated. |

## Conservation runtime procedure

1. Start Sandbox or Blank.
2. Press `F3` to enable periodic conservation diagnostics.
3. Create a closed experiment away from map boundaries.
4. Run phase changes, reactions, structural breakup, and stability qualification.
5. Treat `stabilized` and `broken` as represented state transfers, not mass loss.
6. Investigate any non-zero conservation-error counter. Boundary-lost counters are reserved for explicit transient or map-boundary exits.

## Windows command

```bat
validate_windows.bat Release
```

This command builds the real application and all GLSL shaders, runs the static shader/interface validator, rebuilds the two C++23 contracts with warnings-as-errors, and runs CTest. Runtime visual and GPU checks still require launching the produced executable because they depend on the installed Vulkan driver and GPU.

## Fix22 regression checks

- A complete mouse down/up pair received within one native poll still produces exactly one `primary_pressed` or `secondary_pressed` edge.
- Character primary action drills ordinary terrain even while plasma ammunition is carried. Plasma is consumed only when the first ray hit is a hostile target.
- Every stable terrain pixel requires two ordinary laser hits: 255 integrity with 144 damage per hit.
- At 32 remaining pixels the region stays coherent; at 31 remaining pixels all survivors release in the same simulation pass.
- Ambient empty cells restore oxygen and never cause passive health loss. Health damage requires prolonged zero-oxygen exposure inside a concentrated toxic pocket.
- Authored terrain remains stable, while deliberate sand/silt/cargo samples remain loose and simulated.
- CO2 renders near-black, hydrogen renders pink, and the enlarged UI hit rectangles match the fragment-shader controls.
""",
)

replace("shaders/chemistry.comp", "TILE_RECONSTRUCT", "TILE_STABLE")
replace(
    "shaders/chemistry.comp",
    """            if (tileHas(nearby, TILE_ACTIVE) || tileHas(nearby, TILE_COLLAPSING) ||
                tileHas(nearby, TILE_STABLE)) return false;
""",
    """            if (tileHas(nearby, TILE_ACTIVE) || tileHas(nearby, TILE_COLLAPSING)) return false;
""",
)
replace(
    "shaders/chemistry.comp",
    """    if (tileHas(tile, TILE_STABLE) && source.material == tile.material &&
        source.material != MAT_EMPTY && !isCellGas(source) && !isCellLiquid(source)) {
        // Reconstruction changes only structural state. Material, temperature,
        // age, random variation, and existing damage remain exactly represented.
        result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;
        result.aux &= ~AUX_MOVED;
    }
""",
    """    if (tileHas(tile, TILE_STABLE) && !isStructural(source) &&
        source.material == tile.material && source.material != MAT_EMPTY &&
        !isCellGas(source) && !isCellLiquid(source)) {
        // Stability qualification never creates, replaces, fills, or snaps cells.
        // It marks only the already represented pixels coherent and supported.
        // Material, temperature, age, random variation, and damage are preserved.
        result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;
        result.aux &= ~AUX_MOVED;
    }
""",
)
replace(
    "shaders/chemistry.comp",
    """    } else if (isStructural(source) && tileHas(tile, TILE_COLLAPSING) &&
               ((uint(p.x & 7) + uint(p.y & 7) * TILE_SIZE) == (pc.step & 63u))) {
""",
    """    } else if (isStructural(source) && tileHas(tile, TILE_COLLAPSING)) {
""",
)

replace("shaders/fullscreen.frag", "TILE_RECONSTRUCT", "TILE_STABLE")
replace("shaders/conservation.glsl", "CONS_RECONSTRUCTED", "CONS_STABILIZED")
replace("tools/validate_shader_contracts.py", 'chemistry.split("TILE_RECONSTRUCT", 1)', 'chemistry.split("TILE_STABLE", 1)')
replace("tools/validate_shader_contracts.py", "reconstruction resets represented damage", "stability qualification resets represented damage")

replace("README.md", "- reconstructed Terraria-style tiles", "- stabilized Terraria-style terrain cells")
replace(
    "README.md",
    "A material does not disappear because it came from a particular creation path, because provenance is missing, or because a damaged tile cannot reconstruct. Structural breakup releases represented cells into the same material simulation.",
    "A material does not disappear because it came from a particular creation path, because provenance is missing, or because a damaged region is incomplete. Structural breakup releases represented cells into the same material simulation.",
)
replace(
    "README.md",
    """## Terraria-style terrain reconstruction

The world uses 8x8 aligned terrain regions made from ordinary cells. A loose solid region may reconstruct only when all of these conditions hold:

- at least 52 of 64 cells are occupied
- occupied cells are one compatible material
- temperature and phase are stable
- cells have remained still long enough
- no active burning, melting, erosion, reaction, falling, or displacement exists
- the 120-tick stabilization period completes
- the 240-tick rebuild cooldown has expired

Mixed or incomplete regions remain loose material. Reconstruction preserves material, temperature, represented mass, and existing damage state. It does not heal cells or synthesize missing mass.

A reconstructed region sleeps when intact and inactive. Real damage below half occupancy releases remaining structural cells progressively. Missing provenance, age, or a temporary open neighbor cannot destroy a tile.
""",
    """## Terraria-style terrain stability

The world uses 8x8 aligned terrain regions made from ordinary cells. Regions never reconstruct, synthesize missing pixels, fill gaps, snap material into place, or become a second tile object. Existing cells may qualify for stability only when all of these conditions hold:

- at least 52 of 64 cells are occupied
- occupied cells are one compatible material
- temperature and phase are stable
- cells have remained still long enough
- no active burning, melting, erosion, reaction, falling, or displacement exists
- the 120-tick stability qualification completes
- the 240-tick restabilization cooldown has expired

Qualification changes only the coherence/support state of cells that already exist, allowing the region to stop falling. Empty positions remain empty. Material, temperature, represented mass, and existing damage are preserved.

Each stable terrain pixel has 255 integrity and an ordinary laser hit applies 144 damage, so every pixel requires exactly two hits to dislodge. The region remains coherent with 32 pixels left. Once fewer than 32 remain—more than half dislodged—all remaining pixels release and drop in the same simulation pass.
""",
)
replace("README.md", "- reconstruction candidates", "- stability candidates")
replace(
    "README.md",
    "Debug accounting tracks cells created at explicit world boundaries, converted, intentionally boundary-lost, rebuilt, broken, and conservation errors.",
    "Debug accounting tracks cells created at explicit world boundaries, converted, intentionally boundary-lost, stabilized, broken, and conservation errors.",
)
replace("README.md", "tile reconstruction/sleep controller", "tile stability/sleep controller")
replace(
    "README.md",
    "Each aligned tile has a separate compact controller containing material, occupancy, structural flags, stabilization time, and rebuild cooldown.",
    "Each aligned region has a compact controller containing material, occupancy, stability flags, settled time, and restabilization cooldown.",
)

workflow_path = ROOT / ".github/workflows/windows-release.yml"
workflow = workflow_path.read_text(encoding="utf-8")
apply_step = """      - name: Apply corrected terrain stability rules
        shell: pwsh
        run: |
          python tools/apply_terrain_stability_fix.py
          if ($LASTEXITCODE -ne 0) { throw 'Terrain stability correction failed.' }
          git config user.name 'github-actions[bot]'
          git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
          git add -A
          if (-not (git diff --cached --quiet)) {
              git commit -m 'Correct Terraria terrain stability and collapse rules'
              git push origin HEAD:main
              if ($LASTEXITCODE -ne 0) { throw 'Terrain stability source push failed.' }
          }
          $releaseSha = git rev-parse HEAD
          "RELEASE_SHA=$releaseSha" | Out-File -FilePath $env:GITHUB_ENV -Append

"""
if apply_step not in workflow:
    raise RuntimeError("Release workflow terrain correction step was not found")
workflow_path.write_text(workflow.replace(apply_step, ""), encoding="utf-8", newline="\n")

Path(__file__).unlink()
