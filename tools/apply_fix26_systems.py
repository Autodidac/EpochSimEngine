#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path): return (ROOT / path).read_text(encoding="utf-8")
def write(path, text): (ROOT / path).write_text(text, encoding="utf-8", newline="\n")
def one(text, old, new, label):
    count = text.count(old)
    if count != 1: raise RuntimeError(f"{label}: expected one replacement, found {count}")
    return text.replace(old, new, 1)
def rx(text, pattern, new, label):
    out, count = re.subn(pattern, new, text, count=1, flags=re.S)
    if count != 1: raise RuntimeError(f"{label}: expected one regex replacement, found {count}")
    return out

# Controls: Space jumps; P pauses; W remains vertical/climb input.
win = read("src/window_win32.cpp")
win = one(win, "            case VK_SPACE:\n                self->toggle_pause = true;\n                return 0;\n            case 'N':\n", "            case VK_SPACE:\n                self->jump = true;\n                return 0;\n            case 'P':\n                self->toggle_pause = true;\n                return 0;\n            case 'N':\n", "win32 controls")
win = one(win, "            case 'W': self->move_up = true; self->jump = true; return 0;\n", "            case 'W': self->move_up = true; return 0;\n", "win32 W")
win = one(win, "            case 'W': self->move_up = false; self->jump = false; return 0;\n", "            case VK_SPACE: self->jump = false; return 0;\n            case 'W': self->move_up = false; return 0;\n", "win32 release")
win = one(win, "    impl_->move_up = key_down('W') || key_down(VK_UP);\n    impl_->move_down = key_down('S') || key_down(VK_DOWN);\n    impl_->jump = impl_->move_up;\n", "    impl_->move_up = key_down('W') || key_down(VK_UP);\n    impl_->move_down = key_down('S') || key_down(VK_DOWN);\n    impl_->jump = key_down(VK_SPACE);\n", "win32 polling")
write("src/window_win32.cpp", win)

xcb = read("src/window_xcb.cpp")
xcb = one(xcb, "constexpr std::uint32_t keysym_space = 0x0020u;\nconstexpr std::uint32_t keysym_n = 0x006Eu;\n", "constexpr std::uint32_t keysym_space = 0x0020u;\nconstexpr std::uint32_t keysym_p = 0x0070u;\nconstexpr std::uint32_t keysym_upper_p = 0x0050u;\nconstexpr std::uint32_t keysym_n = 0x006Eu;\n", "xcb P")
xcb = one(xcb, "            } else if (keysym == keysym_space) {\n                impl_->toggle_pause = true;\n            } else if (keysym == keysym_n || keysym == keysym_upper_n) {\n", "            } else if (keysym == keysym_space) {\n                impl_->jump = true;\n            } else if (keysym == keysym_p || keysym == keysym_upper_p) {\n                impl_->toggle_pause = true;\n            } else if (keysym == keysym_n || keysym == keysym_upper_n) {\n", "xcb controls")
xcb = one(xcb, "            } else if (keysym == keysym_w || keysym == keysym_upper_w) {\n                impl_->move_up = true;\n                impl_->jump = true;\n", "            } else if (keysym == keysym_w || keysym == keysym_upper_w) {\n                impl_->move_up = true;\n", "xcb W")
xcb = one(xcb, "            else if (keysym == keysym_w || keysym == keysym_upper_w) { impl_->move_up = false; impl_->jump = false; }\n", "            else if (keysym == keysym_space) impl_->jump = false;\n            else if (keysym == keysym_w || keysym == keysym_upper_w) impl_->move_up = false;\n", "xcb release")
write("src/window_xcb.cpp", xcb)

# BUILD/MINE is real in every scene; movement remains active in both modes.
app = read("src/app.cpp")
app = one(app, "        const bool paint_active = over_simulation && !character_scene && !mining && !inspecting;\n", "        const bool paint_active = over_simulation && !mining && !inspecting;\n", "build mode")
app = one(app, "        // Character scenes always retain mining/shooting. The mode toggle cannot\n        // accidentally route player clicks back into world painting.\n        const bool tool_active = over_simulation && (character_scene || mining) && !inspecting;\n", "        // MINE uses the player tool; BUILD paints or erases the selected material.\n        // Character movement remains active in either mode.\n        const bool tool_active = over_simulation && mining && !inspecting;\n", "tool mode")
write("src/app.cpp", app)

# Compact sidebar layout.
write("include/epoch/sand/ui_layout.hpp", r'''#pragma once
#include "epoch/sand/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>
namespace epoch::sand::ui {
inline constexpr std::uint32_t preferred_sidebar_width = 384u;
inline constexpr std::uint32_t minimum_sidebar_width = 300u;
inline constexpr std::uint32_t status_height = 126u;
inline constexpr std::uint32_t group_tabs_height = 112u;
inline constexpr std::uint32_t palette_items_height = 136u;
inline constexpr std::uint32_t palette_height = 0u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;
struct Layout final { epochengine::gui_lib::Rect status{}, simulation{}, group_tabs{}, palette{}, previous_scene{}, next_scene{}, reset_scene{}, mode_toggle{}, debug_toggle{}, material_card{}; };
struct SimulationViewport final { epochengine::gui_lib::Rect rect{}; std::uint32_t tile_pixel_size{}; };
[[nodiscard]] inline SimulationViewport make_simulation_viewport(const Layout& layout, std::uint32_t grid_width, std::uint32_t grid_height) noexcept {
 constexpr std::uint32_t cells_per_tile=8u; auto tc=(std::max)(1u,(grid_width+7u)/8u); auto tr=(std::max)(1u,(grid_height+7u)/8u); auto pw=(std::max)(1u,(std::uint32_t)layout.simulation.size.x); auto ph=(std::max)(1u,(std::uint32_t)layout.simulation.size.y); if(pw<tc||ph<tr) return {layout.simulation,0u}; auto tp=(std::max)(1u,(std::min)(pw/tc,ph/tr)); auto vw=tc*tp, vh=tr*tp; auto l=layout.simulation.position.x+float((pw-vw)/2u); auto t=layout.simulation.position.y+float((ph-vh)/2u); return {{{l,t},{float(vw),float(vh)}},tp}; }
[[nodiscard]] inline Layout make_layout(std::uint32_t width,std::uint32_t height) noexcept {
 auto sw=(std::max)(width,1u), sh=(std::max)(height,1u); auto requested=(std::max)(minimum_sidebar_width,sw/3u); auto side=sw>minimum_sidebar_width+160u?(std::min)(preferred_sidebar_width,requested):(std::min)(sw,minimum_sidebar_width); auto simw=sw>side?sw-side:1u; float left=float(simw), sidef=float(sw-simw);
 Layout l{.status={{left,0},{sidef,float(status_height)}},.simulation={{0,0},{float(simw),float(sh)}},.group_tabs={{left+margin,float(status_height)+margin},{(std::max)(1.0f,sidef-margin*2),float(group_tabs_height)}},.palette={{left+margin,float(status_height+group_tabs_height)+margin+gap},{(std::max)(1.0f,sidef-margin*2),float(palette_items_height)}}};
 float ct=l.palette.position.y+l.palette.size.y+gap; l.material_card={{left+margin,ct},{(std::max)(1.0f,sidef-margin*2),(std::max)(1.0f,float(sh)-ct-margin)}}; l.previous_scene={{left+8,70},{58,26}}; l.next_scene={{left+70,70},{58,26}}; l.reset_scene={{left+132,70},{80,26}}; l.mode_toggle={{left+8,100},{(std::max)(112.0f,sidef*0.46f),22}}; l.debug_toggle={{l.mode_toggle.position.x+l.mode_toggle.size.x+4,100},{(std::max)(1.0f,sidef-l.mode_toggle.size.x-24),22}}; return l; }
[[nodiscard]] inline epochengine::gui_lib::Rect group_tab_rect(const Layout& l,std::uint32_t i) noexcept { constexpr std::uint32_t c=2; auto rows=(material_group_count+c-1)/c; auto col=i%c,row=i/c; float cw=l.group_tabs.size.x/float(c),ch=l.group_tabs.size.y/float((std::max)(rows,1u)); return {{l.group_tabs.position.x+float(col)*cw+gap*.5f,l.group_tabs.position.y+float(row)*ch+gap*.5f},{(std::max)(1.0f,cw-gap),(std::max)(1.0f,ch-gap)}}; }
[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(const Layout& l,MaterialGroup g,std::uint32_t i) noexcept { constexpr std::uint32_t c=2; auto n=(std::max)(material_group_size(g),1u),rows=(n+c-1)/c,col=i%c,row=i/c; float cw=l.palette.size.x/float(c),ch=l.palette.size.y/float((std::max)(rows,1u)); return {{l.palette.position.x+float(col)*cw+gap*.5f,l.palette.position.y+float(row)*ch+gap*.5f},{(std::max)(1.0f,cw-gap),(std::max)(1.0f,ch-gap)}}; }
[[nodiscard]] inline std::uint32_t group_at(const Layout& l,epochengine::gui_lib::Vec2 p) noexcept { for(std::uint32_t i=0;i<material_group_count;++i) if(epochengine::gui_lib::contains(group_tab_rect(l,i),p)) return i; return material_group_count; }
[[nodiscard]] inline std::uint32_t palette_slot_at(const Layout& l,MaterialGroup g,epochengine::gui_lib::Vec2 p) noexcept { auto n=material_group_size(g); for(std::uint32_t i=0;i<n;++i) if(epochengine::gui_lib::contains(palette_item_rect(l,g,i),p)) return i; return n; }
[[nodiscard]] inline Material palette_material_at(const Layout& l,MaterialGroup g,epochengine::gui_lib::Vec2 p) noexcept { auto s=palette_slot_at(l,g,p); return s<material_group_size(g)?grouped_material(g,s):Material::count; }
} // namespace epoch::sand::ui
''')

renderer=read("src/vulkan_renderer.cpp")
renderer=one(renderer,"            .palette_height = swapchain_extent.height -\n                static_cast<std::uint32_t>(layout.status.size.y + layout.simulation.size.y),\n","            // Existing push slot carries compact sidebar width.\n            .palette_height = static_cast<std::uint32_t>(layout.status.size.x),\n","renderer sidebar")
renderer=one(renderer,"        const std::array<std::int32_t, 7> phases = (simulation_step & 1u) == 0u\n            ? std::array<std::int32_t, 7>{0, 1, 2, 3, 4, 5, 5}\n            : std::array<std::int32_t, 7>{0, 2, 1, 4, 3, 5, 5};\n","        const std::array<std::int32_t, 9> phases = (simulation_step & 1u) == 0u\n            ? std::array<std::int32_t, 9>{0, 1, 2, 3, 4, 5, 5, 5, 5}\n            : std::array<std::int32_t, 9>{0, 2, 1, 4, 3, 5, 5, 5, 5};\n","liquid phases")
renderer=one(renderer,"                    phase == 5 && phase_index == phases.size() - 1u\n                        ? ((simulation_step + 1u) & 1u)\n                        : ((simulation_step + static_cast<std::uint32_t>(phase)) & 1u)),\n","                    phase == 5\n                        ? ((simulation_step + static_cast<std::uint32_t>(phase_index)) & 1u)\n                        : ((simulation_step + static_cast<std::uint32_t>(phase)) & 1u)),\n","liquid parity")
write("src/vulkan_renderer.cpp",renderer)

gen=read("tools/generate_ui_text.py")
gen=one(gen,'    "HP", "O2", "AMMO", "GOLD", "IRON", "LMB TOOL", "RMB DROP", "AL", "CU", "DRILL", "RANGE", "JUMP", "PLASMA", "LOCKED", "READY",\n','    "HP", "O2", "AMMO", "GOLD", "IRON", "LMB TOOL", "RMB DROP", "AL", "CU", "DRILL", "RANGE", "JUMP", "PLASMA", "LOCKED", "READY",\n    "SPACE JUMP", "P PAUSE", "LMB USE", "RMB DROP ERASE", "ALT CELL CARD",\n',"UI hints")
write("tools/generate_ui_text.py",gen)

# Movement fixes.
move=read("shaders/move.comp")
move=one(move,"bool supportedAt(ivec2 position, Cell cell) {\n    return !canCellFallInto(cell, sampleAt(position + ivec2(0, 1)));\n}\n\nint liquidColumnPressure(ivec2 position, uint material) {\n    int pressure = 0;\n    Cell c1 = sampleAt(position + ivec2(0, -1));\n","bool supportedAt(ivec2 position, Cell cell) {\n    return !canCellFallInto(cell, moveAt(position + ivec2(0, 1)));\n}\n\nint liquidColumnPressure(ivec2 position, uint material) {\n    int pressure = 0;\n    Cell c1 = moveAt(position + ivec2(0, -1));\n","current support")
for n in (2,3,4): move=one(move,f"Cell c{n} = sampleAt(position + ivec2(0, -{n}));",f"Cell c{n} = moveAt(position + ivec2(0, -{n}));",f"pressure {n}")
move=rx(move,r"bool liquidCanSpread\(ivec2 sourcePosition, ivec2 targetPosition, Cell source, uint randomValue\) \{.*?\n\}",'''bool liquidCanSpread(ivec2 sourcePosition, ivec2 targetPosition, Cell source, uint randomValue) {
    if (moveAt(targetPosition).material != MAT_EMPTY || !movementAllows(source, randomValue)) return false;
    if (!supportedAt(sourcePosition, source)) return false;
    if (!supportedAt(targetPosition, source)) return true;
    bool sourceCovered = isCellLiquid(moveAt(sourcePosition + ivec2(0, -1)));
    bool targetCovered = isCellLiquid(moveAt(targetPosition + ivec2(0, -1)));
    if (sourceCovered != targetCovered) return sourceCovered;
    int sourcePressure = liquidColumnPressure(sourcePosition, source.material);
    int targetPressure = liquidColumnPressure(targetPosition, source.material);
    if (sourcePressure != targetPressure) return sourcePressure > targetPressure;
    int direction = targetPosition.x > sourcePosition.x ? 1 : -1;
    ivec2 next = targetPosition + ivec2(direction, 0);
    ivec2 next2 = targetPosition + ivec2(direction * 2, 0);
    bool firstDrop = moveInside(next) && moveAt(next).material == MAT_EMPTY && canCellFallInto(source, moveAt(next + ivec2(0, 1)));
    bool secondDrop = moveInside(next2) && moveAt(next).material == MAT_EMPTY && moveAt(next2).material == MAT_EMPTY && canCellFallInto(source, moveAt(next2 + ivec2(0, 1)));
    return firstDrop || secondDrop || (randomValue & 7u) == 0u;
}''',"liquid leveling")
move=rx(move,r"int beeWaveVertical\(Cell bee, ivec2 p\) \{.*?\n\}\n\nbool beeMoveAllowed\(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue\) \{.*?\n\}",'''int beeWaveVertical(Cell bee, ivec2 p) {
    uint phase = (movePc.step / 2u + uint(p.x) * 2u + ((bee.aux >> 8u) & 7u)) % 24u;
    if (phase < 6u) return -1; if (phase < 12u) return 0; if (phase < 18u) return 1; return 0;
}
bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard=adjacentHazard(sourcePosition), targetHazard=adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;
    ivec2 delta=targetPosition-sourcePosition;
    int hiveSignal=localTargetSignal(sourcePosition,MAT_QUEEN_BEE,MAT_BEE_NEST);
    if (stateValue(bee)>0u && hiveSignal>0) {
        int desired=beeWaveVertical(bee,sourcePosition);
        int orbit=((movePc.step/28u+((bee.aux>>11u)&1u))&1u)!=0u?1:-1;
        if(delta.y==desired&&delta.x==0) return true;
        if(delta.x==orbit&&delta.y==0) return true;
        return false;
    }
    int sourceSignal=beeTargetSignal(bee,sourcePosition), targetSignal=beeTargetSignal(bee,targetPosition);
    if(sourceSignal!=targetSignal) return targetSignal>sourceSignal;
    if(delta.y==beeWaveVertical(bee,sourcePosition)) return true;
    if(delta.x!=0&&(randomValue&1u)==0u) return true;
    return (randomValue&7u)==0u;
}''',"bee wave")
move=one(move,"bool insectMoveAllowed(Cell insect, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {\n    if ((insect.aux & AUX_MOVED) != 0u) return false;\n    int sourceSignal = insectTargetSignal(insect, sourcePosition);\n","bool insectMoveAllowed(Cell insect, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {\n    if ((insect.aux & AUX_MOVED) != 0u) return false;\n    bool sourceWet = adjacentContains(sourcePosition, MAT_WATER, MAT_SALTWATER) || adjacentContains(sourcePosition, MAT_DIRTY_WATER, MAT_ACID);\n    bool targetWet = adjacentContains(targetPosition, MAT_WATER, MAT_SALTWATER) || adjacentContains(targetPosition, MAT_DIRTY_WATER, MAT_ACID);\n    if (sourceWet != targetWet) return sourceWet;\n    int sourceSignal = insectTargetSignal(insect, sourcePosition);\n","insect avoidance")
move=one(move,"bool sleepSafe(Cell cell) {\n    if (cell.material == MAT_EMPTY || isStructural(cell)) return true;\n    if (cell.material == MAT_BEE || isInsect(cell.material) ||\n        cell.material == MAT_SEED || cell.material == MAT_POLLEN) return false;\n    if (isCellGas(cell) || isCellLiquid(cell) || isCellPowder(cell) || isLooseSolid(cell)) return false;\n    return isCellImmovable(cell);\n}\n\nbool pairSleeping(ivec2 a, ivec2 b) {\n    TileState tileA = tiles[tileIndex(a, movePc.width)];\n    TileState tileB = tiles[tileIndex(b, movePc.width)];\n    if (!tileHas(tileA, TILE_SLEEPING) || !tileHas(tileB, TILE_SLEEPING)) return false;\n    return sleepSafe(moveAt(a)) && sleepSafe(moveAt(b));\n}\n","bool sleepSafe(ivec2 position, Cell cell) {\n    if (cell.material == MAT_EMPTY || isStructural(cell)) return true;\n    if (cell.material == MAT_BEE || isInsect(cell.material) || cell.material == MAT_SEED || cell.material == MAT_POLLEN) return false;\n    TileState tile = tiles[tileIndex(position, movePc.width)];\n    bool stabilizedRegion = tileHas(tile, TILE_STABLE) && tileHas(tile, TILE_SLEEPING) && tile.material == cell.material && tile.occupancy >= TILE_STABILITY_OCCUPANCY && isReconstructableMaterial(cell.material);\n    if (stabilizedRegion) return true;\n    if (isCellGas(cell) || isCellLiquid(cell) || isCellPowder(cell) || isLooseSolid(cell)) return false;\n    return isCellImmovable(cell);\n}\n\nbool pairSleeping(ivec2 a, ivec2 b) {\n    TileState tileA = tiles[tileIndex(a, movePc.width)];\n    TileState tileB = tiles[tileIndex(b, movePc.width)];\n    if (!tileHas(tileA, TILE_SLEEPING) || !tileHas(tileB, TILE_SLEEPING)) return false;\n    return sleepSafe(a, moveAt(a)) && sleepSafe(b, moveAt(b));\n}\n","stable bricks")
write("shaders/move.comp",move)

# Chemistry/economy/factory.
chem=read("shaders/chemistry.comp")
chem=one(chem,"        if (hasNeighbor(p, MAT_INSECT_HABITAT) && (randomValue & 1023u) == 0u) {\n            result = makeCell((randomValue & 2048u) == 0u ? MAT_ANT : MAT_BEETLE);\n","        if (hasNeighbor(p, MAT_INSECT_HABITAT) && countWithin(p, MAT_ANT, 5) + countWithin(p, MAT_BEETLE, 5) < 8u && (randomValue & 2047u) == 0u) {\n            result = makeCell((randomValue & 4096u) == 0u ? MAT_ANT : MAT_BEETLE);\n","bug spawn")
chem=one(chem,"                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&\n                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&\n                            (randomValue & 255u) == 0u) {\n                    result = makeCell(MAT_BEE);\n","                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&\n                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&\n                            countWithin(p, MAT_BEE, 6) < 12u && (randomValue & 4095u) == 0u) {\n                    result = makeCell(MAT_BEE);\n","bee reproduction")
chem=rx(chem,r"    // Mud is a thin saturation layer\..*?\n    \}\n\n\n    // Waterfall aeration",'''    // Fresh water visibly wets soil before mud forms; moisture remains on the
    // dirt cell so drainage never silently deletes a water cell.
    bool freshContact = hasNeighbor(p, MAT_WATER) || hasNeighbor(p, MAT_DIRTY_WATER);
    uint supportBelow = at(p + ivec2(0, 1)).material;
    bool stableSoil = supportBelow == MAT_DIRT || supportBelow == MAT_STONE || supportBelow == MAT_MUD;
    if (source.material == MAT_DIRT) {
        if (freshContact) result.aux |= AUX_WET;
        bool saturatedSurface = (result.aux & AUX_WET) != 0u && stableSoil && neighborCount(p, MAT_MUD) <= 2u;
        if (saturatedSurface && source.age > 90u && (randomValue & 63u) == 0u) { result = makeCell(MAT_MUD); result.aux |= AUX_WET; }
        else if (!hasFreshMoistureRadius(p, 2) && (result.aux & AUX_WET) != 0u && source.age > 900u && (randomValue & 255u) == 0u) result.aux &= ~AUX_WET;
    } else if (source.material == MAT_MUD && !hasAnyWater(p) && source.age > 600u && (light[index] > 105u || result.temperature > 30) && (randomValue & 63u) == 0u) result = makeCell(MAT_DIRT);


    // Waterfall aeration''',"soil soak")
chem=one(chem,"    if (source.material == MAT_ANT) {\n        if (nearWater || nearFire || nearAcid || nearLava) result = makeCell(MAT_WASTE);\n        else if (hasNeighbor(p, MAT_WASTE) && hasNeighbor(p, MAT_DIRT) && (randomValue & 255u) == 0u)\n            result = makeCell(MAT_FERTILIZER);\n    } else if (source.material == MAT_BEETLE) {\n        if (nearFire || nearAcid || nearLava) result = makeCell(MAT_WASTE);\n        else if ((hasNeighbor(p, MAT_WASTE) || hasNeighbor(p, MAT_WOOD)) &&\n                 hasFreshMoistureRadius(p, 2) && (randomValue & 255u) == 0u)\n            result = makeCell(MAT_FERTILIZER);\n    } else if (source.material == MAT_INSECT_HABITAT) {\n","    if (source.material == MAT_ANT) {\n        if (nearFire || nearAcid || nearLava) result = makeCell(MAT_WASTE);\n    } else if (source.material == MAT_BEETLE) {\n        if (nearFire || nearAcid || nearLava) result = makeCell(MAT_WASTE);\n    } else if (source.material == MAT_WASTE && (hasNeighbor(p, MAT_ANT) || hasNeighbor(p, MAT_BEETLE)) && (hasNeighbor(p, MAT_DIRT) || hasNeighbor(p, MAT_MUD)) && hasFreshMoistureRadius(p, 2) && (randomValue & 255u) == 0u) {\n        result = makeCell(MAT_FERTILIZER);\n    } else if (source.material == MAT_INSECT_HABITAT) {\n","persistent bugs")
chem=one(chem,"        if (!validSupport || nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 24u) {\n            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);\n        } else {\n","        if (nearSaltwater || nearAcid || nearFire || nearLava) {\n            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);\n        } else if (!validSupport && source.age > 600u) {\n            result = makeCell(MAT_WASTE);\n        } else {\n            if (nearWater) result.aux |= AUX_WET;\n","stem resilience")
chem=one(chem,"        if (!validStem || nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 30u) {\n            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);\n        }\n","        if (nearSaltwater || nearAcid || nearFire || nearLava) {\n            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);\n        } else if (!validStem && source.age > 600u) {\n            result = makeCell(MAT_WASTE);\n        } else if (nearWater) result.aux |= AUX_WET;\n","flower resilience")
chem=one(chem,"setStateValue(result, 220u);","setStateValue(result, 480u);","bee rest")
chem=one(chem,"source.age > 2400u && (randomValue & 511u) == 0u","source.age > 3600u && (randomValue & 2047u) == 0u","honey production")
chem=one(chem,"if (hasHungryBee(p) && (randomValue & 255u) == 0u)","if (hasHungryBee(p) && (randomValue & 2047u) == 0u)","honey consumption")
chem=one(chem,"source.age > 1800u && (randomValue & 4095u) == 0u","source.age > 7200u && (randomValue & 32767u) == 0u","wax production")
chem=one(chem,"    if (machine == MAT_SMELTER) {\n        return (resourceMaterial == MAT_IRON || resourceMaterial == MAT_IRON_SHAVINGS) ? 0 : -1;\n    }\n","    if (machine == MAT_SMELTER) {\n        if (resourceMaterial == MAT_IRON_SHAVINGS) return 0;\n        if (resourceMaterial == MAT_ALUMINUM_SHAVINGS) return 1;\n        return -1;\n    }\n","smelter slots")
chem=rx(chem,r"uvec4 machineRecipe\(uint machine\) \{.*?\n\}\n\nbool machineReady\(uint machine, uvec4 inventory\) \{.*?\n\}\n\nuint machineOutput\(uint machine, bool allied\) \{.*?\n\}",'''uvec4 machineRecipe(uint machine) { if (machine == MAT_ASSEMBLER) return uvec4(2u,1u,1u,1u); return uvec4(15u); }
bool machineReady(uint machine, uvec4 inventory) { if (machine == MAT_SMELTER) return inventory.x>=4u || inventory.y>=4u; return all(greaterThanEqual(inventory,machineRecipe(machine))); }
uvec4 machineConsumption(uint machine, uvec4 inventory) { if(machine==MAT_SMELTER) return inventory.x>=4u?uvec4(4u,0u,0u,0u):uvec4(0u,4u,0u,0u); return machineRecipe(machine); }
uint machineOutput(uint machine, uvec4 inventory) { if(machine==MAT_SMELTER) return inventory.x>=4u?MAT_IRON:MAT_ALUMINUM; if(machine==MAT_ASSEMBLER) return MAT_PLASMA_AMMO; return MAT_EMPTY; }''',"factory recipes")
chem=one(chem,"                        result = makeCell(machineOutput(machine.material, machineIsAllied(controller)));\n","                        result = makeCell(machineOutput(machine.material, machineInventory(machine)));\n","factory output")
chem=one(chem,"        if (canOutput) inventory -= machineRecipe(source.material);\n","        if (canOutput) inventory -= machineConsumption(source.material, inventory);\n","factory consumption")
write("shaders/chemistry.comp",chem)

mat=read("shaders/materials.glsl")
mat=one(mat,"    case MAT_DIRT: color = vec4(0.34 + variation, 0.19, 0.08, 1.0); break;\n","    case MAT_DIRT: { float wetDarken=(aux&AUX_WET)!=0u?-0.10:0.0; color=vec4(0.34+variation+wetDarken,0.19+wetDarken*0.55,0.08+wetDarken*0.30,1.0); break; }\n","wet dirt")
write("shaders/materials.glsl",mat)

# ONI-inspired ecosystem scene.
reset=read("shaders/reset.comp")
eco=r'''uint ecosystemMaterial(ivec2 p) {
 int width=int(pc.width),height=int(pc.height),floorY=height-10; uint material=MAT_EMPTY;
 if(p.y>=floorY) return MAT_STONE; if((p.x<8||p.x>=width-8)&&p.y>=8) material=MAT_STONE;
 int groundY=floorY-54+int(hash32(uint(p.x)*31u)%5u); if(p.y>=groundY&&p.y<floorY) material=p.y==groundY?MAT_GRASS:MAT_DIRT;
 int tankLeft=22,tankRight=148,tankTop=54,tankBottom=groundY-8; bool tankWall=(p.x==tankLeft||p.x==tankRight||p.y==tankTop||p.y==tankBottom)&&p.x>=tankLeft&&p.x<=tankRight&&p.y>=tankTop&&p.y<=tankBottom; bool outlet=p.x>=tankRight-5&&p.x<=tankRight&&p.y>=tankBottom-8&&p.y<=tankBottom; if(tankWall&&!outlet) material=MAT_GLASS; if(p.x>tankLeft&&p.x<tankRight&&p.y>tankTop+12&&p.y<tankBottom) material=MAT_WATER;
 int trenchY=groundY-5; if(p.y==trenchY+4&&p.x>=tankRight&&p.x<430) material=MAT_GLASS; if(p.y>trenchY&&p.y<trenchY+4&&p.x>=tankRight&&p.x<430) material=MAT_WATER;
 for(int bed=0;bed<3;++bed){int left=178+bed*78,right=left+58;if(p.x>=left&&p.x<right&&p.y>=groundY-8&&p.y<=groundY)material=p.y==groundY-8?MAT_GRASS:MAT_DIRT;int seedX=left+14,matureX=left+38;if(p.x==seedX&&p.y==groundY-9)material=MAT_SEED;if(p.x==matureX&&p.y>=groundY-12&&p.y<=groundY-9)material=MAT_PLANT_STEM;if(p.x==matureX&&p.y==groundY-13)material=MAT_FLOWER;if(p.x==right-4&&p.y==groundY-9)material=MAT_FERTILIZER;}
 int compostLeft=250,compostRight=392,compostTop=groundY+12,compostBottom=floorY-8; bool compostWall=(p.x==compostLeft||p.x==compostRight||p.y==compostTop||p.y==compostBottom)&&p.x>=compostLeft&&p.x<=compostRight&&p.y>=compostTop&&p.y<=compostBottom;if(compostWall)material=MAT_WOOD;if(rectContains(p,ivec2(compostLeft+10,compostBottom-18),ivec2(compostLeft+34,compostBottom)))material=MAT_INSECT_HABITAT;if(p.y==compostBottom-2&&p.x>compostLeft+42&&p.x<compostRight-8){uint c=hash32(indexOf(p)^pc.seed^0xc0a9057u);material=(c&3u)==0u?MAT_FERTILIZER:MAT_WASTE;}if(p.y==compostTop-1&&p.x>compostLeft+18&&p.x<compostRight-18&&(hash32(indexOf(p)^pc.seed)%29u)==0u)material=MAT_ANT;
 ivec2 queen=ivec2(width-104,groundY-72),q=p-queen;int q2=q.x*q.x+q.y*q.y;if(q2>=28&&q2<108)material=MAT_BEE_NEST;if(q2==0)material=MAT_QUEEN_BEE;else if(q.x>=1&&q.x<=12&&abs(q.y)<=1)material=MAT_EMPTY;else if(q2<28){uint c=hash32(indexOf(p)^pc.seed^0xb33u);material=(c&3u)==0u?MAT_EMPTY:((c&4u)==0u?MAT_HONEY:MAT_POLLEN);}if(q2>130&&q2<1350&&(hash32(indexOf(p)^pc.seed^0xbee51u)%37u)==0u)material=MAT_BEE;
 for(int f=0;f<4;++f){int xx=width-210+f*28;if(p.x==xx&&p.y>=groundY-5&&p.y<=groundY-2)material=MAT_PLANT_STEM;if(p.x==xx&&p.y==groundY-6)material=MAT_FLOWER;}
 if(material==MAT_EMPTY&&p.y>groundY-62&&p.y<groundY-12&&p.x>150&&p.x<430&&(hash32(indexOf(p)^pc.seed^0x02u)%17u)==0u)material=MAT_OXYGEN;if(material==MAT_EMPTY&&p.y>=groundY-16&&p.y<groundY&&(hash32(indexOf(p)^pc.seed^0xc02u)%13u)==0u)material=MAT_CARBON_DIOXIDE;
 int gasLeft=width-188,gasRight=width-20,gasTop=18,gasBottom=72;bool gasWall=(p.x==gasLeft||p.x==gasRight||p.y==gasTop||p.y==gasBottom)&&p.x>=gasLeft&&p.x<=gasRight&&p.y>=gasTop&&p.y<=gasBottom;if(gasWall)material=MAT_GLASS;if(p.x>gasLeft&&p.x<gasRight&&p.y>gasTop&&p.y<gasBottom&&(hash32(indexOf(p)^pc.seed^0x4a2u)&1u)==0u)material=MAT_HYDROGEN;
 return material;
}
'''
reset=rx(reset,r"uint ecosystemMaterial\(ivec2 p\) \{.*?\n\}\n\nuint engineeringMaterial",eco+"\nuint engineeringMaterial","ecosystem")
write("shaders/reset.comp",reset)

# Sidebar shader and persistent large element cards.
frag=read("shaders/fullscreen.frag")
frag=one(frag,"            cell.material == MAT_PLANT_STEM || cell.material == MAT_HYDROGEN ||\n","            cell.material == MAT_HYDROGEN ||\n","stem danger")
sidebar=r'''    uint sidebarWidth=min(renderPc.paletteHeight,renderPc.windowWidth),sidebarLeft=renderPc.windowWidth-sidebarWidth;
    if(x>=sidebarLeft){
      vec3 color=vec3(0.025,0.034,0.048);uint localX=x-sidebarLeft;if(localX<2u)color=vec3(0.14,0.23,0.32);bool text=fixedPixel(pixel,ivec2(int(sidebarLeft+10u),8),2,0u);uint sceneId=renderPc.selectedScene%max(renderPc.sceneCount,1u);text=text||fixedPixel(pixel,ivec2(int(sidebarLeft+10u),31),1,5u)||scenePixel(pixel,ivec2(int(sidebarLeft+58u),27),2,sceneId)||fixedPixel(pixel,ivec2(int(sidebarLeft+10u),51),2,1u)||numberPixel(pixel,ivec2(int(sidebarLeft+58u),51),2,renderPc.framesPerSecond)||fixedPixel(pixel,ivec2(int(sidebarLeft+136u),51),1,renderPc.paused!=0u?3u:2u);
      const uint bx[5]=uint[5](8u,70u,132u,8u,8u+max(112u,sidebarWidth*46u/100u)+4u);const uint bw[5]=uint[5](58u,58u,80u,max(112u,sidebarWidth*46u/100u),max(1u,sidebarWidth-max(112u,sidebarWidth*46u/100u)-24u));for(uint i=0u;i<5u;++i){uint top=i<3u?70u:100u,h=i<3u?26u:22u,left=sidebarLeft+bx[i],right=left+bw[i];if(x>=left&&x<right&&y>=top&&y<top+h){color=i==4u&&renderPc.debugMode!=0u?vec3(0.20,0.38,0.20):vec3(0.075,0.105,0.145);if(borderPixel(x,y,left,top,right,top+h))color*=0.55;}}
      text=text||fixedPixel(pixel,ivec2(int(sidebarLeft+17u),78),1,41u)||fixedPixel(pixel,ivec2(int(sidebarLeft+79u),78),1,42u)||fixedPixel(pixel,ivec2(int(sidebarLeft+151u),78),1,6u)||fixedPixel(pixel,ivec2(int(sidebarLeft+22u),106),1,renderPc.miningMode!=0u?8u:7u)||fixedPixel(pixel,ivec2(int(sidebarLeft+bx[4]+10u),106),1,9u);
      uint contentLeft=sidebarLeft+5u,contentWidth=max(sidebarWidth-10u,1u),groupTop=renderPc.statusHeight+5u,groupRows=max((renderPc.groupCount+1u)/2u,1u),gcw=max(contentWidth/2u,1u),gch=max(renderPc.groupTabsHeight/groupRows,1u);
      if(y>=groupTop&&y<groupTop+renderPc.groupTabsHeight&&x>=contentLeft&&x<contentLeft+contentWidth){uint col=min((x-contentLeft)/gcw,1u),row=min((y-groupTop)/gch,groupRows-1u),g=row*2u+col;if(g<renderPc.groupCount){uint l=contentLeft+col*gcw,r=col==1u?contentLeft+contentWidth:l+gcw,t=groupTop+row*gch,b=min(groupTop+renderPc.groupTabsHeight,t+gch);color=g==renderPc.selectedGroup?vec3(0.14,0.30,0.45):vec3(0.04,0.052,0.07);if(g==renderPc.hoveredGroup)color+=vec3(0.055);if(borderPixel(x,y,l,t,r,b))color*=0.55;int s=int(r-l)>=int(groupTextLength(g))*12+8?2:1,w=int(groupTextLength(g))*6*s-s;if(groupPixel(pixel,ivec2(int(l+r)/2-w/2,int(t+b)/2-(7*s)/2),s,g))color=vec3(0.95);}outColor=vec4(color,1);return;}
      uint paletteTop=groupTop+renderPc.groupTabsHeight+3u;const uint ph=136u;uint sc=max(groupMaterialCount(renderPc.selectedGroup),1u),sr=max((sc+1u)/2u,1u),cw=max(contentWidth/2u,1u),ch=max(ph/sr,1u);
      if(y>=paletteTop&&y<paletteTop+ph&&x>=contentLeft&&x<contentLeft+contentWidth){uint col=min((x-contentLeft)/cw,1u),row=min((y-paletteTop)/ch,sr-1u),slot=row*2u+col;if(slot<sc){uint m=groupMaterial(renderPc.selectedGroup,slot),l=contentLeft+col*cw,r=col==1u?contentLeft+contentWidth:l+cw,t=paletteTop+row*ch,b=min(paletteTop+ph,t+ch);color=materialColor(m,0u,m*1299721u,ivec2(int(slot),int(renderPc.selectedGroup))).rgb*0.62;if(m==renderPc.selectedMaterial)color=min(color*1.10+vec3(0.13),vec3(1));if(m==renderPc.hoveredMaterial)color=min(color+vec3(0.09),vec3(1));if(borderPixel(x,y,l,t,r,b))color*=0.5;int s=int(r-l)>=int(materialTextLength(m))*12+8?2:1,w=int(materialTextLength(m))*6*s-s;if(materialPixel(pixel,ivec2(int(l+r)/2-w/2,int(t+b)/2-(7*s)/2),s,m))color=dot(color,vec3(0.299,0.587,0.114))>0.55?vec3(0.02):vec3(0.97);}outColor=vec4(color,1);return;}
      uint cardTop=paletteTop+ph+3u,actorPanel=actor.enabled!=0u?102u:5u,cardBottom=renderPc.windowHeight>actorPanel+5u?renderPc.windowHeight-actorPanel-5u:renderPc.windowHeight;ivec2 cursor=clamp(ivec2(renderPc.cursorX,renderPc.cursorY),ivec2(0),ivec2(int(renderPc.gridWidth)-1,int(renderPc.gridHeight)-1));Cell inspected=cellAt(cursor);uint cardMaterial=renderPc.inspectMode!=0u?inspected.material:(renderPc.hoveredMaterial<renderPc.materialCount?renderPc.hoveredMaterial:renderPc.selectedMaterial);cardMaterial=min(cardMaterial,renderPc.materialCount-1u);
      if(y>=cardTop&&y<cardBottom){if(borderPixel(x,y,contentLeft,cardTop,contentLeft+contentWidth,cardBottom))color=vec3(0.13,0.29,0.43);text=text||materialPixel(pixel,ivec2(int(contentLeft+10u),int(cardTop+9u)),3,cardMaterial);if(renderPc.inspectMode!=0u){uint phase=cellPhase(inspected);text=text||fixedPixel(pixel,ivec2(int(contentLeft+10u),int(cardTop+36u)),2,12u)||phasePixel(pixel,ivec2(int(contentLeft+70u),int(cardTop+36u)),2,phase)||fixedPixel(pixel,ivec2(int(contentLeft+190u),int(cardTop+36u)),2,13u)||signedNumberPixel(pixel,ivec2(int(contentLeft+238u),int(cardTop+36u)),2,inspected.temperature);}uint first=cardTop+(renderPc.inspectMode!=0u?58u:38u);for(uint line=0u;line<10u;++line){uint ly=first+line*18u;if(ly+14u<cardBottom&&cardPixel(pixel,ivec2(int(contentLeft+10u),int(ly)),2,cardMaterial,line))text=true;}if(text)color=vec3(0.93,0.96,0.99);outColor=vec4(color,1);return;}
      if(actor.enabled!=0u){uint top=cardBottom+3u;if(y>=top){color=vec3(0.032,0.043,0.058);bool a=fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+8u)),2,45u)||numberPixel(pixel,ivec2(int(contentLeft+48u),int(top+8u)),2,actor.health)||fixedPixel(pixel,ivec2(int(contentLeft+116u),int(top+8u)),2,46u)||numberPixel(pixel,ivec2(int(contentLeft+156u),int(top+8u)),2,actor.oxygen)||fixedPixel(pixel,ivec2(int(contentLeft+224u),int(top+8u)),2,47u)||numberPixel(pixel,ivec2(int(contentLeft+286u),int(top+8u)),2,actor.ammo)||fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+34u)),2,60u)||fixedPixel(pixel,ivec2(int(contentLeft+174u),int(top+34u)),2,61u)||fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+60u)),1,62u)||fixedPixel(pixel,ivec2(int(contentLeft+104u),int(top+60u)),1,63u)||fixedPixel(pixel,ivec2(int(contentLeft+250u),int(top+60u)),1,64u);if(a)color=vec3(0.94,0.97,1);outColor=vec4(color,1);return;}}
      if(text)color=vec3(0.94,0.97,1);outColor=vec4(color,1);return;
    }

    uint viewportRight ='''
frag=rx(frag,r"    if \(y < renderPc\.statusHeight\) \{.*?\n    uint viewportRight =",sidebar,"sidebar shader")
h=frag.find("    if (actor.enabled != 0u && renderPc.windowWidth >= 560u) {");i=frag.find("    // Exact Alt inspection card.")
if h<0 or i<0 or i<=h: raise RuntimeError("HUD markers")
frag=frag[:h]+frag[i:]
i=frag.find("    // Exact Alt inspection card.");o=frag.find("    outColor = vec4(color.rgb, 1.0);",i)
if i<0 or o<0: raise RuntimeError("inspect markers")
frag=frag[:i]+frag[o:]
write("shaders/fullscreen.frag",frag)
print("SandHybrid Fix26 systems/sidebar correction applied.")
