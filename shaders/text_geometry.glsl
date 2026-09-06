#ifndef EPOCHSIMENGINE_TEXT_GEOMETRY_GLSL
#define EPOCHSIMENGINE_TEXT_GEOMETRY_GLSL

// The production fallback font has 5x7 ink cells and a six-cell advance.
// Reject only outside the complete label rectangle; internal glyph spacing
// remains untouched. Scalar arithmetic is also compiled by the CPU oracle.
// Division avoids overflowing a width product for a large glyph count.
bool textPixelInBounds(int localX, int localY, int scale, uint glyphCount) {
    if (scale <= 0 || glyphCount == 0u || localX < 0 || localY < 0)
        return false;
    uint column = uint(localX / scale);
    uint row = uint(localY / scale);
    uint glyph = column / 6u;
    return row < 7u && glyph < glyphCount &&
        (column % 6u < 5u || glyph + 1u < glyphCount);
}

#endif
