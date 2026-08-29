// the pipe transitions, banked for the same reason the title card is: none of them can happen
// inside a frame of play, and bank 0 has the level table and every collision path to carry
#pragma bank 5

#include "flow.h"

#include "blocks.h"
#include "camera.h"
#include "enemies.h"
#include "hazards.h"
#include "level.h"
#include "mario.h"
#include "player.h"
#include "powerup.h"
#include "terrain.h"

#include <stdint.h>

uint8_t flow_enter_level(uint8_t index) BANKED {
    powerup_reset();
    level_select(index);
    blocks_load_level();
    blocks_player_big = 0;
    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    hazards_load_level();
    player_init();
    enemies_load_level();
    camera_init(player_x(), player_feet());
    return hazards_count();
}

void flow_enter_sub_area(uint8_t index) BANKED {
    terrain_init(index);
    blocks_enter_area(index);
    enemies_enter_area(index);
    player_place(0U, (uint8_t)level_sub->start_row);
    camera_init(player_x(), player_feet());
}

void flow_leave_sub_area(void) BANKED {
    const uint16_t column = level_sub->return_column;
    const uint8_t top_row = level_sub->return_top_row;

    terrain_init(kAreaMain);
    blocks_enter_area(kAreaMain);
    enemies_enter_area(kAreaMain);
    player_begin_pipe_up(column, top_row);
    // the camera is framed on where he ends up standing, not on the shaft he is still climbing out of
    camera_init((uint16_t)(column << 4), (int16_t)((int16_t)top_row << 4));
}

uint8_t flow_pipe_under_player(void) BANKED {
    uint8_t i;

    for (i = 0; i < level->object_count; ++i) {
        if (level->object_kind[i] != (uint8_t)kObjPipe) {
            continue;
        }
        if (player_over_pipe(level->object_column[i], level->object_row[i]) != 0U) {
            return level->object_param[i];
        }
    }
    return 0xFF;
}

uint8_t flow_warp_under_player(void) BANKED {
    uint8_t i;

    if (level_sub == 0 || level_sub->kind != (uint8_t)kAreaKindWarp) {
        return 0xFF;
    }
    for (i = 0; i < level_sub->warp_count; ++i) {
        if (player_over_pipe(level_sub->warp_column[i], level_sub->exit_top_row) != 0U) {
            return level_sub->warp_level[i];
        }
    }
    return 0xFF;
}

uint8_t flow_over_exit_pipe(void) BANKED {
    return (level_sub != 0) ? player_over_pipe(level_sub->exit_column, level_sub->exit_top_row) : 0U;
}
