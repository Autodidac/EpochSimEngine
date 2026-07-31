#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one marker, found {count}: {old[:80]!r}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


def patch_swarm() -> None:
    path = ROOT / "shaders/bee_swarm.glsl"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "const uint BEE_FORMATION_COUNT = 200u;\n",
        "const uint BEE_FORMATION_COUNT = 100u;\nconst uint BEE_COLONY_MAX = 100u;\n",
        1,
    )
    match = re.search(
        r"const uint BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\] = uint\[\]\((.*?)\n\);",
        text,
        re.S,
    )
    if not match:
        raise RuntimeError("bee_swarm.glsl: packed formation array missing")
    values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))]
    if len(values) != 200:
        raise RuntimeError(f"bee_swarm.glsl: expected 200 formation points, found {len(values)}")
    selected = values[::2]
    lines = []
    for offset in range(0, len(selected), 10):
        rendered = ", ".join(f"{value}u" for value in selected[offset:offset + 10])
        if offset + 10 < len(selected):
            rendered += ","
        lines.append(f"    {rendered}")
    replacement = (
        "const uint BEE_INITIAL_PACKED[BEE_FORMATION_COUNT] = uint[](\n"
        + "\n".join(lines)
        + "\n);"
    )
    text = text[:match.start()] + replacement + text[match.end():]
    text = text.replace("    uint epoch = step / 90u;\n", "    uint epoch = step / 360u;\n", 1)
    old_target = """    ivec2 anchor = beeFormationOffset(targetSlot);
    ivec2 flutter = beeRotateOffset(ivec2(1 + int(slot & 1u), 0), step / 3u + slot * 5u);
    return anchor + flutter;
"""
    new_target = """    // A stable, slightly enlarged mask reads as a symbol instead of 100 unrelated insects.
    ivec2 anchor = beeFormationOffset(targetSlot) * 5 / 4;
    ivec2 flutter = beeRotateOffset(ivec2(1, 0), step / 8u + slot * 5u);
    return anchor + flutter;
"""
    if text.count(old_target) != 1:
        raise RuntimeError("bee_swarm.glsl: biohazard target marker mismatch")
    text = text.replace(old_target, new_target, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_audit() -> None:
    path = ROOT / "tools/audit_ecology_motion.py"
    text = path.read_text(encoding="utf-8")
    text = text.replace("from pathlib import Path\n", "from pathlib import Path\nimport re\n", 1)
    text = text.replace(
        '    "chemistry": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),\n',
        '    "chemistry": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),\n'
        '    "reset": (root / "shaders/reset.comp").read_text(encoding="utf-8"),\n',
        1,
    )
    text = text.replace(
        '    "swarm": ["beeOrbitTarget", "beeSwarmTarget", "beeBiohazardTargetOffset",\n'
        '              "BEE_SWARM_BIOHAZARD_TICKS", "BEE_SWARM_ALTERNATE_TICKS"],\n',
        '    "swarm": ["beeOrbitTarget", "beeSwarmTarget", "beeBiohazardTargetOffset",\n'
        '              "BEE_SWARM_BIOHAZARD_TICKS", "BEE_SWARM_ALTERNATE_TICKS",\n'
        '              "BEE_FORMATION_COUNT = 100u", "BEE_COLONY_MAX = 100u",\n'
        '              "step / 360u", "beeFormationOffset(targetSlot) * 5 / 4"],\n',
        1,
    )
    text = text.replace(
        '    "chemistry": ["flowerDropsSeed", "grassFrontier", "stemMoisture",\n'
        '                  "source.material == MAT_PLANT_STEM",\n'
        '                  "Painted and loaded orphan bees self-seed"],\n',
        '    "chemistry": ["flowerDropsSeed", "grassFrontier", "stemMoisture",\n'
        '                  "source.material == MAT_PLANT_STEM",\n'
        '                  "Painted and loaded orphan bees self-seed",\n'
        '                  "respiringNeighborDemand", "beeRoll < demand.x",\n'
        '                  "fireRespiration", "BEE_COLONY_MAX"],\n'
        '    "reset": ["approved suspended hive", "queen.x - 38",\n'
        '              "q2 >= 25 && q2 < 92", "q.x >= 1 && q.x <= 10"],\n',
        1,
    )
    text = text.replace(
        '    "move": ["return tileHas(tileA, TILE_SLEEPING) && tileHas(tileB, TILE_SLEEPING);",\n'
        '             "targetDistance <= 49"],\n',
        '    "move": ["return tileHas(tileA, TILE_SLEEPING) && tileHas(tileB, TILE_SLEEPING);",\n'
        '             "targetDistance <= 49"],\n'
        '    "swarm": ["BEE_FORMATION_COUNT = 200u", "step / 90u"],\n'
        '    "chemistry": ["respiringNeighborCount",\n'
        '                  "if (nearFire || hasNeighbor(p, MAT_EMBER)"],\n',
        1,
    )
    old_print = 'print("Ecology and motion contracts passed: dynamic tiles, mobile painted bees, passable-media insects, gas displacement, grass, seeds, stems, and flowers.")'
    new_print = '''packed = re.search(r"BEE_INITIAL_PACKED\\[BEE_FORMATION_COUNT\\] = uint\\[\\]\\((.*?)\\n\\);", files["swarm"], re.S)
if packed is None or len(re.findall(r"\\d+u", packed.group(1))) != 100:
    errors.append("swarm: expected exactly 100 authored biohazard points")
macro = (root / "shaders/macro_move.comp").read_text(encoding="utf-8")
if "regionSupported(sourceOrigin, source) && target.material == MAT_EMPTY" in macro:
    errors.append("macro: horizontal bulk movement still rejects represented atmosphere")
if "regionSupported(sourceOrigin, source);" not in macro:
    errors.append("macro: horizontal density/displacement contract missing")
if errors:
    raise SystemExit("\\n".join(errors))
print("Ecology and motion contracts passed: 100-bee colony cap, slower respiration, stable biohazard mask, approved hive, represented-atmosphere macro displacement, mobile life, plants, and flowers.")'''
    if text.count(old_print) != 1:
        raise RuntimeError("audit_ecology_motion.py: final print marker mismatch")
    text = text.replace(old_print, new_print, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def write_docs() -> None:
    missioncache = '''# EpochSimEngine Mission Cache

**Mandatory workflow:** read this file before changing code. Update it in the same commit as mission work. Never delete an OPEN, PARTIAL, BLOCKED, DEFERRED, or REGRESSION mission. A mission becomes COMPLETE only after its acceptance criteria pass on Windows and Linux and, for visual/runtime behavior, after gameplay evidence confirms the result.

Status meanings: `OPEN` not implemented; `PARTIAL` code exists but acceptance is unmet; `REGRESSION` previously attempted behavior is visibly broken; `COMPLETE` verified; `DEFERRED` intentionally scheduled later with the reason retained.

## Release v2.4.0 scope

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-001 | COMPLETE | Preserve the v2.3.3 UI input repairs | Cursor-size controls and buttons use matching rendered/input layout, current event coordinates, and responsive hit areas. |
| MC-002 | COMPLETE | Preserve bee/bug movement through represented atmosphere | Painted bees, ants, and beetles move through gas/liquid by conserved displacement; oxygen remains breathable rather than a blocking solid. |
| MC-003 | COMPLETE | Preserve explicit oxygen/CO2 life model | Living agents consume oxygen, exchange it for CO2, and suffocate in zero oxygen, fully non-breathable gas, or liquid enclosure. Empty cells are not silently breathable. |
| MC-004 | COMPLETE | Preserve oxygen-filled eraser/vacuum scene initialization | Erased and authored atmosphere starts as canonical oxygen rather than unrepresented void. |
| MC-005 | REGRESSION | Preserve bee formation cycle | Code returns to biohazard, but the runtime symbol is difficult to read and the colony dies before repeated lifecycle cycles can be observed. |
| MC-006 | COMPLETE | Restore useful debug counters | SWAPS, macro moves, macro cells, fine repair, sleeping chunks, and skipped cells remain available. |
| MC-007 | COMPLETE | Reduce debug overlay cost | Statistics are sampled every 16 frames and unreadable dense 8x8 grid lines are omitted. |
| MC-008 | COMPLETE | Canonical wet-state materials | Wet sand, wet dirt, wet silt, and mud use material state rather than provenance; drying restores the same base material. |
| MC-009 | COMPLETE | Add Sluice Box processing | With falling water, eight wet-sand feed cells produce one gold and seven silt cells without creating or deleting represented mass. |
| MC-010 | COMPLETE | Structural integrity for cohesive solids | Full cohesive regions require physical support to stabilize; stability never reconstructs, fills, snaps, or synthesizes pixels. |
| MC-011 | PARTIAL | 64x64 chunk-first work rejection | Code caches active/sleeping/dirty/boundary chunks and skips sleeping neighborhoods. Runtime profiling must still prove that clean off-camera regions avoid fine work. |
| MC-012 | REGRESSION | 8x8 macro-element movement | v2.4.0 classified full regions, but horizontal bulk movement required literal empty cells even though open space is represented by oxygen. Runtime showed MACRO 0 / MCELL 0. |
| MC-013 | REGRESSION | True liquid settling without hopping | Equal-level random fallback was removed, but current gameplay still shows hopping and excessive movement. Water must reach a stable rest rapidly and remain still until pressure, support, or boundaries change. |
| MC-014 | PARTIAL | Half-water coalescence and presentation | Short-range attraction and stronger blended coverage exist. Isolated halves must still be eliminated without jitter, mass creation, or trapped single-half pockets. |
| MC-015 | PARTIAL | Normal rendering hides hierarchy artifacts | Normal presentation blends fine edges and half-water. Visible macro squares, block popping, and corner pressure artifacts require gameplay verification and repair. |
| MC-016 | OPEN | Rename hierarchy terminology | Choose durable names for pixel cells, 8x8 bulk elements, 64x64 simulation sections, loaded/frozen rings, and disk-streamed regions; update code, debug UI, and docs together. |

## Atmosphere and ecology follow-up

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-020 | ACTIVE | Reduce bee oxygen consumption | v2.4.1 code target is 128x slower bee respiration and 32x slower other-life respiration. Gameplay must show long-lived colonies while sealed oxygen-free spaces still eventually kill them. |
| MC-021 | ACTIVE | Bound fire CO2 production | v2.4.1 code target converts adjacent oxygen probabilistically from fire/ember instead of every tick. Output remains one conserved O2 volume to one CO2 volume. Runtime stress test remains required. |
| MC-022 | PARTIAL | Prevent lethal local CO2 piles through pressure transport | Horizontal macro displacement is being corrected to swap full represented gas/liquid regions by density. Partial-edge pressure transport still needs runtime proof. |
| MC-023 | OPEN | Validate closed-system volume and pressure | Gases/liquids carry conserved represented volume; displacement raises pressure, transfers existing volume, and never creates or silently deletes material. Add counters and closed-box tests. |
| MC-024 | OPEN | Explain and validate oxygen corner structures | Determine whether the observed oxygen corner pattern is legitimate pressure packing or a movement artifact; preserve the attractive look only when physically consistent. |
| MC-025 | OPEN | Correct fertilizer chemistry | Ember-to-fertilizer is not accepted as a direct reaction. Define a plausible ash/organic waste/silt/dirty-water compost path and conserve all inputs and products. |
| MC-026 | ACTIVE | Restore approved suspended hive | Use the exact earlier FastFreddy suspended wood beam, nest shell, entrance, honey/pollen chamber, and queen geometry in the ecosystem scene. |
| MC-027 | ACTIVE | Cap autonomous hive population at 100 bees | Initial authored formation contains 100 bees; queen/nest reproduction refuses births at the local 100-bee cap. Explicit user-painted bees are not silently deleted. |
| MC-028 | PARTIAL | Complete bee lifecycle | Preserve queen, nest, forage, pollen pickup, return, deposit, honey feeding, migration, aging, hazard death, oxygen use, CO2 exchange, and colony replacement. Runtime multi-cycle evidence remains required. |
| MC-029 | ACTIVE | Make biohazard formation readable | Use a stable, slightly enlarged 100-point mask with slower slot remapping and minimal flutter; biohazard remains the dominant phase and must visibly recur. |

## Performance, sections, and optional concurrency

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-030 | OPEN | Camera-visible simulation guarantee | Every section intersecting the camera is loaded and fully animated before presentation; the camera never shows stale or estimated cells. |
| MC-031 | OPEN | Twelve-nearest active sections | Activate the 12 sections nearest the player/camera first, with deterministic priority and boundary halos. The count must be configurable. |
| MC-032 | OPEN | Loaded frozen ring | Sections outside the active radius remain memory-resident but frozen. Entering the preload radius restores them before they become visible. |
| MC-033 | OPEN | Far-section disk streaming | Serialize the farthest clean sections to disk, free their live buffers, and reload them deterministically as the player approaches. Saves must be versioned and corruption-safe. |
| MC-034 | OPEN | Optional section concurrency | Default/reference mode is deterministic single-thread scheduling. An optional worker pool may process independent sections; results must match reference mode at section boundaries. |
| MC-035 | OPEN | Coroutine review | Use C++23 coroutines only for asynchronous streaming/I/O where they reduce blocking. Do not insert coroutines into ordered Vulkan submission or per-cell hot paths. |
| MC-036 | OPEN | Freeze unseen simulation safely | Off-camera sections freeze only after pending reactions, cross-boundary transfers, actors, pressure, and streaming dependencies are resolved. |
| MC-037 | OPEN | Rendering and debug GPU benchmark | Measure overlay, grid, text, stats collection, macro pass, fine pass, and presentation costs separately. Debug visualization must remain a small minority of frame GPU time. |

## Library architecture and EpochEngine migration path

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-040 | OPEN | Build a static simulation library | Produce a C++23 static library target with public ownership-safe headers; platform windowing and `main` are not part of the library API. |
| MC-041 | OPEN | Thin demo executable | Build `EpochSimEngine_Demo` as a small executable that links the static library and owns native window/event startup. |
| MC-042 | OPEN | Optional subsystems | Concurrency, disk streaming, debug visualization, UI, actors, ecology, and factories can be disabled without forking the simulation core. |
| MC-043 | DEFERRED | EpochEngine integration | Later migrate/rewrite the library into EpochEngine using repository-canonical `epochengine::` APIs and architecture. Keep current boundaries migration-friendly. |

## Permanent invariants

- The cell/material state is authoritative; hierarchy metadata accelerates it but never replaces or invents represented material.
- Material behavior is canonical and provenance-independent.
- No silent deletion, vaporization, reconstruction, or gas creation is permitted.
- 8x8 terrain regions qualify for stability only; they never reconstruct missing cells.
- Each terrain pixel takes two laser hits to dislodge; after more than half are dislodged, the represented remainder collapses rather than vanishing.
- Missed, avoided, failed, and deferred missions remain visible in this cache.
'''
    ledger = '''# EpochSimEngine Release Ledger

This file is the durable release ledger. A mission remains open until its acceptance criteria are verified in Windows and Linux Release builds. Missed, deferred, avoided, and runtime-regressed work must remain listed rather than disappearing between releases.

## v2.4.0 macro hierarchy release — corrected by runtime evidence

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| M01 | REGRESSION | Preserve v2.3.3 life/oxygen behavior | Bees retained the explicit model but consumed oxygen too quickly and died from local CO2 accumulation. Reopened as MC-020 through MC-022. |
| M02 | COMPLETE | Chunk-first work rejection | A clean sleeping 64x64 chunk skips its 8x8 tile scans; dirty/boundary chunks wake deterministically. Runtime off-camera profiling remains MC-011. |
| M03 | COMPLETE | Cached 8x8 macro classification | Tile metadata records uniform, macro-movable, macro-solid, macro-powder, macro-liquid, macro-gas, fine-active, wet, and settled-medium state. |
| M04 | REGRESSION | Full-block macro movement | Runtime showed `MACRO 0 / MCELL 0`. Horizontal bulk movement rejected oxygen-filled open space by requiring literal `MAT_EMPTY`. Reopened as MC-012. |
| M05 | COMPLETE | Structural integrity for solid blocks | Cohesive full solid regions stabilize only with physical support and remain represented by their original cells; no reconstruction or synthesized pixels. |
| M06 | COMPLETE | Wet material model | Wet sand, wet dirt, wet silt, and mud use canonical AUX_WET state; full regions are eligible for macro movement and mixed edges receive bounded fine repair. |
| M07 | REGRESSION | Settled liquid behavior | Equal-level random fallback was removed, but gameplay still showed a moving slope and hopping. Reopened as MC-013. |
| M08 | PARTIAL | Half-water coalescence and presentation | Conservative attraction and presentation exist; isolated halves and settling still require runtime proof. |
| M09 | COMPLETE | Sluice-box processing | A buildable Sluice Box accepts wet sand with falling water and conserves eight feed cells as one gold plus seven silt outputs. |
| M10 | COMPLETE | Debug regression and GPU cost | SWAPS and hierarchy skip counts are visible; debug sampling is reduced and dense 8x8 grid lines are omitted. |
| M11 | PARTIAL | Rendering review | Normal rendering blends half-cell edges, but macro utilization and visible pressure/corner artifacts still require correction and measurement. |
| M12 | PARTIAL | Threading/concurrency review | Native events and Vulkan simulation remain on separate explicit threads. Optional section workers, frozen rings, disk streaming, and coroutine I/O remain open. |
| M13 | COMPLETE | Cross-platform release gate | The v2.4.0 source and packages passed their automated Windows/Linux gates. Visual/runtime acceptance was not implied by those builds. |

## v2.4.1 colony, atmosphere, and represented-space macro correction

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| C01 | ACTIVE | Restore approved suspended hive | Use the exact earlier FastFreddy wood support, nest shell, entrance, chamber, honey/pollen, and queen geometry. |
| C02 | ACTIVE | Cap autonomous colonies at 100 bees | Initial authored colony uses 100 formation slots and autonomous births stop at 100 local bees without deleting user-painted life. |
| C03 | ACTIVE | Slow respiration and bound fire CO2 | Bee O2 exchange is reduced 128x, other-life exchange 32x, fire/ember conversion is probabilistic, and every exchange remains one O2 volume to one CO2 volume. |
| C04 | ACTIVE | Make biohazard readable | The 100-point mask is slightly enlarged, remaps more slowly, has minimal flutter, remains dominant, and visibly recurs. |
| C05 | ACTIVE | Use represented atmosphere in horizontal macro movement | Full liquid/gas regions use validated density displacement into oxygen/CO2/etc.; no literal-empty requirement remains. |
| C06 | PARTIAL | Preserve full bee lifecycle | Forage, pollen pickup, return, deposit, honey feeding, queen migration, nest aging, hazard death, respiration, and suffocation remain wired. Multi-cycle gameplay evidence is still required. |
| C07 | OPEN | Runtime acceptance | Verify non-zero macro counters, longer-lived 100-bee colony, repeated readable biohazard cycles, bounded CO2, and stable liquid behavior in Windows and Linux builds. |

## Active follow-up

`missioncache.md` is the canonical cross-release backlog. It contains hierarchy, liquid, atmosphere, ecology, section streaming, optional concurrency, naming, static-library/demo, and EpochEngine migration missions.

## Carry-forward rule

Any future regression or incomplete acceptance criterion reopens the same mission ID. New work is appended; prior missions are never silently removed.
'''
    (ROOT / "missioncache.md").write_text(missioncache, encoding="utf-8", newline="\n")
    (ROOT / "MISSION_LEDGER.md").write_text(ledger, encoding="utf-8", newline="\n")
    readme = ROOT / "README.md"
    text = readme.read_text(encoding="utf-8")
    section = '''\n\n## v2.4.1 correction\n\nRestores the approved suspended hive, caps autonomous colonies at 100 bees, slows bee respiration, bounds fire/ember oxygen-to-CO2 exchange, stabilizes and enlarges the biohazard mask, and fixes horizontal 8x8 bulk movement so represented oxygen/CO2 is displaceable atmosphere rather than requiring literal empty cells. Runtime acceptance remains tracked in `missioncache.md`.\n'''
    if "## v2.4.1 correction" not in text:
        readme.write_text(text + section, encoding="utf-8", newline="\n")


def main() -> None:
    patch_swarm()
    replace_once(
        "shaders/reset.comp",
        '''    // Fix29 compact hive plus a hidden moving swarm mask. The hive is not terrain.
    ivec2 queen = ecosystemQueen();
    ivec2 q = p - queen;
    int q2 = q.x * q.x + q.y * q.y;
    if (q2 >= 28 && q2 < 108) material = MAT_BEE_NEST;
    if (q2 == 0) material = MAT_QUEEN_BEE;
    else if (q.x >= 1 && q.x <= 12 && abs(q.y) <= 1) material = MAT_EMPTY;
    else if (q2 < 28) {
        uint chamber = hash32(indexOf(p) ^ pc.seed ^ 0xb33u);
        material = (chamber & 3u) == 0u ? MAT_EMPTY : ((chamber & 4u) == 0u ? MAT_HONEY : MAT_POLLEN);
    }
    int formationSlot = beeFormationSlotFromOffset(q);
    if (material == MAT_EMPTY && formationSlot >= 0) material = MAT_BEE;
''',
        '''    // Restore the approved suspended hive from the earlier FastFreddy ecology scene.
    // It remains ordinary canonical nest/honey/pollen/queen cells, never a special prop.
    ivec2 queen = ecosystemQueen();
    ivec2 q = p - queen;
    int q2 = q.x * q.x + q.y * q.y;
    if (p.x > queen.x - 38 && p.x < queen.x + 30 &&
        p.y > queen.y - 17 && p.y < queen.y - 12) material = MAT_WOOD;
    if (q2 >= 25 && q2 < 92) material = MAT_BEE_NEST;
    if (q2 == 0) material = MAT_QUEEN_BEE;
    else if (q.x >= 1 && q.x <= 10 && abs(q.y) <= 1) material = MAT_EMPTY;
    else if (q2 < 25) {
        uint chamber = hash32(indexOf(p) ^ pc.seed ^ 0xb33u);
        material = (chamber & 3u) == 0u ? MAT_EMPTY
            : (((chamber >> 2u) & 1u) == 0u ? MAT_HONEY : MAT_POLLEN);
    }
    int formationSlot = beeFormationSlotFromOffset(q);
    if (material == MAT_EMPTY && formationSlot >= 0) material = MAT_BEE;
''',
    )
    replace_once(
        "shaders/chemistry.comp",
        '''uint respiringNeighborCount(ivec2 p) {
    uint count = 0u;
    for (uint i = 0u; i < 8u; ++i) {
        if (isRespiringLife(at(p + neighborOffsets[i]).material)) ++count;
    }
    return count;
}
''',
        '''uvec2 respiringNeighborDemand(ivec2 p) {
    uvec2 demand = uvec2(0u); // x: bees, y: all other breathing life
    for (uint i = 0u; i < 8u; ++i) {
        uint material = at(p + neighborOffsets[i]).material;
        if (material == MAT_BEE || material == MAT_QUEEN_BEE) ++demand.x;
        else if (isRespiringLife(material)) ++demand.y;
    }
    return demand;
}
''',
    )
    replace_once(
        "shaders/chemistry.comp",
        '''    if (source.material == MAT_OXYGEN) {
        uint volume = representedOxygenVolume(source);
        uint consumers = respiringNeighborCount(p);
        if (nearFire || hasNeighbor(p, MAT_EMBER) ||
            (consumers > 0u && (randomValue & 2047u) < consumers)) {
            result = makeCell(MAT_CARBON_DIOXIDE);
            setStateValue(result, volume);
        }
''',
        '''    if (source.material == MAT_OXYGEN) {
        uint volume = representedOxygenVolume(source);
        uvec2 demand = respiringNeighborDemand(p);
        uint beeRoll = hash32(randomValue ^ 0xb33a71u) & 0x0003ffffu;
        uint lifeRoll = hash32(randomValue ^ 0x11fe21u) & 0x0000ffffu;
        bool beeRespiration = demand.x > 0u && beeRoll < demand.x;       // 128x slower than v2.4.0
        bool otherRespiration = demand.y > 0u && lifeRoll < demand.y;  // 32x slower than v2.4.0
        bool fireRespiration = nearFire && (randomValue & 127u) == 0u;
        bool emberRespiration = hasNeighbor(p, MAT_EMBER) &&
                                (hash32(randomValue ^ 0xe6be2u) & 2047u) == 0u;
        if (beeRespiration || otherRespiration || fireRespiration || emberRespiration) {
            result = makeCell(MAT_CARBON_DIOXIDE);
            setStateValue(result, volume);
        }
''',
    )
    replace_once(
        "shaders/chemistry.comp",
        '''                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            countWithin(p, MAT_BEE, 6) < 4u && (randomValue & 32767u) == 0u) {
                    result = makeCell(MAT_BEE);
''',
        '''                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            beePopulationAroundTile(tileIndex(p + queenOffset, pc.width), 6) < BEE_COLONY_MAX &&
                            countWithin(p, MAT_BEE, 6) < 4u && (randomValue & 32767u) == 0u) {
                    result = makeCell(MAT_BEE);
''',
    )
    replace_once(
        "shaders/chemistry.comp",
        "            bool reproductiveSwarm = localBees >= 180u && localHoney;\n",
        "            bool reproductiveSwarm = localBees >= (BEE_COLONY_MAX * 9u) / 10u && localHoney;\n",
    )
    replace_once(
        "shaders/macro_move.comp",
        '''                allowed = (tileHas(sourceState, TILE_MACRO_LIQUID) || tileHas(sourceState, TILE_MACRO_GAS)) &&
                          regionSupported(sourceOrigin, source) && target.material == MAT_EMPTY;
''',
        '''                // Open space is represented by gas, so horizontal bulk movement must use
                // the already-validated density/displacement contract rather than MAT_EMPTY.
                allowed = (tileHas(sourceState, TILE_MACRO_LIQUID) || tileHas(sourceState, TILE_MACRO_GAS)) &&
                          regionSupported(sourceOrigin, source);
''',
    )
    patch_audit()
    write_docs()


if __name__ == "__main__":
    main()
