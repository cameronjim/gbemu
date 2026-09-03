#include "well.h"

#include "pieces.h"
#include "tetris.h"

#include <gb/gb.h>
#include <stdint.h>

// 0 is empty, otherwise the piece id plus one
static uint8_t cells[kWellRows][kWellCols];
static uint8_t full[kWellRows];
// cursors for the staged flash and repaint passes
static uint8_t flash_row;
static uint8_t redraw_row;
static uint8_t redraw_last;
// scratch rows so a repaint is two gbdk calls instead of twenty
static uint8_t tile_row[kScreenCols];
static uint8_t attr_row[kScreenCols];

static void draw_row(uint8_t y) {
    uint8_t x;
    uint8_t v;

    for (x = 0; x < kWellCols; ++x) {
        v = cells[y][x];
        tile_row[x] = v != 0U ? (uint8_t)(kLockTileId + v - 1U) : kWellEmptyTileId;
        attr_row[x] = v != 0U ? (uint8_t)(v - 1U) : kPalChrome;
    }
    set_bkg_tiles(kWellOriginCol, (uint8_t)(kWellOriginRow + y), kWellCols, 1, tile_row);
    set_bkg_attributes(kWellOriginCol, (uint8_t)(kWellOriginRow + y), kWellCols, 1, attr_row);
}

static void draw_flash_row(uint8_t y) {
    uint8_t x;
    for (x = 0; x < kWellCols; ++x) {
        tile_row[x] = kFlashTileId;
        attr_row[x] = kPalChrome;
    }
    set_bkg_tiles(kWellOriginCol, (uint8_t)(kWellOriginRow + y), kWellCols, 1, tile_row);
    set_bkg_attributes(kWellOriginCol, (uint8_t)(kWellOriginRow + y), kWellCols, 1, attr_row);
}

static void draw_frame(void) {
    uint8_t x;
    uint8_t y;

    for (x = 0; x < kScreenCols; ++x) {
        tile_row[x] = kBackdropTileId;
        attr_row[x] = kPalChrome;
    }
    tile_row[kWallLeftCol] = kWallTileId;
    tile_row[kWallRightCol] = kWallTileId;
    for (x = 0; x < kWellCols; ++x) {
        tile_row[kWellOriginCol + x] = kWellEmptyTileId;
    }
    for (y = 0; y < kScreenRows; ++y) {
        set_bkg_tiles(0, y, kScreenCols, 1, tile_row);
        set_bkg_attributes(0, y, kScreenCols, 1, attr_row);
    }
}

void well_init(void) {
    uint8_t x;
    uint8_t y;

    for (y = 0; y < kWellRows; ++y) {
        full[y] = 0;
        for (x = 0; x < kWellCols; ++x) {
            cells[y][x] = 0;
        }
    }
    flash_row = kWellRows;
    redraw_row = 1;
    redraw_last = 0;
    draw_frame();
}

uint8_t well_blocked(uint8_t piece, uint8_t rot, int8_t px, int8_t py) {
    const uint8_t* shape = pieces_shape(piece, rot);
    uint8_t i;
    int8_t x;
    int8_t y;

    for (i = 0; i < kPieceSprites; ++i) {
        x = (int8_t)(px + (int8_t)(shape[i] & 0x0FU));
        y = (int8_t)(py + (int8_t)(shape[i] >> 4));
        if (x < 0 || x >= (int8_t)kWellCols || y < 0 || y >= (int8_t)kWellRows) {
            return 1U;
        }
        if (cells[(uint8_t)y][(uint8_t)x] != 0U) {
            return 1U;
        }
    }
    return 0U;
}

void well_lock(uint8_t piece, uint8_t rot, int8_t px, int8_t py) {
    const uint8_t* shape = pieces_shape(piece, rot);
    uint8_t i;
    uint8_t x;
    uint8_t y;
    uint8_t tile = (uint8_t)(kLockTileId + piece);
    uint8_t attr = piece;

    for (i = 0; i < kPieceSprites; ++i) {
        x = (uint8_t)(px + (int8_t)(shape[i] & 0x0FU));
        y = (uint8_t)(py + (int8_t)(shape[i] >> 4));
        cells[y][x] = (uint8_t)(piece + 1U);
        set_bkg_tiles((uint8_t)(kWellOriginCol + x), (uint8_t)(kWellOriginRow + y), 1, 1, &tile);
        set_bkg_attributes((uint8_t)(kWellOriginCol + x), (uint8_t)(kWellOriginRow + y), 1, 1, &attr);
    }
}

uint8_t well_mark_full(void) {
    uint8_t x;
    uint8_t y;
    uint8_t count = 0;

    for (y = 0; y < kWellRows; ++y) {
        full[y] = 1;
        for (x = 0; x < kWellCols; ++x) {
            if (cells[y][x] == 0U) {
                full[y] = 0;
                break;
            }
        }
        if (full[y] != 0U) {
            ++count;
        }
    }
    flash_row = 0;
    return count;
}

uint8_t well_flash_step(void) {
    uint8_t n;

    for (n = 0; n < kFlashRowsPerFrame; ++n) {
        while (flash_row < kWellRows && full[flash_row] == 0U) {
            ++flash_row;
        }
        if (flash_row >= kWellRows) {
            return 1U;
        }
        draw_flash_row(flash_row);
        ++flash_row;
    }
    return 0U;
}

void well_collapse(void) {
    int8_t src = (int8_t)(kWellRows - 1U);
    int8_t dst = (int8_t)(kWellRows - 1U);
    uint8_t x;

    // the lowest flagged row is the deepest one the collapse disturbs
    redraw_last = 0;
    for (x = 0; x < kWellRows; ++x) {
        if (full[x] != 0U) {
            redraw_last = x;
        }
    }

    while (src >= 0) {
        if (full[src] == 0U) {
            if (dst != src) {
                for (x = 0; x < kWellCols; ++x) {
                    cells[dst][x] = cells[src][x];
                }
            }
            --dst;
        }
        --src;
    }
    while (dst >= 0) {
        for (x = 0; x < kWellCols; ++x) {
            cells[dst][x] = 0;
        }
        --dst;
    }
    for (x = 0; x < kWellRows; ++x) {
        full[x] = 0;
    }
    redraw_row = 0;
}

uint8_t well_redraw_step(void) {
    uint8_t n;

    for (n = 0; n < kRedrawRowsPerFrame; ++n) {
        if (redraw_row > redraw_last) {
            return 1U;
        }
        draw_row(redraw_row);
        ++redraw_row;
    }
    return (uint8_t)(redraw_row > redraw_last);
}
