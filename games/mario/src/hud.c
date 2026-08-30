// the hud runs once a frame and touches oam, which is exactly what camera.c already does from
// here: one trampoline a frame against the bank 0 bytes m8b had nowhere else to find
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
static uint8_t coin_digit[kHudCoinDigits];
static uint8_t time_digit[kHudTimeDigits];
static uint8_t last_coins;
// what each slot currently carries; never a digit, so the first draw of a level writes all five
static uint8_t shown[kHudDigits];

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

static void place(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kHudDigits; ++i) {
        const uint8_t x = (uint8_t)(i < (uint8_t)kHudCoinDigits
                                        ? (uint8_t)(kHudCoinX + (int)i * 8)
                                        : (uint8_t)(kHudTimeX + ((int)i - (int)kHudCoinDigits) * 8));

        shown[i] = 0xFFU;
        move_sprite((uint8_t)(kSpriteHudFirst + i), (uint8_t)(x + kOamXOffset),
                    (uint8_t)(kHudRowY + kOamYOffset));
    }
}

static void put(uint8_t slot, uint8_t value, uint8_t palette) {
    if (shown[slot] == value) {
        return;
    }
    shown[slot] = value;
    set_sprite_tile((uint8_t)(kSpriteHudFirst + slot),
                    (uint8_t)(kTileDigitFirst + (uint8_t)(value * kDigitTilesPerGlyph)));
    set_sprite_prop((uint8_t)(kSpriteHudFirst + slot), palette);
}

void hud_new_game(void) {
    hud_score = 0;
    hud_coins = 0;
    hud_lives = (uint8_t)kStartLives;
}

void hud_set_short_timer(uint8_t on) {
    short_timer = on;
}

void hud_enter_level(uint16_t ticks) {
    assets_load_digit_tiles();
    hud_time = (short_timer != 0U) ? (uint16_t)kShortTimerTicks : ticks;
    if (hud_time > (uint16_t)kTimerMax) {
        hud_time = (uint16_t)kTimerMax;
    }
    tick_frames = 0;
    last_coins = hud_coins;
    hud_split(hud_time, time_digit, (uint8_t)kHudTimeDigits);
    hud_split(hud_coins, coin_digit, (uint8_t)kHudCoinDigits);
    place();
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
    for (i = 0; i < (uint8_t)kHudCoinDigits; ++i) {
        put(i, coin_digit[i], (uint8_t)kPalHudCoin);
    }
    for (i = 0; i < (uint8_t)kHudTimeDigits; ++i) {
        put((uint8_t)(kHudCoinDigits + i), time_digit[i], (uint8_t)kPalHudTime);
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
