#include "assets.h"
#include "panel.h"
#include "piece.h"
#include "pieces.h"
#include "save.h"
#include "sfx.h"
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
// the piece the rng has already picked; the next box always shows exactly this one
static uint8_t next_id;

static char line[16];
// staged a couple of rows a frame, like the well's own flash/collapse repaints
static uint8_t popup[kPopupRows][kScreenCols];
static uint8_t popup_step;

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

// "LABEL n" with leading zeros trimmed; n is a u32 so the score can print all six digits
static void format_value(const char* label, uint32_t value) {
    uint8_t n = 0;
    uint8_t started = 0;
    uint32_t div = 100000UL;
    uint8_t digit;

    while (label[n] != '\0') {
        line[n] = label[n];
        ++n;
    }
    line[n++] = ' ';
    while (div > 0UL) {
        digit = (uint8_t)((value / div) % 10UL);
        if (digit != 0U || started != 0U || div == 1UL) {
            line[n++] = (char)('0' + digit);
            started = 1U;
        }
        div /= 10UL;
    }
    line[n] = '\0';
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
    print_centered(kPromptRow, "SPACE TO START");
    format_value("TOP", save_best());
    print_centered(kBestRow, line);
}

// wipes any leftover well/panel/popup tiles so a game-over return looks as clean as first boot
static void clear_screen(void) {
    uint8_t blank[kScreenCols];
    uint8_t x;
    uint8_t y;

    for (x = 0; x < kScreenCols; ++x) {
        blank[x] = kFontFirstTile;
    }
    for (y = 0; y < kScreenRows; ++y) {
        set_bkg_tiles(0, y, kScreenCols, 1, blank);
    }
}

// from_game wipes leftover tiles from a round; skipped at boot so the rng seed timing holds
static void enter_title(uint8_t from_game) {
    uint8_t y;

    // bcpd/attribute writes are mode-locked on real hardware: do them all with the lcd off
    DISPLAY_OFF;
    if (from_game) {
        clear_screen();
    }
    load_title_palettes();
    // the whole screen starts on the title palette; the border rows then override their two
    for (y = 0; y < kScreenRows; ++y) {
        paint_row_palette(y, kPalTitle);
    }
    draw_border();
    draw_title();
    DISPLAY_ON;
    state = kStateTitle;
}

// seven piece palettes then the chrome; the title's own two slots are overwritten here
static void load_play_palettes(void) {
    set_bkg_palette(0, kPieceCount, kPiecePalettes);
    set_bkg_palette(kPalChrome, 1, kChromePalette);
    set_sprite_palette(0, kPieceCount, kPiecePalettes);
}

static void load_play_tiles(void) {
    uint8_t i;

    set_bkg_data(kWellEmptyTileId, 1, kGridTile);
    set_bkg_data(kBackdropTileId, 1, kBackdropTile);
    set_bkg_data(kWallTileId, 1, kWallTile);
    set_bkg_data(kFlashTileId, 1, kBorderTile);
    // one tile id per piece, all the same art; the attribute palette gives it its colour
    for (i = 0; i < kPieceCount; ++i) {
        set_bkg_data((uint8_t)(kLockTileId + i), 1, kBlockTile);
    }
    set_sprite_data(kPieceSpriteTileId, 1, kBlockTile);
}

// draws over the frozen well; a dark card reusing the chrome palette, not a new tile set
static void popup_line(uint8_t row, const char* text) {
    uint8_t x = (uint8_t)((kScreenCols - text_len(text)) / 2U);
    uint8_t n = 0;

    while (text[n] != '\0') {
        popup[row][x + n] = (uint8_t)(kFontFirstTile + (uint8_t)text[n] - kFontFirstChar);
        ++n;
    }
}

static void build_popup(uint32_t score) {
    uint8_t r;
    uint8_t c;

    for (r = 0; r < kPopupRows; ++r) {
        for (c = 0; c < kScreenCols; ++c) {
            popup[r][c] = kFontFirstTile; // space glyph: index0 of the chrome palette, the card fill
        }
    }
    popup_line(kPopupOverRow, "GAME OVER");
    format_value("SCORE", score);
    popup_line(kPopupScoreRow, line);
    format_value("TOP", save_best());
    popup_line(kPopupTopScoreRow, line);
    popup_line(kPopupPromptRow, "SPACE TO RETRY");
    popup_step = 0;
}

static void stage_popup(void) {
    uint8_t i;
    uint8_t y;

    for (i = 0; i < kPopupRowsPerFrame && popup_step < kPopupRows; ++i) {
        y = (uint8_t)(kPopupTopRow + popup_step);
        set_bkg_tiles(kPopupCol, y, kScreenCols, 1, popup[popup_step]);
        // the card must read as one solid dark plate, not a patchwork of whatever piece was under it
        paint_row_palette(y, kPalChrome);
        ++popup_step;
    }
}

// draws the box for the piece that will spawn after the one currently falling
static void refill_next(void) {
    next_id = pieces_next();
    panel_set_next(next_id);
}

static void spawn_next(void) {
    uint8_t id = next_id;
    refill_next();

    if (piece_spawn_blocked(id)) {
        piece_hide();
        sfx_over();
        save_record(panel_score());
        build_popup(panel_score());
        state = kStateOver;
        return;
    }
    piece_spawn(id);
    piece_draw();
    grav = panel_gravity_frames();
    state = kStatePlay;
}

static void enter_play(uint8_t seed) {
    // bcpd and the whole playfield repaint are mode-locked, so do them with the lcd off
    DISPLAY_OFF;
    load_play_tiles();
    load_play_palettes();
    well_init();
    // the title's own font_color call has already recolored the stock glyphs by the time any
    // start press can reach here, so the panel's copies are always built from the right source.
    // done here rather than at boot so it never perturbs the boot-to-first-press cycle count the
    // rng seed below is timed against
    panel_build_font();
    panel_init();
    pieces_seed(seed);
    SHOW_SPRITES;
    DISPLAY_ON;
    das_dir = 0;
    drop_hold = 0;
    refill_next();
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
    uint8_t cleared;
    uint8_t was_level;

    piece_bake();
    piece_hide();
    sfx_lock();
    drop_hold = 1;
    cleared = well_mark_full();
    if (cleared != 0U) {
        was_level = panel_level();
        panel_add_lines(cleared);
        sfx_clear(cleared);
        if (panel_level() != was_level) {
            sfx_level();
        }
        flash = kFlashFrames;
        state = kStateFlash;
        return;
    }
    spawn_next();
}

static void play_frame(uint8_t keys, uint8_t pressed) {
    uint8_t step = 0;
    uint8_t soft = 0;

    // only a rotation the well actually accepted blips
    if ((pressed & J_A) && piece_rotate(1)) {
        sfx_rotate();
    }
    if ((pressed & J_B) && piece_rotate(-1)) {
        sfx_rotate();
    }
    das_step(keys);

    if ((keys & J_DOWN) == 0U) {
        drop_hold = 0;
    }
    if ((keys & J_DOWN) != 0U && drop_hold == 0U) {
        step = 1;
        soft = 1;
    } else if (--grav == 0U) {
        step = 1;
    }
    if (step != 0U) {
        grav = panel_gravity_frames();
        if (!piece_fall()) {
            lock_piece();
            return;
        }
        if (soft != 0U) {
            panel_add_soft_drop();
        }
    }
    piece_draw();
}

void main(void) {
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    // free running across every state; the start press captures it as the piece seed
    uint8_t frames = 0;

    font_init();
    font_set(font_load(font_ibm));
    set_bkg_data(kBorderTileId, 1, kBorderTile);
    SPRITES_8x8;
    SHOW_BKG;
    save_init();
    sfx_init();
    enter_title(0);

    while (1) {
        vsync();
        ++frames;
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button never autofires
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            // space is the a button on this frontend, and also starts the game
            if (pressed & (J_START | J_A)) {
                enter_play(frames);
            }
            continue;
        }
        if (state == kStateOver) {
            stage_popup();
            if (pressed & (J_START | J_A)) {
                enter_title(1);
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
