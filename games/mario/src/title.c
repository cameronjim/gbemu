// the title card runs twice in a session and never during play, so it is the cheapest thing bank 0
// could give up: m8a's level table, loader and collision paths needed the room, and the block
// reactions had to come back out of bank 5 because terrain.c probes them twenty times a frame
#pragma bank 5

#include "title.h"

#include "blocks.h"
#include "level.h"
#include "mario.h"
#include "terrain.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

// one bg map row's worth of tile ids or vram bank 1 attribute bytes, reused row by row
static uint8_t map_row[kRingTileCols];

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

// putchar, not printf: the format parser costs about 1.3kb that two fixed strings do not need
static void print_centered(uint8_t y, const char* text) {
    uint8_t i;

    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    for (i = 0; text[i] != '\0'; ++i) {
        putchar(text[i]);
    }
}

// tags a whole bg row with one cgb palette; vram bank 1 holds the attribute map
static void paint_row_palette(uint8_t y, uint8_t palette) {
    uint8_t x;
    for (x = 0; x < kScreenCols; ++x) {
        map_row[x] = palette;
    }
    set_bkg_attributes(0, y, kScreenCols, 1, map_row);
}

// three real cgb palettes: sky backdrop, warm wordmark, green accent
static void load_palettes(void) {
    palette_color_t sky[4] = {RGB(20, 24, 31), RGB(12, 16, 28), RGB(6, 10, 22), RGB(2, 4, 14)};
    palette_color_t wordmark[4] = {RGB(20, 4, 2), RGB(31, 12, 2), RGB(31, 22, 4), RGB(31, 31, 20)};
    palette_color_t accent[4] = {RGB(2, 12, 4), RGB(4, 20, 8), RGB(10, 28, 12), RGB(24, 31, 20)};
    set_bkg_palette(kPalSky, 1, sky);
    set_bkg_palette(kPalWordmark, 1, wordmark);
    set_bkg_palette(kPalAccent, 1, accent);
}

// wipes the whole ring back to blank sky cells; coming back from a level leaves terrain in it
static void clear_map(void) {
    uint8_t y;
    uint8_t x;

    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kTileSky;
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, map_row);
    }
    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kPalSky;
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, map_row);
    }
}

// the whole map is rewritten here, far more vram traffic than a vblank holds, so the lcd is off
void title_show(void) BANKED {
    DISPLAY_OFF;
    HIDE_SPRITES;
    SCX_REG = 0;
    SCY_REG = 0;
    load_palettes();
    clear_map();
    // the wordmark and prompt each tint their own row; every other cell keeps the sky palette
    paint_row_palette(kTitleRow, kPalWordmark);
    paint_row_palette(kPromptRow, kPalAccent);
    font_color(kFontFore, kFontBack);
    // "!" pads the wordmark to an even glyph span so it lands pixel-centered
    print_centered(kTitleRow, "MARIO!");
    print_centered(kPromptRow, "SPACE TO START");
    SHOW_BKG;
    DISPLAY_ON;
}

#if kDebugCamera
// bcpd is mode-locked on real hardware: every palette and attribute write lands with the lcd off
void debug_camera_enter(void) BANKED {
    DISPLAY_OFF;
    HIDE_SPRITES;
    level_select(0);
    blocks_load_level();
    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    SHOW_BKG;
    DISPLAY_ON;
}

void debug_camera_frame(uint8_t keys) BANKED {
    if ((keys & J_RIGHT) != 0U) {
        terrain_scroll_x((int8_t)kCamStepPx);
    } else if ((keys & J_LEFT) != 0U) {
        terrain_scroll_x(-(int8_t)kCamStepPx);
    }
    if ((keys & J_UP) != 0U) {
        terrain_pan_y(-(int8_t)kCamStepPx);
    } else if ((keys & J_DOWN) != 0U) {
        terrain_pan_y((int8_t)kCamStepPx);
    }
    // the only bg writes of the camera state happen here, inside vblank
    terrain_apply_scroll();
}
#endif
