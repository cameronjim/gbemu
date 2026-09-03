#ifndef LEVEL_H
#define LEVEL_H

#include "levels.h"

#include <stdint.h>

// the grid being played, unpacked out of its rom bank once at load time. the engine probes solidity
// six to eight times a frame and streams a column every 16 px, and a bank switch on each of those
// costs the heavy frames their whole margin; a ram copy makes every probe a plain indexed load,
// leaves the rom bank alone, and is what lets enemies.c and hazards.c run banked
extern uint8_t level_grid[LEVEL_GRID_COLUMNS][LEVEL_ROW_STRIDE];
extern uint16_t level_columns;

// the level being played and the sub-area inside it, null while the main grid is loaded
extern const LevelInfo* level;
extern const AreaInfo* level_sub;

// copies the table entry out of kLevelTableBank into ram; every level-> read afterwards is a plain
// load, which is what lets the whole table live outside bank 0
void level_select(uint8_t index);

// unpacks kAreaMain or one of the level's sub-areas; switches banks, so it stays in bank 0
void level_load(uint8_t area);

#endif
