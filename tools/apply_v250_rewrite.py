from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    if old in text:
        file.write_text(text.replace(old, new, 1), encoding="utf-8")
    elif new not in text:
        raise SystemExit(f"{path}: expected source block not found")


replace_once(
    "CMakeLists.txt",
    "project(SandHybrid VERSION 2.4.9 LANGUAGES CXX)",
    "project(SandHybrid VERSION 2.5.0 LANGUAGES CXX)",
)
replace_once(
    "CMakeLists.txt",
    """add_library(SandHybrid STATIC
    src/scene_image.cpp
    src/section_scheduler.cpp
)""",
    """add_library(SandHybrid STATIC
    src/scene_image.cpp
    src/section_grid.cpp
    src/section_scheduler.cpp
)""",
)
replace_once(
    "CMakeLists.txt",
    """    include/sandhybrid/scene_image.hpp
    include/sandhybrid/section_scheduler.hpp
    include/sandhybrid/simulation_policy.hpp""",
    """    include/sandhybrid/scene_image.hpp
    include/sandhybrid/section_grid.hpp
    include/sandhybrid/section_scheduler.hpp
    include/sandhybrid/simulation_policy.hpp""",
)
replace_once(
    "CMakeLists.txt",
    """    add_executable(sandhybrid_section_scheduler_contract tests/section_scheduler_contract.cpp)
    target_link_libraries(sandhybrid_section_scheduler_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_scheduler_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_scheduler_contract)
    add_test(NAME sandhybrid_section_scheduler_contract COMMAND sandhybrid_section_scheduler_contract)

    add_executable(sandhybrid_public_api_contract tests/public_api_contract.cpp)""",
    """    add_executable(sandhybrid_section_scheduler_contract tests/section_scheduler_contract.cpp)
    target_link_libraries(sandhybrid_section_scheduler_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_scheduler_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_scheduler_contract)
    add_test(NAME sandhybrid_section_scheduler_contract COMMAND sandhybrid_section_scheduler_contract)

    add_executable(sandhybrid_section_grid_contract tests/section_grid_contract.cpp)
    target_link_libraries(sandhybrid_section_grid_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_grid_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_grid_contract)
    add_test(NAME sandhybrid_section_grid_contract COMMAND sandhybrid_section_grid_contract)

    add_executable(sandhybrid_public_api_contract tests/public_api_contract.cpp)""",
)

replace_once(
    "include/sandhybrid/library.hpp",
    """#include \"sandhybrid/scene_image.hpp\"
#include \"sandhybrid/section_scheduler.hpp\"""",
    """#include \"sandhybrid/scene_image.hpp\"
#include \"sandhybrid/section_grid.hpp\"
#include \"sandhybrid/section_scheduler.hpp\"""",
)
replace_once(
    "include/sandhybrid/library.hpp",
    "inline constexpr std::uint32_t library_api_version = 1u;",
    "inline constexpr std::uint32_t library_api_version = 2u;",
)

mission_path = Path("missioncache.md")
mission = mission_path.read_text(encoding="utf-8")
rewrite_section = """## Production rewrite and sparse world migration

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-101 | PARTIAL | Execute the production rewrite program | `REWRITE_PLAN.md` is the authoritative staged migration plan. Each stage lands behind deterministic contracts, preserves the old runtime until parity passes, updates this ledger in the same commit, and removes compatibility code only after Windows/Linux runtime acceptance. R1 is complete and R2 is initiated. |
| MC-102 | PARTIAL | Sparse 64x64 section metadata | The platform-neutral core owns signed sparse section coordinates, per-section dirty rectangles, four non-touching phases, boundary-halo wakeups, automatic sleep, and clean metadata retirement. Deterministic contracts pass. Runtime dispatch must still prove that clean sections execute zero material work. |
| MC-103 | OPEN | Canonical paged cell storage | Replace duplicated hierarchy authority with one append-only, save-versioned cell store. Hot fields use structure-of-arrays storage; half-water, moisture, atmosphere components, damage, temperature, and material identity survive page load/save exactly. |
| MC-104 | OPEN | Transactional liquid and gas packet acceleration | Eligible 8x8 liquid/gas packets are derived from canonical cells, validate into scratch state, and atomically commit or run fine fallback in the same tick. No ownership ping-pong, volume loss, hidden fill, or one-frame classification loop is permitted. |
| MC-105 | PARTIAL | Structural solids are metadata only | Stone, iron ore, refined iron, and other block-capable solids use 8x8 support/cohesion/sleep metadata without whole-tile translation. At 31 destroyed cells of 64, the remaining 33 release individually. Runtime fracture and persistence acceptance remain required. |
| MC-106 | OPEN | Canonical packed atmosphere | Atmosphere cells conserve N2/O2/Ar/CO2/H2/He/vapor/contaminants, pressure, and temperature. Painting individual gases modifies composition instead of replacing air, and closed-box tests prove conservation. |
| MC-107 | OPEN | Component actors and directional machinery | Player, insects, conveyors, sluices, smelters, assemblers, vents, and habitats use occupancy/components outside material identity. Machines expose configurable ports, consume explicit inputs, produce explicit outputs, and report accepted/rejected transactions. |
| MC-108 | OPEN | Sparse 512x512 stream pages | Eight-by-eight section pages allocate on demand, load before cross-page transfer, save modified distant pages asynchronously, and evict clean pages. Large mostly-static worlds avoid fixed full-world allocation and scanning. |
| MC-109 | OPEN | Section-driven Vulkan runtime | Vulkan consumes immutable section batches and dirty rectangles from the core, dispatches only active work, retains a CPU reference path, and reports separate simulation/debug/presentation timings. Shader code does not own world policy. |
| MC-110 | OPEN | Deterministic cutover and old-hardware gate | Old and replacement runtimes run identical seeded scenes and compare material totals, gas components, moisture, heat, damage, actors, and machine outputs. One-million-active-cell and mostly-static-large-world baselines pass on Windows/Linux and representative older four-core hardware before old hierarchy code is deleted. |

"""
if "| MC-101 |" not in mission:
    marker = "# Active missions\n\n"
    if marker not in mission:
        raise SystemExit("missioncache.md: active mission marker not found")
    mission = mission.replace(marker, marker + rewrite_section, 1)

old_mc011 = "| MC-011 | PARTIAL | 64x64 section-first rejection | Clean inactive sections skip tile and fine-cell work while preserving safe pressure and boundary halos; runtime profiling proves it. |"
new_mc011 = "| MC-011 | PARTIAL | 64x64 section-first rejection | `SparseSectionGrid` now provides dirty rectangles, safe phases, halo wakeups, and sleeping metadata. The production runtime must consume those batches so clean sections execute zero tile/fine/material work while pressure and boundary exchange remain safe. Runtime profiling proves it. |"
if old_mc011 in mission:
    mission = mission.replace(old_mc011, new_mc011, 1)
elif new_mc011 not in mission:
    raise SystemExit("missioncache.md: MC-011 contract not found")
mission_path.write_text(mission, encoding="utf-8")

readme = Path("README.md")
readme_text = readme.read_text(encoding="utf-8")
rewrite_note = """## Production rewrite

The reusable library now contains the first production rewrite primitive: a sparse 64x64 section grid with dirty rectangles, safe non-touching phases, halo wakeups, sleeping, and 512x512 streaming-page coordinates. See `REWRITE_PLAN.md`. The existing Vulkan runtime remains available during staged parity migration.

"""
if "## Production rewrite" not in readme_text:
    insertion = readme_text.find("\n## ")
    if insertion == -1:
        readme_text += "\n\n" + rewrite_note
    else:
        readme_text = readme_text[: insertion + 1] + rewrite_note + readme_text[insertion + 1 :]
    readme.write_text(readme_text, encoding="utf-8")

Path("tools/apply_v250_rewrite.py").unlink(missing_ok=True)
