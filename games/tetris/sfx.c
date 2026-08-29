#include "sfx.h"

#include "tetris.h"

#include <gb/gb.h>
#include <stdint.h>

// pandocs: nr52 must be powered on before any other sound register keeps a write
void sfx_init(void) {
    NR52_REG = kNr52On;
    NR51_REG = kNr51All;
    NR50_REG = kNr50Max;
}

// every ch1 sound writes nr10, so a previous sweep can never bleed into the next one
void sfx_rotate(void) {
    NR10_REG = kRotateSweep;
    NR11_REG = kRotateDuty;
    NR12_REG = kRotateEnvelope;
    NR13_REG = kRotateFreqLo;
    NR14_REG = kRotateFreqHi;
}

void sfx_lock(void) {
    NR41_REG = kLockLength;
    NR42_REG = kLockEnvelope;
    NR43_REG = kLockPoly;
    NR44_REG = kLockTrigger;
}

void sfx_clear(uint8_t n) {
    NR21_REG = kClearDuty;
    if (n >= kTetrisRows) {
        NR22_REG = kTetrisEnvelope;
        NR23_REG = kTetrisFreqLo;
        NR24_REG = kTetrisFreqHi;
        return;
    }
    NR22_REG = kClearEnvelope;
    NR23_REG = kClearFreqLo;
    NR24_REG = kClearFreqHi;
}

void sfx_level(void) {
    NR10_REG = kLevelSweep;
    NR11_REG = kLevelDuty;
    NR12_REG = kLevelEnvelope;
    NR13_REG = kLevelFreqLo;
    NR14_REG = kLevelFreqHi;
}

void sfx_over(void) {
    NR10_REG = kOverSweep;
    NR11_REG = kOverDuty;
    NR12_REG = kOverEnvelope;
    NR13_REG = kOverFreqLo;
    NR14_REG = kOverFreqHi;
}
