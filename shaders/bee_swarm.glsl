#ifndef EPOCH_SAND_BEE_SWARM_GLSL
#define EPOCH_SAND_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 100u;
const uint BEE_COLONY_MAX = 100u;
const uint BEE_TARGET_NONE = 0xffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;

const uint BEE_SWARM_BIOHAZARD_TICKS = 1800u;
const uint BEE_SWARM_ALTERNATE_TICKS = 600u;
const uint BEE_SWARM_PHASE_TICKS =
    BEE_SWARM_BIOHAZARD_TICKS + BEE_SWARM_ALTERNATE_TICKS;
const uint BEE_SWARM_CYCLE_TICKS = BEE_SWARM_PHASE_TICKS * 2u;

const uint BEE_INITIAL_PACKED[BEE_FORMATION_COUNT] = uint[](
    1479u, 1850u, 1999u, 2109u, 2229u, 2235u, 2366u, 2479u, 2503u, 2510u,
    2622u, 2773u, 2866u, 2871u, 2989u, 2999u, 3243u, 3285u, 3535u, 3885u,
    3924u, 4010u, 4178u, 4432u, 4439u, 4560u, 4650u, 4817u, 5946u, 6208u,
    6219u, 6340u, 6597u, 6989u, 7458u, 7527u, 7829u, 7844u, 7856u, 8042u,
    8098u, 8172u, 8338u, 8353u, 8416u, 8429u, 8624u, 8736u, 8812u, 8909u,
    8914u, 8938u, 8980u, 9110u, 9136u, 9170u, 9199u, 9369u, 9446u, 9578u,
    9617u, 9623u, 9673u, 9804u, 9959u, 10042u, 10220u, 10648u, 10736u, 10853u,
    10988u, 11118u, 11363u, 11411u, 11475u, 11549u, 11673u, 11814u, 11822u, 11930u,
    11984u, 11998u, 12117u, 12134u, 12190u, 12211u, 12310u, 12329u, 12388u, 12456u,
    12578u, 12596u, 12622u, 12627u, 12702u, 12750u, 12768u, 12961u, 13020u, 13097u
);

uint beeHash32(uint value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

ivec2 beeFormationOffset(uint slot) {
    uint packedValue = BEE_INITIAL_PACKED[min(slot, BEE_FORMATION_COUNT - 1u)];
    return ivec2(int(packedValue & 127u) - 64, int(packedValue >> 7u) - 64);
}

int beeFormationSlotFromOffset(ivec2 offset) {
    if (offset.x < -64 || offset.x > 63 || offset.y < -64 || offset.y > 63) return -1;
    uint key = (uint(offset.y + 64) << 7u) | uint(offset.x + 64);
    int low = 0;
    int high = int(BEE_FORMATION_COUNT) - 1;
    for (int iteration = 0; iteration < 8 && low <= high; ++iteration) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_INITIAL_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    return -1;
}

uint beeFormationSlotFromAux(uint aux) { return (aux >> 15u) & 255u; }

ivec2 beeHomeCenterFromAux(uint aux) {
    return ivec2(int(aux & 255u) * 4, int((aux >> 8u) & 127u) * 4);
}

uint beePackMetadata(uint aux, ivec2 homeCenter, uint slot) {
    uint homeX = uint(clamp(homeCenter.x / 4, 0, 255));
    uint homeY = uint(clamp(homeCenter.y / 4, 0, 127));
    uint metadata = homeX | (homeY << 8u) | ((slot & 255u) << 15u);
    return (aux & ~BEE_METADATA_MASK) | metadata;
}

uint beeTimerFromAge(uint age) { return age & 0xffffu; }
uint beeTargetTileFromAge(uint age) { return age >> 16u; }
uint beePackAge(uint timer, uint targetTile) {
    return min(timer, 0xffffu) | (min(targetTile, BEE_TARGET_NONE) << 16u);
}

bool beeIsForager(uint aux) {
    uint slot = beeFormationSlotFromAux(aux);
    return ((slot * 37u + 11u) % 10u) == 0u;
}

ivec2 beeRotateOffset(ivec2 offset, uint phase) {
    switch (phase & 15u) {
    case 0u: return offset;
    case 1u: return ivec2((offset.x * 237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * 237) / 256);
    case 2u: return ivec2((offset.x * 181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * 181) / 256);
    case 3u: return ivec2((offset.x * 98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * 98) / 256);
    case 4u: return ivec2(-offset.y, offset.x);
    case 5u: return ivec2((offset.x * -98 - offset.y * 237) / 256, (offset.x * 237 - offset.y * 98) / 256);
    case 6u: return ivec2((offset.x * -181 - offset.y * 181) / 256, (offset.x * 181 - offset.y * 181) / 256);
    case 7u: return ivec2((offset.x * -237 - offset.y * 98) / 256, (offset.x * 98 - offset.y * 237) / 256);
    case 8u: return -offset;
    case 9u: return ivec2((offset.x * -237 + offset.y * 98) / 256, (offset.x * -98 - offset.y * 237) / 256);
    case 10u: return ivec2((offset.x * -181 + offset.y * 181) / 256, (offset.x * -181 - offset.y * 181) / 256);
    case 11u: return ivec2((offset.x * -98 + offset.y * 237) / 256, (offset.x * -237 - offset.y * 98) / 256);
    case 12u: return ivec2(offset.y, -offset.x);
    case 13u: return ivec2((offset.x * 98 + offset.y * 237) / 256, (offset.x * -237 + offset.y * 98) / 256);
    case 14u: return ivec2((offset.x * 181 + offset.y * 181) / 256, (offset.x * -181 + offset.y * 181) / 256);
    case 15u: return ivec2((offset.x * 237 + offset.y * 98) / 256, (offset.x * -98 + offset.y * 237) / 256);
    }
    return offset;
}

uint beeSwarmState(uint aux, uint step) {
    uint local = step % BEE_SWARM_CYCLE_TICKS;
    uint phase = local / BEE_SWARM_PHASE_TICKS;
    uint phaseLocal = local % BEE_SWARM_PHASE_TICKS;
    if (phaseLocal < BEE_SWARM_BIOHAZARD_TICKS) return 0u;
    ivec2 home = beeHomeCenterFromAux(aux);
    uint cycle = step / BEE_SWARM_CYCLE_TICKS;
    bool reverse = (beeHash32(uint(home.x) * 73856093u ^ uint(home.y) * 19349663u ^ cycle) & 1u) != 0u;
    return reverse ? 2u - phase : 1u + phase;
}

ivec2 beeBiohazardTargetOffset(uint slot, uint step, ivec2 home) {
    const uint increments[8] = uint[8](1u, 3u, 7u, 9u, 11u, 13u, 17u, 19u);
    uint epoch = step / 360u;
    uint increment = increments[beeHash32(uint(home.x) ^ (uint(home.y) << 16u) ^ epoch) & 7u];
    uint targetSlot = (slot + epoch * increment) % BEE_FORMATION_COUNT;
    // A stable, slightly enlarged mask reads as a symbol instead of 100 unrelated insects.
    ivec2 anchor = beeFormationOffset(targetSlot) * 5 / 4;
    ivec2 flutter = beeRotateOffset(ivec2(1, 0), step / 8u + slot * 5u);
    return anchor + flutter;
}

ivec2 beeHaloTargetOffset(uint slot, uint step) {
    int radius = 34 + int((slot * 13u) % 18u);
    uint phase = step / 10u + slot * 7u;
    return beeRotateOffset(ivec2(radius, 0), phase) +
           beeRotateOffset(ivec2(2, 0), step / 3u + slot * 11u);
}

ivec2 beeCloudTargetOffset(uint slot, uint step) {
    uint lobe = slot % 3u;
    ivec2 center = lobe == 0u ? ivec2(0, -29) :
                   (lobe == 1u ? ivec2(-26, 15) : ivec2(26, 15));
    int radius = 5 + int((slot * 17u) % 16u);
    uint phase = step / 8u + slot * 9u;
    return center + beeRotateOffset(ivec2(radius, 0), phase) +
           beeRotateOffset(ivec2(1, 0), step / 2u + slot * 3u);
}

ivec2 beeSwarmTarget(uint aux, uint step) {
    uint slot = beeFormationSlotFromAux(aux);
    ivec2 home = beeHomeCenterFromAux(aux);
    uint state = beeSwarmState(aux, step);
    ivec2 offset = state == 0u ? beeBiohazardTargetOffset(slot, step, home) :
                   (state == 1u ? beeHaloTargetOffset(slot, step)
                                : beeCloudTargetOffset(slot, step));
    return home + offset;
}

ivec2 beeOrbitTarget(uint aux, uint step) { return beeSwarmTarget(aux, step); }

ivec2 beeLandingOffset(uint slot) {
    switch (slot & 15u) {
    case 0u: return ivec2(13, 0); case 1u: return ivec2(12, 5);
    case 2u: return ivec2(9, 9); case 3u: return ivec2(5, 12);
    case 4u: return ivec2(0, 13); case 5u: return ivec2(-5, 12);
    case 6u: return ivec2(-9, 9); case 7u: return ivec2(-12, 5);
    case 8u: return ivec2(-13, 0); case 9u: return ivec2(-12, -5);
    case 10u: return ivec2(-9, -9); case 11u: return ivec2(-5, -12);
    case 12u: return ivec2(0, -13); case 13u: return ivec2(5, -12);
    case 14u: return ivec2(9, -9); case 15u: return ivec2(12, -5);
    }
    return ivec2(13, 0);
}

int beeAxisSign(int value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }

ivec2 beeApproachPosition(ivec2 occupiedPosition, ivec2 fromPosition) {
    ivec2 delta = fromPosition - occupiedPosition;
    ivec2 direction = ivec2(beeAxisSign(delta.x), beeAxisSign(delta.y));
    if (all(equal(direction, ivec2(0)))) direction = ivec2(1, 0);
    return occupiedPosition + direction;
}

ivec2 beeMigrationSite(ivec2 flowerPosition, uint width, uint height) {
    ivec2 site = flowerPosition + ivec2(0, -16);
    site = ivec2((site.x / 4) * 4, (site.y / 4) * 4);
    return clamp(site, ivec2(16), ivec2(int(width) - 17, int(height) - 17));
}

#endif
