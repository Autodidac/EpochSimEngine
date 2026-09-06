#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace shader_text {
using uint = std::uint32_t;
struct uvec2 {
    uint x;
    uint y;
    constexpr uvec2(uint first, uint second) : x(first), y(second) {}
};
// Compile the actual production bounds selector and generated font bytes.
#include "text_geometry.glsl"
#include "epochgui_font.glsl"
}

namespace {
using uint = std::uint32_t;
std::uint64_t geometry_checks{};
std::uint64_t raster_checks{};

bool geometry_oracle(int x, int y, int scale, uint count) {
    // Independent multiplication/half-open rectangle in a wider integer type,
    // not the shader's divided glyph/column formulation.
    if (scale <= 0 || count == 0u) return false;
    // Clamping before multiplication is exact for int-coordinate witnesses
    // and keeps even UINT_MAX-count/INT_MAX-scale cases in int64 range.
    const auto width_cells = std::min(6 * static_cast<std::int64_t>(count) - 1,
        static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1);
    const auto width = width_cells * scale;
    const auto height = 7 * static_cast<std::int64_t>(scale);
    return x >= 0 && y >= 0 && x < width && y < height;
}

bool check_geometry(int x, int y, int scale, uint count) {
    ++geometry_checks;
    if (shader_text::textPixelInBounds(x, y, scale, count) ==
        geometry_oracle(x, y, scale, count)) return true;
    std::cerr << "Text bounds mismatch x/y/scale/count=" << x << '/' << y
              << '/' << scale << '/' << count << '\n';
    return false;
}

bool glyph_pixel(int x, int y, int origin_x, int origin_y, int scale, uint code) {
    const int local_x = x - origin_x;
    const int local_y = y - origin_y;
    if (scale <= 0 || local_x < 0 || local_y < 0) return false;
    const int column = local_x / scale;
    const int row = local_y / scale;
    if (column >= 5 || row >= 7) return false;
    const auto bits = shader_text::epochGuiGlyphBits(code);
    const uint row_bits = row < 6 ? ((bits.x >> (static_cast<uint>(row) * 5u)) & 31u)
                                  : (bits.y & 31u);
    return (row_bits & (1u << static_cast<uint>(4 - column))) != 0u;
}

bool label_pixel(std::string_view label, int x, int y, int origin_x, int origin_y,
                 int scale, bool cull, std::uint64_t& glyph_calls) {
    if (cull && !shader_text::textPixelInBounds(x - origin_x, y - origin_y, scale,
                                               static_cast<uint>(label.size())))
        return false;
    for (std::size_t i = 0u; i < label.size(); ++i) {
        ++glyph_calls;
        if (glyph_pixel(x, y, origin_x + static_cast<int>(i) * 6 * scale,
                        origin_y, scale, static_cast<unsigned char>(label[i]))) return true;
    }
    return false;
}

bool compare_label(std::string_view label, int x, int y, int origin_x, int origin_y,
                   int scale, std::uint64_t& baseline, std::uint64_t& culled) {
    ++raster_checks;
    if (label_pixel(label, x, y, origin_x, origin_y, scale, false, baseline) ==
        label_pixel(label, x, y, origin_x, origin_y, scale, true, culled)) return true;
    std::cerr << "Text raster mismatch x/y/origin/scale/length=" << x << '/' << y
              << '/' << origin_x << ',' << origin_y << '/' << scale << '/'
              << label.size() << '\n';
    return false;
}

std::string displayed_number(std::int64_t number) {
    // signedNumberPixel emits its minus separately and then numberPixel clamps
    // the magnitude to eight digits. Preserve the six-cell sign advance.
    const bool negative = number < 0;
    const auto magnitude = static_cast<std::uint64_t>(negative ? -number : number);
    return (negative ? "-" : "") + std::to_string(std::min(magnitude, std::uint64_t{99999999}));
}

bool signed_number_pixel(std::string_view number, int x, int y, int origin_x,
                         int origin_y, int scale, bool cull, std::uint64_t& calls) {
    if (!number.empty() && number.front() == '-') {
        ++calls;
        if (glyph_pixel(x, y, origin_x, origin_y, scale, 45u)) return true;
        origin_x += 6 * scale;
        number.remove_prefix(1u);
    }
    return label_pixel(number, x, y, origin_x, origin_y, scale, cull, calls);
}
}

int main() {
    // Every coordinate around the entire label box, including internal gaps,
    // last-glyph trailing space, and empty labels, for the supported scales.
    for (uint count = 0u; count <= 80u; ++count) {
        for (const int scale : {-2, 0, 1, 2, 3, 4}) {
            const int extent = std::max(scale, 1);
            for (int y = -3; y <= 7 * extent + 3; ++y) {
                for (int x = -3; x <= static_cast<int>(count) * 6 * extent + 3; ++x) {
                    if (!check_geometry(x, y, scale, count)) return 1;
                }
            }
        }
    }
    // The helper cannot overflow a text-width multiplication, even beyond any
    // real UI label. Coordinate subtraction remains the existing shader rule.
    for (const uint count : {1u, 1000000u, std::numeric_limits<uint>::max()}) {
        for (const int scale : {1, 4, 1000000, std::numeric_limits<int>::max()}) {
            for (const int x : {-100000, -1, 0, 4, 5, 6, 1000000,
                                std::numeric_limits<int>::max()}) {
                for (const int y : {-1, 0, 6, 7, 1000000}) {
                    if (!check_geometry(x, y, scale, count)) return 2;
                }
            }
        }
    }

    std::uint64_t baseline{}, culled{};
    // Every fallback-font byte (including unsupported-byte '?' behavior),
    // then multi-glyph labels with spaces, punctuation and clipped origins.
    for (uint code = 0u; code < 256u; ++code) {
        const std::string label(1u, static_cast<char>(code));
        for (const int scale : {1, 2, 3, 4}) {
            for (int y = -2; y <= 7 * scale + 2; ++y) {
                for (int x = -2; x <= 6 * scale + 2; ++x) {
                    if (!compare_label(label, x, y, 0, 0, scale, baseline, culled)) return 3;
                }
            }
        }
    }
    constexpr std::array labels{"", " ", "A", "A A", " A ", "NUKE FROM SPACE",
        "REGION / WORLD TOTALS", "PAIR WORK N/A", "-99999999", "0-", "0 - 1",
        "[BEE]: (60) + 54/6!", "UNSUPPORTED ~lowercase?"};
    constexpr std::array<std::array<int, 2>, 4> origins{{{-37, -5}, {0, 0}, {17, 11}, {95, 35}}};
    for (const auto label : labels) {
        for (const auto& origin : origins) {
            for (const int scale : {-1, 0, 1, 2, 3, 4}) {
                for (int y = -4; y < 45; ++y) {
                    for (int x = -4; x < 120; ++x) {
                        if (!compare_label(label, x, y, origin[0], origin[1], scale,
                                           baseline, culled)) return 4;
                    }
                }
            }
        }
    }
    for (const std::int64_t value : {0LL, 9LL, 10LL, 99LL, 100LL, 9999999LL,
            99999999LL, 100000000LL, 4294967295LL, -1LL, -10LL, -99999999LL,
            -2147483647LL}) {
        const auto number = displayed_number(value);
        for (const int scale : {0, 1, 2, 4}) {
            for (int y = -4; y < 35; ++y) {
                for (int x = -5; x < 235; ++x) {
                    ++raster_checks;
                    if (signed_number_pixel(number, x, y, -3, 2, scale, false, baseline) !=
                        signed_number_pixel(number, x, y, -3, 2, scale, true, culled)) return 5;
                }
            }
        }
    }

    // A declared algorithmic workload, not a frame-time benchmark: one real
    // 15-glyph ACTIONS label sampled across a 416x640 sidebar at scale two.
    std::uint64_t label_baseline{}, label_culled{};
    for (int y = 0; y < 640; ++y) {
        for (int x = 0; x < 416; ++x) {
            if (!compare_label("NUKE FROM SPACE", x, y, 12, 360, 2,
                               label_baseline, label_culled)) return 6;
        }
    }
    if (label_culled >= label_baseline || culled >= baseline) return 7;
    std::cout << "Text geometry: " << geometry_checks << " exact rectangle cases, "
              << raster_checks << " byte-font raster/sign cases; sample label glyph calls "
              << label_baseline << " -> " << label_culled
              << " (algorithmic only; GPU pixels/timing still require runtime validation).\n";
    return 0;
}
