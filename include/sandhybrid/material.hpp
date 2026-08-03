#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace sandhybrid {

inline constexpr std::int16_t no_temperature = std::numeric_limits<std::int16_t>::max();

enum class Material : std::uint32_t {
    empty = 0,
    sand = 1,
    water = 2,
    dirt = 3,
    stone = 4,
    crystal = 5,
    mud = 6,
    acid = 7,
    grass = 8,
    smoke = 9,
    steam = 10,
    fire = 11,
    lava = 12,
    oil = 13,
    wood = 14,
    plastic = 15,
    acid_resistant_plastic = 16,
    honey = 17,
    bee = 18,
    salt = 19,
    ice = 20,
    aluminum = 21,
    ash = 22,
    ember = 23,
    glass = 24,
    gunpowder = 25,
    snow = 26,
    seed = 27,
    beeswax = 28,
    flower = 29,
    saltwater = 30,
    beehive = 31,
    dirty_steam = 32,
    dirty_water = 33,
    pollen = 34,
    queen_bee = 35,
    iron = 36,
    copper = 37,
    magnet = 38,
    insulator = 39,
    lightning = 40,
    magma_vent = 41,
    uranium = 42,
    radiation = 43,
    aluminum_shavings = 44,
    gold = 45,
    oxygen = 46,
    carbon_dioxide = 47,
    iron_ore = 48,
    steel = 49,
    conveyor = 50,
    smelter = 51,
    assembler = 52,
    insect_habitat = 53,
    power_cell = 54,
    plasma_ammo = 55,
    ant = 56,
    beetle = 57,
    plant_stem = 58,
    factory_core = 59,
    silt = 60,
    fertilizer = 61,
    food = 62,
    waste = 63,
    hydrogen = 64,
    sluice_box = 65,
    atmosphere = 66,
    count
};

inline constexpr auto material_count = static_cast<std::uint32_t>(Material::count);

enum class MaterialPhase : std::uint8_t {
    empty = 0, solid, powder, liquid, gas, plasma, softened, molten, vapor
};

struct MaterialProfile final {
    std::uint16_t strength{};
    std::uint16_t erosion_resistance{};
    std::uint16_t density{};
    std::uint16_t acid_resistance{};
    std::int16_t service_temperature{};
    std::int16_t softening_point{};
    std::int16_t melting_point{};
    std::int16_t boiling_point{};
    std::int16_t vaporization_point{};
    std::int16_t ignition_point{};
    std::uint16_t thermal_conductivity{};
    MaterialPhase base_phase{};
    std::string_view category{};
    std::string_view strengths{};
    std::string_view weaknesses{};
    std::string_view conversions{};
    std::string_view ecological_role{};
    std::string_view danger{};
};

inline constexpr std::array<std::string_view, material_count> material_names{
    "Vacuum",
    "Sand",
    "Water",
    "Soil",
    "Stone",
    "Crystal",
    "Mud",
    "Acid",
    "Grass",
    "Smoke",
    "Steam",
    "Fire",
    "Lava",
    "Oil",
    "Wood",
    "Plastic",
    "AR plastic",
    "Honey",
    "Bee",
    "Salt",
    "Ice",
    "Aluminum",
    "Ash",
    "Ember",
    "Glass",
    "Gunpowder",
    "Snow",
    "Seed",
    "Beeswax",
    "Flower",
    "Saltwater",
    "Beehive",
    "Dirty steam",
    "Dirty water",
    "Pollen",
    "Queen",
    "Iron",
    "Copper",
    "Magnet",
    "Insulator",
    "Lightning",
    "Magma vent",
    "Uranium",
    "Radiation",
    "Al shavings",
    "Gold",
    "Oxygen",
    "CO2",
    "Iron ore",
    "Steel",
    "Conveyor",
    "Smelter",
    "Assembler",
    "Ant colony",
    "Power cell",
    "Plasma ammo",
    "Ant",
    "Beetle",
    "Plant stem",
    "Factory core",
    "Silt",
    "Fertilizer",
    "Food",
    "Waste",
    "Hydrogen",
    "Sluice box",
    "Atmosphere",
};

inline constexpr std::array<MaterialProfile, material_count> material_profiles{{
    {0u, 0u, 0u, 255u, 2000, 32767, 32767, 32767, 32767, 32767, 0u, MaterialPhase::empty, "Unknown", "OPEN SPACE", "ANY MATERIAL", "TO: PLACED MATERIAL", "ROLE: VACUUM / AIR GAP", "DANGER: DECOMPRESSION"},
    {84u, 52u, 84u, 255u, 1050, 1450, 1710, 32767, 2230, 32767, 20u, MaterialPhase::powder, "Terrain", "STRONG: PACKING / FILTERING", "WEAK: FLOWING WATER", "TO: GLASS / SILT", "ROLE: MINERAL MATRIX", "DANGER: BURIAL"},
    {44u, 0u, 44u, 214u, 100, 32767, 0, 100, 100, 32767, 140u, MaterialPhase::liquid, "Water", "STRONG: COOLING / FLOW", "WEAK: HEAT / CONTAMINATION", "TO: STEAM / ICE / MUD", "ROLE: CLEAN PROCESS FLUID", "DANGER: FLOOD / DROWNING"},
    {80u, 48u, 80u, 190u, 320, 32767, 32767, 32767, 1200, 450, 28u, MaterialPhase::powder, "Terrain", "STRONG: FARM SUPPORT", "WEAK: SATURATION / ACID", "TO: MUD / GRASS / SILT", "ROLE: SOIL RESERVOIR", "DANGER: CAVE IN"},
    {235u, 230u, 235u, 252u, 1350, 1200, 1500, 32767, 3200, 32767, 72u, MaterialPhase::solid, "Terrain", "STRONG: SUPPORT / HEAT", "WEAK: FRACTURE / ACID", "TO: RUBBLE / LAVA ROCK", "ROLE: STRUCTURAL MASS", "DANGER: FALLING BLOCK"},
    {205u, 180u, 205u, 255u, 900, 760, 900, 32767, 2400, 32767, 85u, MaterialPhase::solid, "Terrain", "STRONG: ACID / OPTICS", "WEAK: IMPACT / THERMAL SHOCK", "TO: SHARDS / GLASS", "ROLE: TECH MATERIAL", "DANGER: SHARP FRAGMENTS"},
    {68u, 22u, 68u, 185u, 180, 32767, 32767, 100, 100, 32767, 60u, MaterialPhase::powder, "Terrain", "STRONG: WATER STORAGE", "WEAK: DRYING / LOAD", "TO: SOIL / SILT", "ROLE: WET SOIL LAYER", "DANGER: ENTOMBMENT"},
    {48u, 0u, 48u, 255u, 110, 32767, -20, 110, 110, 32767, 95u, MaterialPhase::liquid, "Heat", "STRONG: CORROSION", "WEAK: DILUTION / GLASS", "TO: DIRTY WATER / SALT", "ROLE: CHEMICAL REAGENT", "DANGER: TOXIC CORROSIVE"},
    {64u, 18u, 64u, 165u, 120, 80, 32767, 32767, 350, 220, 14u, MaterialPhase::solid, "Life", "STRONG: CO2 CAPTURE", "WEAK: SALT / FIRE / DARK", "TO: FOOD / WASTE / ASH", "ROLE: PRIMARY PRODUCER", "DANGER: FIRE SPREAD"},
    {4u, 0u, 4u, 255u, 500, 32767, 32767, 32767, 32767, 32767, 8u, MaterialPhase::gas, "Heat", "STRONG: HEAT CARRIER", "WEAK: STEAM / PLANTS", "TO: DIRTY STEAM / CO2", "ROLE: CARBON TRANSPORT", "DANGER: SUFFOCATION"},
    {5u, 0u, 5u, 255u, 600, 32767, 32767, 32767, 32767, 32767, 12u, MaterialPhase::gas, "Water", "STRONG: PRESSURE / HEAT", "WEAK: COLD SURFACES", "TO: WATER / DIRTY STEAM", "ROLE: WATER VAPOR", "DANGER: BURNS / PRESSURE"},
    {2u, 0u, 2u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 4u, MaterialPhase::plasma, "Heat", "STRONG: IGNITION / HEAT", "WEAK: WATER / OXYGEN LOSS", "TO: SMOKE / EMBER / STEAM", "ROLE: COMBUSTION ENERGY", "DANGER: EXTREME FIRE"},
    {205u, 0u, 205u, 255u, 900, 32767, 700, 2600, 2600, 32767, 115u, MaterialPhase::powder, "Heat", "STRONG: HEAT / SEMI-SOLID SLUMP", "WEAK: ISOLATED WATER COOLING", "TO: STONE / STEAM", "ROLE: DENSE GEOLOGIC SEMI-SOLID", "DANGER: ENTOMBMENT / FIRE"},
    {25u, 0u, 25u, 205u, 240, 32767, -40, 300, 420, 220, 24u, MaterialPhase::liquid, "Heat", "STRONG: ENERGY DENSE", "WEAK: FIRE / ACID", "TO: FIRE / SMOKE / CO2", "ROLE: FUEL FEEDSTOCK", "DANGER: FLAMMABLE FLOOD"},
    {170u, 145u, 170u, 220u, 220, 180, 32767, 32767, 600, 300, 16u, MaterialPhase::solid, "Life", "STRONG: LIGHT SUPPORT", "WEAK: FIRE / ROT", "TO: EMBER / ASH / SILT", "ROLE: RENEWABLE STRUCTURE", "DANGER: STRUCTURE FIRE"},
    {160u, 150u, 160u, 238u, 180, 105, 165, 320, 450, 360, 10u, MaterialPhase::solid, "Materials", "STRONG: LIGHT / WATERPROOF", "WEAK: HEAT / ACID", "TO: OIL / SMOKE / WASTE", "ROLE: POLYMER PART", "DANGER: TOXIC SMOKE"},
    {180u, 170u, 180u, 255u, 220, 150, 230, 420, 520, 480, 9u, MaterialPhase::solid, "Materials", "STRONG: ACID / WATER", "WEAK: HEAT / IMPACT", "TO: WASTE / SMOKE", "ROLE: CHEMICAL LINER", "DANGER: TOXIC FIRE"},
    {58u, 0u, 58u, 200u, 95, 32767, -15, 110, 150, 300, 42u, MaterialPhase::liquid, "Colony", "STRONG: FOOD / VISCOSITY", "WEAK: HEAT / WATER", "TO: WAX / FOOD / WASTE", "ROLE: COLONY ENERGY", "DANGER: STICKY FLOOD"},
    {8u, 0u, 8u, 110u, 55, 32767, 32767, 32767, 120, 32767, 3u, MaterialPhase::solid, "Colony", "STRONG: POLLINATION", "WEAK: SMOKE / ACID / FIRE", "TO: POLLEN / WASTE", "ROLE: COLONY WORKER", "DANGER: SWARM"},
    {76u, 8u, 76u, 255u, 800, 780, 801, 1465, 1465, 32767, 30u, MaterialPhase::powder, "Water", "STRONG: PRESERVATION", "WEAK: WATER DISSOLUTION", "TO: SALTWATER / CRYSTAL", "ROLE: DISSOLVED MINERAL", "DANGER: SOIL DAMAGE"},
    {120u, 80u, 40u, 240u, 2, 32767, 0, 100, 100, 32767, 80u, MaterialPhase::solid, "Water", "STRONG: COOLING / FLOATING", "WEAK: HEAT / BRINE", "TO: WATER", "ROLE: FROZEN STORAGE", "DANGER: SLIP / BLOCKAGE"},
    {150u, 118u, 150u, 236u, 660, 500, 660, 2470, 2470, 32767, 240u, MaterialPhase::solid, "Materials", "STRONG: LIGHT / CONDUCTION", "WEAK: ACID / EXTREME HEAT", "TO: SHAVINGS / PARTS", "ROLE: LIGHT ENGINEERING METAL", "DANGER: HOT METAL"},
    {28u, 4u, 28u, 180u, 500, 32767, 32767, 32767, 1100, 32767, 12u, MaterialPhase::powder, "Heat", "STRONG: COMPOST MINERAL", "WEAK: NEEDS WASTE / SILT", "TO: FERTILIZER WITH DIRTY WATER", "ROLE: COMPOST INPUT", "DANGER: RESPIRATORY DUST"},
    {18u, 0u, 18u, 210u, 0, 32767, 32767, 32767, 900, 32767, 18u, MaterialPhase::powder, "Heat", "STRONG: STORED HEAT", "WEAK: WATER / AGE", "TO: ASH ONLY / FIRE", "ROLE: NOT FERTILIZER", "DANGER: REIGNITION"},
    {190u, 160u, 190u, 255u, 700, 600, 1400, 32767, 2500, 32767, 45u, MaterialPhase::solid, "Terrain", "STRONG: ACID / TRANSPARENCY", "WEAK: IMPACT / SHOCK", "TO: SHARDS / SAND", "ROLE: WINDOW / VESSEL", "DANGER: SHARP BREAKAGE"},
    {81u, 6u, 81u, 175u, 150, 95, 32767, 32767, 380, 150, 8u, MaterialPhase::powder, "Heat", "STRONG: RAPID ENERGY", "WEAK: FIRE / SPARK", "TO: FIRE / SMOKE / ASH", "ROLE: EXPLOSIVE FEED", "DANGER: EXPLOSIVE"},
    {32u, 2u, 32u, 220u, 0, 32767, 0, 100, 100, 32767, 50u, MaterialPhase::powder, "Water", "STRONG: COOLING / INSULATION", "WEAK: HEAT / SALT", "TO: WATER / ICE", "ROLE: WATER STORAGE", "DANGER: BURIAL"},
    {38u, 6u, 38u, 180u, 90, 70, 32767, 32767, 260, 190, 8u, MaterialPhase::powder, "Life", "STRONG: REPRODUCTION", "WEAK: SALT / ACID / FIRE", "TO: GRASS / FLOWER", "ROLE: PLANT START", "DANGER: NONE"},
    {145u, 115u, 145u, 230u, 180, 48, 63, 250, 300, 204, 18u, MaterialPhase::solid, "Colony", "STRONG: WATER SEAL", "WEAK: HEAT / FIRE", "TO: NEST / EMBER", "ROLE: COLONY STRUCTURE", "DANGER: FLAMMABLE"},
    {76u, 8u, 76u, 145u, 80, 65, 32767, 32767, 240, 180, 7u, MaterialPhase::solid, "Life", "STRONG: POLLEN / FOOD", "WEAK: DARK / SALT / FIRE", "TO: FOOD / WASTE", "ROLE: POLLINATOR FEED", "DANGER: NONE"},
    {50u, 0u, 50u, 212u, 102, 32767, -21, 102, 102, 32767, 135u, MaterialPhase::liquid, "Water", "STRONG: HEAT CAPACITY", "WEAK: EVAPORATION / ICE", "TO: STEAM / SALT / WATER", "ROLE: BRINE SOLVENT", "DANGER: CORROSIVE FLOOD"},
    {175u, 130u, 175u, 225u, 220, 75, 145, 300, 420, 230, 15u, MaterialPhase::solid, "Colony", "STRONG: COLONY SHELTER", "WEAK: FIRE / ACID", "TO: WAX / HONEY / WASTE", "ROLE: COLONY HOME", "DANGER: DEFENSIVE SWARM"},
    {6u, 0u, 6u, 255u, 500, 32767, 32767, 32767, 32767, 32767, 10u, MaterialPhase::gas, "Water", "STRONG: CARRIES CARBON", "WEAK: COLD / FILTERS", "TO: DIRTY WATER", "ROLE: WASTE VAPOR", "DANGER: TOXIC BURNS"},
    {46u, 0u, 46u, 208u, 100, 32767, -2, 100, 100, 32767, 125u, MaterialPhase::liquid, "Water", "STRONG: NUTRIENT CARRIER", "WEAK: SETTLING / FILTERS", "TO: WATER + SILT", "ROLE: WASTE STREAM", "DANGER: CONTAMINATION"},
    {12u, 1u, 12u, 170u, 70, 55, 32767, 32767, 210, 160, 6u, MaterialPhase::powder, "Life", "STRONG: COLONY FEED", "WEAK: FIRE / WATER", "TO: HONEY / FOOD", "ROLE: BIOLOGIC INPUT", "DANGER: RESPIRATORY"},
    {30u, 0u, 30u, 125u, 60, 32767, 32767, 32767, 120, 32767, 3u, MaterialPhase::solid, "Colony", "STRONG: COLONY CONTROL", "WEAK: HEAT / TOXINS", "TO: WASTE / NEW NEST", "ROLE: COLONY REPRODUCTION", "DANGER: COLONY LOSS"},
    {88u, 80u, 225u, 244u, 1538, 1200, 1538, 2862, 2862, 32767, 200u, MaterialPhase::solid, "Materials", "STRONG: MAGNETIC / SUPPORT", "WEAK: ACID / CORROSION", "TO: STEEL / SCRAP", "ROLE: MACHINE FEED", "DANGER: HEAVY FALL"},
    {86u, 78u, 215u, 242u, 1085, 850, 1085, 2562, 2562, 32767, 255u, MaterialPhase::solid, "Materials", "STRONG: CONDUCTION", "WEAK: ACID / HEAT", "TO: CIRCUITS / SCRAP", "ROLE: POWER NETWORK", "DANGER: ELECTRIC SHOCK"},
    {210u, 190u, 230u, 240u, 800, 650, 800, 2500, 2500, 32767, 180u, MaterialPhase::solid, "Engineering", "STRONG: FIELD CONTROL", "WEAK: HEAT / IMPACT", "TO: SCRAP", "ROLE: METAL SEPARATION", "DANGER: PINCH / FIELD"},
    {180u, 170u, 180u, 255u, 600, 450, 900, 32767, 2200, 650, 2u, MaterialPhase::solid, "Materials", "STRONG: ELECTRIC ISOLATION", "WEAK: HEAT / IMPACT", "TO: WASTE", "ROLE: CIRCUIT SAFETY", "DANGER: BREAKDOWN"},
    {1u, 0u, 1u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 255u, MaterialPhase::plasma, "Engineering", "STRONG: ELECTRIC ENERGY", "WEAK: GROUND / INSULATION", "TO: HEAT / CHARGE", "ROLE: TRANSIENT FORCE", "DANGER: LETHAL VOLTAGE"},
    {255u, 255u, 255u, 255u, 2000, 32767, 32767, 32767, 32767, 32767, 255u, MaterialPhase::solid, "Heat", "STRONG: PERMANENT HEAT", "WEAK: NONE", "TO: LAVA / STONE", "ROLE: GEOLOGIC SOURCE", "DANGER: EXTREME HEAT"},
    {230u, 215u, 230u, 250u, 1132, 1000, 1132, 4131, 4131, 32767, 115u, MaterialPhase::solid, "Industry", "STRONG: ENERGY DENSITY", "WEAK: SHIELDING / ACID", "TO: HEAT / RADIATION", "ROLE: NUCLEAR FUEL", "DANGER: RADIATION"},
    {1u, 0u, 1u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 0u, MaterialPhase::gas, "Engineering", "STRONG: PENETRATION", "WEAK: METAL SHIELDING", "TO: HEAT / DAMAGE", "ROLE: NUCLEAR FIELD", "DANGER: IONIZING"},
    {62u, 18u, 150u, 220u, 660, 500, 660, 2470, 2470, 32767, 220u, MaterialPhase::powder, "Materials", "STRONG: SIFTING / RECYCLING", "WEAK: ACID / HEAT", "TO: ALUMINUM PARTS", "ROLE: DENSE MACHINING FEED", "DANGER: SHARP PARTICLES"},
    {96u, 90u, 210u, 246u, 1064, 850, 1064, 2856, 2856, 32767, 250u, MaterialPhase::solid, "Materials", "STRONG: CONDUCTION / ACID", "WEAK: SOFTNESS", "TO: CIRCUITS / SCRAP", "ROLE: ADVANCED COMPONENT", "DANGER: HEAVY FALL"},
    {7u, 0u, 7u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 4u, MaterialPhase::gas, "Engineering", "STRONG: RESPIRATION", "WEAK: FIRE CONSUMPTION", "TO: CO2 / STEAM", "ROLE: BREATHABLE GAS", "DANGER: FIRE ACCELERATION"},
    {11u, 0u, 11u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 4u, MaterialPhase::gas, "Engineering", "STRONG: FIRE SUPPRESSION", "WEAK: PLANTS / WATER", "TO: OXYGEN / FOOD", "ROLE: CARBON RESERVOIR", "DANGER: ASPHYXIATION"},
    {86u, 24u, 225u, 220u, 1538, 1200, 1538, 2862, 2862, 32767, 180u, MaterialPhase::powder, "Materials", "STRONG: MAGNETIC SIFTING", "WEAK: ACID / CORROSION", "TO: IRON / STEEL", "ROLE: DENSE MACHINING FEED", "DANGER: SHARP PARTICLES"},
    {245u, 240u, 245u, 252u, 1450, 1250, 1450, 2900, 2900, 32767, 185u, MaterialPhase::solid, "Materials", "STRONG: SUPPORT / TOUGHNESS", "WEAK: EXTREME HEAT", "TO: SCRAP / MACHINES", "ROLE: PRIMARY STRUCTURE", "DANGER: HEAVY COLLAPSE"},
    {220u, 215u, 220u, 250u, 850, 700, 1350, 2800, 2800, 32767, 190u, MaterialPhase::solid, "Industry", "STRONG: MATERIAL ROUTING", "WEAK: POWER LOSS / DAMAGE", "TO: SCRAP", "ROLE: FACTORY TRANSPORT", "DANGER: PINCH POINT"},
    {240u, 230u, 240u, 252u, 1500, 1000, 1500, 3000, 3000, 32767, 175u, MaterialPhase::solid, "Industry", "STRONG: METAL PROCESSING", "WEAK: COOLANT LOSS", "TO: STEEL / SMOKE / SLAG", "ROLE: THERMAL FACTORY", "DANGER: HEAT / FIRE"},
    {225u, 215u, 225u, 250u, 700, 550, 1200, 2600, 2600, 32767, 170u, MaterialPhase::solid, "Industry", "STRONG: COMPONENT BUILD", "WEAK: POWER LOSS", "TO: POWER CELLS / AMMO", "ROLE: MANUFACTURING", "DANGER: MOVING PARTS"},
    {175u, 145u, 175u, 220u, 180, 80, 180, 32767, 420, 230, 12u, MaterialPhase::solid, "Colony", "STRONG: SHELTER / PHEROMONES", "WEAK: FIRE / ACID / FLOODING", "TO: ANTS / FERTILIZER / WASTE", "ROLE: DEFINED ANT HOME", "DANGER: INFESTATION"},
    {92u, 40u, 92u, 210u, 220, 120, 220, 350, 450, 180, 150u, MaterialPhase::solid, "Engineering", "STRONG: STORED ENERGY", "WEAK: HEAT / IMPACT", "TO: AMMO / FIRE / SCRAP", "ROLE: ENERGY STORAGE", "DANGER: THERMAL RUNAWAY"},
    {93u, 35u, 93u, 205u, 180, 110, 180, 330, 430, 160, 130u, MaterialPhase::solid, "Engineering", "STRONG: WEAPON ENERGY", "WEAK: HEAT / IMPACT", "TO: PLASMA BOLT / FIRE", "ROLE: FUTURE WEAPON FEED", "DANGER: EXPLOSIVE"},
    {10u, 0u, 10u, 120u, 65, 32767, 32767, 32767, 120, 32767, 2u, MaterialPhase::solid, "Colony", "STRONG: WASTE TRANSPORT", "WEAK: WATER / FIRE / ACID", "TO: FERTILIZER / WASTE", "ROLE: DETRITIVORE", "DANGER: NONE"},
    {18u, 2u, 18u, 145u, 90, 70, 32767, 32767, 160, 32767, 3u, MaterialPhase::solid, "Colony", "STRONG: DEAD MATTER BREAKDOWN", "WEAK: FIRE / ACID / COLD", "TO: FERTILIZER / WASTE", "ROLE: DECOMPOSER", "DANGER: CROP DAMAGE"},
    {70u, 12u, 70u, 140u, 90, 60, 32767, 32767, 240, 180, 7u, MaterialPhase::solid, "Life", "STRONG: FLOWER SUPPORT", "WEAK: DARK / FIRE / SALT", "TO: FLOWER / WASTE", "ROLE: VASCULAR PLANT GROWTH", "DANGER: NONE"},
    {254u, 250u, 254u, 254u, 1200, 950, 1450, 2900, 2900, 32767, 220u, MaterialPhase::solid, "Industry", "STRONG: FACTORY CONTROL", "WEAK: POWER LOSS / DAMAGE", "TO: SCRAP / SHUTDOWN", "ROLE: PRODUCTION CONTROL", "DANGER: SYSTEM FAILURE"},
    {60u, 18u, 60u, 178u, 250, 32767, 32767, 100, 600, 32767, 25u, MaterialPhase::powder, "Terrain", "STRONG: NUTRIENT STORAGE", "WEAK: FAST WATER / DRYING", "TO: FERTILIZER / SOIL", "ROLE: SETTLED SEDIMENT", "DANGER: CLOGGING"},
    {55u, 8u, 55u, 160u, 160, 85, 32767, 32767, 350, 190, 14u, MaterialPhase::powder, "Life", "STRONG: CROP GROWTH", "WEAK: WATER LOSS / FIRE", "TO: FOOD / DIRT / SMOKE", "ROLE: ASH+WASTE+SILT COMPOST", "DANGER: RUNOFF"},
    {42u, 4u, 42u, 145u, 100, 70, 32767, 32767, 260, 180, 11u, MaterialPhase::powder, "Life", "STRONG: LIFE SUPPORT", "WEAK: AGE / FIRE", "TO: WASTE / SMOKE", "ROLE: STORED BIOMASS", "DANGER: SPOILAGE"},
    {58u, 10u, 58u, 150u, 180, 85, 32767, 32767, 400, 210, 12u, MaterialPhase::powder, "Industry", "STRONG: RECOVERABLE CARBON", "WEAK: HEAT / WATER", "TO: SOIL / DIRTY WATER / SMOKE", "ROLE: RECYCLE FEED", "DANGER: BIOHAZARD"},
    {1u, 0u, 1u, 255u, 0, 32767, 32767, 32767, 32767, 560, 180u, MaterialPhase::gas, "Engineering", "STRONG: LIGHT FUEL GAS", "WEAK: IGNITION / CONTAINMENT", "TO: STEAM / FIRE", "ROLE: ENERGY CARRIER", "DANGER: EXPLOSIVE GAS"},
    {210u, 196u, 210u, 248u, 900, 760, 1420, 2850, 2850, 32767, 165u, MaterialPhase::solid, "Industry", "STRONG: WET SAND SEPARATION", "WEAK: DRY FEED / DAMAGE", "TO: GOLD + WATER", "ROLE: GRAVITY MINERAL PROCESSOR", "DANGER: PINCH / FLOOD"},
    {2u, 0u, 2u, 255u, 0, 32767, 32767, 32767, 32767, 32767, 4u, MaterialPhase::gas, "Unknown", "STRONG: BALANCED BREATHABLE AIR", "WEAK: PRESSURE / CONTAMINATION", "TO: CO2 / VAPOR / EXCESS GAS", "ROLE: N2/O2/AR BASELINE", "DANGER: LOW OXYGEN WHEN DEPLETED"},
}};

[[nodiscard]] constexpr const MaterialProfile& material_profile(const Material material) noexcept {
    const auto index = static_cast<std::uint32_t>(material);
    return material_profiles[index < material_profiles.size() ? index : 0u];
}

[[nodiscard]] constexpr MaterialPhase phase_at(const Material material, const std::int32_t temperature) noexcept {
    const auto& profile = material_profile(material);
    if (profile.vaporization_point != no_temperature && temperature >= profile.vaporization_point)
        return MaterialPhase::vapor;
    if (profile.melting_point != no_temperature && temperature >= profile.melting_point &&
        (profile.base_phase == MaterialPhase::solid || profile.base_phase == MaterialPhase::powder))
        return MaterialPhase::molten;
    if (profile.softening_point != no_temperature && temperature >= profile.softening_point &&
        (profile.base_phase == MaterialPhase::solid || profile.base_phase == MaterialPhase::powder))
        return MaterialPhase::softened;
    return profile.base_phase;
}

enum class MaterialGroup : std::uint32_t {
    ground = 0,
    fluids = 1,
    life = 2,
    colony = 3,
    fire_chemistry = 4,
    materials = 5,
    engineering = 6,
    industry = 7,
    count
};

inline constexpr auto material_group_count = static_cast<std::uint32_t>(MaterialGroup::count);
inline constexpr std::uint32_t material_slots_per_group = 10u;

inline constexpr std::array<std::uint32_t, material_group_count> material_group_slot_counts{
    7u,
    8u,
    8u,
    8u,
    9u,
    10u,
    8u,
    7u,
};

inline constexpr std::array<std::string_view, material_group_count> material_group_names{
    "Terrain",
    "Water",
    "Life",
    "Colony",
    "Heat",
    "Materials",
    "Engineering",
    "Industry",
};

inline constexpr std::array<std::array<Material, material_slots_per_group>, material_group_count> material_groups{{
    {Material::sand, Material::dirt, Material::stone, Material::mud, Material::silt, Material::crystal, Material::glass, Material::count, Material::count, Material::count},
    {Material::water, Material::saltwater, Material::dirty_water, Material::ice, Material::snow, Material::steam, Material::dirty_steam, Material::salt, Material::count, Material::count},
    {Material::grass, Material::seed, Material::plant_stem, Material::flower, Material::wood, Material::food, Material::fertilizer, Material::pollen, Material::count, Material::count},
    {Material::honey, Material::bee, Material::queen_bee, Material::beehive, Material::beeswax, Material::ant, Material::beetle, Material::insect_habitat, Material::count, Material::count},
    {Material::fire, Material::lava, Material::oil, Material::ember, Material::ash, Material::gunpowder, Material::acid, Material::magma_vent, Material::smoke, Material::count},
    {Material::plastic, Material::acid_resistant_plastic, Material::aluminum, Material::aluminum_shavings, Material::iron, Material::iron_ore, Material::copper, Material::gold, Material::steel, Material::insulator},
    {Material::magnet, Material::lightning, Material::power_cell, Material::plasma_ammo, Material::oxygen, Material::carbon_dioxide, Material::hydrogen, Material::radiation, Material::count, Material::count},
    {Material::conveyor, Material::smelter, Material::assembler, Material::factory_core, Material::uranium, Material::waste, Material::sluice_box, Material::count, Material::count, Material::count},
}};

[[nodiscard]] constexpr std::string_view material_group_name(const MaterialGroup group) noexcept {
    const auto index = static_cast<std::uint32_t>(group);
    return index < material_group_names.size() ? material_group_names[index] : "Unknown";
}

[[nodiscard]] constexpr std::uint32_t material_group_size(const MaterialGroup group) noexcept {
    const auto index = static_cast<std::uint32_t>(group);
    return index < material_group_slot_counts.size() ? material_group_slot_counts[index] : 0u;
}

[[nodiscard]] constexpr Material grouped_material(const MaterialGroup group, const std::uint32_t slot) noexcept {
    const auto group_index = static_cast<std::uint32_t>(group);
    if (group_index >= material_groups.size() || slot >= material_group_size(group)) return Material::count;
    return material_groups[group_index][slot];
}

[[nodiscard]] constexpr bool is_block_material(const Material material) noexcept {
    switch (material) {
    case Material::stone:
    case Material::crystal:
    case Material::wood:
    case Material::plastic:
    case Material::acid_resistant_plastic:
    case Material::aluminum:
    case Material::glass:
    case Material::iron:
    case Material::iron_ore:
    case Material::copper:
    case Material::gold:
    case Material::magnet:
    case Material::insulator:
    case Material::uranium:
    case Material::steel:
    case Material::conveyor:
    case Material::smelter:
    case Material::assembler:
    case Material::insect_habitat:
    case Material::factory_core:
    case Material::sluice_box:
        return true;
    default:
        return false;
    }
}

} // namespace sandhybrid
