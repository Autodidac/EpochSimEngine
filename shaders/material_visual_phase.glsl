#ifndef EPOCHSIMENGINE_MATERIAL_VISUAL_PHASE_GLSL
#define EPOCHSIMENGINE_MATERIAL_VISUAL_PHASE_GLSL

uint materialPresentationPhase(bool halfWater, uint physicalPhase) {
    // Fractional Water retains its liquid appearance until a real merge owns
    // phase chemistry. Its low state byte carries displaced-medium heat, not
    // vapor density. This choice never changes the physical phase or payload.
    return halfWater ? PHASE_LIQUID : physicalPhase;
}

#endif
