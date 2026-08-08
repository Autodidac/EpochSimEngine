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
#include "material_appearance.glsl"

layout(location = 0) out vec4 outColor;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 2) readonly buffer SunlightBuffer { uint sunlight[]; };
layout(std430, binding = 3) readonly buffer ActorBuffer { ActorState actor; };
layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };
layout(std430, binding = 5) readonly buffer DebugStatsBuffer { uint debugStats[]; };
layout(std430, binding = 7) readonly buffer Chunks { ChunkState chunks[]; };
layout(std430, binding = 8) readonly buffer MapCells { Cell mapCells[]; };
layout(std430, binding = 9) readonly buffer DesignerCells { uint designerCells[]; };

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
    uint mapMode;
    uint cameraOriginX;
    uint cameraOriginY;
    uint cameraViewWidth;
    uint cameraViewHeight;
    uint mapViewportLeft;
    uint mapViewportTop;
    uint mapViewportWidth;
    uint mapViewportHeight;
    uint mapOriginX;
    uint mapOriginY;
    uint mapViewWidth;
    uint mapViewHeight;
    uint selectedInventorySlot;
    uint selectedWorkspace;
    uint renderFrame;
    uint worldTime;
    uint dayCycleSteps;
    uint designerFlags;
    uint blueprintFlags;
    uint framebufferWidth;
    uint framebufferHeight;
} renderPc;

uvec2 logicalWindowPixel() {
    vec2 logicalSize = vec2(max(renderPc.windowWidth, 1u),
                            max(renderPc.windowHeight, 1u));
    vec2 framebufferSize = vec2(max(renderPc.framebufferWidth, 1u),
                                max(renderPc.framebufferHeight, 1u));
    uvec2 pixel = uvec2(gl_FragCoord.xy * logicalSize / framebufferSize);
    return min(pixel, uvec2(max(renderPc.windowWidth, 1u) - 1u,
                            max(renderPc.windowHeight, 1u) - 1u));
}
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
    if (key == 0u) return vec3(1.00, 0.06, 0.04);       // damaged / collapsing
    if (key == 1u) return vec3(0.05, 0.78, 1.00);       // active
    if (key == 2u) return vec3(1.00, 0.08, 0.72);       // fine active
    if (key == 3u) return vec3(1.00, 0.45, 0.03);       // bulk moved
    if (key == 4u) return vec3(0.62, 0.18, 1.00);       // bulk ready
    if (key == 5u) return vec3(1.00, 0.88, 0.04);       // breakup to fine
    if (key == 6u) return vec3(0.08, 0.94, 0.30);       // settled
    if (key == 7u) return vec3(0.06, 0.74, 0.62);       // enclosed medium
    if (key == 8u) return vec3(0.025, 0.075, 0.22);     // sleeping
    return vec3(0.82, 0.88, 0.96);                      // stable / candidate
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

    const uint statCount = 46u;
    uint rowHeight = textScale == 2 ? 18u : 12u;
    uint headerHeight = textScale == 2 ? 24u : 15u;
    uint fixedLabels[statCount] = uint[statCount](
        1u, 174u, 175u, 176u, 177u, 178u, 179u, 143u, 137u,
        160u, 161u, 94u, 95u, 96u,
        144u, 145u,
        162u, 98u, 81u, 163u, 119u, 120u, 121u,
        164u, 115u, 114u, 122u,
        146u, 78u, 116u, 117u, 123u, 124u, 112u, 113u, 111u,
        149u, 150u, 151u, 152u, 153u, 154u, 155u, 156u, 110u, 82u);
    uint fixedValues[statCount] = uint[statCount](
        renderPc.framesPerSecond,
        renderPc.gridWidth,
        renderPc.gridHeight,
        renderPc.gridWidth * renderPc.gridHeight,
        renderPc.tileColumns,
        renderPc.tileRows,
        renderPc.tileColumns * renderPc.tileRows,
        renderPc.gridWidth * renderPc.gridHeight * 36u / (1024u * 1024u),
        renderPc.activeAreaCount,
        debugStats[STAT_SCOPE_CELLS],
        debugStats[STAT_ACTIVE_CELLS],
        debugStats[STAT_STRUCTURAL_CELLS],
        debugStats[STAT_LIQUID_CELLS],
        debugStats[STAT_GAS_CELLS],
        debugStats[STAT_MOVE_PAIR_TESTS],
        debugStats[STAT_CHUNK_SKIPPED_CELLS],
        debugStats[STAT_TOTAL_TILES],
        debugStats[STAT_ACTIVE_TILES],
        debugStats[STAT_SLEEPING_TILES],
        debugStats[STAT_UNCLASSIFIED_TILES],
        debugStats[STAT_FINE_TILES],
        debugStats[STAT_MACRO_TILES],
        debugStats[STAT_SETTLED_TILES],
        debugStats[STAT_TOTAL_CHUNKS],
        debugStats[STAT_ACTIVE_CHUNKS],
        debugStats[STAT_SLEEPING_CHUNKS],
        debugStats[STAT_DIRTY_CHUNKS],
        debugStats[STAT_MOVED_CELLS],
        debugStats[STAT_MOVE_SWAPS],
        debugStats[STAT_MACRO_TILE_MOVES],
        debugStats[STAT_MACRO_CELL_MOVES],
        debugStats[STAT_MACRO_GAS_TILES],
        debugStats[STAT_MACRO_LIQUID_TILES],
        debugStats[STAT_FINE_REPAIR_MOVES],
        debugStats[STAT_GAS_EXCESS_MOVES],
        debugStats[STAT_PLAYER_IMPULSES],
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
            textColor = debugStatColor(stat > 2u ? stat - 3u : stat);
        }
    }

    // Restrained visual grouping: resource pressure, hierarchy/activity, and
    // world events remain readable without surrounding every row with boxes.
    uint separatorYs[4] = uint[4](
        panelTop + headerHeight + 14u * rowHeight - 4u,
        panelTop + headerHeight + 23u * rowHeight - 4u,
        panelTop + headerHeight + 27u * rowHeight - 4u,
        panelTop + headerHeight + 36u * rowHeight - 4u);
    for (uint separator = 0u; separator < 4u; ++separator) {
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
    // Legend order matches the state precedence used by the world overlay.
    uint keyLabels[keyCount] = uint[keyCount](
        128u, 29u, 131u, 130u, 132u, 135u, 133u, 134u, 28u, 129u);
    uint keyColorMap[keyCount] = uint[keyCount](
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u);
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

bool mapOverlayPixel() {
    if (renderPc.mapMode == 0u || renderPc.mapViewportWidth == 0u ||
        renderPc.mapViewportHeight == 0u) return false;
    uvec2 pixel = logicalWindowPixel();
    return pixel.x >= renderPc.mapViewportLeft &&
           pixel.x < renderPc.mapViewportLeft + renderPc.mapViewportWidth &&
           pixel.y >= renderPc.mapViewportTop &&
           pixel.y < renderPc.mapViewportTop + renderPc.mapViewportHeight;
}

bool mapOverlayBorderPixel() {
    if (!mapOverlayPixel()) return false;
    uvec2 pixel = logicalWindowPixel();
    uint right = renderPc.mapViewportLeft + renderPc.mapViewportWidth;
    uint bottom = renderPc.mapViewportTop + renderPc.mapViewportHeight;
    return pixel.x < renderPc.mapViewportLeft + 2u || pixel.x + 2u >= right ||
           pixel.y < renderPc.mapViewportTop + 2u || pixel.y + 2u >= bottom;
}

const uint DESIGNER_GRID_COLUMNS = 64u;
const uint DESIGNER_GRID_ROWS = 32u;

uint designerPlacementMode() { return renderPc.designerFlags & 1u; }
uint designerBrushShape() { return (renderPc.designerFlags >> 1u) & 3u; }
uint designerMode() { return (renderPc.designerFlags >> 3u) & 1u; }
uint designerPane() { return (renderPc.designerFlags >> 4u) & 1u; }
uint inventoryPane() { return (renderPc.designerFlags >> 5u) & 1u; }
uint designerZoom() { return max((renderPc.designerFlags >> 8u) & 255u, 1u); }
uint designerBrushRadius() { return max((renderPc.designerFlags >> 16u) & 255u, 1u); }
bool blueprintSlotOccupied(uint slot) {
    return slot < 4u && (renderPc.blueprintFlags & (1u << slot)) != 0u;
}
uint selectedBlueprintSlot() { return (renderPc.blueprintFlags >> 4u) & 3u; }
bool blueprintPlacementActive() {
    return (renderPc.blueprintFlags & (1u << 6u)) != 0u &&
           blueprintSlotOccupied(selectedBlueprintSlot());
}
uint blueprintWidth() { return (renderPc.blueprintFlags >> 8u) & 0x7fu; }
uint blueprintHeight() { return (renderPc.blueprintFlags >> 16u) & 0x7fu; }
uint blueprintKind() { return (renderPc.blueprintFlags >> 24u) & 1u; }

vec4 designerGridColor(uint sampleX, uint sampleY, uint sampleWidth, uint sampleHeight) {
    uint zoom = min(designerZoom(), 4u);
    uint visibleColumns = max(8u, DESIGNER_GRID_COLUMNS / zoom);
    uint visibleRows = max(8u, DESIGNER_GRID_ROWS / zoom);
    uint originX = (DESIGNER_GRID_COLUMNS - visibleColumns) / 2u;
    uint originY = (DESIGNER_GRID_ROWS - visibleRows) / 2u;
    uint gridX = min(DESIGNER_GRID_COLUMNS - 1u,
                     originX + sampleX * visibleColumns / max(sampleWidth, 1u));
    uint gridY = min(DESIGNER_GRID_ROWS - 1u,
                     originY + sampleY * visibleRows / max(sampleHeight, 1u));
    uint material = min(designerCells[gridY * DESIGNER_GRID_COLUMNS + gridX],
                        renderPc.materialCount - 1u);
    vec3 color = material == MAT_EMPTY
        ? vec3(0.028, 0.038, 0.052)
        : materialColor(material, 0u, gridX * 4099u + gridY * 131u,
                        ivec2(int(gridX), int(gridY))).rgb;
    uint cellPixelWidth = max(sampleWidth / visibleColumns, 1u);
    uint cellPixelHeight = max(sampleHeight / visibleRows, 1u);
    bool cellEdge = cellPixelWidth >= 4u &&
        ((sampleX * visibleColumns) % max(sampleWidth, 1u) < visibleColumns ||
         (sampleY * visibleRows) % max(sampleHeight, 1u) < visibleRows);
    bool tileEdge = (gridX & 7u) == 0u || (gridY & 7u) == 0u;
    if (cellEdge) color *= 0.72;
    if (tileEdge) color = mix(color, vec3(0.24, 0.43, 0.58), 0.32);
    if (designerMode() != 0u) color = mix(color, vec3(0.08, 0.13, 0.19), 0.38);
    return vec4(color, 1.0);
}

Cell cellAt(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
    uint index = uint(p.y) * renderPc.gridWidth + uint(p.x);
    return mapOverlayPixel() ? mapCells[index] : cells[index];
}
TileState tileAt(ivec2 p) { return tiles[tileIndex(p, renderPc.gridWidth)]; }
ChunkState chunkAt(ivec2 p) { return chunks[chunkIndex(p, renderPc.gridWidth)]; }

vec3 backgroundColor(ivec2 grid) {
    float depth = float(grid.y) / float(max(renderPc.gridHeight, 1u));
    return mix(vec3(0.055, 0.105, 0.18), vec3(0.018, 0.024, 0.037), depth);
}

float daylightFactor() {
    float cycle = float(renderPc.worldTime % max(renderPc.dayCycleSteps, 1u)) /
                  float(max(renderPc.dayCycleSteps, 1u));
    // Reset begins at noon; simulation time then advances through sunset,
    // midnight, sunrise, and back to noon. Pausing freezes the phase.
    float sunArc = cos(cycle * 6.28318530718);
    return smoothstep(-0.22, 0.18, sunArc);
}

vec3 applyWorldLighting(vec3 color, Cell cell, ivec2 grid, bool mapSample) {
    uint index = uint(grid.y) * renderPc.gridWidth + uint(grid.x);
    float daylight = daylightFactor();
    float directSun = float(sunlight[index]) / 255.0;
    float ambient = mix(0.19, 0.62, daylight);
    float illumination = mapSample
        ? mix(0.48, 1.0, daylight)
        : clamp(ambient + directSun * daylight * 0.42, 0.16, 1.0);
    bool luminous = cell.material == MAT_LAVA || cell.material == MAT_FIRE ||
                    cell.material == MAT_LIGHTNING || cell.material == MAT_EMBER;
    if (luminous) illumination = max(illumination, 0.95);
    vec3 nightTint = vec3(0.54, 0.67, 0.88);
    return color * illumination * mix(nightTint, vec3(1.0), daylight);
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
    if (cell.material == MAT_ATMOSPHERE || cell.material == MAT_OXYGEN) {
        base.rgb = mix(vec3(0.22, 0.46, 0.70), base.rgb, 0.22);
        base.a = clamp(0.075 + densityField * 0.075 + cohesion * 0.32, 0.065, 0.22);
        return base;
    }
    float restrained = 0.08 + densityField * 0.13 + cohesion;
    if (cell.material == MAT_CARBON_DIOXIDE) restrained = 0.22 + densityField * 0.18 + cohesion;
    if (cell.material == MAT_HYDROGEN) restrained = 0.14 + densityField * 0.17 + cohesion;
    if (sameNeighbors == 0u) restrained *= 0.58;
    base.a = clamp(restrained, 0.045, 0.46);
    return base;
}

vec4 worldColor(Cell cell, ivec2 grid) {
    vec4 base = materialColor(cell.material, cell.age, cell.aux, grid);
    base = applyMaterialAppearance(cell, grid, renderPc.worldTime, base);
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
    ivec2 pixel = ivec2(logicalWindowPixel());
    uint x = uint(pixel.x);
    uint y = uint(pixel.y);

    uint sidebarWidth = min(renderPc.paletteHeight, renderPc.windowWidth);
    uint sidebarLeft = renderPc.windowWidth - sidebarWidth;
    if (renderPc.debugMode != 0u && sidebarWidth >= 300u && x >= sidebarLeft &&
        y >= renderPc.statusHeight) {
        vec3 debugColor = vec3(0.006, 0.010, 0.018);
        int debugScale = renderPc.windowHeight > renderPc.statusHeight + 560u ? 2 : 1;
        debugPanelPixel(pixel, x, y, sidebarLeft + 4u, renderPc.statusHeight + 4u,
                        renderPc.windowWidth - 4u, renderPc.windowHeight - 4u,
                        debugScale, debugColor);
        outColor = vec4(debugColor, 1.0);
        return;
    }
    if (x >= sidebarLeft) {
        vec3 color = vec3(0.025, 0.034, 0.048);
        uint localX = x - sidebarLeft;
        if (localX < 2u) color = vec3(0.14, 0.23, 0.32);
        bool text = false;
        uint rowLeft = sidebarLeft + 8u;
        uint rowWidth = max(sidebarWidth - 16u, 1u);
        uint tabWidth = max(rowWidth / 4u, 1u);
        uint tabLabels[4] = uint[4](166u, 167u, 168u, 169u);
        for (uint tab = 0u; tab < 4u; ++tab) {
            uint left = rowLeft + tab * tabWidth;
            uint right = tab == 3u ? sidebarLeft + sidebarWidth - 8u : left + tabWidth;
            if (x >= left && x < right && y >= 4u && y < 28u) {
                color = tab == renderPc.selectedWorkspace ? vec3(0.16, 0.39, 0.58)
                                                          : vec3(0.055, 0.085, 0.12);
                if (borderPixel(x, y, left, 4u, right, 28u)) color *= 0.55;
            }
            uint length = fixedTextLength(tabLabels[tab]);
            int scale = int(right - left) >= int(length * 6u + 6u) ? 1 : 1;
            int labelWidth = int(length) * 6 * scale - scale;
            if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2, 12), scale,
                           tabLabels[tab])) text = true;
        }
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

        // Scene files/navigation.
        uint rowGap = 4u;
        uint sceneWidth = max((rowWidth - rowGap * 3u) / 4u, 1u);
        uint sceneIds[4] = uint[4](41u, 42u, 65u, 66u);
        for (uint i = 0u; i < 4u; ++i) {
            uint left = rowLeft + i * (sceneWidth + rowGap);
            uint right = i == 3u ? sidebarLeft + sidebarWidth - 8u : left + sceneWidth;
            if (x >= left && x < right && y >= 70u && y < 98u) {
                color = vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 70u, right, 98u)) color *= 0.55;
                uint length = fixedTextLength(sceneIds[i]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             84 - (7 * scale) / 2), scale, sceneIds[i]))
                    color = vec3(0.96);
            }
        }

        // Simulation actions: RESET and PAUSE/RUN are equal-size neighbors.
        uint actionWidth = max((rowWidth - rowGap) / 2u, 1u);
        uint actionIds[2] = uint[2](6u, renderPc.paused != 0u ? 3u : 2u);
        for (uint action = 0u; action < 2u; ++action) {
            uint left = rowLeft + action * (actionWidth + rowGap);
            uint right = action == 1u ? sidebarLeft + sidebarWidth - 8u : left + actionWidth;
            if (x >= left && x < right && y >= 102u && y < 132u) {
                bool enabled = action == 1u && renderPc.paused != 0u;
                color = enabled ? vec3(0.20, 0.38, 0.20) :
                    (action == 0u ? vec3(0.32, 0.16, 0.08) : vec3(0.075, 0.105, 0.145));
                if (borderPixel(x, y, left, 102u, right, 132u)) color *= 0.55;
                uint length = fixedTextLength(actionIds[action]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             117 - (7 * scale) / 2), scale,
                               actionIds[action])) color = vec3(0.97);
            }
        }

        // View/input modes.
        uint viewWidth = max((rowWidth - rowGap * 3u) / 4u, 1u);
        bool playerScene = renderPc.selectedScene == 6u ||
                           renderPc.selectedScene == 7u ||
                           renderPc.selectedScene == 8u;
        uint viewIds[4] = uint[4](
            renderPc.miningMode != 0u ? 8u : 7u,
            playerScene && renderPc.cameraControls == 0u ? 141u : 140u,
            0u,
            9u);
        for (uint control = 0u; control < 4u; ++control) {
            uint left = rowLeft + control * (viewWidth + rowGap);
            uint right = control == 3u ? sidebarLeft + sidebarWidth - 8u : left + viewWidth;
            if (x >= left && x < right && y >= 136u && y < 164u) {
                bool enabled = (control == 1u && renderPc.cameraControls != 0u) ||
                               (control == 2u && renderPc.mapMode != 0u) ||
                               (control == 3u && renderPc.debugMode != 0u);
                color = enabled ? vec3(0.14, 0.31, 0.45) : vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 136u, right, 164u)) color *= 0.55;
            }
            bool labelHit = false;
            if (control == 2u) {
                int scale = int(right - left) >= 42 ? 2 : 1;
                int labelWidth = 17 * scale;
                ivec2 origin = ivec2(int(left + right) / 2 - labelWidth / 2,
                                     150 - (7 * scale) / 2);
                labelHit = glyphPixel(pixel, origin, scale, 77u) ||
                           glyphPixel(pixel, origin + ivec2(6 * scale, 0), scale, 65u) ||
                           glyphPixel(pixel, origin + ivec2(12 * scale, 0), scale, 80u);
            } else {
                uint length = fixedTextLength(viewIds[control]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                labelHit = fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2,
                          150 - (7 * scale) / 2), scale, viewIds[control]);
            }
            if (labelHit) color = vec3(0.97);
        }

        // Primary tools are always visible and strongly differentiated.
        uint utilityWidth = max((rowWidth - rowGap * 2u) / 3u, 1u);
        uint utilityLabels[3] = uint[3](67u, 159u, renderPc.selectedWorkspace == 3u ? 160u : 108u);
        for (uint button = 0u; button < 3u; ++button) {
            uint left = rowLeft + button * (utilityWidth + rowGap);
            uint right = button == 2u ? sidebarLeft + sidebarWidth - 8u : left + utilityWidth;
            if (x >= left && x < right && y >= 168u && y < 202u) {
                if (button == 0u) {
                    color = renderPc.selectedMaterial == MAT_ATMOSPHERE
                        ? vec3(0.10, 0.50, 0.76) : vec3(0.07, 0.25, 0.38);
                } else if (button == 1u) {
                    color = renderPc.selectedMaterial == MAT_EMPTY
                        ? vec3(0.72, 0.14, 0.18) : vec3(0.30, 0.055, 0.07);
                } else {
                    color = vec3(0.10, 0.42, 0.20);
                }
                if (borderPixel(x, y, left, 168u, right, 202u)) color *= 0.55;
                uint length = fixedTextLength(utilityLabels[button]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             185 - (7 * scale) / 2), scale,
                               utilityLabels[button])) color = vec3(1.0);
            }
        }

        uint contentLeft = sidebarLeft + 5u;
        uint contentWidth = max(sidebarWidth - 10u, 1u);
        uint groupTop = renderPc.statusHeight + 5u;
        uint groupRows = max((renderPc.groupCount + 1u) / 2u, 1u);
        uint groupCellWidth = max(contentWidth / 2u, 1u);
        uint groupCellHeight = max(renderPc.groupTabsHeight / groupRows, 1u);
        if ((renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 3u) &&
            y >= groupTop && y < groupTop + renderPc.groupTabsHeight &&
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
        const uint palettePanelHeight = 124u;
        const uint igniteAirGroup = 4u;
        const uint igniteAirTextId = 165u;
        uint materialSlotCount = groupMaterialCount(renderPc.selectedGroup);
        uint slotCount = max(materialSlotCount +
            (renderPc.selectedGroup == igniteAirGroup ? 1u : 0u), 1u);
        uint slotRows = max((slotCount + 1u) / 2u, 1u);
        uint cellWidth = max(contentWidth / 2u, 1u);
        uint cellHeight = max(palettePanelHeight / slotRows, 1u);
        if ((renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 3u) &&
            y >= paletteTop && y < paletteTop + palettePanelHeight &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            uint column = min((x - contentLeft) / cellWidth, 1u);
            uint row = min((y - paletteTop) / cellHeight, slotRows - 1u);
            uint slot = row * 2u + column;
            if (slot < slotCount) {
                bool igniteAirAction = renderPc.selectedGroup == igniteAirGroup &&
                                       slot == materialSlotCount;
                uint left = contentLeft + column * cellWidth;
                uint right = column == 1u ? contentLeft + contentWidth : left + cellWidth;
                uint top = paletteTop + row * cellHeight;
                uint bottom = min(paletteTop + palettePanelHeight, top + cellHeight);
                if (igniteAirAction) {
                    color = vec3(0.48, 0.16, 0.035);
                    if (borderPixel(x, y, left, top, right, bottom)) color *= 0.5;
                    uint length = fixedTextLength(igniteAirTextId);
                    int scale = int(right - left) >= int(length * 12u + 8u) ? 2 : 1;
                    int width = int(length) * 6 * scale - scale;
                    if (fixedPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                                int(top + bottom) / 2 - (7 * scale) / 2),
                                   scale, igniteAirTextId)) color = vec3(1.0, 0.92, 0.72);
                } else {
                    uint material = groupMaterial(renderPc.selectedGroup, slot);
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
            }
            outColor = vec4(color, 1.0);
            return;
        }

        uint settingsLightTop = groupTop;
        uint settingsLightBottom = settingsLightTop + 78u;
        if (renderPc.selectedWorkspace == 2u &&
            y >= settingsLightTop && y < settingsLightBottom &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, settingsLightTop,
                            contentLeft + contentWidth, settingsLightBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool lightingText = fixedPixel(
                pixel, ivec2(int(contentLeft + 8u), int(settingsLightTop + 7u)), 2, 187u) ||
                fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(settingsLightTop + 48u)), 1, 188u);
            uint barLeft = contentLeft + 8u;
            uint barRight = contentLeft + contentWidth - 8u;
            uint barTop = settingsLightTop + 31u;
            uint barBottom = barTop + 10u;
            float cycle = float(renderPc.worldTime % max(renderPc.dayCycleSteps, 1u)) /
                          float(max(renderPc.dayCycleSteps, 1u));
            uint marker = barLeft + uint(cycle * float(max(barRight - barLeft - 1u, 1u)));
            if (x >= barLeft && x < barRight && y >= barTop && y < barBottom) {
                color = mix(vec3(0.025, 0.045, 0.10), vec3(0.90, 0.68, 0.26), daylightFactor());
                if (x + 1u >= marker && x <= marker + 1u) color = vec3(1.0);
            }
            uint sunlightLeft = contentLeft + 78u;
            uint sunlightRight = contentLeft + contentWidth - 8u;
            uint sunlightTop = settingsLightTop + 49u;
            uint sunlightBottom = sunlightTop + 15u;
            if (x >= sunlightLeft && x < sunlightRight &&
                y >= sunlightTop && y < sunlightBottom) {
                uint fillRight = sunlightLeft + uint(daylightFactor() *
                    float(max(sunlightRight - sunlightLeft, 1u)));
                color = x < fillRight ? vec3(0.90, 0.72, 0.28) : vec3(0.055, 0.07, 0.09);
            }
            if (lightingText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        uint keymapTop = (renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 3u)
            ? paletteTop + palettePanelHeight + 3u
            : (renderPc.selectedWorkspace == 2u ? settingsLightBottom + 3u : groupTop);
        uint keymapBottom = keymapTop + 124u;
        if (renderPc.selectedWorkspace == 3u &&
            y >= keymapTop && y < keymapBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, keymapTop, contentLeft + contentWidth, keymapBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool designerText = fixedPixel(
                pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 6u)), 2, 183u);
            uint halfWidth = max(contentWidth / 2u, 1u);
            uint modeTop = keymapTop + 25u;
            uint modeIds[2] = uint[2](184u, 185u);
            for (uint mode = 0u; mode < 2u; ++mode) {
                uint left = contentLeft + mode * halfWidth;
                uint right = mode == 1u ? contentLeft + contentWidth : left + halfWidth;
                if (x >= left && x < right && y >= modeTop && y < modeTop + 28u) {
                    color = mode == designerMode() ? vec3(0.14, 0.30, 0.45)
                                                   : vec3(0.055, 0.07, 0.09);
                    if (borderPixel(x, y, left, modeTop, right, modeTop + 28u)) color *= 0.55;
                }
                designerText = designerText || fixedPixel(
                    pixel, ivec2(int(left + 8u), int(modeTop + 8u)), 1, modeIds[mode]);
            }
            uint paneTop = keymapTop + 59u;
            uint paneIds[2] = uint[2](166u, 181u);
            for (uint pane = 0u; pane < 2u; ++pane) {
                uint left = contentLeft + pane * halfWidth;
                uint right = pane == 1u ? contentLeft + contentWidth : left + halfWidth;
                if (x >= left && x < right && y >= paneTop && y < paneTop + 28u) {
                    color = pane == designerPane() ? vec3(0.12, 0.25, 0.37)
                                                   : vec3(0.048, 0.064, 0.086);
                    if (borderPixel(x, y, left, paneTop, right, paneTop + 28u)) color *= 0.55;
                }
                designerText = designerText || fixedPixel(
                    pixel, ivec2(int(left + 8u), int(paneTop + 8u)), 1, paneIds[pane]);
            }
            uint slotsTop = keymapTop + 92u;
            uint slotGap = 4u;
            uint slotWidth = max((contentWidth - slotGap * 3u) / 4u, 1u);
            if (designerPane() == 1u) {
                for (uint slot = 0u; slot < 4u; ++slot) {
                    uint left = contentLeft + slot * (slotWidth + slotGap);
                    uint right = slot == 3u ? contentLeft + contentWidth : left + slotWidth;
                    bool occupied = blueprintSlotOccupied(slot);
                    bool selected = slot == selectedBlueprintSlot();
                    if (x >= left && x < right && y >= slotsTop && y < keymapBottom - 5u) {
                        color = selected
                            ? (occupied ? vec3(0.10, 0.34, 0.38) : vec3(0.13, 0.16, 0.20))
                            : (occupied ? vec3(0.075, 0.18, 0.22) : vec3(0.044, 0.059, 0.080));
                        if (borderPixel(x, y, left, slotsTop, right, keymapBottom - 5u))
                            color = selected ? vec3(0.28, 0.78, 0.82)
                                             : vec3(0.12, 0.20, 0.28);
                    }
                    designerText = designerText || fixedPixel(
                        pixel, ivec2(int(left + 5u), int(slotsTop + 4u)), 1,
                        occupied ? 181u : 182u) ||
                        numberPixel(pixel, ivec2(int(right - 11u), int(slotsTop + 14u)),
                                    1, slot + 1u);
                }
            }
            if (designerText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }
        if ((renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 2u) &&
            y >= keymapTop && y < keymapBottom && x >= contentLeft && x < contentLeft + contentWidth) {
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
        uint cursorBottom = cursorTop + 112u;
        if ((renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 3u) &&
            y >= cursorTop && y < cursorBottom && x >= contentLeft && x < contentLeft + contentWidth) {
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
                    uint activePlacement = renderPc.selectedWorkspace == 3u
                        ? designerPlacementMode() : renderPc.placementMode;
                    color = mode == activePlacement ? vec3(0.14, 0.30, 0.45)
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
                    uint activeShape = renderPc.selectedWorkspace == 3u
                        ? designerBrushShape() : renderPc.brushShape;
                    color = shape == activeShape ? vec3(0.14, 0.30, 0.45) : vec3(0.055, 0.07, 0.09);
                    if (borderPixel(x, y, left, shapeTop, right, shapeTop + 24u)) color *= 0.55;
                }
                int labelWidth = int(fixedTextLength(shapeIds[shape])) * 6 - 1;
                cursorText = cursorText || fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2, int(shapeTop + 8u)), 1, shapeIds[shape]);
            }
            uint controlTop = cursorTop + 85u;
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
                if (x >= left && x < right && y >= controlTop && y < controlTop + 24u) {
                    color = vec3(0.075, 0.105, 0.145);
                    if (borderPixel(x, y, left, controlTop, right, controlTop + 24u)) color *= 0.55;
                }
            }
            cursorText = cursorText || fixedPixel(pixel, ivec2(int(contentLeft + 53u), int(controlTop + 2u)), 1, 104u) ||
                numberPixel(pixel, ivec2(int(contentLeft + halfWidth / 2u - 8u), int(controlTop + 13u)), 1,
                            renderPc.selectedWorkspace == 3u ? designerBrushRadius() : renderPc.brushRadius) ||
                fixedPixel(pixel, ivec2(int(contentLeft + halfWidth + 53u), int(controlTop + 2u)), 1, 105u) ||
                numberPixel(pixel, ivec2(int(contentLeft + halfWidth + halfWidth / 2u - 8u), int(controlTop + 13u)), 1,
                            renderPc.selectedWorkspace == 3u ? designerZoom() :
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

        uint cardTop = (renderPc.selectedWorkspace == 1u || renderPc.selectedWorkspace == 3u)
            ? cursorBottom + 3u
            : (renderPc.selectedWorkspace == 2u ? keymapBottom + 3u : groupTop + 178u);
        uint cardBottom = renderPc.windowHeight > 5u
            ? renderPc.windowHeight - 5u : renderPc.windowHeight;
        if (renderPc.selectedWorkspace == 3u) {
            uint cardHeight = cardBottom > cardTop ? cardBottom - cardTop : 1u;
            uint designerGridHeight = min(contentWidth / 2u, max(cardHeight / 2u, 1u));
            uint designerGridBottom = min(cardTop + designerGridHeight, cardBottom);
            if (y >= cardTop && y < designerGridBottom) {
                vec4 designerColor = designerGridColor(
                    x - contentLeft, y - cardTop, max(contentWidth, 1u),
                    max(designerGridBottom - cardTop, 1u));
                if (borderPixel(x, y, contentLeft, cardTop,
                                contentLeft + contentWidth, designerGridBottom))
                    designerColor.rgb = vec3(0.20, 0.42, 0.58);
                outColor = designerColor;
                return;
            }
            cardTop = min(designerGridBottom + 3u, cardBottom);
        }
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

        if (renderPc.selectedWorkspace == 0u && y >= groupTop && y < groupTop + 175u) {
            color = vec3(0.032, 0.043, 0.058);
            bool inventoryText = fixedPixel(
                pixel, ivec2(int(contentLeft + 8u), int(groupTop + 6u)), 2, 166u);
            uint tabTop = groupTop + 25u;
            uint tabBottom = tabTop + 28u;
            uint halfWidth = contentWidth / 2u;
            for (uint tab = 0u; tab < 2u; ++tab) {
                uint left = contentLeft + tab * halfWidth;
                uint right = tab == 1u ? contentLeft + contentWidth : left + halfWidth;
                bool tabSelected = inventoryPane() == tab;
                if (x >= left && x < right && y >= tabTop && y < tabBottom)
                    color = tabSelected ? vec3(0.11, 0.24, 0.34) : vec3(0.050, 0.068, 0.090);
                if (borderPixel(x, y, left, tabTop, right, tabBottom))
                    color = tabSelected ? vec3(0.22, 0.50, 0.66) : vec3(0.10, 0.16, 0.22);
                inventoryText = inventoryText || fixedPixel(
                    pixel, ivec2(int(left + 10u), int(tabTop + 7u)), 1,
                    tab == 0u ? 166u : 181u);
            }
            uint paneTop = tabBottom + 3u;
            if (inventoryPane() == 1u) {
                uint slotGap = 5u;
                uint slotWidth = max((contentWidth - slotGap) / 2u, 1u);
                uint slotHeight = 52u;
                for (uint slot = 0u; slot < 4u; ++slot) {
                    uint column = slot % 2u;
                    uint row = slot / 2u;
                    uint left = contentLeft + column * (slotWidth + slotGap);
                    uint right = column == 1u ? contentLeft + contentWidth : left + slotWidth;
                    uint top = paneTop + row * (slotHeight + slotGap);
                    uint bottom = top + slotHeight;
                    bool occupied = blueprintSlotOccupied(slot);
                    bool selected = slot == selectedBlueprintSlot();
                    if (x >= left && x < right && y >= top && y < bottom) {
                        color = selected
                            ? (occupied ? vec3(0.10, 0.34, 0.38) : vec3(0.12, 0.15, 0.19))
                            : (occupied ? vec3(0.065, 0.16, 0.20) : vec3(0.040, 0.054, 0.072));
                        if (borderPixel(x, y, left, top, right, bottom))
                            color = selected ? vec3(0.28, 0.78, 0.82)
                                             : vec3(0.10, 0.16, 0.22);
                    }
                    inventoryText = inventoryText || fixedPixel(
                        pixel, ivec2(int(left + 8u), int(top + 7u)), 1,
                        occupied ? 181u : 182u) ||
                        numberPixel(pixel, ivec2(int(left + 8u), int(top + 27u)),
                                    1, slot + 1u);
                }
            } else {
                uint slotGap = 5u;
                uint slotWidth = max((contentWidth - slotGap) / 2u, 1u);
                uint slotHeight = 52u;
                for (uint slot = 0u; slot < 4u; ++slot) {
                    uint column = slot % 2u;
                    uint row = slot / 2u;
                    uint left = contentLeft + column * (slotWidth + slotGap);
                    uint right = column == 1u ? contentLeft + contentWidth : left + slotWidth;
                    uint top = paneTop + row * (slotHeight + slotGap);
                    uint bottom = top + slotHeight;
                    bool selected = renderPc.selectedInventorySlot == slot;
                    if (x >= left && x < right && y >= top && y < bottom) {
                        color = selected ? vec3(0.11, 0.24, 0.34) : vec3(0.045, 0.062, 0.084);
                        if (borderPixel(x, y, left, top, right, bottom))
                            color = selected ? vec3(0.22, 0.50, 0.66) : vec3(0.12, 0.20, 0.28);
                    }
                    uint slotMaterial = slot == 0u ? MAT_IRON :
                        (slot == 1u ? MAT_GOLD : (slot == 2u ? MAT_COPPER : MAT_ALUMINUM));
                    uint slotCount = slot == 0u ? actor.iron :
                        (slot == 1u ? actor.gold : (slot == 2u ? actor.copper : actor.aluminum));
                    inventoryText = inventoryText ||
                        materialPixel(pixel, ivec2(int(left + 8u), int(top + 7u)), 1, slotMaterial) ||
                        numberPixel(pixel, ivec2(int(left + 8u), int(top + 28u)), 1, slotCount);
                }
            }
            if (inventoryText) color = vec3(0.94, 0.97, 1.0);
            outColor = vec4(color, 1.0);
            return;
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
    bool mapSample = mapOverlayPixel();
    uint sampleLeft = mapSample ? renderPc.mapViewportLeft : renderPc.viewportLeft;
    uint sampleTop = mapSample ? renderPc.mapViewportTop : renderPc.viewportTop;
    uint sampleWidth = max(mapSample ? renderPc.mapViewportWidth : renderPc.viewportWidth, 1u);
    uint sampleHeight = max(mapSample ? renderPc.mapViewportHeight : renderPc.viewportHeight, 1u);
    uint sampleX = x - sampleLeft;
    uint sampleY = y - sampleTop;
    uint sampleOriginX = mapSample ? renderPc.mapOriginX : renderPc.viewOriginX;
    uint sampleOriginY = mapSample ? renderPc.mapOriginY : renderPc.viewOriginY;
    uint sampleViewWidth = max(mapSample ? renderPc.mapViewWidth : renderPc.viewWidth, 1u);
    uint sampleViewHeight = max(mapSample ? renderPc.mapViewHeight : renderPc.viewHeight, 1u);
    uint gridX = min(renderPc.gridWidth - 1u, sampleOriginX +
                      sampleX * sampleViewWidth / sampleWidth);
    uint gridY = min(renderPc.gridHeight - 1u, sampleOriginY +
                      sampleY * sampleViewHeight / sampleHeight);
    ivec2 grid = ivec2(int(gridX), int(gridY));
    Cell cell = cellAt(grid);
    vec4 color = worldColor(cell, grid);
    color.rgb = applyWorldLighting(color.rgb, cell, grid, mapSample);

    if (renderPc.debugMode != 0u || mapSample) {
        bool activeArea = sectionActiveAt(grid, renderPc.activeAreaX, renderPc.activeAreaY,
                                          renderPc.activeScopeMode);
        bool mediumCell = isCellGas(cell) || isCellLiquid(cell) || isHalfWater(cell);
        if (!activeArea) color.rgb *= mapSample ? 0.82 : 0.52;
        ivec2 activeLocal = ivec2(grid.x % ACTIVE_REGION_WIDTH_CELLS,
                                  grid.y % ACTIVE_REGION_HEIGHT_CELLS);
        if (activeArea && (activeLocal.x == 0 || activeLocal.y == 0)) {
            float boundaryAlpha = mapSample ? 0.22 : 0.58;
            color.rgb = mix(color.rgb, vec3(0.18, 0.95, 1.00), boundaryAlpha);
        }
        if (renderPc.debugMode != 0u && !mapSample) {
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
                overlay = debugKeyColor(0u); alpha = 0.72;
            } else if (tileHas(tile, TILE_FINE_ACTIVE)) {
                overlay = debugKeyColor(2u); alpha = 0.64;
            } else if (tileHas(tile, TILE_MACRO_MOVED)) {
                overlay = debugKeyColor(3u); alpha = 0.62;
            } else if (tileHas(tile, TILE_MEDIUM_BREAKUP) &&
                       !tileHas(tile, TILE_SLEEPING)) {
                overlay = debugKeyColor(5u); alpha = 0.60;
            } else if (tileHas(tile, TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)) {
                overlay = debugKeyColor(4u); alpha = 0.56;
            } else if (tileHas(tile, TILE_SETTLED_MEDIUM)) {
                overlay = debugKeyColor(6u); alpha = 0.52;
            } else if (tileHas(tile, TILE_MEDIUM_ENCLOSED)) {
                overlay = debugKeyColor(7u); alpha = 0.48;
            } else if (tileHas(tile, TILE_SLEEPING)) {
                overlay = debugKeyColor(8u); alpha = 0.46;
            } else if (tileHas(tile, TILE_ACTIVE)) {
                overlay = debugKeyColor(1u); alpha = 0.44;
            } else if (tileHas(tile, TILE_STABLE) || tileHas(tile, TILE_CANDIDATE)) {
                overlay = debugKeyColor(9u); alpha = 0.38;
            }
            bool stateEdge = local.x <= 1 || local.y <= 1 || local.x >= 6 || local.y >= 6;
            if (mapSample) {
                alpha *= mediumCell ? (stateEdge ? 0.055 : 0.0) : 0.30;
                if (!mediumCell && !stateEdge) alpha *= 0.16;
            } else if (mediumCell) {
                alpha *= stateEdge ? 0.16 : 0.0;
            } else {
                // Preserve the actual material identity; hierarchy state is an edge key.
                alpha *= stateEdge ? 0.82 : 0.08;
            }
            float occupancyAlpha = max(0.28, float(tileOccupancy(tile)) / 64.0);
            color.rgb = mix(color.rgb, overlay, alpha * occupancyAlpha);

            vec3 chunkOverlay = chunkHas(chunk, CHUNK_DIRTY) ? vec3(1.00, 0.10, 0.04) :
                (chunkHas(chunk, CHUNK_SLEEPING) ? vec3(0.035, 0.10, 0.30)
                                                 : vec3(0.05, 0.42, 0.90));
            float chunkAlpha = mapSample
                ? (mediumCell ? 0.0 : 0.045)
                : (mediumCell ? 0.0 : (chunkHas(chunk, CHUNK_SLEEPING) ? 0.10 : 0.06));
            color.rgb = mix(color.rgb, chunkOverlay, chunkAlpha);

        }

        if (mapSample) {
            uint cameraRight = renderPc.cameraOriginX + renderPc.cameraViewWidth;
            uint cameraBottom = renderPc.cameraOriginY + renderPc.cameraViewHeight;
            bool inCamera = gridX >= renderPc.cameraOriginX && gridX < cameraRight &&
                            gridY >= renderPc.cameraOriginY && gridY < cameraBottom;
            bool cameraEdge = inCamera &&
                (gridX == renderPc.cameraOriginX || gridX + 1u == cameraRight ||
                 gridY == renderPc.cameraOriginY || gridY + 1u == cameraBottom);
            if (cameraEdge) color.rgb = vec3(1.00, 0.92, 0.18);
        }
    }

    if (mapSample && mapOverlayBorderPixel()) {
        color.rgb = vec3(0.68, 0.82, 0.92);
    }

    if (!mapSample && actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {
        vec2 toolOrigin = vec2(float(actor.x), float(actor.y - 4));
        vec2 toolHit = vec2(float(actor.hitX), float(actor.hitY));
        vec2 ray = toolHit - toolOrigin;
        float raySquared = max(dot(ray, ray), 0.0001);
        vec2 samplePoint = vec2(grid) + vec2(0.5);
        float along = clamp(dot(samplePoint - toolOrigin, ray) / raySquared, 0.0, 1.0);
        float beamDistance = segmentDistance(samplePoint, toolOrigin, toolHit);
        uint beamStep = uint(floor(along * sqrt(raySquared)));
        uint burstHash = hash32(uint(grid.x) * 2246822519u ^ uint(grid.y) * 3266489917u ^
                      renderPc.worldTime * 668265263u ^ actor.shotTimer * 374761393u);
        bool tinyDash = ((beamStep + actor.shotTimer) % 7u) < 2u;
        if (beamDistance < 0.46 && tinyDash && (burstHash & 3u) != 0u)
  color = actor.shotTimer > 4u ? vec4(1.0, 0.28, 0.68, 1.0)
                               : vec4(1.0, 0.82, 0.20, 1.0);
        ivec2 impactDelta = grid - ivec2(actor.hitX, actor.hitY);
        int impactDistance = impactDelta.x * impactDelta.x + impactDelta.y * impactDelta.y;
        if (impactDistance >= 1 && impactDistance <= 8 && ((burstHash >> 3u) & 3u) == 0u)
  color = vec4(1.0, 0.96, 0.72, 1.0);
    }

    if (renderPc.selectedWorkspace == 1u &&
        renderPc.inspectMode == 0u && !mapSample) {
        ivec2 delta = grid - ivec2(renderPc.cursorX, renderPc.cursorY);
        int distanceSquared = delta.x * delta.x + delta.y * delta.y;
        if (blueprintPlacementActive() &&
            blueprintWidth() > 0u && blueprintHeight() > 0u) {
            int width = int(blueprintWidth());
            int height = int(blueprintHeight());
            int left = renderPc.cursorX - width / 2;
            int top = renderPc.cursorY - height / 2;
            int right = left + width;
            int bottom = top + height;
            bool inside = grid.x >= left && grid.x < right &&
                          grid.y >= top && grid.y < bottom;
            bool edge = inside &&
                (grid.x == left || grid.x + 1 == right ||
                 grid.y == top || grid.y + 1 == bottom);
            bool valid = left >= 0 && top >= 0 &&
                         right <= int(renderPc.gridWidth) &&
                         bottom <= int(renderPc.gridHeight);
            if (inside)
                color.rgb = mix(color.rgb,
                    blueprintKind() == 0u ? vec3(0.15, 0.95, 0.88)
                                          : vec3(0.45, 0.72, 1.00),
                    edge ? (valid ? 0.92 : 0.82) : 0.12);
            if (edge && !valid) color.rgb = vec3(1.00, 0.28, 0.22);
        } else if (renderPc.miningMode != 0u) {
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
