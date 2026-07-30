#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, token: str) -> None:
    if token not in text(path):
        errors.append(f"{path}: missing {token!r}")


bee = text("shaders/bee_swarm.glsl")
packed_match = re.search(
    r"BEE_BIOHAZARD_PACKED\[BEE_FORMATION_COUNT\]\s*=\s*uint\[\]\((.*?)\);",
    bee,
    re.S,
)
if not packed_match:
    errors.append("bee_swarm.glsl: packed biohazard table missing")
    packed: list[int] = []
else:
    packed = [int(value) for value in re.findall(r"(\d+)u", packed_match.group(1))]

if len(packed) != 200:
    errors.append(f"biohazard formation contains {len(packed)} entries, expected 200")
if len(set(packed)) != len(packed):
    errors.append("biohazard formation contains duplicate cells")

points = [((value & 127) - 64, (value >> 7) - 64) for value in packed]
if points:
    central = sum(1 for x, y in points if 60 <= x * x + y * y <= 145)
    upper = sum(1 for x, y in points if y < -15)
    lower_left = sum(1 for x, y in points if x < -12 and y > -5)
    lower_right = sum(1 for x, y in points if x > 12 and y > -5)
    if central < 24 or min(upper, lower_left, lower_right) < 30:
        errors.append(
            f"biohazard silhouette is unbalanced: center={central}, lobes="
            f"{upper}/{lower_left}/{lower_right}"
        )

for path, token in (
    ("src/app.cpp", 'show_startup_message("Compiling Shaders...")'),
    ("src/window_win32.cpp", "DrawTextW"),
    ("src/window_xcb.cpp", "xcb_image_text_8"),
    ("include/epoch/sand/window.hpp", "show_startup_message"),
    ("shaders/reset.comp", "queenGroundY - 72"),
    ("shaders/reset.comp", "beeFormationSlotFromOffset"),
    ("shaders/move.comp", "beeOrbitTarget"),
    ("shaders/move.comp", "beeMovementTarget"),
    ("shaders/chemistry.comp", "nearestFlowerTileForColony"),
    ("shaders/chemistry.comp", "migrationDestinationTile"),
    ("shaders/chemistry.comp", "reproductiveSwarm"),
    ("shaders/chemistry.comp", "resourceFailure"),
    ("shaders/chemistry.comp", "uint portion = min(remaining, 26u);"),
    ("shaders/tiles.glsl", "TILE_HAS_MIGRATING_QUEEN"),
    ("shaders/tiles.glsl", "tileFlowerPosition"),
    ("shaders/tiles.glsl", "tileBeeCount"),
    ("shaders/tiles.comp", "packTileOccupancy"),
):
    require(path, token)

shared = text("include/epoch/sand/shared_state.hpp")
if "selected_scene{static_cast<std::uint32_t>(Scene::ecosystem)}" not in shared:
    errors.append("ecosystem is not the default scene")

movement = text("shaders/move.comp")
if "regionalTargetSignal" in movement:
    errors.append("bee navigation still contains the old regional cell scanner")
if "atan(" in movement:
    errors.append("movement shader reintroduced atan")
if re.search(r"\bfor\s*\(", re.sub(r"//.*", "", movement)):
    errors.append("movement shader reintroduced loops")

bee_move = re.search(
    r"bool beeMoveAllowed\(.*?\n\}(?=\n\nbool magneticMoveAllowed)",
    movement,
    re.S,
)
if not bee_move:
    errors.append("beeMoveAllowed block missing")
else:
    body = bee_move.group(0)
    for forbidden in ("randomValue &", "regionalTargetSignal", "return (random"):
        if forbidden in body:
            errors.append(f"bee movement still has wandering fallback: {forbidden!r}")
    if "return targetDistance < sourceDistance;" not in body:
        errors.append("bee movement is not strict target descent")

chemistry = text("shaders/chemistry.comp")
if chemistry.count("nearestBeeTile(") < 5:
    errors.append("sparse tile target lookup is not wired through the lifecycle")
if "A painted or orphaned bee without a queen does not wander." not in chemistry:
    errors.append("orphan bee no-wander contract missing")
if "beePackAge(0u, flowerTile)" not in chemistry:
    errors.append("forager target is not cached in bee state")
if "beePackAge(0u, destinationTile)" not in chemistry:
    errors.append("queen migration target is not cached in bee state")

if errors:
    print("Fix31 audit failed:")
    for error in errors:
        print(f" - {error}")
    raise SystemExit(1)

print("Fix31 audit passed: native startup message, 200-cell rotating biohazard swarm, "
      "cached sparse-tile lifecycle targets, no bee wandering, 10% honey feeding, "
      "and queen-led hive migration.")
