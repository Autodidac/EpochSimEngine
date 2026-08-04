#ifndef SANDHYBRID_MATERIAL_APPEARANCE_GLSL
#define SANDHYBRID_MATERIAL_APPEARANCE_GLSL

uint materialAppearanceClass(uint material) {
    if (isGas(material)) return 4u;
    if (isLiquid(material) || material == MAT_LAVA) return 3u;
    if (material == MAT_ALUMINUM || material == MAT_IRON || material == MAT_COPPER ||
        material == MAT_GOLD || material == MAT_STEEL ||
        material == MAT_ALUMINUM_SHAVINGS || material == MAT_IRON_ORE) return 2u;
    if (isPowder(material)) return 1u;
    return 0u;
}

vec4 applyMaterialAppearance(Cell cell, ivec2 grid, uint frame, vec4 base) {
    ivec2 tile = grid / 8;
    ivec2 local = grid - tile * 8;
    uint seed = hash32(uint(tile.x) * 73856093u ^ uint(tile.y) * 19349663u ^
                       cell.material * 83492791u);
    float time = float(frame & 4095u) * 0.045;
    float curved = sin(float(local.x) * 0.72 + float(local.y) * 0.46 +
                       float((seed >> 8u) & 31u) * 0.19 - time);
    uint appearance = materialAppearanceClass(cell.material);
    if (appearance == 2u) {
        float glint = smoothstep(0.70, 0.98, curved) * 0.22;
        float brushed = sin(float(local.y) * 1.45 + float(tile.x & 3) - time * 0.30) * 0.035;
        base.rgb = clamp(base.rgb * (0.95 + brushed) + vec3(glint), 0.0, 1.0);
    } else if (appearance == 3u) {
        float flow = sin(float(grid.x) * 0.17 + float(local.y) * 0.50 - time * 0.55);
        base.rgb *= 0.96 + flow * 0.035;
    } else if (appearance == 1u) {
        base.rgb *= 0.95 + curved * 0.035;
    } else if (appearance == 0u && cell.material != MAT_EMPTY) {
        float contour = float((seed >> uint((local.x + local.y) & 15)) & 1u);
        base.rgb *= 0.97 + contour * 0.025;
    }
    return base;
}

#endif
