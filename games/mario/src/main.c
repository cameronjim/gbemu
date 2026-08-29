#include "mario.h"
#include "player.h"
#include "terrain.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateCamera };

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

// three real cgb palettes: sky backdrop, warm wordmark, green accent
static void load_palettes(void) {
    palette_color_t sky[4] = {RGB(20, 24, 31), RGB(12, 16, 28), RGB(6, 10, 22), RGB(2, 4, 14)};
    palette_color_t wordmark[4] = {RGB(20, 4, 2), RGB(31, 12, 2), RGB(31, 22, 4), RGB(31, 31, 20)};
    palette_color_t accent[4] = {RGB(2, 12, 4), RGB(4, 20, 8), RGB(10, 28, 12), RGB(24, 31, 20)};
    set_bkg_palette(kPalSky, 1, sky);
    set_bkg_palette(kPalWordmark, 1, wordmark);
    set_bkg_palette(kPalAccent, 1, accent);
}

static void draw_title(void) {
    // the wordmark and prompt each tint their own row; every other cell keeps the sky palette
    paint_row_palette(kTitleRow, kPalWordmark);
    paint_row_palette(kPromptRow, kPalAccent);
    font_color(kFontFore, kFontBack);
    // "!" pads the wordmark to an even glyph span so it lands pixel-centered
    print_centered(kTitleRow, "MARIO!");
    print_centered(kPromptRow, "SPACE TO START");
}

// keeps mario at kCamFollowX once he has walked past it; m4 replaces this with smbd's own rules
static uint16_t follow_x(void) {
    const uint16_t mario_x = player_x();

    return mario_x > (uint16_t)kCamFollowX ? (uint16_t)(mario_x - (uint16_t)kCamFollowX) : 0U;
}

// bcpd is mode-locked on real hardware: every palette and attribute write lands with the lcd off
static void enter_camera(void) {
    DISPLAY_OFF;
    terrain_init();
    SHOW_BKG;
    DISPLAY_ON;
}

// the level load and the respawn share this: both refill the whole ring, far more vram traffic
// than one vblank holds, so both do it with the lcd off
static void enter_play(void) {
    DISPLAY_OFF;
    terrain_init();
    terrain_set_pan_y(kPlayScy);
    player_init();
    terrain_set_scroll_x(follow_x());
    terrain_apply_scroll();
    player_draw(terrain_camera_x(), kPlayScy);
    terrain_stream_window();
    SHOW_BKG;
    DISPLAY_ON;
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;

    font_init();
    font_set(font_load(font_ibm));

    DISPLAY_OFF;
    load_palettes();
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding start cannot re-enter the camera every frame
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            if ((pressed & J_START) != 0U) {
                enter_play();
                state = kStatePlay;
            } else if ((pressed & J_SELECT) != 0U) {
                enter_camera();
                state = kStateCamera;
            }
            continue;
        }

        if (state == kStatePlay) {
            if (player_update(keys) != 0U) {
                enter_play();
                continue;
            }
            terrain_set_scroll_x(follow_x());
            // scx and oam are cheap and must land before scanline 0; the ring stream can outlast
            // vblank on a column boundary, so it goes last
            terrain_apply_scroll();
            player_draw(terrain_camera_x(), kPlayScy);
            terrain_stream_window();
            continue;
        }

        // the debug camera: no player, no physics, just d-pad scroll/pan over the compiled terrain
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
}
