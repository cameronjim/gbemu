#include "save.h"

#include "tetris.h"

#include <gb/gb.h>
#include <stdint.h>

static volatile uint8_t* const sram = (volatile uint8_t*)kSramBase;
static uint32_t best;

static uint8_t magic_ok(void) {
    return (uint8_t)(sram[0] == kSaveMagic0 && sram[1] == kSaveMagic1 && sram[2] == kSaveMagic2 &&
                     sram[3] == kSaveMagic3);
}

static void store(uint32_t value) {
    sram[0] = kSaveMagic0;
    sram[1] = kSaveMagic1;
    sram[2] = kSaveMagic2;
    sram[3] = kSaveMagic3;
    sram[kSaveBestOffset] = (uint8_t)value;
    sram[kSaveBestOffset + 1U] = (uint8_t)(value >> 8);
    sram[kSaveBestOffset + 2U] = (uint8_t)(value >> 16);
    sram[kSaveBestOffset + 3U] = (uint8_t)(value >> 24);
}

void save_init(void) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    if (magic_ok()) {
        // one statement per byte: sdcc mis-evaluates a single combined 32-bit shift-and-or here
        best = sram[kSaveBestOffset];
        best |= (uint32_t)sram[kSaveBestOffset + 1U] << 8;
        best |= (uint32_t)sram[kSaveBestOffset + 2U] << 16;
        best |= (uint32_t)sram[kSaveBestOffset + 3U] << 24;
    } else {
        best = 0;
        store(0);
    }
    DISABLE_RAM;
}

uint32_t save_best(void) {
    return best;
}

void save_record(uint32_t score) {
    if (score <= best) {
        return;
    }
    best = score;
    ENABLE_RAM;
    store(best);
    DISABLE_RAM;
}
