#include "panel.h"

#include "pieces.h"
#include "tetris.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t score;
static uint32_t drawn_score;
static uint16_t lines;
static uint8_t level;

static void draw_number(uint8_t col, uint8_t row, uint32_t value, uint8_t width) {
    uint8_t tiles[kScoreDigits];
    uint8_t i;

    for (i = width; i > 0U; --i) {
        tiles[i - 1U] = (uint8_t)(kDigitTileId + (uint8_t)(value % 10UL));
        value /= 10UL;
    }
    set_bkg_tiles(col, row, width, 1, tiles);
}

void panel_build_digits(void) {
    uint8_t glyph[kTileBytes];
    uint8_t d;

    for (d = 0; d < kDigitCount; ++d) {
        get_bkg_data((uint8_t)(kFontFirstTile + (uint8_t)('0' + d) - kFontFirstChar), 1, glyph);
        set_bkg_data((uint8_t)(kDigitTileId + d), 1, glyph);
    }
}

void panel_init(void) {
    gotoxy(kPanelCol, kScoreLabelRow);
    printf("SCORE");
    gotoxy(kPanelCol, kLevelLabelRow);
    printf("LEVEL");
    gotoxy(kPanelCol, kLinesLabelRow);
    printf("LINES");
    gotoxy(kPanelCol, kNextLabelRow);
    printf("NEXT");

    score = 0;
    lines = 0;
    level = 0;
    drawn_score = 0;
    draw_number(kPanelCol, kScoreValueRow, 0, kScoreDigits);
    draw_number(kPanelCol, kLevelValueRow, 0, kLevelDigits);
    draw_number(kPanelCol, kLinesValueRow, 0, kLinesDigits);
}

void panel_set_next(uint8_t piece) {
    const uint8_t* shape = pieces_shape(piece, 0);
    uint8_t occupied[kNextBoxRows][kNextBoxCols];
    uint8_t tile_row[kNextBoxCols];
    uint8_t attr_row[kNextBoxCols];
    uint8_t x;
    uint8_t y;
    uint8_t i;

    for (y = 0; y < kNextBoxRows; ++y) {
        for (x = 0; x < kNextBoxCols; ++x) {
            occupied[y][x] = 0;
        }
    }
    for (i = 0; i < kPieceSprites; ++i) {
        occupied[shape[i] >> 4][shape[i] & 0x0FU] = 1;
    }
    for (y = 0; y < kNextBoxRows; ++y) {
        for (x = 0; x < kNextBoxCols; ++x) {
            if (occupied[y][x] != 0U) {
                tile_row[x] = (uint8_t)(kLockTileId + piece);
                attr_row[x] = piece;
            } else {
                tile_row[x] = kBackdropTileId;
                attr_row[x] = kPalChrome;
            }
        }
        set_bkg_tiles(kPanelCol, (uint8_t)(kNextBoxRow + y), kNextBoxCols, 1, tile_row);
        set_bkg_attributes(kPanelCol, (uint8_t)(kNextBoxRow + y), kNextBoxCols, 1, attr_row);
    }
}

// only the score can change every frame (soft drop), so only it needs a write-on-change guard
static void redraw_score(void) {
    if (score == drawn_score) {
        return;
    }
    drawn_score = score;
    draw_number(kPanelCol, kScoreValueRow, score, kScoreDigits);
}

void panel_add_soft_drop(void) {
    if (score < kScoreCap) {
        ++score;
    }
    redraw_score();
}

void panel_add_lines(uint8_t n) {
    static const uint16_t kLineScore[4] = kLineScoreTable;
    uint32_t added = (uint32_t)kLineScore[n - 1U] * (uint32_t)(level + 1U);

    score += added;
    if (score > kScoreCap) {
        score = kScoreCap;
    }
    lines = (uint16_t)(lines + n);
    if (lines > kLinesCap) {
        lines = kLinesCap;
    }
    level = (uint8_t)(lines / 10U);
    if (level > kLevelMax) {
        level = kLevelMax;
    }
    redraw_score();
    draw_number(kPanelCol, kLinesValueRow, lines, kLinesDigits);
    draw_number(kPanelCol, kLevelValueRow, level, kLevelDigits);
}

uint32_t panel_score(void) {
    return score;
}

uint8_t panel_level(void) {
    return level;
}

uint8_t panel_gravity_frames(void) {
    static const uint8_t kGravity[kLevelMax + 1U] = kGravityTable;
    return kGravity[level];
}
