#ifndef SANDHYBRID_DEBUG_GEOMETRY_GLSL
#define SANDHYBRID_DEBUG_GEOMETRY_GLSL

// Scalar-only helpers are also compiled by the CPU geometry contract. Keep
// sample capacity separate from executed work: sleeping cells are not a count
// of movement pairs rejected, nor cells omitted by every simulation pass.
uint debugOverlapLength(uint firstBegin, uint firstEnd, uint secondBegin, uint secondEnd) {
    uint begin = max(firstBegin, secondBegin);
    uint end = min(firstEnd, secondEnd);
    return end > begin ? end - begin : 0u;
}

uint debugSleepingChunkSampleCells(uint chunkX, uint chunkY, uint chunkSize,
                                  uint worldWidth, uint worldHeight,
                                  uint sampleX, uint sampleY, uint sampleWidth, uint sampleHeight) {
    uint beginX = chunkX * chunkSize;
    uint beginY = chunkY * chunkSize;
    uint width = debugOverlapLength(beginX, min(beginX + chunkSize, worldWidth),
                                   sampleX, sampleX + sampleWidth);
    uint height = debugOverlapLength(beginY, min(beginY + chunkSize, worldHeight),
                                    sampleY, sampleY + sampleHeight);
    return width * height;
}

uint debugLegendSwatchSize(uint scale) { return scale == 2u ? 24u : 18u; }
// Give all eight source cells an equal number of pixels. The 18px swatch
// has a one-pixel margin; the 24px swatch uses its full area.
uint debugLegendMarkerCell(uint pixel, uint swatchSize) {
    uint pitch = max(swatchSize / 8u, 1u);
    uint margin = (swatchSize - min(swatchSize, pitch * 8u)) / 2u;
    return pixel >= margin && pixel < margin + pitch * 8u
        ? (pixel - margin) / pitch : 8u;
}

// Both the legend and world use this exact tile-cell mask.
bool debugStateMarkerPixel(uint state, uint x, uint y) {
    if (x >= 8u || y >= 8u) return false;
    bool top = y == 0u;
    bool left = x == 0u;
    bool right = x == 7u;
    bool topLeft = (top && x <= 2u) || (left && y <= 2u);
    bool topRight = (top && x >= 5u) || (right && y <= 2u);
    bool lowerLeft = left && y >= 5u;
    bool lowerRight = right && y >= 5u;
    if (state == 0u) return top || left || right;                 // damaged
    if (state == 1u) return topLeft;                              // active
    if (state == 2u) return top && (x == 1u || x == 3u || x == 5u); // fine
    if (state == 3u) return lowerRight;                           // macro moved
    if (state == 4u) return topRight;                             // bulk ready
    if (state == 5u) return (left || right) && y >= 3u && y <= 4u; // breakup
    if (state == 6u) return top && x >= 2u && x <= 5u;            // settled
    if (state == 7u) return topLeft || topRight || lowerLeft || lowerRight;
    if (state == 8u) return lowerLeft;                            // sleeping
    return false;                                                // stable/candidate
}

bool debugStateMarkerReadable(uint state, uint cellPixels) {
    // At minimum zoom, passive tiles otherwise blanket the view with marks.
    return cellPixels >= 2u || (cellPixels == 1u && state <= 5u);
}

uint debugLegendCardHeight(uint scale) { return scale == 2u ? 36u : 28u; }
uint debugLegendTitleHeight(uint scale) { return scale == 2u ? 22u : 14u; }
uint debugStatRowHeight(uint scale) { return scale == 2u ? 18u : 12u; }
uint debugHeaderHeight(uint scale) { return scale == 2u ? 24u : 15u; }

uint debugLegendColumns(uint panelWidth, uint scale) {
    // The longest current label is FINE ACTIVE (11 six-pixel glyph advances).
    uint minimumCardWidth = debugLegendSwatchSize(scale) + 12u + 66u * scale + 5u;
    return panelWidth >= 20u + 2u * minimumCardWidth ? 2u : 1u;
}

uint debugLegendRows(uint panelWidth, uint scale) {
    uint columns = debugLegendColumns(panelWidth, scale);
    return (10u + columns - 1u) / columns;
}

uint debugLegendHeight(uint panelWidth, uint scale) {
    return debugLegendTitleHeight(scale) +
           debugLegendRows(panelWidth, scale) * debugLegendCardHeight(scale) + 10u;
}

uint debugPanelRequiredHeight(uint panelWidth, uint scale, bool regionPage) {
    uint statCount = regionPage ? 26u : 8u;
    uint footerHeight = regionPage ? debugLegendHeight(panelWidth, scale) : 24u;
    return debugHeaderHeight(scale) + statCount * debugStatRowHeight(scale) + 8u + footerHeight;
}

uint debugPanelTextScale(uint panelWidth, uint panelHeight, bool regionPage) {
    return panelWidth >= 292u &&
           panelHeight >= debugPanelRequiredHeight(panelWidth, 2u, regionPage) ? 2u : 1u;
}

uint debugVisibleStatCount(uint panelWidth, uint panelHeight, uint scale, bool regionPage) {
    uint requested = regionPage ? 26u : 8u;
    uint footerHeight = regionPage ? debugLegendHeight(panelWidth, scale) : 24u;
    uint reservedHeight = debugHeaderHeight(scale) + 8u + footerHeight;
    uint availableHeight = panelHeight > reservedHeight ? panelHeight - reservedHeight : 0u;
    return min(requested, availableHeight / debugStatRowHeight(scale));
}

#endif
