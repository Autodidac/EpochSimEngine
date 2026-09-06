#ifndef EPOCHSIMENGINE_CHEMISTRY_OWNERSHIP_GLSL
#define EPOCHSIMENGINE_CHEMISTRY_OWNERSHIP_GLSL

// Immutable source IDs own disjoint compiler-sized chemistry passes. A result
// changing material never changes its pass during the current fixed tick.
const uint CHEMISTRY_OWNER_BULK = 0u;
const uint CHEMISTRY_OWNER_BEES = 1u;
const uint CHEMISTRY_OWNER_MACHINERY = 2u;

uint chemistrySourceOwner(uint material) {
    if (material == MAT_BEE || material == MAT_QUEEN_BEE ||
        material == MAT_POLLEN || material == MAT_HONEY ||
        material == MAT_BEESWAX || material == MAT_BEEHIVE)
        return CHEMISTRY_OWNER_BEES;
    if (material == MAT_SMELTER || material == MAT_ASSEMBLER ||
        material == MAT_SLUICE_BOX || material == MAT_INSECT_HABITAT ||
        material == MAT_SAND || material == MAT_SILT || material == MAT_IRON ||
        material == MAT_IRON_ORE || material == MAT_ALUMINUM ||
        material == MAT_ALUMINUM_SHAVINGS || material == MAT_GOLD ||
        material == MAT_STEEL || material == MAT_COPPER ||
        material == MAT_POWER_CELL || material == MAT_PLASMA_AMMO ||
        material == MAT_FOOD || material == MAT_WASTE || material == MAT_FERTILIZER)
        return CHEMISTRY_OWNER_MACHINERY;
    // Include unknown legacy IDs in one pass too: never leave stale scratch.
    return CHEMISTRY_OWNER_BULK;
}

#endif
