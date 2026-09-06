#ifndef EPOCHSIMENGINE_CHEMISTRY_OWNERSHIP_GLSL
#define EPOCHSIMENGINE_CHEMISTRY_OWNERSHIP_GLSL

// Immutable source IDs own disjoint compiler-sized chemistry passes. A result
// changing material never changes its pass during the current fixed tick.
const uint CHEMISTRY_OWNER_BULK = 0u;
const uint CHEMISTRY_OWNER_BEES = 1u;
const uint CHEMISTRY_OWNER_MACHINERY = 2u;
const uint CHEMISTRY_OWNER_DESTINATIONS = 3u;
const uint CHEMISTRY_OWNER_PHASES = 4u;

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
    if (material == MAT_EMPTY || material == MAT_ATMOSPHERE)
        return CHEMISTRY_OWNER_DESTINATIONS;
    if (material == MAT_WATER || material == MAT_DIRTY_WATER ||
        material == MAT_SALTWATER || material == MAT_STEAM ||
        material == MAT_DIRTY_STEAM || material == MAT_CLOUD ||
        material == MAT_ICE || material == MAT_SNOW || material == MAT_SALT ||
        material == MAT_HYDROGEN || material == MAT_OXYGEN ||
        material == MAT_CARBON_DIOXIDE || material == MAT_SMOKE ||
        material == MAT_LAVA || material == MAT_FIRE ||
        material == MAT_EMBER || material == MAT_MAGMA_VENT)
        return CHEMISTRY_OWNER_PHASES;
    // Include unknown legacy IDs in one pass too: never leave stale scratch.
    return CHEMISTRY_OWNER_BULK;
}

#endif
