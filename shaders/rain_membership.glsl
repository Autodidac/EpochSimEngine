#ifndef SANDHYBRID_RAIN_MEMBERSHIP_GLSL
#define SANDHYBRID_RAIN_MEMBERSHIP_GLSL

// Derived ownership, never canonical material. The fixed plane layout depends
// only on storage width, including when a producer uses a clipped push height:
// count[x], summary[0..1][x], then rowWord[0..ceil(height/32)-1][x].
// One final width-sized plane snapshots pre-correction counts for emission
// admission. Its location is storage-derived, not clipped push-height-derived.
// Two summary words cover at most 2048 rows. Producers complete behind a buffer
// barrier before the exclusive one-invocation-per-column rainfall consumer.
layout(std430, binding = 10) buffer RainColumns { uint rainColumns[]; };

bool rainMembershipAddress(ivec2 position, uint width,
                           out uint wordIndex, out uint bitMask) {
    wordIndex = 0u;
    bitMask = 0u;
    if (width == 0u || position.x < 0 || uint(position.x) >= width ||
        position.y < 0 || position.y >= 2048) return false;
    uint rowWord = uint(position.y) >> 5u;
    uint plane = 3u + rowWord;
    uint storageWords = uint(rainColumns.length());
    if (width > storageWords) return false;
    // The last plane is never row membership, even for a clipped producer.
    if (width > (storageWords - width) / (plane + 1u)) return false;
    wordIndex = plane * width + uint(position.x);
    bitMask = 1u << (uint(position.y) & 31u);
    return true;
}

void rainPublishSummary(ivec2 position, uint width) {
    uint rowWord = uint(position.y) >> 5u;
    uint summaryIndex = (1u + (rowWord >> 5u)) * width + uint(position.x);
    atomicOr(rainColumns[summaryIndex], 1u << (rowWord & 31u));
}

bool rainRegister(ivec2 position, uint width) {
    uint wordIndex, bitMask;
    if (!rainMembershipAddress(position, width, wordIndex, bitMask)) return false;
    uint previous = atomicOr(rainColumns[wordIndex], bitMask);
    bool inserted = (previous & bitMask) == 0u;
    if (inserted) atomicAdd(rainColumns[uint(position.x)], 1u);
    // Never clear summary bits in a producer: another row in this word may
    // have been concurrently registered. Stale positive summaries are safe.
    rainPublishSummary(position, width);
    return inserted;
}

bool rainRemove(ivec2 position, uint width) {
    uint wordIndex, bitMask;
    if (!rainMembershipAddress(position, width, wordIndex, bitMask)) return false;
    uint previous = atomicAnd(rainColumns[wordIndex], ~bitMask);
    bool removed = (previous & bitMask) != 0u;
    if (removed) atomicAdd(rainColumns[uint(position.x)], 0xffffffffu);
    return removed;
}

void rainReconcile(ivec2 position, Cell finalCell, uint width) {
    if (isTrackedRain(finalCell)) rainRegister(position, width);
    else rainRemove(position, width);
}

// Weather admission still allows one newly emitted owner only in an empty
// column. Reservation and membership are separate: claiming count 0 -> 1 must
// not be counted a second time when the final corrected cell is published.
bool rainTryReserveEmission(uint x, uint width) {
    if (width == 0u || x >= width ||
        width > uint(rainColumns.length()) / 5u) return false;
    // A tagged owner can phase-change and decrement the live count elsewhere
    // in this dispatch. Immutable pre-pass admission makes that ordering
    // irrelevant: an occupied column cannot emit until a later correction.
    if (rainColumns[uint(rainColumns.length()) - width + x] != 0u) return false;
    return atomicCompSwap(rainColumns[x], 0u, 1u) == 0u;
}

void rainCancelEmission(uint x, uint width) {
    if (width != 0u && x < width &&
        width <= uint(rainColumns.length()) / 5u)
        atomicAdd(rainColumns[x], 0xffffffffu);
}

void rainRegisterReserved(ivec2 position, uint width) {
    uint wordIndex, bitMask;
    if (!rainMembershipAddress(position, width, wordIndex, bitMask)) {
        if (position.x >= 0) rainCancelEmission(uint(position.x), width);
        return;
    }
    uint previous = atomicOr(rainColumns[wordIndex], bitMask);
    if ((previous & bitMask) != 0u)
        rainCancelEmission(uint(position.x), width);
    rainPublishSummary(position, width);
}

#endif
