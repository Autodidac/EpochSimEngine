#!/usr/bin/env python3
"""Production chemistry partition wiring and real-preprocessor contracts.

Run with --glslc PATH. Only preprocessing (-E) is requested; no SPIR-V, GPU,
package, or persistent fixture is created. These tests complement the compiled
scalar ownership oracle, not full shader numerical or synchronization proof.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
from pathlib import Path
import re
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
GLSLC: Path | None = None
OWNER = "EPOCHSIM_CHEMISTRY_OWNER"
OUTPUTS = ("chemistry.comp", "chemistry_bees.comp", "chemistry_machinery.comp",
           "chemistry_destinations.comp", "chemistry_phases.comp")
REFERENCE = 5

# Independent source families, never inferred from chemistrySourceOwner. The
# C++ oracle additionally checks every exact saved ID and unknown uint value.
OWNED_MATERIALS = (
    "DIRT STONE CRYSTAL MUD ACID GRASS OIL WOOD PLASTIC ACID_RESISTANT_PLASTIC "
    "ASH GLASS GUNPOWDER SEED FLOWER MAGNET INSULATOR LIGHTNING URANIUM RADIATION "
    "CONVEYOR ANT BEETLE PLANT_STEM FACTORY_CORE",
    "HONEY BEE BEESWAX BEEHIVE POLLEN QUEEN_BEE",
    "SAND ALUMINUM IRON COPPER ALUMINUM_SHAVINGS GOLD IRON_ORE STEEL SMELTER "
    "ASSEMBLER INSECT_HABITAT POWER_CELL PLASMA_AMMO SILT FERTILIZER FOOD WASTE SLUICE_BOX",
    "EMPTY ATMOSPHERE",
    "WATER SMOKE STEAM FIRE LAVA SALT ICE EMBER SNOW SALTWATER DIRTY_STEAM "
    "DIRTY_WATER MAGMA_VENT OXYGEN CARBON_DIOXIDE HYDROGEN CLOUD",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def uncomment(source: str) -> str:
    # Preserve quoted paths/strings while removing comments, including braces
    # in comments. Preprocessor line markers are not shader statements.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/'
    source = re.sub(pattern, lambda m: " " if m[0].startswith(("//", "/*")) else m[0],
                    source, flags=re.S)
    return re.sub(r"^\s*#\s*line\b[^\n]*", "", source, flags=re.M)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", uncomment(source))


def function_span(source: str, name: str) -> tuple[int, int]:
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{", source)
    require(match is not None, f"missing production function {name}")
    depth = 1
    # Tokenize comments and strings as indivisible objects so their braces do
    # not terminate main(). This works before and after glslc preprocessing.
    tokens = re.finditer(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/|[{}]',
                         source[match.end():], flags=re.S)
    for token in tokens:
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return match.end(), match.end() + token.start()
    raise AssertionError(f"unbalanced production function {name}")


def body(source: str, name: str = "main") -> str:
    begin, end = function_span(source, name)
    return uncomment(source[begin:end])


def edit_main(source: str, old: str, new: str) -> str:
    begin, end = function_span(source, "main")
    main = source[begin:end]
    require(main.count(old) == 1, f"mutation fixture needs one main() occurrence of {old}")
    return source[:begin] + main.replace(old, new, 1) + source[end:]


def preprocess(source: str, owner: int | None) -> str:
    require(GLSLC is not None and GLSLC.is_file(), "explicit glslc executable is required")
    command = [str(GLSLC), "-E", "-fshader-stage=compute", "--target-env=vulkan1.2",
               "-I", str(ROOT / "shaders")]
    if owner is not None:
        command.append(f"-D{OWNER}={owner}")
    command.append("-")
    result = subprocess.run(command, input=source, text=True, encoding="utf-8",
                            capture_output=True, check=False, timeout=60, cwd=ROOT)
    require(result.returncode == 0,
            f"glslc -E owner {owner} failed ({result.returncode}): {result.stderr}")
    require(bool(result.stdout.strip()), f"glslc -E owner {owner} returned no source")
    return result.stdout


def guard_pattern(owner: str) -> str:
    return (r"if\s*\(\s*chemistrySourceOwner\s*\(\s*source\s*\.\s*material\s*\)"
            r"\s*!=\s*uint\s*\(\s*" + owner + r"\s*\)\s*\)\s*"
            r"(?:return\s*;|\{\s*return\s*;\s*\})")


def validate_guard(preprocessed: str, owner: int) -> None:
    main = body(preprocessed)
    source = re.search(r"\bCell\s+source\s*=\s*cells\s*\[\s*index\s*\]\s*;", main)
    require(source is not None, "main must load the immutable canonical source")
    guards = list(re.finditer(guard_pattern(str(owner) + r"u?"), main))
    if owner == REFERENCE:
        require(not re.search(r"chemistrySourceOwner\s*\(\s*source\s*\.\s*material", main),
                "unsplit reference must not filter source ownership")
        return
    require(len(guards) == 1, f"owner {owner}: missing or duplicated immutable-source guard")
    guard = guards[0]
    section = main.find("sectionActiveAt")
    require(source.end() <= guard.start() < section,
            f"owner {owner}: source guard must precede inactive-section scratch writes")
    prefix = main[:guard.start()]
    require(not re.search(r"\b(?:nextCells|commitResult|atomic\w*|recordConservation)\b", prefix),
            f"owner {owner}: scratch write or counter can precede source admission")
    require(not re.search(r"\bsource(?:\s*\.\s*\w+)?\s*(?:[+\-*/|&^]?=|\+\+|--)(?!=)",
                          main[source.end():]), "main must not mutate its source owner")
    for access in re.finditer(r"\bnextCells\s*\[([^]]+)\]", main):
        require(compact(access[1]) == "index" and
                re.match(r"\s*=(?!=)", main[access.end():]) is not None,
                "partition scratch must be write-only at the source's own index")


# Each sentinel must occur in main, not merely in an included helper. Checking
# both presence AND exclusion catches a nested !=1 guard erasing the Bee body.
FAMILIES = {
    "Bees": ({1, 5}, (
        "if ((source.material == (MAT_BEE)))",
        "else if ((source.material == (MAT_POLLEN)))",
        "else if ((source.material == (MAT_HONEY)))",
        "else if ((source.material == (MAT_BEESWAX)))",
        "else if ((source.material == (MAT_QUEEN_BEE)))",
        "else if ((source.material == (MAT_BEEHIVE)))",
        "bool authoredBee = (source.aux & BEE_AUX_SWARM) != 0u;",
        "bool queenCarrier = (source.aux & BEE_AUX_QUEEN) != 0u;",
        "bool migratingFollower = (source.aux & BEE_AUX_MIGRATING) != 0u;",
        "beeHomeHasQueen(result)", "nearestFlowerTileForColony(homeCenter)",
        "beeDepositTarget(p, source)", "hungryBeeTargetsHoney(p)",
        "uint stress = stateValue(source);", "!hasWithin(p, MAT_QUEEN_BEE, 7)",
    )),
    "machinery": ({2, 5}, (
        "if (isFactoryInputResource(source.material))",
        "nearestAcceptingMachine(p, source, 6)",
        "machineAcceptsResource(p, acceptingMachine, source)",
        "atomicAdd(conservation[CONS_CONVERTED], 1u)",
        "if (isMachineController(p, source.material))",
        "uvec4 currentInventory = machineInventory(source);",
        "incomingMachineCounts(p, source.material)",
        "setMachineInventory(result, inventory)",
    )),
    "destination/phase pairs": ({3, 4, 5}, (
        "Cell synthesisPartner;", "hydrogenOxygenPair(p, source, synthesisPartner)",
        "dissolvedOutgasPair(p, source)", "if (hasDissolvedWaterGas(source))",
        "dissolvedOxygenPair(p, source)",
    )),
    "destinations": ({3, 5}, (
        "machineOutputTransition(p, source, result)",
        "machineOutputVentTransition(p, source, result)",
    )),
    "phase carrier": ({4, 5}, ("if (isHalfWater(source))",)),
    "mixed nonbee prestructure": ({0, 2, 3, 4, 5}, (
        "ventOutletOwnsLava(source)", "dirtyWaterSeparationReady(p + ivec2(0, 1), belowSource)",
        "flowerDropsSeed(p)", "pollenBeeTargets(p)", "uint newbornSlot =",
        "respiringNeighborDemand(p)", "respirePackedMedium(result)",
        "int reheatedTemperature = result.temperature;",
    )),
    "compost": ({0, 2, 4, 5}, ("compostFeedReady(p, source)", "compostWaterReady(p, source)")),
    "phase/plant rules": ({0, 2, 4, 5}, (
        "saltDissolutionTarget(p, source)", "incomingSaltUnits(p)",
        "smokeSteamPair(p, source, primaryGas)", "uint connectedLava = lavaNeighborCount(p);",
        "bool outletOwnsLava = ventOutletOwnsLava(outlet);",
        "bool nucleatesCloud =", "uint connectedMass = 1u + neighborCount(p, MAT_CLOUD);",
        "seedHasGrowingConditions(p)", "bool validStem = support.material == MAT_PLANT_STEM;",
    )),
    "closed ecology": ({2, 4, 5}, (
        "harvestConsumesWater(p)", "dirtyWaterSeparationReady(p, source)",
        "fertilizerHarvestReady(p, source)",
    )),
    "residual bulk": ({0, 5}, (
        "result.temperature = max(result.temperature, 900);",
        "result = makeCell(nearSaltwater ? MAT_SALTWATER : MAT_WATER);",
    )),
}

COMMON = (
    "Cell result = source;", "result.age = source.age + 1u;", "result.aux &= ~AUX_MOVED;",
    "sleepingChunkNeighborhood(p)", "sleepingStructuralNeighborhood(p, source)",
    "int ambientTemperature = 20 + int(light[index] / 48u);",
    "int thermalTarget = (neighborTemperature(p) * 3 + ambientTemperature) / 4;",
    "materialThermalConductivity(source.material)",
    "result.temperature += clamp((thermalTarget - result.temperature) / thermalDivisor, -20, 20);",
    "result.temperature = clamp(result.temperature, -200, 5000);",
    "bool durableStructural = isStructural(source) && isBlockCapable(source.material);",
    "durableStructural = durableStructural || fixedHiveContent;",
    "cellPhase(result) == PHASE_MOLTEN", "tileHas(tile, TILE_COLLAPSING)",
    "applyStructuralHazards(p, source, result, nearWater, nearSaltwater, nearLava, nearFire, nearAcid);",
    "isRespiringLife(source.material)", "uint localOxygen = oxygenVolumeWithin(p, 2);",
    "bool fullyChoked = fullyChokedByMedium(p);", "if (isConductive(source.material))",
    "isFlammable(source.material)", "uint ignitionStrength = flammability(source.material);",
    "acidResistance(source.material)",
)


def validate_families(preprocessed: str, owner: int, reference: str) -> None:
    main, original = compact(body(preprocessed)), compact(body(reference))
    # Normalize only SOURCE_IS's documented redundant constant specialization
    # so exact lifecycle branch headers can be compared with reference owner5.
    # Never simplify arbitrary predicates or infer runtime numerical parity.
    main = re.sub(r"\(chemistrySourceOwner\((MAT_\w+)\)==uint\([0-4]u?\)&&"
                  r"source\.material==\(\1\)\)", r"(source.material==(\1))", main)
    for family, (owners, sentinels) in FAMILIES.items():
        for sentinel in sentinels:
            needle = compact(sentinel)
            count = original.count(needle)
            require(count > 0, f"reference main lost required {family} rule: {sentinel}")
            expected = count if owner in owners else 0
            require(main.count(needle) == expected,
                    f"owner {owner}: {family} main rule expected {expected} occurrences: {sentinel}")
    for sentinel in COMMON:
        require(compact(sentinel) in main, f"owner {owner}: common rule was excluded: {sentinel}")
    require(main.endswith("commitResult(index,source,result);"),
            f"owner {owner}: missing final canonical-source commit")
    sequence = ("Cellresult=source;", "intthermalTarget=", "applyStructuralHazards(",
                "uintlocalOxygen=", "if(isConductive(source.material))", "uintignitionStrength=")
    positions = [main.index(needle) for needle in sequence]
    require(positions == sorted(positions), f"owner {owner}: common rule order changed")
    # Every SOURCE_IS occurrence for this owner's material must survive, not
    # just one mention in a common prelude. This independently catches a family
    # guard that erases its own lifecycle while leaving helper declarations.
    material_groups = [set("MAT_" + name for name in group.split()) for group in OWNED_MATERIALS]
    require(tuple(map(len, material_groups)) == (25, 6, 18, 2, 17) and
            len(set.union(*material_groups)) == 68, "independent owner oracle changed")
    pattern = r"\(source\.material==\((MAT_\w+)\)\)"
    original_predicates = Counter(re.findall(pattern, original))
    actual_predicates = Counter(re.findall(pattern, main))
    require(set(original_predicates) <= set.union(*material_groups),
            "reference uses a source material missing from the independent oracle")
    for material in (set.union(*material_groups) if owner == REFERENCE else material_groups[owner]):
        require(actual_predicates[material] == original_predicates[material],
                f"owner {owner}: own-material rule was excluded or duplicated: {material}")


def wrap_bee_guard(source: str) -> str:
    start = re.search(r"^\s*#if\s+" + OWNER + r"\s*==\s*1\s*\|\|\s*" +
                      OWNER + r"\s*==\s*5[^\n]*\n", source, re.M)
    require(start is not None, "mutation fixture needs the production Bee conditional")
    depth = 1
    for directive in re.finditer(r"^\s*#\s*(if|ifdef|ifndef|endif)\b[^\n]*(?:\n|$)",
                                 source[start.end():], re.M):
        depth += -1 if directive[1] == "endif" else 1
        if depth == 0:
            end = start.end() + directive.end()
            return (source[:start.start()] + f"\n#if {OWNER} != 1\n" +
                    source[start.start():end] + "\n#endif\n" + source[end:])
    raise AssertionError("unbalanced production Bee conditional")


def validate_renderer(source: str) -> None:
    clean = uncomment(source)
    recorder = compact(body(clean, "record_chemistry"))
    require("conststd::arraypipelines{chemistry_pipeline,chemistry_bees_pipeline,"
            "chemistry_machinery_pipeline,chemistry_destinations_pipeline,chemistry_phases_pipeline};"
            in recorder, "recorder must own each pipeline once")
    require("owner<pipelines.size();++owner" in recorder, "recorder must dispatch all five owners")
    require(recorder.count("vkCmdDispatch(") == 1 and recorder.count("bind_compute(") == 1,
            "recorder requires one shared dispatch/bind site")
    for needle in (
        "bind_compute(command_buffer,pipelines[owner],set_index);",
        "vkCmdPushConstants(command_buffer,compute_pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,"
        "0,sizeof(push),&push);",
        "vkCmdDispatch(command_buffer,divide_round_up(width,simulation_local_size),"
        "divide_round_up(height,simulation_local_size),1);",
        "buffer_barrier(command_buffer,cell_buffers[set_index^1u],VK_ACCESS_SHADER_WRITE_BIT,"
        "VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,"
        "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);",
    ):
        require(needle in recorder, f"recorder changed immutable push/extent/dependency: {needle}")
    require(not re.search(r"copy_cell_rectangle|vkCmdCopy|swap\(|current_set=|set_index=", recorder),
            "canonical cells must not copy/swap between chemistry owners")
    require("conservation_corrections_pipeline" not in recorder,
            "correction belongs after all owners, not inside their loop")

    calls = list(re.finditer(r"\brecord_chemistry\s*(<\s*Profile\s*>)?\s*\(([^;{}]*?)\)\s*;", clean, re.S))
    expected = Counter((
        ("<Profile>", "command_buffer,current_set,simulation_push,active_dispatch.width,"
         "active_dispatch.height,profile_queries,stage_boundary"),
        ("", "command_buffer,current_set,push,acceptance_width,acceptance_height"),
        ("", "command_buffer,current_set,simulation_push,acceptance_width,acceptance_height"),
    ))
    require(Counter((compact(call[1] or ""), compact(call[2])) for call in calls) == expected,
            "production and both acceptance paths must use the same five-owner recorder")
    require(not re.search(r"bind_compute\s*\([^;]*,\s*chemistry(?:_bees|_machinery|_destinations|_phases)?"
                          r"_pipeline\s*,", clean), "direct chemistry binds may bypass the five-owner recorder")
    for call in calls:
        suffix = clean[call.end():]
        copy = re.search(r"\bcopy_cell_rectangle\s*\(([^;]+)\)\s*;", suffix)
        require(copy is not None, "chemistry path lost bounded scratch copyback")
        between = compact(suffix[:copy.start()])
        push = compact(call[2]).split(",")[2]
        require(between.count("bind_compute(command_buffer,conservation_corrections_pipeline,current_set);") == 1,
                "each complete chemistry stage needs exactly one following correction")
        require(f"0,sizeof({push}),&{push});" in between,
                "correction must receive the same unchanged chemistry push")
        require(between.count("record_rain_admission_snapshot(command_buffer);") == 1,
                "rain admission must be snapshotted once after all chemistry owners")
        require(between.count("vkCmdDispatch(") == 1 and "record_chemistry" not in between,
                "unexpected dispatch between complete chemistry and copyback")
        require(not re.search(r"vkCmdCopy|current_set=|swap\(", between),
                "canonical buffer changed before the one correction/copyback")
        require(compact(copy[1]).startswith("command_buffer,next_set,current_set,"),
                "bounded chemistry copyback must target the canonical source set")
    for pipeline, filename in zip(("chemistry_pipeline", "chemistry_bees_pipeline",
                                   "chemistry_machinery_pipeline", "chemistry_destinations_pipeline",
                                   "chemistry_phases_pipeline"), OUTPUTS):
        require(f'{pipeline}=create_compute_pipeline("{filename}.spv");' in compact(clean),
                f"missing deployed pipeline {filename}")
        require(f"vkDestroyPipeline(device,{pipeline},nullptr);" in compact(clean),
                f"missing owned-pipeline destruction: {pipeline}")


def validate_profile(source: str) -> None:
    clean = compact(source)
    for name in ("record_chemistry", "record_simulation_step"):
        require("template<boolProfile=false,typenameStageBoundary=std::nullptr_t>void" + name + "("
                in clean, f"{name}: profiling must default to a compile-time-disabled instantiation")
    recorder = compact(body(source, "record_chemistry"))
    profile_guard = (
        "ifconstexpr(Profile){constautoboundary=3u+static_cast<std::uint32_t>(owner);"
        "vkCmdWriteTimestamp(command_buffer,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,profile_queries,boundary);"
        "ifconstexpr(!std::is_same_v<StageBoundary,std::nullptr_t>)stage_boundary(boundary);}")
    require(recorder.count(profile_guard) == 1,
            "owner timestamps and callbacks need the compile-time profile guard")
    ordinary = recorder.replace(profile_guard, "")
    require(not re.search(r"vkCmdWriteTimestamp|profile_queries|stage_boundary", ordinary),
            "ordinary owner dispatch must emit no profiling work")
    require(recorder.index("vkCmdDispatch(") < recorder.index(profile_guard),
            "owner boundary must follow its complete dispatch")

    step = compact(body(source, "record_simulation_step"))
    step_guard = ("ifconstexpr(Profile){vkCmdWriteTimestamp(command_buffer,"
                  "VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,profile_queries,boundary);"
                  "ifconstexpr(!std::is_same_v<StageBoundary,std::nullptr_t>)stage_boundary(boundary);}")
    require(step.count(step_guard) == 1 and "vkCmdWriteTimestamp" not in step.replace(step_guard, ""),
            "fixed-tick markers must also remain compile-time profile-only")
    marks = [int(value) for value in re.findall(r"mark_profile\((\d+)u\);", step)]
    require(marks == [0, 1, 2, 8, 9, 10, 11, 12, 13, 14, 15],
            "fixed-tick marker numbers overlap owners 3..7 or omit the final boundary")
    call = step.index("record_chemistry<Profile>(")
    require(step.index("mark_profile(2u);") < call < step.index("mark_profile(8u);"),
            "five owner boundaries must sit between classification and correction completion")
    profile = compact(body(source, "run_simulation_profile"))
    require("constexprstd::uint32_tquery_count=16u;" in profile,
            "profiler must allocate all 16 boundary queries")
    names = re.search(r"stage_names\{([^}]+)\};", profile)
    require(names is not None and re.findall(r'"([^"\\]*)"', names[1]) == [
        "sunlight", "tile_and_chunk_classification", "chemistry_bulk", "chemistry_bees",
        "chemistry_machinery", "chemistry_destinations", "chemistry_phases",
        "conservation_corrections", "chemistry_copyback", "tracked_rainfall", "macro_movement",
        "structural_repair", "movement_snapshot", "fine_movement", "bee_birth_and_movement"],
        "profiler stage labels no longer match the 15 measured boundary intervals")
    require("boundary!=next_boundary||boundary>=query_count" in profile and
            "if(next_boundary!=query_count)" in profile,
            "serial diagnostics must reject missing, repeated or out-of-range end markers")


def validate_build(cmake: str, shader: str, package: str) -> None:
    listing = re.search(r"set\(SANDHYBRID_SHADER_SOURCES\s+([^)]*)\)", cmake)
    require(listing is not None, "missing maintained shader output list")
    entries = listing[1].split()
    for output in OUTPUTS:
        require(entries.count(output) == 1, f"shader output must be unique: {output}")
    for output, owner in zip(OUTPUTS[1:], (1, 2, 3, 4)):
        branch = re.search(r'(?:if|elseif)\(SHADER_FILE STREQUAL "' + re.escape(output) +
                           r'"\)(.*?)(?=\s*(?:elseif|endif)\()', cmake, re.S)
        require(branch is not None, f"missing source mapping for {output}")
        require('set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")' in branch[1] and
                f"set(SHADER_DEFINITIONS -D{OWNER}={owner})" in branch[1],
                f"{output} must compile the common body with owner {owner}")
    require(re.search(r"#ifndef\s+" + OWNER + r"\s+#define\s+" + OWNER + r"\s+0\b", shader),
            "ordinary chemistry.comp must default to bulk owner zero")
    require("set(SHADER_DEFINITIONS)" in cmake and "${SHADER_DEFINITIONS}" in cmake,
            "each shader command must reset and receive its owner definition")
    for needle in ('DEPENDS "${SHADER_INPUT}"', '"${SHADER_SOURCE_DIR}/chemistry_ownership.glsl"',
                   '"${SHADER_SOURCE_DIR}/material_ids.glsl"',
                   "add_custom_target(sandhybrid_runtime_shaders ALL",
                   "${SANDHYBRID_SHADER_BINARIES}",
                   "add_dependencies(SandHybrid_Demo sandhybrid_runtime_shaders)",
                   "install(FILES ${SANDHYBRID_SHADER_BINARIES} DESTINATION ${CMAKE_INSTALL_BINDIR}/shaders)"):
        require(needle in cmake, f"missing generated/deployed/installed shader dependency: {needle}")
    assignment = next((node.value for node in ast.parse(package).body
                       if isinstance(node, ast.Assign) and any(
                           isinstance(target, ast.Name) and target.id == "SHADERS" for target in node.targets)), None)
    require(isinstance(assignment, ast.Call) and isinstance(assignment.func, ast.Attribute) and
            assignment.func.attr == "split" and isinstance(assignment.func.value, ast.Constant) and
            isinstance(assignment.func.value.value, str), "package shader allowlist is not a literal split list")
    allowed = assignment.func.value.value.split()
    require(Counter(allowed) == Counter(entries), "package allowlist differs from built shader outputs")


class ChemistryPartitionContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.shader = (ROOT / "shaders/chemistry.comp").read_text(encoding="utf-8")
        cls.renderer = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
        cls.cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        cls.package = (ROOT / "tools/package_release.py").read_text(encoding="utf-8")
        cls.preprocessed = {owner: preprocess(cls.shader, owner) for owner in range(REFERENCE + 1)}

    def test_immutable_guard_precedes_every_scratch_or_counter_path(self) -> None:
        for owner, source in self.preprocessed.items():
            with self.subTest(owner=owner):
                validate_guard(source, owner)
        self.assertIn("readonlybufferCurrentCells{Cellcells[];};", compact(self.shader))
        self.assertIn("writeonlybufferNextCells{CellnextCells[];};", compact(self.shader))
        commit = compact(body(self.shader, "commitResult"))
        self.assertLess(commit.index("recordConservation(before,after);"), commit.index("nextCells[index]=after;"))

    def test_real_preprocessor_retains_each_family_in_its_own_pass(self) -> None:
        for owner, source in self.preprocessed.items():
            with self.subTest(owner=owner):
                validate_families(source, owner, self.preprocessed[REFERENCE])

    def test_source_specialization_is_immutable_and_reference_is_unsplit(self) -> None:
        self.assertIn('#include "chemistry_ownership.glsl"', self.shader)
        self.assertRegex(self.shader, r"#define\s+SOURCE_IS\(M\)\s+\(chemistrySourceOwner\(M\)\s*==\s*"
                         r"uint\(" + OWNER + r"\)\s*&&\s*source\.material\s*==\s*\(M\)\)")
        self.assertRegex(self.shader, r"#define\s+SOURCE_IS\(M\)\s+\(source\.material\s*==\s*\(M\)\)")
        self.assertNotRegex(body(self.shader), r"source\.material\s*(?:==|!=)\s*MAT_\w+")
        default = preprocess(self.shader, None)
        self.assertEqual(compact(body(default)), compact(body(self.preprocessed[0])))

    def test_renderer_uses_five_owners_before_one_correction_copyback(self) -> None:
        validate_renderer(self.renderer)

    def test_profile_markers_cover_each_owner_without_normal_runtime_commands(self) -> None:
        validate_profile(self.renderer)

    def test_build_deployment_and_package_share_all_five_outputs(self) -> None:
        validate_build(self.cmake, self.shader, self.package)

    def test_negative_late_guard_is_rejected(self) -> None:
        begin, end = function_span(self.shader, "main")
        main = self.shader[begin:end]
        guard = re.search(guard_pattern(OWNER), main)
        self.assertIsNotNone(guard)
        misplaced = main[:guard.start()] + main[guard.end():]
        self.assertEqual(misplaced.count("Cell result = source;"), 1)
        misplaced = misplaced.replace("Cell result = source;", guard[0] + "\nCell result = source;", 1)
        mutant = self.shader[:begin] + misplaced + self.shader[end:]
        with self.assertRaisesRegex(AssertionError, "guard must precede"):
            validate_guard(preprocess(mutant, 1), 1)

    def test_negative_nonbee_guard_cannot_swallow_bee_lifecycle(self) -> None:
        mutant = preprocess(wrap_bee_guard(self.shader), 1)
        with self.assertRaisesRegex(AssertionError, "Bees main rule"):
            validate_families(mutant, 1, self.preprocessed[REFERENCE])

    def test_negative_destination_and_phase_guards_cannot_exclude_their_own_rules(self) -> None:
        for owner, wrong_owner, family in ((3, 4, "destinations"), (4, 3, "phase carrier")):
            with self.subTest(owner=owner):
                conditional = f"#if {OWNER} == {owner} || {OWNER} == {REFERENCE}"
                replacement = f"#if {OWNER} == {wrong_owner} || {OWNER} == {REFERENCE}"
                self.assertEqual(self.shader.count(conditional), 1)
                mutant = preprocess(self.shader.replace(conditional, replacement, 1), owner)
                with self.assertRaisesRegex(AssertionError, family + " main rule"):
                    validate_families(mutant, owner, self.preprocessed[REFERENCE])

    def test_negative_own_source_predicate_loss_cannot_hide_behind_family_sentinels(self) -> None:
        for owner, material in ((3, "MAT_EMPTY"), (4, "MAT_CLOUD")):
            with self.subTest(owner=owner):
                old = f"else if (SOURCE_IS({material})) {{"
                mutant = preprocess(edit_main(self.shader, old, "else if (false) {"), owner)
                with self.assertRaisesRegex(AssertionError, "own-material rule was excluded"):
                    validate_families(mutant, owner, self.preprocessed[REFERENCE])

    def test_negative_missing_common_hazard_is_rejected(self) -> None:
        call = "applyStructuralHazards(p, source, result, nearWater, nearSaltwater, nearLava, nearFire, nearAcid);"
        mutant = preprocess(edit_main(self.shader, call, ""), 2)
        with self.assertRaisesRegex(AssertionError, "common rule was excluded"):
            validate_families(mutant, 2, self.preprocessed[REFERENCE])

    def test_negative_normal_runtime_profile_work_is_rejected(self) -> None:
        mutant = self.renderer.replace("if constexpr (Profile)", "if (true)", 1)
        self.assertNotEqual(mutant, self.renderer)
        with self.assertRaisesRegex(AssertionError, "compile-time profile guard"):
            validate_profile(mutant)

    def test_negative_result_guard_and_duplicate_host_owner_are_rejected(self) -> None:
        mutant = re.sub(r"chemistrySourceOwner\s*\(\s*source\s*\.\s*material\s*\)",
                        "chemistrySourceOwner(result.material)", self.preprocessed[1])
        self.assertNotEqual(mutant, self.preprocessed[1])
        with self.assertRaisesRegex(AssertionError, "immutable-source guard"):
            validate_guard(mutant, 1)
        mutant_host = self.renderer.replace("const std::array pipelines{chemistry_pipeline, chemistry_bees_pipeline,",
                                             "const std::array pipelines{chemistry_pipeline, chemistry_pipeline,", 1)
        self.assertNotEqual(mutant_host, self.renderer)
        with self.assertRaisesRegex(AssertionError, "each pipeline once"):
            validate_renderer(mutant_host)

    def test_helpers_cannot_fake_main_family_evidence(self) -> None:
        source = 'void helper() { bool authoredBee = true; }\nvoid main() { /* } */ return; }'
        self.assertNotIn("authoredBee", body(source))
        self.assertEqual(compact(body(source)), "return;")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glslc", required=True, type=Path,
                        help="Exact platform glslc executable supplied by the native CMake configuration")
    options, unittest_args = parser.parse_known_args()
    GLSLC = options.glslc.resolve()
    if not GLSLC.is_file():
        parser.error(f"glslc does not exist: {GLSLC}")
    unittest.main(argv=[sys.argv[0], *unittest_args], verbosity=2)
