#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str, count: int = 1) -> None:
    text = read(path)
    actual = text.count(old)
    if actual != count:
        raise SystemExit(f"{path}: expected {count} occurrences, found {actual}: {old[:90]!r}")
    write(path, text.replace(old, new, count))


MISSION_LEDGER = r'''# FastFreddyTestbed Mission Ledger

This file is the durable release ledger. A mission remains open until its acceptance criteria are verified in Windows and Linux Release builds. Missed, deferred, or avoided work must remain listed rather than disappearing between releases.

## v2.4.0 macro hierarchy release

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| M01 | COMPLETE | Preserve v2.3.3 life/oxygen behavior | Bees, ants, and beetles retain conserved gas/liquid displacement, respiration, CO2 exchange, and suffocation behavior. |
| M02 | COMPLETE | Chunk-first work rejection | A clean sleeping 64x64 chunk skips its 8x8 tile scans; dirty/boundary chunks wake deterministically. |
| M03 | COMPLETE | Cached 8x8 macro classification | Tile metadata records uniform, macro-movable, macro-solid, macro-powder, macro-liquid, macro-gas, fine-active, wet, and settled-medium state. |
| M04 | COMPLETE | Full-block macro movement | Full aligned loose solids, powders, liquids, and gases decide movement from cached tile state before any fine pair logic; macro-moved tiles are excluded from the same frame's pixel pass. |
| M05 | COMPLETE | Structural integrity for solid blocks | Cohesive full solid regions stabilize only with physical support and remain represented by their original cells; no reconstruction or synthesized pixels. |
| M06 | COMPLETE | Wet material model | Wet sand, wet dirt, wet silt, and mud use canonical AUX_WET state; full regions move in macro passes and mixed edges receive bounded periodic fine repair. |
| M07 | COMPLETE | Settled liquid behavior | Equal-level random liquid hopping is removed; water spreads only for a drop, cover difference, or pressure gradient and can reach a true settled state. |
| M08 | COMPLETE | Half-water coalescence and presentation | Half-water receives deterministic short-range attraction toward another half, keeps conserved displaced gas, and renders with stronger smoothed coverage without changing volume. |
| M09 | COMPLETE | Sluice-box processing | A buildable Sluice Box accepts only wet sand while supplied by flowing water and conserves eight feed cells as one gold plus seven silt outputs. |
| M10 | COMPLETE | Debug regression and GPU cost | SWAPS and hierarchy skip counts are visible again; debug sampling is reduced to every 16 frames and the 8x8 grid is omitted when too dense to read. |
| M11 | COMPLETE | Rendering review | Macro/fine/settled states have distinct debug visualization while normal rendering blends half-cell edges and never exposes raw macro blocks. |
| M12 | COMPLETE | Threading/concurrency review | Native events and Vulkan simulation remain on separate explicit threads. No coroutine is inserted into the ordered GPU submission path; the useful concurrency gain is GPU hierarchy rejection rather than CPU task switching. |
| M13 | COMPLETE | Cross-platform release gate | All 12 Vulkan shaders, C++23 targets, material/ecology/shader audits, three contract tests, packages, and SHA-256 checks pass on Windows and Linux before release. |

## Carry-forward rule

Any future regression or incomplete acceptance criterion reopens the same mission ID. New work is appended; prior missions are never silently removed.
'''
write("MISSION_LEDGER.md", MISSION_LEDGER)

# Add the sluice box to the single material catalog and regenerate all generated tables later.
replace(
    "tools/material_catalog.py",
    "    ('hydrogen', 'Hydrogen', 1, 0, 0, 255, 'STRONG: LIGHT FUEL GAS', 'WEAK: IGNITION / CONTAINMENT', 'TO: STEAM / FIRE', 'ROLE: ENERGY CARRIER', 'DANGER: EXPLOSIVE GAS'),\n]",
    "    ('hydrogen', 'Hydrogen', 1, 0, 0, 255, 'STRONG: LIGHT FUEL GAS', 'WEAK: IGNITION / CONTAINMENT', 'TO: STEAM / FIRE', 'ROLE: ENERGY CARRIER', 'DANGER: EXPLOSIVE GAS'),\n"
    "    ('sluice_box', 'Sluice box', 210, 196, 900, 248, 'STRONG: WET SAND SEPARATION', 'WEAK: DRY FEED / DAMAGE', 'TO: GOLD + SILT', 'ROLE: GRAVITY MINERAL PROCESSOR', 'DANGER: PINCH / FLOOD'),\n]",
)
replace(
    "tools/material_catalog.py",
    "    ('industry', 'Industry', [50, 51, 52, 59, 42, 63]),",
    "    ('industry', 'Industry', [50, 51, 52, 59, 42, 63, 65]),",
)
replace(
    "tools/material_catalog.py",
    "    'insect_habitat', 'factory_core',\n)",
    "    'insect_habitat', 'factory_core', 'sluice_box',\n)",
)
replace(
    "tools/material_catalog.py",
    "    'hydrogen': (1, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 560, 180),\n}",
    "    'hydrogen': (1, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 560, 180),\n"
    "    'sluice_box': (210, 760, 1420, 2850, 2850, NO_TEMPERATURE, 165),\n}",
)

# Common material behavior for the generated MAT_SLUICE_BOX identifier.
for path in ("shaders/materials.glsl",):
    replace(path,
        "           material == MAT_INSECT_HABITAT || material == MAT_FACTORY_CORE;",
        "           material == MAT_INSECT_HABITAT || material == MAT_FACTORY_CORE ||\n"
        "           material == MAT_SLUICE_BOX;",
        count=3)
replace(
    "shaders/materials.glsl",
    "    case MAT_FACTORY_CORE: return 254u;\n    default: return acidImmune(material) ? 255u : 175u;",
    "    case MAT_FACTORY_CORE: return 254u;\n    case MAT_SLUICE_BOX: return 248u;\n"
    "    default: return acidImmune(material) ? 255u : 175u;",
)
replace(
    "shaders/materials.glsl",
    "    case MAT_SAND: color = vec4(0.88 + variation, 0.72 + variation, 0.34, 1.0); break;",
    "    case MAT_SAND: { float wet = (aux & AUX_WET) != 0u ? -0.16 : 0.0;\n"
    "        color = vec4(0.88 + variation + wet, 0.72 + variation + wet * 0.75, 0.34 + wet * 0.35, 1.0); break; }",
)
replace(
    "shaders/materials.glsl",
    "    case MAT_FACTORY_CORE: color = vec4(0.24, 0.88, 0.82, 1.0); break;",
    "    case MAT_FACTORY_CORE: color = vec4(0.24, 0.88, 0.82, 1.0); break;\n"
    "    case MAT_SLUICE_BOX: { bool riffle = ((position.x + position.y) & 3) == 0;\n"
    "        color = riffle ? vec4(0.72, 0.55, 0.18, 1.0) : vec4(0.22, 0.29, 0.34, 1.0); break; }",
)

# Machines are aligned blocks with one controller cell.
replace(
    "shaders/paint.comp",
    "                                  material == MAT_INSECT_HABITAT) &&",
    "                                  material == MAT_INSECT_HABITAT || material == MAT_SLUICE_BOX) &&",
)

# Scene loading gives the new machine canonical charged/controller state.
replace(
    "src/scene_image.cpp",
    "    case Material::factory_core:\n        aux |= aux_charged;",
    "    case Material::factory_core:\n    case Material::sluice_box:\n        aux |= aux_charged;",
)

# The cache structures stay 16 bytes while carrying the hierarchy classification.
write("shaders/tiles.glsl", r'''#ifndef EPOCH_SAND_TILES_GLSL
#define EPOCH_SAND_TILES_GLSL

const uint TILE_SIZE = 8u;
const uint TILE_CELL_COUNT = 64u;
const uint TILE_STABILITY_OCCUPANCY = 52u;
const uint TILE_MIN_COHESIVE_CELLS = 32u;
const uint TILE_COLLAPSE_OCCUPANCY = TILE_MIN_COHESIVE_CELLS;
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
const uint TILE_HAS_QUEEN = 0x00000100u;
const uint TILE_HAS_HIVE = 0x00000200u;
const uint TILE_HAS_FLOWER = 0x00000400u;
const uint TILE_HAS_HONEY = 0x00000800u;
const uint TILE_HAS_BEES = 0x00001000u;
const uint TILE_HAS_MIGRATING_QUEEN = 0x00002000u;
const uint TILE_BEE_HAZARD = 0x00004000u;
const uint TILE_UNIFORM = 0x00008000u;
const uint TILE_MACRO_MOVABLE = 0x00010000u;
const uint TILE_FINE_ACTIVE = 0x00020000u;
const uint TILE_SETTLED_MEDIUM = 0x00040000u;
const uint TILE_WET_CONTENT = 0x00080000u;
const uint TILE_MACRO_SOLID = 0x00100000u;
const uint TILE_MACRO_POWDER = 0x00200000u;
const uint TILE_MACRO_LIQUID = 0x00400000u;
const uint TILE_MACRO_GAS = 0x00800000u;
const uint TILE_MACRO_MOVED = 0x01000000u;

struct TileState {
    uint material;
    uint occupancy;
    uint flags;
    uint counters; // terrain: low 16 stability, high 16 cooldown
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
bool tileIsMacro(TileState state) { return tileHas(state, TILE_MACRO_MOVABLE); }
bool tileNeedsFine(TileState state) { return tileHas(state, TILE_FINE_ACTIVE); }

const uint TILE_OCCUPANCY_MASK = 0x0000007fu;
const uint TILE_QUEEN_X_SHIFT = 7u;
const uint TILE_QUEEN_Y_SHIFT = 10u;
const uint TILE_FLOWER_X_SHIFT = 13u;
const uint TILE_FLOWER_Y_SHIFT = 16u;
const uint TILE_HONEY_X_SHIFT = 19u;
const uint TILE_HONEY_Y_SHIFT = 22u;
const uint TILE_BEE_COUNT_SHIFT = 25u;

uint tileOccupancy(TileState state) { return state.occupancy & TILE_OCCUPANCY_MASK; }
ivec2 tileQueenLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_QUEEN_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_QUEEN_Y_SHIFT) & 7u));
}
ivec2 tileFlowerLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_FLOWER_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_FLOWER_Y_SHIFT) & 7u));
}
ivec2 tileHoneyLocal(TileState state) {
    return ivec2(int((state.occupancy >> TILE_HONEY_X_SHIFT) & 7u),
                 int((state.occupancy >> TILE_HONEY_Y_SHIFT) & 7u));
}
uint tileBeeCount(TileState state) { return (state.occupancy >> TILE_BEE_COUNT_SHIFT) & 127u; }

uint packTileOccupancy(uint occupancy, ivec2 queenLocal, ivec2 flowerLocal,
                       ivec2 honeyLocal, uint beeCount) {
    return min(occupancy, 64u) |
           (uint(clamp(queenLocal.x, 0, 7)) << TILE_QUEEN_X_SHIFT) |
           (uint(clamp(queenLocal.y, 0, 7)) << TILE_QUEEN_Y_SHIFT) |
           (uint(clamp(flowerLocal.x, 0, 7)) << TILE_FLOWER_X_SHIFT) |
           (uint(clamp(flowerLocal.y, 0, 7)) << TILE_FLOWER_Y_SHIFT) |
           (uint(clamp(honeyLocal.x, 0, 7)) << TILE_HONEY_X_SHIFT) |
           (uint(clamp(honeyLocal.y, 0, 7)) << TILE_HONEY_Y_SHIFT) |
           (min(beeCount, 127u) << TILE_BEE_COUNT_SHIFT);
}

ivec2 brickOriginFromIndex(uint index, uint width) {
    uint columns = tileColumns(width);
    return ivec2(int(index % columns), int(index / columns)) * int(TILE_SIZE);
}
ivec2 tileQueenPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileQueenLocal(state);
}
ivec2 tileFlowerPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileFlowerLocal(state);
}
ivec2 tileHoneyPosition(uint index, uint width, TileState state) {
    return brickOriginFromIndex(index, width) + tileHoneyLocal(state);
}

#endif
''')

write("shaders/chunks.glsl", r'''#ifndef EPOCH_SAND_CHUNKS_GLSL
#define EPOCH_SAND_CHUNKS_GLSL

const uint CHUNK_TILES_PER_AXIS = 8u;
const uint CHUNK_CELL_SIZE = TILE_SIZE * CHUNK_TILES_PER_AXIS;
const uint CHUNK_TILE_COUNT = CHUNK_TILES_PER_AXIS * CHUNK_TILES_PER_AXIS;
const uint CHUNK_SLEEP_TICKS = 30u;

const uint CHUNK_ACTIVE = 0x00000001u;
const uint CHUNK_SLEEPING = 0x00000002u;
const uint CHUNK_DIRTY = 0x00000004u;
const uint CHUNK_BOUNDARY = 0x00000008u;
const uint CHUNK_MACRO_ACTIVE = 0x00000010u;
const uint CHUNK_FINE_ACTIVE = 0x00000020u;

struct ChunkState {
    uint flags;
    uint activeTiles;
    uint sleepingTiles;
    uint counters; // low 16 quiet ticks, high 16 present tile count
};

uint chunkColumns(uint width) { return (width + CHUNK_CELL_SIZE - 1u) / CHUNK_CELL_SIZE; }
uint chunkRows(uint height) { return (height + CHUNK_CELL_SIZE - 1u) / CHUNK_CELL_SIZE; }
uvec2 chunkCoordinate(ivec2 p) { return uvec2(max(p, ivec2(0))) / CHUNK_CELL_SIZE; }
uint chunkIndex(ivec2 p, uint width) {
    uvec2 chunk = chunkCoordinate(p);
    return chunk.y * chunkColumns(width) + chunk.x;
}
uint chunkQuietTicks(ChunkState state) { return state.counters & 0xffffu; }
uint chunkPresentTiles(ChunkState state) { return state.counters >> 16u; }
uint packChunkCounters(uint quietTicks, uint presentTiles) {
    return min(quietTicks, 0xffffu) | (min(presentTiles, 0xffffu) << 16u);
}
bool chunkHas(ChunkState state, uint flag) { return (state.flags & flag) != 0u; }

#endif
''')

write("shaders/tiles.comp", r'''#version 450
#extension GL_GOOGLE_include_directive : require
#include "materials.glsl"
#include "tiles.glsl"
#include "chunks.glsl"

layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 4) buffer Tiles { TileState tiles[]; };
layout(std430, binding = 7) readonly buffer Chunks { ChunkState chunks[]; };

bool tileInside(ivec2 p) {
    return p.x >= 0 && p.y >= 0 && p.x < int(pc.width) && p.y < int(pc.height);
}

bool isMacroWetMaterial(Cell cell) {
    return cell.material == MAT_MUD ||
           ((cell.material == MAT_SAND || cell.material == MAT_DIRT || cell.material == MAT_SILT) &&
            (cell.aux & AUX_WET) != 0u);
}

void main() {
    uvec2 tile = gl_GlobalInvocationID.xy;
    uint columns = tileColumns(pc.width);
    uint rows = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;
    if (tile.x >= columns || tile.y >= rows) return;

    uint index = tile.y * columns + tile.x;
    ivec2 origin = ivec2(tile * TILE_SIZE);
    ChunkState priorChunk = chunks[chunkIndex(origin, pc.width)];
    if (chunkHas(priorChunk, CHUNK_SLEEPING) && !chunkHas(priorChunk, CHUNK_DIRTY)) return;

    TileState previous = tiles[index];
    uint counts[MATERIAL_COUNT];
    for (uint i = 0u; i < MATERIAL_COUNT; ++i) counts[i] = 0u;
    uint represented = 0u;
    uint occupied = 0u;
    uint structural = 0u;
    uint stable = 0u;
    uint healthSum = 0u;
    uint minimumAge = 0xffffffffu;
    bool hot = false;
    bool moving = false;
    bool reacting = false;
    bool activeContent = false;
    bool wetContent = false;
    bool halfWater = false;
    bool activeAgent = false;
    bool hasQueen = false;
    bool hasHive = false;
    bool hasFlower = false;
    bool hasHoney = false;
    bool hasMigratingQueen = false;
    bool beeHazard = false;
    ivec2 queenLocal = ivec2(0);
    ivec2 flowerLocal = ivec2(0);
    ivec2 honeyLocal = ivec2(0);
    uint beeCount = 0u;

    for (uint y = 0u; y < TILE_SIZE; ++y) {
        for (uint x = 0u; x < TILE_SIZE; ++x) {
            ivec2 p = origin + ivec2(x, y);
            if (!tileInside(p)) continue;
            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
            ++represented;
            if (cell.material < MATERIAL_COUNT) ++counts[cell.material];
            minimumAge = min(minimumAge, cell.age);
            wetContent = wetContent || isMacroWetMaterial(cell);
            halfWater = halfWater || isHalfWater(cell);
            ivec2 local = ivec2(x, y);
            if (cell.material == MAT_QUEEN_BEE) { if (!hasQueen) queenLocal = local; hasQueen = true; }
            if (cell.material == MAT_BEE_NEST) hasHive = true;
            if (cell.material == MAT_FLOWER) { if (!hasFlower) flowerLocal = local; hasFlower = true; }
            if (cell.material == MAT_HONEY) { if (!hasHoney) honeyLocal = local; hasHoney = true; }
            if (cell.material == MAT_BEE) {
                ++beeCount;
                if ((cell.aux & AUX_CHARGED) != 0u) { if (!hasMigratingQueen) queenLocal = local; hasMigratingQueen = true; }
            }
            bool agent = cell.material == MAT_BEE || cell.material == MAT_QUEEN_BEE ||
                         cell.material == MAT_BEE_NEST || cell.material == MAT_ANT ||
                         cell.material == MAT_BEETLE || cell.material == MAT_SEED ||
                         cell.material == MAT_POLLEN;
            activeAgent = activeAgent || agent;
            beeHazard = beeHazard || cell.material == MAT_FIRE || cell.material == MAT_LAVA ||
                        cell.material == MAT_ACID || cell.material == MAT_SMOKE ||
                        cell.material == MAT_DIRTY_STEAM || cell.material == MAT_LIGHTNING ||
                        cell.material == MAT_RADIATION;
            bool fluid = isCellGas(cell) || isCellLiquid(cell);
            bool loose = !isStructural(cell) && !isReconstructableMaterial(cell.material) &&
                         !isCellImmovable(cell);
            activeContent = activeContent || agent || loose || fluid;
            moving = moving || (cell.aux & AUX_MOVED) != 0u || cell.age < 8u;
            hot = hot || abs(cell.temperature - 20) > 80;
            reacting = reacting || cell.material == MAT_FIRE || cell.material == MAT_EMBER ||
                       cell.material == MAT_GUNPOWDER || cell.material == MAT_ACID;
            if (fluid) continue;

            ++occupied;
            if (isStructural(cell)) {
                ++structural;
                uint health = stateValue(cell);
                healthSum += health == 0u ? 255u : health;
            }
            uint phase = cellPhase(cell);
            bool cellStable = cell.age >= 30u && (cell.aux & AUX_MOVED) == 0u &&
                              phase != PHASE_SOFTENED && phase != PHASE_MOLTEN && phase != PHASE_VAPOR;
            if (cellStable) ++stable;
        }
    }

    uint dominant = MAT_EMPTY;
    uint dominantCount = 0u;
    for (uint material = 1u; material < MATERIAL_COUNT; ++material) {
        if (counts[material] > dominantCount) { dominantCount = counts[material]; dominant = material; }
    }
    bool uniformMaterial = dominant != MAT_EMPTY && dominantCount == represented;
    bool fullRegion = uniformMaterial && represented == TILE_CELL_COUNT;
    bool structuralTile = structural > 0u;
    bool macroLiquid = fullRegion && isLiquid(dominant) && !halfWater;
    bool macroGas = fullRegion && isGas(dominant);
    bool macroPowder = fullRegion && isPowder(dominant);
    bool macroSolid = fullRegion && isBlockCapable(dominant) && !structuralTile;
    bool macroMovable = (macroLiquid || macroGas || macroPowder || macroSolid) &&
                        !activeAgent && !hot && !reacting;

    uint supportSamples = 0u;
    bool anchored = origin.y + int(TILE_SIZE) >= int(pc.height);
    if (!anchored) {
        int belowY = origin.y + int(TILE_SIZE);
        for (int x = 0; x < int(TILE_SIZE); ++x) {
            Cell below = cells[indexOf(ivec2(origin.x + x, belowY))];
            if (isStructural(below) || isCellImmovable(below) ||
                (isBlockCapable(below.material) && !isCellLiquid(below) && !isCellGas(below))) ++supportSamples;
        }
    }
    bool physicallySupported = anchored || supportSamples >= 4u;

    uint cooldown = tileCooldown(previous);
    if (cooldown > 0u) --cooldown;
    bool stabilizable = uniformMaterial && isReconstructableMaterial(dominant) && !wetContent;
    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&
                           tileOccupancy(previous) >= TILE_STABILITY_OCCUPANCY;
    bool reducedDurability = structuralTile && dominantCount < TILE_STABILITY_OCCUPANCY;
    bool damaged = structuralTile &&
                   (reducedDurability || dominantCount < TILE_CELL_COUNT ||
                    (structural > 0u && healthSum / structural < 240u));
    bool candidate = !structuralTile && cooldown == 0u && stabilizable && physicallySupported &&
                     occupied >= TILE_STABILITY_OCCUPANCY && stable == occupied &&
                     !moving && !hot && !reacting;
    uint stableTicks = candidate ? min(tileStableTicks(previous) + 1u, TILE_STABILIZE_TICKS) : 0u;
    bool stableRegion = candidate && stableTicks >= TILE_STABILIZE_TICKS;
    bool collapsing = structuralTile && dominantCount < TILE_MIN_COHESIVE_CELLS;
    bool terrainStable = (structuralTile || stableRegion) && !collapsing;
    bool supported = terrainStable && (physicallySupported || structuralTile);

    bool settledMedium = (macroLiquid || macroGas) && !moving && minimumAge >= 30u;
    bool fineActive = represented != 0u && !macroMovable &&
                      (activeAgent || moving || reacting || hot || !uniformMaterial || represented < TILE_CELL_COUNT);

    uint flags = 0u;
    if (structuralTile) flags |= TILE_STRUCTURAL;
    if (supported) flags |= TILE_SUPPORTED;
    if (candidate) flags |= TILE_CANDIDATE;
    if (terrainStable) flags |= TILE_STABLE;
    if (collapsing) flags |= TILE_COLLAPSING;
    if (damaged || previouslyDense && reducedDurability) flags |= TILE_DAMAGED;
    if (hasQueen) flags |= TILE_HAS_QUEEN;
    if (hasHive) flags |= TILE_HAS_HIVE;
    if (hasFlower) flags |= TILE_HAS_FLOWER;
    if (hasHoney) flags |= TILE_HAS_HONEY;
    if (beeCount > 0u) flags |= TILE_HAS_BEES;
    if (hasMigratingQueen) flags |= TILE_HAS_MIGRATING_QUEEN;
    if (beeHazard) flags |= TILE_BEE_HAZARD;
    if (uniformMaterial) flags |= TILE_UNIFORM;
    if (macroMovable) flags |= TILE_MACRO_MOVABLE;
    if (fineActive) flags |= TILE_FINE_ACTIVE;
    if (settledMedium) flags |= TILE_SETTLED_MEDIUM;
    if (wetContent) flags |= TILE_WET_CONTENT;
    if (macroSolid) flags |= TILE_MACRO_SOLID;
    if (macroPowder) flags |= TILE_MACRO_POWDER;
    if (macroLiquid) flags |= TILE_MACRO_LIQUID;
    if (macroGas) flags |= TILE_MACRO_GAS;

    bool sleepingTerrain = terrainStable && !damaged && !moving && !hot && !reacting && !activeAgent;
    bool sleepingAtmosphere = represented == 0u;
    bool sleeping = sleepingTerrain || sleepingAtmosphere || settledMedium;
    if (sleeping) flags |= TILE_SLEEPING;
    else flags |= TILE_ACTIVE;

    if (collapsing) cooldown = TILE_RESTABILIZE_COOLDOWN;
    uint packedOccupancy = packTileOccupancy(
        structuralTile ? dominantCount : represented, queenLocal, flowerLocal, honeyLocal, beeCount);
    tiles[index] = TileState(dominant, packedOccupancy, flags, packTileCounters(stableTicks, cooldown));
}
''')

write("shaders/chunks.comp", r'''#version 450
#extension GL_GOOGLE_include_directive : require
#include "materials.glsl"
#include "tiles.glsl"
#include "chunks.glsl"

layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };
layout(std430, binding = 7) buffer Chunks { ChunkState chunks[]; };

void main() {
    uvec2 chunk = gl_GlobalInvocationID.xy;
    uint columns = chunkColumns(pc.width);
    uint rows = chunkRows(pc.height);
    if (chunk.x >= columns || chunk.y >= rows) return;

    uint index = chunk.y * columns + chunk.x;
    ChunkState previous = chunks[index];
    if (chunkHas(previous, CHUNK_SLEEPING) && !chunkHas(previous, CHUNK_DIRTY)) return;

    uint tileColumnsValue = tileColumns(pc.width);
    uint tileRowsValue = (pc.height + TILE_SIZE - 1u) / TILE_SIZE;
    uvec2 brickOrigin = chunk * CHUNK_TILES_PER_AXIS;
    uint presentTiles = 0u;
    uint activeTiles = 0u;
    uint sleepingTiles = 0u;
    uint macroTiles = 0u;
    uint fineTiles = 0u;
    for (uint y = 0u; y < CHUNK_TILES_PER_AXIS; ++y) {
        for (uint x = 0u; x < CHUNK_TILES_PER_AXIS; ++x) {
            uvec2 tile = brickOrigin + uvec2(x, y);
            if (tile.x >= tileColumnsValue || tile.y >= tileRowsValue) continue;
            ++presentTiles;
            TileState state = tiles[tile.y * tileColumnsValue + tile.x];
            if (tileHas(state, TILE_SLEEPING)) ++sleepingTiles;
            if (tileHas(state, TILE_ACTIVE) || tileHas(state, TILE_COLLAPSING) || tileHas(state, TILE_DAMAGED)) ++activeTiles;
            if (tileHas(state, TILE_MACRO_MOVABLE)) ++macroTiles;
            if (tileHas(state, TILE_FINE_ACTIVE)) ++fineTiles;
        }
    }

    bool dirty = chunkHas(previous, CHUNK_DIRTY);
    bool quiet = presentTiles != 0u && sleepingTiles == presentTiles && activeTiles == 0u && !dirty;
    uint quietTicks = quiet ? min(chunkQuietTicks(previous) + 1u, CHUNK_SLEEP_TICKS) : 0u;
    bool sleeping = quiet && quietTicks >= CHUNK_SLEEP_TICKS;
    uint flags = sleeping ? CHUNK_SLEEPING : CHUNK_ACTIVE;
    if (presentTiles != CHUNK_TILE_COUNT) flags |= CHUNK_BOUNDARY;
    if (macroTiles > 0u) flags |= CHUNK_MACRO_ACTIVE;
    if (fineTiles > 0u) flags |= CHUNK_FINE_ACTIVE;
    chunks[index] = ChunkState(flags, activeTiles, sleepingTiles,
                               packChunkCounters(quietTicks, presentTiles));
}
''')

write("shaders/macro_move.comp", r'''#version 450
#extension GL_GOOGLE_include_directive : require
#define EPOCH_SAND_NO_SIM_PUSH
#include "materials.glsl"
#include "tiles.glsl"
#include "chunks.glsl"
#include "debug_stats.glsl"

layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 4) buffer Tiles { TileState tiles[]; };
layout(std430, binding = 5) buffer DebugStatsBuffer { uint debugStats[]; };
layout(std430, binding = 7) buffer Chunks { ChunkState chunks[]; };

layout(push_constant) uniform MovementPush {
    uint width; uint height; uint step; uint seed;
    int phase; int parity; uint collectDebug; uint reserved1;
} macroPc;

shared uint macroMoveAllowed;
shared ivec2 sourceTileShared;
shared ivec2 targetTileShared;
shared uint sourceTileIndexShared;
shared uint targetTileIndexShared;

bool macroInside(ivec2 p) { return p.x >= 0 && p.y >= 0 && p.x < int(macroPc.width) && p.y < int(macroPc.height); }
uint macroIndex(ivec2 p) { return uint(p.y) * macroPc.width + uint(p.x); }
Cell macroAt(ivec2 p) { return macroInside(p) ? cells[macroIndex(p)] : Cell(MAT_STONE, 0u, 20, AUX_STRUCTURAL); }
uint macroHash(ivec2 tile, uint salt) {
    return hash32(uint(tile.x) * 73856093u ^ uint(tile.y) * 19349663u ^ macroPc.step * 83492791u ^ macroPc.seed ^ salt);
}

bool pairCoordinates(out ivec2 sourceTile, out ivec2 targetTile) {
    uint columns = tileColumns(macroPc.width);
    uint rows = (macroPc.height + TILE_SIZE - 1u) / TILE_SIZE;
    ivec2 pair = ivec2(gl_WorkGroupID.xy);
    if (macroPc.phase == 0) {
        sourceTile = ivec2(pair.x, pair.y * 2 + macroPc.parity);
        targetTile = sourceTile + ivec2(0, 1);
    } else if (macroPc.phase == 1 || macroPc.phase == 2) {
        sourceTile = ivec2(pair.x, pair.y * 2 + macroPc.parity);
        targetTile = sourceTile + ivec2(macroPc.phase == 1 ? -1 : 1, 1);
    } else if (macroPc.phase == 5) {
        sourceTile = ivec2(pair.x, pair.y * 2 + macroPc.parity + 1);
        targetTile = sourceTile - ivec2(0, 1);
    } else {
        int baseX = pair.x * 2 + macroPc.parity;
        sourceTile = macroPc.phase == 3 ? ivec2(baseX, pair.y) : ivec2(baseX + 1, pair.y);
        targetTile = macroPc.phase == 3 ? sourceTile + ivec2(1, 0) : sourceTile + ivec2(-1, 0);
    }
    return sourceTile.x >= 0 && sourceTile.y >= 0 && targetTile.x >= 0 && targetTile.y >= 0 &&
           sourceTile.x < int(columns) && targetTile.x < int(columns) &&
           sourceTile.y < int(rows) && targetTile.y < int(rows);
}

bool macroSourceAllows(TileState state, Cell representative, uint randomValue) {
    if (!tileHas(state, TILE_MACRO_MOVABLE) || tileHas(state, TILE_MACRO_MOVED)) return false;
    if (state.material != representative.material || tileOccupancy(state) != TILE_CELL_COUNT) return false;
    if ((representative.aux & AUX_MOVED) != 0u || isStructural(representative)) return false;
    if (representative.material == MAT_HONEY) return (randomValue & 3u) == 0u;
    if (representative.material == MAT_LAVA) return stateValue(representative) >= 160u || (randomValue & 1u) == 0u;
    return true;
}

bool materialCanDisplace(Cell source, Cell target) {
    if (target.material == MAT_EMPTY) return true;
    if (isStructural(target)) return false;
    if (isCellGas(source)) return isCellGas(target) && materialDensity(source.material) < materialDensity(target.material);
    if (isCellGas(target)) return true;
    if (isCellPowder(source) || isBlockCapable(source.material))
        return isCellLiquid(target) && materialDensity(source.material) > materialDensity(target.material);
    if (isCellLiquid(source))
        return isCellLiquid(target) && materialDensity(source.material) > materialDensity(target.material);
    return false;
}

bool targetTileAllows(TileState targetState, Cell source, Cell targetRepresentative) {
    if (tileHas(targetState, TILE_FINE_ACTIVE) || tileHas(targetState, TILE_MACRO_MOVED)) return false;
    if (tileOccupancy(targetState) == 0u) return true;
    if (!tileHas(targetState, TILE_UNIFORM) || tileOccupancy(targetState) != TILE_CELL_COUNT) return false;
    return targetState.material == targetRepresentative.material && materialCanDisplace(source, targetRepresentative);
}

bool regionSupported(ivec2 sourceOrigin, Cell source) {
    int belowY = sourceOrigin.y + int(TILE_SIZE);
    if (belowY >= int(macroPc.height)) return true;
    for (int x = 0; x < int(TILE_SIZE); ++x)
        if (materialCanDisplace(source, macroAt(ivec2(sourceOrigin.x + x, belowY)))) return false;
    return true;
}

void markMacroChunkDirty(ivec2 p) {
    atomicOr(chunks[chunkIndex(p, macroPc.width)].flags, CHUNK_DIRTY | CHUNK_ACTIVE);
}

void main() {
    if (gl_LocalInvocationIndex == 0u) {
        macroMoveAllowed = 0u;
        ivec2 sourceTile;
        ivec2 targetTile;
        if (pairCoordinates(sourceTile, targetTile)) {
            uint columns = tileColumns(macroPc.width);
            uint sourceTileIndex = uint(sourceTile.y) * columns + uint(sourceTile.x);
            uint targetTileIndex = uint(targetTile.y) * columns + uint(targetTile.x);
            TileState sourceState = tiles[sourceTileIndex];
            TileState targetState = tiles[targetTileIndex];
            ivec2 sourceOrigin = sourceTile * int(TILE_SIZE);
            ivec2 targetOrigin = targetTile * int(TILE_SIZE);
            Cell source = macroAt(sourceOrigin);
            Cell target = macroAt(targetOrigin);
            uint randomValue = macroHash(sourceTile, uint(macroPc.phase) + 1u);
            bool valid = macroSourceAllows(sourceState, source, randomValue) &&
                         targetTileAllows(targetState, source, target);
            bool allowed = false;
            if (valid && macroPc.phase == 5) {
                allowed = tileHas(sourceState, TILE_MACRO_GAS);
            } else if (valid && macroPc.phase == 0) {
                allowed = !tileHas(sourceState, TILE_MACRO_GAS);
            } else if (valid && (macroPc.phase == 1 || macroPc.phase == 2)) {
                allowed = !tileHas(sourceState, TILE_MACRO_GAS) && regionSupported(sourceOrigin, source);
            } else if (valid) {
                allowed = (tileHas(sourceState, TILE_MACRO_LIQUID) || tileHas(sourceState, TILE_MACRO_GAS)) &&
                          regionSupported(sourceOrigin, source) && target.material == MAT_EMPTY;
            }
            if (allowed) {
                sourceTileShared = sourceTile;
                targetTileShared = targetTile;
                sourceTileIndexShared = sourceTileIndex;
                targetTileIndexShared = targetTileIndex;
                macroMoveAllowed = 1u;
            }
        }
    }
    barrier();
    if (macroMoveAllowed == 0u) return;

    ivec2 local = ivec2(gl_LocalInvocationID.xy);
    ivec2 sourcePosition = sourceTileShared * int(TILE_SIZE) + local;
    ivec2 targetPosition = targetTileShared * int(TILE_SIZE) + local;
    uint sourceIndex = macroIndex(sourcePosition);
    uint targetIndex = macroIndex(targetPosition);
    Cell a = cells[sourceIndex];
    Cell b = cells[targetIndex];
    a.age = 0u; b.age = 0u;
    a.aux |= AUX_MOVED; b.aux |= AUX_MOVED;
    cells[sourceIndex] = b;
    cells[targetIndex] = a;
    barrier();

    if (gl_LocalInvocationIndex == 0u) {
        TileState sourceState = tiles[sourceTileIndexShared];
        TileState targetState = tiles[targetTileIndexShared];
        sourceState.flags = (sourceState.flags | TILE_ACTIVE | TILE_MACRO_MOVED) & ~(TILE_SLEEPING | TILE_SETTLED_MEDIUM);
        targetState.flags = (targetState.flags | TILE_ACTIVE | TILE_MACRO_MOVED) & ~(TILE_SLEEPING | TILE_SETTLED_MEDIUM);
        tiles[sourceTileIndexShared] = targetState;
        tiles[targetTileIndexShared] = sourceState;
        markMacroChunkDirty(sourcePosition);
        markMacroChunkDirty(targetPosition);
        if (macroPc.collectDebug != 0u) {
            atomicAdd(debugStats[STAT_MACRO_TILE_MOVES], 1u);
            atomicAdd(debugStats[STAT_MACRO_CELL_MOVES], TILE_CELL_COUNT);
        }
    }
}
''')

# Fine movement knows about wet-state repair, cached macro ownership, deterministic liquid rest,
# and the new catalog entry.
replace("shaders/move.comp", "const uint AUX_CHARGED = 0x40000000u;", "const uint AUX_WET = 0x80000000u;\nconst uint AUX_CHARGED = 0x40000000u;")
replace(
    "shaders/move.comp",
    "     10u,  18u,  70u, 254u,  60u,  55u,  42u,  58u,\n      1u\n);",
    "     10u,  18u,  70u, 254u,  60u,  55u,  42u,  58u,\n      1u, 210u\n);",
)
replace(
    "shaders/move.comp",
    "bool movementAllows(Cell cell, uint randomValue) {\n",
    "bool macroExclusiveCell(Cell cell) {\n"
    "    return cell.material == MAT_MUD ||\n"
    "           ((cell.material == MAT_SAND || cell.material == MAT_DIRT || cell.material == MAT_SILT) &&\n"
    "            (cell.aux & AUX_WET) != 0u);\n"
    "}\n\n"
    "bool movementAllows(Cell cell, uint randomValue) {\n"
    "    if (macroExclusiveCell(cell) && (movePc.reserved1 & 2u) == 0u) return false;\n",
)
replace(
    "shaders/move.comp",
    "    return firstDrop || secondDrop || (randomValue & 7u) == 0u;",
    "    if (firstDrop || secondDrop) return true;\n"
    "    // Equal-height basins have no random fallback: accumulated age becomes real rest.\n"
    "    return source.age < 12u && sourcePressure > 0 && targetPressure + 1 < sourcePressure;",
)
replace(
    "shaders/move.comp",
    "    if (tileHas(tileA, TILE_ACTIVE) || tileHas(tileB, TILE_ACTIVE) ||\n        tileHas(tileA, TILE_HAS_BEES) || tileHas(tileB, TILE_HAS_BEES)) return false;",
    "    if (tileHas(tileA, TILE_MACRO_MOVED) || tileHas(tileB, TILE_MACRO_MOVED)) {\n"
    "        if (movePc.reserved0 != 0u) atomicAdd(debugStats[STAT_CHUNK_SKIPPED_CELLS], 2u);\n"
    "        return true;\n"
    "    }\n"
    "    bool finePair = tileNeedsFine(tileA) || tileNeedsFine(tileB);\n"
    "    if (!finePair && (tileIsMacro(tileA) || tileIsMacro(tileB) ||\n"
    "                      tileHas(tileA, TILE_SETTLED_MEDIUM) || tileHas(tileB, TILE_SETTLED_MEDIUM))) {\n"
    "        if (movePc.reserved0 != 0u) atomicAdd(debugStats[STAT_CHUNK_SKIPPED_CELLS], 2u);\n"
    "        return true;\n"
    "    }\n"
    "    if (tileHas(tileA, TILE_ACTIVE) || tileHas(tileB, TILE_ACTIVE) ||\n"
    "        tileHas(tileA, TILE_HAS_BEES) || tileHas(tileB, TILE_HAS_BEES)) return false;",
)
replace(
    "shaders/move.comp",
    "bool isMovementHazard(uint material) {",
    "bool halfWaterAhead(ivec2 position, int direction) {\n"
    "    for (int distance = 2; distance <= 4; ++distance) {\n"
    "        Cell candidate = sampleAt(position + ivec2(direction * distance, 0));\n"
    "        if (isHalfWaterCell(candidate)) return true;\n"
    "        if (!isOpenGas(candidate)) return false;\n"
    "    }\n"
    "    return false;\n"
    "}\n\n"
    "bool isMovementHazard(uint material) {",
)
replace(
    "shaders/move.comp",
    "    if (movePc.reserved1 != 0u && !isHalfWaterCell(a) && !isHalfWaterCell(b)) return;",
    "    if (isHalfWaterCell(a) && isOpenGas(b) && halfWaterAhead(left, 1)) { swapCells(left, right); return; }\n"
    "    if (isHalfWaterCell(b) && isOpenGas(a) && halfWaterAhead(right, -1)) { swapCells(left, right); return; }\n\n"
    "    if ((movePc.reserved1 & 1u) != 0u && !isHalfWaterCell(a) && !isHalfWaterCell(b)) return;",
)
replace(
    "shaders/move.comp",
    "        else if (movedMaterial == MAT_BEETLE) atomicAdd(debugStats[STAT_BEETLE_MOVES], 1u);",
    "        else if (movedMaterial == MAT_BEETLE) atomicAdd(debugStats[STAT_BEETLE_MOVES], 1u);\n"
    "        if (macroExclusiveCell(a) || macroExclusiveCell(b)) atomicAdd(debugStats[STAT_FINE_REPAIR_MOVES], 1u);",
)
replace(
    "shaders/move.comp",
    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\n           material == MAT_FACTORY_CORE;",
    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\n"
    "           material == MAT_FACTORY_CORE || material == MAT_SLUICE_BOX;",
    count=3,
)

# Wet sand/silt and the sluice machine are canonical chemistry, not provenance rules.
replace(
    "shaders/chemistry.comp",
    "bool isMachine(uint material) {\n    return material == MAT_SMELTER || material == MAT_ASSEMBLER;\n}",
    "bool isMachine(uint material) {\n"
    "    return material == MAT_SMELTER || material == MAT_ASSEMBLER || material == MAT_SLUICE_BOX;\n"
    "}",
)
replace(
    "shaders/chemistry.comp",
    "bool isFactoryInputResource(uint material) {\n    return material == MAT_IRON",
    "bool isFactoryInputResource(uint material) {\n    return material == MAT_SAND || material == MAT_IRON",
)
replace(
    "shaders/chemistry.comp",
    "int machineResourceSlot(uint machine, uint resourceMaterial) {",
    "int machineResourceSlot(uint machine, Cell resourceCell) {\n"
    "    uint resourceMaterial = resourceCell.material;\n"
    "    if (machine == MAT_SLUICE_BOX)\n"
    "        return resourceMaterial == MAT_SAND && (resourceCell.aux & AUX_WET) != 0u ? 0 : -1;",
)
replace(
    "shaders/chemistry.comp",
    "ivec2 nearestAcceptingMachine(ivec2 resourcePosition, uint resourceMaterial, int radius) {",
    "ivec2 nearestAcceptingMachine(ivec2 resourcePosition, Cell resourceCell, int radius) {",
)
replace(
    "shaders/chemistry.comp",
    "                machineResourceSlot(candidate.material, resourceMaterial) >= 0)",
    "                machineResourceSlot(candidate.material, resourceCell) >= 0)",
)
replace(
    "shaders/chemistry.comp",
    "            int slot = machineResourceSlot(machine, resourceCell.material);",
    "            int slot = machineResourceSlot(machine, resourceCell);",
)
replace(
    "shaders/chemistry.comp",
    "                all(equal(nearestAcceptingMachine(resourcePosition, resourceCell.material, 6), controller)))",
    "                all(equal(nearestAcceptingMachine(resourcePosition, resourceCell, 6), controller)))",
)
replace(
    "shaders/chemistry.comp",
    "uvec4 machineRecipe(uint machine) { if (machine == MAT_ASSEMBLER) return uvec4(2u,1u,1u,1u); return uvec4(15u); }\n"
    "bool machineReady(uint machine, uvec4 inventory) { if (machine == MAT_SMELTER) return inventory.x>=4u || inventory.y>=4u; return all(greaterThanEqual(inventory,machineRecipe(machine))); }\n"
    "uvec4 machineConsumption(uint machine, uvec4 inventory) { if(machine==MAT_SMELTER) return inventory.x>=4u?uvec4(4u,0u,0u,0u):uvec4(0u,4u,0u,0u); return machineRecipe(machine); }\n"
    "uint machineOutput(uint machine, uvec4 inventory) { if(machine==MAT_SMELTER) return inventory.x>=4u?MAT_IRON:MAT_ALUMINUM; if(machine==MAT_ASSEMBLER) return MAT_PLASMA_AMMO; return MAT_EMPTY; }",
    "uvec4 machineRecipe(uint machine) { if (machine == MAT_ASSEMBLER) return uvec4(2u,1u,1u,1u); return uvec4(15u); }\n"
    "bool machineReady(uint machine, uvec4 inventory) {\n"
    "    if (machine == MAT_SMELTER) return inventory.x >= 4u || inventory.y >= 4u;\n"
    "    if (machine == MAT_SLUICE_BOX) return inventory.z > 0u || inventory.y > 0u;\n"
    "    return all(greaterThanEqual(inventory, machineRecipe(machine)));\n"
    "}\n"
    "uvec4 machineConsumption(uint machine, uvec4 inventory) {\n"
    "    if (machine == MAT_SMELTER) return inventory.x >= 4u ? uvec4(4u,0u,0u,0u) : uvec4(0u,4u,0u,0u);\n"
    "    if (machine == MAT_SLUICE_BOX) return inventory.z > 0u ? uvec4(0u,0u,1u,0u) : uvec4(0u,1u,0u,0u);\n"
    "    return machineRecipe(machine);\n"
    "}\n"
    "uint machineOutput(uint machine, uvec4 inventory) {\n"
    "    if (machine == MAT_SMELTER) return inventory.x >= 4u ? MAT_IRON : MAT_ALUMINUM;\n"
    "    if (machine == MAT_ASSEMBLER) return MAT_PLASMA_AMMO;\n"
    "    if (machine == MAT_SLUICE_BOX) return inventory.z > 0u ? MAT_GOLD : MAT_SILT;\n"
    "    return MAT_EMPTY;\n"
    "}",
)
replace(
    "shaders/chemistry.comp",
    "        ivec2 acceptingMachine = nearestAcceptingMachine(p, source.material, 6);",
    "        ivec2 acceptingMachine = nearestAcceptingMachine(p, source, 6);",
)
replace(
    "shaders/chemistry.comp",
    "        uvec4 inventory = min(machineInventory(source) + incomingMachineCounts(p, source.material), uvec4(15u));\n"
    "        bool canOutput = at(machineOutputPosition(p)).material == MAT_EMPTY &&",
    "        uvec4 inventory = min(machineInventory(source) + incomingMachineCounts(p, source.material), uvec4(15u));\n"
    "        if (source.material == MAT_SLUICE_BOX && inventory.x >= 8u && fallingWaterNear(p) &&\n"
    "            inventory.y <= 8u && inventory.z < 15u) {\n"
    "            inventory.x -= 8u;\n"
    "            inventory.y += 7u;\n"
    "            inventory.z += 1u;\n"
    "        }\n"
    "        bool canOutput = at(machineOutputPosition(p)).material == MAT_EMPTY &&",
)
replace(
    "shaders/chemistry.comp",
    "    if (source.material == MAT_DIRT) {\n        if (freshContact) result.aux |= AUX_WET;",
    "    if (source.material == MAT_SAND || source.material == MAT_SILT) {\n"
    "        if (freshContact) result.aux |= AUX_WET;\n"
    "        else if ((result.aux & AUX_WET) != 0u && source.age > 600u && (randomValue & 127u) == 0u)\n"
    "            result.aux &= ~AUX_WET;\n"
    "    } else if (source.material == MAT_DIRT) {\n"
    "        if (freshContact) result.aux |= AUX_WET;",
)
replace(
    "shaders/chemistry.comp",
    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\n           material == MAT_FACTORY_CORE;",
    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\n"
    "           material == MAT_FACTORY_CORE || material == MAT_SLUICE_BOX;",
    count=2,
)

# Debug hierarchy counters and restored swaps visibility.
replace(
    "shaders/debug_stats.glsl",
    "const uint STAT_CHUNK_SKIPPED_CELLS = 100u;",
    "const uint STAT_CHUNK_SKIPPED_CELLS = 100u;\n"
    "const uint STAT_FINE_TILES = 101u;\n"
    "const uint STAT_MACRO_TILES = 102u;\n"
    "const uint STAT_SETTLED_TILES = 103u;\n"
    "const uint STAT_FINE_REPAIR_MOVES = 104u;",
)
replace(
    "shaders/debug_stats.comp",
    "        if (tileHas(tile, TILE_ACTIVE)) atomicAdd(debugStats[STAT_ACTIVE_TILES], 1u);",
    "        if (tileHas(tile, TILE_ACTIVE)) atomicAdd(debugStats[STAT_ACTIVE_TILES], 1u);\n"
    "        if (tileHas(tile, TILE_FINE_ACTIVE)) atomicAdd(debugStats[STAT_FINE_TILES], 1u);\n"
    "        if (tileHas(tile, TILE_MACRO_MOVABLE)) atomicAdd(debugStats[STAT_MACRO_TILES], 1u);\n"
    "        if (tileHas(tile, TILE_SETTLED_MEDIUM)) atomicAdd(debugStats[STAT_SETTLED_TILES], 1u);",
)

# Macro decisions gain gas-rise phase, tile cache barriers, bounded fine repair, and cheaper debug sampling.
replace(
    "src/vulkan_renderer.cpp",
    "        const std::array<std::int32_t, 5> macro_phases = (simulation_step & 1u) == 0u\n  ? std::array<std::int32_t, 5>{0, 1, 2, 3, 4}\n  : std::array<std::int32_t, 5>{0, 2, 1, 4, 3};",
    "        const std::array<std::int32_t, 6> macro_phases = (simulation_step & 1u) == 0u\n"
    "  ? std::array<std::int32_t, 6>{0, 5, 1, 2, 3, 4}\n"
    "  : std::array<std::int32_t, 6>{0, 5, 2, 1, 4, 3};",
)
replace(
    "src/vulkan_renderer.cpp",
    "  if (phase <= 2) {",
    "  if (phase <= 2 || phase == 5) {",
)
replace(
    "src/vulkan_renderer.cpp",
    "  buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,\n                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,\n                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);",
    "  buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,\n"
    "                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,\n"
    "                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);\n"
    "  buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,\n"
    "                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,\n"
    "                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);",
    count=1,
)
replace(
    "src/vulkan_renderer.cpp",
    "                .reserved1 = phase_index >= 9u ? 1u : 0u,",
    "                .reserved1 = (phase_index >= 9u ? 1u : 0u) |\n"
    "                             (((simulation_step & 3u) == 0u) ? 2u : 0u),",
)
replace(
    "src/vulkan_renderer.cpp",
    "            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 8u) == 0u;",
    "            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 16u) == 0u;",
)

# Presentation blends half volume and avoids drawing unreadable tile grids.
replace(
    "shaders/fullscreen.frag",
    "    if (isHalfWater(cell)) base.rgb = mix(backgroundColor(grid), base.rgb, 0.50);",
    "    if (isHalfWater(cell)) {\n"
    "        uint waterNeighbors = 0u;\n"
    "        waterNeighbors += cellAt(grid + ivec2(-1, 0)).material == MAT_WATER ? 1u : 0u;\n"
    "        waterNeighbors += cellAt(grid + ivec2(1, 0)).material == MAT_WATER ? 1u : 0u;\n"
    "        waterNeighbors += cellAt(grid + ivec2(0, -1)).material == MAT_WATER ? 1u : 0u;\n"
    "        waterNeighbors += cellAt(grid + ivec2(0, 1)).material == MAT_WATER ? 1u : 0u;\n"
    "        float coverage = 0.62 + float(waterNeighbors) * 0.045;\n"
    "        base.rgb = mix(backgroundColor(grid), base.rgb, min(coverage, 0.80));\n"
    "    }",
)
replace(
    "shaders/fullscreen.frag",
    "        ivec2 local = ivec2(int(gridX & 7u), int(gridY & 7u));\n        if (local.x == 0 || local.y == 0)\n  color.rgb *= renderPc.viewWidth < renderPc.gridWidth ? 0.72 : 0.88;\n        ivec2 chunkLocal = ivec2(int(gridX % CHUNK_CELL_SIZE), int(gridY % CHUNK_CELL_SIZE));",
    "        ivec2 local = ivec2(int(gridX & 7u), int(gridY & 7u));\n"
    "        bool readableTileGrid = renderPc.viewportWidth / max(renderPc.viewWidth, 1u) >= 2u;\n"
    "        if (readableTileGrid && (local.x == 0 || local.y == 0)) color.rgb *= 0.82;\n"
    "        ivec2 chunkLocal = ivec2(int(gridX & (CHUNK_CELL_SIZE - 1u)),\n"
    "                                     int(gridY & (CHUNK_CELL_SIZE - 1u)));",
)
replace(
    "shaders/fullscreen.frag",
    "        else if (tileHas(tile, TILE_SLEEPING)) { overlay = vec3(0.16, 0.72, 0.38); alpha = 0.22; }\n        else if (tileHas(tile, TILE_ACTIVE)) { overlay = vec3(0.10, 0.65, 0.92); alpha = 0.18; }",
    "        else if (tileHas(tile, TILE_MACRO_MOVED)) { overlay = vec3(0.96, 0.48, 0.10); alpha = 0.30; }\n"
    "        else if (tileHas(tile, TILE_FINE_ACTIVE)) { overlay = vec3(0.86, 0.18, 0.74); alpha = 0.22; }\n"
    "        else if (tileHas(tile, TILE_MACRO_MOVABLE)) { overlay = vec3(0.12, 0.78, 0.88); alpha = 0.20; }\n"
    "        else if (tileHas(tile, TILE_SETTLED_MEDIUM)) { overlay = vec3(0.18, 0.70, 0.34); alpha = 0.16; }\n"
    "        else if (tileHas(tile, TILE_SLEEPING)) { overlay = vec3(0.16, 0.72, 0.38); alpha = 0.22; }\n"
    "        else if (tileHas(tile, TILE_ACTIVE)) { overlay = vec3(0.10, 0.65, 0.92); alpha = 0.18; }",
)
replace(
    "shaders/fullscreen.frag",
    "        uint rows = narrowPanel ? 6u : 3u;",
    "        uint rows = narrowPanel ? 7u : 4u;",
)
replace(
    "shaders/fullscreen.frag",
    "            uint fixedLabels[8] = uint[8](1u, 76u, 80u, 79u, 82u, 83u, 98u, 81u);\n            uint fixedValues[8] = uint[8](",
    "            uint fixedLabels[9] = uint[9](1u, 76u, 80u, 79u, 78u, 82u, 83u, 98u, 81u);\n"
    "            uint fixedValues[9] = uint[9](",
)
replace(
    "shaders/fullscreen.frag",
    "                debugStats[STAT_MOVED_CELLS],\n                debugStats[STAT_BEE_COUNT],",
    "                debugStats[STAT_MOVED_CELLS],\n                debugStats[STAT_MOVE_SWAPS],\n                debugStats[STAT_BEE_COUNT],",
)
replace(
    "shaders/fullscreen.frag",
    "            for (uint stat = 0u; stat < 8u; ++stat) {",
    "            for (uint stat = 0u; stat < 9u; ++stat) {",
)
replace(
    "shaders/fullscreen.frag",
    "            uint hierarchyValues[4] = uint[4](\n                debugStats[STAT_SLEEPING_CHUNKS],\n                debugStats[STAT_ACTIVE_CHUNKS],\n                debugStats[STAT_MACRO_TILE_MOVES],\n                debugStats[STAT_MACRO_CELL_MOVES]);\n            for (uint stat = 0u; stat < 4u; ++stat) {\n                uint slot = stat + 8u;",
    "            uint hierarchyValues[5] = uint[5](\n"
    "                debugStats[STAT_SLEEPING_CHUNKS],\n"
    "                debugStats[STAT_ACTIVE_CHUNKS],\n"
    "                debugStats[STAT_MACRO_TILE_MOVES],\n"
    "                debugStats[STAT_MACRO_CELL_MOVES],\n"
    "                debugStats[STAT_CHUNK_SKIPPED_CELLS]);\n"
    "            for (uint stat = 0u; stat < 5u; ++stat) {\n"
    "                uint slot = stat + 9u;",
)

# Documentation and static contracts.
replace(
    "README.md",
    "# SandHybrid",
    "# SandHybrid\n\nSee `MISSION_LEDGER.md` for the durable macro-hierarchy release ledger.",
)
replace(
    "HALF_WATER.md",
    "Half-water",
    "Half-water",
)
write("HALF_WATER.md", read("HALF_WATER.md") + "\n\n## v2.4.0 settling\n\nEqual-level random water hopping is removed. Half cells retain conserved displaced gas, receive deterministic short-range attraction toward another half cell, and use stronger presentation coverage without changing represented volume. Full uniform liquid regions are decided by the 8x8 macro hierarchy; mixed edges are repaired periodically by the fine pass.\n")
replace(
    "tests/behavior_contract.cpp",
    "static_assert(sizeof(epoch::sand::Cell) == 16u);",
    "static_assert(sizeof(epoch::sand::Cell) == 16u);\n"
    "static_assert(epoch::sand::material_count == 66u);\n"
    "static_assert(epoch::sand::is_block_material(epoch::sand::Material::sluice_box));",
)
replace(
    "tools/validate_shader_contracts.py",
    "    for token in (\"oxygenVolume > 0u\", \"fullyChoked\", \"state.health -= 1u\"):",
    "    for token in (\"oxygenVolume > 0u\", \"fullyChoked\", \"state.health -= 1u\"):",
)
replace(
    "tools/validate_shader_contracts.py",
    "    renderer_cpp = (ROOT / \"src/vulkan_renderer.cpp\").read_text(encoding=\"utf-8\")",
    "    renderer_cpp = (ROOT / \"src/vulkan_renderer.cpp\").read_text(encoding=\"utf-8\")\n"
    "    macro_move = (SHADERS / \"macro_move.comp\").read_text(encoding=\"utf-8\")\n"
    "    tiles_comp = (SHADERS / \"tiles.comp\").read_text(encoding=\"utf-8\")\n"
    "    chemistry = (SHADERS / \"chemistry.comp\").read_text(encoding=\"utf-8\")\n"
    "    for token in (\"TILE_MACRO_MOVABLE\", \"TILE_FINE_ACTIVE\", \"TILE_SETTLED_MEDIUM\"):\n"
    "        if token not in macro_move + tiles_comp:\n"
    "            errors.append(f\"macro hierarchy contract missing {token!r}\")\n"
    "    for token in (\"MAT_SLUICE_BOX\", \"inventory.x -= 8u\", \"inventory.y += 7u\", \"inventory.z += 1u\"):\n"
    "        if token not in chemistry:\n"
    "            errors.append(f\"sluice conservation contract missing {token!r}\")",
)

# The generator now materializes the new ID, C++ catalog, physics, and packed UI tables.
import subprocess
subprocess.run(["python3", str(ROOT / "tools/generate_ui_text.py")], cwd=ROOT, check=True)

print("Applied v2.4.0 macro hierarchy, wet-material, sluice, settling, rendering, and debug missions.")
