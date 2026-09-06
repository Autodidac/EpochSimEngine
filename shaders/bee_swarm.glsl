#ifndef SANDHYBRID_BEE_SWARM_GLSL
#define SANDHYBRID_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 60u;
const uint BEE_COLONY_MAX = 60u;
const uint BEE_TIMER_BITS = 14u;
const uint BEE_TIMER_MAX = 0x3fffu;
const uint BEE_TARGET_NEWBORN = 0x3fffeu;
const uint BEE_TARGET_SEARCH = 0x3fffdu;
const uint BEE_TARGET_NONE = 0x3ffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;
// Old district homes keep their exact bytes. This formerly clear persistent
// metadata bit admits independently placed colonies throughout the resident sky
// and gaps: global 16-cell home X10/Y7 plus six-bit slot.
const uint BEE_AUX_GLOBAL_HOME = 0x00800000u;
const uint BEE_AUTHORED_HOME_SLOT_BIT = 0x80u;
const uint BEE_POPULATION_INVALID = 0xffffffffu;

const ivec2 BEE_AUTHORED_WORLD_CELLS = ivec2(640, 360);
const ivec2 BEE_PERSISTENT_WORLD_CELLS = ivec2(5120, 360);

const uint BEE_INITIAL_PACKED[BEE_FORMATION_COUNT] = uint[](
    5437u, 5564u, 5572u, 5691u, 5701u, 5818u, 5830u, 5946u, 5958u, 6075u,
    6085u, 6204u, 6205u, 6211u, 6212u, 6334u, 6335u, 6336u, 6337u, 6338u,
    8495u, 8496u, 8497u, 8527u, 8528u, 8529u, 8626u, 8654u, 8755u, 8781u,
    8884u, 8908u, 9013u, 9035u, 9142u, 9162u, 9270u, 9290u, 9398u, 9418u,
    9525u, 9547u, 9644u, 9652u, 9676u, 9684u, 9773u, 9779u, 9805u, 9811u,
    9902u, 9906u, 9934u, 9938u, 10031u, 10032u, 10033u, 10063u, 10064u,
    10065u
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

bool beeUsesPersistentWorldHome(uint width, uint height) {
    return int(width) >= BEE_PERSISTENT_WORLD_CELLS.x &&
           int(height) >= BEE_PERSISTENT_WORLD_CELLS.y;
}

uint beeRawSlotFromAux(uint aux, uint width, uint height) {
    if (beeUsesPersistentWorldHome(width, height) &&
        (aux & BEE_AUX_GLOBAL_HOME) != 0u) return (aux >> 17u) & 63u;
    return beeUsesPersistentWorldHome(width, height)
        ? ((aux >> 13u) & 127u)
        : ((aux >> 15u) & 255u);
}
uint beeFormationSlotFromAux(uint aux, uint width, uint height) {
    return beeRawSlotFromAux(aux, width, height) & 127u;
}
bool beeUsesAuthoredHome(uint aux, uint width, uint height) {
    return !beeUsesPersistentWorldHome(width, height) &&
           (beeRawSlotFromAux(aux, width, height) & BEE_AUTHORED_HOME_SLOT_BIT) != 0u;
}

ivec2 beeAuthoredWorldOrigin(uint width, uint height) {
    int skyHeight = int(height) >= BEE_AUTHORED_WORLD_CELLS.y * 3
        ? BEE_AUTHORED_WORLD_CELLS.y * 2
        : 0;
    int originX = int(width) >= BEE_AUTHORED_WORLD_CELLS.x * 3
        ? BEE_AUTHORED_WORLD_CELLS.x * 2
        : max((int(width) - BEE_AUTHORED_WORLD_CELLS.x) / 2, 0);
    return ivec2(originX, skyHeight);
}

// Every persistent decoded home lies on the eight-cell lattice (tagged global
// homes use its sixteen-cell subset). Legacy homes lie on either the four-cell
// lattice or that lattice translated by the authored origin. Their common
// stride is gcd(4, origin.x, origin.y), including odd custom-width origins.
uint beePopulationStride(uint width, uint height) {
    if (beeUsesPersistentWorldHome(width, height)) return 8u;
    ivec2 origin = beeAuthoredWorldOrigin(width, height);
    uint originBits = uint(origin.x) | uint(origin.y);
    if ((originBits & 1u) != 0u) return 1u;
    return (originBits & 2u) != 0u ? 2u : 4u;
}

uint beePopulationColumns(uint width, uint height) {
    uint stride = beePopulationStride(width, height);
    return (width + stride - 1u) / stride;
}

uint beePopulationHomeIndex(ivec2 decodedHome, uint width, uint height) {
    if (decodedHome.x < 0 || decodedHome.y < 0 ||
        decodedHome.x >= int(width) || decodedHome.y >= int(height))
        return BEE_POPULATION_INVALID;
    uint stride = beePopulationStride(width, height);
    if (uint(decodedHome.x) % stride != 0u || uint(decodedHome.y) % stride != 0u)
        return BEE_POPULATION_INVALID;
    return (uint(decodedHome.y) / stride) * beePopulationColumns(width, height) +
           uint(decodedHome.x) / stride;
}

int beePersistentGap(uint width) {
    int spare = max(int(width) - BEE_PERSISTENT_WORLD_CELLS.x, 0);
    return (spare / 7 / 8) * 8;
}

int beePersistentSurfaceRow(uint district) {
    if (district == 1u) return 37;
    if (district == 2u) return 42;
    if (district == 3u) return 17;
    if (district == 4u) return 22;
    if (district == 5u || district == 6u) return 42;
    if (district == 7u) return 41;
    return 40;
}

int beePersistentSurfaceY(uint height) {
    int authoredTop = int(height) >= BEE_AUTHORED_WORLD_CELLS.y * 3
        ? BEE_AUTHORED_WORLD_CELLS.y * 2
        : 0;
    return authoredTop + 40 * 8;
}

ivec2 beePersistentDistrictOrigin(uint width, uint height, uint district) {
    uint bounded = min(district, 7u);
    int gap = beePersistentGap(width);
    return ivec2(int(bounded) * (BEE_AUTHORED_WORLD_CELLS.x + gap),
                 beePersistentSurfaceY(height) -
                     beePersistentSurfaceRow(bounded) * 8);
}

ivec2 beePersistentWorldOrigin(uint width, uint height) {
    return beePersistentDistrictOrigin(width, height, 0u);
}

bool beePersistentAddress(ivec2 homeCenter, uint width, uint height,
                          out uint district, out ivec2 districtLocal) {
    for (uint candidate = 0u; candidate < 8u; ++candidate) {
        ivec2 origin = beePersistentDistrictOrigin(width, height, candidate);
        ivec2 local = homeCenter - origin;
        if (local.x >= 0 && local.y >= 0 &&
            local.x < BEE_AUTHORED_WORLD_CELLS.x &&
            local.y < BEE_AUTHORED_WORLD_CELLS.y) {
            district = candidate;
            districtLocal = local;
            return true;
        }
    }
    district = 0u;
    districtLocal = homeCenter;
    return false;
}

ivec2 beeHomeCenterFromAux(uint aux, uint width, uint height) {
    if (beeUsesPersistentWorldHome(width, height)) {
        if ((aux & BEE_AUX_GLOBAL_HOME) != 0u)
            return ivec2(int(aux & 1023u) * 16, int((aux >> 10u) & 127u) * 16);
        uint district = (aux >> 20u) & 7u;
        ivec2 origin = beePersistentDistrictOrigin(width, height, district);
        ivec2 local = ivec2(int(aux & 127u) * 8,
                            int((aux >> 7u) & 63u) * 8);
        return origin + local;
    }
    ivec2 home = ivec2(int(aux & 255u) * 4, int((aux >> 8u) & 127u) * 4);
    return beeUsesAuthoredHome(aux, width, height)
        ? home + beeAuthoredWorldOrigin(width, height)
        : home;
}

uint beePackMetadata(uint aux, ivec2 homeCenter, uint slot, uint width, uint height) {
    uint district = 0u;
    ivec2 districtLocal = ivec2(0);
    bool persistent = int(width) >= BEE_PERSISTENT_WORLD_CELLS.x &&
                      int(height) >= BEE_PERSISTENT_WORLD_CELLS.y &&
                      beePersistentAddress(homeCenter, width, height,
                                           district, districtLocal);
    if (persistent) {
        uint homeX = uint(clamp(districtLocal.x / 8, 0, 127));
        uint homeY = uint(clamp(districtLocal.y / 8, 0, 63));
        uint metadata = homeX | (homeY << 7u) | ((slot & 127u) << 13u) |
                        (district << 20u);
        return (aux & ~BEE_METADATA_MASK) | metadata;
    }

    if (beeUsesPersistentWorldHome(width, height)) {
        uint metadata = BEE_AUX_GLOBAL_HOME |
            uint(clamp(homeCenter.x / 16, 0, 1023)) |
            (uint(clamp(homeCenter.y / 16, 0, 127)) << 10u) |
            ((slot & 63u) << 17u);
        return (aux & ~BEE_METADATA_MASK) | metadata;
    }
    ivec2 authoredOrigin = beeAuthoredWorldOrigin(width, height);
    bool authored = all(greaterThanEqual(homeCenter, authoredOrigin)) &&
                    all(lessThan(homeCenter, authoredOrigin + BEE_AUTHORED_WORLD_CELLS));
    ivec2 storedHome = authored ? homeCenter - authoredOrigin : homeCenter;
    uint homeX = uint(clamp(storedHome.x / 4, 0, 255));
    uint homeY = uint(clamp(storedHome.y / 4, 0, 127));
    uint packedSlot = (slot & 127u) | (authored ? BEE_AUTHORED_HOME_SLOT_BIT : 0u);
    uint metadata = homeX | (homeY << 8u) | (packedSlot << 15u);
    return (aux & ~BEE_METADATA_MASK) | metadata;
}
uint beeTimerFromAge(uint age) { return age & BEE_TIMER_MAX; }
uint beeTargetTileFromAge(uint age) { return age >> BEE_TIMER_BITS; }
uint beePackAge(uint timer, uint targetTile) {
    return min(timer, BEE_TIMER_MAX) |
           (min(targetTile, BEE_TARGET_NONE) << BEE_TIMER_BITS);
}

bool beeForagerSlot(uint slot) { return ((slot * 37u + 11u) % 10u) == 0u; }
uint beeForagerOrdinal(uint slot) {
    return min(slot, BEE_FORMATION_COUNT - 1u) / 10u;
}
uint beeDepartureThreshold(uint slot) { return 1200u + (slot * 29u) % 600u; }
uint beeInitialTimer(uint slot) {
    if (!beeForagerSlot(slot)) return (slot * 17u) % 900u;
    uint activationTick = 1u + beeForagerOrdinal(slot) * 12u;
    return beeDepartureThreshold(slot) - activationTick;
}

// A replacement bee must retain the Queen's exact intra-tile position while
// it owns the reserved newborn route. Persistent metadata intentionally stores
// only the home tile, so the otherwise-unused newborn timer carries the exact
// 3-bit x/y remainder (legacy district) or tagged 4-bit remainder (global16)
// until the bee reaches its assigned formation cell.
uint beeNewbornHomeTimer(ivec2 homeCenter, uint metadata) {
    if ((metadata & BEE_AUX_GLOBAL_HOME) != 0u)
        return 256u | uint(homeCenter.x & 15) | (uint(homeCenter.y & 15) << 4u);
    return uint(homeCenter.x & 7) | (uint(homeCenter.y & 7) << 3u);
}

ivec2 beeNewbornExactHome(uint age, ivec2 alignedHome) {
    uint packed = beeTimerFromAge(age);
    if ((packed & 256u) != 0u)
        return alignedHome + ivec2(int(packed & 15u), int((packed >> 4u) & 15u));
    return alignedHome + ivec2(int(packed & 7u), int((packed >> 3u) & 7u));
}

int beeHomeSearchExtent(uint aux, uint width, uint height) {
    return beeUsesPersistentWorldHome(width, height) &&
        (aux & BEE_AUX_GLOBAL_HOME) != 0u ? 15 : 7;
}

bool beeOwnsHome(uint aux, ivec2 queen, uint width, uint height) {
    uint expected = beePackMetadata(0u, queen, 0u, width, height);
    return all(equal(beeHomeCenterFromAux(aux, width, height),
                     beeHomeCenterFromAux(expected, width, height)));
}

bool beeIsForager(uint aux, uint width, uint height) {
    uint slot = beeFormationSlotFromAux(aux, width, height);
    return beeForagerSlot(slot);
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

ivec2 beeBiohazardTargetOffset(uint slot, uint step) {
    // Stable one-to-one slot ownership keeps the photographed compact composite
    // readable. Real foragers still leave through their explicit flower target
    // and return through pollen/honey lifecycle targets.
    // Resting owners do not orbit their assigned pixel: a universal one-cell
    // flutter made all 60 canonical bees look like an amorphous swarm.
    return beeFormationOffset(slot);
}

ivec2 beeSwarmTarget(uint aux, uint step, uint width, uint height) {
    uint slot = beeFormationSlotFromAux(aux, width, height);
    ivec2 home = beeHomeCenterFromAux(aux, width, height);
    return home + beeBiohazardTargetOffset(slot, step);
}

ivec2 beeOrbitTarget(uint aux, uint step, uint width, uint height) {
    return beeSwarmTarget(aux, step, width, height);
}

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

ivec2 beeForagerApproachPosition(ivec2 occupiedPosition, uint slot) {
    const ivec2 offsets[6] = ivec2[6](
        ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1),
        ivec2(1, 0), ivec2(1, 1), ivec2(-1, 1));
    return occupiedPosition + offsets[beeForagerOrdinal(slot)];
}

ivec2 beeMigrationSite(ivec2 flowerPosition, uint width, uint height) {
    ivec2 site = flowerPosition + ivec2(0, -16);
    site = ivec2((site.x / 4) * 4, (site.y / 4) * 4);
    return clamp(site, ivec2(16), ivec2(int(width) - 17, int(height) - 17));
}

#endif
