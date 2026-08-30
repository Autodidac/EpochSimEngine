#ifndef SANDHYBRID_BEE_SWARM_GLSL
#define SANDHYBRID_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 100u;
const uint BEE_COLONY_MAX = 100u;
const uint BEE_TARGET_NONE = 0xffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;
const uint BEE_AUTHORED_HOME_SLOT_BIT = 0x80u;

const ivec2 BEE_AUTHORED_WORLD_CELLS = ivec2(640, 360);
const ivec2 BEE_PERSISTENT_WORLD_CELLS = ivec2(5120, 360);

const uint BEE_INITIAL_PACKED[BEE_FORMATION_COUNT] = uint[](
    4541u, 4542u, 4543u, 4545u, 4546u, 4547u, 4668u, 4669u, 4675u, 4676u,
    4795u, 4805u, 4922u, 4923u, 4933u, 4934u, 5049u, 5050u, 5062u, 5063u,
    5177u, 5191u, 5433u, 5447u, 5561u, 5575u, 5689u, 5703u, 8240u, 8241u,
    8243u, 8269u, 8271u, 8272u, 8366u, 8367u, 8401u, 8402u, 8493u, 8494u,
    8530u, 8531u, 8620u, 8627u, 8653u, 8660u, 8747u, 8748u, 8788u, 8789u,
    9003u, 9012u, 9036u, 9045u, 9131u, 9173u, 9259u, 9269u, 9291u, 9301u,
    9515u, 9516u, 9527u, 9545u, 9556u, 9557u, 9644u, 9645u, 9655u, 9657u,
    9671u, 9673u, 9683u, 9684u, 9774u, 9783u, 9788u, 9796u, 9801u, 9810u,
    9902u, 9903u, 9909u, 9910u, 9918u, 9922u, 9930u, 9931u, 9937u, 9938u,
    10032u, 10033u, 10034u, 10036u, 10037u, 10059u, 10060u, 10062u, 10063u, 10064u
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
uint beeTimerFromAge(uint age) { return age & 0xffffu; }
uint beeTargetTileFromAge(uint age) { return age >> 16u; }
uint beePackAge(uint timer, uint targetTile) {
    return min(timer, 0xffffu) | (min(targetTile, BEE_TARGET_NONE) << 16u);
}

bool beeIsForager(uint aux, uint width, uint height) {
    uint slot = beeFormationSlotFromAux(aux, width, height);
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

ivec2 beeBiohazardTargetOffset(uint slot, uint step) {
    // Stable one-to-one slot ownership keeps the photographed compact composite
    // readable. Real foragers still leave through their explicit flower target
    // and return through pollen/honey lifecycle targets.
    // Resting owners do not orbit their assigned pixel: a universal one-cell
    // flutter made all 100 canonical bees look like an amorphous swarm.
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

ivec2 beeMigrationSite(ivec2 flowerPosition, uint width, uint height) {
    ivec2 site = flowerPosition + ivec2(0, -16);
    site = ivec2((site.x / 4) * 4, (site.y / 4) * 4);
    return clamp(site, ivec2(16), ivec2(int(width) - 17, int(height) - 17));
}

#endif
