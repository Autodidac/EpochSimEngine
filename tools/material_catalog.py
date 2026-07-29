"""Single source of truth for EpochSand material UI/catalog metadata."""
MATERIALS = [
    ('empty', 'Eraser', 0, 0, 2000, 255, 'OPEN SPACE', 'ANY MATERIAL', 'TO: PLACED MATERIAL', 'ROLE: VACUUM / AIR GAP', 'DANGER: DECOMPRESSION'),
    ('sand', 'Sand', 84, 52, 1050, 255, 'STRONG: PACKING / FILTERING', 'WEAK: FLOWING WATER', 'TO: GLASS / SILT', 'ROLE: MINERAL MATRIX', 'DANGER: BURIAL'),
    ('water', 'Water', 44, 0, 100, 214, 'STRONG: COOLING / FLOW', 'WEAK: HEAT / CONTAMINATION', 'TO: STEAM / ICE / MUD', 'ROLE: CLEAN PROCESS FLUID', 'DANGER: FLOOD / DROWNING'),
    ('dirt', 'Dirt', 80, 48, 320, 190, 'STRONG: FARM SUPPORT', 'WEAK: SATURATION / ACID', 'TO: MUD / GRASS / SILT', 'ROLE: SOIL RESERVOIR', 'DANGER: CAVE IN'),
    ('stone', 'Stone', 235, 230, 1350, 252, 'STRONG: SUPPORT / HEAT', 'WEAK: FRACTURE / ACID', 'TO: RUBBLE / LAVA ROCK', 'ROLE: STRUCTURAL MASS', 'DANGER: FALLING BLOCK'),
    ('crystal', 'Crystal', 205, 180, 900, 255, 'STRONG: ACID / OPTICS', 'WEAK: IMPACT / THERMAL SHOCK', 'TO: SHARDS / GLASS', 'ROLE: TECH MATERIAL', 'DANGER: SHARP FRAGMENTS'),
    ('mud', 'Mud', 68, 22, 180, 185, 'STRONG: WATER STORAGE', 'WEAK: DRYING / LOAD', 'TO: DIRT / SILT', 'ROLE: WET SOIL LAYER', 'DANGER: ENTOMBMENT'),
    ('acid', 'Acid', 48, 0, 110, 255, 'STRONG: CORROSION', 'WEAK: DILUTION / GLASS', 'TO: DIRTY WATER / SALT', 'ROLE: CHEMICAL REAGENT', 'DANGER: TOXIC CORROSIVE'),
    ('grass', 'Grass', 64, 18, 120, 165, 'STRONG: CO2 CAPTURE', 'WEAK: SALT / FIRE / DARK', 'TO: FOOD / WASTE / ASH', 'ROLE: PRIMARY PRODUCER', 'DANGER: FIRE SPREAD'),
    ('smoke', 'Smoke', 4, 0, 500, 255, 'STRONG: HEAT CARRIER', 'WEAK: STEAM / PLANTS', 'TO: DIRTY STEAM / CO2', 'ROLE: CARBON TRANSPORT', 'DANGER: SUFFOCATION'),
    ('steam', 'Steam', 5, 0, 600, 255, 'STRONG: PRESSURE / HEAT', 'WEAK: COLD SURFACES', 'TO: WATER / DIRTY STEAM', 'ROLE: WATER VAPOR', 'DANGER: BURNS / PRESSURE'),
    ('fire', 'Fire', 2, 0, 0, 255, 'STRONG: IGNITION / HEAT', 'WEAK: WATER / OXYGEN LOSS', 'TO: SMOKE / EMBER / STEAM', 'ROLE: COMBUSTION ENERGY', 'DANGER: EXTREME FIRE'),
    ('lava', 'Lava', 72, 0, 900, 255, 'STRONG: HEAT / FLOW', 'WEAK: WATER COOLING', 'TO: STONE / STEAM', 'ROLE: GEOLOGIC HEAT', 'DANGER: MELTING / FIRE'),
    ('oil', 'Oil', 25, 0, 240, 205, 'STRONG: ENERGY DENSE', 'WEAK: FIRE / ACID', 'TO: FIRE / SMOKE / CO2', 'ROLE: FUEL FEEDSTOCK', 'DANGER: FLAMMABLE FLOOD'),
    ('wood', 'Wood', 170, 145, 220, 220, 'STRONG: LIGHT SUPPORT', 'WEAK: FIRE / ROT', 'TO: EMBER / ASH / SILT', 'ROLE: RENEWABLE STRUCTURE', 'DANGER: STRUCTURE FIRE'),
    ('plastic', 'Plastic', 160, 150, 180, 238, 'STRONG: LIGHT / WATERPROOF', 'WEAK: HEAT / ACID', 'TO: OIL / SMOKE / WASTE', 'ROLE: POLYMER PART', 'DANGER: TOXIC SMOKE'),
    ('acid_resistant_plastic', 'AR plastic', 180, 170, 220, 255, 'STRONG: ACID / WATER', 'WEAK: HEAT / IMPACT', 'TO: WASTE / SMOKE', 'ROLE: CHEMICAL LINER', 'DANGER: TOXIC FIRE'),
    ('honey', 'Honey', 58, 0, 95, 200, 'STRONG: FOOD / VISCOSITY', 'WEAK: HEAT / WATER', 'TO: WAX / FOOD / WASTE', 'ROLE: COLONY ENERGY', 'DANGER: STICKY FLOOD'),
    ('bee', 'Bee', 8, 0, 55, 110, 'STRONG: POLLINATION', 'WEAK: SMOKE / ACID / FIRE', 'TO: POLLEN / WASTE', 'ROLE: COLONY WORKER', 'DANGER: SWARM'),
    ('salt', 'Salt', 76, 8, 800, 255, 'STRONG: PRESERVATION', 'WEAK: WATER DISSOLUTION', 'TO: SALTWATER / CRYSTAL', 'ROLE: DISSOLVED MINERAL', 'DANGER: SOIL DAMAGE'),
    ('ice', 'Ice', 120, 80, 2, 240, 'STRONG: COOLING / FLOATING', 'WEAK: HEAT / BRINE', 'TO: WATER', 'ROLE: FROZEN STORAGE', 'DANGER: SLIP / BLOCKAGE'),
    ('metal', 'Metal', 235, 230, 1450, 248, 'STRONG: SUPPORT / CONDUCTION', 'WEAK: ACID / LIGHTNING', 'TO: SCRAP / OXIDE', 'ROLE: ENGINEERING FRAME', 'DANGER: ELECTRIC SHOCK'),
    ('ash', 'Ash', 28, 4, 500, 180, 'STRONG: MINERAL FEED', 'WEAK: WATER / WIND', 'TO: FERTILIZER / SILT', 'ROLE: RECYCLED MINERALS', 'DANGER: RESPIRATORY DUST'),
    ('ember', 'Ember', 18, 0, 0, 210, 'STRONG: STORED HEAT', 'WEAK: WATER / AGE', 'TO: ASH / FIRE', 'ROLE: COMBUSTION INTERMEDIATE', 'DANGER: REIGNITION'),
    ('glass', 'Glass', 190, 160, 700, 255, 'STRONG: ACID / TRANSPARENCY', 'WEAK: IMPACT / SHOCK', 'TO: SHARDS / SAND', 'ROLE: WINDOW / VESSEL', 'DANGER: SHARP BREAKAGE'),
    ('gunpowder', 'Gunpowder', 81, 6, 150, 175, 'STRONG: RAPID ENERGY', 'WEAK: FIRE / SPARK', 'TO: FIRE / SMOKE / ASH', 'ROLE: EXPLOSIVE FEED', 'DANGER: EXPLOSIVE'),
    ('snow', 'Snow', 32, 2, 0, 220, 'STRONG: COOLING / INSULATION', 'WEAK: HEAT / SALT', 'TO: WATER / ICE', 'ROLE: WATER STORAGE', 'DANGER: BURIAL'),
    ('seed', 'Seed', 38, 6, 90, 180, 'STRONG: REPRODUCTION', 'WEAK: SALT / ACID / FIRE', 'TO: GRASS / FLOWER', 'ROLE: PLANT START', 'DANGER: NONE'),
    ('beeswax', 'Beeswax', 145, 115, 180, 230, 'STRONG: WATER SEAL', 'WEAK: HEAT / FIRE', 'TO: NEST / EMBER', 'ROLE: COLONY STRUCTURE', 'DANGER: FLAMMABLE'),
    ('flower', 'Flower', 76, 8, 80, 145, 'STRONG: POLLEN / FOOD', 'WEAK: DARK / SALT / FIRE', 'TO: FOOD / WASTE', 'ROLE: POLLINATOR FEED', 'DANGER: NONE'),
    ('saltwater', 'Saltwater', 50, 0, 102, 212, 'STRONG: HEAT CAPACITY', 'WEAK: EVAPORATION / ICE', 'TO: STEAM / SALT / WATER', 'ROLE: BRINE SOLVENT', 'DANGER: CORROSIVE FLOOD'),
    ('bee_nest', 'Bee nest', 175, 130, 220, 225, 'STRONG: COLONY SHELTER', 'WEAK: FIRE / ACID', 'TO: WAX / HONEY / WASTE', 'ROLE: COLONY HOME', 'DANGER: DEFENSIVE SWARM'),
    ('dirty_steam', 'Dirty steam', 6, 0, 500, 255, 'STRONG: CARRIES CARBON', 'WEAK: COLD / FILTERS', 'TO: DIRTY WATER', 'ROLE: WASTE VAPOR', 'DANGER: TOXIC BURNS'),
    ('dirty_water', 'Dirty water', 46, 0, 100, 208, 'STRONG: NUTRIENT CARRIER', 'WEAK: SETTLING / FILTERS', 'TO: WATER + SILT', 'ROLE: WASTE STREAM', 'DANGER: CONTAMINATION'),
    ('pollen', 'Pollen', 12, 1, 70, 170, 'STRONG: COLONY FEED', 'WEAK: FIRE / WATER', 'TO: HONEY / FOOD', 'ROLE: BIOLOGIC INPUT', 'DANGER: RESPIRATORY'),
    ('queen_bee', 'Queen', 30, 0, 60, 125, 'STRONG: COLONY CONTROL', 'WEAK: HEAT / TOXINS', 'TO: WASTE / NEW NEST', 'ROLE: COLONY REPRODUCTION', 'DANGER: COLONY LOSS'),
    ('iron', 'Iron', 88, 80, 1538, 244, 'STRONG: MAGNETIC / SUPPORT', 'WEAK: ACID / CORROSION', 'TO: STEEL / SCRAP', 'ROLE: MACHINE FEED', 'DANGER: HEAVY FALL'),
    ('copper', 'Copper', 86, 78, 1085, 242, 'STRONG: CONDUCTION', 'WEAK: ACID / HEAT', 'TO: CIRCUITS / SCRAP', 'ROLE: POWER NETWORK', 'DANGER: ELECTRIC SHOCK'),
    ('magnet', 'Magnet', 210, 190, 800, 240, 'STRONG: FIELD CONTROL', 'WEAK: HEAT / IMPACT', 'TO: SCRAP', 'ROLE: ORE SEPARATION', 'DANGER: PINCH / FIELD'),
    ('insulator', 'Insulator', 180, 170, 600, 255, 'STRONG: ELECTRIC ISOLATION', 'WEAK: HEAT / IMPACT', 'TO: WASTE', 'ROLE: CIRCUIT SAFETY', 'DANGER: BREAKDOWN'),
    ('lightning', 'Lightning', 1, 0, 0, 255, 'STRONG: ELECTRIC ENERGY', 'WEAK: GROUND / INSULATION', 'TO: HEAT / CHARGE', 'ROLE: TRANSIENT FORCE', 'DANGER: LETHAL VOLTAGE'),
    ('magma_vent', 'Magma vent', 255, 255, 2000, 255, 'STRONG: PERMANENT HEAT', 'WEAK: NONE', 'TO: LAVA / STONE', 'ROLE: GEOLOGIC SOURCE', 'DANGER: EXTREME HEAT'),
    ('uranium', 'Uranium', 230, 215, 1132, 250, 'STRONG: ENERGY DENSITY', 'WEAK: SHIELDING / ACID', 'TO: HEAT / RADIATION', 'ROLE: NUCLEAR FUEL', 'DANGER: RADIATION'),
    ('radiation', 'Radiation', 1, 0, 0, 255, 'STRONG: PENETRATION', 'WEAK: METAL SHIELDING', 'TO: HEAT / DAMAGE', 'ROLE: NUCLEAR FIELD', 'DANGER: IONIZING'),
    ('gold_ore', 'Gold ore', 225, 205, 1064, 250, 'STRONG: MINERAL VALUE', 'WEAK: MINING DAMAGE', 'TO: GOLD / SILT', 'ROLE: RESOURCE DEPOSIT', 'DANGER: CAVE IN'),
    ('gold', 'Gold', 96, 90, 1064, 246, 'STRONG: CONDUCTION / ACID', 'WEAK: SOFTNESS', 'TO: CIRCUITS / SCRAP', 'ROLE: ADVANCED COMPONENT', 'DANGER: HEAVY FALL'),
    ('oxygen', 'Oxygen', 7, 0, 0, 255, 'STRONG: RESPIRATION', 'WEAK: FIRE CONSUMPTION', 'TO: CO2 / STEAM', 'ROLE: BREATHABLE GAS', 'DANGER: FIRE ACCELERATION'),
    ('carbon_dioxide', 'CO2', 11, 0, 0, 255, 'STRONG: FIRE SUPPRESSION', 'WEAK: PLANTS / WATER', 'TO: OXYGEN / FOOD', 'ROLE: CARBON RESERVOIR', 'DANGER: ASPHYXIATION'),
    ('iron_ore', 'Iron ore', 225, 205, 1450, 242, 'STRONG: IRON FEED', 'WEAK: MINING DAMAGE', 'TO: IRON / SILT', 'ROLE: RESOURCE DEPOSIT', 'DANGER: CAVE IN'),
    ('steel', 'Steel', 245, 240, 1450, 252, 'STRONG: SUPPORT / TOUGHNESS', 'WEAK: EXTREME HEAT', 'TO: SCRAP / MACHINES', 'ROLE: PRIMARY STRUCTURE', 'DANGER: HEAVY COLLAPSE'),
    ('conveyor', 'Conveyor', 220, 215, 850, 250, 'STRONG: MATERIAL ROUTING', 'WEAK: POWER LOSS / DAMAGE', 'TO: SCRAP', 'ROLE: FACTORY TRANSPORT', 'DANGER: PINCH POINT'),
    ('smelter', 'Smelter', 240, 230, 1500, 252, 'STRONG: ORE PROCESSING', 'WEAK: COOLANT LOSS', 'TO: STEEL / SMOKE / SLAG', 'ROLE: THERMAL FACTORY', 'DANGER: HEAT / FIRE'),
    ('assembler', 'Assembler', 225, 215, 700, 250, 'STRONG: COMPONENT BUILD', 'WEAK: POWER LOSS', 'TO: POWER CELLS / AMMO', 'ROLE: MANUFACTURING', 'DANGER: MOVING PARTS'),
    ('bot_fabricator', 'Bot fab', 230, 220, 700, 250, 'STRONG: BOT PRODUCTION', 'WEAK: POWER / INPUT LOSS', 'TO: ALLY / ENEMY BOT', 'ROLE: AUTOMATION', 'DANGER: HOSTILE OUTPUT'),
    ('power_cell', 'Power cell', 92, 40, 220, 210, 'STRONG: STORED ENERGY', 'WEAK: HEAT / IMPACT', 'TO: AMMO / FIRE / SCRAP', 'ROLE: ENERGY STORAGE', 'DANGER: THERMAL RUNAWAY'),
    ('plasma_ammo', 'Plasma ammo', 93, 35, 180, 205, 'STRONG: WEAPON ENERGY', 'WEAK: HEAT / IMPACT', 'TO: PLASMA BOLT / FIRE', 'ROLE: FUTURE WEAPON FEED', 'DANGER: EXPLOSIVE'),
    ('ally_bot', 'Ally bot', 180, 150, 450, 220, 'STRONG: COMBAT / LABOR', 'WEAK: ACID / PLASMA', 'TO: SCRAP / WASTE', 'ROLE: FRIENDLY AUTOMATION', 'DANGER: WEAPON FIRE'),
    ('enemy_bot', 'Enemy bot', 180, 150, 450, 220, 'STRONG: COMBAT / PRESSURE', 'WEAK: PLASMA / MAGNETS', 'TO: SCRAP', 'ROLE: HOSTILE FORCE', 'DANGER: HOSTILE'),
    ('plasma_bolt', 'Plasma bolt', 2, 0, 0, 255, 'STRONG: ARMOR DAMAGE', 'WEAK: RANGE / SHIELD', 'TO: HEAT / CHARGE', 'ROLE: PROJECTILE ENERGY', 'DANGER: EXTREME ENERGY'),
    ('factory_core', 'Factory core', 254, 250, 1200, 254, 'STRONG: FACTORY CONTROL', 'WEAK: POWER LOSS / DAMAGE', 'TO: SCRAP / SHUTDOWN', 'ROLE: PRODUCTION CONTROL', 'DANGER: SYSTEM FAILURE'),
    ('silt', 'Silt', 60, 18, 250, 178, 'STRONG: NUTRIENT STORAGE', 'WEAK: FAST WATER / DRYING', 'TO: FERTILIZER / DIRT', 'ROLE: SETTLED SEDIMENT', 'DANGER: CLOGGING'),
    ('fertilizer', 'Fertilizer', 55, 8, 160, 160, 'STRONG: CROP GROWTH', 'WEAK: WATER LOSS / FIRE', 'TO: FOOD / DIRT / SMOKE', 'ROLE: FARM NUTRIENT', 'DANGER: RUNOFF'),
    ('food', 'Food', 42, 4, 100, 145, 'STRONG: LIFE SUPPORT', 'WEAK: AGE / FIRE', 'TO: WASTE / SMOKE', 'ROLE: STORED BIOMASS', 'DANGER: SPOILAGE'),
    ('waste', 'Waste', 58, 10, 180, 150, 'STRONG: RECOVERABLE CARBON', 'WEAK: HEAT / WATER', 'TO: DIRT / DIRTY WATER / SMOKE', 'ROLE: RECYCLE FEED', 'DANGER: BIOHAZARD'),
    ('hydrogen', 'Hydrogen', 1, 0, 0, 255, 'STRONG: LIGHT FUEL GAS', 'WEAK: IGNITION / CONTAINMENT', 'TO: STEAM / FIRE', 'ROLE: ENERGY CARRIER', 'DANGER: EXPLOSIVE GAS'),
]
GROUPS = [
    ('ground', 'Terrain', [0, 1, 3, 4, 6, 60, 5, 24]),
    ('fluids', 'Water', [2, 30, 33, 20, 26, 10, 32, 19]),
    ('life', 'Life', [8, 27, 29, 14, 62, 63, 61, 34]),
    ('colony', 'Colony', [17, 18, 35, 31, 28, 46, 47, 9, 64]),
    ('fire_chemistry', 'Heat', [11, 12, 13, 23, 22, 25, 7, 41]),
    ('materials', 'Materials', [15, 16, 21, 36, 37, 45, 49, 39]),
    ('engineering', 'Engineering', [38, 40, 42, 43, 44, 48, 54, 55]),
    ('industry', 'Industry', [50, 51, 52, 53, 56, 57, 58, 59]),
]

BLOCK_MATERIALS = (
    'stone', 'crystal', 'wood', 'plastic', 'acid_resistant_plastic',
    'metal', 'glass', 'iron', 'copper', 'magnet', 'insulator', 'uranium',
    'gold_ore', 'iron_ore', 'steel', 'conveyor', 'smelter', 'assembler',
    'bot_fabricator', 'factory_core',
)

# Canonical physical configuration shared by C++ cards/tests and GLSL. Temperatures
# are degrees Celsius. 32767 means the transition does not apply in the bounded
# simulation. Material behavior never depends on placement provenance.
NO_TEMPERATURE = 32767
PHASE_EMPTY = 0
PHASE_SOLID = 1
PHASE_POWDER = 2
PHASE_LIQUID = 3
PHASE_GAS = 4
PHASE_PLASMA = 5

_BASE_LIQUIDS = {'water', 'acid', 'lava', 'oil', 'honey', 'saltwater', 'dirty_water'}
_BASE_GASES = {'smoke', 'steam', 'dirty_steam', 'oxygen', 'carbon_dioxide', 'radiation', 'hydrogen'}
_BASE_PLASMA = {'fire', 'lightning', 'plasma_bolt'}
_BASE_POWDERS = {
    'sand', 'dirt', 'mud', 'salt', 'ash', 'gunpowder', 'snow', 'seed', 'pollen',
    'silt', 'fertilizer', 'food', 'waste', 'ember'
}

# density, softening, melting, boiling, vaporization, ignition, conductivity
PHYSICS_OVERRIDES = {
    'empty': (0, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 0),
    'sand': (84, 1450, 1710, NO_TEMPERATURE, 2230, NO_TEMPERATURE, 20),
    'water': (44, NO_TEMPERATURE, 0, 100, 100, NO_TEMPERATURE, 140),
    'dirt': (80, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 1200, 450, 28),
    'stone': (235, 1200, 1500, NO_TEMPERATURE, 3200, NO_TEMPERATURE, 72),
    'crystal': (205, 760, 900, NO_TEMPERATURE, 2400, NO_TEMPERATURE, 85),
    'mud': (68, NO_TEMPERATURE, NO_TEMPERATURE, 100, 100, NO_TEMPERATURE, 60),
    'acid': (48, NO_TEMPERATURE, -20, 110, 110, NO_TEMPERATURE, 95),
    'grass': (64, 80, NO_TEMPERATURE, NO_TEMPERATURE, 350, 220, 14),
    'smoke': (4, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 8),
    'steam': (5, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 12),
    'fire': (2, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 4),
    'lava': (72, NO_TEMPERATURE, 700, 2600, 2600, NO_TEMPERATURE, 115),
    'oil': (25, NO_TEMPERATURE, -40, 300, 420, 220, 24),
    'wood': (170, 180, NO_TEMPERATURE, NO_TEMPERATURE, 600, 300, 16),
    'plastic': (160, 105, 165, 320, 450, 360, 10),
    'acid_resistant_plastic': (180, 150, 230, 420, 520, 480, 9),
    'honey': (58, NO_TEMPERATURE, -15, 110, 150, 300, 42),
    'bee': (8, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 120, NO_TEMPERATURE, 3),
    'salt': (76, 780, 801, 1465, 1465, NO_TEMPERATURE, 30),
    'ice': (40, NO_TEMPERATURE, 0, 100, 100, NO_TEMPERATURE, 80),
    'metal': (235, 950, 1250, 2700, 2700, NO_TEMPERATURE, 220),
    'ash': (28, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 1100, NO_TEMPERATURE, 12),
    'ember': (18, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 900, NO_TEMPERATURE, 18),
    'glass': (190, 600, 1400, NO_TEMPERATURE, 2500, NO_TEMPERATURE, 45),
    'gunpowder': (81, 95, NO_TEMPERATURE, NO_TEMPERATURE, 380, 150, 8),
    'snow': (32, NO_TEMPERATURE, 0, 100, 100, NO_TEMPERATURE, 50),
    'seed': (38, 70, NO_TEMPERATURE, NO_TEMPERATURE, 260, 190, 8),
    'beeswax': (145, 48, 63, 250, 300, 204, 18),
    'flower': (76, 65, NO_TEMPERATURE, NO_TEMPERATURE, 240, 180, 7),
    'saltwater': (50, NO_TEMPERATURE, -21, 102, 102, NO_TEMPERATURE, 135),
    'bee_nest': (175, 75, 145, 300, 420, 230, 15),
    'dirty_steam': (6, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 10),
    'dirty_water': (46, NO_TEMPERATURE, -2, 100, 100, NO_TEMPERATURE, 125),
    'pollen': (12, 55, NO_TEMPERATURE, NO_TEMPERATURE, 210, 160, 6),
    'queen_bee': (30, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 120, NO_TEMPERATURE, 3),
    'iron': (225, 1200, 1538, 2862, 2862, NO_TEMPERATURE, 200),
    'copper': (215, 850, 1085, 2562, 2562, NO_TEMPERATURE, 255),
    'magnet': (230, 650, 800, 2500, 2500, NO_TEMPERATURE, 180),
    'insulator': (180, 450, 900, NO_TEMPERATURE, 2200, 650, 2),
    'lightning': (1, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 255),
    'magma_vent': (255, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 255),
    'uranium': (230, 1000, 1132, 4131, 4131, NO_TEMPERATURE, 115),
    'radiation': (1, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 0),
    'gold_ore': (225, 900, 1064, 2856, 2856, NO_TEMPERATURE, 120),
    'gold': (210, 850, 1064, 2856, 2856, NO_TEMPERATURE, 250),
    'oxygen': (7, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 4),
    'carbon_dioxide': (11, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 4),
    'iron_ore': (225, 1200, 1538, 2862, 2862, NO_TEMPERATURE, 90),
    'steel': (245, 1250, 1450, 2900, 2900, NO_TEMPERATURE, 185),
    'conveyor': (220, 700, 1350, 2800, 2800, NO_TEMPERATURE, 190),
    'smelter': (240, 1000, 1500, 3000, 3000, NO_TEMPERATURE, 175),
    'assembler': (225, 550, 1200, 2600, 2600, NO_TEMPERATURE, 170),
    'bot_fabricator': (230, 550, 1200, 2600, 2600, NO_TEMPERATURE, 170),
    'power_cell': (92, 120, 220, 350, 450, 180, 150),
    'plasma_ammo': (93, 110, 180, 330, 430, 160, 130),
    'ally_bot': (180, 350, 1200, 2600, 2600, NO_TEMPERATURE, 130),
    'enemy_bot': (180, 350, 1200, 2600, 2600, NO_TEMPERATURE, 130),
    'plasma_bolt': (2, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 255),
    'factory_core': (254, 950, 1450, 2900, 2900, NO_TEMPERATURE, 220),
    'silt': (60, NO_TEMPERATURE, NO_TEMPERATURE, 100, 600, NO_TEMPERATURE, 25),
    'fertilizer': (55, 85, NO_TEMPERATURE, NO_TEMPERATURE, 350, 190, 14),
    'food': (42, 70, NO_TEMPERATURE, NO_TEMPERATURE, 260, 180, 11),
    'waste': (58, 85, NO_TEMPERATURE, NO_TEMPERATURE, 400, 210, 12),
    'hydrogen': (1, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE, 560, 180),
}


def base_phase(name):
    if name == 'empty':
        return PHASE_EMPTY
    if name in _BASE_LIQUIDS:
        return PHASE_LIQUID
    if name in _BASE_GASES:
        return PHASE_GAS
    if name in _BASE_PLASMA:
        return PHASE_PLASMA
    if name in _BASE_POWDERS:
        return PHASE_POWDER
    return PHASE_SOLID


def physics_for(name, fallback_density):
    density, softening, melting, boiling, vaporization, ignition, conductivity = PHYSICS_OVERRIDES.get(
        name,
        (fallback_density, NO_TEMPERATURE, NO_TEMPERATURE, NO_TEMPERATURE,
         NO_TEMPERATURE, NO_TEMPERATURE, 32),
    )
    return {
        'density': density,
        'base_phase': base_phase(name),
        'softening': softening,
        'melting': melting,
        'boiling': boiling,
        'vaporization': vaporization,
        'ignition': ignition,
        'conductivity': conductivity,
    }


def material_group_labels():
    labels = ['Unknown'] * len(MATERIALS)
    for _, label, ids in GROUPS:
        for material_id in ids:
            labels[material_id] = label
    return labels
