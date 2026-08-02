from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    if old in text:
        file.write_text(text.replace(old, new, 1), encoding="utf-8")
    elif new not in text:
        raise SystemExit(f"{path}: expected source block not found")


replace_once(
    "include/sandhybrid/simulation_policy.hpp",
    """inline constexpr std::uint32_t stability_occupancy = 52u;
inline constexpr std::uint32_t collapse_occupancy = tile_cells / 2u;""",
    """inline constexpr std::uint32_t stability_occupancy = 52u;
inline constexpr std::uint32_t destroyed_cells_to_crumble = 31u;
inline constexpr std::uint32_t collapse_occupancy =
    tile_cells - destroyed_cells_to_crumble + 1u;""",
)
replace_once(
    "include/sandhybrid/simulation_policy.hpp",
    """[[nodiscard]] constexpr bool bulk_region_eligible(
    const std::uint32_t represented_cells,
    const bool uniform_material,
    const bool structural,
    const bool reacting) noexcept {
    return represented_cells == macro_tile_cells && uniform_material &&
 !structural && !reacting;
}""",
    """[[nodiscard]] constexpr bool bulk_region_eligible(
    const std::uint32_t represented_cells,
    const bool uniform_material,
    const bool structural,
    const bool reacting,
    const bool block_capable = false) noexcept {
    return represented_cells == macro_tile_cells && uniform_material &&
           !structural && !reacting && !block_capable;
}""",
)

replace_once(
    "shaders/materials.glsl",
    """const uint STRUCTURAL_BLOCK_SIZE = 8u;
const uint STRUCTURAL_FULL_CELLS = STRUCTURAL_BLOCK_SIZE * STRUCTURAL_BLOCK_SIZE;
const uint STRUCTURAL_COLLAPSE_CELLS = STRUCTURAL_FULL_CELLS / 2u;""",
    """const uint STRUCTURAL_BLOCK_SIZE = 8u;
const uint STRUCTURAL_FULL_CELLS = STRUCTURAL_BLOCK_SIZE * STRUCTURAL_BLOCK_SIZE;
const uint STRUCTURAL_DESTROYED_CELLS_TO_CRUMBLE = 31u;
const uint STRUCTURAL_COLLAPSE_CELLS =
    STRUCTURAL_FULL_CELLS - STRUCTURAL_DESTROYED_CELLS_TO_CRUMBLE + 1u;""",
)
replace_once(
    "shaders/tiles.comp",
    """    // Authored and previously qualified structures retain support until the
    // established >50% represented-cell collapse rule is crossed. This prevents""",
    """    // Authored and previously qualified structures retain support until the
    // 31-destroyed-cell (48.4375%) collapse threshold is crossed. This prevents""",
)

replace_once(
    "tests/behavior_contract.cpp",
    """    constexpr std::uint32_t initial_mass = 64u;
    constexpr std::uint32_t detached_pixels = 33u;
    constexpr std::uint32_t remaining_pixels = initial_mass - detached_pixels;""",
    """    constexpr std::uint32_t initial_mass = 64u;
    constexpr std::uint32_t detached_pixels = 31u;
    constexpr std::uint32_t remaining_pixels = initial_mass - detached_pixels;""",
)
replace_once(
    "tests/behavior_contract.cpp",
    """static_assert(!sandhybrid::policy::should_collapse(32u));
static_assert(sandhybrid::policy::should_collapse(31u));""",
    """static_assert(!sandhybrid::policy::should_collapse(34u));
static_assert(sandhybrid::policy::should_collapse(33u));""",
)
replace_once(
    "tests/material_contract.cpp",
    """static_assert(!sandhybrid::policy::should_collapse(32u));
static_assert(sandhybrid::policy::should_collapse(31u));""",
    """static_assert(!sandhybrid::policy::should_collapse(34u));
static_assert(sandhybrid::policy::should_collapse(33u));""",
)
replace_once(
    "tests/material_contract.cpp",
    """static_assert(sandhybrid::policy::bulk_region_eligible(64u, true, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(63u, true, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(64u, false, false, false));""",
    """static_assert(sandhybrid::policy::bulk_region_eligible(64u, true, false, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(64u, true, false, false, true));
static_assert(!sandhybrid::policy::bulk_region_eligible(63u, true, false, false, false));
static_assert(!sandhybrid::policy::bulk_region_eligible(64u, false, false, false, false));""",
)

obsolete = Path("tools/apply_terrain_stability_fix.py")
if obsolete.exists():
    obsolete.unlink()

validator = Path("tools/validate_shader_contracts.py")
text = validator.read_text(encoding="utf-8")
anchor = '''    if "TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)" not in fullscreen:
        errors.append("bulk-ready debug state is not decoupled from macro movement")
'''
contract = '''    simulation_policy = (ROOT / "include/sandhybrid/simulation_policy.hpp").read_text(encoding="utf-8")
    for token in (
        "destroyed_cells_to_crumble = 31u",
        "tile_cells - destroyed_cells_to_crumble + 1u",
        "const bool block_capable = false",
        "!structural && !reacting && !block_capable",
    ):
        if token not in simulation_policy:
            errors.append(f"C++ solid tile policy contract missing {token!r}")
    if (ROOT / "tools/apply_terrain_stability_fix.py").exists():
        errors.append("obsolete terrain rewrite tool can restore pre-v2.4.9 tile behavior")
'''
if contract not in text:
    if anchor not in text:
        raise SystemExit("solid policy validator insertion anchor not found")
    validator.write_text(text.replace(anchor, anchor + contract, 1), encoding="utf-8")

Path("tools/apply_v249_solid_contracts.py").unlink(missing_ok=True)
