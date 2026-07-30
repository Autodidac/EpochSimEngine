#!/usr/bin/env python3
from __future__ import annotations

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def unique_circle(center: tuple[float, float], radius: float, inward: float | None,
                  target: int) -> list[tuple[int, int]]:
    sequence: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()
    for index in range(5000):
        angle = math.tau * index / 5000.0
        if inward is not None:
            difference = (angle - inward + math.pi) % math.tau - math.pi
            if abs(difference) < 0.72:
                continue
        point = (
            round(center[0] + radius * math.cos(angle)),
            round(center[1] + radius * math.sin(angle)),
        )
        if point not in seen:
            seen.add(point)
            sequence.append(point)

    selected: list[tuple[int, int]] = []
    selected_set: set[tuple[int, int]] = set()
    for index in range(target):
        candidate = sequence[round(index * (len(sequence) - 1) / (target - 1))]
        if candidate not in selected_set:
            selected_set.add(candidate)
            selected.append(candidate)
    for candidate in sequence:
        if len(selected) >= target:
            break
        if candidate not in selected_set:
            selected_set.add(candidate)
            selected.append(candidate)
    return selected[:target]


points = unique_circle((0.0, 0.0), 9.5, None, 32)
for direction in (-math.pi / 2.0, math.pi / 6.0, 5.0 * math.pi / 6.0):
    center = (22.0 * math.cos(direction), 22.0 * math.sin(direction))
    points.extend(unique_circle(center, 19.5, direction + math.pi, 48))

for direction in (-math.pi / 2.0, math.pi / 6.0, 5.0 * math.pi / 6.0):
    perpendicular = direction + math.pi / 2.0
    for index in range(8):
        radius = 12.0 + (index // 2) * 3.0
        side = -1.0 if index % 2 == 0 else 1.0
        bend = 2.5 + side * 0.5 * (index // 2)
        points.append((
            round(radius * math.cos(direction) + side * bend * math.cos(perpendicular)),
            round(radius * math.sin(direction) + side * bend * math.sin(perpendicular)),
        ))

unique: list[tuple[int, int]] = []
seen: set[tuple[int, int]] = set()
for point in points:
    if point not in seen:
        seen.add(point)
        unique.append(point)
for candidate in ((13, -7), (0, 13), (0, -13), (-11, 8), (11, 8)):
    if len(unique) >= 200:
        break
    if candidate not in seen:
        seen.add(candidate)
        unique.append(candidate)

if len(unique) != 200:
    raise SystemExit(f"generated {len(unique)} biohazard cells instead of 200")

packed = sorted(((y + 64) << 7) | (x + 64) for x, y in unique)
if len(set(packed)) != 200:
    raise SystemExit("generated duplicate packed biohazard cells")

packed_lines = [
    "    " + ", ".join(f"{value}u" for value in packed[index:index + 10])
    + ("," if index + 10 < len(packed) else "")
    for index in range(0, len(packed), 10)
]

rotation_cases: list[str] = []
for phase in range(16):
    angle = math.tau * phase / 16.0
    cosine = round(math.cos(angle) * 256)
    sine = round(math.sin(angle) * 256)
    rotation_cases.append(
        f"    case {phase}u: return ivec2("
        f"(offset.x * {cosine} - offset.y * {sine}) / 256, "
        f"(offset.x * {sine} + offset.y * {cosine}) / 256);"
    )

landing = (
    (13, 0), (12, 5), (9, 9), (5, 12),
    (0, 13), (-5, 12), (-9, 9), (-12, 5),
    (-13, 0), (-12, -5), (-9, -9), (-5, -12),
    (0, -13), (5, -12), (9, -9), (12, -5),
)
landing_cases = [
    f"    case {index}u: return ivec2({x}, {y});"
    for index, (x, y) in enumerate(landing)
]

binary_step = """    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }"""

shader = f"""#ifndef EPOCH_SAND_BEE_SWARM_GLSL
#define EPOCH_SAND_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 200u;
const uint BEE_TARGET_NONE = 0xffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;

const uint BEE_BIOHAZARD_PACKED[BEE_FORMATION_COUNT] = uint[](
{chr(10).join(packed_lines)}
);

ivec2 beeFormationOffset(uint slot) {{
    uint packedValue = BEE_BIOHAZARD_PACKED[min(slot, BEE_FORMATION_COUNT - 1u)];
    return ivec2(int(packedValue & 127u) - 64, int(packedValue >> 7u) - 64);
}}

int beeFormationSlotFromOffset(ivec2 offset) {{
    if (offset.x < -64 || offset.x > 63 || offset.y < -64 || offset.y > 63) return -1;
    uint key = (uint(offset.y + 64) << 7u) | uint(offset.x + 64);
    int low = 0;
    int high = int(BEE_FORMATION_COUNT) - 1;
{chr(10).join(binary_step for _ in range(8))}
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
{chr(10).join(rotation_cases)}
    }}
    return offset;
}}

ivec2 beeOrbitTarget(uint aux, uint step) {{
    uint phase = step / 8u;
    return beeHomeCenterFromAux(aux) +
           beeRotateOffset(beeFormationOffset(beeFormationSlotFromAux(aux)), phase);
}}

ivec2 beeLandingOffset(uint slot) {{
    switch (slot & 15u) {{
{chr(10).join(landing_cases)}
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
"""

(ROOT / "shaders/bee_swarm.glsl").write_text(shader, encoding="utf-8", newline="\n")
print("Generated exactly 200 packed biohazard swarm cells.")
