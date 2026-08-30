// the title card runs twice in a session and never during play, so it is the cheapest thing bank 0
// could give up: m8a's level table, loader and collision paths needed the room, and the block
// reactions had to come back out of bank 5 because terrain.c probes them twenty times a frame
#pragma bank 5

#include "title.h"

#include "blocks.h"
#include "enemies.h"
#include "flow.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "save.h"
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
void card_print_centered(uint8_t y, const char* text) {
    uint8_t i;

    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    for (i = 0; text[i] != '\0'; ++i) {
        putchar(text[i]);
    }
}

// tags `rows` consecutive whole bg rows starting at y0 with one cgb palette; vram bank 1 holds the
// attribute map. every banner uses this so the tinted band is never just the glyph height
void card_paint_band(uint8_t y0, uint8_t rows, uint8_t palette) {
    uint8_t x;
    uint8_t r;
    for (x = 0; x < kScreenCols; ++x) {
        map_row[x] = palette;
    }
    for (r = 0; r < rows; ++r) {
        set_bkg_attributes(0, (uint8_t)(y0 + r), kScreenCols, 1, map_row);
    }
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
void card_clear_map(void) {
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

// one label followed by `digits` decimal digits, the pair centered across the 20 columns together
void card_print_value(uint8_t y, const char* label, uint16_t value, uint8_t digits, uint8_t trailing) {
    uint8_t out[6];
    uint8_t i;
    const uint8_t len = (uint8_t)(text_len(label) + digits + trailing);

    hud_split(value, out, digits);
    gotoxy((uint8_t)((kScreenCols - len) / 2U), y);
    for (i = 0; label[i] != '\0'; ++i) {
        putchar(label[i]);
    }
    for (i = 0; i < digits; ++i) {
        putchar((char)('0' + out[i]));
    }
    // smb's score is always a multiple of ten and hud_score counts tens, so the card prints the
    // zero the counter does not carry
    for (i = 0; i < trailing; ++i) {
        putchar('0');
    }
}

// every card opens the same way: a blank sky map, the wordmark palette banded around its heading
// row - one padding row above the text, one below, so there is banner-colored space around the
// letters instead of a band exactly as tall as they are
void card_begin(uint8_t heading_row) {
    DISPLAY_OFF;
    HIDE_SPRITES;
    SCX_REG = 0;
    SCY_REG = 0;
    load_palettes();
    card_clear_map();
    card_paint_band((uint8_t)(heading_row - 1U), kBannerRows, kPalWordmark);
    font_color(kFontFore, kFontBack);
}

void card_end(void) {
    SHOW_BKG;
    DISPLAY_ON;
}

// the whole map is rewritten here, far more vram traffic than a vblank holds, so the lcd is off
static void title_show(void) {
    card_begin(kTitleRow);
    // "!" pads the wordmark to an even glyph span so it lands pixel-centered
    card_print_centered(kTitleRow, "MARIO!");
    // one line, banded the same way every other banner is: a padding row above and below. m19's
    // file select is what start opens now, so the card no longer carries a menu of its own
    card_paint_band((uint8_t)(kPromptRow - 1U), kBannerRows, kPalAccent);
    card_print_centered(kPromptRow, "SPACE TO START");
    card_end();
}

void title_reset(void) BANKED {
    title_show();
}

// every debug way into a level goes through here, so both labs and the level select arm the same
// run. a player's own way in is the file select and the world map, which do this themselves
static uint8_t start_run(uint8_t* level, uint8_t lab, uint8_t short_timer) {
    // a debug run belongs to no file, so nothing it clears can be recorded over a real save
    save_select(kSaveNoSlot);
    *level = flow_begin_run(*level);
    hud_set_short_timer(short_timer);
    enemies_set_lab(lab);
    blocks_set_lab(lab);
    return kTitlePlay;
}

uint8_t title_frame(uint8_t pressed, uint8_t* level) BANKED {
    // start and a both open the file select: the frontend maps space to a, and space is the
    // advertised start key, so a must never land in a lab
    if ((pressed & (J_START | J_A)) != 0U) {
        return kTitleFile;
    }
    if ((pressed & J_SELECT) != 0U) {
#if kTimerLab
        // select with down held: the timer lab, a countdown short enough to watch run out
        // (down is the modifier because pressing it does nothing a player would mind)
        if ((joypad() & J_DOWN) != 0U) {
            return start_run(level, 0, 1);
        }
#endif
#if kLevelSelect
        // select with up held: straight into the selected level, past the file select and the map.
        // the map locks every node past a file's furthest, so this is the only way a probe can
        // reach 1-4 without clearing the three levels ahead of it; see kLevelSelect in mario.h
        if ((joypad() & J_UP) != 0U) {
            return start_run(level, 0, 0);
        }
#endif
#if kEnemyLab
        // select alone: the same level, seeded with the lab's denser roster and its second
        // dispenser; see kEnemyLab
        return start_run(level, 1, 0);
#endif
    }
#if kLevelSelect
    // step through world one before starting: see kLevelSelect in mario.h
    if ((pressed & J_RIGHT) != 0U) {
        *level = (uint8_t)((*level + 1U) % (uint8_t)kLevelCount);
    } else if ((pressed & J_LEFT) != 0U) {
        *level = (uint8_t)((*level + (uint8_t)kLevelCount - 1U) % (uint8_t)kLevelCount);
    }
#endif
#if kDebugCamera
    if ((pressed & J_B) != 0U) {
        debug_camera_enter();
        return kTitleCamera;
    }
#endif
    return kTitleStay;
}

void card_pause(uint8_t level) BANKED {
    card_begin(kPauseRow);
    card_print_centered(kPauseRow, "PAUSED");
    // systems.md: smbd's small screen moves the lives and the level name onto this card
    card_print_value((uint8_t)(kPauseRow + 2U), "WORLD 1-", (uint16_t)(level + 1U), 1, 0);
    card_print_value((uint8_t)(kPauseRow + 4U), "SCORE ", hud_score, 5, 1);
    card_print_value((uint8_t)(kPauseRow + 6U), "LIVES ", hud_lives, 2, 0);
    card_print_centered((uint8_t)(kPauseRow + 9U), "START RESUMES");
    card_end();
}

void card_game_over(void) {
    card_begin(kTitleRow);
    card_print_centered(kTitleRow, "GAME OVER");
    card_print_value((uint8_t)(kTitleRow + 3U), "SCORE ", hud_score, 5, 1);
    card_end();
}

// the clear card's score line moves every frame while the countdown converts, so the two are split:
// this paints the whole card once with the lcd off
void card_clear(void) {
    card_begin(kTitleRow);
    card_print_centered(kTitleRow, "COURSE CLEAR");
    card_clear_refresh();
    card_end();
}

// ...and this rewrites the two lines that move, which is twenty cells inside one vblank
void card_clear_refresh(void) {
    card_print_value((uint8_t)(kTitleRow + 3U), "TIME ", hud_time, 3, 0);
    card_print_value((uint8_t)(kTitleRow + 5U), "SCORE ", hud_score, 5, 1);
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
