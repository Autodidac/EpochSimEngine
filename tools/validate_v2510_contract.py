from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def require(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"{path}: missing {needle!r}")


require("include/sandhybrid/world_layout.hpp", "outside_authored_scene")
require("include/sandhybrid/world_layout.hpp", "resident_transition_material")
require("include/sandhybrid/terrain_generation.hpp", "neighboring_vein_material")
require("include/sandhybrid/terrain_generation.hpp", "rubble_pocket_selected")
require("shaders/reset.comp", "int authoredWidth = min(int(pc.width), AUTHORED_WORLD_CELLS.x)")
require("shaders/reset.comp", "Open crater with a retained lava lake")
require("shaders/move.comp", "liquidSpreadReach")
require("shaders/move.comp", "Interior cells therefore settle")
require("shaders/macro_move.comp", "MAT_HONEY) return (randomValue & 1u) == 0u")
require("shaders/tiles.glsl", "TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u")
require("shaders/tiles.glsl", "TILE_FRACTURE_ARMED")
require("shaders/tiles.comp", "thresholdCollapse = fractureArmed")
require("shaders/move.comp", "releaseCollapsingStructural")
require("shaders/fullscreen.frag", "bool tinyDash")
require("src/app.cpp", "designer_workspace &&")
require("src/vulkan_renderer.cpp", "std::array<std::int32_t, 15> phases")
if "source.age < 18u" in (ROOT / "shaders/move.comp").read_text(encoding="utf-8"):
    raise SystemExit("legacy liquid age friction remains")
if "int(pc.width) * 2 / 3" in (ROOT / "shaders/reset.comp").read_text(encoding="utf-8"):
    raise SystemExit("volcano lake still uses resident-world width")
if "ivec2(int(rowLeft), 62), 1, 170u" in (ROOT / "shaders/fullscreen.frag").read_text(encoding="utf-8"):
    raise SystemExit("overlapping sidebar captions remain")
