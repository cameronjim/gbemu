#include "terrain.h"

#include "assets.h"
#include "blocks.h"
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
// one block-column's worth of tiles (2 tile columns x kBgRows), expanded from the banked level data
static uint8_t tile_buf[kTilesPerBlock * kBgRows];
static uint8_t attr_buf[kTilesPerBlock * kBgRows];

static void read_grid_column(uint16_t block_col, uint8_t* out) {
    uint8_t r;

    for (r = 0; r < LEVEL_ROWS; ++r) {
        out[r] = level_grid[block_col][r];
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

// a whole streamed column is 60 tile bytes plus 60 attribute bytes, which is more than the frame
// that streams it can pay: the engine was against its budget before the enemies arrived. two things
// bring it down. the streamer repaints only the span of rows that actually differ from what the
// ring slot already holds - across 1-1's flat stretches that is nothing at all and most of the rest
// is a block or two - and whatever is left goes out a slice at a time, one slice a frame. the
// column being streamed is the ring's own last slot, two blocks past the right edge of the view,
// so the whole debt has to be paid inside the four or five frames it takes him to run those 32 px:
// spreading it wider let a column's palette attributes arrive after its tiles were already on screen
#define kOwedSlices 2U
#define kOwedSteps (2U * kOwedSlices)
static int16_t owed_col = -1;
static uint8_t owed_step;
// the changed span, in bg tile rows: where it starts, how many rows it covers, how many of those
// the current half has already written, and how many go out per frame
static uint8_t owed_row;
static uint8_t owed_rows;
static uint8_t owed_done;
static uint8_t owed_slice;

static void owed_advance(void) {
    uint8_t rows;

    if (owed_step == kOwedSlices) {
        owed_done = 0; // the tile half is finished; the attribute half starts over
    }
    rows = (uint8_t)(owed_rows - owed_done);
    if (rows > owed_slice) {
        rows = owed_slice;
    }
    if (rows != 0U) {
        const uint8_t row = (uint8_t)(owed_row + owed_done);
        const uint16_t offset = (uint16_t)owed_done * kTilesPerBlock;

        if (owed_step < kOwedSlices) {
            set_bkg_tiles(ring_tile_col(owed_col), row, kTilesPerBlock, rows, tile_buf + offset);
        } else {
            set_bkg_attributes(ring_tile_col(owed_col), row, kTilesPerBlock, rows, attr_buf + offset);
        }
        owed_done = (uint8_t)(owed_done + rows);
    }
    ++owed_step;
    if (owed_step >= kOwedSteps) {
        owed_col = -1;
    }
}

static void flush_attrs(void) {
    while (owed_col >= 0) {
        owed_advance();
    }
}

// writes one block cell's 2x2 face at the given tile row; the caller has already range-checked
static void put_face(int16_t column, uint8_t tile_row, uint8_t kind) {
    uint8_t tiles[4];
    uint8_t attrs[4];

    tiles[0] = kBlockTileTl[kind];
    tiles[1] = kBlockTileTr[kind];
    tiles[2] = kBlockTileBl[kind];
    tiles[3] = kBlockTileBr[kind];
    attrs[0] = kBlockPalette[kind];
    attrs[1] = attrs[0];
    attrs[2] = attrs[0];
    attrs[3] = attrs[0];
    set_bkg_tiles(ring_tile_col(column), tile_row, kTilesPerBlock, kTilesPerBlock, tiles);
    set_bkg_attributes(ring_tile_col(column), tile_row, kTilesPerBlock, kTilesPerBlock, attrs);
}

// expands one banked block column into its 2x2-per-block tile/attribute pair and writes the ring slot;
// out of level bounds is left as whatever the ring already holds (sky, by the init fill below)
static void stream_column(int16_t block_col) {
    uint8_t rows[LEVEL_ROWS];
    uint8_t r;
    uint8_t kind;
    uint8_t pal;
    uint8_t lo = 0xFFU;
    uint8_t hi = 0;
    uint8_t* cached;
    uint8_t* tp;
    uint8_t* ap;

    if (block_col < 0 || block_col >= (int16_t)level_columns) {
        return;
    }

    read_block_column((uint16_t)block_col, rows);

    cached = ring_kinds[ring_slot(block_col)];
    for (r = 0; r < LEVEL_ROWS; ++r) {
        if (cached[r] != rows[r]) {
            if (lo == 0xFFU) {
                lo = r;
            }
            hi = r;
        }
    }
    if (lo == 0xFFU) {
        return;
    }
    // the buffers are about to be rebuilt, so whatever they still owe goes out first
    flush_attrs();

    // the two buffers fill in exactly the order the ring wants them, so one walking pointer each
    // beats recomputing a row index eight times a row
    tp = tile_buf;
    ap = attr_buf;
    for (r = lo; r <= hi; ++r) {
        kind = rows[r];
        pal = kBlockPalette[kind];
        cached[r] = kind;
        *tp++ = kBlockTileTl[kind];
        *tp++ = kBlockTileTr[kind];
        *tp++ = kBlockTileBl[kind];
        *tp++ = kBlockTileBr[kind];
        *ap++ = pal;
        *ap++ = pal;
        *ap++ = pal;
        *ap++ = pal;
    }

    // this frame pays only the first slice; the next quiet ones pay the rest
    owed_col = block_col;
    owed_row = (uint8_t)(lo * kTilesPerBlock);
    owed_rows = (uint8_t)((uint8_t)(hi - lo + 1U) * kTilesPerBlock);
    owed_done = 0;
    owed_slice = (uint8_t)((owed_rows + (kOwedSlices - 1U)) / kOwedSlices);
    owed_step = 0;
    owed_advance();
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
    uint8_t set_palettes;

    level_load(next_area);
    // every row, not just the first: the streamer repaints only the rows that differ from the
    // cache, and m8b's cards leave the map holding text a stale cache would happily keep
    for (i = 0; i < (int16_t)kRingBlocks; ++i) {
        ring_forget(i);
    }
    assets_load_bg_tiles();
    set_palettes = level_palette_set();
    if (set_palettes == (uint8_t)kLevelTypeUnderground) {
        assets_load_bg_palettes_underground();
    } else if (set_palettes == (uint8_t)kLevelTypeCastle) {
        assets_load_bg_palettes_castle();
    } else {
        assets_load_bg_palettes();
    }

    world_x = 0;
    world_y = 0;
    window_start = 0;

    level_px = (uint16_t)(level_columns * kBlockPx);
    max_world_x = (level_px > kScreenWidthPx) ? (uint16_t)(level_px - kScreenWidthPx) : 0U;

    for (i = 0; i < (int16_t)kRingBlocks; ++i) {
        stream_column(i);
    }
    // the init fill runs with the lcd off, so it owes nothing by the time play starts
    flush_attrs();

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
    const uint8_t before = owed_step;

    sync_window((uint16_t)(world_x >> 4));
    // a frame that started no column is the quiet one that can pay a step of what is still owed
    if (owed_col >= 0 && owed_step == before) {
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

// the bridge the axe drops: the cells go to sky in the ram grid, and whichever of them the ring
// still holds is repainted. anything standing on them is now standing on the death plane
void terrain_clear_cell(int16_t column, int16_t row) {
    if (column < 0 || column >= (int16_t)level_columns || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return;
    }
    level_grid[column][row] = kBlockEmpty;
    terrain_write_block(column, row);
}

void terrain_write_block(int16_t column, int16_t row) {
    if (column_in_ring(column) == 0U || row < 0 || row >= (int16_t)LEVEL_ROWS) {
        return;
    }
    // a whole-column attribute write would paint over the face this is about to place
    flush_attrs();
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
    flush_attrs();
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

void terrain_apply_scroll(void) {
    SCX_REG = (uint8_t)world_x; // truncation is the ring's own 256px wrap
    SCY_REG = world_y;
}
