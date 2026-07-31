from __future__ import annotations

from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one marker in {path}, found {count}: {old[:180]!r}")
    write(path, text.replace(old, new, 1))


# Directly painted agents are one cell per click instead of packed brush blobs.
# Before replacing a gas cell, redistribute its represented gas volume into
# adjacent cells of the same gas. This keeps oxygen lossless and turns placement
# into local pressure instead of deletion.
replace_once(
    "shaders/paint.comp",
    "void markPaintChunkDirty(ivec2 p) {\n"
    "    atomicOr(chunks[chunkIndex(p, pc.width)].flags, CHUNK_DIRTY | CHUNK_ACTIVE);\n"
    "}\n\n"
    "void main() {\n",
    "void markPaintChunkDirty(ivec2 p) {\n"
    "    atomicOr(chunks[chunkIndex(p, pc.width)].flags, CHUNK_DIRTY | CHUNK_ACTIVE);\n"
    "}\n\n"
    "const ivec2 paintDisplacementOffsets[8] = ivec2[8](\n"
    "    ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(-1, 0),\n"
    "    ivec2(1, 0), ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1));\n\n"
    "bool isDirectPaintLife(uint material) {\n"
    "    return material == MAT_BEE || material == MAT_QUEEN_BEE ||\n"
    "           material == MAT_ANT || material == MAT_BEETLE;\n"
    "}\n\n"
    "uint paintRepresentedGasVolume(Cell medium) {\n"
    "    if (!isGas(medium.material)) return 0u;\n"
    "    uint volume = stateValue(medium);\n"
    "    return volume == 0u ? 1u : volume;\n"
    "}\n\n"
    "bool displacePaintGas(ivec2 p, Cell medium) {\n"
    "    if (!isGas(medium.material)) return true;\n"
    "    uint remaining = paintRepresentedGasVolume(medium);\n"
    "    uint capacity = 0u;\n"
    "    for (uint i = 0u; i < 8u; ++i) {\n"
    "        ivec2 candidate = p + paintDisplacementOffsets[i];\n"
    "        if (!inside(candidate)) continue;\n"
    "        Cell receiver = cells[indexOf(candidate)];\n"
    "        if (receiver.material != medium.material) continue;\n"
    "        capacity += 255u - min(paintRepresentedGasVolume(receiver), 255u);\n"
    "    }\n"
    "    if (capacity < remaining) return false;\n"
    "    for (uint i = 0u; i < 8u && remaining != 0u; ++i) {\n"
    "        ivec2 candidate = p + paintDisplacementOffsets[i];\n"
    "        if (!inside(candidate)) continue;\n"
    "        uint receiverIndex = indexOf(candidate);\n"
    "        Cell receiver = cells[receiverIndex];\n"
    "        if (receiver.material != medium.material) continue;\n"
    "        uint represented = min(paintRepresentedGasVolume(receiver), 255u);\n"
    "        uint transfer = min(remaining, 255u - represented);\n"
    "        if (transfer == 0u) continue;\n"
    "        setStateValue(receiver, represented + transfer);\n"
    "        cells[receiverIndex] = receiver;\n"
    "        markPaintChunkDirty(candidate);\n"
    "        remaining -= transfer;\n"
    "    }\n"
    "    return remaining == 0u;\n"
    "}\n\n"
    "void main() {\n",
)
replace_once(
    "shaders/paint.comp",
    "    if ((material == MAT_SEED || material == MAT_QUEEN_BEE) && any(notEqual(p, center))) return;\n",
    "    if ((material == MAT_SEED || isDirectPaintLife(material)) && any(notEqual(p, center))) return;\n",
)
replace_once(
    "shaders/paint.comp",
    "    if (colonySlot >= 0) {\n"
    "        cell.aux |= BEE_AUX_SWARM | BEE_AUX_FED;\n"
    "        cell.aux = beePackMetadata(cell.aux, center, uint(colonySlot));\n"
    "        cell.age = beePackAge(uint((colonySlot * 17) % 900), BEE_TARGET_NONE);\n"
    "    }\n\n"
    "    uint index = indexOf(p);\n"
    "    Cell previous = cells[index];\n"
    "    recordConservation(previous, cell);\n",
    "    if (colonySlot >= 0) {\n"
    "        cell.aux |= BEE_AUX_SWARM | BEE_AUX_FED;\n"
    "        cell.aux = beePackMetadata(cell.aux, center, uint(colonySlot));\n"
    "        cell.age = beePackAge(uint((colonySlot * 17) % 900), BEE_TARGET_NONE);\n"
    "    } else if (material == MAT_BEE) {\n"
    "        uint slot = cellHash(center, 0xb33f1u) % BEE_FORMATION_COUNT;\n"
    "        cell.aux |= BEE_AUX_SWARM | BEE_AUX_FED;\n"
    "        cell.aux = beePackMetadata(cell.aux, center, slot);\n"
    "        cell.age = beePackAge(0u, BEE_TARGET_NONE);\n"
    "    }\n\n"
    "    uint index = indexOf(p);\n"
    "    Cell previous = cells[index];\n"
    "    if (isDirectPaintLife(material) && isGas(previous.material) &&\n"
    "        !displacePaintGas(p, previous)) return;\n"
    "    recordConservation(previous, cell);\n",
)

# Existing painted or loaded orphan bees self-seed a bounded local swarm target
# on their next chemistry update instead of remaining permanently frozen.
replace_once(
    "shaders/chemistry.comp",
    "            if (!authoredBee) {\n"
    "                // A painted or orphaned bee without a queen does not wander.\n"
    "                result.aux &= ~AUX_MOVED;\n"
    "            }\n",
    "            if (!authoredBee) {\n"
    "                // Painted and loaded orphan bees self-seed a bounded local swarm.\n"
    "                uint slot = hash32(indexOf(p) ^ source.aux ^ 0xb33f1u) % BEE_FORMATION_COUNT;\n"
    "                result.aux |= BEE_AUX_SWARM | BEE_AUX_FED;\n"
    "                result.aux = beePackMetadata(result.aux, p, slot);\n"
    "                result.age = beePackAge(0u, BEE_TARGET_NONE);\n"
    "            }\n",
)

# Give insects an explicit gas/liquid path that is independent of the grounded
# walking branch. Bees already use target motion; add the missing symmetric
# diagonal endpoint so a bee is not skipped merely because it occupies target.
replace_once(
    "shaders/move.comp",
    "bool isConveyorCargo(Cell cell) {\n",
    "bool isPassableLifeMedium(Cell medium) {\n"
    "    return isCellGas(medium) || isCellLiquid(medium);\n"
    "}\n\n"
    "bool insectMediumMoveAllowed(Cell insect, ivec2 sourcePosition,\n"
    "                             ivec2 targetPosition, uint randomValue) {\n"
    "    if ((insect.aux & AUX_MOVED) != 0u) return false;\n"
    "    int sourceSignal = insectTargetSignal(insect, sourcePosition);\n"
    "    int targetSignal = insectTargetSignal(insect, targetPosition);\n"
    "    if (sourceSignal != targetSignal) return targetSignal > sourceSignal;\n"
    "    ivec2 delta = targetPosition - sourcePosition;\n"
    "    if (delta.y > 0) return true;\n"
    "    if (delta.y < 0) return (randomValue & 15u) == 0u;\n"
    "    return insectMoveAllowed(insect, sourcePosition, targetPosition, randomValue);\n"
    "}\n\n"
    "bool isConveyorCargo(Cell cell) {\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b) &&\n"
    "        mergeHalfWaterPair(bottom, top, b, a)) return;\n\n"
    "    if (isInsect(a.material) && lifeCanEnter(b) && !insectGrounded(top) &&\n",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b) &&\n"
    "        mergeHalfWaterPair(bottom, top, b, a)) return;\n\n"
    "    if (isInsect(a.material) && isPassableLifeMedium(b) &&\n"
    "        insectMediumMoveAllowed(a, top, bottom, randomValue)) {\n"
    "        swapCells(top, bottom);\n"
    "        return;\n"
    "    }\n"
    "    if (isInsect(b.material) && isPassableLifeMedium(a) &&\n"
    "        insectMediumMoveAllowed(b, bottom, top, randomValue >> 1u)) {\n"
    "        swapCells(top, bottom);\n"
    "        return;\n"
    "    }\n\n"
    "    if (isInsect(a.material) && lifeCanEnter(b) && !insectGrounded(top) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isHalfWaterCell(source) && isHalfWaterCell(target) &&\n"
    "        mergeHalfWaterPair(targetPosition, sourcePosition, target, source)) return;\n\n"
    "    Cell below = sampleAt(sourcePosition + ivec2(0, 1));\n",
    "    if (isHalfWaterCell(source) && isHalfWaterCell(target) &&\n"
    "        mergeHalfWaterPair(targetPosition, sourcePosition, target, source)) return;\n\n"
    "    if (isInsect(source.material) && isPassableLifeMedium(target) &&\n"
    "        insectMediumMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {\n"
    "        swapCells(sourcePosition, targetPosition);\n"
    "        return;\n"
    "    }\n"
    "    if (isInsect(target.material) && isPassableLifeMedium(source) &&\n"
    "        insectMediumMoveAllowed(target, targetPosition, sourcePosition, randomValue >> 1u)) {\n"
    "        swapCells(sourcePosition, targetPosition);\n"
    "        return;\n"
    "    }\n\n"
    "    Cell below = sampleAt(sourcePosition + ivec2(0, 1));\n",
)
replace_once(
    "shaders/move.comp",
    "    if (source.material == MAT_BEE && lifeCanEnter(target) &&\n"
    "        beeMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {\n"
    "        swapCells(sourcePosition, targetPosition);\n"
    "        return;\n"
    "    }\n\n"
    "    if (isMagnetic(source.material) && target.material == MAT_EMPTY &&\n",
    "    if (source.material == MAT_BEE && lifeCanEnter(target) &&\n"
    "        beeMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {\n"
    "        swapCells(sourcePosition, targetPosition);\n"
    "        return;\n"
    "    }\n"
    "    if (target.material == MAT_BEE && lifeCanEnter(source) &&\n"
    "        beeMoveAllowed(target, targetPosition, sourcePosition, randomValue >> 1u)) {\n"
    "        swapCells(sourcePosition, targetPosition);\n"
    "        return;\n"
    "    }\n\n"
    "    if (isMagnetic(source.material) && target.material == MAT_EMPTY &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isInsect(a.material) && lifeCanEnter(b) &&\n"
    "        insectGrounded(left) && insectGrounded(right) &&\n",
    "    if (isInsect(a.material) && isPassableLifeMedium(b) &&\n"
    "        insectMediumMoveAllowed(a, left, right, randomValue)) {\n"
    "        swapCells(left, right);\n"
    "        return;\n"
    "    }\n"
    "    if (isInsect(b.material) && isPassableLifeMedium(a) &&\n"
    "        insectMediumMoveAllowed(b, right, left, randomValue >> 1u)) {\n"
    "        swapCells(left, right);\n"
    "        return;\n"
    "    }\n\n"
    "    if (isInsect(a.material) && lifeCanEnter(b) &&\n"
    "        insectGrounded(left) && insectGrounded(right) &&\n",
)

# Extend the repository's own source audits so future changes cannot silently
# restore frozen painted bees or grounded-only medium movement.
replace_once(
    "tools/audit_ecology_motion.py",
    "    \"move\": (root / \"shaders/move.comp\").read_text(encoding=\"utf-8\"),\n"
    "    \"swarm\": (root / \"shaders/bee_swarm.glsl\").read_text(encoding=\"utf-8\"),\n",
    "    \"move\": (root / \"shaders/move.comp\").read_text(encoding=\"utf-8\"),\n"
    "    \"paint\": (root / \"shaders/paint.comp\").read_text(encoding=\"utf-8\"),\n"
    "    \"swarm\": (root / \"shaders/bee_swarm.glsl\").read_text(encoding=\"utf-8\"),\n",
)
replace_once(
    "tools/audit_ecology_motion.py",
    "             \"Dense agents circulate around the moving hidden-mask target\", \"insectMoveAllowed\"],\n"
    "    \"swarm\": [\"beeOrbitTarget\", \"beeSwarmTarget\", \"beeBiohazardTargetOffset\",\n",
    "             \"Dense agents circulate around the moving hidden-mask target\", \"insectMoveAllowed\",\n"
    "             \"isPassableLifeMedium\", \"insectMediumMoveAllowed\",\n"
    "             \"target.material == MAT_BEE\"],\n"
    "    \"paint\": [\"isDirectPaintLife\", \"displacePaintGas\",\n"
    "              \"BEE_AUX_SWARM | BEE_AUX_FED\"],\n"
    "    \"swarm\": [\"beeOrbitTarget\", \"beeSwarmTarget\", \"beeBiohazardTargetOffset\",\n",
)
replace_once(
    "tools/audit_ecology_motion.py",
    "    \"chemistry\": [\"flowerDropsSeed\", \"grassFrontier\", \"stemMoisture\",\n"
    "                  \"source.material == MAT_PLANT_STEM\"],\n",
    "    \"chemistry\": [\"flowerDropsSeed\", \"grassFrontier\", \"stemMoisture\",\n"
    "                  \"source.material == MAT_PLANT_STEM\",\n"
    "                  \"Painted and loaded orphan bees self-seed\"],\n",
)
replace_once(
    "tools/audit_ecology_motion.py",
    "print(\"Ecology and motion contracts passed: dynamic tiles, hidden swarm masks, insects, grass, seeds, stems, and flowers.\")\n",
    "print(\"Ecology and motion contracts passed: dynamic tiles, mobile painted bees, passable-media insects, gas displacement, grass, seeds, stems, and flowers.\")\n",
)

replace_once(
    "tools/validate_shader_contracts.py",
    "    actor_comp = (SHADERS / \"actor.comp\").read_text(encoding=\"utf-8\")\n"
    "    reset_comp = (SHADERS / \"reset.comp\").read_text(encoding=\"utf-8\")\n",
    "    actor_comp = (SHADERS / \"actor.comp\").read_text(encoding=\"utf-8\")\n"
    "    paint_comp = (SHADERS / \"paint.comp\").read_text(encoding=\"utf-8\")\n"
    "    move_comp = (SHADERS / \"move.comp\").read_text(encoding=\"utf-8\")\n"
    "    chemistry_comp = (SHADERS / \"chemistry.comp\").read_text(encoding=\"utf-8\")\n"
    "    reset_comp = (SHADERS / \"reset.comp\").read_text(encoding=\"utf-8\")\n",
)
replace_once(
    "tools/validate_shader_contracts.py",
    "    for token in (\"oxygenVolume > 0u\", \"fullyChoked\", \"state.health -= 1u\"):\n"
    "        if token not in actor_comp:\n"
    "            errors.append(f\"closed-system atmosphere contract missing {token!r}\")\n"
    "    if \"std::jthread\" in app_cpp or \"stop_token\" in app_cpp or \"request_stop\" in app_cpp:\n",
    "    for token in (\"oxygenVolume > 0u\", \"fullyChoked\", \"state.health -= 1u\"):\n"
    "        if token not in actor_comp:\n"
    "            errors.append(f\"closed-system atmosphere contract missing {token!r}\")\n"
    "    for token in (\"isDirectPaintLife\", \"displacePaintGas\",\n"
    "                  \"BEE_AUX_SWARM | BEE_AUX_FED\"):\n"
    "        if token not in paint_comp:\n"
    "            errors.append(f\"painted life/oxygen displacement contract missing {token!r}\")\n"
    "    for token in (\"isPassableLifeMedium\", \"insectMediumMoveAllowed\",\n"
    "                  \"target.material == MAT_BEE\"):\n"
    "        if token not in move_comp:\n"
    "            errors.append(f\"passable life movement contract missing {token!r}\")\n"
    "    if \"Painted and loaded orphan bees self-seed\" not in chemistry_comp:\n"
    "        errors.append(\"painted bee activation contract missing\")\n"
    "    if \"std::jthread\" in app_cpp or \"stop_token\" in app_cpp or \"request_stop\" in app_cpp:\n",
)

print("Applied focused painted-life, oxygen displacement, and passable-media movement patch.")
