#include "blocks.h"

#include "assets.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// the item slot's phases: rising out of the block it came from, then loose in the world
#define kItemRising 0U
#define kItemLoose 1U

// which grid the state below belongs to; the two never react at the same time
static uint8_t area;
// the loaded sub-area's coin list, or an empty one while the main grid is up
static const uint8_t* coin_col;
static const uint8_t* coin_row_of;
static uint8_t area_coins;

// per compiled block, indexed by the generated reaction list
static uint8_t state[LEVEL_MAX_BLOCKS];
static uint8_t budget[LEVEL_MAX_BLOCKS];
static uint8_t coin_taken[LEVEL_MAX_COINS];
uint8_t blocks_player_big;

// only one cell bounces at a time, smb-style
static int16_t bump_column;
static int16_t bump_row;
static uint8_t bump_timer;

// the single item slot: mushroom/star/1-up, 16x16, two 8x16 sprites. the four the draw pass needs
// are plain ram rather than statics, because blocks_draw itself lives in a bank now (blocks_draw.c)
uint8_t blocks_item_kind;
static uint8_t item_phase;
static uint8_t item_timer;
uint16_t blocks_item_x;
int16_t blocks_item_y;
static int16_t item_origin_y;
static int8_t item_dir;
static int8_t item_dy;
static uint8_t item_accum;
static uint8_t item_grounded;

// the single coin-pop slot: 8x16, one sprite
uint8_t blocks_coin_active;
static uint8_t coin_timer;
uint16_t blocks_coin_x;
int16_t blocks_coin_y;

static uint16_t coins_collected;
static uint8_t items_taken;

// oam writes are cheap but not free, and both slots sit empty for most of a level: parking them
// once on the frame they go away keeps a quiet frame down to no oam traffic at all
uint8_t blocks_item_shown;
uint8_t blocks_coin_shown;

// the engine probes solidity four to six times a frame and streams a column every 16 px, and a
// linear scan of the reaction list on each of those costs more than the frame has left. these row
// tables answer "could anything here be overridden at all" in one indexed load, and only rows that
// actually hold a block ever pay for the scan
static uint8_t row_has_block[LEVEL_ROWS];
static uint8_t row_has_hidden[LEVEL_ROWS];
static uint8_t row_has_coin[LEVEL_ROWS];
// nothing is overridden until something is bumped or collected, which skips the scan outright
uint8_t blocks_override_count;
uint8_t blocks_busy;
uint8_t blocks_solid_edits;
static uint8_t main_solid_edits;
// and once something is, only the handful of cells actually altered are worth walking, not the
// whole reaction list. the same reasoning covers the level's one or two hidden blocks
static uint8_t altered[LEVEL_MAX_BLOCKS];
static uint8_t altered_count;
static uint8_t hidden_block[LEVEL_MAX_BLOCKS];
static uint8_t hidden_count;
static uint8_t coins_taken;

#if kEnemyLab
// the lab's second dispenser. 1-1 places its other mushroom_fire block at the top of a pyramid the
// route planner cannot climb, and a flower resting on a block inside a run of them is unreachable:
// the compiled hidden block is the level's one lone block with open sky all around it
static uint8_t lab;
#endif

// the content the block actually pays out, which the lab rewrites for exactly one entry
static uint8_t content_of(uint8_t index) {
#if kEnemyLab
    if (lab != 0U && level->block_kind[index] == kBlockListHidden) {
        return kContentMushroom;
    }
#endif
    return level->block_content[index];
}

static void publish_busy(void);

static int16_t row_of(int16_t py) {
    return py < 0 ? (int16_t)-1 : (int16_t)(py >> 4);
}

static int16_t col_of(int16_t px) {
    return px < 0 ? (int16_t)-1 : (int16_t)(px >> 4);
}

// the reaction list is 17 entries for 1-1 and lives in bank 0, so this scan costs no bank switch
static int16_t find_block(int16_t column, int16_t row) {
    uint8_t i;

    if (column < 0 || row < 0) {
        return -1;
    }
    for (i = 0; i < level->block_count; ++i) {
        if (level->block_column[i] == (uint16_t)column && level->block_row[i] == (uint8_t)row) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t find_coin(int16_t column, int16_t row) {
    uint8_t i;

    if (column < 0 || row < 0) {
        return -1;
    }
    for (i = 0; i < area_coins; ++i) {
        if (coin_col[i] == (uint8_t)column && coin_row_of[i] == (uint8_t)row) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void start_bump(int16_t column, int16_t row) {
    if (bump_timer != 0U) {
        terrain_restore_block(bump_column, bump_row);
    }
    bump_column = column;
    bump_row = row;
    bump_timer = (uint8_t)kBumpFrames;
    terrain_bump_block(column, row);
}

static void pop_coin(int16_t column, int16_t row) {
    // the 8 px sprite sits centered in the block's 16 px cell and starts one cell above it
    blocks_coin_x = (uint16_t)(((uint16_t)column << 4) + 4U);
    blocks_coin_y = (int16_t)(((int16_t)row << 4) - (int16_t)kBlockPx);
    coin_timer = 0;
    blocks_coin_active = 1;
    ++coins_collected;
    // the hud counters are plain ram, so a coin costs an increment and an add rather than a
    // trampoline into bank 5; hud_frame picks up the change on the same frame
    ++hud_coins;
    hud_score = (uint16_t)(hud_score + kScoreTens(kCoinPoints));
}

static void spawn_item(uint8_t content) {
    if (content == kContentStar) {
        blocks_item_kind = kItemStar;
    } else if (content == kContentOneup) {
        blocks_item_kind = kItemOneup;
    } else {
        // smb picks at dispense time, not at bump time: a mushroom_fire block pays the flower only
        // if mario is already grown when it opens
        blocks_item_kind = blocks_player_big != 0U ? kItemFlower : kItemMushroom;
    }
    item_phase = kItemRising;
    item_timer = 0;
    item_dir = 1;
    item_dy = 0;
    item_accum = 0;
    item_grounded = 0;
}

static void react(uint8_t index, int16_t column, int16_t row) {
    const uint8_t kind = level->block_kind[index];
    const uint8_t content = content_of(index);
    // a revealed hidden block is a used block from the moment it materializes
    uint8_t spend = (kind == kBlockListHidden) ? 1U : 0U;

    if (content == kContentCoin) {
        pop_coin(column, row);
        spend = 1U;
    } else if (content == kContentMulticoin) {
        if (budget[index] != 0U) {
            --budget[index];
            pop_coin(column, row);
        }
        if (budget[index] == 0U) {
            spend = 1U;
        }
    } else if (content != kContentNothing && content != kContentVine) {
        // vine is m8's beanstalk; until then it bounces like an empty block
        blocks_item_x = (uint16_t)((uint16_t)column << 4);
        item_origin_y = (int16_t)((int16_t)row << 4);
        blocks_item_y = item_origin_y;
        spawn_item(content);
        spend = 1U;
    }
    if (spend != 0U) {
        state[index] = kBlockStateSpent;
        altered[altered_count] = index;
        ++altered_count;
        ++blocks_override_count;
        if (kind == kBlockListHidden) {
            // the grid holds sky here, so this override is the only thing making the cell solid
            ++main_solid_edits;
            ++blocks_solid_edits;
        }
    }
    start_bump(column, row);
    publish_busy();
}

void blocks_load_level(void) {
    uint8_t i;

    for (i = 0; i < LEVEL_ROWS; ++i) {
        row_has_block[i] = 0;
        row_has_hidden[i] = 0;
        row_has_coin[i] = 0;
    }
    hidden_count = 0;
    altered_count = 0;
    for (i = 0; i < level->block_count; ++i) {
        state[i] = kBlockStateIdle;
        budget[i] = (uint8_t)kMulticoinBudget;
        row_has_block[level->block_row[i]] = 1;
        if (level->block_kind[i] == kBlockListHidden) {
            row_has_hidden[level->block_row[i]] = 1;
            hidden_block[hidden_count] = i;
            ++hidden_count;
        }
    }
    for (i = 0; i < LEVEL_MAX_COINS; ++i) {
        coin_taken[i] = 0;
    }
    area_coins = 0;
    blocks_override_count = 0;
    blocks_solid_edits = 0;
    main_solid_edits = 0;
    coins_taken = 0;
    coins_collected = 0;
    items_taken = 0;
    blocks_player_big = 0;
}

void blocks_enter_area(uint8_t next_area) {
    uint8_t i;

    // the per-block state arrays are per grid already, so a round trip through a pipe finds the
    // main level's spent blocks exactly as it left them; only the live slots are dropped
    area = next_area;
    if (area == kAreaMain) {
        area_coins = 0;
    } else {
        coin_col = level_sub->coin_column;
        coin_row_of = level_sub->coin_row;
        area_coins = level_sub->coin_count;
        for (i = 0; i < LEVEL_ROWS; ++i) {
            row_has_coin[i] = 0;
        }
        for (i = 0; i < area_coins; ++i) {
            row_has_coin[coin_row_of[i]] = 1;
        }
    }
    // the count is per grid: coins taken in the room must not make the main level's streamer scan
    blocks_override_count = (area == kAreaMain) ? altered_count : coins_taken;
    blocks_solid_edits = (area == kAreaMain) ? main_solid_edits : 0U;
    bump_timer = 0;
    bump_column = 0;
    bump_row = 0;
    blocks_item_kind = kItemNone;
    blocks_coin_active = 0;
    blocks_item_shown = 1;
    blocks_coin_shown = 1;
    publish_busy();
}

uint8_t blocks_kind_override(int16_t column, int16_t row, uint8_t kind) {
    int16_t index;

    if (blocks_override_count == 0U || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return kind;
    }
    if (area == kAreaMain) {
        uint8_t i;
        uint8_t j;

        if (row_has_block[(uint8_t)row] == 0U) {
            return kind;
        }
        for (i = 0; i < altered_count; ++i) {
            j = altered[i];
            if (level->block_column[j] != (uint16_t)column || level->block_row[j] != (uint8_t)row) {
                continue;
            }
            return state[j] == kBlockStateSpent ? kBlockSpent : kBlockEmpty;
        }
        return kind;
    }
    if (row_has_coin[(uint8_t)row] == 0U) {
        return kind;
    }
    index = find_coin(column, row);
    return (index >= 0 && coin_taken[index] != 0U) ? kBlockEmpty : kind;
}

void blocks_apply_column(int16_t column, uint8_t* rows) {
    uint8_t i;

    if (column < 0 || blocks_override_count == 0U) {
        return;
    }
    if (area == kAreaMain) {
        uint8_t j;

        for (i = 0; i < altered_count; ++i) {
            j = altered[i];
            if (level->block_column[j] != (uint16_t)column) {
                continue;
            }
            rows[level->block_row[j]] =
                state[j] == kBlockStateSpent ? (uint8_t)kBlockSpent : (uint8_t)kBlockEmpty;
        }
        return;
    }
    for (i = 0; i < area_coins; ++i) {
        if (coin_taken[i] != 0U && coin_col[i] == (uint8_t)column) {
            rows[coin_row_of[i]] = kBlockEmpty;
        }
    }
}

uint8_t blocks_hidden_at(int16_t column, int16_t row) {
    uint8_t i;
    uint8_t j;

    if (area != kAreaMain || row < 0 || row >= (int16_t)LEVEL_ROWS || row_has_hidden[(uint8_t)row] == 0U) {
        return 0;
    }
    for (i = 0; i < hidden_count; ++i) {
        j = hidden_block[i];
        if (state[j] == kBlockStateIdle && level->block_column[j] == (uint16_t)column &&
            level->block_row[j] == (uint8_t)row) {
            return 1;
        }
    }
    return 0;
}

void blocks_head_bump(int16_t column, int16_t row) {
    int16_t index;

    if (area != kAreaMain) {
        return;
    }
    index = find_block(column, row);
    if (index < 0) {
        // 1-2 stamps some 450 plain bricks straight into the grid, far past what the reaction list
        // holds, so smb's rule for a brick with nothing in it is answered off the grid instead
        if (terrain_kind_at(column, row) != (uint8_t)kBlockBrick) {
            return;
        }
        if (blocks_player_big == 0U) {
            start_bump(column, row);
            publish_busy();
            return;
        }
        hud_score = (uint16_t)(hud_score + kScoreTens(kBrickPoints));
        // straight into the ram grid, which needs no override bookkeeping: a death reloads the
        // grid from rom and the brick is back, exactly as smb leaves it
        terrain_clear_cell(column, row);
        blocks_busy = 1;
        return;
    }
    if (state[index] != kBlockStateIdle) {
        return;
    }
    if (level->block_kind[index] == kBlockListBrick && level->block_content[index] == kContentNothing &&
        blocks_player_big != 0U) {
        // the break path: compiled, but only m7's grown mario ever reaches it
        hud_score = (uint16_t)(hud_score + kScoreTens(kBrickPoints));
        state[index] = kBlockStateGone;
        altered[altered_count] = (uint8_t)index;
        ++altered_count;
        ++blocks_override_count;
        ++main_solid_edits;
        ++blocks_solid_edits;
        terrain_write_block(column, row);
        blocks_busy = 1;
        return;
    }
    react((uint8_t)index, column, row);
}

// the loose item walks at the bible's post-emergence speed, turns at walls and falls off ledges
static void item_walk(void) {
    const int16_t next_x = (int16_t)((int16_t)blocks_item_x + (int16_t)item_dir * kItemWalkPx);
    const int16_t top = row_of(blocks_item_y);
    const int16_t bottom = row_of((int16_t)(blocks_item_y + kPlayerHeightPx - 1));
    const int16_t probe = col_of(item_dir > 0 ? (int16_t)(next_x + kPlayerWidthPx - 1) : next_x);

    if (terrain_solid_at(probe, top) != 0U || terrain_solid_at(probe, bottom) != 0U) {
        item_dir = (int8_t)-item_dir;
        return;
    }
    blocks_item_x = (uint16_t)next_x;
}

static void item_fall(void) {
    const int16_t left = col_of((int16_t)blocks_item_x);
    const int16_t right = col_of((int16_t)(blocks_item_x + kPlayerWidthPx - 1));
    uint16_t sum;
    int16_t row;

    if (item_grounded != 0U) {
        row = row_of((int16_t)(blocks_item_y + kPlayerHeightPx));
        if (terrain_solid_at(left, row) != 0U || terrain_solid_at(right, row) != 0U) {
            return;
        }
        item_grounded = 0;
    }

    sum = (uint16_t)((uint16_t)item_accum + (uint16_t)kItemGravitySubpx);
    item_accum = (uint8_t)sum;
    if (sum > 0xFFU) {
        item_dy = (int8_t)(item_dy + 1);
        if (item_dy > kItemMaxFallPx) {
            item_dy = kItemMaxFallPx;
        }
    }
    blocks_item_y = (int16_t)(blocks_item_y + item_dy);
    if (item_dy < 0) {
        return;
    }
    row = row_of((int16_t)(blocks_item_y + kPlayerHeightPx - 1));
    if (terrain_solid_at(left, row) == 0U && terrain_solid_at(right, row) == 0U) {
        return;
    }
    blocks_item_y = (int16_t)(((int16_t)row << 4) - kPlayerHeightPx);
    item_accum = 0;
    if (blocks_item_kind == kItemStar) {
        // the star is the one item that keeps hopping; the height is ours, not the bible's
        item_dy = kStarBouncePx;
        return;
    }
    item_dy = 0;
    item_grounded = 1;
}

static uint8_t boxes_overlap(uint16_t ax, int16_t ay, uint8_t ah, uint16_t bx, int16_t by, uint8_t width) {
    if (ax + kPlayerWidthPx <= bx || bx + width <= ax) {
        return 0;
    }
    return (ay + ah > by && by + kPlayerHeightPx > ay) ? 1U : 0U;
}

// the kItem* he picked up this frame, or kItemNone
static uint8_t item_update(uint16_t player_px, int16_t player_py, uint8_t player_h, uint16_t cam_x) {
    if (blocks_item_kind == kItemNone) {
        return kItemNone;
    }
    if (item_phase == kItemRising) {
        ++item_timer;
        blocks_item_y = (int16_t)(item_origin_y - (int16_t)(item_timer / kItemRiseFramesPerPx));
        if (item_timer >= (uint8_t)(kItemRisePx * (int16_t)kItemRiseFramesPerPx)) {
            item_phase = kItemLoose;
        }
    } else if (blocks_item_kind != kItemFlower) {
        // roster.json: the flower is the one item that neither slides nor falls once it is out
        item_walk();
        item_fall();
    }

    if (boxes_overlap(player_px, player_py, player_h, blocks_item_x, blocks_item_y, kPlayerWidthPx) != 0U) {
        const uint8_t taken = blocks_item_kind;

        blocks_item_kind = kItemNone;
        ++items_taken;
        return taken;
    }
    // off either side of the view, or out the bottom of the level, and it is gone
    if (blocks_item_y > (int16_t)kLevelHeightPx ||
        (int16_t)blocks_item_x + kPlayerWidthPx + kItemDespawnMarginPx < (int16_t)cam_x ||
        (int16_t)blocks_item_x > (int16_t)(cam_x + kScreenWidthPx + kItemDespawnMarginPx)) {
        blocks_item_kind = kItemNone;
    }
    return kItemNone;
}

static void collect_world_coins(uint16_t player_px, int16_t player_py, uint8_t player_h) {
    uint8_t i;
    uint8_t column;
    uint16_t cx;
    int16_t cy;

    if (area == kAreaMain) {
        return;
    }
    // only the column he is in and its two neighbours can touch him, so the box math is skipped
    // for the rest of the room's coins
    column = (uint8_t)(player_px >> 4);
    for (i = 0; i < area_coins; ++i) {
        if (coin_taken[i] != 0U || coin_col[i] + 1U < column || coin_col[i] > column + 1U) {
            continue;
        }
        cx = (uint16_t)((uint16_t)coin_col[i] << 4);
        cy = (int16_t)((int16_t)coin_row_of[i] << 4);
        if (boxes_overlap(player_px, player_py, player_h, cx, cy, kBlockPx) == 0U) {
            continue;
        }
        coin_taken[i] = 1;
        ++coins_taken;
        ++blocks_override_count;
        ++coins_collected;
        ++hud_coins;
        hud_score = (uint16_t)(hud_score + kScoreTens(kCoinPoints));
        terrain_write_block((int16_t)coin_col[i], (int16_t)coin_row_of[i]);
    }
}

// everything blocks_update and blocks_draw could have to do this frame, worked out once
static void publish_busy(void) {
    blocks_busy = (uint8_t)((bump_timer != 0U || blocks_item_kind != kItemNone || blocks_coin_active != 0U ||
                             blocks_item_shown != 0U || blocks_coin_shown != 0U || area != (uint8_t)kAreaMain)
                                ? 1U
                                : 0U);
}

uint8_t blocks_update(uint16_t player_px, int16_t player_py, uint8_t player_h, uint16_t cam_x) {
    uint8_t taken;

    if (bump_timer != 0U) {
        --bump_timer;
        if (bump_timer == 0U) {
            terrain_restore_block(bump_column, bump_row);
        }
    }
    if (blocks_coin_active != 0U) {
        blocks_coin_y =
            (int16_t)(blocks_coin_y +
                      (coin_timer < (uint8_t)(kCoinPopFrames / 2U) ? -kCoinPopRisePx : kCoinPopRisePx));
        ++coin_timer;
        if (coin_timer >= (uint8_t)kCoinPopFrames) {
            blocks_coin_active = 0;
        }
    }
    taken = item_update(player_px, player_py, player_h, cam_x);
    collect_world_coins(player_px, player_py, player_h);
    publish_busy();
    return taken;
}

uint16_t blocks_coins(void) {
    return coins_collected;
}

uint8_t blocks_items_taken(void) {
    return items_taken;
}

void blocks_set_lab(uint8_t on) {
#if kEnemyLab
    lab = on;
#else
    (void)on;
#endif
}
