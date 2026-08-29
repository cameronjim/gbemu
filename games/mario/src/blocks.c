#include "blocks.h"

#include "assets.h"
#include "level_1_1.h"
#include "mario.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// the item slot's phases: rising out of the block it came from, then loose in the world
#define kItemRising 0U
#define kItemLoose 1U

// which grid the state below belongs to; the two never react at the same time
static uint8_t area;

// per compiled block, indexed by the generated reaction list
static uint8_t state[LEVEL_1_1_BLOCK_COUNT];
static uint8_t budget[LEVEL_1_1_BLOCK_COUNT];
static uint8_t coin_taken[LEVEL_1_1_AREA0_COIN_COUNT];
static uint8_t player_big;

// only one cell bounces at a time, smb-style
static int16_t bump_column;
static int16_t bump_row;
static uint8_t bump_timer;

// the single item slot: mushroom/star/1-up, 16x16, two 8x16 sprites
static uint8_t item_kind;
static uint8_t item_phase;
static uint8_t item_timer;
static uint16_t item_x;
static int16_t item_y;
static int16_t item_origin_y;
static int8_t item_dir;
static int8_t item_dy;
static uint8_t item_accum;
static uint8_t item_grounded;

// the single coin-pop slot: 8x16, one sprite
static uint8_t coin_active;
static uint8_t coin_timer;
static uint16_t coin_x;
static int16_t coin_y;

static uint16_t coin_count;
static uint8_t items_taken;

// oam writes are cheap but not free, and both slots sit empty for most of a level: parking them
// once on the frame they go away keeps a quiet frame down to no oam traffic at all
static uint8_t item_shown;
static uint8_t coin_shown;

// the engine probes solidity four to six times a frame and streams a column every 16 px, and a
// linear scan of the reaction list on each of those costs more than the frame has left. these row
// tables answer "could anything here be overridden at all" in one indexed load, and only rows that
// actually hold a block ever pay for the scan
static uint8_t row_has_block[LEVEL_1_1_ROWS];
static uint8_t row_has_hidden[LEVEL_1_1_ROWS];
static uint8_t row_has_coin[LEVEL_1_1_ROWS];
// nothing is overridden until something is bumped or collected, which skips the scan outright
uint8_t blocks_override_count;
uint8_t blocks_solid_edits;
static uint8_t main_solid_edits;
// and once something is, only the handful of cells actually altered are worth walking, not the
// whole reaction list. the same reasoning covers the level's one or two hidden blocks
static uint8_t altered[LEVEL_1_1_BLOCK_COUNT];
static uint8_t altered_count;
static uint8_t hidden_block[LEVEL_1_1_BLOCK_COUNT];
static uint8_t hidden_count;
static uint8_t coins_taken;

// kItem* indexes both the art block and the cgb sprite palette
static const uint8_t kItemPalette[4] = {kPalMario, kPalMushroom, kPalStar, kPalOneup};

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
    for (i = 0; i < LEVEL_1_1_BLOCK_COUNT; ++i) {
        if (level_1_1_block_column[i] == (uint16_t)column && level_1_1_block_row[i] == (uint8_t)row) {
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
    for (i = 0; i < LEVEL_1_1_AREA0_COIN_COUNT; ++i) {
        if (level_1_1_area0_coin_column[i] == (uint8_t)column &&
            level_1_1_area0_coin_row[i] == (uint8_t)row) {
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
    coin_x = (uint16_t)(((uint16_t)column << 4) + 4U);
    coin_y = (int16_t)(((int16_t)row << 4) - (int16_t)kBlockPx);
    coin_timer = 0;
    coin_active = 1;
    ++coin_count;
}

static void spawn_item(uint8_t content) {
    if (content == kContentStar) {
        item_kind = kItemStar;
    } else if (content == kContentOneup) {
        item_kind = kItemOneup;
    } else {
        // mushroom_fire while mario is small is the mushroom; m7 gives big mario the flower instead
        item_kind = kItemMushroom;
    }
    item_phase = kItemRising;
    item_timer = 0;
    item_dir = 1;
    item_dy = 0;
    item_accum = 0;
    item_grounded = 0;
}

static void react(uint8_t index, int16_t column, int16_t row) {
    const uint8_t kind = level_1_1_block_kind[index];
    const uint8_t content = level_1_1_block_content[index];
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
        item_x = (uint16_t)((uint16_t)column << 4);
        item_origin_y = (int16_t)((int16_t)row << 4);
        item_y = item_origin_y;
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
}

void blocks_load_level(void) {
    uint8_t i;

    for (i = 0; i < LEVEL_1_1_ROWS; ++i) {
        row_has_block[i] = 0;
        row_has_hidden[i] = 0;
        row_has_coin[i] = 0;
    }
    hidden_count = 0;
    altered_count = 0;
    for (i = 0; i < LEVEL_1_1_BLOCK_COUNT; ++i) {
        state[i] = kBlockStateIdle;
        budget[i] = (uint8_t)kMulticoinBudget;
        row_has_block[level_1_1_block_row[i]] = 1;
        if (level_1_1_block_kind[i] == kBlockListHidden) {
            row_has_hidden[level_1_1_block_row[i]] = 1;
            hidden_block[hidden_count] = i;
            ++hidden_count;
        }
    }
    for (i = 0; i < LEVEL_1_1_AREA0_COIN_COUNT; ++i) {
        coin_taken[i] = 0;
        row_has_coin[level_1_1_area0_coin_row[i]] = 1;
    }
    blocks_override_count = 0;
    blocks_solid_edits = 0;
    main_solid_edits = 0;
    coins_taken = 0;
    coin_count = 0;
    items_taken = 0;
    player_big = 0;
}

void blocks_enter_area(uint8_t next_area) {
    // the per-block state arrays are per grid already, so a round trip through a pipe finds the
    // main level's spent blocks exactly as it left them; only the live slots are dropped
    area = next_area;
    // the count is per grid: coins taken in the room must not make the main level's streamer scan
    blocks_override_count = (area == kAreaMain) ? altered_count : coins_taken;
    blocks_solid_edits = (area == kAreaMain) ? main_solid_edits : 0U;
    bump_timer = 0;
    bump_column = 0;
    bump_row = 0;
    item_kind = kItemNone;
    coin_active = 0;
    item_shown = 1;
    coin_shown = 1;
}

uint8_t blocks_kind_override(int16_t column, int16_t row, uint8_t kind) {
    int16_t index;

    if (blocks_override_count == 0U || row < 0 || row >= (int16_t)LEVEL_1_1_ROWS) {
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
            if (level_1_1_block_column[j] != (uint16_t)column || level_1_1_block_row[j] != (uint8_t)row) {
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
            if (level_1_1_block_column[j] != (uint16_t)column) {
                continue;
            }
            rows[level_1_1_block_row[j]] =
                state[j] == kBlockStateSpent ? (uint8_t)kBlockSpent : (uint8_t)kBlockEmpty;
        }
        return;
    }
    for (i = 0; i < LEVEL_1_1_AREA0_COIN_COUNT; ++i) {
        if (coin_taken[i] != 0U && level_1_1_area0_coin_column[i] == (uint8_t)column) {
            rows[level_1_1_area0_coin_row[i]] = kBlockEmpty;
        }
    }
}

uint8_t blocks_hidden_at(int16_t column, int16_t row) {
    uint8_t i;
    uint8_t j;

    if (area != kAreaMain || row < 0 || row >= (int16_t)LEVEL_1_1_ROWS ||
        row_has_hidden[(uint8_t)row] == 0U) {
        return 0;
    }
    for (i = 0; i < hidden_count; ++i) {
        j = hidden_block[i];
        if (state[j] == kBlockStateIdle && level_1_1_block_column[j] == (uint16_t)column &&
            level_1_1_block_row[j] == (uint8_t)row) {
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
    if (index < 0 || state[index] != kBlockStateIdle) {
        return;
    }
    if (level_1_1_block_kind[index] == kBlockListBrick && level_1_1_block_content[index] == kContentNothing &&
        player_big != 0U) {
        // the break path: compiled, but only m7's grown mario ever reaches it
        state[index] = kBlockStateGone;
        altered[altered_count] = (uint8_t)index;
        ++altered_count;
        ++blocks_override_count;
        ++main_solid_edits;
        ++blocks_solid_edits;
        terrain_write_block(column, row);
        return;
    }
    react((uint8_t)index, column, row);
}

// the loose item walks at the bible's post-emergence speed, turns at walls and falls off ledges
static void item_walk(void) {
    const int16_t next_x = (int16_t)((int16_t)item_x + (int16_t)item_dir * kItemWalkPx);
    const int16_t top = row_of(item_y);
    const int16_t bottom = row_of((int16_t)(item_y + kPlayerHeightPx - 1));
    const int16_t probe = col_of(item_dir > 0 ? (int16_t)(next_x + kPlayerWidthPx - 1) : next_x);

    if (terrain_solid_at(probe, top) != 0U || terrain_solid_at(probe, bottom) != 0U) {
        item_dir = (int8_t)-item_dir;
        return;
    }
    item_x = (uint16_t)next_x;
}

static void item_fall(void) {
    const int16_t left = col_of((int16_t)item_x);
    const int16_t right = col_of((int16_t)(item_x + kPlayerWidthPx - 1));
    uint16_t sum;
    int16_t row;

    if (item_grounded != 0U) {
        row = row_of((int16_t)(item_y + kPlayerHeightPx));
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
    item_y = (int16_t)(item_y + item_dy);
    if (item_dy < 0) {
        return;
    }
    row = row_of((int16_t)(item_y + kPlayerHeightPx - 1));
    if (terrain_solid_at(left, row) == 0U && terrain_solid_at(right, row) == 0U) {
        return;
    }
    item_y = (int16_t)(((int16_t)row << 4) - kPlayerHeightPx);
    item_accum = 0;
    if (item_kind == kItemStar) {
        // the star is the one item that keeps hopping; the height is ours, not the bible's
        item_dy = kStarBouncePx;
        return;
    }
    item_dy = 0;
    item_grounded = 1;
}

static uint8_t boxes_overlap(uint16_t ax, int16_t ay, uint16_t bx, int16_t by, uint8_t width) {
    if (ax + kPlayerWidthPx <= bx || bx + width <= ax) {
        return 0;
    }
    return (ay + kPlayerHeightPx > by && by + kPlayerHeightPx > ay) ? 1U : 0U;
}

static void item_update(uint16_t player_px, int16_t player_py, uint16_t cam_x) {
    if (item_kind == kItemNone) {
        return;
    }
    if (item_phase == kItemRising) {
        ++item_timer;
        item_y = (int16_t)(item_origin_y - (int16_t)(item_timer / kItemRiseFramesPerPx));
        if (item_timer >= (uint8_t)(kItemRisePx * (int16_t)kItemRiseFramesPerPx)) {
            item_phase = kItemLoose;
        }
    } else {
        item_walk();
        item_fall();
    }

    if (boxes_overlap(player_px, player_py, item_x, item_y, kPlayerWidthPx) != 0U) {
        // collection counts the item and drops it; the effects themselves are m7
        item_kind = kItemNone;
        ++items_taken;
        return;
    }
    // off either side of the view, or out the bottom of the level, and it is gone
    if (item_y > (int16_t)kLevelHeightPx ||
        (int16_t)item_x + kPlayerWidthPx + kItemDespawnMarginPx < (int16_t)cam_x ||
        (int16_t)item_x > (int16_t)(cam_x + kScreenWidthPx + kItemDespawnMarginPx)) {
        item_kind = kItemNone;
    }
}

static void collect_world_coins(uint16_t player_px, int16_t player_py) {
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
    for (i = 0; i < LEVEL_1_1_AREA0_COIN_COUNT; ++i) {
        if (coin_taken[i] != 0U || level_1_1_area0_coin_column[i] + 1U < column ||
            level_1_1_area0_coin_column[i] > column + 1U) {
            continue;
        }
        cx = (uint16_t)((uint16_t)level_1_1_area0_coin_column[i] << 4);
        cy = (int16_t)((int16_t)level_1_1_area0_coin_row[i] << 4);
        if (boxes_overlap(player_px, player_py, cx, cy, kBlockPx) == 0U) {
            continue;
        }
        coin_taken[i] = 1;
        ++coins_taken;
        ++blocks_override_count;
        ++coin_count;
        terrain_write_block((int16_t)level_1_1_area0_coin_column[i], (int16_t)level_1_1_area0_coin_row[i]);
    }
}

void blocks_update(uint16_t player_px, int16_t player_py, uint16_t cam_x) {
    if (bump_timer != 0U) {
        --bump_timer;
        if (bump_timer == 0U) {
            terrain_restore_block(bump_column, bump_row);
        }
    }
    if (coin_active != 0U) {
        coin_y = (int16_t)(coin_y +
                           (coin_timer < (uint8_t)(kCoinPopFrames / 2U) ? -kCoinPopRisePx : kCoinPopRisePx));
        ++coin_timer;
        if (coin_timer >= (uint8_t)kCoinPopFrames) {
            coin_active = 0;
        }
    }
    item_update(player_px, player_py, cam_x);
    collect_world_coins(player_px, player_py);
}

static void hide(uint8_t slot) {
    move_sprite(slot, 0, 0);
}

void blocks_draw(uint16_t cam_x, uint8_t cam_y) {
    int16_t sx;
    int16_t sy;
    uint8_t tile;

    if (item_kind == kItemNone) {
        if (item_shown != 0U) {
            item_shown = 0;
            hide(kSpriteItemL);
            hide(kSpriteItemR);
        }
    } else {
        sx = (int16_t)((int16_t)item_x - (int16_t)cam_x);
        sy = (int16_t)(item_y - (int16_t)cam_y);
        if (sy <= -(int16_t)kPlayerHeightPx || sy >= (int16_t)kScreenHeightPx ||
            sx <= -(int16_t)kPlayerWidthPx || sx >= (int16_t)kScreenWidthPx) {
            if (item_shown != 0U) {
                item_shown = 0;
                hide(kSpriteItemL);
                hide(kSpriteItemR);
            }
        } else {
            item_shown = 1;
            tile = (uint8_t)(kTileItemFirst + (uint8_t)((item_kind - 1U) * kItemTilesPerKind));
            set_sprite_tile(kSpriteItemL, tile);
            set_sprite_tile(kSpriteItemR, (uint8_t)(tile + 2U));
            set_sprite_prop(kSpriteItemL, kItemPalette[item_kind]);
            set_sprite_prop(kSpriteItemR, kItemPalette[item_kind]);
            move_sprite(kSpriteItemL, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
            move_sprite(kSpriteItemR, (uint8_t)(sx + 8 + kOamXOffset), (uint8_t)(sy + kOamYOffset));
        }
    }

    if (coin_active == 0U) {
        if (coin_shown != 0U) {
            coin_shown = 0;
            hide(kSpriteCoin);
        }
        return;
    }
    sx = (int16_t)((int16_t)coin_x - (int16_t)cam_x);
    sy = (int16_t)(coin_y - (int16_t)cam_y);
    if (sy <= -(int16_t)kPlayerHeightPx || sy >= (int16_t)kScreenHeightPx || sx <= -8 ||
        sx >= (int16_t)kScreenWidthPx) {
        if (coin_shown != 0U) {
            coin_shown = 0;
            hide(kSpriteCoin);
        }
        return;
    }
    coin_shown = 1;
    set_sprite_tile(kSpriteCoin, kTileCoinPop);
    set_sprite_prop(kSpriteCoin, kPalCoin);
    move_sprite(kSpriteCoin, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}

uint16_t blocks_coins(void) {
    return coin_count;
}

uint8_t blocks_items_taken(void) {
    return items_taken;
}

void blocks_set_player_big(uint8_t big) {
    player_big = big;
}
