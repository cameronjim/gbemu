#pragma bank 5

#include "level.h"

#include "mario.h"
#include "romcopy.h"

#include <gb/gb.h>
#include <stdint.h>

uint8_t level_grid[LEVEL_GRID_COLUMNS][LEVEL_ROW_STRIDE];
uint16_t level_columns;
const LevelInfo* level;
const AreaInfo* level_sub;

static uint8_t current;
// m8b's bank-0 relief: the table moved to kLevelTableBank, so the entry being played is copied here
// and every level->/level_sub-> read after that is a plain ram load with no bank switch behind it
static LevelInfo active;
static AreaInfo active_area;

// the lists the entry's pointers name, copied out of the level's bank at select so the scans that
// walk them every frame read ram; sized to the largest level, like level_grid
static uint16_t arena_block_column[LEVEL_MAX_BLOCKS];
static uint8_t arena_block_row[LEVEL_MAX_BLOCKS];
static uint8_t arena_block_kind[LEVEL_MAX_BLOCKS];
static uint8_t arena_block_content[LEVEL_MAX_BLOCKS];
static uint16_t arena_enemy_column[LEVEL_MAX_ENEMIES];
static uint8_t arena_enemy_row[LEVEL_MAX_ENEMIES];
static uint8_t arena_enemy_kind[LEVEL_MAX_ENEMIES];
static uint16_t arena_object_column[LEVEL_MAX_OBJECTS];
static uint8_t arena_object_row[LEVEL_MAX_OBJECTS];
static uint8_t arena_object_kind[LEVEL_MAX_OBJECTS];
static uint8_t arena_object_param[LEVEL_MAX_OBJECTS];
static uint16_t arena_jump_target_column[LEVEL_MAX_JUMPS];
static uint8_t arena_jump_target_row[LEVEL_MAX_JUMPS];
static uint16_t arena_segment_x0[LEVEL_MAX_SEGMENTS];
static uint16_t arena_segment_x1[LEVEL_MAX_SEGMENTS];
static uint8_t arena_segment_type[LEVEL_MAX_SEGMENTS];
static uint8_t arena_coin_column[LEVEL_MAX_COINS];
static uint8_t arena_coin_row[LEVEL_MAX_COINS];
static uint8_t arena_warp_column[LEVEL_MAX_WARPS];
static uint8_t arena_warp_level[LEVEL_MAX_WARPS];

static const uint8_t* take8(const uint8_t* src, uint8_t* dst, uint8_t count) {
    rom_copy(active.bank, src, dst, count);
    return dst;
}

static const uint16_t* take16(const uint16_t* src, uint16_t* dst, uint8_t count) {
    rom_copy(active.bank, src, dst, (uint16_t)(count * 2U));
    return dst;
}

void level_select(uint8_t index) BANKED {
    current = (index < (uint8_t)kLevelCount) ? index : 0U;
    rom_copy((uint8_t)kLevelTableBank, &kLevels[current], &active, sizeof(LevelInfo));
    active.block_column = take16(active.block_column, arena_block_column, active.block_count);
    active.block_row = take8(active.block_row, arena_block_row, active.block_count);
    active.block_kind = take8(active.block_kind, arena_block_kind, active.block_count);
    active.block_content = take8(active.block_content, arena_block_content, active.block_count);
    active.enemy_column = take16(active.enemy_column, arena_enemy_column, active.enemy_count);
    active.enemy_row = take8(active.enemy_row, arena_enemy_row, active.enemy_count);
    active.enemy_kind = take8(active.enemy_kind, arena_enemy_kind, active.enemy_count);
    active.object_column = take16(active.object_column, arena_object_column, active.object_count);
    active.object_row = take8(active.object_row, arena_object_row, active.object_count);
    active.object_kind = take8(active.object_kind, arena_object_kind, active.object_count);
    active.object_param = take8(active.object_param, arena_object_param, active.object_count);
    active.jump_target_column =
        take16(active.jump_target_column, arena_jump_target_column, active.jump_count);
    active.jump_target_row = take8(active.jump_target_row, arena_jump_target_row, active.jump_count);
    active.segment_x0 = take16(active.segment_x0, arena_segment_x0, active.segment_count);
    active.segment_x1 = take16(active.segment_x1, arena_segment_x1, active.segment_count);
    active.segment_type = take8(active.segment_type, arena_segment_type, active.segment_count);
    level = &active;
}

// runs with the lcd off beside terrain_init's fill
void level_load(uint8_t next_area) BANKED {
    if (next_area == (uint8_t)kAreaMain) {
        level_sub = 0;
        level_columns = active.columns;
        rom_copy(active.bank, active.grid, level_grid, (uint16_t)(level_columns * LEVEL_ROW_STRIDE));
        return;
    }
    rom_copy((uint8_t)kLevelTableBank, &active.areas[next_area], &active_area, sizeof(AreaInfo));
    active_area.coin_column = take8(active_area.coin_column, arena_coin_column, active_area.coin_count);
    active_area.coin_row = take8(active_area.coin_row, arena_coin_row, active_area.coin_count);
    active_area.warp_column = take8(active_area.warp_column, arena_warp_column, active_area.warp_count);
    active_area.warp_level = take8(active_area.warp_level, arena_warp_level, active_area.warp_count);
    level_sub = &active_area;
    level_columns = active_area.columns;
    rom_copy(active_area.bank, active_area.grid, level_grid, (uint16_t)(level_columns * LEVEL_ROW_STRIDE));
}
