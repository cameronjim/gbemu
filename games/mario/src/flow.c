// the pipe transitions, banked for the same reason the title card is: none of them can happen
// inside a frame of play, and bank 0 has the level table and every collision path to carry
#pragma bank 5

#include "flow.h"

#include "assets.h"
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

uint8_t flow_grid_coins;
uint8_t flow_side_pipes;
uint8_t flow_pipe_side_armed;

// whether this level has a sideways pipe at all, worked out once at load. the walk-in trigger is
// the one pipe test that cannot hang off a button edge - he holds right into the mouth - so
// without this the game loop would ask the player module for his stance, and then bank 5 for the
// object list, on every frame of every walk in every level. 1-1's frames do not have that to give
static void scan_side_pipes(void) {
    uint8_t i;

    flow_side_pipes = 0;
    for (i = 0; i < level->object_count; ++i) {
        if (level->object_kind[i] == (uint8_t)kObjPipeSide) {
            flow_side_pipes = 1;
            return;
        }
    }
}

// loose coins the compiler stamped straight into the main grid (1-2 is full of them) are picked up
// by walking through them, which is a probe of the cells his box covers on every frame of play.
// most levels have none at all, so the grid is read once here - lcd off, beside the level load -
// and the game loop skips the whole path unless this says there is something to find
static void scan_grid_coins(void) {
    uint16_t c;
    uint8_t r;

    flow_grid_coins = 0;
    for (c = 0; c < level_columns; ++c) {
        for (r = 0; r < (uint8_t)LEVEL_ROWS; ++r) {
            if (level_grid[c][r] == (uint8_t)kBlockCoin) {
                flow_grid_coins = 1;
                return;
            }
        }
    }
}

uint8_t flow_enter_level(uint8_t index) BANKED {
    powerup_enter_level();
    level_select(index);
    blocks_load_level();
    // the form carries between levels, so the dispenser rule sees it on the first frame rather
    // than a frame later, when the play loop's own copy catches up
    blocks_player_big = (uint8_t)((powerup_flags & kPowerFlagBig) != 0U ? 1U : 0U);
    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    scan_grid_coins();
    scan_side_pipes();
    hazards_load_level();
    player_init();
    // player_init always stands him up small; a carried form has to be his own height before the
    // first frame is drawn, or the play loop grows him in place on frame one and he pops upward
    player_set_big(powerup_pose);
    enemies_load_level();
    camera_init(player_x(), player_feet());
    hud_enter_level(level->timer, index);
    return hazards_count();
}

uint8_t flow_begin_run(uint8_t selected) {
    // a fresh run is small mario, whatever the last one ended as
    powerup_reset();
    // systems.md: the english build resets the form and the score on a reload, so picking a file
    // carries the level it saved and nothing else - three fresh lives and a score of zero. the
    // levels a run walks between on the map share those, which is why nothing resets them again
    hud_new_game();
    return selected;
}

// the pennant's row as it comes down the pole, and its own step timer. they live in this bank with
// the code that walks them; nothing outside it ever reads either
static uint8_t flag_row;
static uint8_t flag_timer;

void flow_flag_arm(void) BANKED {
    flag_row = level->flag_top_row;
    flag_timer = 0;
}

uint8_t flow_flag_step(void) BANKED {
    if (level->has_flag == 0U || flag_row >= level->flag_base_row) {
        return 1;
    }
    ++flag_timer;
    if (flag_timer >= (uint8_t)kClearFlagStepFrames) {
        flag_timer = 0;
        // the old cell back to sky, the pennant one row lower: the cloth hangs left of the shaft
        terrain_set_cell((int16_t)((int16_t)level->flag_column - 1), (int16_t)flag_row,
                         (uint8_t)kBlockEmpty);
        ++flag_row;
        terrain_set_cell((int16_t)((int16_t)level->flag_column - 1), (int16_t)flag_row,
                         (uint8_t)kBlockFlagCloth);
    }
    return (uint8_t)(flag_row >= level->flag_base_row ? 1U : 0U);
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

// which of the three bg palette sets is on screen, so a card resume can put the same one back
static uint8_t palette_set;

static void load_palette_set(uint8_t set_palettes) {
    palette_set = set_palettes;
    if (set_palettes == (uint8_t)kLevelTypeUnderground) {
        assets_load_bg_palettes_underground();
    } else if (set_palettes == (uint8_t)kLevelTypeCastle) {
        assets_load_bg_palettes_castle();
    } else {
        assets_load_bg_palettes();
    }
}

uint8_t flow_after_death(void) BANKED {
    // the form is the one thing a level clear carries and a death does not
    powerup_reset();
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
    // the node after this one, which may be kLevelCount: world one finished, every node cleared.
    // the file records that as is - it is what "furthest unlocked" means - and front_cleared is
    // what opens the node and clamps this back to a real one before the map puts mario down
    next = (uint8_t)(*level + 1U);
    save_record(next, hud_score);
    *level = next;
    return kAfterCardMap;
}

// the card wrote over the whole bg map, and every actor is exactly where it was frozen: the ring
// is refilled from column zero the way a level load does it and then streamed forward to where the
// camera already stands, which is lcd-off work either way. nothing but the bg is touched
void flow_resume_from_card(uint8_t area, uint16_t camera_x) BANKED {
    // terrain_init reloads the palette set for column zero, which on 1-2 is its above-ground start
    // segment, and working it out again afterwards is no better: the camera stands a screen behind
    // mario, so just past a segment boundary it names the segment he has already left. the set that
    // was on screen when the card went up is the one that belongs back on it
    const uint8_t was = palette_set;

    terrain_init(area);
    terrain_set_scroll_x(camera_x);
    terrain_stream_window();
    load_palette_set(was);
    SHOW_SPRITES;
}

void terrain_sync_palette(void) BANKED {
    load_palette_set(level_palette_set(terrain_camera_x() >> 4));
}

// a same-grid segment jump (1-2's entrance/exit pipes): current_area never changes and level_grid
// never reloads (the whole level, every segment, is already unpacked into it), but the vram ring
// and the bg palette still have to catch up to wherever he landed - the same lcd-off work a
// sub-area swap pays, just reached through terrain's incremental scroll path since the grid
// itself needs no work. called from flow_enter_sub_area below, never on its own: main.c's state
// machine only ever sees the one pending_area value, kJumpAreaFlag and all.
//
// answers 1 when the landing armed a rise out of a pipe, which the caller has to hold its pipe-up
// state open for. the target cell decides which landing it is, so the data format never had to
// grow a field for it: a kBlockPipeTl there is a pipe cap and he comes up out of it exactly the
// way a bonus room's return pipe does (1-2's underground exit lands on the ending's pipe), and
// anything else is a plain placement - with nothing solid under his feet, which is 1-2's entrance
// shaft, that placement is the top of an eleven row fall
static uint8_t pipe_jump(uint8_t index) {
    const uint16_t column = level->jump_target_column[index];
    const uint8_t row = level->jump_target_row[index];

    terrain_set_scroll_x((uint16_t)(column << 4));
    terrain_stream_window();
    terrain_sync_palette();
    if (terrain_kind_at((int16_t)column, (int16_t)row) == (uint8_t)kBlockPipeTl) {
        player_begin_pipe_up(column, row);
        // framed on where he ends up standing, not on the shaft he is climbing out of
        camera_init((uint16_t)(column << 4), (int16_t)((int16_t)row << 4));
        return 1U;
    }
    player_place(column, row);
    // player_place stands him on the cell it names, so an empty one has to be let go of: a bounce
    // of zero speed is the reset that does it - off the ground with gravity already running, and
    // with the jump edge spent, so an a button still held from the pipe cannot launch him
    if (terrain_floor_at((int16_t)column, (int16_t)row) == 0U) {
        player_stomp_bounce(0);
    }
    camera_init(player_x(), player_feet());
    return 0U;
}

uint8_t flow_enter_sub_area(uint8_t index) BANKED {
    if ((index & (uint8_t)kJumpAreaFlag) != 0U) {
        return pipe_jump((uint8_t)(index & (uint8_t)~kJumpAreaFlag));
    }
    // the room's own grid is about to replace the main one, so its coins belong to blocks.c's
    // coin list from here until the return trip re-scans
    flow_grid_coins = 0;
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
    return 0U;
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
    scan_grid_coins();
}

// a sideways mouth is entered by walking into it, so the test is a wall contact rather than a
// stance: his right shoulder against the rim column (collide_x parks the hitbox exactly one px
// short of a solid cell), still on the near side of it, and his body overlapping the two rows the
// mouth spans. the caller has already checked that he is grounded, still and holding right
static uint8_t at_side_mouth(uint16_t column, uint8_t row) {
    const uint16_t rim = (uint16_t)(column << 4);
    const uint16_t left = (uint16_t)(player_x() + kPlayerHitInsetPx);
    const int16_t feet = player_feet();

    if ((uint16_t)(left + kPlayerHitWidthPx) < rim || left >= rim) {
        return 0;
    }
    return ((int16_t)(player_box_top() >> 4) <= (int16_t)((int16_t)row + 1) &&
            (int16_t)((feet - 1) >> 4) >= (int16_t)row)
               ? 1U
               : 0U;
}

uint8_t flow_pipe_target(uint8_t down_held) BANKED {
    uint8_t i;

    flow_pipe_side_armed = 0;
    for (i = 0; i < level->object_count; ++i) {
        const uint8_t kind = level->object_kind[i];
        const uint16_t column = level->object_column[i];
        const uint8_t row = level->object_row[i];

        if (down_held == 0U) {
            // right is held instead, so only a sideways mouth can answer. the same flagged return
            // an ordinary jump pipe gives, plus the byte that tells main.c to walk him in rather
            // than sink him
            if (kind == (uint8_t)kObjPipeSide && at_side_mouth(column, row) != 0U) {
                flow_pipe_side_armed = 1;
                return (uint8_t)(kJumpAreaFlag | level->object_param[i]);
            }
        } else if (kind == (uint8_t)kObjPipe) {
            if (player_over_pipe(column, row) != 0U) {
                return level->object_param[i];
            }
        } else if (kind == (uint8_t)kObjPipeJump) {
            // a same-grid segment teleport (1-2's entrance/exit pipes): folded into the same scan
            // and the same return value as an ordinary sub-area pipe, flagged so
            // flow_enter_sub_area can tell them apart - main.c's state machine never has to
            if (player_over_pipe(column, row) != 0U) {
                return (uint8_t)(kJumpAreaFlag | level->object_param[i]);
            }
        }
    }
    return 0xFF;
}

// the cells his box covers, at most two columns by three rows, and never the whole grid: a coin
// among them is cleared through terrain's own write path so the ram grid, the ring cache and vram
// all agree, and pays exactly what a sub-area coin pays. a death reloads the grid from rom and the
// coins come back, which is smb's own behaviour
void flow_collect_grid_coins(void) BANKED {
    const uint16_t px = player_x();
    const int16_t top = player_box_top();
    const int16_t last_row = (int16_t)((int16_t)(top + (int16_t)player_box_height() - 1) >> 4);
    const int16_t last_col = (int16_t)((uint16_t)(px + kPlayerWidthPx - 1U) >> 4);
    int16_t c;
    int16_t r;

    for (c = (int16_t)(px >> 4); c <= last_col && c < (int16_t)level_columns; ++c) {
        for (r = (top < 0) ? 0 : (int16_t)(top >> 4); r <= last_row && r < (int16_t)LEVEL_ROWS; ++r) {
            if (level_grid[c][r] != (uint8_t)kBlockCoin) {
                continue;
            }
            terrain_clear_cell(c, r);
            ++hud_coins;
            hud_score = (uint16_t)(hud_score + kScoreTens(kCoinPoints));
        }
    }
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
