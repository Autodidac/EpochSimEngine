#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"Expected text not found in {path}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"Expected one regex match in {path}, found {count}: {pattern}")
    write(path, updated)


# -----------------------------------------------------------------------------
# Canonical catalog: legacy IDs remain stable for save compatibility, but ore
# blocks are no longer exposed, authored, mined, or used as resource deposits.
# -----------------------------------------------------------------------------
catalog = read("tools/material_catalog.py")
catalog = re.sub(
    r"\('gold_ore', 'Gold ore'.*?\),\n",
    "    ('gold_ore', 'Gold concentrate', 96, 20, 1064, 246, 'STRONG: LEGACY SAVE COMPATIBILITY', 'WEAK: NOT AUTHORED', 'TO: GOLD PIXELS', 'ROLE: DEPRECATED RESOURCE ID', 'DANGER: NONE'),\n",
    catalog,
    count=1,
)
catalog = re.sub(
    r"\('iron_ore', 'Iron ore'.*?\),\n",
    "    ('iron_ore', 'Iron concentrate', 88, 20, 1450, 244, 'STRONG: LEGACY SAVE COMPATIBILITY', 'WEAK: NOT AUTHORED', 'TO: IRON PIXELS', 'ROLE: DEPRECATED RESOURCE ID', 'DANGER: NONE'),\n",
    catalog,
    count=1,
)
catalog = catalog.replace("'ROLE: ORE SEPARATION'", "'ROLE: METAL SEPARATION'")
catalog = catalog.replace("'STRONG: ORE PROCESSING'", "'STRONG: METAL PROCESSING'")
catalog = catalog.replace(
    "('engineering', 'Engineering', [38, 40, 42, 43, 44, 48, 54, 55]),",
    "('engineering', 'Engineering', [38, 40, 54, 55]),",
)
catalog = catalog.replace(
    "    'metal', 'glass', 'iron', 'copper', 'magnet', 'insulator', 'uranium',\n"
    "    'gold_ore', 'iron_ore', 'steel', 'conveyor', 'smelter', 'assembler',",
    "    'metal', 'glass', 'iron', 'copper', 'gold', 'magnet', 'insulator', 'uranium',\n"
    "    'steel', 'conveyor', 'smelter', 'assembler',",
)
write("tools/material_catalog.py", catalog)

replace_once(
    "tools/generate_ui_text.py",
    '    "Gold mine", "Demolition", "Frontier base",',
    '    "Platformer", "Demolition", "Frontier base",',
)
replace_once(
    "include/epoch/sand/scene.hpp",
    '    "Gold mine", "Demolition", "Frontier base"',
    '    "Platformer", "Demolition", "Frontier base"',
)

subprocess.run([sys.executable, str(ROOT / "tools/generate_ui_text.py")], cwd=ROOT, check=True)

# -----------------------------------------------------------------------------
# Input and tool ownership: character scenes always own the player tools, and
# Win32 movement is refreshed from actual key state to survive lost messages.
# -----------------------------------------------------------------------------
replace_once(
    "src/app.cpp",
    """        const bool over_simulation = epochengine::gui_lib::contains(layout.simulation, pointer);
        const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
        const bool inspecting = input.inspect_material;
        shared_state.primary_down.store(input.primary_down && over_simulation && !mining && !inspecting,
                                         std::memory_order_relaxed);
        shared_state.secondary_down.store(input.secondary_down && over_simulation && !mining && !inspecting,
                                           std::memory_order_relaxed);
        const bool tool_active = over_simulation && mining && !inspecting;
""",
    """        const bool over_simulation = epochengine::gui_lib::contains(layout.simulation, pointer);
        const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
        const bool character_scene = scene_has_character(scene);
        const bool inspecting = input.inspect_material;
        const bool paint_active = over_simulation && !character_scene && !mining && !inspecting;
        shared_state.primary_down.store(input.primary_down && paint_active,
                                         std::memory_order_relaxed);
        shared_state.secondary_down.store(input.secondary_down && paint_active,
                                           std::memory_order_relaxed);
        // Character scenes always retain mining/shooting. The mode toggle cannot
        // accidentally route player clicks back into world painting.
        const bool tool_active = over_simulation && (character_scene || mining) && !inspecting;
""",
)

replace_once(
    "src/window_win32.cpp",
    """        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    input = WindowInput{
""",
    """        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    // Refresh continuous movement from physical key state. This prevents focus,
    // capture, or message coalescing from dropping a key-up/down transition and
    // silently disabling player movement.
    const bool focused = GetForegroundWindow() == impl_->handle;
    const auto key_down = [focused](const int key) noexcept {
        return focused && (GetAsyncKeyState(key) & 0x8000) != 0;
    };
    impl_->move_left = key_down('A') || key_down(VK_LEFT);
    impl_->move_right = key_down('D') || key_down(VK_RIGHT);
    impl_->move_up = key_down('W') || key_down(VK_UP);
    impl_->move_down = key_down('S') || key_down(VK_DOWN);
    impl_->jump = impl_->move_up;

    input = WindowInput{
""",
)

replace_once(
    "src/vulkan_renderer.cpp",
    """        const bool actor_action = state.fire_tool.load(std::memory_order_relaxed) ||
                                  state.deposit_resource.load(std::memory_order_relaxed) ||
                                  state.fire_tool_pressed.load(std::memory_order_acquire) ||
                                  state.deposit_resource_pressed.load(std::memory_order_acquire);
        if (run_simulation || reset_actor || actor_action)
            record_actor(frame.command_buffer, state, reset_actor, run_simulation);
""",
    """        const bool actor_action = state.fire_tool.load(std::memory_order_relaxed) ||
                                  state.deposit_resource.load(std::memory_order_relaxed) ||
                                  state.fire_tool_pressed.load(std::memory_order_acquire) ||
                                  state.deposit_resource_pressed.load(std::memory_order_acquire);
        const bool actor_motion = state.move_x.load(std::memory_order_relaxed) != 0 ||
                                  state.move_y.load(std::memory_order_relaxed) != 0 ||
                                  state.jump.load(std::memory_order_relaxed);
        const bool actor_simulation = run_simulation ||
                                      (actor_motion && !state.paused.load(std::memory_order_relaxed));
        if (run_simulation || reset_actor || actor_action || actor_motion)
            record_actor(frame.command_buffer, state, reset_actor, actor_simulation);
""",
)

# -----------------------------------------------------------------------------
# Actor: never consume structural platforms, collect only loose metal pixels,
# remove ore conversion, keep ambient breathing nonlethal, and spawn correctly
# in the dedicated platformer scene.
# -----------------------------------------------------------------------------
replace_regex(
    "shaders/actor.comp",
    r"bool blocksActor\(Cell cell\) \{.*?\n\}",
    """bool blocksActor(Cell cell) {
    if (cell.material == MAT_EMPTY || isGas(cell.material) || isLiquid(cell.material) ||
        cell.material == MAT_BEE || cell.material == MAT_POLLEN ||
        cell.material == MAT_FLOWER || cell.material == MAT_GRASS ||
        cell.material == MAT_ALLY_BOT) return false;
    bool loosePickup = !isStructural(cell) &&
        (cell.material == MAT_GOLD || cell.material == MAT_IRON ||
         cell.material == MAT_COPPER || cell.material == MAT_METAL ||
         cell.material == MAT_POWER_CELL || cell.material == MAT_PLASMA_AMMO);
    return !loosePickup;
}""",
)
replace_regex(
    "shaders/actor.comp",
    r"void collectResources\(inout ActorState state\) \{.*?\n\}",
    """void collectResources(inout ActorState state) {
    ivec2 center = ivec2(state.x, state.y);
    for (int y = -7; y <= 1; ++y) {
        for (int x = -3; x <= 3; ++x) {
            ivec2 p = center + ivec2(x, y);
            if (!actorInside(p)) continue;
            uint index = actorIndex(p);
            Cell resourceCell = cells[index];
            if (isStructural(resourceCell)) continue;
            uint material = resourceCell.material;
            if (material == MAT_GOLD) {
                atomicAdd(conservation[CONS_CONVERTED], 1u);
                cells[index] = Cell(MAT_EMPTY, 0u, 20, 0u);
                state.gold = min(state.gold + 1u, 9999u);
            } else if (material == MAT_IRON || material == MAT_COPPER ||
                       material == MAT_METAL || material == MAT_STEEL) {
                atomicAdd(conservation[CONS_CONVERTED], 1u);
                cells[index] = Cell(MAT_EMPTY, 0u, 20, 0u);
                state.iron = min(state.iron + 1u, 9999u);
            } else if (material == MAT_PLASMA_AMMO) {
                atomicAdd(conservation[CONS_CONVERTED], 1u);
                cells[index] = Cell(MAT_EMPTY, 0u, 20, 0u);
                state.ammo = min(state.ammo + 1u, 999u);
            }
        }
    }
}""",
)
actor = read("shaders/actor.comp")
actor = actor.replace(
    """            Cell fragment = cell;
            if (cell.material == MAT_GOLD_ORE) fragment = makeCellWithEntropy(MAT_GOLD, actorPc.seed, actorPc.step);
            else if (cell.material == MAT_IRON_ORE) fragment = makeCellWithEntropy(MAT_IRON, actorPc.seed, actorPc.step);
            else {
                fragment.aux &= ~(AUX_STRUCTURAL | AUX_SUPPORTED);
                fragment.aux |= AUX_MOVED;
                fragment.age = 0u;
                setStateValue(fragment, 1u);
            }
""",
    """            Cell fragment = cell;
            fragment.aux &= ~(AUX_STRUCTURAL | AUX_SUPPORTED);
            fragment.aux |= AUX_MOVED;
            fragment.age = 0u;
            setStateValue(fragment, 1u);
""",
)
actor = actor.replace(
    """    Cell fragment = cell;
    if (cell.material == MAT_GOLD_ORE) fragment = makeCellWithEntropy(MAT_GOLD, actorPc.seed, actorPc.step);
    else if (cell.material == MAT_IRON_ORE) fragment = makeCellWithEntropy(MAT_IRON, actorPc.seed, actorPc.step);
    else {
        fragment.age = 0u;
        fragment.aux |= AUX_MOVED;
    }
""",
    """    Cell fragment = cell;
    fragment.age = 0u;
    fragment.aux |= AUX_MOVED;
""",
)
write("shaders/actor.comp", actor)

replace_regex(
    "shaders/actor.comp",
    r"void updateBreathing\(inout ActorState state\) \{.*?\n\}",
    """void updateBreathing(inout ActorState state) {
    ivec2 center = ivec2(state.x, state.y - 4);
    ivec2 oxygenCell = center;
    uint emptyCells = 0u;
    uint oxygenCells = 0u;
    uint toxicCells = 0u;
    for (int y = -4; y <= 4; ++y) {
        for (int x = -4; x <= 4; ++x) {
            ivec2 p = center + ivec2(x, y);
            uint material = actorAt(p).material;
            if (material == MAT_EMPTY) ++emptyCells;
            if (material == MAT_OXYGEN) {
                if (oxygenCells == 0u) oxygenCell = p;
                ++oxygenCells;
            }
            if (material == MAT_CARBON_DIOXIDE || material == MAT_SMOKE ||
                material == MAT_DIRTY_STEAM) ++toxicCells;
        }
    }

    bool ambientAir = emptyCells >= 12u && toxicCells < 18u;
    bool breathable = ambientAir || oxygenCells >= 2u;
    bool toxicPocket = toxicCells >= 18u && toxicCells > emptyCells + oxygenCells;
    if (breathable && !toxicPocket) {
        state.oxygen = min(255u, state.oxygen + 3u);
        state.exposureTicks = 0u;
        if (!ambientAir && oxygenCells > 0u && (actorPc.step % 180u) == 0u && actorInside(oxygenCell)) {
            uint oxygenIndex = actorIndex(oxygenCell);
            Cell carbonDioxide = makeCellWithEntropy(MAT_CARBON_DIOXIDE, actorPc.seed, actorPc.step);
            recordConservation(cells[oxygenIndex], carbonDioxide);
            cells[oxygenIndex] = carbonDioxide;
        }
    } else {
        state.exposureTicks = min(state.exposureTicks + 1u, 4095u);
        if ((actorPc.step % 60u) == 0u && state.oxygen > 0u)
            state.oxygen -= toxicPocket ? min(state.oxygen, 2u) : 1u;
    }
    // Atmosphere affects the oxygen meter only. Health is reserved for explicit
    // contact damage, so classification noise can never kill the player.
}""",
)
replace_regex(
    "shaders/actor.comp",
    r"void resetActor\(inout ActorState state\) \{.*?\n\}",
    """void resetActor(inout ActorState state) {
    state = ActorState(0, 0, 0, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                       255u, 255u, 0, 0, actorPc.scene, 0u);
    if (actorPc.scene == SCENE_GOLD_MINE) {
        state.x = 78;
        state.y = 127;
        state.ammo = 18u;
        state.enabled = 1u;
    } else if (actorPc.scene == SCENE_DEMOLITION) {
        state.x = int(actorPc.width / 8u);
        state.y = int(actorPc.height) - 36;
        state.ammo = 12u;
        state.enabled = 1u;
    } else if (actorPc.scene == SCENE_FRONTIER_BASE) {
        state.x = 168;
        state.y = 200;
        state.ammo = 24u;
        state.enabled = 1u;
    }
    state.hitX = state.x;
    state.hitY = state.y - 4;
}""",
)
replace_once(
    "shaders/actor.comp",
    "                state.moveCooldown = 1u;",
    "                state.moveCooldown = 2u;",
)

# -----------------------------------------------------------------------------
# Rebuild the three visibly broken scenes. Scene ID 6 stays stable but becomes
# the dedicated platformer scene requested by the user.
# -----------------------------------------------------------------------------
volcano = r'''uint volcanoMaterial(ivec2 p) {
    int floorY = int(pc.height) - 10;
    int centerX = int(pc.width / 2u);
    int coneTop = max(22, floorY - 198);
    int vertical = floorY - p.y;
    uint material = MAT_EMPTY;

    if (p.y >= floorY) return MAT_STONE;
    int halfWidth = max(0, 194 - vertical * 9 / 10);
    if (p.y >= coneTop && vertical >= 0 && abs(p.x - centerX) < halfWidth)
        material = MAT_STONE;

    int chamberTop = floorY - 86;
    int chamberBottom = floorY - 18;
    int dx = p.x - centerX;
    int chamberY = (chamberTop + chamberBottom) / 2;
    int dy = p.y - chamberY;
    if (dx * dx * 4 + dy * dy * 9 < 92 * 92) material = MAT_LAVA;
    if (abs(dx) <= 7 && p.y >= coneTop + 12 && p.y <= chamberTop + 20) material = MAT_LAVA;
    if (p.y >= coneTop && p.y < coneTop + 18 && abs(dx) < 24)
        material = p.y >= coneTop + 8 ? MAT_LAVA : MAT_EMPTY;
    if (rectContains(p, ivec2(centerX - 5, chamberBottom - 10),
                         ivec2(centerX + 6, chamberBottom))) material = MAT_MAGMA_VENT;

    int lakeLeft = min(int(pc.width) - 126, centerX + 150);
    int lakeRight = min(int(pc.width) - 18, centerX + 268);
    int lakeTop = floorY - 58;
    bool lakeWall = (p.x == lakeLeft || p.x == lakeRight || p.y == floorY - 1) &&
                    p.x >= lakeLeft && p.x <= lakeRight && p.y >= lakeTop && p.y < floorY;
    if (lakeWall) material = MAT_STONE;
    if (p.x > lakeLeft && p.x < lakeRight && p.y > lakeTop + 12 && p.y < floorY - 1)
        material = MAT_WATER;

    if (p.y < coneTop && p.y > max(3, coneTop - 92) && abs(dx) < 72 &&
        (hash32(indexOf(p) ^ pc.seed ^ 0x51a9u) % 17u) == 0u) material = MAT_SMOKE;
    return material;
}'''
replace_regex("shaders/reset.comp", r"uint volcanoMaterial\(ivec2 p\) \{.*?\n\}\n\nuint waterworksMaterial", volcano + "\n\nuint waterworksMaterial")

engineering = r'''uint engineeringMaterial(ivec2 p) {
    int floorY = int(pc.height) - 12;
    uint material = p.y >= floorY ? MAT_INSULATOR : MAT_EMPTY;

    // Left station: contained water/steam thermal vessel on a steel workbench.
    int tankLeft = 24;
    int tankRight = 188;
    int tankTop = 48;
    int tankBottom = 246;
    bool tankWall = (p.x == tankLeft || p.x == tankRight || p.y == tankTop || p.y == tankBottom) &&
                    p.x >= tankLeft && p.x <= tankRight && p.y >= tankTop && p.y <= tankBottom;
    if (tankWall) material = MAT_GLASS;
    if (p.x > tankLeft && p.x < tankRight && p.y > 132 && p.y < tankBottom) material = MAT_WATER;
    if (p.x > tankLeft + 8 && p.x < tankRight - 8 && p.y > tankTop + 10 && p.y < 118 &&
        (hash32(indexOf(p) ^ pc.seed) & 7u) == 0u) material = MAT_STEAM;
    if (rectContains(p, ivec2(78, tankBottom + 2), ivec2(134, tankBottom + 12))) material = MAT_COPPER;
    if (rectContains(p, ivec2(101, tankBottom - 5), ivec2(111, tankBottom + 2))) material = MAT_EMBER;

    // Center station: a real sediment sifter. Sand/silt and independent metal
    // pixels fall through a narrow outlet toward a magnet and separated bins.
    int hopperLeft = 226;
    int hopperRight = 390;
    int hopperTop = 42;
    int hopperBottom = 150;
    bool hopperWall = (p.x == hopperLeft || p.x == hopperRight || p.y == hopperTop) &&
                      p.x >= hopperLeft && p.x <= hopperRight && p.y >= hopperTop && p.y <= hopperBottom;
    if (hopperWall) material = MAT_STEEL;
    if (p.y == hopperBottom && p.x >= hopperLeft && p.x <= hopperRight && abs(p.x - 308) > 7)
        material = MAT_STEEL;
    if (p.x > hopperLeft && p.x < hopperRight && p.y > hopperTop + 8 && p.y < hopperBottom) {
        uint mix = hash32(indexOf(p) ^ pc.seed ^ 0x5f17u);
        if ((mix % 71u) == 0u) material = MAT_GOLD;
        else if ((mix % 43u) == 0u) material = MAT_COPPER;
        else if ((mix % 23u) == 0u) material = MAT_IRON;
        else material = (mix & 3u) == 0u ? MAT_SILT : MAT_SAND;
    }
    if (rectContains(p, ivec2(410, 154), ivec2(444, 214))) material = MAT_MAGNET;
    if (rectContains(p, ivec2(232, 246), ivec2(438, 254))) material = MAT_STEEL;
    if ((p.x == 232 || p.x == 332 || p.x == 438) && p.y >= 214 && p.y <= 306) material = MAT_GLASS;
    if (p.y == 306 && p.x >= 232 && p.x <= 438) material = MAT_GLASS;

    // Right station: sealed, readable pink hydrogen and cyan oxygen chamber.
    int gasLeft = int(pc.width) - 172;
    int gasRight = int(pc.width) - 24;
    int gasTop = 42;
    int gasBottom = 164;
    bool gasWall = (p.x == gasLeft || p.x == gasRight || p.y == gasTop || p.y == gasBottom) &&
                   p.x >= gasLeft && p.x <= gasRight && p.y >= gasTop && p.y <= gasBottom;
    if (gasWall) material = MAT_GLASS;
    if (p.x > gasLeft && p.x < gasRight && p.y > gasTop && p.y < gasBottom) {
        int split = (gasLeft + gasRight) / 2;
        material = p.x < split ? MAT_HYDROGEN : MAT_OXYGEN;
    }
    if (rectContains(p, ivec2((gasLeft + gasRight) / 2 - 2, gasBottom - 9),
                         ivec2((gasLeft + gasRight) / 2 + 3, gasBottom))) material = MAT_EMBER;

    if (p.y >= floorY - 8 && p.y < floorY && p.x > 12 && p.x < int(pc.width) - 12)
        material = MAT_STEEL;
    return material;
}'''
replace_regex("shaders/reset.comp", r"uint engineeringMaterial\(ivec2 p\) \{.*?\n\}\n\nuint goldMineMaterial", engineering + "\n\nuint goldMineMaterial")

platformer = r'''uint goldMineMaterial(ivec2 p) {
    int width = int(pc.width);
    int height = int(pc.height);
    int floorY = height - 10;
    int upperFloor = 130;
    int middleFloor = 210;
    int lowerFloor = 286;
    uint material = MAT_EMPTY;

    if (p.y >= floorY) return MAT_STONE;
    if ((p.x < 12 || p.x >= width - 12) && p.y >= 18) material = MAT_STONE;

    // Tall left water tower and controlled waterfall.
    int towerLeft = 28;
    int towerRight = 132;
    int towerTop = 30;
    int towerBottom = upperFloor;
    bool towerWall = (p.x == towerLeft || p.x == towerRight || p.y == towerTop || p.y == towerBottom) &&
                     p.x >= towerLeft && p.x <= towerRight && p.y >= towerTop && p.y <= towerBottom;
    if (towerWall) material = MAT_STEEL;
    if (p.x > towerLeft && p.x < towerRight && p.y > towerTop + 8 && p.y < 78) material = MAT_WATER;
    if (p.x >= 74 && p.x <= 80 && p.y >= 78 && p.y < towerBottom) material = MAT_WATER;
    if ((p.x == 70 || p.x == 84) && p.y >= 74 && p.y < towerBottom) material = MAT_GLASS;

    // Large upper reservoir and filter bed, matching the reference layout.
    int reservoirLeft = 176;
    int reservoirRight = width - 28;
    int reservoirTop = 24;
    int reservoirBottom = 106;
    bool reservoirWall = (p.x == reservoirLeft || p.x == reservoirRight ||
                          p.y == reservoirTop || p.y == reservoirBottom) &&
                         p.x >= reservoirLeft && p.x <= reservoirRight &&
                         p.y >= reservoirTop && p.y <= reservoirBottom;
    if (reservoirWall) material = MAT_STEEL;
    if (p.x > reservoirLeft && p.x < reservoirRight && p.y > reservoirTop && p.y < 84)
        material = MAT_WATER;
    if (p.x > reservoirLeft && p.x < reservoirRight && p.y >= 84 && p.y < reservoirBottom)
        material = ((p.x / 8) & 1) == 0 ? MAT_STONE : MAT_GRASS;

    // Upper player deck.
    if (p.y >= upperFloor && p.y < upperFloor + 6 && p.x >= 22 && p.x < 226)
        material = MAT_STEEL;
    if (p.x >= 22 && p.x < 30 && p.y >= upperFloor && p.y < middleFloor)
        material = MAT_STEEL;

    // Middle deck, a loose sand pile with independent metal pixels, and a water room.
    if (p.y >= middleFloor && p.y < middleFloor + 6 && p.x >= 22 && p.x < width - 22)
        material = MAT_STEEL;
    ivec2 pileCenter = ivec2(286, middleFloor);
    ivec2 pile = p - pileCenter;
    if (pile.y <= 0 && pile.y > -58 && abs(pile.x) < 68 + pile.y) {
        uint mix = hash32(indexOf(p) ^ pc.seed ^ 0x91f3u);
        if ((mix % 97u) == 0u) material = MAT_GOLD;
        else if ((mix % 41u) == 0u) material = MAT_COPPER;
        else if ((mix % 29u) == 0u) material = MAT_IRON;
        else material = MAT_SAND;
    }

    int poolLeft = width - 176;
    int poolRight = width - 32;
    int poolTop = 146;
    int poolBottom = middleFloor;
    bool poolWall = (p.x == poolLeft || p.x == poolRight || p.y == poolTop || p.y == poolBottom) &&
                    p.x >= poolLeft && p.x <= poolRight && p.y >= poolTop && p.y <= poolBottom;
    if (poolWall) material = MAT_GLASS;
    if (p.x > poolLeft && p.x < poolRight && p.y > poolTop + 12 && p.y < poolBottom)
        material = MAT_WATER;

    // Reachable steel ramps connect every level without teleporting the actor.
    if (p.x >= 126 && p.x < 226) {
        int rampY = middleFloor - ((p.x - 126) * (middleFloor - upperFloor)) / 100;
        if (p.y >= rampY && p.y < rampY + 3) material = MAT_STEEL;
    }
    if (p.x >= 34 && p.x < 134) {
        int rampY = lowerFloor - ((p.x - 34) * (lowerFloor - middleFloor)) / 100;
        if (p.y >= rampY && p.y < rampY + 3) material = MAT_STEEL;
    }

    // Lower combat floor and a contained lava hazard trench.
    if (p.y >= lowerFloor && p.y < lowerFloor + 6 && p.x >= 22 && p.x < width - 22)
        material = MAT_STEEL;
    int trenchLeft = 258;
    int trenchRight = 388;
    if ((p.x == trenchLeft || p.x == trenchRight) && p.y >= lowerFloor && p.y < floorY)
        material = MAT_STONE;
    if (p.x > trenchLeft && p.x < trenchRight && p.y > lowerFloor + 18 && p.y < floorY)
        material = MAT_LAVA;
    if (p.y == lowerFloor - 1 && p.x > width - 150 && p.x < width - 46 && (p.x % 23) == 0)
        material = MAT_ENEMY_BOT;
    if (rectContains(p, ivec2(430, lowerFloor - 10), ivec2(446, lowerFloor)))
        material = MAT_PLASMA_AMMO;

    return material;
}'''
replace_regex("shaders/reset.comp", r"uint goldMineMaterial\(ivec2 p\) \{.*?\n\}\n\nuint demolitionMaterial", platformer + "\n\nuint demolitionMaterial")

reset = read("shaders/reset.comp")
reset = re.sub(
    r"\n\s*ivec2 coarse = p / int\(STRUCTURAL_BLOCK_SIZE\);.*?\n\s*\}\n\s*\}\n\n\s*// Underground base shell",
    "\n    }\n\n    // Underground base shell",
    reset,
    count=1,
    flags=re.S,
)
reset = reset.replace(
    """    if (scene == SCENE_ENGINEERING && material == MAT_IRON) {
        ivec2 magnetCenter = ivec2(int(pc.width / 2u), int(pc.height - 110u));
        if (p.y > magnetCenter.y - 96 && p.y < magnetCenter.y - 42 &&
            abs(p.x - magnetCenter.x) < 54) return true;
    }
""",
    """    if (scene == SCENE_ENGINEERING &&
        rectContains(p, ivec2(227, 51), ivec2(390, 150)) &&
        (material == MAT_IRON || material == MAT_COPPER || material == MAT_GOLD)) return true;
    if (scene == SCENE_GOLD_MINE &&
        (material == MAT_IRON || material == MAT_COPPER || material == MAT_GOLD)) return true;
""",
)
reset = reset.replace(
    "    if (scene == SCENE_ENGINEERING && material == MAT_SAND) return true;\n"
    "    if (scene == SCENE_GOLD_MINE && material == MAT_SAND) return true;",
    "    if (scene == SCENE_ENGINEERING && (material == MAT_SAND || material == MAT_SILT)) return true;\n"
    "    if (scene == SCENE_GOLD_MINE && (material == MAT_SAND || material == MAT_SILT)) return true;",
)
reset = reset.replace("walls, slopes, shells, ore strata, machines, and terrain", "walls, slopes, shells, machines, and terrain")
if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset:
    raise RuntimeError("Authored scenes still contain ore blocks")
write("shaders/reset.comp", reset)

# -----------------------------------------------------------------------------
# Material motion: real powders remain powders; loose metal resources are solid
# pixels. Legacy concentrate IDs cannot become blocks or appear in authored maps.
# -----------------------------------------------------------------------------
materials = read("shaders/materials.glsl")
materials = materials.replace(
    """    return material == MAT_SAND || material == MAT_DIRT || material == MAT_MUD || material == MAT_SALT ||
           material == MAT_ASH || material == MAT_GUNPOWDER || material == MAT_SNOW ||
           material == MAT_SEED || material == MAT_POLLEN || material == MAT_GOLD ||
           material == MAT_IRON || material == MAT_STEEL || material == MAT_POWER_CELL ||
           material == MAT_PLASMA_AMMO || material == MAT_SILT ||
           material == MAT_FERTILIZER || material == MAT_FOOD || material == MAT_WASTE;
""",
    """    return material == MAT_SAND || material == MAT_DIRT || material == MAT_MUD || material == MAT_SALT ||
           material == MAT_ASH || material == MAT_GUNPOWDER || material == MAT_SNOW ||
           material == MAT_SEED || material == MAT_POLLEN || material == MAT_SILT ||
           material == MAT_FERTILIZER || material == MAT_FOOD || material == MAT_WASTE;
""",
)
materials = materials.replace("material == MAT_URANIUM || material == MAT_GOLD_ORE || material == MAT_IRON_ORE ||\n           material == MAT_STEEL", "material == MAT_URANIUM || material == MAT_STEEL")
materials = materials.replace("material == MAT_URANIUM || material == MAT_GOLD_ORE ||\n            material == MAT_IRON_ORE || material == MAT_STEEL", "material == MAT_URANIUM || material == MAT_STEEL")
materials = materials.replace("material == MAT_URANIUM || material == MAT_GOLD_ORE || material == MAT_IRON_ORE ||\n            material == MAT_STEEL", "material == MAT_URANIUM || material == MAT_STEEL")
materials = materials.replace("material == MAT_METAL || material == MAT_GLASS || material == MAT_IRON ||\n           material == MAT_COPPER", "material == MAT_METAL || material == MAT_GLASS || material == MAT_IRON ||\n           material == MAT_COPPER || material == MAT_GOLD")
write("shaders/materials.glsl", materials)

movement = read("shaders/move.comp")
movement = movement.replace(
    """    return isPowderCellMaterial(material) || material == MAT_GOLD ||
           material == MAT_IRON || material == MAT_STEEL ||
           material == MAT_POWER_CELL || material == MAT_PLASMA_AMMO;
""",
    """    return isPowderCellMaterial(material) || material == MAT_GOLD ||
           material == MAT_IRON || material == MAT_COPPER || material == MAT_METAL ||
           material == MAT_STEEL || material == MAT_POWER_CELL ||
           material == MAT_PLASMA_AMMO;
""",
)
movement = movement.replace("material == MAT_METAL || material == MAT_GLASS || material == MAT_IRON ||\n            material == MAT_COPPER", "material == MAT_METAL || material == MAT_GLASS || material == MAT_IRON ||\n            material == MAT_COPPER || material == MAT_GOLD")
movement = movement.replace("material == MAT_URANIUM || material == MAT_GOLD_ORE || material == MAT_IRON_ORE ||\n            material == MAT_STEEL", "material == MAT_URANIUM || material == MAT_STEEL")
movement = movement.replace("(material == MAT_GOLD_ORE && temperature >= 1064) ||\n            (material == MAT_GOLD && temperature >= 1064) ||\n            (material == MAT_IRON_ORE && temperature >= 1538) ||", "(material == MAT_GOLD && temperature >= 1064) ||")
write("shaders/move.comp", movement)

# -----------------------------------------------------------------------------
# Larger UI matching the hit-test layout.
# -----------------------------------------------------------------------------
ui_layout = read("include/epoch/sand/ui_layout.hpp")
ui_layout = ui_layout.replace("status_height = 60u", "status_height = 72u")
ui_layout = ui_layout.replace("group_tabs_height = 40u", "group_tabs_height = 48u")
ui_layout = ui_layout.replace("palette_items_height = 64u", "palette_items_height = 76u")
ui_layout = ui_layout.replace("right - 92.0f), 10.0f}, {92.0f, 40.0f", "right - 104.0f), 12.0f}, {104.0f, 48.0f")
ui_layout = ui_layout.replace("right - 220.0f), 10.0f}, {124.0f, 40.0f", "right - 244.0f), 12.0f}, {136.0f, 48.0f")
ui_layout = ui_layout.replace("safe_width >= 920u", "safe_width >= 1040u")
ui_layout = ui_layout.replace("right - 316.0f, 10.0f}, {92.0f, 40.0f", "right - 352.0f, 12.0f}, {104.0f, 48.0f")
ui_layout = ui_layout.replace("right - 388.0f, 10.0f}, {68.0f, 40.0f", "right - 434.0f, 12.0f}, {78.0f, 48.0f")
ui_layout = ui_layout.replace("right - 460.0f, 10.0f}, {68.0f, 40.0f", "right - 516.0f, 12.0f}, {78.0f, 48.0f")
write("include/epoch/sand/ui_layout.hpp", ui_layout)

fullscreen = read("shaders/fullscreen.frag")
fullscreen = fullscreen.replace("int[5](68, 68, 92, 124, 92)", "int[5](78, 78, 104, 136, 104)")
fullscreen = fullscreen.replace("renderPc.windowWidth >= 920u", "renderPc.windowWidth >= 1040u")
fullscreen = fullscreen.replace("y >= 10u && y < 50u", "y >= 12u && y < 60u")
fullscreen = fullscreen.replace("fixedPixel(pixel, ivec2(10, 11), 3, 0u)", "fixedPixel(pixel, ivec2(12, 12), 4, 0u)")
fullscreen = fullscreen.replace("fixedPixel(pixel, ivec2(fpsX, 11), 3, 1u)", "fixedPixel(pixel, ivec2(fpsX, 12), 4, 1u)")
fullscreen = fullscreen.replace("numberPixel(pixel, ivec2(fpsX + 62, 11), 3", "numberPixel(pixel, ivec2(fpsX + 82, 12), 4")
fullscreen = fullscreen.replace("starts[0] + 11, 23), 2", "starts[0] + 12, 29), 2")
fullscreen = fullscreen.replace("starts[1] + 11, 23), 2", "starts[1] + 12, 29), 2")
fullscreen = fullscreen.replace("starts[2] + 17, 23), 2", "starts[2] + 21, 29), 2")
fullscreen = fullscreen.replace("? 39 : 33), 23), 2", "? 43 : 37), 29), 2")
fullscreen = fullscreen.replace("starts[4] + 17, 23), 2", "starts[4] + 23, 29), 2")
fullscreen = fullscreen.replace(
    "int labelScale = int(right - left) >= int(groupTextLength(group)) * 12 + 8 ? 2 : 1;",
    "int labelScale = int(right - left) >= int(groupTextLength(group)) * 18 + 8 ? 3 :\n                             (int(right - left) >= int(groupTextLength(group)) * 12 + 8 ? 2 : 1);",
)
fullscreen = fullscreen.replace(
    "int labelScale = int(right - left) >= int(materialTextLength(materialId)) * 12 + 8 ? 2 : 1;",
    "int labelScale = int(right - left) >= int(materialTextLength(materialId)) * 18 + 8 ? 3 :\n                             (int(right - left) >= int(materialTextLength(materialId)) * 12 + 8 ? 2 : 1);",
)
write("shaders/fullscreen.frag", fullscreen)

# -----------------------------------------------------------------------------
# Contracts and docs now describe the actual pass, not the earlier regression.
# -----------------------------------------------------------------------------
tests = read("tests/material_contract.cpp")
tests = tests.replace(
    "static_assert(epoch::sand::material_group_size(MaterialGroup::colony) == 9u);",
    "static_assert(epoch::sand::material_group_size(MaterialGroup::colony) == 9u);\n"
    "static_assert(epoch::sand::material_group_size(MaterialGroup::engineering) == 4u);",
)
tests = tests.replace(
    "static_assert(epoch::sand::scene_has_character(Scene::frontier_base));",
    "static_assert(epoch::sand::scene_has_character(Scene::frontier_base));\n"
    "static_assert(epoch::sand::scene_name(Scene::gold_mine) == \"Platformer\");",
)
replace_old = r'''constexpr bool every_material_is_grouped_once() {
    std::array<std::uint32_t, epoch::sand::material_count> counts{};
    for (const auto& group : epoch::sand::material_groups) {
        for (const auto material : group) {
            if (material != Material::count) ++counts[static_cast<std::uint32_t>(material)];
        }
    }
    for (const auto count : counts) if (count != 1u) return false;
    return true;
}
static_assert(every_material_is_grouped_once());'''
replace_new = r'''constexpr bool palette_materials_are_unique() {
    std::array<std::uint32_t, epoch::sand::material_count> counts{};
    for (const auto& group : epoch::sand::material_groups) {
        for (const auto material : group) {
            if (material == Material::count) continue;
            auto& count = counts[static_cast<std::uint32_t>(material)];
            if (++count != 1u) return false;
        }
    }
    return counts[static_cast<std::uint32_t>(Material::gold_ore)] == 0u &&
           counts[static_cast<std::uint32_t>(Material::iron_ore)] == 0u;
}
static_assert(palette_materials_are_unique());'''
if replace_old not in tests:
    raise RuntimeError("material palette test block not found")
tests = tests.replace(replace_old, replace_new)
write("tests/material_contract.cpp", tests)

validator = read("tools/validate_shader_contracts.py")
validator = validator.replace(
    """    if len(group_values) != material_count:
        errors.append(f"generated group map has {len(group_values)} active entries, expected {material_count}")
    if sorted(group_values) != list(range(material_count)):
        errors.append("generated group map is not a permutation of every material ID")
""",
    """    if any(value >= material_count for value in group_values):
        errors.append("generated group map contains an invalid material ID")
    if len(group_values) != len(set(group_values)):
        errors.append("generated group map contains duplicate material IDs")
    for hidden in (cpp_ids.get("gold_ore"), cpp_ids.get("iron_ore")):
        if hidden in group_values:
            errors.append("legacy ore/concentrate IDs must not appear in the palette")
""",
)
validator = validator.replace(
    """    for token in (
        "toxicPocket && state.oxygen == 0u",
        "state.exposureTicks >= 600u",
        "ambientAir",
    ):
        if token not in actor_comp:
            errors.append(f"bounded health contract missing {token!r}")
""",
    """    for token in ("ambientAir", "Atmosphere affects the oxygen meter only"):
        if token not in actor_comp:
            errors.append(f"nonlethal atmosphere contract missing {token!r}")
    if "state.health -=" in actor_comp:
        errors.append("passive atmosphere still drains player health")
""",
)
validator = validator.replace(
    "for token in (\"status_height = 60u\", \"group_tabs_height = 40u\", \"palette_items_height = 64u\"):",
    "for token in (\"status_height = 72u\", \"group_tabs_height = 48u\", \"palette_items_height = 76u\"):",
)
validator = validator.replace(
    "for token in (\"int[5](68, 68, 92, 124, 92)\", \"ivec2(10, 11), 3, 0u\", \"hudTop + 78u\"):",
    "for token in (\"int[5](78, 78, 104, 136, 104)\", \"ivec2(12, 12), 4, 0u\", \"hudTop + 78u\"):",
)
validator = validator.replace(
    """    for token in ("state.exposureTicks >= 600u", "actorPc.step % 120u", "toxicPocket && state.oxygen == 0u"):
        if token not in actor:
            errors.append(f"actor bounded suffocation contract missing {token!r}")
""",
    """    if "state.health -=" in actor:
        errors.append("actor health is still reduced by passive atmosphere classification")
""",
)
validator = validator.replace(
    """    for token in ("authoredStructuralCell", "looseAuthoredCargo"):
        if token not in reset:
            errors.append(f"authored scene stability contract missing {token!r}")
""",
    """    for token in ("authoredStructuralCell", "looseAuthoredCargo", "Large upper reservoir", "real sediment sifter"):
        if token not in reset:
            errors.append(f"authored scene contract missing {token!r}")
    if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset or "MAT_GOLD_ORE" in actor or "MAT_IRON_ORE" in actor:
        errors.append("ore blocks remain in authored scenes or player mining")
""",
)
write("tools/validate_shader_contracts.py", validator)

readme = read("README.md")
readme = readme.replace("7. Gold Mine", "7. Platformer")
readme = readme.replace(
    "- `M`: Mine/Build mode",
    "- `M`: Mine/Build mode in sandbox scenes; character scenes always keep player tools active",
)
marker = "## Default scenes\n"
resource_text = """## Resource pixels and sifting

EpochSand has no authored ore blocks. Gold, iron, copper, and generic metal exist as independent material pixels mixed sparsely through sand or silt. Gravity, filters, conveyors, and magnets separate those pixels without changing them into nuclear fuel or radiation particles. Structural steel, iron, or copper is never collected merely because the player walks nearby.

"""
if resource_text not in readme:
    readme = readme.replace(marker, resource_text + marker)
readme = readme.replace(
    "6. Engineering Lab\n7. Platformer",
    "6. Engineering Lab — contained thermal vessel, sediment/metal sifter, and sealed gas cell\n7. Platformer — multi-level playable structure based on the supplied reference",
)
write("README.md", readme)

# Final source assertions before the real shader compiler runs.
assert "Platformer" in read("include/epoch/sand/scene.hpp")
assert "MAT_GOLD_ORE" not in read("shaders/reset.comp")
assert "MAT_IRON_ORE" not in read("shaders/reset.comp")
assert "state.health -=" not in read("shaders/actor.comp")
assert "GetAsyncKeyState" in read("src/window_win32.cpp")
assert "character_scene || mining" in read("src/app.cpp")
assert "Large upper reservoir" in read("shaders/reset.comp")
assert "real sediment sifter" in read("shaders/reset.comp")
assert "0.015" in read("shaders/materials.glsl")
assert "MAT_HYDROGEN: color = vec4(1.00" in read("shaders/materials.glsl")

print("Applied screenshot/gameplay pass.")
