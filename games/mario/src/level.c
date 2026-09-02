#include "level.h"

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>
#include <string.h>

uint8_t level_grid[LEVEL_GRID_COLUMNS][LEVEL_ROW_STRIDE];
uint16_t level_columns;
const LevelInfo* level;
const AreaInfo* level_sub;

static uint8_t current;
// m8b's bank-0 relief: the table moved to kLevelTableBank, so the entry being played is copied here
// and every level->/level_sub-> read after that is a plain ram load with no bank switch behind it
static LevelInfo active;
static AreaInfo active_area;

void level_select(uint8_t index) {
    const uint8_t caller_bank = CURRENT_BANK;

    current = (index < (uint8_t)kLevelCount) ? index : 0U;
    SWITCH_ROM_MBC5((uint8_t)kLevelTableBank);
    memcpy(&active, &kLevels[current], sizeof(LevelInfo));
    SWITCH_ROM_MBC5(caller_bank);
    level = &active;
}

// the only bank switch left in the engine, and it runs with the lcd off beside terrain_init's fill
void level_load(uint8_t next_area) {
    // whoever called may itself be banked, and returning into bank 0's window would land them in
    // another module's code; the caller's bank goes back exactly as it was
    const uint8_t caller_bank = CURRENT_BANK;
    const uint8_t* src;
    uint16_t c;
    uint8_t r;

    if (next_area == (uint8_t)kAreaMain) {
        level_sub = 0;
        level_columns = active.columns;
        src = active.grid;
        SWITCH_ROM_MBC5(active.bank);
    } else {
        SWITCH_ROM_MBC5((uint8_t)kLevelTableBank);
        memcpy(&active_area, &active.areas[next_area], sizeof(AreaInfo));
        level_sub = &active_area;
        level_columns = active_area.columns;
        src = active_area.grid;
        SWITCH_ROM_MBC5(active_area.bank);
    }
    for (c = 0; c < level_columns; ++c) {
        for (r = 0; r < (uint8_t)LEVEL_ROWS; ++r) {
            level_grid[c][r] = src[r];
        }
        src += LEVEL_ROW_STRIDE;
    }
    SWITCH_ROM_MBC5(caller_bank);
}

