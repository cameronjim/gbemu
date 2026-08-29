#include "panel.h"

#include "assets.h"
#include "pieces.h"
#include "tetris.h"

#include <gb/gb.h>
#include <stdint.h>

static uint32_t score;
static uint32_t drawn_score;
static uint16_t lines;
static uint8_t level;

// letters this panel ever prints: score, level, lines, next. order matches the recolored
// glyph block panel_build_font writes starting at kPanelLetterTileId
static const char kPanelLetters[] = "CEILNORSTVX";

static void draw_number(uint8_t col, uint8_t row, uint32_t value, uint8_t width) {
    uint8_t tiles[kScoreDigits];
    uint8_t i;

    for (i = width; i > 0U; --i) {
        tiles[i - 1U] = (uint8_t)(kDigitTileId + (uint8_t)(value % 10UL));
        value /= 10UL;
    }
    set_bkg_tiles(col, row, width, 1, tiles);
}

static uint8_t panel_glyph_tile(char c) {
    uint8_t i;

    for (i = 0; i < kPanelLetterCount; ++i) {
        if (kPanelLetters[i] == c) {
            return (uint8_t)(kPanelLetterTileId + i);
        }
    }
    return kPanelLetterTileId; // unreached: every panel label is drawn from kPanelLetters
}

// "SCORE", "LEVEL", "LINES", "NEXT" through the panel's own glyph tiles (title letterforms
// recolored onto the panel backdrop), not the console/ascii font's own boxed tile range
static void draw_panel_text(uint8_t col, uint8_t row, const char* text) {
    uint8_t tiles[5]; // longest panel label ("SCORE"/"LEVEL"/"LINES") is 5 chars
    uint8_t n = 0;

    while (text[n] != '\0') {
        tiles[n] = panel_glyph_tile(text[n]);
        ++n;
    }
    set_bkg_tiles(col, row, n, 1, tiles);
}

// by the time any start press can reach here the title has already run font_color, so the stock
// ibm font sits in vram as index0 background / index3 ink; swapping every background pixel to
// index1 drops it straight onto the panel backdrop with no boxed cell, while the high plane (the
// ink shape) is untouched so the letterform is pixel-identical to the title's own glyph
static void copy_panel_glyph(uint8_t dst_tile, char c) {
    uint8_t glyph[kTileBytes];
    uint8_t src_tile = (uint8_t)(kFontFirstTile + (uint8_t)c - kFontFirstChar);
    uint8_t row;

    get_bkg_data(src_tile, 1, glyph);
    for (row = 0; row < kTileBytes; row += 2U) {
        glyph[row] = 0xFFU; // low plane byte; high plane (row+1) keeps the ink shape as-is
    }
    set_bkg_data(dst_tile, 1, glyph);
}

void panel_build_font(void) {
    uint8_t i;

    for (i = 0; i < kDigitCount; ++i) {
        copy_panel_glyph((uint8_t)(kDigitTileId + i), (char)('0' + i));
    }
    for (i = 0; i < kPanelLetterCount; ++i) {
        copy_panel_glyph((uint8_t)(kPanelLetterTileId + i), kPanelLetters[i]);
    }

    // the two stock glyphs whose bare stems break the panel's letter rhythm (see assets.c)
    set_bkg_data(panel_glyph_tile('I'), 1, kPanelGlyphI);
    set_bkg_data((uint8_t)(kDigitTileId + 1U), 1, kPanelGlyphOne);
}

void panel_init(void) {
    draw_panel_text(kScoreLabelCol, kScoreLabelRow, "SCORE");
    draw_panel_text(kLevelLabelCol, kLevelLabelRow, "LEVEL");
    draw_panel_text(kLinesLabelCol, kLinesLabelRow, "LINES");
    draw_panel_text(kNextLabelCol, kNextLabelRow, "NEXT");

    score = 0;
    lines = 0;
    level = 0;
    drawn_score = 0;
    draw_number(kScoreValueCol, kScoreValueRow, 0, kScoreDigits);
    draw_number(kLevelValueCol, kLevelValueRow, 0, kLevelDigits);
    draw_number(kLinesValueCol, kLinesValueRow, 0, kLinesDigits);
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
        set_bkg_tiles(kNextBoxCol, (uint8_t)(kNextBoxRow + y), kNextBoxCols, 1, tile_row);
        set_bkg_attributes(kNextBoxCol, (uint8_t)(kNextBoxRow + y), kNextBoxCols, 1, attr_row);
    }
}

// only the score can change every frame (soft drop), so only it needs a write-on-change guard
static void redraw_score(void) {
    if (score == drawn_score) {
        return;
    }
    drawn_score = score;
    draw_number(kScoreValueCol, kScoreValueRow, score, kScoreDigits);
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
    draw_number(kLinesValueCol, kLinesValueRow, lines, kLinesDigits);
    draw_number(kLevelValueCol, kLevelValueRow, level, kLevelDigits);
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
