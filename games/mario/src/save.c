// the battery slot crossy and flappy keep, widened to three files. it is touched a handful of
// times a session and never inside a frame of play, so it rides in bank 5 with the rest of the
// between-levels code. it has to ride there and nowhere else: most of what it publishes is not
// BANKED, so a caller reaches it with a plain call, and that only lands on the right bytes while
// bank 5 is the bank switched in - which it is, because every caller is itself bank 5 code
#pragma bank 5

#include "save.h"

#include "level.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

static volatile uint8_t* const sram = (volatile uint8_t*)kSramBase;

// the staged copy of the three slots. every read is a plain ram load; sram is only ever opened to
// write, which keeps ENABLE_RAM/DISABLE_RAM off every path but the four that change something
static uint8_t slot_used[kSaveSlots];
static uint8_t slot_level[kSaveSlots];
static uint16_t slot_score[kSaveSlots];
static uint8_t current = kSaveNoSlot;

static uint8_t slot_at(uint8_t slot) {
    return (uint8_t)(kSaveSlotBase + slot * kSaveSlotStride);
}

static uint8_t magic_ok(uint8_t version) {
    return (uint8_t)(sram[0] == (uint8_t)kSaveMagic0 && sram[1] == (uint8_t)kSaveMagic1 &&
                     sram[2] == (uint8_t)kSaveMagic2 && sram[3] == version);
}

// one slot out of the staged copy and back into sram, header included: the header is four bytes
// and writing it here means a fresh cart is stamped by the first thing that touches a file
static void store_slot(uint8_t slot) {
    const uint8_t at = slot_at(slot);

    sram[0] = (uint8_t)kSaveMagic0;
    sram[1] = (uint8_t)kSaveMagic1;
    sram[2] = (uint8_t)kSaveMagic2;
    sram[3] = (uint8_t)kSaveMagic3;
    sram[at + kSaveSlotUsedOffset] = slot_used[slot];
    sram[at + kSaveSlotLevelOffset] = slot_level[slot];
    sram[at + kSaveSlotScoreOffset] = (uint8_t)slot_score[slot];
    sram[at + kSaveSlotScoreOffset + 1U] = (uint8_t)(slot_score[slot] >> 8);
}

static void store_all(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kSaveSlots; ++i) {
        store_slot(i);
    }
}

static void clear_slot(uint8_t slot) {
    slot_used[slot] = 0;
    slot_level[slot] = 0;
    slot_score[slot] = 0;
}

void save_init(void) BANKED {
    uint8_t i;
    uint8_t at;

    ENABLE_RAM;
    SWITCH_RAM(0);
    for (i = 0; i < (uint8_t)kSaveSlots; ++i) {
        clear_slot(i);
    }
    if (magic_ok((uint8_t)kSaveMagic3) != 0U) {
        for (i = 0; i < (uint8_t)kSaveSlots; ++i) {
            at = slot_at(i);
            // a slot is only believed whole: a level past the end of world one means the byte is
            // not a level at all, so the file is dropped rather than read as garbage
            if (sram[at + kSaveSlotUsedOffset] == 1U &&
                sram[at + kSaveSlotLevelOffset] <= (uint8_t)kLevelCount) {
                slot_used[i] = 1;
                slot_level[i] = sram[at + kSaveSlotLevelOffset];
                slot_score[i] = (uint16_t)(sram[at + kSaveSlotScoreOffset] |
                                           ((uint16_t)sram[at + kSaveSlotScoreOffset + 1U] << 8));
            }
        }
    } else if (magic_ok((uint8_t)kSaveLegacyMagic3) != 0U && sram[kSaveLegacyLevelOffset] != 0U &&
               sram[kSaveLegacyLevelOffset] <= (uint8_t)kLevelCount) {
        // the one-slot save that shipped before this. both of its fields mean exactly what they
        // still mean, so it migrates into file 1 rather than being thrown away; a legacy slot at
        // level 0 was the "no progress yet" state and migrates to nothing
        slot_used[0] = 1;
        slot_level[0] = sram[kSaveLegacyLevelOffset];
        slot_score[0] =
            (uint16_t)(sram[kSaveLegacyScoreOffset] | ((uint16_t)sram[kSaveLegacyScoreOffset + 1U] << 8));
        store_all();
    } else {
        store_all();
    }
    DISABLE_RAM;
}

uint8_t save_slot_used(uint8_t slot) {
    return (slot < (uint8_t)kSaveSlots) ? slot_used[slot] : 0U;
}

uint8_t save_slot_level(uint8_t slot) {
    return (slot < (uint8_t)kSaveSlots) ? slot_level[slot] : 0U;
}

uint16_t save_slot_score(uint8_t slot) {
    return (slot < (uint8_t)kSaveSlots) ? slot_score[slot] : 0U;
}

void save_erase(uint8_t slot) BANKED {
    if (slot >= (uint8_t)kSaveSlots) {
        return;
    }
    clear_slot(slot);
    ENABLE_RAM;
    store_slot(slot);
    DISABLE_RAM;
}

void save_select(uint8_t slot) {
    current = (slot < (uint8_t)kSaveSlots) ? slot : (uint8_t)kSaveNoSlot;
}

uint8_t save_current(void) {
    return current;
}

void save_begin(void) BANKED {
    if (current >= (uint8_t)kSaveSlots || slot_used[current] != 0U) {
        return;
    }
    slot_used[current] = 1;
    ENABLE_RAM;
    store_slot(current);
    DISABLE_RAM;
}

void save_record(uint8_t level, uint16_t score) {
    if (current >= (uint8_t)kSaveSlots || level > (uint8_t)kLevelCount ||
        (slot_used[current] != 0U && level <= slot_level[current])) {
        return;
    }
    slot_used[current] = 1;
    slot_level[current] = level;
    slot_score[current] = score;
    ENABLE_RAM;
    store_slot(current);
    DISABLE_RAM;
}
