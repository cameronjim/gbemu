// smbd's pause screen, off a capture: a black card in place of the level. PAUSE on the top row,
// WORLD 1-x under it, mario's own sprite beside x and his lives, then a CONTINUE / END menu (smbd's
// SAVE is left out: the file records every clear by itself, so there is nothing for it to write)
// with the cursor on the left. the level's map and palettes are repainted on the way back, the
// same lcd-off rebuild every card pays (states.c leave_card)
#pragma bank 6

#include "pause.h"

#include "hud.h"
#include "mario.h"
#include "terrain.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// the level's own map, parked at scroll 0 so a cell is where it is drawn
#define kBgMapBase ((uint8_t*)0x9800U)

static uint8_t cursor;
static uint8_t row_cells[kRingTileCols];

// the same two glyph lists assets_load_hud_font copies in; hud.c keeps its own copy of this lookup
static uint8_t glyph(char c) {
    static const char kLetters[] = kHudGlyphChars;
    static const char kMore[] = kHudGlyphChars2;
    uint8_t i;

    if (c >= '0' && c <= '9') {
        return (uint8_t)(kTileHudDigitFirst + (uint8_t)c - (uint8_t)'0');
    }
    for (i = 0; kLetters[i] != '\0'; ++i) {
        if (kLetters[i] == c) {
            return (uint8_t)(kTileHudLetterFirst + i);
        }
    }
    for (i = 0; kMore[i] != '\0'; ++i) {
        if (kMore[i] == c) {
            return (uint8_t)(kTileHudLetterSecond + i);
        }
    }
    return kTileHudBlank;
}

// the lcd is on for the cursor moves: pandocs, vram is closed to the cpu in mode 3 only
static void put_cell(uint8_t row, uint8_t col, uint8_t value) {
    while ((STAT_REG & 0x03U) == 0x03U) {
    }
    kBgMapBase[((uint16_t)row << 5) + col] = value;
}

static void put_text(uint8_t row, uint8_t col, const char* text) {
    while (*text != '\0') {
        put_cell(row, col, glyph(*text));
        ++col;
        ++text;
    }
}

// two digits at most (a level number, the lives), so a subtraction loop and no divide
static void put_number(uint8_t row, uint8_t col, uint8_t value) {
    uint8_t tens = 0;

    while (value >= 10U) {
        value = (uint8_t)(value - 10U);
        ++tens;
    }
    if (tens != 0U) {
        put_cell(row, col, (uint8_t)(kTileHudDigitFirst + tens));
        ++col;
    }
    put_cell(row, col, (uint8_t)(kTileHudDigitFirst + value));
}

static const char* item_label(uint8_t item) {
    return (item == 0U) ? "CONTINUE" : "END";
}

static void put_cursor(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kPauseEntries; ++i) {
        put_cell((uint8_t)(kPauseMenuRow + i * kPauseMenuStep), kPauseMenuCol,
                 glyph(i == cursor ? '>' : ' '));
    }
}

// mario's idle pose as four bg cells: the sprite tiles sit at 0x8e00, which a bg id past 0x7f
// reads (lcdc bit 4 clear), in the order draw_row lays an 8x16 pair out - left column, then right
static void put_mario(uint8_t row, uint8_t col) {
    const uint8_t tile = (uint8_t)(kTileMarioFirst + kFrameIdle * kMarioTilesPerFrame);

    VBK_REG = VBK_ATTRIBUTES;
    put_cell(row, col, kPausePalMario);
    put_cell(row, (uint8_t)(col + 1U), kPausePalMario);
    put_cell((uint8_t)(row + 1U), col, kPausePalMario);
    put_cell((uint8_t)(row + 1U), (uint8_t)(col + 1U), kPausePalMario);
    VBK_REG = VBK_TILES;
    put_cell(row, col, tile);
    put_cell((uint8_t)(row + 1U), col, (uint8_t)(tile + 1U));
    put_cell(row, (uint8_t)(col + 1U), (uint8_t)(tile + 2U));
    put_cell((uint8_t)(row + 1U), (uint8_t)(col + 1U), (uint8_t)(tile + 3U));
}

void pause_begin(uint8_t level) BANKED {
    // white ink on black for the text cells, and mario's own colours (assets_load_sprite_palettes:
    // skin, cap red, the dark) for his four cells; every palette comes back with the level
    palette_color_t ink[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(31, 31, 31), RGB(31, 31, 31)};
    palette_color_t mario[4] = {RGB(0, 0, 0), RGB(31, 22, 2), RGB(27, 0, 0), RGB(14, 13, 0)};
    uint8_t y;
    uint8_t x;
    uint8_t i;

    cursor = 0;
    DISPLAY_OFF;
    HIDE_SPRITES;
    // the strip goes with the scroll: the card owns the whole screen
    terrain_park_scroll();
    set_bkg_palette(kCamPalSky, 1, ink);
    set_bkg_palette(kPausePalMario, 1, mario);
    for (x = 0; x < (uint8_t)kRingTileCols; ++x) {
        row_cells[x] = kHudBarAttr;
    }
    for (y = 0; y < (uint8_t)kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, row_cells);
    }
    for (x = 0; x < (uint8_t)kRingTileCols; ++x) {
        row_cells[x] = kTileHudBlank;
    }
    for (y = 0; y < (uint8_t)kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, row_cells);
    }
    put_text(kPauseTitleRow, kPauseTitleCol, "PAUSE");
    put_text(kPauseWorldRow, kPauseWorldCol, "WORLD 1-");
    put_number(kPauseWorldRow, (uint8_t)(kPauseWorldCol + 8U), (uint8_t)(level + 1U));
    put_mario(kPauseMarioRow, kPauseMarioCol);
    put_text(kPauseLivesRow, kPauseLivesXCol, "x");
    put_number(kPauseLivesRow, kPauseLivesCol, hud_lives);
    for (i = 0; i < (uint8_t)kPauseEntries; ++i) {
        put_text((uint8_t)(kPauseMenuRow + i * kPauseMenuStep), (uint8_t)(kPauseMenuCol + 1U), item_label(i));
    }
    put_cursor();
    SHOW_BKG;
    DISPLAY_ON;
}

uint8_t pause_frame(uint8_t pressed) BANKED {
    if ((pressed & J_START) != 0U) {
        return kPauseResume;
    }
    if ((pressed & J_A) != 0U) {
        if (cursor == 0U) {
            return kPauseResume;
        }
        return kPauseQuit;
    }
    if ((pressed & J_UP) != 0U) {
        cursor = (cursor == 0U) ? (uint8_t)(kPauseEntries - 1U) : (uint8_t)(cursor - 1U);
        put_cursor();
    } else if ((pressed & J_DOWN) != 0U) {
        cursor = (cursor == (uint8_t)(kPauseEntries - 1U)) ? 0U : (uint8_t)(cursor + 1U);
        put_cursor();
    }
    return kPauseStay;
}
