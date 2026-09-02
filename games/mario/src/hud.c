// the hud runs once a frame and writes vram, and it is banked for camera.c's reason: one
// trampoline a frame against the bank 0 bytes m8b had nowhere else to find
#pragma bank 5

#include "hud.h"

#include "assets.h"
#include "mario.h"
#include "physics_constants.h"

#include <gb/gb.h>
#include <stdint.h>

uint16_t hud_score;
uint8_t hud_coins;
uint8_t hud_lives;
uint16_t hud_time;

// frames since the last tick, and the lab's short countdown
static uint8_t tick_frames;
static uint8_t short_timer;
// the digits on screen, kept beside the counters so a frame is compares and no division
static uint8_t score_digit[kHudScoreDigits];
static uint8_t coin_digit[kHudCoinDigits];
static uint8_t time_digit[kHudTimeDigits];
static uint8_t last_coins;
static uint16_t last_score;
// what each cell of the strip currently carries; never a digit, so the first frame of a level
// writes all ten
static uint8_t shown[kHudScoreDigits + kHudCoinDigits + kHudTimeDigits];
#define kHudSlotScore 0U
#define kHudSlotCoin kHudScoreDigits
#define kHudSlotTime (kHudScoreDigits + kHudCoinDigits)

// the window's own tile map, which nothing else in this game writes; the level's ring keeps 0x9800
#define kWinMapBase ((uint8_t*)0x9C00U)

// no divide: the counters are two and three digits, so repeated subtraction beats a library call
void hud_split(uint16_t value, uint8_t* out, uint8_t count) {
    uint8_t i = count;

    while (i != 0U) {
        uint16_t rest = value;
        // the carry is the whole rest of the number, not one digit of it: a uint8_t here wrapped
        // every score past 25600, which 1-1's full sixteen goombas plus its time bonus now clears
        uint16_t digit = 0;

        while (rest >= 10U) {
            rest = (uint16_t)(rest - 10U);
            ++digit;
        }
        --i;
        out[i] = (uint8_t)rest;
        value = digit;
    }
}

// straight into the window map rather than through set_win_tiles, for put_face's reason in
// terrain.c: the strip is three rows of the map's first columns, so nothing can wrap off an edge
// and none of that generality has to be paid for
static void put_cell(uint8_t row, uint8_t col, uint8_t tile) {
    kWinMapBase[((uint16_t)row << 5) + col] = tile;
}

// the bar prints nine letters and ten digits and no more, so a character is looked up in the same
// list assets_load_hud_font copied in rather than costing an id per ascii code
static uint8_t glyph(char c) {
    static const char kLetters[] = kHudGlyphChars;
    uint8_t i;

    if (c >= '0' && c <= '9') {
        return (uint8_t)(kTileHudDigitFirst + (uint8_t)c - (uint8_t)'0');
    }
    for (i = 0; kLetters[i] != '\0'; ++i) {
        if (kLetters[i] == c) {
            return (uint8_t)(kTileHudLetterFirst + i);
        }
    }
    return kTileHudBlank;
}

static void put_text(uint8_t row, uint8_t col, const char* text) {
    uint8_t i;

    for (i = 0; text[i] != '\0'; ++i) {
        put_cell(row, (uint8_t)(col + i), glyph(text[i]));
    }
}

static void put_digit(uint8_t slot, uint8_t row, uint8_t col, uint8_t value) {
    if (shown[slot] == value) {
        return;
    }
    shown[slot] = value;
    put_cell(row, col, glyph((char)('0' + value)));
}

// the whole strip, with the lcd off: every cell of all three rows tagged for the one palette and
// blanked to solid black, then the labels that never move again
static void paint_bar(uint8_t level) {
    uint8_t row;
    uint8_t col;
    uint8_t i;

    VBK_REG = VBK_ATTRIBUTES;
    for (row = 0; row < (uint8_t)kHudBarRows; ++row) {
        for (col = 0; col < (uint8_t)kScreenCols; ++col) {
            put_cell(row, col, kHudBarAttr);
        }
    }
    VBK_REG = VBK_TILES;
    for (row = 0; row < (uint8_t)kHudBarRows; ++row) {
        for (col = 0; col < (uint8_t)kScreenCols; ++col) {
            put_cell(row, col, glyph(' '));
        }
    }
    put_text(kHudRowLabel, kHudLabelCol, "MARIO");
    put_cell(kHudRowLabel, kHudCoinIconCol, kTileHudCoin);
    put_text(kHudRowLabel, (uint8_t)(kHudCoinIconCol + 1U), "x");
    put_text(kHudRowLabel, kHudTimeLabelCol, "TIME");
    // world one is all there is, so the label is the level's own number rather than a WORLD line
    // there is no room for; the pause card still carries the spelled-out one
    put_text(kHudRowLabel, kHudWorldCol, "1-");
    put_cell(kHudRowLabel, (uint8_t)(kHudWorldCol + 2U), glyph((char)('1' + level)));
    // smb's score is always a multiple of ten and hud_score counts tens, so the strip prints the
    // zero the counter does not carry, exactly as the cards do
    put_cell(kHudRowValue, (uint8_t)(kHudScoreCol + kHudScoreDigits), glyph('0'));
    for (i = 0; i < (uint8_t)(kHudScoreDigits + kHudCoinDigits + kHudTimeDigits); ++i) {
        shown[i] = 0xFFU;
    }
}

void hud_new_game(void) {
    hud_score = 0;
    hud_coins = 0;
    hud_lives = (uint8_t)kStartLives;
}

void hud_set_short_timer(uint8_t on) {
    short_timer = on;
}

void hud_enter_level(uint16_t ticks, uint8_t level) {
    uint8_t i;

    assets_load_hud_font();
    hud_time = (short_timer != 0U) ? (uint16_t)kShortTimerTicks : ticks;
    if (hud_time > (uint16_t)kTimerMax) {
        hud_time = (uint16_t)kTimerMax;
    }
    tick_frames = 0;
    last_coins = hud_coins;
    last_score = hud_score;
    hud_split(hud_time, time_digit, (uint8_t)kHudTimeDigits);
    hud_split(hud_coins, coin_digit, (uint8_t)kHudCoinDigits);
    hud_split(hud_score, score_digit, (uint8_t)kHudScoreDigits);
    paint_bar(level);
    // m8b's five digit sprites are gone from oam; the slots are parked above the visible area once
    // here so nothing they last drew can linger, and left free (see kSpriteFreeFirst in mario.h)
    for (i = 0; i < (uint8_t)kSpriteFreeCount; ++i) {
        move_sprite((uint8_t)(kSpriteFreeFirst + i), 0, 0);
    }
}

void hud_add_life(void) BANKED {
    if (hud_lives < (uint8_t)kLivesMax) {
        ++hud_lives;
    }
}

// the countdown's own borrow chain, so a tick costs three compares instead of a division
static void tick_down(void) {
    uint8_t i = (uint8_t)kHudTimeDigits;

    while (i != 0U) {
        --i;
        if (time_digit[i] != 0U) {
            --time_digit[i];
            return;
        }
        time_digit[i] = 9;
    }
}

uint8_t hud_frame(void) BANKED {
    uint8_t i;
    uint8_t timeout = 0;

    if (hud_coins != last_coins) {
        // smb rolls the counter over rather than resetting it, and pays a life for the hundred
        if (hud_coins >= (uint8_t)kCoinsPerLife) {
            hud_coins = (uint8_t)(hud_coins - (uint8_t)kCoinsPerLife);
            hud_add_life();
        }
        last_coins = hud_coins;
        hud_split(hud_coins, coin_digit, (uint8_t)kHudCoinDigits);
    }
    if (hud_score != last_score) {
        last_score = hud_score;
        hud_split(hud_score, score_digit, (uint8_t)kHudScoreDigits);
    }
    if (hud_time != 0U) {
        ++tick_frames;
        if (tick_frames >= (uint8_t)kTimerFramesPerTick) {
            tick_frames = 0;
            --hud_time;
            tick_down();
            // roster.json gives no hurry-up behaviour beyond the music, which is m10's, so an
            // expired countdown does the one thing it still does in smb: it kills him
            timeout = (hud_time == 0U) ? 1U : 0U;
        }
    }
    for (i = 0; i < (uint8_t)kHudScoreDigits; ++i) {
        put_digit((uint8_t)(kHudSlotScore + i), kHudRowValue, (uint8_t)(kHudScoreCol + i), score_digit[i]);
    }
    for (i = 0; i < (uint8_t)kHudCoinDigits; ++i) {
        put_digit((uint8_t)(kHudSlotCoin + i), kHudRowLabel, (uint8_t)(kHudCoinCol + i), coin_digit[i]);
    }
    for (i = 0; i < (uint8_t)kHudTimeDigits; ++i) {
        put_digit((uint8_t)(kHudSlotTime + i), kHudRowValue, (uint8_t)(kHudTimeCol + i), time_digit[i]);
    }
    return timeout;
}

uint8_t hud_spend_time_bonus(void) {
    uint8_t n = (uint8_t)kTimeBonusTicksPerFrame;

    while (n != 0U && hud_time != 0U) {
        --hud_time;
        hud_score = (uint16_t)(hud_score + kScoreTens(kTimeBonusPoints));
        --n;
    }
    return (hud_time != 0U) ? 1U : 0U;
}
