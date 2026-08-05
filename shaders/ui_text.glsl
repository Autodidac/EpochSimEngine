#ifndef SANDHYBRID_UI_TEXT_GLSL
#define SANDHYBRID_UI_TEXT_GLSL

layout(std430, binding = 6) readonly buffer UiTextStorageBuffer {
    uint uiTextStorage[];
};

const uint FIXED_TEXT_OFFSETS_BASE = 0u;
const uint FIXED_TEXT_WORDS_BASE = 190u;
const uint FIXED_TEXT_COUNT = 189u;

uint fixedTextLength(uint id) {
    if (id >= FIXED_TEXT_COUNT) return 0u;
    uint begin = uiTextStorage[FIXED_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[FIXED_TEXT_OFFSETS_BASE + id + 1u];
    return end - begin;
}

uint fixedTextChar(uint id, uint index) {
    if (id >= FIXED_TEXT_COUNT) return 32u;
    uint begin = uiTextStorage[FIXED_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[FIXED_TEXT_OFFSETS_BASE + id + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[FIXED_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

const uint MATERIAL_TEXT_OFFSETS_BASE = 573u;
const uint MATERIAL_TEXT_WORDS_BASE = 641u;
const uint MATERIAL_TEXT_COUNT = 67u;

uint materialTextLength(uint id) {
    if (id >= MATERIAL_TEXT_COUNT) return 0u;
    uint begin = uiTextStorage[MATERIAL_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[MATERIAL_TEXT_OFFSETS_BASE + id + 1u];
    return end - begin;
}

uint materialTextChar(uint id, uint index) {
    if (id >= MATERIAL_TEXT_COUNT) return 32u;
    uint begin = uiTextStorage[MATERIAL_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[MATERIAL_TEXT_OFFSETS_BASE + id + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[MATERIAL_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

const uint GROUP_TEXT_OFFSETS_BASE = 750u;
const uint GROUP_TEXT_WORDS_BASE = 759u;
const uint GROUP_TEXT_COUNT = 8u;

uint groupTextLength(uint id) {
    if (id >= GROUP_TEXT_COUNT) return 0u;
    uint begin = uiTextStorage[GROUP_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[GROUP_TEXT_OFFSETS_BASE + id + 1u];
    return end - begin;
}

uint groupTextChar(uint id, uint index) {
    if (id >= GROUP_TEXT_COUNT) return 32u;
    uint begin = uiTextStorage[GROUP_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[GROUP_TEXT_OFFSETS_BASE + id + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[GROUP_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

const uint SCENE_TEXT_OFFSETS_BASE = 773u;
const uint SCENE_TEXT_WORDS_BASE = 783u;
const uint SCENE_TEXT_COUNT = 9u;

uint sceneTextLength(uint id) {
    if (id >= SCENE_TEXT_COUNT) return 0u;
    uint begin = uiTextStorage[SCENE_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[SCENE_TEXT_OFFSETS_BASE + id + 1u];
    return end - begin;
}

uint sceneTextChar(uint id, uint index) {
    if (id >= SCENE_TEXT_COUNT) return 32u;
    uint begin = uiTextStorage[SCENE_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[SCENE_TEXT_OFFSETS_BASE + id + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[SCENE_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

const uint PHASE_TEXT_OFFSETS_BASE = 805u;
const uint PHASE_TEXT_WORDS_BASE = 815u;
const uint PHASE_TEXT_COUNT = 9u;

uint phaseTextLength(uint id) {
    if (id >= PHASE_TEXT_COUNT) return 0u;
    uint begin = uiTextStorage[PHASE_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[PHASE_TEXT_OFFSETS_BASE + id + 1u];
    return end - begin;
}

uint phaseTextChar(uint id, uint index) {
    if (id >= PHASE_TEXT_COUNT) return 32u;
    uint begin = uiTextStorage[PHASE_TEXT_OFFSETS_BASE + id];
    uint end = uiTextStorage[PHASE_TEXT_OFFSETS_BASE + id + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[PHASE_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

const uint GROUP_MATERIAL_BASE = 835u;
const uint GROUP_MATERIAL_COUNTS_BASE = 827u;
const uint GROUP_COUNT = 8u;
const uint GROUP_MATERIAL_SLOTS = 10u;

uint groupMaterialCount(uint group) {
    return group < GROUP_COUNT ? uiTextStorage[GROUP_MATERIAL_COUNTS_BASE + group] : 0u;
}

uint groupMaterial(uint group, uint slot) {
    if (group >= GROUP_COUNT || slot >= GROUP_MATERIAL_SLOTS) return MATERIAL_COUNT;
    return uiTextStorage[GROUP_MATERIAL_BASE + group * GROUP_MATERIAL_SLOTS + slot];
}

const uint CARD_TEXT_OFFSETS_BASE = 915u;
const uint CARD_TEXT_WORDS_BASE = 1586u;
const uint CARD_MATERIAL_COUNT = 67u;
const uint CARD_LINE_COUNT = 10u;

uint cardTextLength(uint id, uint line) {
    if (id >= CARD_MATERIAL_COUNT || line >= CARD_LINE_COUNT) return 0u;
    uint key = id * CARD_LINE_COUNT + line;
    uint begin = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key];
    uint end = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key + 1u];
    return end - begin;
}

uint cardTextChar(uint id, uint line, uint index) {
    if (id >= CARD_MATERIAL_COUNT || line >= CARD_LINE_COUNT) return 32u;
    uint key = id * CARD_LINE_COUNT + line;
    uint begin = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key];
    uint end = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key + 1u];
    if (index >= end - begin) return 32u;
    uint byteIndex = begin + index;
    uint word = uiTextStorage[CARD_TEXT_WORDS_BASE + (byteIndex >> 2u)];
    return (word >> ((byteIndex & 3u) * 8u)) & 255u;
}

#endif
