#!/usr/bin/env python3
"""Run the legacy shader contract suite while enforcing v2.5.10 replacements.

The legacy validator still names eight v2.5.8/v2.5.9 implementation strings that
v2.5.10 deliberately replaced. This wrapper permits exactly those stale failures,
rejects every other legacy failure, and then validates the replacement contracts.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"

EXPECTED_STALE_ERRORS = {
    "v2.5.8 movement equilibrium contract missing 'source.material != MAT_HONEY && source.material != MAT_OIL'",
    "context-sensitive tool contract missing 'state.shotTimer = plasma ? 14u : 7u'",
    "shader terrain-generation contract missing 'terrainTrapResourceCell'",
    "library terrain-generation contract missing 'trap_resource_cell'",
    "movement shader reintroduced driver-expensive loops",
    "durable structural retention contract missing 'bool collapsing = structuralTile && !durableStructuralTile'",
    "resident ground deposit contract missing 'terrainTrapResourceCell'",
    "CPU resident ground deposit contract missing 'trap_resource_cell'",
}


def require(text: str, token: str, errors: list[str], contract: str) -> None:
    if token not in text:
        errors.append(f"{contract} missing {token!r}")


def main() -> int:
    legacy = subprocess.run(
        [sys.executable, str(ROOT / "tools/validate_shader_contracts.py")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    legacy_errors = {
        line[4:]
        for line in legacy.stderr.splitlines()
        if line.startswith("  - ")
    }
    if legacy.returncode == 0:
        legacy_errors = set()
    elif legacy_errors != EXPECTED_STALE_ERRORS:
        unexpected = sorted(legacy_errors - EXPECTED_STALE_ERRORS)
        missing = sorted(EXPECTED_STALE_ERRORS - legacy_errors)
        print(legacy.stdout, end="", file=sys.stderr)
        print(legacy.stderr, end="", file=sys.stderr)
        if unexpected:
            print("Unexpected legacy shader-contract failures:", file=sys.stderr)
            for error in unexpected:
                print(f"  - {error}", file=sys.stderr)
        if missing:
            print("Expected stale failures changed; update the migration explicitly:", file=sys.stderr)
            for error in missing:
                print(f"  - {error}", file=sys.stderr)
        return 1

    move = (SHADERS / "move.comp").read_text(encoding="utf-8")
    actor = (SHADERS / "actor.comp").read_text(encoding="utf-8")
    tiles = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    tile_defs = (SHADERS / "tiles.glsl").read_text(encoding="utf-8")
    terrain_glsl = (SHADERS / "terrain_generation.glsl").read_text(encoding="utf-8")
    terrain_hpp = (ROOT / "include/sandhybrid/terrain_generation.hpp").read_text(encoding="utf-8")
    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")
    renderer = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")

    errors: list[str] = []
    for token in (
        "int liquidSpreadReach(uint material)",
        "bool liquidPathLeadsToDrop",
        "for (int offset = 1; offset <= 8; ++offset)",
        "Interior cells therefore settle",
        "releaseCollapsingStructural",
        "tileHas(targetTile, TILE_COLLAPSING)",
    ):
        require(move, token, errors, "v2.5.10 liquid/fracture movement contract")
    if "source.age < 18u" in move:
        errors.append("legacy 18-frame liquid surface cutoff remains")

    require(actor, "state.shotTimer = plasma ? 6u : 4u", errors,
            "compact tool-burst contract")
    require(fullscreen, "bool tinyDash", errors, "sparse tool-render contract")

    for token in (
        "terrainTrapResource",
        "terrainNeighboringVeinMaterial",
        "uint coreDepth",
        "uint coreDeposit",
        "TERRAIN_FLAG_DELIBERATE_LOOSE",
    ):
        require(terrain_glsl, token, errors, "GLSL coherent terrain contract")
    for token in (
        "trap_resource",
        "neighboring_vein_material",
        "const auto core_depth",
        "const auto core_deposit",
        "deliberate_loose",
    ):
        require(terrain_hpp, token, errors, "C++ coherent terrain contract")

    for token in (
        "TILE_FRACTURE_ARMED",
        "TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u",
    ):
        require(tile_defs, token, errors, "31-cell fracture definition contract")
    for token in (
        "bool thresholdCollapse = fractureArmed && structural < TILE_MIN_COHESIVE_CELLS;",
        "bool collapsing = thresholdCollapse || unsupportedStructural;",
        "if (fractureArmed) flags |= TILE_FRACTURE_ARMED;",
    ):
        require(tiles, token, errors, "fracture-armed tile contract")

    require(renderer, "std::array<std::int32_t, 15> phases", errors,
            "six-pass liquid equalization contract")

    if errors:
        print("v2.5.10 shader contract validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    if legacy_errors:
        print(
            "Legacy shader contracts passed except for the eight explicitly replaced "
            "v2.5.10 implementation strings."
        )
    print("v2.5.10 liquid, terrain, fracture, workspace, and tool contracts valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
