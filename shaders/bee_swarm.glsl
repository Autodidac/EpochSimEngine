#ifndef EPOCH_SAND_BEE_SWARM_GLSL
#define EPOCH_SAND_BEE_SWARM_GLSL

const uint BEE_FORMATION_COUNT = 200u;
const uint BEE_TARGET_NONE = 0xffffu;
const uint BEE_AUX_QUEEN = 0x40000000u;
const uint BEE_AUX_POLLEN = 0x20000000u;
const uint BEE_AUX_FED = 0x10000000u;
const uint BEE_AUX_SWARM = 0x08000000u;
const uint BEE_AUX_MIGRATING = 0x02000000u;
const uint BEE_METADATA_MASK = 0x00ffffffu;

const uint BEE_BIOHAZARD_PACKED[BEE_FORMATION_COUNT] = uint[](
    2366u, 2370u, 2489u, 2503u, 2741u, 2763u, 2879u, 2883u, 2993u, 3002u,
    3023u, 3144u, 3254u, 3404u, 3501u, 3539u, 3634u, 3791u, 4011u, 4053u,
    4143u, 4306u, 4521u, 4567u, 4653u, 4926u, 4930u, 4948u, 5160u, 5178u,
    5190u, 5208u, 5292u, 5559u, 5577u, 5588u, 5800u, 5804u, 5848u, 6069u,
    6091u, 6099u, 6205u, 6211u, 6313u, 6359u, 6446u, 6572u, 6577u, 6580u,
    6583u, 6592u, 6601u, 6604u, 6607u, 6612u, 6696u, 6715u, 6725u, 6737u,
    6744u, 6819u, 6877u, 6955u, 6960u, 6997u, 7071u, 7085u, 7091u, 7093u,
    7095u, 7113u, 7115u, 7117u, 7121u, 7125u, 7137u, 7208u, 7217u, 7247u,
    7258u, 7332u, 7342u, 7378u, 7452u, 7518u, 7524u, 7603u, 7604u, 7628u,
    7629u, 7712u, 7727u, 7761u, 7856u, 7888u, 7961u, 8034u, 8039u, 8107u,
    8149u, 8220u, 8243u, 8269u, 8421u, 8471u, 8489u, 8535u, 8553u, 8624u,
    8656u, 8730u, 8884u, 8908u, 9062u, 9109u, 9128u, 9176u, 9195u, 9369u,
    9395u, 9399u, 9417u, 9421u, 9641u, 9687u, 9703u, 9749u, 9787u, 9797u,
    9835u, 9881u, 9911u, 9920u, 9929u, 10046u, 10050u, 10155u, 10197u, 10215u,
    10262u, 10301u, 10307u, 10346u, 10427u, 10432u, 10437u, 10522u, 10542u, 10578u,
    10674u, 10679u, 10684u, 10687u, 10692u, 10697u, 10702u, 10853u, 10903u, 10946u,
    10985u, 11036u, 11197u, 11199u, 11201u, 11362u, 11417u, 11461u, 11495u, 11551u,
    11578u, 11711u, 11713u, 11743u, 11849u, 11933u, 11939u, 11958u, 11995u, 12003u,
    12072u, 12091u, 12101u, 12109u, 12192u, 12204u, 12209u, 12242u, 12246u, 12256u,
    12453u, 12471u, 12489u, 12507u, 12595u, 12621u, 12713u, 12718u, 12754u, 12759u
);

ivec2 beeFormationOffset(uint slot) {
    uint packedValue = BEE_BIOHAZARD_PACKED[min(slot, BEE_FORMATION_COUNT - 1u)];
    return ivec2(int(packedValue & 127u) - 64, int(packedValue >> 7u) - 64);
}

int beeFormationSlotFromOffset(ivec2 offset) {
    if (offset.x < -64 || offset.x > 63 || offset.y < -64 || offset.y > 63) return -1;
    uint key = (uint(offset.y + 64) << 7u) | uint(offset.x + 64);
    int low = 0;
    int high = int(BEE_FORMATION_COUNT) - 1;
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
        if (key == middleKey) return middle;
        if (key < middleKey) high = middle - 1;
        else low = middle + 1;
    }
    if (low <= high) {
        int middle = (low + high) / 2;
        uint middleKey = BEE_BIOHAZARD_PACKED[middle];
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
    case 0u: return ivec2((offset.x * 256 - offset.y * 0) / 256, (offset.x * 0 + offset.y * 256) / 256);
    case 1u: return ivec2((offset.x * 237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * 237) / 256);
    case 2u: return ivec2((offset.x * 181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * 181) / 256);
    case 3u: return ivec2((offset.x * 98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * 98) / 256);
    case 4u: return ivec2((offset.x * 0 - offset.y * 256) / 256, (offset.x * 256 + offset.y * 0) / 256);
    case 5u: return ivec2((offset.x * -98 - offset.y * 237) / 256, (offset.x * 237 + offset.y * -98) / 256);
    case 6u: return ivec2((offset.x * -181 - offset.y * 181) / 256, (offset.x * 181 + offset.y * -181) / 256);
    case 7u: return ivec2((offset.x * -237 - offset.y * 98) / 256, (offset.x * 98 + offset.y * -237) / 256);
    case 8u: return ivec2((offset.x * -256 - offset.y * 0) / 256, (offset.x * 0 + offset.y * -256) / 256);
    case 9u: return ivec2((offset.x * -237 - offset.y * -98) / 256, (offset.x * -98 + offset.y * -237) / 256);
    case 10u: return ivec2((offset.x * -181 - offset.y * -181) / 256, (offset.x * -181 + offset.y * -181) / 256);
    case 11u: return ivec2((offset.x * -98 - offset.y * -237) / 256, (offset.x * -237 + offset.y * -98) / 256);
    case 12u: return ivec2((offset.x * 0 - offset.y * -256) / 256, (offset.x * -256 + offset.y * 0) / 256);
    case 13u: return ivec2((offset.x * 98 - offset.y * -237) / 256, (offset.x * -237 + offset.y * 98) / 256);
    case 14u: return ivec2((offset.x * 181 - offset.y * -181) / 256, (offset.x * -181 + offset.y * 181) / 256);
    case 15u: return ivec2((offset.x * 237 - offset.y * -98) / 256, (offset.x * -98 + offset.y * 237) / 256);
    }
    return offset;
}

ivec2 beeFlutterOffset(uint slot, uint step) {
    // Individual bees circle their own fixed silhouette anchor.
    uint phase = step + slot * 5u;
    int radius = 1 + int((slot * 13u) % 3u);
    return beeRotateOffset(ivec2(radius, 0), phase);
}

ivec2 beeSwarmWave(uint slot, uint step) {
    // The three lobes breathe out of phase while the symbol itself stays fixed.
    ivec2 base = beeFormationOffset(slot);
    uint lobePhase = base.y < -10 ? 0u : (base.x < 0 ? 5u : 10u);
    return beeRotateOffset(ivec2(1, 0), step / 6u + lobePhase);
}

ivec2 beeOrbitTarget(uint aux, uint step) {
    uint slot = beeFormationSlotFromAux(aux);
    return beeHomeCenterFromAux(aux) + beeFormationOffset(slot) +
           beeFlutterOffset(slot, step) + beeSwarmWave(slot, step);
}

ivec2 beeLandingOffset(uint slot) {
    switch (slot & 15u) {
    case 0u: return ivec2(13, 0);
    case 1u: return ivec2(12, 5);
    case 2u: return ivec2(9, 9);
    case 3u: return ivec2(5, 12);
    case 4u: return ivec2(0, 13);
    case 5u: return ivec2(-5, 12);
    case 6u: return ivec2(-9, 9);
    case 7u: return ivec2(-12, 5);
    case 8u: return ivec2(-13, 0);
    case 9u: return ivec2(-12, -5);
    case 10u: return ivec2(-9, -9);
    case 11u: return ivec2(-5, -12);
    case 12u: return ivec2(0, -13);
    case 13u: return ivec2(5, -12);
    case 14u: return ivec2(9, -9);
    case 15u: return ivec2(12, -5);
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
