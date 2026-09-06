#ifndef EPOCHSIMENGINE_QUEEN_VENTILATION_GLSL
#define EPOCHSIMENGINE_QUEEN_VENTILATION_GLSL

// Call only after the source-only vent footprint excludes all competing
// owners except at most eight neighboring Bees. The frozen gas invocation
// still owns its rare Bee respiration event, independently of Bee movement.
// Reserve that breath as well as the Queen's; never fund both with one unit.
uint queenVentRequiredBreaths(uint beeCount, uint donorRandomValue) {
    uint beeRoll = hash32(donorRandomValue ^ 0xb33a71u) & 0x0003ffffu;
    return 1u + (beeRoll < beeCount ? 1u : 0u);
}

#endif
