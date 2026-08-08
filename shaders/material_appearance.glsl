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
    // Material presentation is clock-static except for the established metal/ore glint.
    // Gas coherence is state-driven in fullscreen.frag, never render-clock animated.
    if (materialAppearanceClass(cell.material) != 2u) return base;

    ivec2 tile = grid / 8;
    ivec2 local = grid - tile * 8;
    uint seed = hash32(uint(tile.x) * 73856093u ^ uint(tile.y) * 19349663u ^
                       cell.material * 83492791u);
    float time = float(frame & 4095u) * 0.045;
    float curved = sin(float(local.x) * 0.72 + float(local.y) * 0.46 +
                       float((seed >> 8u) & 31u) * 0.19 - time);
    float glint = smoothstep(0.70, 0.98, curved) * 0.22;
    float brushed = sin(float(local.y) * 1.45 + float(tile.x & 3) - time * 0.30) * 0.035;
    base.rgb = clamp(base.rgb * (0.95 + brushed) + vec3(glint), 0.0, 1.0);
    return base;
}

#endif
