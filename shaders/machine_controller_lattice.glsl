#ifndef EPOCHSIMENGINE_MACHINE_CONTROLLER_LATTICE_GLSL
#define EPOCHSIMENGINE_MACHINE_CONTROLLER_LATTICE_GLSL

// Shared verbatim with the CPU contract. Controllers occupy global residue
// (3,3) modulo eight. Starting at this relative offset and stepping by eight
// retains the original ascending row-major order and strict-distance ties.
// Production radius is six: coordinate +/- radius must fit a signed int.
// Relative increments stay <= 14 even beside INT_MAX; absolute increments
// after the last admitted candidate could overflow. Keep the bitmask for
// negative coordinates rather than substituting signed remainder.
int machineControllerFirstOffset(int coordinate, int radius) {
    return -radius + ((3 - ((coordinate - radius) & 7)) & 7);
}

#endif
