// smb's two bits of throwaway motion: the four fragments a broken brick throws out and the puff a
// fireball leaves on a wall. neither sits on a collision path and both are pure arithmetic over the
// five oam slots the hud left free, so they ride in bank 6 with the other draw passes and bank 0
// carries none of it - the same trade blocks_draw.c already makes
#pragma bank 6

#include "debris.h"

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

uint8_t debris_busy;
uint8_t debris_timer;
uint8_t debris_puff;

// the four quarters, thrown the way smb throws them: the upper pair fast and wide, the lower pair
// slower and shallower, one of each to a side. every fragment shares one gravity accumulator, so
// the whole set costs a single carry a frame rather than four
static const int8_t kFragDx[kDebrisCount] = {-2, 2, -1, 1};
static const int8_t kFragDy[kDebrisCount] = {-5, -5, -3, -3};
// and where in the 16x16 cell each of them starts, which is the quarter it was
static const uint8_t kFragOx[kDebrisCount] = {0, 8, 0, 8};
static const uint8_t kFragOy[kDebrisCount] = {0, 0, 8, 8};
// the spin: one drawn quarter-brick through all four flip orientations reads as a tumbling chip
static const uint8_t kFragSpin[4] = {0U, (uint8_t)S_FLIPX, (uint8_t)(S_FLIPX | S_FLIPY), (uint8_t)S_FLIPY};

static int16_t frag_x[kDebrisCount];
static int16_t frag_y[kDebrisCount];
static uint8_t accum;
static int8_t gained;
// one bit per fragment slot, so a fragment already off the view writes no oam at all
static uint8_t frag_shown;
static uint8_t puff_shown;
static uint16_t puff_x;
static int16_t puff_y;

// oam y 0 parks a sprite entirely above the screen, the same trick blocks_draw uses
static void hide(uint8_t slot) {
    move_sprite(slot, 0, 0);
}

static void publish(void) {
    debris_busy =
        (uint8_t)((debris_timer != 0U || debris_puff != 0U || frag_shown != 0U || puff_shown != 0U) ? 1U
                                                                                                    : 0U);
}

void debris_break(uint16_t px, int16_t py) BANKED {
    uint8_t i;

    for (i = 0; i < (uint8_t)kDebrisCount; ++i) {
        frag_x[i] = (int16_t)((int16_t)px + (int16_t)kFragOx[i]);
        frag_y[i] = (int16_t)(py + (int16_t)kFragOy[i]);
    }
    accum = 0;
    gained = 0;
    debris_timer = (uint8_t)kDebrisFrames;
    publish();
}

void debris_poof(uint16_t px, int16_t py) BANKED {
    puff_x = px;
    puff_y = py;
    debris_puff = (uint8_t)kPuffFrames;
    publish();
}

void debris_frame(uint16_t cam_x, uint8_t cam_y) BANKED {
    uint8_t spin = 0;
    uint8_t i;

    if (debris_timer != 0U) {
        const uint16_t sum = (uint16_t)((uint16_t)accum + (uint16_t)kDebrisGravitySubpx);

        accum = (uint8_t)sum;
        if (sum > 0xFFU && gained < (int8_t)kDebrisGainCap) {
            ++gained;
        }
        for (i = 0; i < (uint8_t)kDebrisCount; ++i) {
            frag_x[i] = (int16_t)(frag_x[i] + kFragDx[i]);
            frag_y[i] = (int16_t)(frag_y[i] + (int16_t)(kFragDy[i] + gained));
        }
        --debris_timer;
        spin = kFragSpin[((uint8_t)(kDebrisFrames - debris_timer) / kDebrisSpinFrames) & 3U];
    }

    for (i = 0; i < (uint8_t)kDebrisCount; ++i) {
        const uint8_t oam = (uint8_t)(kSpriteFreeFirst + i);
        const uint8_t bit = (uint8_t)(1U << i);
        int16_t sx;
        int16_t sy;

        if (debris_timer != 0U) {
            sx = (int16_t)(frag_x[i] - (int16_t)cam_x);
            sy = (int16_t)(frag_y[i] - (int16_t)cam_y);
        } else {
            sx = 0;
            sy = (int16_t)kScreenHeightPx; // off the view, so the branch below parks it
        }
        if (sy <= -16 || sy >= (int16_t)kScreenHeightPx || sx <= -8 || sx >= (int16_t)kScreenWidthPx) {
            if ((frag_shown & bit) != 0U) {
                frag_shown = (uint8_t)(frag_shown & (uint8_t)~bit);
                hide(oam);
            }
            continue;
        }
        frag_shown = (uint8_t)(frag_shown | bit);
        set_sprite_tile(oam, (uint8_t)kTileDebris);
        // the goomba's tan-over-brown-over-black is the closest sprite set to the brick's own
        // browns; no cgb sprite palette slot is left to give the fragments one of their own
        set_sprite_prop(oam, (uint8_t)(kPalGoomba | spin));
        move_sprite(oam, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    }

    if (debris_puff != 0U) {
        const uint8_t oam = (uint8_t)(kSpriteFreeFirst + kDebrisCount);
        const int16_t sx = (int16_t)((int16_t)puff_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(puff_y - (int16_t)cam_y);

        --debris_puff;
        if (sy <= -16 || sy >= (int16_t)kScreenHeightPx || sx <= -8 || sx >= (int16_t)kScreenWidthPx) {
            debris_puff = 0;
        } else {
            puff_shown = 1;
            // the first half of the window is the tight burst, the second the widened one
            set_sprite_tile(oam, debris_puff >= (uint8_t)(kPuffFrames / 2U) ? (uint8_t)kTilePuffA
                                                                            : (uint8_t)kTilePuffB);
            set_sprite_prop(oam, (uint8_t)kPalStar);
            move_sprite(oam, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
        }
    }
    if (debris_puff == 0U && puff_shown != 0U) {
        puff_shown = 0;
        hide((uint8_t)(kSpriteFreeFirst + kDebrisCount));
    }
    publish();
}
