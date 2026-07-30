#ifndef EPOCH_SAND_CONSERVATION_GLSL
#define EPOCH_SAND_CONSERVATION_GLSL

const uint CONS_CREATED = 0u;
const uint CONS_DESTROYED = 1u;
const uint CONS_CONVERTED = 2u;
const uint CONS_BOUNDARY_LOST = 3u;
const uint CONS_PHASE_CHANGES = 4u;
const uint CONS_STABILIZED = 5u;
const uint CONS_BROKEN = 6u;
const uint CONS_ERRORS = 7u;

layout(std430, binding = 5) buffer ConservationBuffer { uint conservation[8]; };

bool isEphemeralEnergy(uint material) {
    return material == MAT_FIRE || material == MAT_LIGHTNING || material == MAT_RADIATION ||
           material == MAT_PLANT_STEM;
}

void recordConservation(Cell before, Cell after) {
    if (before.material >= MATERIAL_COUNT || after.material >= MATERIAL_COUNT) {
        atomicAdd(conservation[CONS_ERRORS], 1u);
        return;
    }
    if (before.material == MAT_EMPTY && after.material != MAT_EMPTY) {
        atomicAdd(conservation[CONS_CREATED], 1u);
    } else if (before.material != MAT_EMPTY && after.material == MAT_EMPTY) {
        atomicAdd(conservation[isEphemeralEnergy(before.material) ? CONS_BOUNDARY_LOST : CONS_DESTROYED], 1u);
    } else if (before.material != after.material) {
        atomicAdd(conservation[CONS_CONVERTED], 1u);
    }
    if (before.material == after.material && cellPhase(before) != cellPhase(after))
        atomicAdd(conservation[CONS_PHASE_CHANGES], 1u);
    if (!isStructural(before) && isStructural(after)) atomicAdd(conservation[CONS_STABILIZED], 1u);
    if (isStructural(before) && !isStructural(after)) atomicAdd(conservation[CONS_BROKEN], 1u);
}

#endif
