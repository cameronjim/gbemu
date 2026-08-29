#include "terrain.h"

#include "assets.h"
#include "level_1_1.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// camera left edge and top edge, in px; world_x drives scx (mod 256, the ring's own wrap), world_y is scy
static uint16_t world_x;
static uint8_t world_y;
static uint16_t max_world_x;
// leftmost block column currently streamed into the ring; the ring always holds kRingBlocks of them
static int16_t window_start;

// one block-column's worth of tiles (2 tile columns x kBgRows), expanded from the banked level data
static uint8_t tile_buf[kTilesPerBlock * kBgRows];
static uint8_t attr_buf[kTilesPerBlock * kBgRows];

// reads one column of the banked level grid; brackets the bank switch so bank 0 is current otherwise
static void read_block_column(uint16_t block_col, uint8_t* out) {
    uint8_t r;

    SWITCH_ROM_MBC5(LEVEL_1_1_BANK);
    for (r = 0; r < LEVEL_1_1_ROWS; ++r) {
        out[r] = level_1_1_blocks[block_col][r];
    }
    SWITCH_ROM_MBC5(0);
}

// expands one banked block column into its 2x2-per-block tile/attribute pair and writes the ring slot;
// out of level bounds is left as whatever the ring already holds (sky, by the init fill below)
static void stream_column(int16_t block_col) {
    uint8_t rows[LEVEL_1_1_ROWS];
    uint8_t r;
    uint8_t kind;
    uint8_t top;
    uint8_t tile_col;

    if (block_col < 0 || block_col >= (int16_t)LEVEL_1_1_LENGTH_COLUMNS) {
        return;
    }

    read_block_column((uint16_t)block_col, rows);

    for (r = 0; r < LEVEL_1_1_ROWS; ++r) {
        kind = rows[r];
        top = (uint8_t)(r << 1);
        tile_buf[top * kTilesPerBlock] = kBlockTileTl[kind];
        tile_buf[top * kTilesPerBlock + 1U] = kBlockTileTr[kind];
        tile_buf[(top + 1U) * kTilesPerBlock] = kBlockTileBl[kind];
        tile_buf[(top + 1U) * kTilesPerBlock + 1U] = kBlockTileBr[kind];
        attr_buf[top * kTilesPerBlock] = kBlockPalette[kind];
        attr_buf[top * kTilesPerBlock + 1U] = kBlockPalette[kind];
        attr_buf[(top + 1U) * kTilesPerBlock] = kBlockPalette[kind];
        attr_buf[(top + 1U) * kTilesPerBlock + 1U] = kBlockPalette[kind];
    }

    // two calls total: one for the tile ids (vbk 0), one for the palette attributes (vbk 1)
    tile_col = (uint8_t)(((uint16_t)block_col * kTilesPerBlock) & (kRingTileCols - 1U));
    set_bkg_tiles(tile_col, 0, kTilesPerBlock, kBgRows, tile_buf);
    set_bkg_attributes(tile_col, 0, kTilesPerBlock, kBgRows, attr_buf);
}

// the ring holds [window_start, window_start + kRingBlocks); shifts one column at a time either way.
// vblank budget: the ring wraps every kRingBlocks columns, so the column that scrolls in on one edge
// always reuses the slot the column falling off the other edge just vacated - one banked read plus
// two 2x30 vram writes. the play camera moves at most mario's 2.5 px/frame plus the look-ahead
// anchor's 2 px/frame, so a frame never crosses more than one 16 px block boundary and never streams
// more than one column; only terrain_init()'s 16-column fill exceeds a vblank, and it runs lcd-off
static int16_t clamp_window_start(int16_t desired) {
    int16_t max_start = (int16_t)LEVEL_1_1_LENGTH_COLUMNS - (int16_t)kRingBlocks;
    if (max_start < 0) {
        max_start = 0;
    }
    if (desired < 0) {
        desired = 0;
    }
    if (desired > max_start) {
        desired = max_start;
    }
    return desired;
}

static void sync_window(uint16_t cam_block) {
    int16_t target = clamp_window_start((int16_t)cam_block - (int16_t)kWindowLeftMargin);

    // scrolling forward streams the column arriving at the ring's right edge...
    while (window_start < target) {
        ++window_start;
        stream_column((int16_t)(window_start + (int16_t)kRingBlocks - 1));
    }
    // ...and scrolling backward is its mirror: the column arriving at the left edge
    while (window_start > target) {
        --window_start;
        stream_column(window_start);
    }
}

void terrain_init(void) {
    uint16_t level_px;
    int16_t i;

    assets_load_bg_tiles();
    assets_load_bg_palettes();

    world_x = 0;
    world_y = 0;
    window_start = 0;

    level_px = (uint16_t)(LEVEL_1_1_LENGTH_COLUMNS * kBlockPx);
    max_world_x = (level_px > kScreenWidthPx) ? (uint16_t)(level_px - kScreenWidthPx) : 0U;

    for (i = 0; i < (int16_t)kRingBlocks; ++i) {
        stream_column(i);
    }

    terrain_apply_scroll();
}

void terrain_scroll_x(int8_t delta_px) {
    int16_t next = (int16_t)world_x + delta_px;

    if (next < 0) {
        next = 0;
    }
    if (next > (int16_t)max_world_x) {
        next = (int16_t)max_world_x;
    }
    world_x = (uint16_t)next;
    sync_window((uint16_t)(world_x >> 4)); // /kBlockPx
}

void terrain_set_scroll_x(uint16_t world_px) {
    world_x = (world_px > max_world_x) ? max_world_x : world_px;
}

void terrain_stream_window(void) {
    sync_window((uint16_t)(world_x >> 4));
}

void terrain_set_pan_y(uint8_t y_px) {
    world_y = (y_px > (uint8_t)kScyMax) ? (uint8_t)kScyMax : y_px;
}

uint16_t terrain_camera_x(void) {
    return world_x;
}

uint16_t terrain_max_camera_x(void) {
    return max_world_x;
}

uint8_t terrain_solid_at(int16_t column, int16_t row) {
    uint8_t kind;

    if (column < 0 || column >= (int16_t)LEVEL_1_1_LENGTH_COLUMNS) {
        return 1; // the level's ends are walls, smb-style
    }
    if (row < 0 || row >= (int16_t)LEVEL_1_1_ROWS) {
        return 0;
    }
    SWITCH_ROM_MBC5(LEVEL_1_1_BANK);
    kind = level_1_1_blocks[(uint16_t)column][(uint8_t)row];
    SWITCH_ROM_MBC5(0);
    // sky and the flag pole are the only cells the player passes through
    return (kind != kBlockEmpty && kind != kBlockFlagPole) ? 1U : 0U;
}

void terrain_pan_y(int8_t delta_px) {
    int16_t next = (int16_t)world_y + delta_px;

    if (next < 0) {
        next = 0;
    }
    if (next > (int16_t)kScyMax) {
        next = (int16_t)kScyMax;
    }
    world_y = (uint8_t)next;
}

void terrain_apply_scroll(void) {
    SCX_REG = (uint8_t)world_x; // truncation is the ring's own 256px wrap
    SCY_REG = world_y;
}
