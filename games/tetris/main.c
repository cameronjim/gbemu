#include "assets.h"
#include "tetris.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

// one row of vram bank 1 attribute bytes, reused for every palette-tagged row
static uint8_t attr_row[kScreenCols];

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

static void print_centered(uint8_t y, const char* text) {
    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    printf("%s", text);
}

// tags a whole bg row with one cgb palette; vram bank 1 holds the attribute map
static void paint_row_palette(uint8_t y, uint8_t palette) {
    uint8_t x;
    for (x = 0; x < kScreenCols; ++x) {
        attr_row[x] = palette;
    }
    set_bkg_attributes(0, y, kScreenCols, 1, attr_row);
}

// two real cgb palettes: white text on a dark backdrop, and the border's own warm colour
static void load_palettes(void) {
    palette_color_t title[4] = {RGB(2, 2, 6), RGB(10, 10, 18), RGB(20, 20, 27), RGB(31, 31, 31)};
    palette_color_t border[4] = {RGB(2, 2, 6), RGB(12, 4, 2), RGB(24, 10, 2), RGB(31, 20, 4)};
    set_bkg_palette(kPalTitle, 1, title);
    set_bkg_palette(kPalBorder, 1, border);
}

// a solid strip along the top and bottom screen row, tinted with its own palette
static void draw_border(void) {
    uint8_t strip[kScreenCols];
    uint8_t x;
    for (x = 0; x < kScreenCols; ++x) {
        strip[x] = kBorderTileId;
    }
    set_bkg_tiles(0, kBorderTopRow, kScreenCols, 1, strip);
    set_bkg_tiles(0, kBorderBottomRow, kScreenCols, 1, strip);
    paint_row_palette(kBorderTopRow, kPalBorder);
    paint_row_palette(kBorderBottomRow, kPalBorder);
}

static void draw_title(void) {
    font_color(kFontFore, kFontBack);
    print_centered(kTitleRow, "TETRIS");
    print_centered(kPromptRow, "PRESS START");
}

void main(void) {
    uint8_t y;
    uint8_t keys;

    font_init();
    font_set(font_load(font_ibm));
    set_bkg_data(kBorderTileId, 1, kBorderTile);

    // bcpd/attribute writes are mode-locked on real hardware: do them all with the lcd off
    DISPLAY_OFF;
    load_palettes();
    // the whole screen starts on the title palette; the border rows then override their two
    for (y = 0; y < kScreenRows; ++y) {
        paint_row_palette(y, kPalTitle);
    }
    draw_border();
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;

    // no gameplay yet: start is read but does nothing until milestone 23
    while (1) {
        vsync();
        keys = joypad();
        (void)keys;
    }
}
