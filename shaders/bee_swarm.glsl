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
    1084u, 1088u, 1205u, 1210u, 1223u, 1341u, 1347u, 1355u, 1460u, 1463u,
    1481u, 1485u, 1585u, 1711u, 1747u, 1871u, 1964u, 2002u, 2132u, 2348u,
    2390u, 2472u, 2730u, 2776u, 2855u, 3162u, 3241u, 3288u, 3366u, 3496u,
    3545u, 3750u, 3783u, 3880u, 3906u, 3929u, 4029u, 4032u, 4036u, 4042u,
    4045u, 4184u, 4263u, 4284u, 4303u, 4314u, 4410u, 4520u, 4562u, 4696u,
    4777u, 4794u, 4820u, 4951u, 6463u, 6588u, 6594u, 6718u, 6725u, 6841u,
    6983u, 7096u, 7195u, 7198u, 7321u, 7396u, 7447u, 7453u, 7528u, 7571u,
    7573u, 7577u, 7654u, 7711u, 7785u, 7788u, 7827u, 7966u, 8012u, 8045u,
    8078u, 8081u, 8175u, 8223u, 8244u, 8305u, 8333u, 8349u, 8370u, 8397u,
    8463u, 8562u, 8652u, 8717u, 8733u, 8755u, 8946u, 8971u, 9076u, 9118u,
    9162u, 9165u, 9268u, 9308u, 9333u, 9356u, 9373u, 9417u, 9420u, 9461u,
    9482u, 9503u, 9527u, 9543u, 9587u, 9610u, 9612u, 9659u, 9693u, 9797u,
    9867u, 9869u, 9888u, 10018u, 10076u, 10100u, 10124u, 10206u, 10273u, 10355u,
    10379u, 10381u, 10403u, 10460u, 10534u, 10537u, 10613u, 10637u, 10792u, 10795u,
    10845u, 10995u, 11021u, 11098u, 11277u, 11279u, 11355u, 11378u, 11481u, 11606u,
    11663u, 11736u, 11760u, 11794u, 11860u, 11990u, 12013u, 12109u, 12115u, 12143u,
    12178u, 12241u, 12308u, 12396u, 12491u, 12595u, 12649u, 12693u, 12721u, 12748u,
    12780u, 12824u, 12903u, 12906u, 12950u, 12954u, 12976u, 13005u, 13161u, 13208u,
    13212u, 13264u, 13284u, 13358u, 13395u, 13410u, 13414u, 13469u, 13475u, 13478u,
    13482u, 13526u, 13530u, 13535u, 13599u, 13660u, 13732u, 13735u, 13785u, 13792u
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
    // Each bee circles a small moving anchor. The anchor preserves the symbol,
    // while the local orbit makes the silhouette read as a living swarm.
    uint phase = step / 2u + slot * 5u;
    int radius = 2 + int((slot * 13u) % 3u);
    return beeRotateOffset(ivec2(radius, 0), phase);
}

ivec2 beeOrbitTarget(uint aux, uint step) {
    uint slot = beeFormationSlotFromAux(aux);
    uint shapePhase = step / 24u;
    ivec2 anchor = beeHomeCenterFromAux(aux) +
                   beeRotateOffset(beeFormationOffset(slot), shapePhase);
    return anchor + beeFlutterOffset(slot, step);
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
