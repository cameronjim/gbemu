#include "assets.h"
#include "piece.h"
#include "pieces.h"
#include "tetris.h"
#include "well.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateFlash, kStateCollapse, kStateOver };

// one row of vram bank 1 attribute bytes, reused for every palette-tagged row
static uint8_t attr_row[kScreenCols];

static uint8_t state;
static uint8_t grav;
static uint8_t flash;
// 0 none, 1 left, 2 right, with the frames left before the held direction repeats
static uint8_t das_dir;
static uint8_t das_timer;
// a lock while down is held must not soft drop the next piece too
static uint8_t drop_hold;

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
static void load_title_palettes(void) {
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

// seven piece palettes then the chrome; the title's own two slots are overwritten here
static void load_play_palettes(void) {
    set_bkg_palette(0, kPieceCount, kPiecePalettes);
    set_bkg_palette(kPalChrome, 1, kChromePalette);
    set_sprite_palette(0, kPieceCount, kPiecePalettes);
}

static void load_play_tiles(void) {
    uint8_t i;

    set_bkg_data(kWellEmptyTileId, 1, kBlankTile);
    set_bkg_data(kBackdropTileId, 1, kBackdropTile);
    set_bkg_data(kWallTileId, 1, kWallTile);
    set_bkg_data(kFlashTileId, 1, kBorderTile);
    // one tile id per piece, all the same art; the attribute palette gives it its colour
    for (i = 0; i < kPieceCount; ++i) {
        set_bkg_data((uint8_t)(kLockTileId + i), 1, kBlockTile);
    }
    set_sprite_data(kPieceSpriteTileId, 1, kBlockTile);
}

static void spawn_next(void) {
    uint8_t id = pieces_next();

    if (piece_spawn_blocked(id)) {
        piece_hide();
        state = kStateOver;
        return;
    }
    piece_spawn(id);
    piece_draw();
    grav = kGravityFrames;
    state = kStatePlay;
}

static void enter_play(uint8_t seed) {
    // bcpd and the whole playfield repaint are mode-locked, so do them with the lcd off
    DISPLAY_OFF;
    load_play_tiles();
    load_play_palettes();
    well_init();
    SHOW_SPRITES;
    DISPLAY_ON;
    pieces_seed(seed);
    das_dir = 0;
    drop_hold = 0;
    spawn_next();
}

static void das_step(uint8_t keys) {
    uint8_t dir = 0;

    if (keys & J_LEFT) {
        dir = 1;
    } else if (keys & J_RIGHT) {
        dir = 2;
    }
    if (dir == 0U) {
        das_dir = 0;
        return;
    }
    if (dir != das_dir) {
        das_dir = dir;
        das_timer = kDasDelay;
        piece_move(dir == 1U ? -1 : 1);
        return;
    }
    if (das_timer > 0U) {
        --das_timer;
        return;
    }
    das_timer = kDasRepeat;
    piece_move(dir == 1U ? -1 : 1);
}

static void lock_piece(void) {
    piece_bake();
    piece_hide();
    drop_hold = 1;
    if (well_mark_full() != 0U) {
        flash = kFlashFrames;
        state = kStateFlash;
        return;
    }
    spawn_next();
}

static void play_frame(uint8_t keys, uint8_t pressed) {
    uint8_t step = 0;

    if (pressed & J_A) {
        piece_rotate(1);
    }
    if (pressed & J_B) {
        piece_rotate(-1);
    }
    das_step(keys);

    if ((keys & J_DOWN) == 0U) {
        drop_hold = 0;
    }
    if ((keys & J_DOWN) != 0U && drop_hold == 0U) {
        step = 1;
    } else if (--grav == 0U) {
        step = 1;
    }
    if (step != 0U) {
        grav = kGravityFrames;
        if (!piece_fall()) {
            lock_piece();
            return;
        }
    }
    piece_draw();
}

void main(void) {
    uint8_t y;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    // free running across every state; the start press captures it as the piece seed
    uint8_t frames = 0;

    font_init();
    font_set(font_load(font_ibm));
    set_bkg_data(kBorderTileId, 1, kBorderTile);
    SPRITES_8x8;

    // bcpd/attribute writes are mode-locked on real hardware: do them all with the lcd off
    DISPLAY_OFF;
    load_title_palettes();
    // the whole screen starts on the title palette; the border rows then override their two
    for (y = 0; y < kScreenRows; ++y) {
        paint_row_palette(y, kPalTitle);
    }
    draw_border();
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;
    state = kStateTitle;

    while (1) {
        vsync();
        ++frames;
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            if (pressed & J_START) {
                enter_play(frames);
            }
            continue;
        }
        if (state == kStateOver) {
            if (pressed & J_START) {
                enter_play(frames);
            }
            continue;
        }
        if (state == kStateFlash) {
            well_flash_step();
            --flash;
            if (flash == 0U) {
                well_collapse();
                state = kStateCollapse;
            }
            continue;
        }
        if (state == kStateCollapse) {
            if (well_redraw_step()) {
                spawn_next();
            }
            continue;
        }
        play_frame(keys, pressed);
    }
}
