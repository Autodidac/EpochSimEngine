from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[1]

def require(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"{path}: missing required Fix30 contract: {needle}")

def forbid(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle in text:
        raise SystemExit(f"{path}: stale Fix29 bee behavior remains: {needle}")

def glsl_round(value: float) -> int:
    return math.floor(value + 0.5) if value >= 0.0 else math.ceil(value - 0.5)

positions: set[tuple[int, int]] = set()
lane_counts = [0, 0, 0]
for slot in range(200):
    lane = slot % 3
    lane_index = slot // 3
    lane_count = 66 if lane == 2 else 67
    angle_offset = (0.0, 0.7, 1.4)[lane]
    angle = math.tau * lane_index / lane_count + angle_offset
    base = (18.0, 30.0, 44.0)[lane]
    amplitude = (3.5, 5.0, 7.0)[lane]
    phase = (0.0, 1.7, 3.2)[lane]
    radius = base + math.sin(angle * 3.0 + base * 0.14 + phase) * amplitude
    position = (glsl_round(math.cos(angle) * radius),
                glsl_round(math.sin(angle) * radius))
    if position in positions:
        raise SystemExit(f"duplicate formation slot {slot}: {position}")
    positions.add(position)
    lane_counts[lane] += 1

if len(positions) != 200 or lane_counts != [67, 67, 66]:
    raise SystemExit(f"invalid formation: {len(positions)=}, {lane_counts=}")

require("shaders/reset.comp", "const uint BEE_FORMATION_COUNT = 200u;")
require("shaders/reset.comp", "for (uint slot = 0u; slot < BEE_FORMATION_COUNT; ++slot)")
require("shaders/reset.comp", "cell.aux = (cell.aux & ~0x0000ff00u) | (uint(slot) << 8u);")
require("shaders/reset.comp", "setStateValue(cell, 96u + uint(slot % 160));")
forbid("shaders/reset.comp", "(swarm & 7u) == 0u")
forbid("shaders/reset.comp", "(swarm % 211u) == 0u")

require("shaders/move.comp", "bool isAuthoredForager(Cell bee)")
require("shaders/move.comp", "return isAuthoredBee(bee) && (beeFormationSlot(bee) % 5u) == 0u;")
require("shaders/move.comp", "ivec2 authoredBeeTravelTarget(Cell bee)")
require("shaders/move.comp", "bool authoredBeeOrbitAllowed(Cell bee")
require("shaders/move.comp", "float(movePc.step) * 0.012")
require("shaders/move.comp", "return authoredBeeOrbitAllowed(bee, sourcePosition, targetPosition, randomValue);")
forbid("shaders/move.comp", "authored swarm bees use three persistent integer orbit lanes")

require("shaders/chemistry.comp", "bool hungryBeeTargetsHoney(ivec2 honeyPosition)")
require("shaders/chemistry.comp", "Forty deterministic foragers")
require("shaders/chemistry.comp", "source.age > 90u && (randomValue & 63u) == 0u")
require("shaders/chemistry.comp", "uint portion = min(remaining, 26u);")
require("shaders/chemistry.comp", "160u + (beeFormationSlot(source) % 80u)")

print("Fix30 audit passed: exactly 200 unique slots, 40 foragers, pollen-to-honey lifecycle, 10% honey portions, and formation rejoin logic.")
