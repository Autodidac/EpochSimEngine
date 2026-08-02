#ifndef SANDHYBRID_MATERIALS_GLSL
#define SANDHYBRID_MATERIALS_GLSL

#include "material_ids.glsl"
#include "material_physics.glsl"

const uint AUX_WET = 0x80000000u;
const uint AUX_CHARGED = 0x40000000u;
const uint AUX_BEE_POLLEN = 0x20000000u;
const uint AUX_BEE_FED = 0x10000000u;
const uint AUX_PLANT_STEM = 0x08000000u;
const uint AUX_STRUCTURAL = 0x04000000u;
const uint AUX_SUPPORTED = 0x02000000u;
const uint AUX_MOVED = 0x01000000u;
const uint AUX_WATER_HALF = 0x00800000u;
const uint AUX_STATE_MASK = 0x000000ffu;
const uint AUX_RANDOM_MASK = 0x007fff00u;
const uint STRUCTURAL_BLOCK_SIZE = 8u;
const uint STRUCTURAL_FULL_CELLS = STRUCTURAL_BLOCK_SIZE * STRUCTURAL_BLOCK_SIZE;
const uint STRUCTURAL_DESTROYED_CELLS_TO_CRUMBLE = 31u;
const uint STRUCTURAL_COLLAPSE_CELLS =
    STRUCTURAL_FULL_CELLS - STRUCTURAL_DESTROYED_CELLS_TO_CRUMBLE + 1u;

struct Cell {
    uint material;
    uint age;
    int temperature;
    uint aux;
};

bool isHalfWater(Cell cell) {
    return cell.material == MAT_WATER && (cell.aux & AUX_WATER_HALF) != 0u;
}

uint waterHalfUnits(Cell cell) {
    return cell.material == MAT_WATER ? (isHalfWater(cell) ? 1u : 2u) : 0u;
}

void setHalfWater(inout Cell cell, bool halfState) {
    if (cell.material != MAT_WATER) return;
    if (halfState) cell.aux |= AUX_WATER_HALF;
    else cell.aux &= ~AUX_WATER_HALF;
}

#ifndef SANDHYBRID_NO_SIM_PUSH
layout(push_constant) uniform SimulationPush {
    uint width;
    uint height;
    uint step;
    uint seed;
    int brushX;
    int brushY;
    uint radius;
    uint material;
    int activeSectionX;
    int activeSectionY;
    uint activeMode;
    uint reserved;
} pc;
#endif

uint hash32(uint value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

uint stateValue(Cell cell) {
    return cell.aux & AUX_STATE_MASK;
}

void setStateValue(inout Cell cell, uint value) {
    cell.aux = (cell.aux & ~AUX_STATE_MASK) | min(value, 255u);
}

uint machineSlot(Cell cell, uint slot) {
    return (cell.aux >> (8u + min(slot, 3u) * 4u)) & 15u;
}

uvec4 machineInventory(Cell cell) {
    return uvec4(machineSlot(cell, 0u), machineSlot(cell, 1u),
                 machineSlot(cell, 2u), machineSlot(cell, 3u));
}

void setMachineInventory(inout Cell cell, uvec4 inventory) {
    inventory = min(inventory, uvec4(15u));
    cell.aux = (cell.aux & ~AUX_RANDOM_MASK) |
               (inventory.x << 8u) | (inventory.y << 12u) |
               (inventory.z << 16u) | (inventory.w << 20u);
}

uint salinity(Cell cell) {
    return (cell.material == MAT_WATER || cell.material == MAT_SALTWATER) ? stateValue(cell) : 0u;
}

void setSalinity(inout Cell cell, uint value) {
    uint concentration = min(value, 255u);
    cell.material = concentration >= 12u ? MAT_SALTWATER : MAT_WATER;
    setStateValue(cell, concentration);
}

Cell makeCellWithEntropy(uint material, uint seed, uint step) {
    int temperature = 20;
    if (material == MAT_LAVA || material == MAT_MAGMA_VENT) temperature = 1300;
    else if (material == MAT_FIRE || material == MAT_LIGHTNING) temperature = 700;
    else if (material == MAT_EMBER) temperature = 420;
    else if (material == MAT_ICE) temperature = -20;
    else if (material == MAT_SNOW) temperature = -8;
    else if (material == MAT_STEAM || material == MAT_DIRTY_STEAM) temperature = 110;
    else if (material == MAT_URANIUM) temperature = 42;
    else if (material == MAT_SMELTER) temperature = 180;
    else if (material == MAT_PLANT_STEM) temperature = 20;

    uint aux = hash32(material ^ seed ^ step) & AUX_RANDOM_MASK;
    if (material == MAT_SALTWATER) aux |= 96u;
    else if (material == MAT_DIRTY_WATER) aux |= 96u;
    else if (material == MAT_SALT || material == MAT_HONEY || material == MAT_SILT ||
             material == MAT_FERTILIZER || material == MAT_FOOD || material == MAT_WASTE) aux |= 255u;
    else if (material == MAT_BEE) aux |= AUX_BEE_FED | 180u;
    else if (material == MAT_POLLEN) aux |= 16u;
    else if (material == MAT_ALUMINUM_SHAVINGS || material == MAT_GOLD || material == MAT_IRON_ORE ||
             material == MAT_IRON || material == MAT_STEEL || material == MAT_POWER_CELL ||
             material == MAT_PLASMA_AMMO) aux |= 255u;
    else if (material == MAT_OXYGEN) aux |= 220u;
    else if (material == MAT_ATMOSPHERE) aux |= 54u;
    else if (material == MAT_CARBON_DIOXIDE) aux |= 180u;
    else if (material == MAT_HYDROGEN) aux |= 210u;
    else if (material == MAT_ANT || material == MAT_BEETLE)
        aux |= 1u + (hash32(seed ^ step ^ material) & 1u);
    else if (material == MAT_PLANT_STEM) aux |= AUX_PLANT_STEM | 1u;
    else if (material == MAT_CONVEYOR || material == MAT_FACTORY_CORE) aux |= AUX_CHARGED | 255u;
    return Cell(material, 0u, temperature, aux);
}

#ifndef SANDHYBRID_NO_SIM_PUSH
uint cellHash(ivec2 position, uint salt) {
    return hash32(uint(position.x) * 73856093u ^ uint(position.y) * 19349663u ^ pc.step * 83492791u ^ pc.seed ^ salt);
}

bool inside(ivec2 position) {
    return position.x >= 0 && position.y >= 0 && position.x < int(pc.width) && position.y < int(pc.height);
}

uint indexOf(ivec2 position) {
    return uint(position.y) * pc.width + uint(position.x);
}

Cell makeCell(uint material) {
    return makeCellWithEntropy(material, pc.seed, pc.step);
}
#endif


uint cellPhase(Cell cell) {
    uint base = materialBasePhase(cell.material);
    int vapor = materialVaporizationPoint(cell.material);
    int melting = materialMeltingPoint(cell.material);
    int softening = materialSofteningPoint(cell.material);
    if (vapor != NO_TEMPERATURE && cell.temperature >= vapor) return PHASE_VAPOR;
    if ((base == PHASE_SOLID || base == PHASE_POWDER) &&
        melting != NO_TEMPERATURE && cell.temperature >= melting) return PHASE_MOLTEN;
    if ((base == PHASE_SOLID || base == PHASE_POWDER) &&
        softening != NO_TEMPERATURE && cell.temperature >= softening) return PHASE_SOFTENED;
    return base;
}

bool isCellGas(Cell cell) {
    uint phase = cellPhase(cell);
    return phase == PHASE_GAS || phase == PHASE_VAPOR;
}

bool isCellLiquid(Cell cell) {
    uint phase = cellPhase(cell);
    return phase == PHASE_LIQUID || phase == PHASE_MOLTEN;
}

bool isCellPowder(Cell cell) {
    return cellPhase(cell) == PHASE_POWDER;
}

bool isCellSolid(Cell cell) {
    uint phase = cellPhase(cell);
    return phase == PHASE_SOLID || phase == PHASE_SOFTENED;
}

bool isThermallyMobile(Cell cell) {
    uint phase = cellPhase(cell);
    return phase == PHASE_LIQUID || phase == PHASE_MOLTEN || phase == PHASE_GAS || phase == PHASE_VAPOR;
}

bool isGas(uint material) {
    return material == MAT_SMOKE || material == MAT_STEAM || material == MAT_DIRTY_STEAM ||
           material == MAT_FIRE || material == MAT_LIGHTNING || material == MAT_RADIATION ||
           material == MAT_OXYGEN || material == MAT_ATMOSPHERE || material == MAT_CARBON_DIOXIDE ||
           material == MAT_HYDROGEN;
}

bool isFreshWater(uint material) {
    return material == MAT_WATER;
}

bool isWaterBased(uint material) {
    return material == MAT_WATER || material == MAT_SALTWATER || material == MAT_DIRTY_WATER;
}

bool isLiquid(uint material) {
    return material == MAT_WATER || material == MAT_SALTWATER || material == MAT_DIRTY_WATER ||
           material == MAT_ACID || material == MAT_LAVA || material == MAT_OIL || material == MAT_HONEY;
}

bool isPowder(uint material) {
    return material == MAT_SAND || material == MAT_DIRT || material == MAT_MUD || material == MAT_SALT ||
           material == MAT_ASH || material == MAT_GUNPOWDER || material == MAT_SNOW ||
           material == MAT_SEED || material == MAT_POLLEN || material == MAT_SILT ||
           material == MAT_FERTILIZER || material == MAT_FOOD || material == MAT_WASTE ||
           material == MAT_ALUMINUM_SHAVINGS || material == MAT_IRON_ORE;
}

bool isPlant(uint material) {
    return material == MAT_GRASS || material == MAT_PLANT_STEM || material == MAT_FLOWER;
}

bool isOrganic(uint material) {
    return material == MAT_GRASS || material == MAT_PLANT_STEM || material == MAT_FLOWER || material == MAT_SEED ||
           material == MAT_WOOD || material == MAT_HONEY || material == MAT_BEE ||
           material == MAT_QUEEN_BEE || material == MAT_POLLEN || material == MAT_FOOD ||
           material == MAT_WASTE || material == MAT_FERTILIZER;
}

bool isConductive(uint material) {
    return material == MAT_ALUMINUM || material == MAT_IRON || material == MAT_COPPER ||
           material == MAT_STEEL || material == MAT_MAGNET || material == MAT_WATER ||
           material == MAT_SALTWATER || material == MAT_DIRTY_WATER || material == MAT_GOLD ||
           material == MAT_CONVEYOR || material == MAT_SMELTER || material == MAT_ASSEMBLER ||
           material == MAT_INSECT_HABITAT || material == MAT_FACTORY_CORE ||
           material == MAT_SLUICE_BOX;
}

bool isMagnetic(uint material) {
    return material == MAT_IRON || material == MAT_IRON_ORE;
}

bool isImmovable(uint material) {
    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_GRASS ||
           material == MAT_PLANT_STEM || material == MAT_WOOD ||
           material == MAT_PLASTIC || material == MAT_ACID_RESISTANT_PLASTIC ||
           material == MAT_ICE || material == MAT_ALUMINUM || material == MAT_GLASS ||
           material == MAT_BEESWAX || material == MAT_FLOWER || material == MAT_BEEHIVE ||
           material == MAT_QUEEN_BEE || material == MAT_IRON || material == MAT_COPPER ||
           material == MAT_MAGNET || material == MAT_INSULATOR || material == MAT_MAGMA_VENT ||
           material == MAT_URANIUM || material == MAT_STEEL || material == MAT_CONVEYOR || material == MAT_SMELTER ||
           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||
           material == MAT_FACTORY_CORE;
}

bool isCellImmovable(Cell cell) {
    if (isThermallyMobile(cell)) return false;
    return isImmovable(cell.material);
}

uint density(uint material) {
    return materialDensity(material);
}

bool blocksSun(uint material) {
    return material == MAT_STONE || material == MAT_DIRT || material == MAT_MUD ||
           material == MAT_ALUMINUM || material == MAT_IRON || material == MAT_COPPER ||
           material == MAT_MAGNET || material == MAT_INSULATOR || material == MAT_WOOD ||
           material == MAT_PLASTIC || material == MAT_ACID_RESISTANT_PLASTIC ||
           material == MAT_GRASS || material == MAT_FLOWER || material == MAT_SAND ||
           material == MAT_GUNPOWDER || material == MAT_HONEY || material == MAT_BEESWAX ||
           material == MAT_BEEHIVE || material == MAT_QUEEN_BEE || material == MAT_LAVA ||
           material == MAT_MAGMA_VENT || material == MAT_URANIUM || material == MAT_STEEL || material == MAT_CONVEYOR ||
           material == MAT_SMELTER || material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||
           material == MAT_FACTORY_CORE || material == MAT_SLUICE_BOX ||
           material == MAT_SILT || material == MAT_FERTILIZER ||
           material == MAT_FOOD || material == MAT_WASTE ||
           material == MAT_ALUMINUM_SHAVINGS || material == MAT_IRON_ORE;
}

bool isFlammable(uint material) {
    return material == MAT_GRASS || material == MAT_PLANT_STEM ||
           material == MAT_FLOWER || material == MAT_OIL ||
           material == MAT_WOOD || material == MAT_PLASTIC || material == MAT_HONEY ||
           material == MAT_BEESWAX || material == MAT_BEEHIVE || material == MAT_BEE ||
           material == MAT_GUNPOWDER || material == MAT_SEED || material == MAT_POLLEN ||
           material == MAT_FERTILIZER || material == MAT_FOOD || material == MAT_WASTE ||
           material == MAT_POWER_CELL || material == MAT_PLASMA_AMMO;
}

uint flammability(uint material) {
    switch (material) {
    case MAT_GUNPOWDER: return 245u;
    case MAT_OIL: return 150u;
    case MAT_POLLEN: return 112u;
    case MAT_BEESWAX: return 92u;
    case MAT_BEEHIVE: return 82u;
    case MAT_FLOWER: return 76u;
    case MAT_GRASS: return 64u;
    case MAT_SEED: return 48u;
    case MAT_PLASTIC: return 38u;
    case MAT_WOOD: return 24u;
    case MAT_BEE: return 18u;
    case MAT_HONEY: return 8u;
    case MAT_FERTILIZER: return 44u;
    case MAT_FOOD: return 36u;
    case MAT_WASTE: return 52u;
    default: return 0u;
    }
}

bool acidImmune(uint material) {
    return material == MAT_EMPTY || material == MAT_ACID || material == MAT_CRYSTAL ||
           material == MAT_ACID_RESISTANT_PLASTIC || material == MAT_GLASS ||
           material == MAT_SAND || material == MAT_FIRE || material == MAT_SMOKE ||
           material == MAT_STEAM || material == MAT_DIRTY_STEAM || material == MAT_INSULATOR ||
           material == MAT_RADIATION || material == MAT_LIGHTNING ||
           material == MAT_ALUMINUM_SHAVINGS || material == MAT_GOLD;
}

uint acidResistance(uint material) {
    switch (material) {
    case MAT_STONE: return 252u;
    case MAT_ALUMINUM: return 248u;
    case MAT_IRON: return 244u;
    case MAT_COPPER: return 242u;
    case MAT_MAGNET: return 240u;
    case MAT_PLASTIC: return 238u;
    case MAT_BEESWAX: return 230u;
    case MAT_BEEHIVE: return 225u;
    case MAT_WOOD: return 220u;
    case MAT_WATER: return 214u;
    case MAT_SALTWATER: return 212u;
    case MAT_DIRTY_WATER: return 208u;
    case MAT_OIL: return 205u;
    case MAT_HONEY: return 200u;
    case MAT_DIRT: return 190u;
    case MAT_MUD: return 185u;
    case MAT_SEED: return 180u;
    case MAT_GRASS: return 165u;
    case MAT_FLOWER: return 145u;
    case MAT_SILT: return 178u;
    case MAT_FERTILIZER: return 160u;
    case MAT_FOOD: return 145u;
    case MAT_WASTE: return 150u;
    case MAT_BEE: return 110u;
    case MAT_QUEEN_BEE: return 125u;
    case MAT_LAVA: return 255u;
    case MAT_MAGMA_VENT: return 255u;
    case MAT_ALUMINUM_SHAVINGS: return 250u;
    case MAT_GOLD: return 246u;
    case MAT_IRON_ORE: return 242u;
    case MAT_STEEL: return 252u;
    case MAT_CONVEYOR: return 250u;
    case MAT_SMELTER: return 252u;
    case MAT_ASSEMBLER: return 250u;
    case MAT_INSECT_HABITAT: return 250u;
    case MAT_FACTORY_CORE: return 254u;
    case MAT_SLUICE_BOX: return 248u;
    default: return acidImmune(material) ? 255u : 175u;
    }
}



bool isBlockCapable(uint material) {
    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_WOOD ||
           material == MAT_PLASTIC || material == MAT_ACID_RESISTANT_PLASTIC ||
           material == MAT_ALUMINUM || material == MAT_GLASS || material == MAT_IRON ||
           material == MAT_IRON_ORE ||
           material == MAT_COPPER || material == MAT_GOLD || material == MAT_MAGNET || material == MAT_INSULATOR ||
           material == MAT_URANIUM || material == MAT_STEEL || material == MAT_CONVEYOR || material == MAT_SMELTER ||
           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||
           material == MAT_FACTORY_CORE || material == MAT_SLUICE_BOX;
}

// Configured terrain-forming solids may sleep as cohesive 8x8 Terraria tiles.
// This property is canonical: placement source and provenance never participate.
bool isReconstructableMaterial(uint material) {
    return isBlockCapable(material) || material == MAT_SAND || material == MAT_DIRT ||
           material == MAT_SILT || material == MAT_SALT || material == MAT_ICE;
}

bool isStructural(Cell cell) {
    return (cell.aux & AUX_STRUCTURAL) != 0u;
}

bool isSupported(Cell cell) {
    return (cell.aux & AUX_SUPPORTED) != 0u;
}

bool isLooseSolid(Cell cell) {
    return isBlockCapable(cell.material) && !isStructural(cell) && isCellSolid(cell);
}

#ifndef SANDHYBRID_NO_SIM_PUSH
Cell makeStructuralCell(uint material, bool supported) {
    Cell cell = makeCell(material);
    cell.aux |= AUX_STRUCTURAL;
    if (supported) cell.aux |= AUX_SUPPORTED;
    setStateValue(cell, 255u);
    return cell;
}
#endif

// Density exchange is restricted to fluid-involved pairs. Powders never sort
// through other powders or solids; they only fall into empty/gas cells or sink
// through liquids when physically denser.
bool canFallInto(uint moving, uint target) {
    if (target == MAT_EMPTY) return true;
    if (isImmovable(target)) return false;
    if (moving == MAT_BEE || moving == MAT_QUEEN_BEE) return false;
    if (isGas(target) && !isGas(moving)) return true;
    if (isPowder(moving)) return isLiquid(target) && density(moving) > density(target);
    if (isLiquid(moving)) {
        return (isLiquid(target) && density(moving) > density(target)) || isGas(target);
    }
    return false;
}

bool canRiseInto(uint moving, uint target) {
    if (!isGas(moving)) return false;
    return target == MAT_EMPTY || (isGas(target) && density(moving) < density(target));
}

uint staticCellHash(uint aux, ivec2 position, uint salt) {
    return hash32((aux & (AUX_RANDOM_MASK | AUX_STATE_MASK)) ^
                  uint(position.x) * 0x9e3779b9u ^ uint(position.y) * 0x85ebca6bu ^ salt);
}

vec4 materialColor(uint material, uint age, uint aux, ivec2 position) {
    uint textureHash = staticCellHash(aux, position, material * 2654435761u);
    float variation = float((textureHash >> 24u) & 15u) / 255.0;
    bool charged = (aux & AUX_CHARGED) != 0u;

    vec4 color;
    switch (material) {
    case MAT_EMPTY: color = vec4(0.025, 0.035, 0.055, 1.0); break;
    case MAT_SAND: { float wet = (aux & AUX_WET) != 0u ? -0.16 : 0.0;
        color = vec4(0.88 + variation + wet, 0.72 + variation + wet * 0.75, 0.34 + wet * 0.35, 1.0); break; }
    case MAT_WATER: color = vec4(0.08, 0.34 + variation, 0.92, 0.88); break;
    case MAT_DIRT: { float wetDarken=(aux&AUX_WET)!=0u?-0.10:0.0; color=vec4(0.34+variation+wetDarken,0.19+wetDarken*0.55,0.08+wetDarken*0.30,1.0); break; }
    case MAT_STONE: {
        float speckle = float(textureHash & 7u) * 0.012;
        color = vec4(0.30 + speckle, 0.32 + speckle, 0.35 + speckle, 1.0); break;
    }
    case MAT_CRYSTAL: {
        uint facet = staticCellHash(aux, position / 3, 0x51ed270bu);
        float tone = float((facet >> 27u) & 7u) * 0.035;
        bool edge = ((position.x + position.y + int(aux & 7u)) % 7) == 0;
        color = vec4(0.28 + tone, 0.72 + tone, edge ? 1.0 : 0.92, 0.94); break;
    }
    case MAT_MUD: color = vec4(0.28, 0.18 + variation, 0.09, 1.0); break;
    case MAT_ACID: color = vec4(0.45, 0.95, 0.08 + variation, 0.9); break;
    case MAT_GRASS: color = vec4(0.12, 0.58 + variation, 0.16, 1.0); break;
    case MAT_SMOKE: color = vec4(0.18 + variation, 0.19 + variation, 0.22 + variation, 0.72); break;
    case MAT_STEAM: color = vec4(0.68, 0.80, 0.88, 0.64); break;
    case MAT_FIRE: color = vec4(1.0, 0.30 + variation, 0.03, 1.0); break;
    case MAT_LAVA: color = vec4(0.98, 0.18 + variation, 0.015, 1.0); break;
    case MAT_OIL: color = vec4(0.10 + variation, 0.075, 0.055, 0.96); break;
    case MAT_WOOD: {
        float grain = float((uint(abs(position.x * 3 + position.y)) + (textureHash & 15u)) % 9u) * 0.012;
        color = vec4(0.38 + grain, 0.20 + grain * 0.5, 0.07, 1.0); break;
    }
    case MAT_PLASTIC: color = vec4(0.80, 0.24 + variation, 0.42, 1.0); break;
    case MAT_ACID_RESISTANT_PLASTIC: color = vec4(0.22, 0.75, 0.78 + variation, 1.0); break;
    case MAT_HONEY: color = vec4(0.94, 0.54 + variation, 0.06, 0.96); break;
    case MAT_BEE: {
        uint wing = (age / 2u + uint(position.x + position.y)) & 1u;
        uint stripe = (textureHash >> 4u) & 3u;
        color = stripe == 0u ? vec4(0.08, 0.055, 0.025, 1.0)
                             : vec4(0.96, 0.62 + float(wing) * 0.08, 0.08, 1.0);
        break;
    }
    case MAT_SALT: color = vec4(0.91 + variation, 0.92 + variation, 0.94 + variation, 1.0); break;
    case MAT_ICE: {
        bool crack = ((textureHash ^ uint(position.x * 13 + position.y * 7)) & 31u) == 0u;
        color = vec4(crack ? 0.78 : 0.52, crack ? 0.93 : 0.80, 0.98, 0.92); break;
    }
    case MAT_ALUMINUM: color = vec4(0.46 + variation, 0.49 + variation, 0.54 + variation, 1.0); break;
    case MAT_ASH: color = vec4(0.33 + variation, 0.32 + variation, 0.30 + variation, 1.0); break;
    case MAT_EMBER: color = vec4(0.74, 0.16 + variation, 0.04, 1.0); break;
    case MAT_GLASS: {
        bool seam = ((position.x * 5 + position.y * 3 + int(textureHash & 7u)) % 19) == 0;
        color = vec4(seam ? 0.62 : 0.20, seam ? 0.84 : 0.54, seam ? 0.92 : 0.66, 0.62); break;
    }
    case MAT_GUNPOWDER: color = vec4(0.14 + variation, 0.14 + variation, 0.15 + variation, 1.0); break;
    case MAT_SNOW: color = vec4(0.92 + variation, 0.96 + variation, 1.0, 1.0); break;
    case MAT_SEED: color = vec4(0.47, 0.31 + variation, 0.08, 1.0); break;
    case MAT_BEESWAX: color = vec4(0.95, 0.70 + variation, 0.16, 1.0); break;
    case MAT_FLOWER: {
        uint petal = textureHash % 3u;
        color = petal == 0u ? vec4(0.96, 0.24, 0.48, 1.0)
              : petal == 1u ? vec4(0.72, 0.30, 0.96, 1.0)
                             : vec4(1.0, 0.70, 0.16, 1.0); break;
    }
    case MAT_SALTWATER: {
        float saltTone = float(aux & AUX_STATE_MASK) / 255.0;
        color = vec4(0.07 + saltTone * 0.05, 0.42 + variation, 0.72 + saltTone * 0.12, 0.9); break;
    }
    case MAT_BEEHIVE: {
        float pulse = 0.025 * sin(float(age) * 0.055 + float(position.x + position.y) * 0.45);
        bool comb = ((position.x + (position.y & 1)) % 4) == 0;
        color = vec4(0.62 + pulse, (comb ? 0.37 : 0.42) + pulse, 0.08, 1.0); break;
    }
    case MAT_DIRTY_STEAM: color = vec4(0.43, 0.47 + variation, 0.48, 0.68); break;
    case MAT_DIRTY_WATER: color = vec4(0.16, 0.30 + variation, 0.31, 0.94); break;
    case MAT_POLLEN: color = vec4(0.98, 0.78 + variation, 0.08, 1.0); break;
    case MAT_QUEEN_BEE: {
        bool stripe = ((position.x + position.y) & 1) == 0;
        color = stripe ? vec4(0.78, 0.34, 0.035, 1.0)
                       : vec4(0.16, 0.075, 0.025, 1.0);
        break;
    }
    case MAT_IRON: color = vec4(0.34 + variation, 0.29 + variation, 0.26 + variation, 1.0); break;
    case MAT_COPPER: color = vec4(0.78 + variation, 0.31, 0.12, 1.0); break;
    case MAT_MAGNET: color = ((position.x + position.y) & 1) == 0
        ? vec4(0.78, 0.08, 0.10, 1.0) : vec4(0.12, 0.20, 0.72, 1.0); break;
    case MAT_INSULATOR: color = vec4(0.82, 0.76 + variation, 0.58, 1.0); break;
    case MAT_LIGHTNING: color = vec4(0.48, 0.74, 1.0, 1.0); break;
    case MAT_MAGMA_VENT: color = vec4(0.36, 0.05, 0.02, 1.0); break;
    case MAT_URANIUM: color = vec4(0.27, 0.67 + variation, 0.18, 1.0); break;
    case MAT_RADIATION: color = vec4(0.50, 0.90, 0.14, 0.72); break;
    case MAT_ALUMINUM_SHAVINGS: {
        bool fleck = (textureHash & 15u) < 4u;
        color = fleck ? vec4(0.94, 0.70, 0.12, 1.0) : vec4(0.26, 0.23, 0.20, 1.0);
        break;
    }
    case MAT_GOLD: color = vec4(0.98, 0.76 + variation, 0.10, 1.0); break;
    case MAT_OXYGEN: color = vec4(0.30, 0.76 + variation * 0.25, 1.00, 0.34); break;
    case MAT_ATMOSPHERE: color = vec4(0.34, 0.64 + variation * 0.18, 0.72, 0.16); break;
    case MAT_CARBON_DIOXIDE: color = vec4(0.015 + variation * 0.08, 0.020 + variation * 0.06, 0.030 + variation * 0.08, 0.62); break;
    case MAT_HYDROGEN: color = vec4(1.00, 0.32 + variation * 0.18, 0.68 + variation * 0.18, 0.42); break;
    case MAT_IRON_ORE: {
        bool fleck = (textureHash & 15u) < 5u;
        color = fleck ? vec4(0.58, 0.30, 0.19, 1.0) : vec4(0.24, 0.21, 0.20, 1.0); break;
    }
    case MAT_STEEL: color = vec4(0.57 + variation, 0.62 + variation, 0.68 + variation, 1.0); break;
    case MAT_CONVEYOR: {
        bool belt = (position.y & 3) < 2;
        color = belt ? vec4(0.15, 0.18, 0.21, 1.0) : vec4(0.42, 0.46, 0.50, 1.0); break;
    }
    case MAT_SMELTER: color = vec4(0.42, 0.19, 0.06, 1.0); break;
    case MAT_ASSEMBLER: color = vec4(0.12, 0.44, 0.52, 1.0); break;
    case MAT_INSECT_HABITAT: color = vec4(0.34, 0.19, 0.61, 1.0); break;
    case MAT_POWER_CELL: color = vec4(0.45, 0.96, 0.35 + variation, 1.0); break;
    case MAT_PLASMA_AMMO: color = vec4(0.92, 0.18 + variation, 0.96, 1.0); break;
    case MAT_ANT: color = vec4(0.15, 0.72 + variation, 0.98, 1.0); break;
    case MAT_BEETLE: color = vec4(0.94, 0.16 + variation, 0.14, 1.0); break;
    case MAT_PLANT_STEM: color = (aux & AUX_CHARGED) != 0u
        ? vec4(0.20, 0.80, 1.0, 1.0) : vec4(1.0, 0.26, 0.12, 1.0); break;
    case MAT_FACTORY_CORE: color = vec4(0.24, 0.88, 0.82, 1.0); break;
    case MAT_SLUICE_BOX: { bool riffle = ((position.x + position.y) & 3) == 0;
        color = riffle ? vec4(0.72, 0.55, 0.18, 1.0) : vec4(0.22, 0.29, 0.34, 1.0); break; }
    case MAT_SILT: color = vec4(0.34 + variation, 0.29 + variation, 0.20, 1.0); break;
    case MAT_FERTILIZER: color = vec4(0.26, 0.42 + variation, 0.14, 1.0); break;
    case MAT_FOOD: color = vec4(0.88, 0.56 + variation, 0.16, 1.0); break;
    case MAT_WASTE: color = vec4(0.30 + variation, 0.20, 0.11, 1.0); break;
    default: color = vec4(1.0, 0.0, 1.0, 1.0); break;
    }


    if (charged && isConductive(material)) {
        color.rgb = min(color.rgb + vec3(0.18, 0.28, 0.55), vec3(1.0));
    }
    return color;
}

#endif
