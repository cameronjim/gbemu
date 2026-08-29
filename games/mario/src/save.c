// the same battery slot crossy and flappy keep, with mario's own magic. it is touched twice a
// session, so it rides in bank 5 with the rest of the between-levels code
#pragma bank 5

#include "save.h"

#include "level.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

static volatile uint8_t* const sram = (volatile uint8_t*)kSramBase;
// systems.md: smbd saves the specific level, not the world. the english build resets form and
// score on a reload, so the slot carries the furthest level and the score it was reached with,
// and a continue starts that level small with fresh lives
static uint8_t furthest;
static uint16_t score_at;

static uint8_t magic_ok(void) {
    return (uint8_t)(sram[0] == (uint8_t)kSaveMagic0 && sram[1] == (uint8_t)kSaveMagic1 &&
                     sram[2] == (uint8_t)kSaveMagic2 && sram[3] == (uint8_t)kSaveMagic3);
}

static void store(void) {
    sram[0] = (uint8_t)kSaveMagic0;
    sram[1] = (uint8_t)kSaveMagic1;
    sram[2] = (uint8_t)kSaveMagic2;
    sram[3] = (uint8_t)kSaveMagic3;
    sram[kSaveLevelOffset] = furthest;
    sram[kSaveScoreOffset] = (uint8_t)score_at;
    sram[kSaveScoreOffset + 1U] = (uint8_t)(score_at >> 8);
}

void save_init(void) BANKED {
    ENABLE_RAM;
    SWITCH_RAM(0);
    if (magic_ok() != 0U && sram[kSaveLevelOffset] < (uint8_t)kLevelCount) {
        furthest = sram[kSaveLevelOffset];
        score_at = (uint16_t)(sram[kSaveScoreOffset] | ((uint16_t)sram[kSaveScoreOffset + 1U] << 8));
    } else {
        furthest = 0;
        score_at = 0;
        store();
    }
    DISABLE_RAM;
}

uint8_t save_has_progress(void) {
    return (furthest != 0U) ? 1U : 0U;
}

uint8_t save_level(void) {
    return furthest;
}

void save_record(uint8_t level, uint16_t score) {
    if (level <= furthest) {
        return;
    }
    furthest = level;
    score_at = score;
    ENABLE_RAM;
    store();
    DISABLE_RAM;
}
