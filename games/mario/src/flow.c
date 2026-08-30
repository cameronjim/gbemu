// the pipe transitions, banked for the same reason the title card is: none of them can happen
// inside a frame of play, and bank 0 has the level table and every collision path to carry
#pragma bank 5

#include "flow.h"

#include "blocks.h"
#include "camera.h"
#include "enemies.h"
#include "hazards.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
#include "player.h"
#include "powerup.h"
#include "save.h"
#include "terrain.h"
#include "title.h"

#include <gb/gb.h>
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
    hud_enter_level(level->timer);
    return hazards_count();
}

uint8_t flow_begin_run(uint8_t entry, uint8_t selected) {
    hud_new_game();
    // systems.md: the english build resets the form and the score on a reload, so a continue
    // carries the level it saved and nothing else. must-verify which one smbd's jp build keeps
    return (entry == (uint8_t)kMenuContinue && save_has_progress() != 0U) ? save_level() : selected;
}

// roster.json: "contact height determines the score bonus", in five bands from the pole's base to
// its top. the shaft's own span is split evenly between them - the bible gives no pixel boundaries
void flow_score_flag(int16_t feet) BANKED {
    static const uint16_t kBandPoints[kFlagBandCount] = kFlagBandPointsInit;
    const int16_t base = (int16_t)((int16_t)(level->flag_base_row + 1U) << 4);
    const int16_t top = (int16_t)((int16_t)level->flag_top_row << 4);
    int16_t span = (int16_t)(base - top);
    int16_t step = 0;
    int16_t edge = base;
    uint8_t band = 0;

    // by subtraction, not by dividing: one divide here pulls sdcc's signed 16-bit helper into
    // bank 0, which m8b had no room for
    while (span >= (int16_t)kFlagBandCount) {
        span = (int16_t)(span - (int16_t)kFlagBandCount);
        ++step;
    }
    while (band + 1U < (uint8_t)kFlagBandCount) {
        edge = (int16_t)(edge - step);
        if (feet > edge) {
            break;
        }
        ++band;
    }
    hud_score = (uint16_t)(hud_score + kScoreTens(kBandPoints[band]));
}

// how long the card on screen has been up; the states that own one all live here
static uint8_t card_timer;

uint8_t flow_after_death(void) BANKED {
    if (hud_lives != 0U) {
        --hud_lives;
    }
    if (hud_lives != 0U) {
        return kAfterDeathRespawn;
    }
    card_game_over();
    card_timer = 0;
    return kAfterDeathGameOver;
}

uint8_t flow_game_over_frame(void) BANKED {
    ++card_timer;
    return (card_timer >= (uint8_t)kGameOverFrames) ? 1U : 0U;
}

void flow_clear_card(void) BANKED {
    card_clear();
    card_timer = 0;
}

uint8_t flow_clear_frame(uint8_t* level) BANKED {
    uint8_t next;

    // smb converts whatever is left of the countdown into points before the card clears
    if (hud_spend_time_bonus() != 0U) {
        card_clear_refresh();
        return kAfterCardStay;
    }
    if (card_timer == 0U) {
        card_clear_refresh();
    }
    ++card_timer;
    if (card_timer < (uint8_t)kClearCardFrames) {
        return kAfterCardStay;
    }
    next = (uint8_t)(*level + 1U);
    if (next >= (uint8_t)kLevelCount) {
        return kAfterCardTitle;
    }
    save_record(next, hud_score);
    *level = next;
    return kAfterCardNext;
}

// the card wrote over the whole bg map, and every actor is exactly where it was frozen: the ring
// is refilled from column zero the way a level load does it and then streamed forward to where the
// camera already stands, which is lcd-off work either way. nothing but the bg is touched
void flow_resume_from_card(uint8_t area, uint16_t camera_x) BANKED {
    terrain_init(area);
    terrain_set_scroll_x(camera_x);
    terrain_stream_window();
    SHOW_SPRITES;
}

void flow_enter_sub_area(uint8_t index) BANKED {
    // the room's own AreaInfo has to be in ram before anything reads it: blocks_enter_area takes
    // the coin table off level_sub, and level_sub is still null while the main level is loaded, so
    // without this the room's coin list was whatever bytes sat at address zero - which is why none
    // of the room's coins could be picked up. terrain_init below loads it again, with the lcd off
    level_load(index);
    // block/coin state must point at this room before the ring paints it, or the coin blocks paint
    // from their raw compiled kind (always full) and stay visible even for coins already taken
    blocks_enter_area(index);
    terrain_init(index);
    enemies_enter_area(index);
    // the room's own entry cell: 1-1's drops him past the brick wall down its left edge, not into it
    player_place(level_sub->start_column, (uint8_t)level_sub->start_row);
    camera_init(player_x(), player_feet());
}

void flow_leave_sub_area(void) BANKED {
    const uint16_t column = level_sub->return_column;
    const uint8_t top_row = level_sub->return_top_row;

    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
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
