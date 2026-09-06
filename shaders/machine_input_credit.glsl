#ifndef EPOCHSIMENGINE_MACHINE_INPUT_CREDIT_GLSL
#define EPOCHSIMENGINE_MACHINE_INPUT_CREDIT_GLSL

// Shared verbatim with the CPU accounting contract. Capacity is derived from
// immutable source inventory, never from inventory after same-tick output.
uint machineInputCreditIncrement(uint current, uint capacity) {
    return current < capacity ? current + 1u : current;
}

#endif
