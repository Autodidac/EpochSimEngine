#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

#include "sandhybrid/ui_text_data.hpp"

namespace shader_geometry {
using uint = std::uint32_t;
using std::max;
using std::min;
// Compile the actual shader helpers, rather than a second layout/count model.
#include "debug_geometry.glsl"
}

using namespace shader_geometry;

bool check_layout(uint panelWidth, uint panelHeight, bool expectAllStats) {
    uint scale = debugPanelTextScale(panelWidth, panelHeight, true);
    uint columns = debugLegendColumns(panelWidth, scale);
    uint rows = debugLegendRows(panelWidth, scale);
    uint visibleStats = debugVisibleStatCount(panelWidth, panelHeight, scale, true);
    uint statsBottom = debugHeaderHeight(scale) + visibleStats * debugStatRowHeight(scale);
    uint legendHeight = debugLegendHeight(panelWidth, scale);
    uint keyTop = max(statsBottom + 8u, panelHeight > legendHeight ? panelHeight - legendHeight : 0u);
    if ((visibleStats == 26u) != expectAllStats || rows * columns != 10u ||
        keyTop < statsBottom + 8u || keyTop + legendHeight > panelHeight) return false;

    constexpr std::array<uint, 10> labels{128u, 29u, 131u, 130u, 132u,
                                          135u, 133u, 134u, 28u, 129u};
    uint columnWidth = (panelWidth - 20u) / columns;
    uint swatch = debugLegendSwatchSize(scale);
    for (uint key = 0u; key < 10u; ++key) {
        uint left = 10u + (key % columns) * columnWidth;
        uint top = keyTop + debugLegendTitleHeight(scale) +
                   (key / columns) * debugLegendCardHeight(scale);
        uint right = min(left + columnWidth - 5u, panelWidth - 5u);
        uint bottom = top + debugLegendCardHeight(scale) - 4u;
        uint label = labels[key];
        uint labelLength = sandhybrid::ui::text_storage[label + 1u] -
                           sandhybrid::ui::text_storage[label];
        if (left + swatch + 12u + labelLength * 6u * scale > right ||
            top + 4u + swatch >= bottom || top + 7u + 7u * scale > bottom ||
            bottom > panelHeight - 4u) return false;
    }
    return true;
}

bool check_sample(uint worldWidth, uint worldHeight, uint sampleX, uint sampleY,
                  uint sampleWidth, uint sampleHeight) {
    constexpr uint chunkSize = 64u;
    uint allSleeping = 0u;
    uint patternedSleeping = 0u;
    for (uint cy = 0u; cy < (worldHeight + chunkSize - 1u) / chunkSize; ++cy) {
        for (uint cx = 0u; cx < (worldWidth + chunkSize - 1u) / chunkSize; ++cx) {
            uint count = debugSleepingChunkSampleCells(cx, cy, chunkSize, worldWidth, worldHeight,
                                                       sampleX, sampleY, sampleWidth, sampleHeight);
            allSleeping += count;
            if ((cx + cy) % 3u == 0u) patternedSleeping += count;
        }
    }
    uint expectedAll = 0u;
    uint expectedPattern = 0u;
    for (uint y = sampleY; y < min(sampleY + sampleHeight, worldHeight); ++y) {
        for (uint x = sampleX; x < min(sampleX + sampleWidth, worldWidth); ++x) {
            ++expectedAll;
            if ((x / chunkSize + y / chunkSize) % 3u == 0u) ++expectedPattern;
        }
    }
    return allSleeping == expectedAll && patternedSleeping == expectedPattern &&
           allSleeping <= sampleWidth * sampleHeight;
}

int main() {
    // Actual logical-window panel sizes: 960x720, 1280x800, 1920x1080,
    // 2560x1440. Framebuffer scale must not enter these calculations.
    for (const auto& panel : std::array<std::array<uint, 2>, 4>{
             {{312u, 504u}, {416u, 584u}, {416u, 864u}, {416u, 1224u}}}) {
        if (!check_layout(panel[0], panel[1], true)) return 1;
    }
    if (debugPanelTextScale(312u, 504u, true) != 1u ||
        debugPanelTextScale(416u, 584u, true) != 1u ||
        debugPanelTextScale(416u, 864u, true) != 2u) return 2;

    // Short panels preserve the complete key and explicitly report that fewer
    // stats fit. A genuine one-column fallback still owns all ten rows.
    if (!check_layout(312u, 384u, false) || !check_layout(200u, 800u, true) ||
        debugLegendRows(200u, 1u) != 10u) return 3;
    for (uint width = 292u; width <= 424u; width += 4u) {
        for (uint height = 504u; height <= 1200u; height += 8u) {
            if (!check_layout(width, height, true)) return 4;
        }
    }

    // All sixteen rotating 640x360 samples, including half-chunk Y origins.
    for (uint region = 0u; region < 16u; ++region) {
        if (!check_sample(10240u, 1440u, (region & 3u) * 640u,
                          (region >> 2u) * 360u, 640u, 360u)) return 5;
    }
    if (!check_sample(1045u, 777u, 640u, 720u, 640u, 360u) ||
        !check_sample(1045u, 777u, 1045u, 777u, 0u, 0u) ||
        !check_sample(1045u, 777u, 63u, 63u, 3u, 3u)) return 6;

    std::cout << "Debug geometry: complete fitting 10-state legends and exact sampled sleeping cells passed.\n";
    return 0;
}
