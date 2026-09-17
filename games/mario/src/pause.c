// the pause overlay lives in the window map under the hud strip: the lyc handler that drops the
// window after the strip is moved down to the band's last line for as long as the menu is up, and
// put back on resume. the level's own map and every actor stay exactly where they froze. the real
// layout is unmeasured (no capture of smbd's pause screen was to be had); what it carries is what
// the sources give: the level name, the lives, and SAVE beside RESUME and QUIT
#pragma bank 6

#include "pause.h"

#include "hud.h"
#include "mario.h"
#include "save.h"

#include <gb/gb.h>
#include <stdint.h>

// the window's own tile map, shared with hud.c's strip on row 0
#define kWinMapBase ((uint8_t*)0x9C00U)

static uint8_t pause_level;
static uint8_t cursor;
static uint8_t entries;

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

// the lcd is on: pandocs, vram is closed to the cpu in mode 3 only
static void put_cell(uint8_t row, uint8_t col, uint8_t value) {
    while ((STAT_REG & 0x03U) == 0x03U) {
    }
    kWinMapBase[((uint16_t)row << 5) + col] = value;
}

static void put_text(uint8_t row, uint8_t col, const char* text) {
    while (*text != '\0') {
        put_cell(row, col, glyph(*text));
        ++col;
        ++text;
    }
}

// two digits at most (a level number, the lives), so a subtraction loop and no divide
static void put_number(uint8_t row, uint8_t col, uint8_t value, uint8_t digits) {
    uint8_t tens = 0;

    while (value >= 10U) {
        value = (uint8_t)(value - 10U);
        ++tens;
    }
    if (digits == 2U) {
        put_cell(row, col, (uint8_t)(kTileHudDigitFirst + tens));
        ++col;
    }
    put_cell(row, col, (uint8_t)(kTileHudDigitFirst + value));
}

static const char* item_label(uint8_t item) {
    if (item == 0U) {
        return "RESUME";
    }
    // a run without an open file (the title's level select, the labs) has nothing to save into
    if (entries == 3U && item == 1U) {
        return "SAVE";
    }
    return "QUIT";
}

static void put_cursor(void) {
    uint8_t i;

    for (i = 0; i < entries; ++i) {
        put_cell((uint8_t)(kPauseMenuRow + i), kPauseMenuCol, glyph(i == cursor ? '>' : ' '));
    }
}

void pause_begin(uint8_t level) BANKED {
    uint8_t row;
    uint8_t col;
    uint8_t i;

    pause_level = level;
    cursor = 0;
    entries = (save_current() == (uint8_t)kSaveNoSlot) ? 2U : 3U;
    // the band's cells wear the strip's own attribute: white ink on the level's sky in every set.
    // one row past the band too: the lyc handler's latency leaks a line of it, as the strip's does
    VBK_REG = VBK_ATTRIBUTES;
    for (row = 1; row <= (uint8_t)(kPauseRows + 1U); ++row) {
        for (col = 0; col < (uint8_t)kScreenCols; ++col) {
            put_cell(row, col, kHudBarAttr);
        }
    }
    VBK_REG = VBK_TILES;
    for (row = 1; row <= (uint8_t)(kPauseRows + 1U); ++row) {
        for (col = 0; col < (uint8_t)kScreenCols; ++col) {
            put_cell(row, col, kTileHudBlank);
        }
    }
    put_text(kPauseInfoRow, kPauseWorldCol, "WORLD 1-");
    put_number(kPauseInfoRow, (uint8_t)(kPauseWorldCol + 8U), (uint8_t)(level + 1U), 1);
    put_text(kPauseInfoRow, kPauseLivesCol, "MARIO x");
    put_number(kPauseInfoRow, (uint8_t)(kPauseLivesCol + 7U), hud_lives, 2);
    for (i = 0; i < entries; ++i) {
        put_text((uint8_t)(kPauseMenuRow + i), (uint8_t)(kPauseMenuCol + 1U), item_label(i));
    }
    put_cursor();
    LYC_REG = (uint8_t)kPauseBandLines;
}

void pause_end(void) BANKED {
    uint8_t row;
    uint8_t col;

    LYC_REG = (uint8_t)kHudBarLines;
    for (row = 1; row <= (uint8_t)(kPauseRows + 1U); ++row) {
        for (col = 0; col < (uint8_t)kScreenCols; ++col) {
            put_cell(row, col, kTileHudBlank);
        }
    }
}

uint8_t pause_frame(uint8_t pressed) BANKED {
    if ((pressed & J_START) != 0U) {
        return kPauseResume;
    }
    if ((pressed & J_A) != 0U) {
        if (cursor == 0U) {
            return kPauseResume;
        }
        if (cursor == (uint8_t)(entries - 1U)) {
            return kPauseQuit;
        }
        // the file already holds the furthest node the map opened, so this records the score
        // against it and says so; the english smbd keeps level progress and drops the score on a
        // reload, which is what the slot keeps too
        save_record(pause_level, hud_score);
        put_text(kPauseMenuRow + 1U, (uint8_t)(kPauseMenuCol + 1U), "SAVED");
        return kPauseStay;
    }
    if ((pressed & J_UP) != 0U) {
        cursor = (cursor == 0U) ? (uint8_t)(entries - 1U) : (uint8_t)(cursor - 1U);
        put_cursor();
    } else if ((pressed & J_DOWN) != 0U) {
        cursor = (cursor == (uint8_t)(entries - 1U)) ? 0U : (uint8_t)(cursor + 1U);
        put_cursor();
    }
    return kPauseStay;
}
