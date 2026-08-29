#include "camera.h"
#include "mario.h"
#include "player.h"
#include "terrain.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateClear, kStateCamera };

// one bg map row's worth of tile ids or vram bank 1 attribute bytes, reused row by row
static uint8_t map_row[kRingTileCols];

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
static void enter_title(void) {
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

// scx/scy and oam are cheap and must land before scanline 0; the ring stream can outlast vblank on
// a column boundary, so it always goes last
static void present(void) {
    terrain_set_scroll_x(camera_x());
    terrain_set_pan_y(camera_y());
    terrain_apply_scroll();
    player_draw(camera_x(), camera_y());
    terrain_stream_window();
}

// the level load and the respawn share this: both refill the whole ring, far more vram traffic
// than one vblank holds, so both do it with the lcd off
static void enter_play(void) {
    DISPLAY_OFF;
    terrain_init();
    player_init();
    camera_init(player_x(), player_y());
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

#if kDebugCamera
// bcpd is mode-locked on real hardware: every palette and attribute write lands with the lcd off
static void enter_camera(void) {
    DISPLAY_OFF;
    HIDE_SPRITES;
    terrain_init();
    SHOW_BKG;
    DISPLAY_ON;
}

// the m2 debug camera: no player, no physics, just d-pad scroll/pan over the compiled terrain
static void camera_state_frame(uint8_t keys) {
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

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t status = kPlayerAlive;

    font_init();
    font_set(font_load(font_ibm));
    enter_title();

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button cannot re-enter a state every frame
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            if ((pressed & J_START) != 0U) {
                enter_play();
                state = kStatePlay;
            }
#if kDebugCamera
            else if ((pressed & J_B) != 0U) {
                enter_camera();
                state = kStateCamera;
            }
#endif
            continue;
        }

        if (state == kStatePlay) {
            status = player_update(keys);
            if (status == kPlayerFell) {
                enter_play();
                continue;
            }
            if (status == kPlayerFlag) {
                player_begin_clear();
                state = kStateClear;
            } else {
                camera_update(player_x(), player_y(), player_on_ground(), player_standing(), keys);
            }
            present();
            continue;
        }

        if (state == kStateClear) {
            if (player_clear_update() != 0U) {
                enter_title();
                state = kStateTitle;
                continue;
            }
            // the sequence owns mario, so the camera tracks him as a supported-but-moving actor
            camera_update(player_x(), player_y(), 1, 0, 0);
            present();
            continue;
        }

#if kDebugCamera
        camera_state_frame(keys);
#endif
    }
}
