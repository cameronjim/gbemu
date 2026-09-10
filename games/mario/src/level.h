#ifndef LEVEL_H
#define LEVEL_H

#include "levels.h"

#include <gb/gb.h>
#include <stdint.h>

extern uint8_t level_grid[LEVEL_GRID_COLUMNS][LEVEL_ROW_STRIDE];
extern uint16_t level_columns;
extern const LevelInfo* level;
extern const AreaInfo* level_sub;

void level_select(uint8_t index) BANKED;
void level_load(uint8_t area) BANKED;

#endif
