#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    ".cpp", ".hpp", ".h", ".glsl", ".comp", ".frag", ".vert", ".py",
    ".md", ".txt", ".cmake", ".in", ".yml", ".yaml", ".bat",
}
SKIP_PARTS = {".git", "build", "dist", "third_party", "vcpkg_installed"}


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if old in text:
        write(path, text.replace(old, new, 1))
    elif new not in text:
        raise RuntimeError(f"{path}: expected block not found: {old[:100]!r}")


def regex_once(path: str, pattern: str, replacement: str) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: expected one regex match, found {count}: {pattern}")
    write(path, updated)


# Canonical names. Numeric material IDs 31 and 48 remain unchanged for saves;
# there are deliberately no source aliases or deprecated constants.
RENAMES = (
    ("MAT_BEE_NEST", "MAT_BEEHIVE"),
    ("bee_nest", "beehive"),
    ("Bee Nest", "Beehive"),
    ("Bee nest", "Beehive"),
    ("bee nest", "beehive"),
    ("BEE NEST", "BEEHIVE"),
    ("MAT_IRON_SHAVINGS", "MAT_IRON_ORE"),
    ("iron_shavings", "iron_ore"),
    ("Iron Shavings", "Iron Ore"),
    ("Iron shavings", "Iron ore"),
    ("iron shavings", "iron ore"),
    ("IRON SHAVINGS", "IRON ORE"),
)

for path in ROOT.rglob("*"):
    if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
        continue
    if any(part in SKIP_PARTS for part in path.parts):
        continue
    text = path.read_text(encoding="utf-8")
    updated = text
    for old, new in RENAMES:
        updated = updated.replace(old, new)
    if updated != text:
        path.write_text(updated, encoding="utf-8")


# Shared exact Fix28 compact Beehive geometry. Reset and paint use this single
# implementation so authored and buildable hives cannot drift again.
write(
    "shaders/beehive.glsl",
    """#ifndef SANDHYBRID_BEEHIVE_GLSL
#define SANDHYBRID_BEEHIVE_GLSL

const int BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 28;
const int BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 108;
const int BEEHIVE_CHAMBER_RADIUS_SQUARED = 28;
const int BEEHIVE_EXIT_MIN_X = 1;
const int BEEHIVE_EXIT_MAX_X = 12;
const int BEEHIVE_EXIT_HALF_HEIGHT = 1;

// Exact compact hive from SandHybrid Windows x64 Fix28. MATERIAL_COUNT means
// the position belongs to the surrounding swarm rather than the hive body.
uint beehivePrefabMaterial(ivec2 offset, uint entropy) {
    int radiusSquared = offset.x * offset.x + offset.y * offset.y;
    if (radiusSquared == 0) return MAT_QUEEN_BEE;
    if (offset.x >= BEEHIVE_EXIT_MIN_X && offset.x <= BEEHIVE_EXIT_MAX_X &&
        abs(offset.y) <= BEEHIVE_EXIT_HALF_HEIGHT)
        return MAT_EMPTY;
    if (radiusSquared < BEEHIVE_CHAMBER_RADIUS_SQUARED) {
        if ((entropy & 3u) == 0u) return MAT_EMPTY;
        return (entropy & 4u) == 0u ? MAT_HONEY : MAT_POLLEN;
    }
    if (radiusSquared >= BEEHIVE_SHELL_MIN_RADIUS_SQUARED &&
        radiusSquared < BEEHIVE_SHELL_MAX_RADIUS_SQUARED)
        return MAT_BEEHIVE;
    return MATERIAL_COUNT;
}

#endif
""",
)

replace_once(
    "shaders/reset.comp",
    '#include "bee_swarm.glsl"\n',
    '#include "bee_swarm.glsl"\n#include "beehive.glsl"\n',
)
regex_once(
    "shaders/reset.comp",
    r"// Exact approved suspended hive geometry.*?\nvoid applySuspendedHive\(.*?\n\}\n\nuint sandboxMaterial",
    """// Canonical Fix28 compact Beehive shared by generated scenes and the tool.
void applyBeehive(ivec2 p, ivec2 queen, inout uint material, bool includeFormation) {
    ivec2 offset = p - queen;
    uint prefab = beehivePrefabMaterial(offset, hash32(indexOf(p) ^ pc.seed ^ 0xb33u));
    if (prefab != MATERIAL_COUNT) material = prefab;
    if (includeFormation) {
        int slot = beeFormationSlotFromOffset(offset);
        if (material == MAT_EMPTY && slot >= 0) material = MAT_BEE;
    }
}

uint sandboxMaterial""",
)
reset = read("shaders/reset.comp").replace("applySuspendedHive(", "applyBeehive(")
reset = reset.replace("Exact pre-PR19 suspended hive", "Canonical Fix28 compact Beehive")
write("shaders/reset.comp", reset)

replace_once(
    "shaders/paint.comp",
    '#include "bee_swarm.glsl"\n',
    '#include "bee_swarm.glsl"\n#include "beehive.glsl"\n',
)
regex_once(
    "shaders/paint.comp",
    r"    if \(material == MAT_BEEHIVE\) \{.*?\n    \} else \{\n        cell = makeCell\(material\);\n    \}",
    """    if (material == MAT_BEEHIVE) {
        uint prefab = beehivePrefabMaterial(delta, cellHash(p, 0xb33u));
        if (prefab != MATERIAL_COUNT) {
            cell = makeCell(prefab);
        } else {
            colonySlot = beeFormationSlotFromOffset(delta);
            if (colonySlot < 0) return;
            cell = makeCell(MAT_BEE);
        }
    } else {
        cell = makeCell(material);
    }""",
)

# ID 48 is now Iron Ore. Loose cells retain powder gravity; tile placement is
# structural because the same ID is block-capable in every shared contract.
catalog = read("tools/material_catalog.py")
catalog = catalog.replace(
    "'aluminum', 'glass', 'iron', 'copper'",
    "'aluminum', 'glass', 'iron', 'iron_ore', 'copper'",
)
write("tools/material_catalog.py", catalog)

for shader_path in ("shaders/materials.glsl", "shaders/move.comp"):
    text = read(shader_path)
    text = text.replace(
        "material == MAT_ALUMINUM || material == MAT_GLASS || material == MAT_IRON ||\n",
        "material == MAT_ALUMINUM || material == MAT_GLASS || material == MAT_IRON ||\n"
        "           material == MAT_IRON_ORE ||\n",
        1,
    )
    write(shader_path, text)

material_hpp = read("include/sandhybrid/material.hpp")
material_hpp = material_hpp.replace(
    "    case Material::iron:\n    case Material::copper:",
    "    case Material::iron:\n    case Material::iron_ore:\n    case Material::copper:",
    1,
)
write("include/sandhybrid/material.hpp", material_hpp)

# Scene PPM files now use stable colors selected directly from the visible cell
# palette instead of a mathematically unique but visually meaningless hash.
scene_cpp = read("src/scene_image.cpp")
scene_cpp = scene_cpp.replace(
    '#include "sandhybrid/material.hpp"\n',
    '#include "sandhybrid/material.hpp"\n#include "sandhybrid/material_color.hpp"\n',
    1,
)
scene_cpp = re.sub(
    r"\nstruct Rgb final \{.*?\n\};\n\nconstexpr Rgb scene_color\(.*?\n\}\n",
    "\n",
    scene_cpp,
    count=1,
    flags=re.S,
)
scene_cpp = re.sub(
    r"std::uint32_t material_from_color\(const Rgb color\) noexcept \{.*?\n\}\n",
    "std::uint32_t material_from_color(const Rgb8 color) noexcept {\n"
    "    return material_from_editor_color(color);\n"
    "}\n",
    scene_cpp,
    count=1,
    flags=re.S,
)
scene_cpp = scene_cpp.replace("std::vector<Rgb> pixels", "std::vector<Rgb8> pixels")
scene_cpp = scene_cpp.replace("scene_color(", "material_editor_color(")
scene_cpp = scene_cpp.replace(
    "Use exact colors for lossless editing. Non-key colors load as the nearest material.",
    "Colors are stable representatives of the visible cell palette for ordinary Paint editing. "
    "Exact key colors are lossless; nearby colors load as the nearest material.",
)
write("src/scene_image.cpp", scene_cpp)

color_hpp = read("include/sandhybrid/material_color.hpp")
if "static_assert(sizeof(Rgb8) == 3u);" not in color_hpp:
    color_hpp = color_hpp.replace(
        "};\n\n// Stable, paint-editor-friendly",
        "};\nstatic_assert(sizeof(Rgb8) == 3u);\n\n// Stable, paint-editor-friendly",
        1,
    )
write("include/sandhybrid/material_color.hpp", color_hpp)

scene_readme = read("scenes/README.txt")
scene_readme = scene_readme.replace(
    "material_key.txt and material_key.ppm are generated beside the scenes. Use their exact colors for lossless material editing. Other colors are mapped to the nearest material.",
    "material_key.txt and material_key.ppm are generated beside the scenes. Their stable RGB swatches closely match the visible simulation cells, so the PPM can be edited by eye in ordinary Paint. Exact key colors are lossless; nearby colors map to the nearest material.",
)
write("scenes/README.txt", scene_readme)

# Public API, tests, shader dependencies, and package version.
cmake = read("CMakeLists.txt")
cmake = cmake.replace("project(SandHybrid VERSION 2.5.1 LANGUAGES CXX)",
                      "project(SandHybrid VERSION 2.5.2 LANGUAGES CXX)", 1)
cmake = cmake.replace(
    '                    "${SHADER_SOURCE_DIR}/bee_swarm.glsl"\n',
    '                    "${SHADER_SOURCE_DIR}/bee_swarm.glsl"\n'
    '                    "${SHADER_SOURCE_DIR}/beehive.glsl"\n',
    1,
)
cmake = cmake.replace(
    "    include/sandhybrid/material.hpp\n",
    "    include/sandhybrid/material.hpp\n    include/sandhybrid/material_color.hpp\n",
    1,
)
cmake = cmake.replace(
    "    add_executable(sandhybrid_behavior_contract tests/behavior_contract.cpp)",
    "    add_executable(sandhybrid_material_color_contract tests/material_color_contract.cpp)\n"
    "    target_link_libraries(sandhybrid_material_color_contract PRIVATE SandHybrid::SandHybrid)\n"
    "    target_compile_features(sandhybrid_material_color_contract PRIVATE cxx_std_23)\n"
    "    sandhybrid_configure_warnings(sandhybrid_material_color_contract)\n"
    "    add_test(NAME sandhybrid_material_color_contract COMMAND sandhybrid_material_color_contract)\n\n"
    "    add_executable(sandhybrid_behavior_contract tests/behavior_contract.cpp)",
    1,
)
write("CMakeLists.txt", cmake)

library_hpp = read("include/sandhybrid/library.hpp")
if '#include "sandhybrid/material_color.hpp"' not in library_hpp:
    library_hpp = library_hpp.replace(
        '#include "sandhybrid/material.hpp"\n',
        '#include "sandhybrid/material.hpp"\n#include "sandhybrid/material_color.hpp"\n',
        1,
    )
write("include/sandhybrid/library.hpp", library_hpp)

# Strengthen the compile-time material contract around the two canonical ID
# renames and the cell/tile distinction.
material_test = read("tests/material_contract.cpp")
material_test = material_test.replace(
    "static_assert(static_cast<std::uint32_t>(Material::atmosphere) == 66u);",
    "static_assert(static_cast<std::uint32_t>(Material::beehive) == 31u);\n"
    "static_assert(static_cast<std::uint32_t>(Material::iron_ore) == 48u);\n"
    "static_assert(static_cast<std::uint32_t>(Material::atmosphere) == 66u);",
    1,
)
material_test = material_test.replace(
    "static_assert(sandhybrid::is_block_material(Material::stone));",
    "static_assert(sandhybrid::material_names[31u] == \"Beehive\");\n"
    "static_assert(sandhybrid::material_names[48u] == \"Iron ore\");\n"
    "static_assert(sandhybrid::is_block_material(Material::stone));\n"
    "static_assert(sandhybrid::is_block_material(Material::iron_ore));",
    1,
)
write("tests/material_contract.cpp", material_test)

# Regenerate packed UI/card data from the renamed canonical catalog. Other
# zero-argument generators that explicitly consume material_catalog are safe to
# run and keep generated C++/GLSL contracts synchronized.
for generator in sorted((ROOT / "tools").glob("generate_*.py")):
    source = generator.read_text(encoding="utf-8")
    if generator.name == "generate_ui_text.py" or "material_catalog" in source:
        subprocess.run(["python3", str(generator)], cwd=ROOT, check=True)

# Static contract validator: aliases and legacy user-facing names are forbidden;
# both scene paths must consume the shared palette and Fix28 prefab.
validator = read("tools/validate_shader_contracts.py")
validator = validator.replace(
    'for forbidden in ("gold_ore", "iron_ore", "metal", "ally_bot", "enemy_bot", "bot_fabricator"):','for forbidden in ("gold_ore", "iron_shavings", "bee_nest", "metal", "ally_bot", "enemy_bot", "bot_fabricator"):',
    1,
)
insert_anchor = "    if not count_match or int(count_match.group(1)) != material_count:\n        errors.append(\"MATERIAL_COUNT does not match Material::count\")\n"
insert_block = insert_anchor + """
    if cpp_ids.get("beehive") != 31:
        errors.append("canonical Beehive material must retain save ID 31")
    if cpp_ids.get("iron_ore") != 48:
        errors.append("canonical Iron Ore material must retain save ID 48")

    legacy_tokens = ("MAT_BEE_NEST", "bee_nest", "Bee nest", "MAT_IRON_SHAVINGS", "iron_shavings", "Iron shavings")
    legacy_roots = (ROOT / "include", ROOT / "src", ROOT / "shaders", ROOT / "tests", ROOT / "tools")
    for legacy_root in legacy_roots:
        for source_path in legacy_root.rglob("*"):
            if not source_path.is_file() or source_path.suffix.lower() not in {".cpp", ".hpp", ".glsl", ".comp", ".frag", ".vert", ".py"}:
                continue
            source_text = source_path.read_text(encoding="utf-8")
            for token in legacy_tokens:
                if token in source_text:
                    errors.append(f"legacy alias/token remains in {source_path.relative_to(ROOT)}: {token}")

    beehive_glsl = (SHADERS / "beehive.glsl").read_text(encoding="utf-8")
    for token in ("BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 28", "BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 108", "BEEHIVE_EXIT_MAX_X = 12", "beehivePrefabMaterial"):
        if token not in beehive_glsl:
            errors.append(f"Fix28 Beehive contract missing {token!r}")

    scene_image_cpp = (ROOT / "src/scene_image.cpp").read_text(encoding="utf-8")
    for token in ("material_color.hpp", "material_editor_color", "material_from_editor_color"):
        if token not in scene_image_cpp:
            errors.append(f"paint-editable scene palette contract missing {token!r}")
    if "% 224u" in scene_image_cpp or "scene_color(" in scene_image_cpp:
        errors.append("obsolete hashed scene palette remains")
"""
if insert_anchor not in validator:
    raise RuntimeError("tools/validate_shader_contracts.py: material count anchor not found")
validator = validator.replace(insert_anchor, insert_block, 1)
write("tools/validate_shader_contracts.py", validator)

# Canonical backlog: explicit priorities, no release scrapbook, and missing
# authoring/naming goals recovered from the conversation chain.
mission = read("missioncache.md")
mission = mission.replace(
    "This is the **single canonical mission document** for SandHybrid. It contains active work, permanent invariants, and archived release history. There is no separate mission ledger.",
    "This is the **single canonical product backlog** for SandHybrid. It contains active work, priority, acceptance criteria, permanent invariants, and concise accepted foundations. Release history lives only in `CHANGELOG.md`.",
    1,
)
priority_anchor = "Statuses: `OPEN`, `PARTIAL`, `REGRESSION`, `DEFERRED`.\n\n"
priority_block = priority_anchor + """## Priority lanes

- **P0 / primary release gate:** MC-038, MC-112, MC-115, and MC-116. These are the current release blockers and must pass deterministic contracts plus Windows/Linux Release packaging before publication.
- **P1 / runtime correctness:** simulation, conservation, water, atmosphere, ecology, machinery, UI, scene, and performance missions that require packaged observation or deterministic runtime evidence.
- **P2 / architecture and later integration:** streaming, full replacement-runtime cutover, optional subsystem extraction, and EpochEngine integration.
- Priority is a scheduling property, not a second status system. Every unfinished row remains in this table exactly once.

"""
if "## Priority lanes" not in mission:
    mission = mission.replace(priority_anchor, priority_block, 1)

mission = re.sub(
    r"\| MC-038 \|.*?\|\n",
    "| MC-038 | PARTIAL | Canonical Fix28 Beehive prefab | Material save ID 31 is named `Beehive` in code, shaders, UI, cards, debug, docs, scene keys, and mission text with no alias constant. Sandbox, Ecosystem, and the buildable tool use one shared Fix28 compact prefab: shell `28 <= radius² < 108`, chamber `radius² < 28`, queen at center, and right entrance `x=1..12`, `|y|<=1`. Reset/place/save/load preserve queen and 100-bee colony metadata. Static contracts pass; packaged reset/place/save/load observation remains required. |\n",
    mission,
    count=1,
)
mission = re.sub(
    r"\| MC-032 \|.*?\|\n",
    "| MC-032 | PARTIAL | Complete bee lifecycle | Forage, pollen, Beehive return, deposit, honey feeding, queen/Be​​ehive aging, migration, hazards, respiration, replacement, and the 100-bee autonomous cap pass multi-cycle runtime testing. |\n".replace("Be​​ehive", "Beehive"),
    mission,
    count=1,
)
mission = re.sub(
    r"\| MC-037 \|.*?\|\n",
    "| MC-037 | PARTIAL | Life debug counters | Debug reports actor moves and species counts for bees, queens, Beehives, ants, beetles, habitats, flowers, pollen, and honey. Respiration, suffocation, births, deaths, Beehive returns, and medium displacement require separate counters and runtime acceptance. |\n",
    mission,
    count=1,
)
mission = re.sub(
    r"\| MC-112 \|.*?\|\n",
    "| MC-112 | PARTIAL | Stone, Iron Ore, and Iron cell/tile parity | Save ID 48 is canonically `Iron Ore`; no Iron Shavings identifier or alias remains. Stone, Iron Ore, and refined Iron use `CELLS` for loose individual pixels and `TILES` for supported structural 8x8 arrangements. Tile metadata never translates the solid wholesale, and at 31 destroyed cells the remainder releases as loose cells. Static ID, palette, phase, block-capability, and shader contracts pass; packaged placement/fracture proof remains required. |\n",
    mission,
    count=1,
)

scene_heading = "## Scene authoring and repository hygiene\n\n| ID | Status | Mission | Acceptance |\n|---|---|---|---|\n"
scene_rows = (
    "| MC-115 | PARTIAL | Paint-editable scene image palette | Every material has one unique stable RGB scene color chosen near its ordinary rendered cell color. Save, Load, `material_key.txt`, and `material_key.ppm` use the same shared C++ table; exact colors round-trip losslessly and small Paint/resampling differences choose the nearest material. The old hashed permutation palette is removed. Static round-trip contracts pass; packaged Paint edit/load observation remains required. |\n"
    "| MC-116 | PARTIAL | Canonical backlog and one brief changelog | `missioncache.md` contains each unfinished requirement once with priority and acceptance criteria, not per-release prose. `CHANGELOG.md` is the only release-history file. Versioned release-note files, one-shot patch payloads, obsolete workflows, and completed agent branches are removed before publication. CI validates the cache and release tree. |\n\n"
)
if "| MC-115 |" not in mission:
    mission = mission.replace("## Chemistry and materials\n", scene_heading + scene_rows + "## Chemistry and materials\n", 1)

# Remove the obsolete giant broad-pass snapshot; active rows themselves are the audit.
mission = re.sub(
    r"## v2\.4\.8 broad-pass audit\n.*?(?=# Permanent invariants)",
    "## Backlog review policy\n\nEvery P0 mission is attempted in the current release. Every P1/P2 row is reviewed for contradiction and retained until its own acceptance passes. Static compilation never closes visual, runtime, conservation, or performance work.\n\n",
    mission,
    count=1,
    flags=re.S,
)
mission = mission.replace(
    "- Every generated scene and every loaded saved counterpart uses the same upper-center authored footprint, three-zone subterranean geology stack, and stone-wrapped two-brick bottom lava band.\n",
    "- Every generated scene and every loaded saved counterpart uses the same upper-center authored footprint, three-zone subterranean geology stack, and stone-wrapped two-brick bottom lava band.\n"
    "- Scene PPM colors are unique, stable, paint-friendly representatives of visible cell colors and share one Save/Load/material-key table.\n"
    "- Material save ID 31 is Beehive and ID 48 is Iron Ore; neither has a deprecated source alias.\n",
    1,
)
mission = re.sub(
    r"# Archived release history\n.*\Z",
    """# Accepted foundations

| ID | Accepted foundation | Evidence location |
|---|---|---|
| MC-070 | Reusable platform-neutral `SandHybrid::SandHybrid` core library, optional Vulkan runtime, thin demo host, installed CMake package, and downstream consumer contract. | `CHANGELOG.md`; Windows/Linux CI history. |

Detailed release history belongs only in `CHANGELOG.md`. Completed source branches and one-shot patch machinery are deleted after publication.
""",
    mission,
    count=1,
    flags=re.S,
)
write("missioncache.md", mission)

write(
    "CHANGELOG.md",
    """# Changelog

## 2.5.2

- Restored the compact SandHybrid Fix28 Beehive model as the single generated/buildable prefab and renamed material ID 31 to Beehive without aliases.
- Renamed save ID 48 to Iron Ore, retained loose-cell gravity, and enabled structural tile placement alongside refined Iron.
- Replaced hashed scene-image colors with unique paint-friendly RGB values close to rendered cells; Save, Load, and both material keys now share one palette.
- Reorganized the mission cache around explicit P0/P1/P2 priorities and removed embedded release-history bulk.
- Consolidated release history into this file and removed obsolete versioned release-note files/workflows.

## 2.5.1

- Added the common upper-center authored scene, three subterranean geology zones, and stone-wrapped two-brick bottom lava band to generated and loaded scenes.

## 2.5.0

- Established the reusable library architecture and initiated the sparse 64x64 section-grid production rewrite with dirty rectangles, safe phases, halo wakeups, sleeping, and 512x512 stream-page coordinates.
""",
)
for obsolete in ("RELEASE_NOTES_v2.5.0.md", "RELEASE_NOTES_v2.5.1.md"):
    (ROOT / obsolete).unlink(missing_ok=True)

# Permanent release workflow receives the new version and the one changelog.
old_workflow = ROOT / ".github/workflows/v250-ci.yml"
workflow = old_workflow.read_text(encoding="utf-8")
workflow = workflow.replace("SandHybrid v2.5.1 CI and Release", "SandHybrid v2.5.2 CI and Release")
workflow = workflow.replace("sandhybrid-v251", "sandhybrid-v252")
workflow = workflow.replace("v2.5.1", "v2.5.2")
workflow = workflow.replace("RELEASE_NOTES_v2.5.2.md", "CHANGELOG.md")
workflow = workflow.replace(
    "run: git push origin --delete agent/scene-geology-stack-v251 || true",
    "run: |\n          git push origin --delete agent/beehive-primary-cache-release-v252 || true\n          git push origin --delete agent/scene-geology-stack-v251 || true",
)
write(".github/workflows/ci-release.yml", workflow)
old_workflow.unlink()

# Update package/docs wording that still calls the material a nest after the
# canonical token pass, and assert no versioned release note debris remains.
for path in (ROOT / "README.md", ROOT / "LIBRARY.md", ROOT / "REWRITE_PLAN.md"):
    if path.is_file():
        text = path.read_text(encoding="utf-8")
        text = text.replace("bee nests", "Beehives").replace("Bee nests", "Beehives")
        text = text.replace("queen/nest", "queen/Beehive").replace("nest returns", "Beehive returns")
        path.write_text(text, encoding="utf-8")

# Self-delete. The temporary workflow removes itself after validation/commit.
Path(__file__).unlink(missing_ok=True)
