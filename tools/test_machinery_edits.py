"""Executable CPU admission/ownership contracts, not shader runtime acceptance.

The small material and conveyor predicates are read from the actual production
shader, evaluated over their finite domains, and coupled to an independent
immutable-snapshot rank/capacity oracle. GPU payload/behavior proof lives in
acceptance_machinery_edits.inl and must be run separately.
"""

import ast
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
CHEMISTRY = (ROOT / "shaders/chemistry.comp").read_text(encoding="utf-8")
MOVEMENT = (ROOT / "shaders/move.comp").read_text(encoding="utf-8")
IDS = {name: int(value) for name, value in re.findall(
    r"const uint (MAT_\w+) = (\d+)u;",
    (ROOT / "shaders/material_ids.glsl").read_text(encoding="utf-8"))}


@lru_cache(maxsize=32)
def body(source, name):
    start = re.search(r"\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", source)
    if start is None:
        raise AssertionError(f"Missing production function {name}")
    depth = 1
    index = start.end()
    while depth and index < len(source):
        depth += (source[index] == "{") - (source[index] == "}")
        index += 1
    if depth:
        raise AssertionError(f"Unbalanced production function {name}")
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", source[start.end():index - 1], flags=re.S)


def evaluate(expression, values):
    """Only Boolean/comparison/name/literal expressions, never arbitrary code."""
    expression = expression.replace("&&", " and ").replace("||", " or ").strip()
    expression = re.sub(r"!(?!=)", " not ", expression)
    expression = re.sub(r"(?<=\d)u\b", "", expression)
    expression = " ".join(expression.split())
    parsed = ast.parse(expression, mode="eval")
    allowed = (ast.Expression, ast.BoolOp, ast.And, ast.Or, ast.Compare,
               ast.Eq, ast.NotEq, ast.Gt, ast.Lt, ast.GtE, ast.LtE,
               ast.Name, ast.Load, ast.Constant, ast.BinOp, ast.BitAnd, ast.UnaryOp, ast.Not)
    if any(not isinstance(node, allowed) for node in ast.walk(parsed)):
        raise AssertionError(f"Unsupported production predicate: {expression}")
    return bool(eval(compile(parsed, "<production predicate>", "eval"),
                     {"__builtins__": {}}, values))


def material_predicate(source, name, material):
    expression = re.fullmatch(r"\s*return\s+(.+?);\s*", body(source, name), re.S)
    if expression is None:
        raise AssertionError(f"Expected a pure material predicate: {name}")
    return evaluate(expression[1], dict(IDS, material=material))


def stored_gas(material):
    return material_predicate(MOVEMENT, "isStoredGasMaterial", material)


def open_gas(material):
    expression = re.fullmatch(r"\s*return\s+(.+?);\s*", body(MOVEMENT, "isOpenGas"), re.S)
    if expression is None:
        raise AssertionError("Expected a pure open-gas predicate")
    predicate = expression[1].replace("isStoredGasMaterial(cell.material)", "stored_gas")
    predicate = predicate.replace("cell.material", "material")
    return evaluate(predicate, dict(IDS, material=material, stored_gas=stored_gas(material)))


def conveyor_accepts(material, cargo, belt, reverse=False):
    horizontal = body(MOVEMENT, "processHorizontal")
    first, second, direction = ("b", "a", "rightBelt") if reverse else ("a", "b", "leftBelt")
    guard = re.search(r"if \((isConveyorCargo\(" + first +
                      r"\).*?)\)\s*\{\s*swapCells\(left, right\);", horizontal, re.S)
    if guard is None:
        raise AssertionError("Missing conveyor transaction guard")
    expression = guard[1].replace(f"isConveyorCargo({first})", "cargo")
    expression = expression.replace(f"isOpenGas({second})", "open_gas")
    expression = expression.replace(f"{second}.material", "material")
    return evaluate(expression, dict(IDS, cargo=cargo, material=material,
                    open_gas=open_gas(material),
                    **{direction: belt}))


def habitat_slots():
    selector = body(CHEMISTRY, "machineResourceSlot")
    block = re.search(r"if \(machine == MAT_INSECT_HABITAT\)\s*\{([^}]+)\}", selector)
    if block is None:
        raise AssertionError("Missing Habitat selector")
    return {IDS[name]: int(slot) for name, slot in re.findall(
        r"if \(resourceMaterial == (MAT_\w+)\) return (\d+);", block[1])}


@dataclass(frozen=True)
class Donor:
    x: int
    y: int
    material: int
    age: int = 11
    temperature: int = 20
    aux: int = 255


@dataclass(frozen=True)
class Controller:
    x: int
    y: int
    inventory: tuple


@dataclass(frozen=True)
class Dispatch:
    width: int = 2048
    height: int = 1440
    origin_x: int = 0
    origin_y: int = 0
    enabled: bool = False
    radius: int = 0
    rows: int = 0


def endpoint_active(x, y, dispatch):
    if dispatch is None:
        return True
    if not (0 <= x < dispatch.width and 0 <= y < dispatch.height):
        return False
    if dispatch.enabled and not (
        dispatch.origin_x <= x // 640 < dispatch.origin_x + 4 and
        dispatch.origin_y <= y // 360 < dispatch.origin_y + 4):
        return False
    local_x = x - (max(dispatch.origin_x, 0) * 640 if dispatch.enabled else 0)
    local_y = y - (max(dispatch.origin_y, 0) * 360 if dispatch.enabled else 0)
    if local_x < 0 or local_y < 0:
        return False
    # Evaluate the actual final extent predicate, after the explicitly checked
    # world/section/origin gates, without interpreting arbitrary shader code.
    guard = body(CHEMISTRY, "machineEndpointActive")
    expression = re.search(r"return (pc.radius == 0u.+?);", guard, re.S)
    if expression is None:
        raise AssertionError("Missing exact machine endpoint extent predicate")
    expression = expression[1].replace("pc.radius", "radius").replace("pc.material", "rows")
    expression = expression.replace("uint(local.x)", "x").replace("uint(local.y)", "y")
    return evaluate(expression, dict(radius=dispatch.radius, rows=dispatch.rows,
                                    x=local_x, y=local_y))


def pair_awake(donor_chunk, controller_chunk, structural, donor_tile, controller_tile):
    guard = body(CHEMISTRY, "machinePairAwake")
    expression = re.search(r"bool chunkAwake = (.+?);", guard, re.S)
    if expression is None:
        raise AssertionError("Missing constant-time chunk wake witness")
    values = dict(donor_sleep=bool(donor_chunk & 2), donor_dirty=bool(donor_chunk & 4),
                  controller_sleep=bool(controller_chunk & 2), controller_dirty=bool(controller_chunk & 4))
    expression = expression[1]
    for original, name in (("donorChunk, CHUNK_SLEEPING", "donor_sleep"),
                           ("donorChunk, CHUNK_DIRTY", "donor_dirty"),
                           ("controllerChunk, CHUNK_SLEEPING", "controller_sleep"),
                           ("controllerChunk, CHUNK_DIRTY", "controller_dirty")):
        expression = expression.replace(f"chunkHas({original})", name)
    if not evaluate(expression, values):
        return False
    if not structural or not donor_tile & 4:
        return True
    expression = re.search(r"return (tileHas\(controllerTile.+?);", guard, re.S)[1]
    expression = expression.replace("tileHas(controllerTile, TILE_ACTIVE)", "active")
    expression = expression.replace("tileHas(controllerTile, TILE_COLLAPSING)", "collapsing")
    return evaluate(expression, dict(active=bool(controller_tile & 8), collapsing=bool(controller_tile & 64)))


def snapshot_transaction(donors, controllers, dispatch=None):
    """Independent oracle: nearest squared-distance, scan-order tie, six reach.

    Rank uses every eligible donor in the *original* snapshot. A full nearest
    owner does not silently redirect its refused donor into a second owner.
    """
    slots = habitat_slots()
    owners = {}
    for index, donor in enumerate(donors):
        if donor.material not in slots or not endpoint_active(donor.x, donor.y, dispatch):
            continue
        candidates = [(owner, controller) for owner, controller in enumerate(controllers)
                      if controller.x % 8 == 3 and controller.y % 8 == 3
                      and endpoint_active(controller.x, controller.y, dispatch)
                      and abs(controller.x - donor.x) <= 6
                      and abs(controller.y - donor.y) <= 6]
        if candidates:
            owners[index] = min(candidates, key=lambda pair: (
                (pair[1].x - donor.x) ** 2 + (pair[1].y - donor.y) ** 2,
                pair[1].y, pair[1].x))[0]
    after = [list(controller.inventory) for controller in controllers]
    accepted = set()
    for index, owner in owners.items():
        donor = donors[index]
        slot = slots[donor.material]
        rank = sum(1 for other, other_owner in owners.items()
                   if other_owner == owner and donors[other].material == donor.material
                   and (donors[other].y, donors[other].x) < (donor.y, donor.x))
        if rank < 15 - controllers[owner].inventory[slot]:
            if not material_predicate(CHEMISTRY, "isFactoryInputResource", donor.material):
                raise AssertionError("Recipient credits a donor excluded from source debit")
            accepted.add(index)
            after[owner][slot] += 1
    return accepted, after


class MachineryEditsContract(unittest.TestCase):
    def test_every_credited_resource_has_a_debit_path(self):
        credited = set(re.findall(r"resourceMaterial == (MAT_\w+)",
                                  body(CHEMISTRY, "machineResourceSlot")))
        self.assertTrue(credited)
        for name in credited:
            with self.subTest(material=name):
                self.assertTrue(material_predicate(CHEMISTRY, "isFactoryInputResource", IDS[name]))
        self.assertEqual(habitat_slots(), {
            IDS["MAT_FOOD"]: 0, IDS["MAT_WASTE"]: 1, IDS["MAT_FERTILIZER"]: 2})

    def test_legacy_habitat_omission_is_detected(self):
        old = CHEMISTRY.replace(" ||\n           material == MAT_FOOD || material == MAT_WASTE || material == MAT_FERTILIZER", "")
        self.assertNotEqual(old, CHEMISTRY)
        self.assertFalse(material_predicate(old, "isFactoryInputResource", IDS["MAT_FOOD"]))

    def test_real_conveyor_guards_all_materials_directions_and_belts(self):
        for material in IDS.values():
            for cargo in (False, True):
                for belt in (-1, 0, 1):
                    for reverse in (False, True):
                        expected = cargo and (belt < 0 if reverse else belt > 0) and (
                            material == IDS["MAT_EMPTY"] or stored_gas(material))
                        self.assertEqual(conveyor_accepts(material, cargo, belt, reverse), expected)
        self.assertTrue(conveyor_accepts(IDS["MAT_ATMOSPHERE"], True, 1))
        self.assertFalse(conveyor_accepts(IDS["MAT_WATER"], True, 1))
        self.assertFalse(conveyor_accepts(IDS["MAT_STONE"], True, 1))

    def test_capacity_ranks_and_unit_ledger(self):
        for material, slot in habitat_slots().items():
            for capacity_used in range(16):
                for count in range(18):
                    inventory = [0, 0, 0, 0]
                    inventory[slot] = capacity_used
                    donors = [Donor(29 + n % 7, 29 + n // 7, material) for n in range(count)]
                    accepted, after = snapshot_transaction(donors, [Controller(35, 35, tuple(inventory))])
                    self.assertEqual(len(accepted), min(count, 15 - capacity_used))
                    self.assertEqual(sum(inventory) + count, sum(after[0]) + count - len(accepted))
                    self.assertTrue(all(0 <= value <= 15 for value in after[0]))
                    self.assertEqual(accepted, set(range(len(accepted))))

    def test_two_controllers_do_not_double_credit_or_redirect_full_nearest(self):
        donors = [Donor(39, 34, IDS["MAT_FOOD"]), Donor(39, 35, IDS["MAT_FOOD"])]
        controllers = [Controller(35, 35, (14, 0, 0, 0)), Controller(43, 35, (0, 0, 0, 0))]
        accepted, after = snapshot_transaction(donors, controllers)
        self.assertEqual(accepted, {0})
        self.assertEqual(after, [[15, 0, 0, 0], [0, 0, 0, 0]])
        self.assertEqual(snapshot_transaction(donors, list(reversed(controllers)))[1],
                         [[0, 0, 0, 0], [15, 0, 0, 0]])

    def test_boundary_wrong_material_and_wrong_controller_residue(self):
        donors = [Donor(42, 35, IDS["MAT_FOOD"]), Donor(35, 42, IDS["MAT_WASTE"]),
                  Donor(28, 35, IDS["MAT_FERTILIZER"]), Donor(34, 35, IDS["MAT_COPPER"])]
        self.assertEqual(snapshot_transaction(donors, [Controller(35, 35, (0, 0, 0, 0))])[0], set())
        self.assertEqual(snapshot_transaction([Donor(35, 34, IDS["MAT_FOOD"])],
                         [Controller(36, 35, (0, 0, 0, 0))])[0], set())

    def test_accepted_donor_is_final_before_competing_waste_rule(self):
        main = body(CHEMISTRY, "main")
        debit = main.index("if (isFactoryInputResource(source.material))")
        controller = main.index("if (isMachineController(p, source.material))", debit)
        selected = main[debit:controller]
        # Execution reaches a return in the accepted-owner branch: a later
        # Waste->Fertilizer choice cannot restore the already inventoried unit.
        self.assertRegex(selected, r"result = makeCell\(MAT_EMPTY\);.*nextCells\[index\] = result;\s*return;".replace(".*", "[\\s\\S]*"))
        self.assertIn("machineAcceptsResource(p, acceptingMachine, source)", selected)
        self.assertIn("atomicAdd(conservation[CONS_CONVERTED], 1u)", selected)
        self.assertLess(controller, main.index("source.material == MAT_WASTE && (hasNeighbor"))

    def test_endpoint_geometry_matches_actual_invocation_admission(self):
        guard = body(CHEMISTRY, "machineEndpointActive")
        self.assertIn("!inside(position)", guard)
        self.assertIn("!sectionActiveAt(position, pc.activeSectionX, pc.activeSectionY, pc.activeMode)", guard)
        self.assertIn("activeDispatchCellOrigin(", guard)
        self.assertIn("any(lessThan(local, ivec2(0)))", guard)
        self.assertNotRegex(guard, r"\b(for|while)\s*\(")
        main = body(CHEMISTRY, "main")
        self.assertIn("gl_GlobalInvocationID.x >= pc.radius", main)
        self.assertIn("gl_GlobalInvocationID.y >= pc.material", main)
        self.assertIn("activeDispatchCellOrigin(", main)
        for dispatch in (
            Dispatch(width=201, height=197, radius=192, rows=191),
            Dispatch(width=201, height=197),
            Dispatch(width=3840, height=1800, origin_x=1, origin_y=1, enabled=True),
            Dispatch(width=3840, height=1800, origin_x=1, origin_y=1,
                     enabled=True, radius=192, rows=193),
            Dispatch(width=200, height=190, origin_x=-1, origin_y=-1, enabled=True),
            Dispatch(width=200, height=190, radius=192, rows=0),
        ):
            ox = max(dispatch.origin_x, 0) * 640 if dispatch.enabled else 0
            oy = max(dispatch.origin_y, 0) * 360 if dispatch.enabled else 0
            xs = {-1, 0, 1, 190, 191, 192, 193, dispatch.width - 1, dispatch.width}
            ys = {-1, 0, 1, 190, 191, 192, 193, dispatch.height - 1, dispatch.height}
            xs.update(ox + delta for delta in (-1, 0, 1, 190, 191, 192, 193, 2559, 2560))
            ys.update(oy + delta for delta in (-1, 0, 1, 190, 191, 192, 193, 1439, 1440))
            for x in xs:
                for y in ys:
                    # Model main from invocation IDs outward, independently
                    # from the machine guard's endpoint-to-local derivation.
                    gid_x, gid_y = x - ox, y - oy
                    invoked = gid_x >= 0 and gid_y >= 0
                    if dispatch.radius:
                        invoked &= gid_x < dispatch.radius and gid_y < dispatch.rows
                    invoked &= 0 <= x < dispatch.width and 0 <= y < dispatch.height
                    if dispatch.enabled:
                        invoked &= 0 <= max(x, 0) // 640 - dispatch.origin_x < 4
                        invoked &= 0 <= max(y, 0) // 360 - dispatch.origin_y < 4
                    self.assertEqual(endpoint_active(x, y, dispatch), invoked,
                                     (dispatch, x, y))

    def test_both_endpoints_and_rank_exclude_unprocessed_neighbors(self):
        for name, snippets in {
            "nearestAcceptingMachine": ("!machineEndpointActive(resourcePosition)",
                                         "!machineEndpointActive(candidatePosition)"),
            "machineInputRank": ("!machineEndpointActive(candidatePosition)",),
            "machineAcceptsResource": ("!machineEndpointActive(resourcePosition)",
                                       "!machineEndpointActive(controller)"),
            "incomingMachineCounts": ("!machineEndpointActive(controller)",
                                       "!machineEndpointActive(resourcePosition)"),
        }.items():
            for snippet in snippets:
                self.assertIn(snippet, body(CHEMISTRY, name))
        clip = Dispatch(radius=192, rows=192)
        outside_donor = [Donor(193, 35, IDS["MAT_FOOD"])]
        self.assertEqual(snapshot_transaction(outside_donor, [Controller(187, 35, (0, 0, 0, 0))], clip)[0], set())
        inside_donor = [Donor(190, 67, IDS["MAT_FOOD"])]
        self.assertEqual(snapshot_transaction(inside_donor, [Controller(195, 67, (0, 0, 0, 0))], clip)[0], set())
        donors = [Donor(192, 97, IDS["MAT_FOOD"]), Donor(186, 98, IDS["MAT_FOOD"])]
        accepted, after = snapshot_transaction(donors, [Controller(187, 99, (14, 0, 0, 0))], clip)
        self.assertEqual(accepted, {1})
        self.assertEqual(after, [[15, 0, 0, 0]])
        translated = Dispatch(origin_x=1, origin_y=1, enabled=True, radius=192, rows=192)
        self.assertEqual(snapshot_transaction([Donor(638, 451, IDS["MAT_FOOD"])],
                         [Controller(643, 451, (0, 0, 0, 0))], translated)[0], set())
        self.assertEqual(snapshot_transaction([Donor(641, 483, IDS["MAT_FOOD"])],
                         [Controller(635, 483, (0, 0, 0, 0))], translated)[0], set())
        self.assertEqual(snapshot_transaction([Donor(640, 515, IDS["MAT_FOOD"])],
                         [Controller(643, 515, (0, 0, 0, 0))], translated)[0], {0})

    def test_controller_lattice_pruning_preserves_translation_and_ties(self):
        selector = body(CHEMISTRY, "nearestAcceptingMachine")
        guard = re.search(r"if \((\(candidatePosition.x & 7\).*?)\) continue;", selector)
        self.assertIsNotNone(guard)
        self.assertLess(selector.index("!machineEndpointActive(candidatePosition)"), guard.start())
        self.assertLess(guard.end(), selector.index("Cell candidate = at(candidatePosition)"))
        self.assertIn("isMachineController(candidatePosition, candidate.material)", selector)
        self.assertIn("if (distanceSquared < best)", selector)
        expression = guard[1].replace("candidatePosition.x", "x").replace("candidatePosition.y", "y")
        for origin_x, origin_y in ((0, 0), (640, 360), (-640, -360), (639, 359)):
            for dx in range(-9, 10):
                for dy in range(-9, 10):
                    x, y = origin_x + dx, origin_y + dy
                    rejected = evaluate(expression, dict(x=x, y=y))
                    for active in (False, True):
                        for machine in (False, True):
                            old = active and machine and (x & 7) == 3 and (y & 7) == 3
                            new = active and not rejected and machine and (x & 7) == 3 and (y & 7) == 3
                            self.assertEqual(new, old, (x, y, active, machine))

    def test_constant_time_sleep_witness_cannot_admit_a_skipped_donor(self):
        guard = body(CHEMISTRY, "machinePairAwake")
        self.assertNotRegex(guard, r"\b(for|while)\s*\(")
        self.assertIn("abs(resourcePosition - controller), ivec2(6)", guard)
        self.assertIn("if (!chunkAwake) return false", guard)
        self.assertIn("if (!isStructural(resourceCell)) return true", guard)
        self.assertIn("if (!tileHas(donorTile, TILE_SLEEPING)) return true", guard)
        self.assertIn("machinePairAwake(resourcePosition, candidatePosition, resourceCell)",
                      body(CHEMISTRY, "nearestAcceptingMachine"))
        self.assertIn("!machinePairAwake(resourcePosition, controller, resourceCell)",
                      body(CHEMISTRY, "machineAcceptsResource"))
        # Six-cell reach places the controller inside each donor sleep loop's
        # three-by-three neighborhood, even across tile/chunk boundaries.
        for origin in range(64, 128):
            for delta in range(-6, 7):
                self.assertLessEqual(abs(origin // 8 - (origin + delta) // 8), 1)
                self.assertLessEqual(abs(origin // 64 - (origin + delta) // 64), 1)
        for donor_chunk in range(8):
            for controller_chunk in range(8):
                for structural in (False, True):
                    for donor_tile in (0, 4, 8, 64, 12):
                        for controller_tile in (0, 4, 8, 64, 12):
                            allowed = pair_awake(donor_chunk, controller_chunk,
                                                 structural, donor_tile, controller_tile)
                            for third_chunk_awake in (False, True):
                                for third_tile_awake in (False, True):
                                    chunk_skip = (bool(donor_chunk & 2) and not donor_chunk & 4 and
                                        bool(controller_chunk & 2) and not controller_chunk & 4 and
                                        not third_chunk_awake)
                                    tile_skip = (structural and bool(donor_tile & 4) and
                                        not donor_tile & (8 | 64) and not controller_tile & (8 | 64) and
                                        not third_tile_awake)
                                    if allowed:
                                        self.assertFalse(chunk_skip or tile_skip)
        self.assertTrue(pair_awake(2, 1, True, 4, 8))
        self.assertTrue(pair_awake(2, 6, True, 4, 64))
        self.assertFalse(pair_awake(2, 2, False, 0, 8))
        self.assertFalse(pair_awake(1, 1, True, 4, 4))
        # Deliberate conservative delay: a third neighbor can wake actual
        # chemistry, but is not inspected by this constant-time pair witness.
        self.assertFalse(pair_awake(2, 2, False, 0, 8))


if __name__ == "__main__":
    unittest.main()
