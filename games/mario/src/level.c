#include "level.h"

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

uint8_t level_grid[LEVEL_GRID_COLUMNS][LEVEL_ROW_STRIDE];
uint16_t level_columns;
const LevelInfo* level;
const AreaInfo* level_sub;

static uint8_t current;

void level_select(uint8_t index) {
    current = (index < (uint8_t)kLevelCount) ? index : 0U;
    level = &kLevels[current];
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
        level_columns = level->columns;
        src = level->grid;
        SWITCH_ROM_MBC5(level->bank);
    } else {
        level_sub = &level->areas[next_area];
        level_columns = level_sub->columns;
        src = level_sub->grid;
        SWITCH_ROM_MBC5(level_sub->bank);
    }
    for (c = 0; c < level_columns; ++c) {
        for (r = 0; r < (uint8_t)LEVEL_ROWS; ++r) {
            level_grid[c][r] = src[r];
        }
        src += LEVEL_ROW_STRIDE;
    }
    SWITCH_ROM_MBC5(caller_bank);
}

uint8_t level_palette_set(void) {
    // every sub-area smb has is a room underground, whatever the level holding it looks like
    return (level_sub != 0) ? (uint8_t)kLevelTypeUnderground : level->type;
}
