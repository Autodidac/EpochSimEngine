#version 450
#extension GL_GOOGLE_include_directive : require
#define EPOCH_SAND_NO_SIM_PUSH
#include "materials.glsl"
#include "tiles.glsl"
#include "actor.glsl"
#include "epochgui_font.glsl"
#include "ui_text.glsl"

layout(location = 0) out vec4 outColor;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 3) readonly buffer ActorBuffer { ActorState actor; };
layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };

layout(push_constant) uniform RenderPush {
    uint gridWidth;
    uint gridHeight;
    uint windowWidth;
    uint windowHeight;
    uint selectedMaterial;
    uint materialCount;
    int cursorX;
    int cursorY;
    uint brushRadius;
    uint statusHeight;
    uint paletteHeight;
    uint groupTabsHeight;
    uint materialSlots;
    uint framesPerSecond;
    uint paused;
    uint stepsPerFrame;
    uint selectedGroup;
    uint hoveredGroup;
    uint hoveredMaterial;
    uint selectedScene;
    uint groupCount;
    uint sceneCount;
    uint miningMode;
    uint inspectMode;
    uint debugMode;
    uint tileColumns;
    uint tileRows;
} renderPc;

uint glyphRow(uint code, uint row) {
    if (row >= 7u) return 0u;
    uvec2 bits = epochGuiGlyphBits(code);
    return row < 6u ? ((bits.x >> (row * 5u)) & 31u) : (bits.y & 31u);
}

bool glyphPixel(ivec2 pixel, ivec2 origin, int scale, uint code) {
    ivec2 local = pixel - origin;
    if (scale <= 0 || local.x < 0 || local.y < 0) return false;
    int column = local.x / scale;
    int row = local.y / scale;
    if (column >= 5 || row >= 7) return false;
    return (glyphRow(code, uint(row)) & (1u << uint(4 - column))) != 0u;
}

bool fixedPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < fixedTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, fixedTextChar(id, i))) return true;
    return false;
}
bool materialPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < materialTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, materialTextChar(id, i))) return true;
    return false;
}
bool groupPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < groupTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, groupTextChar(id, i))) return true;
    return false;
}
bool scenePixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < sceneTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, sceneTextChar(id, i))) return true;
    return false;
}
bool phasePixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < phaseTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, phaseTextChar(id, i))) return true;
    return false;
}
bool cardPixel(ivec2 pixel, ivec2 origin, int scale, uint materialId, uint line) {
    for (uint i = 0u; i < cardTextLength(materialId, line); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, cardTextChar(materialId, line, i))) return true;
    return false;
}

uint decimalLength(uint value) {
    if (value >= 10000u) return 5u;
    if (value >= 1000u) return 4u;
    if (value >= 100u) return 3u;
    if (value >= 10u) return 2u;
    return 1u;
}
uint decimalDivisor(uint positionFromRight) {
    if (positionFromRight == 4u) return 10000u;
    if (positionFromRight == 3u) return 1000u;
    if (positionFromRight == 2u) return 100u;
    if (positionFromRight == 1u) return 10u;
    return 1u;
}
bool numberPixel(ivec2 pixel, ivec2 origin, int scale, uint value) {
    value = min(value, 99999u);
    uint length = decimalLength(value);
    for (uint i = 0u; i < length; ++i) {
        uint divisor = decimalDivisor(length - i - 1u);
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale,
                       48u + (value / divisor) % 10u)) return true;
    }
    return false;
}
bool signedNumberPixel(ivec2 pixel, ivec2 origin, int scale, int value) {
    if (value < 0) {
        if (glyphPixel(pixel, origin, scale, 45u)) return true;
        return numberPixel(pixel, origin + ivec2(6 * scale, 0), scale, uint(-value));
    }
    return numberPixel(pixel, origin, scale, uint(value));
}

bool borderPixel(uint x, uint y, uint left, uint top, uint right, uint bottom) {
    return x <= left + 1u || x + 2u >= right || y <= top + 1u || y + 2u >= bottom;
}

Cell cellAt(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
    return cells[uint(p.y) * renderPc.gridWidth + uint(p.x)];
}
TileState tileAt(ivec2 p) { return tiles[tileIndex(p, renderPc.gridWidth)]; }

vec3 backgroundColor(ivec2 grid) {
    float depth = float(grid.y) / float(max(renderPc.gridHeight, 1u));
    return mix(vec3(0.055, 0.105, 0.18), vec3(0.018, 0.024, 0.037), depth);
}

vec4 gasPresentation(Cell cell, ivec2 grid, vec4 base) {
    // ONI-style readability without changing simulation storage: contiguous gas
    // fields render as coherent translucent volumes while isolated cells remain
    // subtle instead of producing noisy full-screen particle halos.
    float densityField = float(stateValue(cell)) / 255.0;
    uint sameNeighbors = 0u;
    sameNeighbors += cellAt(grid + ivec2(-1, 0)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(1, 0)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(0, -1)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(0, 1)).material == cell.material ? 1u : 0u;
    float cohesion = float(sameNeighbors) * 0.045;
    float restrained = 0.08 + densityField * 0.13 + cohesion;
    if (cell.material == MAT_CARBON_DIOXIDE) restrained = 0.22 + densityField * 0.18 + cohesion;
    if (cell.material == MAT_HYDROGEN) restrained = 0.14 + densityField * 0.17 + cohesion;
    if (sameNeighbors == 0u) restrained *= 0.58;
    base.a = clamp(restrained, 0.045, 0.46);
    return base;
}

vec4 worldColor(Cell cell, ivec2 grid) {
    vec4 base = materialColor(cell.material, cell.age, cell.aux, grid);
    uint phase = cellPhase(cell);
    if (phase == PHASE_GAS || phase == PHASE_VAPOR) {
        base = gasPresentation(cell, grid, base);
        base.rgb = mix(backgroundColor(grid), base.rgb, base.a);
        base.a = 1.0;
    }
    if (phase == PHASE_MOLTEN) {
        float heat = clamp(float(cell.temperature - materialMeltingPoint(cell.material)) / 900.0, 0.0, 1.0);
        base.rgb = mix(base.rgb, vec3(1.0, 0.30, 0.04), 0.22 + heat * 0.36);
    } else if (phase == PHASE_SOFTENED) {
        base.rgb = mix(base.rgb, vec3(0.94, 0.58, 0.18), 0.16);
    } else if (phase == PHASE_VAPOR) {
        base.rgb = mix(base.rgb, vec3(0.72, 0.78, 0.88), 0.35);
    }

    // Cohesive terrain shading follows actual material boundaries, never raw
    // tile boundaries. Normal play therefore reads as terrain rather than a grid.
    if (isStructural(cell)) {
        uint different = 0u;
        different += cellAt(grid + ivec2(-1, 0)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(1, 0)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(0, -1)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(0, 1)).material != cell.material ? 1u : 0u;
        base.rgb *= 1.0 - float(different) * 0.055;
    }
    return vec4(base.rgb, 1.0);
}

bool dangerous(Cell cell) {
    int ignition = materialIgnitionPoint(cell.material);
    return cell.material == MAT_ACID || cell.material == MAT_LAVA || cell.material == MAT_FIRE ||
           cell.material == MAT_LIGHTNING || cell.material == MAT_RADIATION ||
           cell.material == MAT_PLASMA_BOLT || cell.material == MAT_HYDROGEN ||
           cell.temperature >= 400 ||
           (ignition != NO_TEMPERATURE && cell.temperature >= ignition);
}

float segmentDistance(vec2 point, vec2 start, vec2 finish) {
    vec2 segment = finish - start;
    float denominator = max(dot(segment, segment), 0.0001);
    float t = clamp(dot(point - start, segment) / denominator, 0.0, 1.0);
    return length(point - (start + segment * t));
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    uint x = uint(clamp(gl_FragCoord.x, 0.0, float(renderPc.windowWidth - 1u)));
    uint y = uint(clamp(gl_FragCoord.y, 0.0, float(renderPc.windowHeight - 1u)));

    if (y < renderPc.statusHeight) {
        vec3 color = vec3(0.035, 0.046, 0.065);
        if (y + 2u >= renderPc.statusHeight) color = vec3(0.14, 0.23, 0.32);

        const int widths[5] = int[5](68, 68, 92, 124, 92);
        int starts[5];
        int rightEdge = int(renderPc.windowWidth) - 5;
        starts[4] = rightEdge - widths[4];
        starts[3] = starts[4] - 4 - widths[3];
        starts[2] = starts[3] - 4 - widths[2];
        starts[1] = starts[2] - 4 - widths[1];
        starts[0] = starts[1] - 4 - widths[0];
        bool fullControls = renderPc.windowWidth >= 920u;
        int firstControl = fullControls ? 0 : 3;
        for (int i = firstControl; i < 5; ++i) {
            if (int(x) >= starts[i] && int(x) < starts[i] + widths[i] && y >= 10u && y < 50u)
                color = i == 4 && renderPc.debugMode != 0u ? vec3(0.24, 0.40, 0.22) : vec3(0.075, 0.105, 0.145);
        }

        bool text = fixedPixel(pixel, ivec2(10, 11), 3, 0u);
        int fpsX = fullControls ? starts[0] - 178 : starts[3] - 178;
        fpsX = max(fpsX, 208);
        text = text || fixedPixel(pixel, ivec2(fpsX, 11), 3, 1u) ||
               numberPixel(pixel, ivec2(fpsX + 62, 11), 3, renderPc.framesPerSecond);

        if (renderPc.windowWidth >= 1250u) {
            int sceneX = 204;
            text = text || fixedPixel(pixel, ivec2(sceneX, 15), 2, 5u) ||
                   scenePixel(pixel, ivec2(sceneX + 66, 15), 2,
                              renderPc.selectedScene % max(renderPc.sceneCount, 1u));
        }
        if (renderPc.windowWidth >= 1080u) {
            int hintX = max(fpsX - 150, 390);
            text = text || fixedPixel(pixel, ivec2(hintX, 18), 2,
                                      renderPc.inspectMode != 0u ? 10u : 40u);
        }

        if (fullControls) {
            text = text || fixedPixel(pixel, ivec2(starts[0] + 11, 23), 2, 41u) ||
                   fixedPixel(pixel, ivec2(starts[1] + 11, 23), 2, 42u) ||
                   fixedPixel(pixel, ivec2(starts[2] + 17, 23), 2, 6u);
        }
        text = text || fixedPixel(pixel, ivec2(starts[3] +
                   (renderPc.miningMode != 0u ? 39 : 33), 23), 2,
                   renderPc.miningMode != 0u ? 8u : 7u) ||
               fixedPixel(pixel, ivec2(starts[4] + 17, 23), 2, 9u);
        if (text) color = vec3(0.93, 0.96, 0.98);
        outColor = vec4(color, 1.0);
        return;
    }

    uint controlsStart = renderPc.windowHeight - renderPc.paletteHeight;
    if (y >= controlsStart) {
        vec3 panel = vec3(0.030, 0.040, 0.056);
        uint contentTop = controlsStart + 3u;
        if (y < contentTop) { outColor = vec4(0.12, 0.20, 0.28, 1.0); return; }
        uint localY = y - contentTop;
        if (localY < renderPc.groupTabsHeight) {
            uint usableX = x > 5u ? x - 5u : 0u;
            uint usableWidth = max(renderPc.windowWidth - 10u, 1u);
            uint group = min(renderPc.groupCount - 1u, usableX * renderPc.groupCount / usableWidth);
            uint left = 5u + group * usableWidth / renderPc.groupCount;
            uint right = 5u + (group + 1u) * usableWidth / renderPc.groupCount;
            uint top = contentTop;
            uint bottom = contentTop + renderPc.groupTabsHeight;
            vec3 color = group == renderPc.selectedGroup ? vec3(0.14, 0.30, 0.45) : panel;
            if (group == renderPc.hoveredGroup) color += vec3(0.045);
            if (borderPixel(x, y, left, top, right, bottom)) color *= 0.55;
            int labelScale = int(right - left) >= int(groupTextLength(group)) * 12 + 8 ? 2 : 1;
            int labelWidth = int(groupTextLength(group)) * 6 * labelScale - labelScale;
            int labelX = int(left + right) / 2 - labelWidth / 2;
            int labelY = int(top + bottom) / 2 - (7 * labelScale) / 2;
            if (groupPixel(pixel, ivec2(labelX, labelY), labelScale, group)) color = vec3(0.95);
            outColor = vec4(color, 1.0);
            return;
        }

        uint itemTop = contentTop + renderPc.groupTabsHeight;
        uint usableX = x > 5u ? x - 5u : 0u;
        uint usableWidth = max(renderPc.windowWidth - 10u, 1u);
        uint slotCount = max(groupMaterialCount(renderPc.selectedGroup), 1u);
        uint slot = min(slotCount - 1u, usableX * slotCount / usableWidth);
        uint materialId = groupMaterial(renderPc.selectedGroup, slot);
        if (materialId >= renderPc.materialCount) { outColor = vec4(panel, 1.0); return; }
        uint left = 5u + slot * usableWidth / slotCount;
        uint right = 5u + (slot + 1u) * usableWidth / slotCount;
        uint bottom = renderPc.windowHeight - 3u;
        vec3 color = materialColor(materialId, 0u, materialId * 1299721u,
                                   ivec2(int(slot), int(renderPc.selectedGroup))).rgb * 0.66;
        if (materialId == renderPc.selectedMaterial) color = min(color * 1.12 + vec3(0.12), vec3(1.0));
        if (materialId == renderPc.hoveredMaterial) color = min(color + vec3(0.08), vec3(1.0));
        if (borderPixel(x, y, left, itemTop, right, bottom)) color *= materialId == renderPc.selectedMaterial ? 0.36 : 0.52;
        int labelScale = int(right - left) >= int(materialTextLength(materialId)) * 12 + 8 ? 2 : 1;
        int labelWidth = int(materialTextLength(materialId)) * 6 * labelScale - labelScale;
        int labelX = int(left + right) / 2 - labelWidth / 2;
        int labelY = int(itemTop + bottom) / 2 - (7 * labelScale) / 2;
        if (materialPixel(pixel, ivec2(labelX, labelY), labelScale, materialId)) {
            color = dot(color, vec3(0.299, 0.587, 0.114)) > 0.55 ? vec3(0.02) : vec3(0.97);
        }
        outColor = vec4(color, 1.0);
        return;
    }

    uint simulationHeight = max(renderPc.windowHeight - renderPc.statusHeight - renderPc.paletteHeight, 1u);
    uint simulationY = y - renderPc.statusHeight;
    uint gridX = min(renderPc.gridWidth - 1u, x * renderPc.gridWidth / max(renderPc.windowWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u, simulationY * renderPc.gridHeight / simulationHeight);
    ivec2 grid = ivec2(int(gridX), int(gridY));
    Cell cell = cellAt(grid);
    TileState tile = tileAt(grid);
    vec4 color = worldColor(cell, grid);

    if (renderPc.debugMode != 0u) {
        ivec2 local = ivec2(int(gridX & 7u), int(gridY & 7u));
        if (local.x == 0 || local.y == 0) color.rgb *= 0.45;
        vec3 overlay = vec3(0.0);
        float alpha = 0.0;
        if (tileHas(tile, TILE_COLLAPSING) || tileHas(tile, TILE_DAMAGED)) { overlay = vec3(0.95, 0.15, 0.10); alpha = 0.34; }
        else if (tileHas(tile, TILE_RECONSTRUCT) || tileHas(tile, TILE_CANDIDATE)) { overlay = vec3(0.95, 0.72, 0.12); alpha = 0.30; }
        else if (tileHas(tile, TILE_SLEEPING)) { overlay = vec3(0.16, 0.72, 0.38); alpha = 0.22; }
        else if (tileHas(tile, TILE_ACTIVE)) { overlay = vec3(0.10, 0.65, 0.92); alpha = 0.18; }
        color.rgb = mix(color.rgb, overlay, alpha * float(tile.occupancy) / 64.0);
    }

    if (actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {
        vec2 toolOrigin = vec2(float(actor.x), float(actor.y - 4));
        vec2 toolHit = vec2(float(actor.hitX), float(actor.hitY));
        float beamDistance = segmentDistance(vec2(grid) + vec2(0.5), toolOrigin, toolHit);
        if (beamDistance < 0.72) {
            color = actor.shotTimer > 7u ? vec4(1.0, 0.28, 0.68, 1.0)
                                         : vec4(1.0, 0.82, 0.20, 1.0);
        }
        ivec2 impactDelta = grid - ivec2(actor.hitX, actor.hitY);
        int impactDistance = impactDelta.x * impactDelta.x + impactDelta.y * impactDelta.y;
        if (impactDistance >= 3 && impactDistance <= 10) color = vec4(1.0, 0.96, 0.72, 1.0);
    }

    if (actor.enabled != 0u && actor.health != 0u) {
        ivec2 d = grid - ivec2(actor.x, actor.y);
        bool body = d.x >= -2 && d.x <= 2 && d.y >= -7 && d.y <= 0;
        bool visor = d.y >= -6 && d.y <= -5 && d.x >= -1 && d.x <= 2;
        if (body) color = visor ? vec4(0.20, 0.88, 1.0, 1.0) : vec4(0.82, 0.88, 0.94, 1.0);
    }

    if (renderPc.inspectMode == 0u) {
        ivec2 delta = grid - ivec2(renderPc.cursorX, renderPc.cursorY);
        int distanceSquared = delta.x * delta.x + delta.y * delta.y;
        if (renderPc.miningMode != 0u) {
            bool cross = (abs(delta.x) <= 4 && delta.y == 0) || (abs(delta.y) <= 4 && delta.x == 0);
            bool ring = distanceSquared >= 8 && distanceSquared <= 14;
            if (cross || ring) color.rgb = vec3(1.0, 0.88, 0.26);
        } else {
            int outer = int(renderPc.brushRadius * renderPc.brushRadius);
            int innerRadius = max(int(renderPc.brushRadius) - 1, 0);
            if (distanceSquared <= outer && distanceSquared >= innerRadius * innerRadius)
                color.rgb = vec3(1.0) - color.rgb;
        }
    }

    if (actor.enabled != 0u && renderPc.windowWidth >= 560u) {
        uint hudLeft = 10u;
        uint hudTop = renderPc.statusHeight + 10u;
        uint hudRight = min(renderPc.windowWidth - 10u, hudLeft + 670u);
        uint hudBottom = hudTop + 78u;
        if (x >= hudLeft && x < hudRight && y >= hudTop && y < hudBottom) {
            vec3 hudColor = vec3(0.025, 0.034, 0.048);
            if (borderPixel(x, y, hudLeft, hudTop, hudRight, hudBottom)) hudColor = vec3(0.12, 0.22, 0.31);

            uint hpBarLeft = hudLeft + 50u;
            uint hpBarRight = hpBarLeft + 176u;
            uint o2BarLeft = hudLeft + 300u;
            uint o2BarRight = o2BarLeft + 176u;
            if (x >= hpBarLeft && x < hpBarRight && y >= hudTop + 9u && y < hudTop + 29u) {
                uint fill = hpBarLeft + actor.health * (hpBarRight - hpBarLeft) / 255u;
                hudColor = x < fill ? vec3(0.22, 0.78, 0.30) : vec3(0.18, 0.08, 0.08);
            }
            if (x >= o2BarLeft && x < o2BarRight && y >= hudTop + 9u && y < hudTop + 29u) {
                uint fill = o2BarLeft + actor.oxygen * (o2BarRight - o2BarLeft) / 255u;
                hudColor = x < fill ? vec3(0.24, 0.72, 0.98) : vec3(0.06, 0.09, 0.13);
            }

            bool hudText = fixedPixel(pixel, ivec2(int(hudLeft + 10u), int(hudTop + 12u)), 2, 45u) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 258u), int(hudTop + 12u)), 2, 46u) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 10u), int(hudTop + 44u)), 2, 47u) ||
                           numberPixel(pixel, ivec2(int(hudLeft + 78u), int(hudTop + 44u)), 2, actor.ammo) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 156u), int(hudTop + 44u)), 2, 48u) ||
                           numberPixel(pixel, ivec2(int(hudLeft + 224u), int(hudTop + 44u)), 2, actor.gold) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 304u), int(hudTop + 44u)), 2, 49u) ||
                           numberPixel(pixel, ivec2(int(hudLeft + 372u), int(hudTop + 44u)), 2, actor.iron) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 450u), int(hudTop + 44u)), 1, 50u) ||
                           fixedPixel(pixel, ivec2(int(hudLeft + 548u), int(hudTop + 44u)), 1, 51u);
            if (hudText) hudColor = vec3(0.94, 0.97, 1.0);
            outColor = vec4(hudColor, 0.97);
            return;
        }
    }

    // Exact Alt inspection card. It reads the cursor cell and its tile directly;
    // it never scans, selects, paints, or modifies simulation state.
    if (renderPc.inspectMode != 0u && renderPc.windowWidth >= 320u && simulationHeight >= 190u) {
        ivec2 cursor = clamp(ivec2(renderPc.cursorX, renderPc.cursorY), ivec2(0),
                             ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
        Cell inspected = cellAt(cursor);
        TileState inspectedTile = tileAt(cursor);
        uint cursorScreenX = uint(cursor.x) * renderPc.windowWidth / max(renderPc.gridWidth, 1u);
        uint cardWidth = min(366u, renderPc.windowWidth - 24u);
        uint cardHeight = min(252u, simulationHeight - 12u);
        uint cardLeft = cursorScreenX > renderPc.windowWidth / 2u ? 12u : renderPc.windowWidth - cardWidth - 12u;
        uint cursorScreenY = renderPc.statusHeight + uint(cursor.y) * simulationHeight / max(renderPc.gridHeight, 1u);
        uint minTop = renderPc.statusHeight + 10u;
        uint maxTop = controlsStart > cardHeight + 10u ? controlsStart - cardHeight - 10u : minTop;
        uint cardTop = clamp(cursorScreenY > cardHeight / 2u ? cursorScreenY - cardHeight / 2u : minTop, minTop, maxTop);
        uint cardRight = cardLeft + cardWidth;
        uint cardBottom = cardTop + cardHeight;
        if (x >= cardLeft && x < cardRight && y >= cardTop && y < cardBottom) {
            bool alert = dangerous(inspected);
            vec3 cardColor = vec3(0.036, 0.048, 0.066);
            if (borderPixel(x, y, cardLeft, cardTop, cardRight, cardBottom))
                cardColor = alert ? vec3(0.76, 0.16, 0.10) : vec3(0.13, 0.29, 0.43);
            else if (y < cardTop + 30u) cardColor = vec3(0.065, 0.095, 0.135);

            bool text = materialPixel(pixel, ivec2(int(cardLeft + 10u), int(cardTop + 8u)), 2, inspected.material);
            if (alert) text = text || fixedPixel(pixel, ivec2(int(cardRight - 50u), int(cardTop + 11u)), 1, 25u);
            uint phase = cellPhase(inspected);
            text = text || fixedPixel(pixel, ivec2(int(cardLeft + 10u), int(cardTop + 38u)), 1, 12u) ||
                   phasePixel(pixel, ivec2(int(cardLeft + 58u), int(cardTop + 38u)), 1, phase) ||
                   fixedPixel(pixel, ivec2(int(cardLeft + 172u), int(cardTop + 38u)), 1, 13u) ||
                   signedNumberPixel(pixel, ivec2(int(cardLeft + 208u), int(cardTop + 38u)), 1, inspected.temperature);
            text = text || fixedPixel(pixel, ivec2(int(cardLeft + 10u), int(cardTop + 54u)), 1, 14u) ||
                   numberPixel(pixel, ivec2(int(cardLeft + 46u), int(cardTop + 54u)), 1, inspectedTile.occupancy) ||
                   fixedPixel(pixel, ivec2(int(cardLeft + 104u), int(cardTop + 54u)), 1, 15u) ||
                   numberPixel(pixel, ivec2(int(cardLeft + 170u), int(cardTop + 54u)), 1,
                               isStructural(inspected) ? (stateValue(inspected) == 0u ? 255u : stateValue(inspected)) : 0u) ||
                   fixedPixel(pixel, ivec2(int(cardLeft + 228u), int(cardTop + 54u)), 1, 16u) ||
                   numberPixel(pixel, ivec2(int(cardLeft + 276u), int(cardTop + 54u)), 1, materialDensity(inspected.material));

            uint stateLabel = isStructural(inspected) ? 26u : 27u;
            if (tileHas(inspectedTile, TILE_SLEEPING)) stateLabel = 28u;
            else if (tileHas(inspectedTile, TILE_CANDIDATE)) stateLabel = 30u;
            text = text || fixedPixel(pixel, ivec2(int(cardLeft + 10u), int(cardTop + 70u)), 1, stateLabel);

            for (uint line = 0u; line < 10u; ++line) {
                ivec2 origin = ivec2(int(cardLeft + 10u), int(cardTop + 86u + line * 15u));
                if (cardPixel(pixel, origin, 1, inspected.material, line)) text = true;
            }
            if (text) cardColor = alert && y < cardTop + 30u ? vec3(1.0, 0.78, 0.68) : vec3(0.92, 0.95, 0.98);
            outColor = vec4(cardColor, 0.98);
            return;
        }
    }

    outColor = vec4(color.rgb, 1.0);
}
