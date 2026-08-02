#version 450
#extension GL_GOOGLE_include_directive : require
#define SANDHYBRID_NO_SIM_PUSH
#include "materials.glsl"
#include "tiles.glsl"
#include "chunks.glsl"
#include "actor.glsl"
#include "epochgui_font.glsl"
#include "ui_text.glsl"
#include "debug_stats.glsl"

layout(location = 0) out vec4 outColor;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 3) readonly buffer ActorBuffer { ActorState actor; };
layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };
layout(std430, binding = 5) readonly buffer DebugStatsBuffer { uint debugStats[]; };
layout(std430, binding = 7) readonly buffer Chunks { ChunkState chunks[]; };

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
    uint viewportLeft;
    uint viewportTop;
    uint viewportWidth;
    uint viewportHeight;
    uint viewOriginX;
    uint viewOriginY;
    uint viewWidth;
    uint viewHeight;
    uint brushShape;
    uint placementMode;
    uint activeAreaCount;
    int activeAreaX;
    int activeAreaY;
    uint activeScopeMode;
    uint cameraControls;
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
    if (value >= 10000000u) return 8u;
    if (value >= 1000000u) return 7u;
    if (value >= 100000u) return 6u;
    if (value >= 10000u) return 5u;
    if (value >= 1000u) return 4u;
    if (value >= 100u) return 3u;
    if (value >= 10u) return 2u;
    return 1u;
}
uint decimalDivisor(uint positionFromRight) {
    if (positionFromRight == 7u) return 10000000u;
    if (positionFromRight == 6u) return 1000000u;
    if (positionFromRight == 5u) return 100000u;
    if (positionFromRight == 4u) return 10000u;
    if (positionFromRight == 3u) return 1000u;
    if (positionFromRight == 2u) return 100u;
    if (positionFromRight == 1u) return 10u;
    return 1u;
}
bool numberPixel(ivec2 pixel, ivec2 origin, int scale, uint value) {
    value = min(value, 99999999u);
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


bool statPixel(ivec2 pixel, ivec2 origin, int scale, uint labelId, uint value) {
    bool label = fixedPixel(pixel, origin, scale, labelId);
    int numberX = int(fixedTextLength(labelId)) * 6 * scale + 4 * scale;
    return label || numberPixel(pixel, origin + ivec2(numberX, 0), scale, value);
}

vec3 debugStatColor(uint stat) {
    if (stat <= 2u) return vec3(0.74, 0.94, 1.00);      // timing and cells
    if (stat <= 4u) return vec3(1.00, 0.62, 0.18);      // fine movement
    if (stat <= 6u) return vec3(1.00, 0.90, 0.28);      // colony
    if (stat <= 8u) return vec3(0.52, 0.94, 0.58);      // tile activity
    if (stat <= 12u) return vec3(1.00, 0.48, 0.82);     // actors and impulses
    if (stat <= 21u) return vec3(0.50, 0.78, 1.00);     // hierarchy and chunks
    if (stat == 22u) return vec3(0.56, 0.76, 1.00);     // gas tiles
    if (stat == 23u) return vec3(0.26, 0.94, 1.00);     // liquid tiles
    if (stat == 24u) return vec3(0.42, 1.00, 0.70);     // enclosed media
    return vec3(1.00, 0.30, 0.74);                      // breakup to fine cells
}

vec3 debugKeyColor(uint key) {
    if (key == 0u) return vec3(1.00, 0.08, 0.05);       // damaged/collapsing
    if (key == 1u) return vec3(1.00, 0.88, 0.08);       // stable/candidate
    if (key == 2u) return vec3(1.00, 0.42, 0.04);       // bulk moved
    if (key == 3u) return vec3(1.00, 0.10, 0.72);       // fine active: vivid hot state
    if (key == 4u) return vec3(0.58, 0.20, 1.00);       // bulk ready: vivid violet
    if (key == 5u) return vec3(0.08, 0.96, 0.28);       // settled medium: vivid green
    if (key == 6u) return vec3(0.035, 0.10, 0.30);      // sleeping: dark cool state
    if (key == 7u) return vec3(0.04, 0.52, 1.00);       // active
    if (key == 8u) return vec3(0.12, 0.72, 0.94);       // enclosed medium
    return vec3(1.00, 0.10, 0.56);                      // breakup
}

bool borderPixel(uint x, uint y, uint left, uint top, uint right, uint bottom) {
    return x <= left + 1u || x + 2u >= right || y <= top + 1u || y + 2u >= bottom;
}

bool debugPanelPixel(ivec2 pixel, uint x, uint y, uint panelLeft, uint panelTop,
                     uint panelRight, uint panelBottom, int textScale, inout vec3 color) {
    if (x < panelLeft || x >= panelRight || y < panelTop || y >= panelBottom) return false;
    color = vec3(0.006, 0.010, 0.018);
    if (borderPixel(x, y, panelLeft, panelTop, panelRight, panelBottom))
        color = vec3(0.34, 0.72, 1.00);

    bool textHit = false;
    vec3 textColor = vec3(0.96, 0.99, 1.00);
    if (fixedPixel(pixel, ivec2(int(panelLeft + 10u), int(panelTop + 5u)), textScale, 75u)) {
        textHit = true;
        textColor = vec3(1.00);
    }
    uint scopeLabelX = panelRight > panelLeft + 122u ? panelRight - 112u : panelLeft + 10u;
    if (fixedPixel(pixel, ivec2(int(scopeLabelX), int(panelTop + 8u)), 1,
                   renderPc.activeScopeMode == 2u ? 139u : 138u)) {
        textHit = true;
        textColor = vec3(0.52, 0.94, 0.58);
    }

    const uint statCount = 22u;
    uint rowHeight = textScale == 2 ? 18u : 12u;
    uint headerHeight = textScale == 2 ? 24u : 15u;
    uint fixedLabels[statCount] = uint[statCount](
        1u, 143u, 137u, 144u, 145u, 98u, 81u,
        146u, 78u, 117u, 147u, 148u, 149u, 150u,
        151u, 152u, 153u, 154u, 155u, 156u, 110u, 82u);
    uint fixedValues[statCount] = uint[statCount](
        renderPc.framesPerSecond,
        renderPc.gridWidth * renderPc.gridHeight * 36u / (1024u * 1024u),
        renderPc.activeAreaCount,
        debugStats[STAT_MOVE_PAIR_TESTS],
        debugStats[STAT_CHUNK_SKIPPED_CELLS],
        debugStats[STAT_ACTIVE_TILES],
        debugStats[STAT_SLEEPING_TILES],
        debugStats[STAT_MOVED_CELLS],
        debugStats[STAT_MOVE_SWAPS],
        debugStats[STAT_MACRO_TILE_MOVES],
        debugStats[STAT_MACRO_GAS_TILES],
        debugStats[STAT_MACRO_LIQUID_TILES],
        debugStats[STAT_STRUCTURAL_COLLAPSES],
        debugStats[STAT_CONVEYOR_MOVES],
        debugStats[STAT_MACHINE_INPUTS],
        debugStats[STAT_MACHINE_OUTPUTS],
        debugStats[STAT_VOLCANO_LAVA_OUTPUTS],
        debugStats[STAT_VOLCANO_GAS_OUTPUTS],
        debugStats[STAT_GAS_EDGE_ACTIVE_TILES],
        debugStats[STAT_CHEMISTRY_CHANGES],
        debugStats[STAT_ACTOR_MOVES],
        debugStats[STAT_BEE_COUNT]);
    for (uint stat = 0u; stat < statCount; ++stat) {
        bool hit = statPixel(pixel,
            ivec2(int(panelLeft + 10u), int(panelTop + headerHeight + stat * rowHeight)),
            textScale, fixedLabels[stat], fixedValues[stat]);
        if (hit) {
            textHit = true;
            textColor = debugStatColor(stat > 1u ? stat - 2u : stat);
        }
    }

    // Restrained visual grouping: resource pressure, hierarchy/activity, and
    // world events remain readable without surrounding every row with boxes.
    uint separatorYs[3] = uint[3](
        panelTop + headerHeight + 5u * rowHeight - 4u,
        panelTop + headerHeight + 9u * rowHeight - 4u,
        panelTop + headerHeight + 13u * rowHeight - 4u);
    for (uint separator = 0u; separator < 3u; ++separator) {
        uint separatorY = separatorYs[separator];
        if (y >= separatorY && y < separatorY + 1u &&
            x >= panelLeft + 8u && x < panelRight - 8u)
            color = vec3(0.10, 0.21, 0.30);
    }

    uint keyRows = 5u;
    uint cardHeight = textScale == 2 ? 36u : 28u;
    uint keyTitleHeight = textScale == 2 ? 22u : 14u;
    uint cardsHeight = keyTitleHeight + keyRows * cardHeight + 10u;
    uint keyTop = max(panelTop + headerHeight + statCount * rowHeight + 8u,
                      panelBottom > cardsHeight ? panelBottom - cardsHeight : panelTop);
    if (fixedPixel(pixel, ivec2(int(panelLeft + 10u), int(keyTop)), textScale, 127u)) {
        textHit = true;
        textColor = vec3(1.00);
    }
    const uint keyCount = 10u;
    uint keyLabels[keyCount] = uint[keyCount](
        128u, 130u, 131u, 29u, 132u, 135u, 133u, 134u, 28u, 129u);
    uint keyColorMap[keyCount] = uint[keyCount](
        0u, 2u, 3u, 7u, 4u, 9u, 5u, 8u, 6u, 1u);
    uint keyColumns = panelRight - panelLeft >= 330u ? 2u : 1u;
    uint keyColumnWidth = max((panelRight - panelLeft - 20u) / keyColumns, 1u);
    uint swatchSize = textScale == 2 ? 24u : 18u;
    for (uint key = 0u; key < keyCount; ++key) {
        uint column = key % keyColumns;
        uint row = key / keyColumns;
        uint keyLeft = panelLeft + 10u + column * keyColumnWidth;
        uint keyY = keyTop + keyTitleHeight + row * cardHeight;
        uint cardRight = min(keyLeft + keyColumnWidth - 5u, panelRight - 5u);
        uint cardBottom = min(keyY + cardHeight - 4u, panelBottom - 4u);
        if (x >= keyLeft && x < cardRight && y >= keyY && y < cardBottom) {
            color = vec3(0.025, 0.040, 0.060);
            if (borderPixel(x, y, keyLeft, keyY, cardRight, cardBottom))
                color = vec3(0.16, 0.26, 0.36);
        }
        uint swatchTop = keyY + 4u;
        if (swatchTop + swatchSize < cardBottom && x >= keyLeft + 4u && x < keyLeft + 4u + swatchSize &&
            y >= swatchTop && y < swatchTop + swatchSize) {
            uint colorKey = keyColorMap[key];
            color = debugKeyColor(colorKey);
            if (borderPixel(x, y, keyLeft + 4u, swatchTop,
                            keyLeft + 4u + swatchSize, swatchTop + swatchSize))
                color = vec3(1.00);
        }
        if (keyY + 7u * uint(textScale) < panelBottom && fixedPixel(pixel,
            ivec2(int(keyLeft + swatchSize + 12u), int(keyY + 7u)), textScale, keyLabels[key])) {
            textHit = true;
            textColor = vec3(0.94, 0.98, 1.00);
        }
    }
    if (textHit) color = textColor;
    return true;
}

Cell cellAt(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
    return cells[uint(p.y) * renderPc.gridWidth + uint(p.x)];
}
TileState tileAt(ivec2 p) { return tiles[tileIndex(p, renderPc.gridWidth)]; }
ChunkState chunkAt(ivec2 p) { return chunks[chunkIndex(p, renderPc.gridWidth)]; }

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
    if ((cell.aux & AUX_WET) != 0u && !isCellLiquid(cell) && !isCellGas(cell)) {
        base.rgb = mix(base.rgb, vec3(0.08, 0.24, 0.42), 0.20);
        base.rgb *= 0.84;
    }
    if (isHalfWater(cell)) {
        uint waterNeighbors = 0u;
        waterNeighbors += cellAt(grid + ivec2(-1, 0)).material == MAT_WATER ? 1u : 0u;
        waterNeighbors += cellAt(grid + ivec2(1, 0)).material == MAT_WATER ? 1u : 0u;
        waterNeighbors += cellAt(grid + ivec2(0, -1)).material == MAT_WATER ? 1u : 0u;
        waterNeighbors += cellAt(grid + ivec2(0, 1)).material == MAT_WATER ? 1u : 0u;
        float coverage = 0.62 + float(waterNeighbors) * 0.045;
        base.rgb = mix(backgroundColor(grid), base.rgb, min(coverage, 0.80));
    }
    bool metalSurface = cell.material == MAT_ALUMINUM || cell.material == MAT_IRON ||
                        cell.material == MAT_COPPER || cell.material == MAT_GOLD ||
                        cell.material == MAT_STEEL || cell.material == MAT_ALUMINUM_SHAVINGS ||
                        cell.material == MAT_IRON_ORE;
    if (metalSurface) {
        uint grainHash = hash32(uint(grid.x) * 73856093u ^ uint(grid.y) * 19349663u ^ cell.aux);
        float grain = float((grainHash >> 8u) & 31u) / 31.0 - 0.5;
        float brushed = ((grid.x + int(cell.age >> 4u)) & 7) == 0 ? 0.10 : 0.0;
        base.rgb = clamp(base.rgb * (0.88 + grain * 0.20) + vec3(brushed), 0.0, 1.0);
        if (cell.material == MAT_IRON || cell.material == MAT_IRON_ORE)
            base.rgb = mix(base.rgb, vec3(0.34, 0.20, 0.13), float((grainHash >> 16u) & 7u) / 42.0);
        if (cell.material == MAT_COPPER)
            base.rgb = mix(base.rgb, vec3(0.08, 0.42, 0.32), float((grainHash >> 20u) & 3u) / 24.0);
    }
    if (cell.material == MAT_PLANT_STEM) {
        uint stemHash = hash32(uint(grid.x) * 2654435761u ^ uint(grid.y) ^ cell.aux);
        base.rgb = mix(vec3(0.12, 0.42, 0.10), vec3(0.34, 0.66, 0.18), float(stemHash & 7u) / 7.0);
    }
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
           cell.material == MAT_LIGHTNING || cell.material == MAT_RADIATION ||            cell.material == MAT_HYDROGEN ||
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

    uint sidebarWidth = min(renderPc.paletteHeight, renderPc.windowWidth);
    uint sidebarLeft = renderPc.windowWidth - sidebarWidth;
    if (renderPc.debugMode != 0u && sidebarWidth >= 300u && x >= sidebarLeft &&
        y >= renderPc.statusHeight) {
        vec3 debugColor = vec3(0.006, 0.010, 0.018);
        debugPanelPixel(pixel, x, y, sidebarLeft + 4u, renderPc.statusHeight + 4u,
                        renderPc.windowWidth - 4u, renderPc.windowHeight - 4u,
                        2, debugColor);
        outColor = vec4(debugColor, 1.0);
        return;
    }
    if (x >= sidebarLeft) {
        vec3 color = vec3(0.025, 0.034, 0.048);
        uint localX = x - sidebarLeft;
        if (localX < 2u) color = vec3(0.14, 0.23, 0.32);
        bool text = fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 8), 2, 0u);
        uint sceneId = renderPc.selectedScene % max(renderPc.sceneCount, 1u);
        text = text || fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 31), 1, 5u) ||
               scenePixel(pixel, ivec2(int(sidebarLeft + 58u), 27), 2, sceneId) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 51), 2, 1u) ||
               numberPixel(pixel, ivec2(int(sidebarLeft + 58u), 51), 2, renderPc.framesPerSecond) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 136u), 51), 1,
                          renderPc.paused != 0u ? 3u : 2u) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 220u), 31), 1, 105u) ||
               numberPixel(pixel, ivec2(int(sidebarLeft + 264u), 31), 1,
                           max(renderPc.gridWidth / max(renderPc.viewWidth, 1u), 1u)) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 220u), 51), 1, 138u) ||
               numberPixel(pixel, ivec2(int(sidebarLeft + 274u), 51), 1,
                           renderPc.activeAreaCount);

        // A small divider separates scene/navigation controls from editing
        // controls without consuming another boxed panel.
        if (y >= 97u && y < 98u && x >= sidebarLeft + 8u && x < renderPc.windowWidth - 8u)
            color = vec3(0.10, 0.21, 0.30);

        uint sceneGap = 3u;
        uint sceneLeft = sidebarLeft + 8u;
        uint sceneWidth = max(1u, (sidebarWidth - 16u - sceneGap * 4u) / 5u);
        uint sceneIds[5] = uint[5](41u, 42u, 6u, 65u, 66u);
        for (uint i = 0u; i < 5u; ++i) {
            uint left = sceneLeft + i * (sceneWidth + sceneGap);
            uint right = left + sceneWidth;
            if (x >= left && x < right && y >= 70u && y < 96u) {
                color = vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 70u, right, 96u)) color *= 0.55;
                uint length = fixedTextLength(sceneIds[i]);
                int scale = int(sceneWidth) >= int(length * 12u + 6u) ? 2 : 1;
                int width = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                             83 - (7 * scale) / 2), scale, sceneIds[i]))
                    color = vec3(0.95);
            }
        }

        uint controlGap = 3u;
        uint controlLeft = sidebarLeft + 8u;
        uint controlWidth = max(1u, (sidebarWidth - 16u - controlGap * 3u) / 4u);
        uint controlLefts[4] = uint[4](
  controlLeft,
  controlLeft + controlWidth + controlGap,
  controlLeft + (controlWidth + controlGap) * 2u,
  controlLeft + (controlWidth + controlGap) * 3u);
        bool playerScene = renderPc.selectedScene == 6u ||
                 renderPc.selectedScene == 7u ||
                 renderPc.selectedScene == 8u;
        uint controlIds[4] = uint[4](
  renderPc.miningMode != 0u ? 8u : 7u,
  renderPc.paused != 0u ? 3u : 2u,
  playerScene && renderPc.cameraControls == 0u ? 141u : 140u,
  9u);
        for (uint control = 0u; control < 4u; ++control) {
  uint left = controlLefts[control];
  uint right = control == 3u ? renderPc.windowWidth - 8u : left + controlWidth;
  if (x >= left && x < right && y >= 100u && y < 122u) {
      bool enabled = (control == 1u && renderPc.paused != 0u) ||
                     (control == 2u && renderPc.cameraControls != 0u) ||
                     (control == 3u && renderPc.debugMode != 0u);
      color = enabled ? vec3(0.20, 0.38, 0.20) : vec3(0.075, 0.105, 0.145);
      if (borderPixel(x, y, left, 100u, right, 122u)) color *= 0.55;
  }
  uint length = fixedTextLength(controlIds[control]);
  int labelScale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
  int labelWidth = int(length) * 6 * labelScale - labelScale;
  if (fixedPixel(pixel,
      ivec2(int(left + right) / 2 - labelWidth / 2,
            111 - (7 * labelScale) / 2),
      labelScale, controlIds[control])) {
      text = true;
      color = vec3(0.95);
  }
        }

        uint contentLeft = sidebarLeft + 5u;
        uint contentWidth = max(sidebarWidth - 10u, 1u);
        uint groupTop = renderPc.statusHeight + 5u;
        uint groupRows = max((renderPc.groupCount + 1u) / 2u, 1u);
        uint groupCellWidth = max(contentWidth / 2u, 1u);
        uint groupCellHeight = max(renderPc.groupTabsHeight / groupRows, 1u);
        if (y >= groupTop && y < groupTop + renderPc.groupTabsHeight &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            uint column = min((x - contentLeft) / groupCellWidth, 1u);
            uint row = min((y - groupTop) / groupCellHeight, groupRows - 1u);
            uint group = row * 2u + column;
            if (group < renderPc.groupCount) {
                uint left = contentLeft + column * groupCellWidth;
                uint right = column == 1u ? contentLeft + contentWidth : left + groupCellWidth;
                uint top = groupTop + row * groupCellHeight;
                uint bottom = min(groupTop + renderPc.groupTabsHeight, top + groupCellHeight);
                color = group == renderPc.selectedGroup ? vec3(0.14, 0.30, 0.45) : vec3(0.04, 0.052, 0.07);
                if (group == renderPc.hoveredGroup) color += vec3(0.055);
                if (borderPixel(x, y, left, top, right, bottom)) color *= 0.55;
                int scale = int(right - left) >= int(groupTextLength(group)) * 12 + 8 ? 2 : 1;
                int width = int(groupTextLength(group)) * 6 * scale - scale;
                if (groupPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                            int(top + bottom) / 2 - (7 * scale) / 2), scale, group))
                    color = vec3(0.95);
            }
            outColor = vec4(color, 1.0);
            return;
        }

        uint paletteTop = groupTop + renderPc.groupTabsHeight + 3u;
        const uint palettePanelHeight = 136u;
        uint slotCount = max(groupMaterialCount(renderPc.selectedGroup), 1u);
        uint slotRows = max((slotCount + 1u) / 2u, 1u);
        uint cellWidth = max(contentWidth / 2u, 1u);
        uint cellHeight = max(palettePanelHeight / slotRows, 1u);
        if (y >= paletteTop && y < paletteTop + palettePanelHeight &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            uint column = min((x - contentLeft) / cellWidth, 1u);
            uint row = min((y - paletteTop) / cellHeight, slotRows - 1u);
            uint slot = row * 2u + column;
            if (slot < slotCount) {
                uint material = groupMaterial(renderPc.selectedGroup, slot);
                uint left = contentLeft + column * cellWidth;
                uint right = column == 1u ? contentLeft + contentWidth : left + cellWidth;
                uint top = paletteTop + row * cellHeight;
                uint bottom = min(paletteTop + palettePanelHeight, top + cellHeight);
                color = materialColor(material, 0u, material * 1299721u,
                                      ivec2(int(slot), int(renderPc.selectedGroup))).rgb * 0.62;
                if (material == renderPc.selectedMaterial) color = min(color * 1.10 + vec3(0.13), vec3(1.0));
                if (material == renderPc.hoveredMaterial) color = min(color + vec3(0.09), vec3(1.0));
                if (borderPixel(x, y, left, top, right, bottom)) color *= 0.5;
                int scale = int(right - left) >= int(materialTextLength(material)) * 12 + 8 ? 2 : 1;
                int width = int(materialTextLength(material)) * 6 * scale - scale;
                if (materialPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                               int(top + bottom) / 2 - (7 * scale) / 2), scale, material))
                    color = dot(color, vec3(0.299, 0.587, 0.114)) > 0.55 ? vec3(0.02) : vec3(0.97);
            }
            outColor = vec4(color, 1.0);
            return;
        }

        uint eraserTop = paletteTop + palettePanelHeight + 3u;
        uint eraserBottom = eraserTop + 24u;
        uint utilityGap = 4u;
        uint utilityWidth = max((contentWidth - utilityGap * 2u) / 3u, 1u);
        uint utilityLefts[3] = uint[3](
            contentLeft,
            contentLeft + utilityWidth + utilityGap,
            contentLeft + (utilityWidth + utilityGap) * 2u);
        uint utilityRights[3] = uint[3](
            contentLeft + utilityWidth,
            contentLeft + utilityWidth * 2u + utilityGap,
            contentLeft + contentWidth);
        uint utilityLabels[3] = uint[3](67u, 108u, 159u);
        if (y >= eraserTop && y < eraserBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            for (uint button = 0u; button < 3u; ++button) {
                uint left = utilityLefts[button];
                uint right = utilityRights[button];
                if (x < left || x >= right) continue;
                if (button == 0u) {
                    color = renderPc.selectedMaterial == MAT_ATMOSPHERE
                        ? vec3(0.10, 0.46, 0.68) : vec3(0.08, 0.30, 0.46);
                } else if (button == 1u) {
                    color = vec3(0.10, 0.38, 0.20);
                } else {
                    color = renderPc.selectedMaterial == MAT_EMPTY
                        ? vec3(0.62, 0.12, 0.16) : vec3(0.22, 0.055, 0.07);
                }
                if (borderPixel(x, y, left, eraserTop, right, eraserBottom)) color *= 0.55;
                uint label = utilityLabels[button];
                uint length = fixedTextLength(label);
                int scale = int(right - left) >= int(length) * 12 + 4 ? 2 : 1;
                int width = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + (right - left) / 2u) - width / 2,
                                            int(eraserTop + (24u - uint(7 * scale)) / 2u)),
                               scale, label))
                    color = vec3(1.0, 0.94, 0.94);
                outColor = vec4(color, 1.0);
                return;
            }
            outColor = vec4(vec3(0.015, 0.022, 0.032), 1.0);
            return;
        }

        uint keymapTop = eraserBottom + 3u;
        uint keymapBottom = keymapTop + 126u;
        if (y >= keymapTop && y < keymapBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, keymapTop, contentLeft + contentWidth, keymapBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool keyText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 6u)), 2, 68u);
            uint leftIds[7] = uint[7](62u, 63u, 64u, 69u, 140u, 141u, 106u);
            uint rightIds[7] = uint[7](70u, 71u, 72u, 73u, 74u, 107u, 109u);
            uint columnMiddle = contentLeft + contentWidth / 2u;
            if (x >= columnMiddle && x < columnMiddle + 1u &&
                y >= keymapTop + 23u && y < keymapBottom - 6u)
                color = vec3(0.12, 0.20, 0.28);
            for (uint i = 0u; i < 7u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 14u)), 1, leftIds[i]);
            for (uint i = 0u; i < 7u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(columnMiddle + 8u), int(keymapTop + 25u + i * 14u)), 1, rightIds[i]);
            if (keyText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        uint cursorTop = keymapBottom + 3u;
        uint cursorBottom = cursorTop + 120u;
        if (y >= cursorTop && y < cursorBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, cursorTop, contentLeft + contentWidth, cursorBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool cursorText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(cursorTop + 5u)), 2, 99u) ||
                              fixedPixel(pixel, ivec2(int(contentLeft + 112u), int(cursorTop + 8u)), 1, 142u);
            uint placementTop = cursorTop + 23u;
            uint placementWidth = max(contentWidth / 2u, 1u);
            uint placementIds[2] = uint[2](80u, 136u);
            for (uint mode = 0u; mode < 2u; ++mode) {
                uint left = contentLeft + mode * placementWidth;
                uint right = mode == 1u ? contentLeft + contentWidth : left + placementWidth;
                if (x >= left && x < right && y >= placementTop && y < placementTop + 26u) {
                    color = mode == renderPc.placementMode ? vec3(0.14, 0.30, 0.45)
                                                           : vec3(0.055, 0.07, 0.09);
                    if (borderPixel(x, y, left, placementTop, right, placementTop + 26u)) color *= 0.55;
                }
                int labelWidth = int(fixedTextLength(placementIds[mode])) * 12 - 2;
                cursorText = cursorText || fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2, int(placementTop + 6u)),
                    2, placementIds[mode]);
            }
            uint shapeTop = cursorTop + 53u;
            uint shapeWidth = max(contentWidth / 4u, 1u);
            uint shapeIds[4] = uint[4](100u, 101u, 102u, 103u);
            for (uint shape = 0u; shape < 4u; ++shape) {
                uint left = contentLeft + shape * shapeWidth;
                uint right = shape == 3u ? contentLeft + contentWidth : left + shapeWidth;
                if (x >= left && x < right && y >= shapeTop && y < shapeTop + 24u) {
                    color = shape == renderPc.brushShape ? vec3(0.14, 0.30, 0.45) : vec3(0.055, 0.07, 0.09);
                    if (borderPixel(x, y, left, shapeTop, right, shapeTop + 24u)) color *= 0.55;
                }
                int labelWidth = int(fixedTextLength(shapeIds[shape])) * 6 - 1;
                cursorText = cursorText || fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2, int(shapeTop + 8u)), 1, shapeIds[shape]);
            }
            uint controlTop = cursorTop + 89u;
            uint halfWidth = contentWidth / 2u;
            const uint controlButtonWidth = 44u;
            uint brushMinusLeft = contentLeft + 4u;
            uint brushPlusLeft = contentLeft + halfWidth - controlButtonWidth - 4u;
            uint zoomMinusLeft = contentLeft + halfWidth + 4u;
            uint zoomPlusLeft = contentLeft + contentWidth - controlButtonWidth - 4u;
            uint buttonLefts[4] = uint[4](brushMinusLeft, brushPlusLeft, zoomMinusLeft, zoomPlusLeft);
            for (uint button = 0u; button < 4u; ++button) {
                uint left = buttonLefts[button];
                uint right = left + controlButtonWidth;
                if (x >= left && x < right && y >= controlTop && y < controlTop + 26u) {
                    color = vec3(0.075, 0.105, 0.145);
                    if (borderPixel(x, y, left, controlTop, right, controlTop + 26u)) color *= 0.55;
                }
            }
            cursorText = cursorText || fixedPixel(pixel, ivec2(int(contentLeft + 53u), int(controlTop + 2u)), 1, 104u) ||
                numberPixel(pixel, ivec2(int(contentLeft + halfWidth / 2u - 8u), int(controlTop + 13u)), 1, renderPc.brushRadius) ||
                fixedPixel(pixel, ivec2(int(contentLeft + halfWidth + 53u), int(controlTop + 2u)), 1, 105u) ||
                numberPixel(pixel, ivec2(int(contentLeft + halfWidth + halfWidth / 2u - 8u), int(controlTop + 13u)), 1,
                            max(renderPc.gridWidth / max(renderPc.viewWidth, 1u), 1u));
            bool minusLeft = glyphPixel(pixel, ivec2(int(brushMinusLeft + 17u), int(controlTop + 6u)), 2, 45u);
            bool plusLeft = glyphPixel(pixel, ivec2(int(brushPlusLeft + 17u), int(controlTop + 6u)), 2, 43u);
            bool minusRight = glyphPixel(pixel, ivec2(int(zoomMinusLeft + 17u), int(controlTop + 6u)), 2, 45u);
            bool plusRight = glyphPixel(pixel, ivec2(int(zoomPlusLeft + 17u), int(controlTop + 6u)), 2, 43u);
            if (minusLeft || plusLeft || minusRight || plusRight) cursorText = true;
            if (cursorText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        uint cardTop = cursorBottom + 3u;
        uint actorPanel = actor.enabled != 0u ? 102u : 5u;
        uint cardBottom = renderPc.windowHeight > actorPanel + 5u
            ? renderPc.windowHeight - actorPanel - 5u : renderPc.windowHeight;
        ivec2 cursor = clamp(ivec2(renderPc.cursorX, renderPc.cursorY), ivec2(0),
                              ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
        Cell inspected = cellAt(cursor);
        uint cardMaterial = renderPc.inspectMode != 0u ? inspected.material :
            (renderPc.hoveredMaterial < renderPc.materialCount ? renderPc.hoveredMaterial : renderPc.selectedMaterial);
        cardMaterial = min(cardMaterial, renderPc.materialCount - 1u);
        if (y >= cardTop && y < cardBottom) {
            if (borderPixel(x, y, contentLeft, cardTop, contentLeft + contentWidth, cardBottom))
                color = vec3(0.13, 0.29, 0.43);
            if (renderPc.inspectMode != 0u && isHalfWater(inspected)) {
                text = text || fixedPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, 157u);
            } else if (renderPc.inspectMode != 0u && (inspected.aux & AUX_WET) != 0u &&
                       !isCellLiquid(inspected) && !isCellGas(inspected)) {
                text = text || fixedPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, 158u) ||
                       materialPixel(pixel, ivec2(int(contentLeft + 82u), int(cardTop + 9u)), 3, cardMaterial);
            } else {
                text = text || materialPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, cardMaterial);
            }
            if (renderPc.inspectMode != 0u) {
                uint phase = cellPhase(inspected);
                text = text || fixedPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 36u)), 2, 12u) ||
                       phasePixel(pixel, ivec2(int(contentLeft + 70u), int(cardTop + 36u)), 2, phase) ||
                       fixedPixel(pixel, ivec2(int(contentLeft + 190u), int(cardTop + 36u)), 2, 13u) ||
                       signedNumberPixel(pixel, ivec2(int(contentLeft + 238u), int(cardTop + 36u)), 2,
                                         inspected.temperature);
            }
            uint first = cardTop + (renderPc.inspectMode != 0u ? 58u : 38u);
            for (uint line = 0u; line < 10u; ++line) {
                uint lineY = first + line * 18u;
                if (lineY + 14u < cardBottom &&
                    cardPixel(pixel, ivec2(int(contentLeft + 10u), int(lineY)), 2, cardMaterial, line)) text = true;
            }
            if (text) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        if (actor.enabled != 0u) {
            uint top = cardBottom + 3u;
            if (y >= top) {
                color = vec3(0.032, 0.043, 0.058);
                bool actorText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(top + 8u)), 2, 45u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 48u), int(top + 8u)), 2, actor.health) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 116u), int(top + 8u)), 2, 46u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 156u), int(top + 8u)), 2, actor.oxygen) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 224u), int(top + 8u)), 2, 47u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 286u), int(top + 8u)), 2, actor.ammo) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(top + 34u)), 2, 60u) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 174u), int(top + 34u)), 2, 61u);
                if (actorText) color = vec3(0.94, 0.97, 1.0);
                outColor = vec4(color, 1.0);
                return;
            }
        }
        if (text) color = vec3(0.94, 0.97, 1.0);
        outColor = vec4(color, 1.0);
        return;
    }

    uint viewportRight = renderPc.viewportLeft + renderPc.viewportWidth;
    uint viewportBottom = renderPc.viewportTop + renderPc.viewportHeight;
    if (x < renderPc.viewportLeft || x >= viewportRight ||
        y < renderPc.viewportTop || y >= viewportBottom) {
        // Deliberate letterbox, not a clipped simulation tile.
        vec3 bar = vec3(0.018, 0.024, 0.034);
        if ((x + y) % 24u == 0u) bar += vec3(0.006);
        outColor = vec4(bar, 1.0);
        return;
    }
    uint simulationHeight = max(renderPc.viewportHeight, 1u);
    uint simulationX = x - renderPc.viewportLeft;
    uint simulationY = y - renderPc.viewportTop;
    uint gridX = min(renderPc.gridWidth - 1u, renderPc.viewOriginX +
                      simulationX * max(renderPc.viewWidth, 1u) / max(renderPc.viewportWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u, renderPc.viewOriginY +
                      simulationY * max(renderPc.viewHeight, 1u) / simulationHeight);
    ivec2 grid = ivec2(int(gridX), int(gridY));
    Cell cell = cellAt(grid);
    vec4 color = worldColor(cell, grid);

    if (renderPc.debugMode != 0u) {
        bool activeArea = sectionActiveAt(grid, renderPc.activeAreaX, renderPc.activeAreaY,
                                          renderPc.activeScopeMode);
        if (!activeArea) color.rgb *= 0.28;
        ivec2 activeLocal = ivec2(grid.x % ACTIVE_REGION_WIDTH_CELLS,
                                  grid.y % ACTIVE_REGION_HEIGHT_CELLS);
        if (activeArea && (activeLocal.x == 0 || activeLocal.y == 0))
            color.rgb = mix(color.rgb, vec3(0.18, 0.95, 1.00), 0.78);
        TileState tile = tileAt(grid);
        ChunkState chunk = chunkAt(grid);
        ivec2 local = ivec2(int(gridX & 7u), int(gridY & 7u));
        bool readableTileGrid = renderPc.viewportWidth / max(renderPc.viewWidth, 1u) >= 2u;
        if (readableTileGrid && (local.x == 0 || local.y == 0))
            color.rgb = mix(color.rgb, vec3(0.94, 0.98, 1.00), 0.28);
        ivec2 chunkLocal = ivec2(int(gridX & (CHUNK_CELL_SIZE - 1u)),
                                int(gridY & (CHUNK_CELL_SIZE - 1u)));
        if (chunkLocal.x == 0 || chunkLocal.y == 0)
            color.rgb = mix(color.rgb, vec3(0.01, 0.015, 0.025), 0.72);

        vec3 overlay = vec3(0.0);
        float alpha = 0.0;
        if (tileHas(tile, TILE_COLLAPSING) || tileHas(tile, TILE_DAMAGED)) {
            overlay = debugKeyColor(0u); alpha = 0.62;
        } else if (tileHas(tile, TILE_MEDIUM_BREAKUP)) {
            overlay = debugKeyColor(9u); alpha = 0.58;
        } else if (tileHas(tile, TILE_MACRO_MOVED)) {
            overlay = debugKeyColor(2u); alpha = 0.56;
        } else if (tileHas(tile, TILE_FINE_ACTIVE)) {
            overlay = debugKeyColor(3u); alpha = 0.48;
        } else if (tileHas(tile, TILE_STABLE) || tileHas(tile, TILE_CANDIDATE)) {
            overlay = debugKeyColor(1u); alpha = 0.50;
        } else if (tileHas(tile, TILE_SETTLED_MEDIUM)) {
            overlay = debugKeyColor(5u); alpha = 0.46;
        } else if (tileHas(tile, TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)) {
            overlay = debugKeyColor(4u); alpha = 0.44;
        } else if (tileHas(tile, TILE_MEDIUM_ENCLOSED)) {
            overlay = debugKeyColor(8u); alpha = 0.40;
        } else if (tileHas(tile, TILE_SLEEPING)) {
            overlay = debugKeyColor(6u); alpha = 0.36;
        } else if (tileHas(tile, TILE_ACTIVE)) {
            overlay = debugKeyColor(7u); alpha = 0.34;
        }
        float occupancyAlpha = max(0.28, float(tileOccupancy(tile)) / 64.0);
        color.rgb = mix(color.rgb, overlay, alpha * occupancyAlpha);

        vec3 chunkOverlay = chunkHas(chunk, CHUNK_DIRTY) ? vec3(1.00, 0.10, 0.04) :
            (chunkHas(chunk, CHUNK_SLEEPING) ? vec3(0.035, 0.10, 0.30)
                                             : vec3(0.05, 0.42, 0.90));
        color.rgb = mix(color.rgb, chunkOverlay, chunkHas(chunk, CHUNK_SLEEPING) ? 0.12 : 0.07);
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
            int radius = int(renderPc.brushRadius);
            bool cursorEdge = false;
            if (renderPc.brushShape == 0u) {
                int outer = radius * radius;
                int innerRadius = max(radius - 1, 0);
                cursorEdge = distanceSquared <= outer && distanceSquared >= innerRadius * innerRadius;
            } else if (renderPc.brushShape == 1u) {
                cursorEdge = max(abs(delta.x), abs(delta.y)) == radius;
            } else if (renderPc.brushShape == 2u) {
                cursorEdge = abs(delta.x) <= radius && abs(delta.y) <= 1;
            } else {
                cursorEdge = abs(delta.x) <= 1 && abs(delta.y) <= radius;
            }
            if (cursorEdge) color.rgb = vec3(1.0) - color.rgb;
        }
    }

    outColor = vec4(color.rgb, 1.0);
}
