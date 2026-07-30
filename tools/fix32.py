#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one literal match, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


def regex_once(path: Path, pattern: str, replacement: str) -> None:
    text = path.read_text(encoding="utf-8")
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{path}: expected one regex match, found {count}")
    path.write_text(text, encoding="utf-8", newline="\n")


swarm = ROOT / "shaders/bee_swarm.glsl"
new_points = """const uint BEE_BIOHAZARD_PACKED[BEE_FORMATION_COUNT] = uint[](
    1084u, 1088u, 1205u, 1210u, 1223u, 1341u, 1347u, 1355u, 1460u, 1463u,
    1481u, 1485u, 1585u, 1711u, 1747u, 1871u, 1964u, 2002u, 2132u, 2348u,
    2390u, 2472u, 2730u, 2776u, 2855u, 3162u, 3241u, 3288u, 3366u, 3496u,
    3545u, 3750u, 3783u, 3880u, 3906u, 3929u, 4029u, 4032u, 4036u, 4042u,
    4045u, 4184u, 4263u, 4284u, 4303u, 4314u, 4410u, 4520u, 4562u, 4696u,
    4777u, 4794u, 4820u, 4951u, 6463u, 6588u, 6594u, 6718u, 6725u, 6841u,
    6983u, 7096u, 7195u, 7198u, 7321u, 7396u, 7447u, 7453u, 7528u, 7571u,
    7573u, 7577u, 7654u, 7711u, 7785u, 7788u, 7827u, 7966u, 8012u, 8045u,
    8078u, 8081u, 8175u, 8223u, 8244u, 8305u, 8333u, 8349u, 8370u, 8397u,
    8463u, 8562u, 8652u, 8717u, 8733u, 8755u, 8946u, 8971u, 9076u, 9118u,
    9162u, 9165u, 9268u, 9308u, 9333u, 9356u, 9373u, 9417u, 9420u, 9461u,
    9482u, 9503u, 9527u, 9543u, 9587u, 9610u, 9612u, 9659u, 9693u, 9797u,
    9867u, 9869u, 9888u, 10018u, 10076u, 10100u, 10124u, 10206u, 10273u, 10355u,
    10379u, 10381u, 10403u, 10460u, 10534u, 10537u, 10613u, 10637u, 10792u, 10795u,
    10845u, 10995u, 11021u, 11098u, 11277u, 11279u, 11355u, 11378u, 11481u, 11606u,
    11663u, 11736u, 11760u, 11794u, 11860u, 11990u, 12013u, 12109u, 12115u, 12143u,
    12178u, 12241u, 12308u, 12396u, 12491u, 12595u, 12649u, 12693u, 12721u, 12748u,
    12780u, 12824u, 12903u, 12906u, 12950u, 12954u, 12976u, 13005u, 13161u, 13208u,
    13212u, 13264u, 13284u, 13358u, 13395u, 13410u, 13414u, 13469u, 13475u, 13478u,
    13482u, 13526u, 13530u, 13535u, 13599u, 13660u, 13732u, 13735u, 13785u, 13792u
);"""
regex_once(
    swarm,
    r"const uint BEE_BIOHAZARD_PACKED\[BEE_FORMATION_COUNT\] = uint\[\]\(.*?\n\);",
    new_points,
)

replace_once(
    swarm,
    """ivec2 beeOrbitTarget(uint aux, uint step) {
    uint phase = step / 8u;
    return beeHomeCenterFromAux(aux) +
           beeRotateOffset(beeFormationOffset(beeFormationSlotFromAux(aux)), phase);
}""",
    """ivec2 beeFlutterOffset(uint slot, uint step) {
    // Each bee circles a small moving anchor. The anchor preserves the symbol,
    // while the local orbit makes the silhouette read as a living swarm.
    uint phase = step / 2u + slot * 5u;
    int radius = 2 + int((slot * 13u) % 3u);
    return beeRotateOffset(ivec2(radius, 0), phase);
}

ivec2 beeOrbitTarget(uint aux, uint step) {
    uint slot = beeFormationSlotFromAux(aux);
    uint shapePhase = step / 24u;
    ivec2 anchor = beeHomeCenterFromAux(aux) +
                   beeRotateOffset(beeFormationOffset(slot), shapePhase);
    return anchor + beeFlutterOffset(slot, step);
}""",
)

move = ROOT / "shaders/move.comp"
replace_once(
    move,
    """bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;
    if ((bee.aux & BEE_AUX_SWARM) == 0u) return false;

    ivec2 destination = beeMovementTarget(bee);
    ivec2 sourceDelta = sourcePosition - destination;
    ivec2 targetDelta = targetPosition - destination;
    int sourceDistance = sourceDelta.x * sourceDelta.x + sourceDelta.y * sourceDelta.y;
    int targetDistance = targetDelta.x * targetDelta.x + targetDelta.y * targetDelta.y;
    return targetDistance < sourceDistance;
}""",
    """bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;
    if ((bee.aux & BEE_AUX_SWARM) == 0u) return false;

    ivec2 destination = beeMovementTarget(bee);
    ivec2 sourceDelta = sourcePosition - destination;
    ivec2 targetDelta = targetPosition - destination;
    int sourceDistance = sourceDelta.x * sourceDelta.x + sourceDelta.y * sourceDelta.y;
    int targetDistance = targetDelta.x * targetDelta.x + targetDelta.y * targetDelta.y;
    if (targetDistance < sourceDistance) return true;

    // A dense swarm deadlocks when every bee demands a strictly closer empty
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
    return boundedSidestep && ((randomValue ^ slot ^ movePc.step) & 1u) == 0u;
}""",
)

layout = ROOT / "include/epoch/sand/ui_layout.hpp"
replace_once(layout, "inline constexpr std::uint32_t keymap_height = 76u;",
             "inline constexpr std::uint32_t keymap_height = 108u;")

fullscreen = ROOT / "shaders/fullscreen.frag"
replace_once(fullscreen, "uint keymapBottom = keymapTop + 76u;",
             "uint keymapBottom = keymapTop + 108u;")
replace_once(
    fullscreen,
    """            for (uint i = 0u; i < 6u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 12u)), 1, leftIds[i]);
            for (uint i = 0u; i < 5u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + contentWidth / 2u), int(keymapTop + 25u + i * 12u)), 1, rightIds[i]);""",
    """            uint columnMiddle = contentLeft + contentWidth / 2u;
            if (x >= columnMiddle && x < columnMiddle + 1u &&
                y >= keymapTop + 23u && y < keymapBottom - 6u)
                color = vec3(0.12, 0.20, 0.28);
            for (uint i = 0u; i < 6u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 13u)), 1, leftIds[i]);
            for (uint i = 0u; i < 5u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(columnMiddle + 8u), int(keymapTop + 25u + i * 13u)), 1, rightIds[i]);""",
)

# Update the two inherited text contracts to describe the new bounded circulation.
for audit_path in (
    ROOT / "tools/audit_ecology_motion.py",
    ROOT / "tools/validate_shader_contracts.py",
):
    text = audit_path.read_text(encoding="utf-8")
    text = text.replace(
        '"return targetDistance < sourceDistance;"',
        '"if (targetDistance < sourceDistance) return true;", "boundedSidestep"',
    )
    audit_path.write_text(text, encoding="utf-8", newline="\n")

# Narrow regression checks for this patch.
swarm_text = swarm.read_text(encoding="utf-8")
move_text = move.read_text(encoding="utf-8")
layout_text = layout.read_text(encoding="utf-8")
fullscreen_text = fullscreen.read_text(encoding="utf-8")

values = [int(value) for value in re.findall(
    r"(\d+)u", re.search(
        r"BEE_BIOHAZARD_PACKED\[BEE_FORMATION_COUNT\].*?\((.*?)\);",
        swarm_text, re.S).group(1))]
if len(values) != 200 or len(set(values)) != 200:
    raise SystemExit("biohazard swarm table must contain exactly 200 unique cells")
points = [((value & 127) - 64, (value >> 7) - 64) for value in values]
if min(x * x + y * y for x, y in points) < 144:
    raise SystemExit("biohazard swarm overlaps the compact Fix29 hive")
for token in ("beeFlutterOffset", "shapePhase = step / 24u"):
    if token not in swarm_text:
        raise SystemExit(f"missing living swarm contract: {token}")
for token in ("boundedSidestep", "tangentScore > 0", "targetDistance <= 49"):
    if token not in move_text:
        raise SystemExit(f"missing deadlock escape contract: {token}")
if "keymap_height = 108u" not in layout_text or "keymapBottom = keymapTop + 108u" not in fullscreen_text:
    raise SystemExit("keymap height contract is inconsistent")
if "i * 13u" not in fullscreen_text or "columnMiddle + 8u" not in fullscreen_text:
    raise SystemExit("keymap rows remain compressed")

print("Applied Fix32: living biohazard swarm motion and readable keymap.")
