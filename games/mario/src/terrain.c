#include "terrain.h"

#include "assets.h"
#include "blocks.h"
#include "flow.h"
#include "level.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// camera left edge and top edge, in px; world_x drives scx (mod 256, the ring's own wrap), world_y is scy
static uint16_t world_x;
static uint8_t world_y;
static uint16_t max_world_x;
// leftmost block column currently streamed into the ring; the ring always holds kRingBlocks of them
static int16_t window_start;

static void read_grid_column(uint16_t block_col, uint8_t* out) {
    // one walking pointer, not level_grid[block_col][r] fifteen times: the column base costs a
    // shift and a 16-bit add, and this runs on the one frame per block that has the least to spare
    const uint8_t* src = level_grid[block_col];
    uint8_t r;

    for (r = 0; r < LEVEL_ROWS; ++r) {
        out[r] = *src++;
    }
}

// the same column with blocks.c's runtime state patched over the compiled bytes
static void read_block_column(uint16_t block_col, uint8_t* out) {
    read_grid_column(block_col, out);
    if (blocks_override_count != 0U) {
        blocks_apply_column((int16_t)block_col, out);
    }
}

static uint8_t grid_cell(uint16_t block_col, uint8_t row) {
    return level_grid[block_col][row];
}

// the kinds each ring slot currently paints. a column scrolling in reuses the slot the column
// kRingBlocks back just left, and across 1-1's long flat stretches those two hold exactly the same
// kinds, so the 2x30 tile and attribute writes are skipped outright and the frame stays in budget
static uint8_t ring_kinds[kRingBlocks][LEVEL_ROWS];

static uint8_t ring_slot(int16_t column) {
    return (uint8_t)((uint16_t)column & (kRingBlocks - 1U));
}

// any direct cell write leaves the slot out of step with its cached kinds. every row is marked
// unknown, not just the first: the streamer now repaints only the rows that differ, so a single
// poisoned row would leave the cell that was written by hand standing
static void ring_forget(int16_t column) {
    uint8_t* cached = ring_kinds[ring_slot(column)];
    uint8_t r;

    for (r = 0; r < LEVEL_ROWS; ++r) {
        cached[r] = 0xFFU;
    }
}

// the ring only holds kRingBlocks columns, so a write outside them would land on some other
// column's slot; those cells are repainted by the streamer when they scroll back in anyway
static uint8_t column_in_ring(int16_t column) {
    return (column >= window_start && column < (int16_t)(window_start + (int16_t)kRingBlocks)) ? 1U : 0U;
}

static uint8_t ring_tile_col(int16_t column) {
    return (uint8_t)(((uint16_t)column * kTilesPerBlock) & (kRingTileCols - 1U));
}

// writes one block cell's 2x2 face at the given tile row; the caller has already range-checked.
//
// straight into the bg map rather than through set_bkg_tiles and set_bkg_attributes. those two
// handle any rectangle up to the full 32x32 with wrapping on both axes, and their setup costs more
// than these eight bytes do - which is the whole difference between the scenery fitting in a frame
// and not. a face never needs any of that generality: ring_tile_col is always even so the pair
// cannot wrap off the right edge of the ring, and the deepest tile row a caller can name is 28, so
// the second row cannot run off the bottom of the map either
#define kBgMapBase ((uint8_t*)0x9800U) // gbdk's default bg map, which nothing in this game moves
static void put_face(int16_t column, uint8_t tile_row, uint8_t kind) {
    uint8_t* const cell = kBgMapBase + ((uint16_t)tile_row << 5) + ring_tile_col(column);
    const uint8_t tl = kBlockTileTl[kind];
    const uint8_t tr = kBlockTileTr[kind];
    const uint8_t bl = kBlockTileBl[kind];
    const uint8_t br = kBlockTileBr[kind];
    const uint8_t pal = kBlockPalette[kind];

    VBK_REG = VBK_TILES;
    cell[0] = tl;
    cell[1] = tr;
    cell[kRingTileCols] = bl;
    cell[kRingTileCols + 1U] = br;
    VBK_REG = VBK_ATTRIBUTES;
    cell[0] = pal;
    cell[1] = pal;
    cell[kRingTileCols] = pal;
    cell[kRingTileCols + 1U] = pal;
    VBK_REG = VBK_TILES;
}

// a whole streamed column is 60 tile bytes plus 60 attribute bytes, far more than the frame that
// streams it can pay: the engine was against its budget before the enemies arrived, and the frame
// that crosses a block boundary is the most expensive one it has - it is already paying to read the
// column and compare it against what the ring slot holds. so that frame pays for nothing else. it
// notes which of the column's fifteen rows actually differ (across a flat stretch, none of them do
// and there is nothing to note) and stops there; the frames that cross no boundary - four or five
// of every six, even at a full run - each paint kOwedCellsPerStep of the noted cells, tiles and
// palette attributes together so no half-painted cell is ever on screen. there is room for that:
// the column being streamed is the ring's own last slot, three blocks past the right edge of the
// view, which is a good twenty frames before anyone can see it
#define kOwedCellsPerStep 2U
static int16_t owed_col = -1;
// the kinds the owed column paints, which of its rows differ from what the slot already held, and
// how far down that list the painting has got
static uint8_t owed_kind[LEVEL_ROWS];
static uint8_t owed_dirty[LEVEL_ROWS];
static uint8_t owed_next;

// the marks are cleared as they are paid, which is what lets the frame that sets them touch only
// the rows that differ: between columns the list is always back to all zeroes
static void owed_advance(void) {
    uint8_t painted = 0;

    while (owed_next < (uint8_t)LEVEL_ROWS && painted < kOwedCellsPerStep) {
        const uint8_t r = owed_next++;

        if (owed_dirty[r] != 0U) {
            owed_dirty[r] = 0;
            put_face(owed_col, (uint8_t)(r * kTilesPerBlock), owed_kind[r]);
            ++painted;
        }
    }
    if (owed_next >= (uint8_t)LEVEL_ROWS) {
        owed_col = -1;
    }
}

static void owed_flush(void) {
    while (owed_col >= 0) {
        owed_advance();
    }
}

// notes one banked block column against the ring slot it will be painted into; out of level bounds
// is left as whatever the ring already holds (sky, by the init fill below)
static void stream_column(int16_t block_col) {
    uint8_t r;
    uint8_t any = 0;
    uint8_t* cached;

    if (block_col < 0 || block_col >= (int16_t)level_columns) {
        return;
    }

    // the owed list is about to be rebuilt, so whatever the last column still owes goes out first.
    // at any camera speed the game can reach there are more idle frames per block than an ordinary
    // column has cells to paint, so this hardly ever has anything left to pay
    owed_flush();

    // straight into the owed list rather than through a local: the kinds this reads are the ones
    // the painting steps will want, and a fifteen byte copy is fifteen bytes this frame does not have
    read_block_column((uint16_t)block_col, owed_kind);

    cached = ring_kinds[ring_slot(block_col)];
    for (r = 0; r < LEVEL_ROWS; ++r) {
        if (cached[r] != owed_kind[r]) {
            cached[r] = owed_kind[r];
            owed_dirty[r] = 1U;
            any = 1U;
        }
    }
    if (any == 0U) {
        return; // a flat stretch: this column paints exactly what the slot already holds
    }

    // and the painting itself waits for a frame that is not also crossing a block boundary
    owed_col = block_col;
    owed_next = 0;
}

// the ring holds [window_start, window_start + kRingBlocks); shifts one column at a time either way.
// vblank budget: the ring wraps every kRingBlocks columns, so the column that scrolls in on one edge
// always reuses the slot the column falling off the other edge just vacated - one banked read plus
// two 2x30 vram writes. the play camera moves at most mario's 2.5 px/frame plus the look-ahead
// anchor's 2 px/frame, so a frame never crosses more than one 16 px block boundary and never streams
// more than one column; only terrain_init()'s 16-column fill exceeds a vblank, and it runs lcd-off
static int16_t clamp_window_start(int16_t desired) {
    int16_t max_start = (int16_t)level_columns - (int16_t)kRingBlocks;
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

void terrain_init(uint8_t next_area) {
    uint16_t level_px;
    int16_t i;

    level_load(next_area);
    // every row, not just the first: the streamer repaints only the rows that differ from the
    // cache, and m8b's cards leave the map holding text a stale cache would happily keep
    for (i = 0; i < (int16_t)kRingBlocks; ++i) {
        ring_forget(i);
    }
    assets_load_block_tables();
    assets_load_bg_tiles();
    // the scenery lives in vram bank 1 and a card never touches it, but a fresh cart, a pipe and
    // a respawn all arrive here with the lcd off and nothing loaded, so it is reloaded beside the
    // terrain rather than tracked
    assets_load_scenery_tiles();

    world_x = 0;
    world_y = 0;
    window_start = 0;
    // column 0 of whatever just loaded - correct for the main grid's own opening segment and for
    // every sub-area, which level_palette_set() always calls underground regardless of column.
    // lives in flow.c/bank5 (banked call from here), not this file - bank0 has no room to spare
    terrain_sync_palette();

    level_px = (uint16_t)(level_columns * kBlockPx);
    max_world_x = (level_px > kScreenWidthPx) ? (uint16_t)(level_px - kScreenWidthPx) : 0U;

    for (i = 0; i < (int16_t)kRingBlocks; ++i) {
        stream_column(i);
    }
    // the init fill runs with the lcd off, so it owes nothing by the time play starts
    owed_flush();

    terrain_apply_scroll();
    // the lcd is off here and the first vblank is a whole frame away, so the camera goes straight
    // into the registers as well: without it the level's opening frame renders on the last card's
    // scroll
    terrain_commit_scroll();
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
    const int16_t was_col = owed_col;
    const uint8_t was_next = owed_next;

    sync_window((uint16_t)(world_x >> 4));
    // a frame that started no column is the quiet one that can pay a step of what is still owed
    if (owed_col >= 0 && owed_col == was_col && owed_next == was_next) {
        owed_advance();
    }
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

uint8_t terrain_kind_at(int16_t column, int16_t row) {
    uint8_t kind;

    if (column < 0 || column >= (int16_t)level_columns || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return kBlockEmpty;
    }
    kind = grid_cell((uint16_t)column, (uint8_t)row);
    return blocks_override_count == 0U ? kind : blocks_kind_override(column, row, kind);
}

// kFloorSolid, kFloorThin, or 0. one indexed load per probe, which is what keeps the six-way
// kind test m8a's new blocks would otherwise need off a path walked twenty times a frame
uint8_t terrain_floor_at(int16_t column, int16_t row) {
    uint8_t kind;

    if (column < 0 || column >= (int16_t)level_columns) {
        return kFloorSolid; // the level's ends are walls, smb-style
    }
    if (row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return 0;
    }
    kind = grid_cell((uint16_t)column, (uint8_t)row);
    // only a materialized hidden block or a broken brick can change the answer
    if (blocks_solid_edits != 0U) {
        kind = blocks_kind_override(column, row, kind);
    }
    return kBlockFloor[kind];
}

// the same probe again rather than a wrapper around terrain_floor_at: this one is called about
// twenty times a frame and the engine has no room for the extra call level
uint8_t terrain_solid_at(int16_t column, int16_t row) {
    uint8_t kind;

    if (column < 0 || column >= (int16_t)level_columns) {
        return 1;
    }
    if (row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return 0;
    }
    kind = grid_cell((uint16_t)column, (uint8_t)row);
    if (blocks_solid_edits != 0U) {
        kind = blocks_kind_override(column, row, kind);
    }
    return (uint8_t)(kBlockFloor[kind] & kFloorSolid);
}

// a direct cell write: the kind goes into the ram grid, and if the ring still holds that column its
// face is repainted too. the flag pennant coming down the pole moves one cell a step this way
void terrain_set_cell(int16_t column, int16_t row, uint8_t kind) {
    if (column < 0 || column >= (int16_t)level_columns || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return;
    }
    level_grid[column][row] = kind;
    terrain_write_block(column, row);
}

// the bridge the axe drops: the cells go to sky, and anything standing on them is now standing on
// the death plane
void terrain_clear_cell(int16_t column, int16_t row) {
    terrain_set_cell(column, row, (uint8_t)kBlockEmpty);
}

void terrain_write_block(int16_t column, int16_t row) {
    if (column_in_ring(column) == 0U || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return;
    }
    // a whole-column repaint would paint over the face this is about to place
    owed_flush();
    ring_forget(column);
    put_face(column, (uint8_t)((uint8_t)row * kTilesPerBlock), terrain_kind_at(column, row));
}

void terrain_bump_block(int16_t column, int16_t row) {
    uint8_t sky[4];
    uint8_t pal[4];
    uint8_t i;

    // the bounce borrows the tile row above the cell, so it only runs where that row is open sky
    if (row < 1 || column_in_ring(column) == 0U ||
        terrain_kind_at(column, (int16_t)(row - 1)) != kBlockEmpty) {
        return;
    }
    owed_flush();
    ring_forget(column);
    put_face(column, (uint8_t)((uint8_t)row * kTilesPerBlock - 1U), terrain_kind_at(column, row));
    for (i = 0; i < 4U; ++i) {
        sky[i] = kBlockTileTl[kBlockEmpty];
        pal[i] = kBlockPalette[kBlockEmpty];
    }
    // the risen block leaves its own bottom tile row showing the backdrop behind it
    set_bkg_tiles(ring_tile_col(column), (uint8_t)((uint8_t)row * kTilesPerBlock + 1U), kTilesPerBlock, 1,
                  sky);
    set_bkg_attributes(ring_tile_col(column), (uint8_t)((uint8_t)row * kTilesPerBlock + 1U), kTilesPerBlock,
                       1, pal);
}

void terrain_restore_block(int16_t column, int16_t row) {
    terrain_write_block(column, (int16_t)(row - 1));
    terrain_write_block(column, row);
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

// what the next vblank will put in scx/scy, and whether the hud strip is one of this frame's
// layers. present() runs while the ppu is drawing - the logic above it has already eaten most of
// the frame - so writing the scroll registers there tore the picture in half: the pan docs have
// the ppu sampling scx per scanline, so the lines already fetched kept the old camera and the rest
// took the new one, which is exactly the sideways shift a moving block showed. the isr below is
// the only writer now, so the scroll lands with the oam gbdk dma's at the same vblank
static uint8_t shadow_scx;
static uint8_t shadow_scy;
uint8_t terrain_bar_on;

static void commit_scroll(void) {
    SCX_REG = shadow_scx;
    SCY_REG = shadow_scy;
}

// both handlers run every frame, so both are three instructions: the vbl one lands the camera and
// raises the readout row, the lyc one drops it again after its 8 px
static void scroll_vbl(void) {
    commit_scroll();
    if (terrain_bar_on != 0U) {
        SHOW_WIN;
    }
}

static void bar_lcd(void) {
    HIDE_WIN;
}

void terrain_install_isrs(void) {
    WX_REG = 7; // the window's own -7 offset: 7 puts its left edge at screen x 0
    WY_REG = 0;
    LCDC_REG |= LCDCF_WIN9C00; // the level's ring keeps gbdk's 0x9800 map
    add_VBL(scroll_vbl);
    add_LCD(bar_lcd);
    // gbdk's default chain terminator spins until the ppu reaches mode 0 or 1 before returning,
    // which from a scanline-8 interrupt is most of a scanline of cpu the engine's own frame wants.
    // this handler writes one register and touches neither vram nor oam, so it has nothing to wait
    // for; nowait_int_handler has to be added last to replace the terminator
    add_LCD(nowait_int_handler);
    STAT_REG = STATF_LYC;
    LYC_REG = (uint8_t)kHudBarLines;
    set_interrupts((uint8_t)(VBL_IFLAG | LCD_IFLAG));
}

void terrain_apply_scroll(void) {
    shadow_scx = (uint8_t)world_x; // truncation is the ring's own 256px wrap
    shadow_scy = world_y;
}

void terrain_commit_scroll(void) {
    commit_scroll();
}

void terrain_park_scroll(void) {
    shadow_scx = 0;
    shadow_scy = 0;
    commit_scroll();
    // every caller is a card painting the 0x9800 map with the lcd off, and the strip is not theirs
    terrain_bar_on = 0;
    HIDE_WIN;
}
