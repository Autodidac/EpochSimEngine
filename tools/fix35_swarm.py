#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import random

ROOT = Path(__file__).resolve().parents[1]


def build_swarm_offsets() -> list[tuple[int, int]]:
    rng = random.Random(0xBEE3501)

    def central(x: int, y: int) -> bool:
        r2 = x * x + y * y
        return 13 * 13 <= r2 <= 21 * 21

    def lobe(x: int, y: int, cx: int, cy: int) -> bool:
        dx = x - cx
        dy = y - cy
        r2 = dx * dx + dy * dy
        if not (12 * 12 <= r2 <= 25 * 25):
            return False
        return dx * -cx + dy * -cy < 220

    components = [
        [(x, y) for y in range(-63, 64) for x in range(-63, 64) if central(x, y)],
        [(x, y) for y in range(-63, 64) for x in range(-63, 64)
         if lobe(x, y, 0, -30) and x * x + y * y > 150],
        [(x, y) for y in range(-63, 64) for x in range(-63, 64)
         if lobe(x, y, -26, 15) and x * x + y * y > 150],
        [(x, y) for y in range(-63, 64) for x in range(-63, 64)
         if lobe(x, y, 26, 15) and x * x + y * y > 150],
    ]
    counts = [32, 56, 56, 56]
    chosen: list[tuple[int, int]] = []
    used: set[tuple[int, int]] = set()
    for candidates, count in zip(components, counts):
        rng.shuffle(candidates)
        taken = 0
        for point in candidates:
            if point in used:
                continue
            used.add(point)
            chosen.append(point)
            taken += 1
            if taken == count:
                break
        if taken != count:
            raise SystemExit(f"unable to generate {count} unique swarm points")
    if len(chosen) != 200 or len(set(chosen)) != 200:
        raise SystemExit("swarm generator did not produce exactly 200 unique points")
    return sorted(chosen, key=lambda p: ((p[1] + 64) << 7) | (p[0] + 64))


def write_bee_swarm() -> None:
    packed = [((y + 64) << 7) | (x + 64) for x, y in build_swarm_offsets()]
    rows = []
    for start in range(0, len(packed), 10):
        suffix = "," if start + 10 < len(packed) else ""
        rows.append("    " + ", ".join(f"{value}u" for value in packed[start:start + 10]) + suffix)
    packed_text = "\n".join(rows)

    glsl = f'''#ifndef EPOCH_SAND_BEE_SWARM_GLSL
#define EPOCH_SAND_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 200u;
const uint BEE_TARGET_NONE = 0xffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;

const uint BEE_SWARM_BIOHAZARD_TICKS = 1800u;
const uint BEE_SWARM_ALTERNATE_TICKS = 600u;
const uint BEE_SWARM_CYCLE_TICKS =
    BEE_SWARM_BIOHAZARD_TICKS + BEE_SWARM_ALTERNATE_TICKS * 2u;

const uint BEE_INITIAL_PACKED[BEE_FORMATION_COUNT] = uint[](
{packed_text}
);

uint beeHash32(uint value) {{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}}

ivec2 beeFormationOffset(uint slot) {{
    uint packedValue = BEE_INITIAL_PACKED[min(slot, BEE_FORMATION_COUNT - 1u)];
    return ivec2(int(packedValue & 127u) - 64, int(packedValue >> 7u) - 64);
}}

int beeFormationSlotFromOffset(ivec2 offset) {{
    if (offset.x < -64 || offset.x > 63 || offset.y < -64 || offset.y > 63) return -1;
    uint key = (uint(offset.y + 64) << 7u) | uint(offset.x + 64);
    int low = 0;
    int high = int(BEE_FORMATION_COUNT) - 1;
    for (int iteration = 0; iteration < 8 && low <= high; ++iteration) {{
        int middle = (low + high) / 2;
        uint middleKey = BEE_INITIAL_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }}
    return -1;
}}

uint beeFormationSlotFromAux(uint aux) {{ return (aux >> 15u) & 255u; }}

ivec2 beeHomeCenterFromAux(uint aux) {{
    return ivec2(int(aux & 255u) * 4, int((aux >> 8u) & 127u) * 4);
}}

uint beePackMetadata(uint aux, ivec2 homeCenter, uint slot) {{
    uint homeX = uint(clamp(homeCenter.x / 4, 0, 255));
    uint homeY = uint(clamp(homeCenter.y / 4, 0, 127));
    uint metadata = homeX | (homeY << 8u) | ((slot & 255u) << 15u);
    return (aux & ~BEE_METADATA_MASK) | metadata;
}}

uint beeTimerFromAge(uint age) {{ return age & 0xffffu; }}
uint beeTargetTileFromAge(uint age) {{ return age >> 16u; }}
uint beePackAge(uint timer, uint targetTile) {{
    return min(timer, 0xffffu) | (min(targetTile, BEE_TARGET_NONE) << 16u);
}}

bool beeIsForager(uint aux) {{
    uint slot = beeFormationSlotFromAux(aux);
    return ((slot * 37u + 11u) % 10u) == 0u;
}}

ivec2 beeRotateOffset(ivec2 offset, uint phase) {{
    switch (phase & 15u) {{
    case 0u: return offset;
    case 1u: return ivec2((offset.x * 237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * 237) / 256);
    case 2u: return ivec2((offset.x * 181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * 181) / 256);
    case 3u: return ivec2((offset.x * 98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * 98) / 256);
    case 4u: return ivec2(-offset.y, offset.x);
    case 5u: return ivec2((offset.x * -98 - offset.y * 237) / 256, (offset.x * 237 - offset.y * 98) / 256);
    case 6u: return ivec2((offset.x * -181 - offset.y * 181) / 256, (offset.x * 181 - offset.y * 181) / 256);
    case 7u: return ivec2((offset.x * -237 - offset.y * 98) / 256, (offset.x * 98 - offset.y * 237) / 256);
    case 8u: return -offset;
    case 9u: return ivec2((offset.x * -237 + offset.y * 98) / 256, (offset.x * -98 - offset.y * 237) / 256);
    case 10u: return ivec2((offset.x * -181 + offset.y * 181) / 256, (offset.x * -181 - offset.y * 181) / 256);
    case 11u: return ivec2((offset.x * -98 + offset.y * 237) / 256, (offset.x * -237 - offset.y * 98) / 256);
    case 12u: return ivec2(offset.y, -offset.x);
    case 13u: return ivec2((offset.x * 98 + offset.y * 237) / 256, (offset.x * -237 + offset.y * 98) / 256);
    case 14u: return ivec2((offset.x * 181 + offset.y * 181) / 256, (offset.x * -181 + offset.y * 181) / 256);
    case 15u: return ivec2((offset.x * 237 + offset.y * 98) / 256, (offset.x * -98 + offset.y * 237) / 256);
    }}
    return offset;
}}

uint beeSwarmState(uint aux, uint step) {{
    uint local = step % BEE_SWARM_CYCLE_TICKS;
    if (local < BEE_SWARM_BIOHAZARD_TICKS) return 0u;
    ivec2 home = beeHomeCenterFromAux(aux);
    uint cycle = step / BEE_SWARM_CYCLE_TICKS;
    bool reverse = (beeHash32(uint(home.x) * 73856093u ^ uint(home.y) * 19349663u ^ cycle) & 1u) != 0u;
    uint alternate = (local - BEE_SWARM_BIOHAZARD_TICKS) / BEE_SWARM_ALTERNATE_TICKS;
    return reverse ? 2u - alternate : 1u + alternate;
}}

ivec2 beeBiohazardTargetOffset(uint slot, uint step, ivec2 home) {{
    const uint increments[8] = uint[8](1u, 3u, 7u, 9u, 11u, 13u, 17u, 19u);
    uint epoch = step / 90u;
    uint increment = increments[beeHash32(uint(home.x) ^ (uint(home.y) << 16u) ^ epoch) & 7u];
    uint targetSlot = (slot + epoch * increment) % BEE_FORMATION_COUNT;
    ivec2 anchor = beeFormationOffset(targetSlot);
    ivec2 flutter = beeRotateOffset(ivec2(1 + int(slot & 1u), 0), step / 3u + slot * 5u);
    return anchor + flutter;
}}

ivec2 beeHaloTargetOffset(uint slot, uint step) {{
    int radius = 34 + int((slot * 13u) % 18u);
    uint phase = step / 10u + slot * 7u;
    return beeRotateOffset(ivec2(radius, 0), phase) +
           beeRotateOffset(ivec2(2, 0), step / 3u + slot * 11u);
}}

ivec2 beeCloudTargetOffset(uint slot, uint step) {{
    uint lobe = slot % 3u;
    ivec2 center = lobe == 0u ? ivec2(0, -29) :
                   (lobe == 1u ? ivec2(-26, 15) : ivec2(26, 15));
    int radius = 5 + int((slot * 17u) % 16u);
    uint phase = step / 8u + slot * 9u;
    return center + beeRotateOffset(ivec2(radius, 0), phase) +
           beeRotateOffset(ivec2(1, 0), step / 2u + slot * 3u);
}}

ivec2 beeSwarmTarget(uint aux, uint step) {{
    uint slot = beeFormationSlotFromAux(aux);
    ivec2 home = beeHomeCenterFromAux(aux);
    uint state = beeSwarmState(aux, step);
    ivec2 offset = state == 0u ? beeBiohazardTargetOffset(slot, step, home) :
                   (state == 1u ? beeHaloTargetOffset(slot, step)
                                : beeCloudTargetOffset(slot, step));
    return home + offset;
}}

ivec2 beeOrbitTarget(uint aux, uint step) {{ return beeSwarmTarget(aux, step); }}

ivec2 beeLandingOffset(uint slot) {{
    switch (slot & 15u) {{
    case 0u: return ivec2(13, 0); case 1u: return ivec2(12, 5);
    case 2u: return ivec2(9, 9); case 3u: return ivec2(5, 12);
    case 4u: return ivec2(0, 13); case 5u: return ivec2(-5, 12);
    case 6u: return ivec2(-9, 9); case 7u: return ivec2(-12, 5);
    case 8u: return ivec2(-13, 0); case 9u: return ivec2(-12, -5);
    case 10u: return ivec2(-9, -9); case 11u: return ivec2(-5, -12);
    case 12u: return ivec2(0, -13); case 13u: return ivec2(5, -12);
    case 14u: return ivec2(9, -9); case 15u: return ivec2(12, -5);
    }}
    return ivec2(13, 0);
}}

int beeAxisSign(int value) {{ return value > 0 ? 1 : (value < 0 ? -1 : 0); }}

ivec2 beeApproachPosition(ivec2 occupiedPosition, ivec2 fromPosition) {{
    ivec2 delta = fromPosition - occupiedPosition;
    ivec2 direction = ivec2(beeAxisSign(delta.x), beeAxisSign(delta.y));
    if (all(equal(direction, ivec2(0)))) direction = ivec2(1, 0);
    return occupiedPosition + direction;
}}

ivec2 beeMigrationSite(ivec2 flowerPosition, uint width, uint height) {{
    ivec2 site = flowerPosition + ivec2(0, -16);
    site = ivec2((site.x / 4) * 4, (site.y / 4) * 4);
    return clamp(site, ivec2(16), ivec2(int(width) - 17, int(height) - 17));
}}

#endif
'''
    (ROOT / "shaders/bee_swarm.glsl").write_text(glsl, encoding="utf-8", newline="\n")


write_bee_swarm()

paint = r'''#version 450
#extension GL_GOOGLE_include_directive : require
#include "materials.glsl"
#include "conservation.glsl"
#include "tiles.glsl"
#include "chunks.glsl"
#include "bee_swarm.glsl"

layout(local_size_x = 16, local_size_y = 16) in;
layout(std430, binding = 0) buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 7) buffer Chunks { ChunkState chunks[]; };

void markPaintChunkDirty(ivec2 p) {
    atomicOr(chunks[chunkIndex(p, pc.width)].flags, CHUNK_DIRTY | CHUNK_ACTIVE);
}

void main() {
    ivec2 local = ivec2(gl_GlobalInvocationID.xy);
    int diameter = int(pc.radius * 2u + 1u);
    if (local.x >= diameter || local.y >= diameter) return;

    ivec2 center = ivec2(pc.brushX, pc.brushY);
    ivec2 p = center + local - ivec2(int(pc.radius));
    if (!inside(p)) return;

    uint material = pc.material & 0xffffu;
    uint brushShape = (pc.material >> 16u) & 3u;
    material = material < MATERIAL_COUNT ? material : MAT_EMPTY;

    if (isBlockCapable(material)) {
        ivec2 blockOrigin = (center / int(STRUCTURAL_BLOCK_SIZE)) * int(STRUCTURAL_BLOCK_SIZE);
        ivec2 blockEnd = blockOrigin + ivec2(int(STRUCTURAL_BLOCK_SIZE));
        if (p.x < blockOrigin.x || p.y < blockOrigin.y || p.x >= blockEnd.x || p.y >= blockEnd.y) return;
        bool anchored = blockEnd.y >= int(pc.height) - 1;
        Cell cell = makeStructuralCell(material, anchored);
        bool machineController = (material == MAT_SMELTER || material == MAT_ASSEMBLER ||
                                  material == MAT_INSECT_HABITAT) &&
                                 (p.x & 7) == 3 && (p.y & 7) == 3;
        cell.aux = (cell.aux & ~AUX_RANDOM_MASK) |
                   (cellHash(p, uint(local.x + local.y * diameter)) & AUX_RANDOM_MASK) |
                   (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                                AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED | AUX_STATE_MASK));
        if (machineController) {
            setMachineInventory(cell, uvec4(0u));
            cell.aux |= AUX_CHARGED;
        }
        uint index = indexOf(p);
        Cell previous = cells[index];
        recordConservation(previous, cell);
        cells[index] = cell;
        markPaintChunkDirty(p);
        return;
    }

    if ((material == MAT_SEED || material == MAT_QUEEN_BEE) && any(notEqual(p, center))) return;

    ivec2 delta = p - center;
    int distanceSquared = delta.x * delta.x + delta.y * delta.y;
    int radius = int(pc.radius);
    bool insideBrush = false;
    if (brushShape == 0u) insideBrush = distanceSquared <= radius * radius;
    else if (brushShape == 1u) insideBrush = abs(delta.x) <= radius && abs(delta.y) <= radius;
    else if (brushShape == 2u) insideBrush = abs(delta.x) <= radius && abs(delta.y) <= 1;
    else insideBrush = abs(delta.x) <= 1 && abs(delta.y) <= radius;
    if (!insideBrush) return;

    Cell cell;
    int colonySlot = -1;
    if (material == MAT_BEE_NEST) {
        // Exact Fix29 compact hive. The larger dispatch only initializes the
        // hidden 200-bee mask around the hive; it does not enlarge the shell.
        if (distanceSquared >= 28 && distanceSquared < 108) {
            cell = makeCell(MAT_BEE_NEST);
        } else if (all(equal(p, center))) {
            cell = makeCell(MAT_QUEEN_BEE);
        } else if (delta.x >= 1 && delta.x <= 12 && abs(delta.y) <= 1) {
            cell = makeCell(MAT_EMPTY);
        } else if (distanceSquared < 28) {
            uint chamber = cellHash(p, 0xb33u);
            cell = makeCell((chamber & 3u) == 0u ? MAT_EMPTY :
                            ((chamber & 4u) == 0u ? MAT_HONEY : MAT_POLLEN));
        } else {
            colonySlot = beeFormationSlotFromOffset(delta);
            if (colonySlot < 0) return;
            cell = makeCell(MAT_BEE);
        }
    } else {
        cell = makeCell(material);
    }

    cell.aux = (cell.aux & ~AUX_RANDOM_MASK) |
               (cellHash(p, uint(local.x + local.y * diameter)) & AUX_RANDOM_MASK) |
               (cell.aux & (AUX_WET | AUX_CHARGED | AUX_BEE_POLLEN | AUX_BEE_FED |
                            AUX_PLANT_STEM | AUX_STRUCTURAL | AUX_SUPPORTED | AUX_STATE_MASK));
    if (colonySlot >= 0) {
        cell.aux |= BEE_AUX_SWARM | BEE_AUX_FED;
        cell.aux = beePackMetadata(cell.aux, center, uint(colonySlot));
        cell.age = beePackAge(uint((colonySlot * 17) % 900), BEE_TARGET_NONE);
    }

    uint index = indexOf(p);
    Cell previous = cells[index];
    recordConservation(previous, cell);
    cells[index] = cell;
    markPaintChunkDirty(p);
}
'''
(ROOT / "shaders/paint.comp").write_text(paint, encoding="utf-8", newline="\n")

reset_path = ROOT / "shaders/reset.comp"
reset = reset_path.read_text(encoding="utf-8")
reset = reset.replace("MAT_LAVA", "MAT_MAGMA_VENT")
reset = reset.replace(
    "// Organic hive and true three-lobed biohazard swarm. The hive is not terrain.",
    "// Fix29 compact hive plus a hidden moving swarm mask. The hive is not terrain.")
reset_path.write_text(reset, encoding="utf-8", newline="\n")

move_path = ROOT / "shaders/move.comp"
move = move_path.read_text(encoding="utf-8")
old = '''    // A dense swarm deadlocks when every bee demands a strictly closer empty
    // cell. Permit only target-bounded tangential sidesteps, so blocked bees
    // circulate around their moving anchors without regaining random wandering.
    ivec2 stepDelta = targetPosition - sourcePosition;
    uint slot = beeFormationSlotFromAux(bee.aux);
    bool clockwise = ((slot * 17u + 3u) & 1u) == 0u;
    ivec2 tangent = clockwise ? ivec2(-sourceDelta.y, sourceDelta.x)
                              : ivec2(sourceDelta.y, -sourceDelta.x);
    int tangentScore = stepDelta.x * tangent.x + stepDelta.y * tangent.y;
    int forwardScore = stepDelta.x * -sourceDelta.x + stepDelta.y * -sourceDelta.y;
    bool boundedSidestep = targetDistance <= sourceDistance + 2 &&
                           targetDistance <= 49 && forwardScore >= 0 &&
                           tangentScore > 0;
    return boundedSidestep;
'''
new = '''    // Dense agents circulate around the moving hidden-mask target instead of
    // deadlocking behind one another. This remains bounded target motion, not wandering.
    ivec2 stepDelta = targetPosition - sourcePosition;
    uint slot = beeFormationSlotFromAux(bee.aux);
    bool clockwise = ((slot * 17u + 3u) & 1u) == 0u;
    ivec2 tangent = clockwise ? ivec2(-sourceDelta.y, sourceDelta.x)
                              : ivec2(sourceDelta.y, -sourceDelta.x);
    int tangentScore = stepDelta.x * tangent.x + stepDelta.y * tangent.y;
    int forwardScore = stepDelta.x * -sourceDelta.x + stepDelta.y * -sourceDelta.y;
    bool boundedSidestep = targetDistance <= sourceDistance + 4 &&
                           forwardScore >= -1 && tangentScore > 0;
    if (boundedSidestep) return true;
    return sourceDistance <= 4 && targetDistance <= 9 && tangentScore >= 0 &&
           ((randomValue ^ slot) & 7u) == 0u;
'''
if old not in move:
    raise SystemExit("move.comp: Fix34 sidestep block not found")
move_path.write_text(move.replace(old, new, 1), encoding="utf-8", newline="\n")

print("Applied Fix35 swarm, colony prefab, and magma-vent scene patch.")
