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
    3002u, 3005u, 3007u, 3009u, 3012u, 3014u, 3129u, 3144u, 3255u, 3273u,
    3382u, 3403u, 3508u, 3532u, 3634u, 3662u, 3761u, 3919u, 4016u, 4048u,
    4143u, 4305u, 4398u, 4434u, 4653u, 4691u, 4909u, 4947u, 5293u, 5331u,
    5421u, 5460u, 5567u, 5572u, 5715u, 5805u, 5951u, 5956u, 5971u, 6061u,
    6226u, 6318u, 6334u, 6339u, 6482u, 6575u, 6704u, 6718u, 6723u, 6737u,
    6864u, 6961u, 6976u, 7090u, 7100u, 7103u, 7106u, 7119u, 7123u, 7208u,
    7211u, 7213u, 7215u, 7217u, 7227u, 7236u, 7238u, 7245u, 7247u, 7249u,
    7254u, 7256u, 7332u, 7335u, 7347u, 7353u, 7373u, 7386u, 7388u, 7459u,
    7480u, 7495u, 7585u, 7624u, 7645u, 7647u, 7712u, 7736u, 7881u, 7904u,
    7967u, 7991u, 8033u, 8094u, 8137u, 8163u, 8246u, 8266u, 8349u, 8393u,
    8419u, 8476u, 8503u, 8549u, 8731u, 8756u, 8759u, 8777u, 8780u, 8805u,
    8882u, 8904u, 8986u, 9007u, 9016u, 9038u, 9062u, 9132u, 9145u, 9159u,
    9242u, 9271u, 9274u, 9286u, 9289u, 9296u, 9318u, 9396u, 9404u, 9406u,
    9408u, 9410u, 9412u, 9497u, 9521u, 9547u, 9555u, 9575u, 9646u, 9703u,
    9753u, 9806u, 9920u, 9958u, 10010u, 10064u, 10176u, 10214u, 10266u, 10304u,
    10431u, 10469u, 10523u, 10561u, 10687u, 10725u, 10779u, 10818u, 10909u, 10941u,
    10980u, 11075u, 11107u, 11165u, 11197u, 11204u, 11295u, 11323u, 11362u, 11424u,
    11450u, 11461u, 11489u, 11590u, 11615u, 11681u, 11683u, 11703u, 11705u, 11720u,
    11742u, 11849u, 11868u, 11940u, 11942u, 11956u, 11958u, 11979u, 11981u, 11995u,
    12072u, 12075u, 12077u, 12080u, 12082u, 12111u, 12113u, 12116u, 12119u, 12121u
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

ivec2 beeOrbitTarget(uint aux, uint step) {
    uint phase = step / 8u;
    return beeHomeCenterFromAux(aux) +
           beeRotateOffset(beeFormationOffset(beeFormationSlotFromAux(aux)), phase);
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
