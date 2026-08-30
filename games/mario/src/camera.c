// bank 0 ran out again in m8a. the camera is two entry points a frame and its three readouts are
// ram, so banking it costs one trampoline and gives the draw pass eleven plain loads back
#pragma bank 5

#include "camera.h"

#include "mario.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

uint16_t camera_pos_x;
uint8_t camera_pos_y;
// mario's screen anchor; select slides it forward and releasing slides it back, 2 px a frame
uint8_t camera_pos_anchor;
// how long the current look direction has been held with mario standing; kCamLookDelayFrames is the
// frame it engages on and the counter stops one past that, so a tap costs nothing but a byte.
static uint8_t look_hold;
// 0 none, J_UP or J_DOWN for the direction being held
static uint8_t look_dir;
// where the view sat when the look engaged. Latching it means holding a direction reaches a finite
// target instead of walking the view off a frame at a time.
static uint8_t look_base;

static uint8_t clamp_scy(int16_t want) {
    if (want < 0) {
        return 0U;
    }
    if (want > (int16_t)kScyMax) {
        return (uint8_t)kScyMax;
    }
    return (uint8_t)want;
}

// the view that leaves mario's feet kCamGroundOffsetPx above the bottom edge. Only camera_init uses
// it now: it is where the level opens, not something the play camera chases.
static uint8_t band_for(int16_t feet_y) {
    return clamp_scy((int16_t)(feet_y + kCamGroundOffsetPx - (int16_t)kScreenHeightPx));
}

// the deadzone. feet - camera_pos_y is where his feet are down the screen; while that sits inside
// the window the answer is "don't move", which is the whole point - a hop onto a ledge shifts his
// feet up the screen and leaves the ground he came from on it. Play coordinates fit in one byte and
// the two sums below cannot carry out of one (96 + 112), so this stays off the 16-bit helpers: the
// hot path runs every frame and one frame over budget costs the game a whole logic step.
static uint8_t window_target(uint8_t feet, uint8_t on_ground) {
    const uint8_t top = on_ground != 0U ? (uint8_t)kCamWindowTopPx : (uint8_t)kCamSafeTopPx;
    const uint8_t bottom = on_ground != 0U ? (uint8_t)kCamWindowBottomPx : (uint8_t)kCamSafeBottomPx;

    if (feet < (uint8_t)(camera_pos_y + top)) {
        return feet > top ? (uint8_t)(feet - top) : 0U;
    }
    if (feet > (uint8_t)(camera_pos_y + bottom)) {
        const uint8_t want = (uint8_t)(feet - bottom);
        return want < (uint8_t)kScyMax ? want : (uint8_t)kScyMax;
    }
    return camera_pos_y;
}

// one frame of easing toward want. Never an assignment: the step is a fraction of what is left, so
// a big correction starts at the cap and decelerates onto its target over many frames.
static void ease_to(uint8_t want, uint8_t on_ground) {
    const uint8_t shift = on_ground != 0U ? (uint8_t)kCamEaseShift : (uint8_t)kCamAirEaseShift;
    uint8_t gap;
    uint8_t step;

    if (want == camera_pos_y) {
        return;
    }
    gap = want > camera_pos_y ? (uint8_t)(want - camera_pos_y) : (uint8_t)(camera_pos_y - want);
    step = (uint8_t)(gap >> shift);
    if (step == 0U) {
        step = 1U;
    } else if (step > (uint8_t)kCamEaseMaxPx) {
        step = (uint8_t)kCamEaseMaxPx;
    }
    if (step > gap) {
        step = gap;
    }
    camera_pos_y = want > camera_pos_y ? (uint8_t)(camera_pos_y + step) : (uint8_t)(camera_pos_y - step);
}

static void step_anchor(uint8_t keys) {
    const uint8_t want = (keys & J_SELECT) != 0U ? (uint8_t)kCamLookAheadX : (uint8_t)kCamFollowX;

    if (camera_pos_anchor > want) {
        camera_pos_anchor = (uint8_t)(camera_pos_anchor - (uint8_t)kCamAnchorStepPx);
        if (camera_pos_anchor < want) {
            camera_pos_anchor = want;
        }
    } else if (camera_pos_anchor < want) {
        camera_pos_anchor = (uint8_t)(camera_pos_anchor + (uint8_t)kCamAnchorStepPx);
        if (camera_pos_anchor > want) {
            camera_pos_anchor = want;
        }
    }
}

// backward scrolling is the smbd difference: nothing ratchets the camera forward, the level's two
// ends are the only bounds
static void step_x(uint16_t mario_x) {
    const uint16_t max_x = terrain_max_camera_x();
    uint16_t want =
        mario_x > (uint16_t)camera_pos_anchor ? (uint16_t)(mario_x - (uint16_t)camera_pos_anchor) : 0U;

    if (want > max_x) {
        want = max_x;
    }
    camera_pos_x = want;
}

static void step_y(uint8_t feet, uint8_t on_ground, uint8_t standing, uint8_t keys) {
    const uint8_t up = (keys & J_UP) != 0U ? J_UP : 0U;
    const uint8_t down = (keys & J_DOWN) != 0U ? J_DOWN : 0U;
    const uint8_t want_dir = (up != 0U) != (down != 0U) ? (uint8_t)(up | down) : 0U;

    if (on_ground == 0U || standing == 0U || want_dir == 0U) {
        // moving, airborne or empty-handed cancels the peek outright, and the next hold starts over
        look_dir = 0U;
        look_hold = 0U;
    } else {
        if (want_dir != look_dir) {
            look_dir = want_dir;
            look_hold = 0U;
        }
        if (look_hold < (uint8_t)kCamLookDelayFrames) {
            ++look_hold;
        } else {
            if (look_hold == (uint8_t)kCamLookDelayFrames) {
                look_base = window_target(feet, 1U);
                ++look_hold;
            }
            ease_to(look_dir == J_UP ? (look_base > (uint8_t)kCamLookUpPx
                                            ? (uint8_t)(look_base - (uint8_t)kCamLookUpPx)
                                            : 0U)
                                     : (look_base > (uint8_t)(kScyMax - kCamLookDownPx)
                                            ? (uint8_t)kScyMax
                                            : (uint8_t)(look_base + (uint8_t)kCamLookDownPx)),
                    1U);
            return;
        }
    }
    ease_to(window_target(feet, on_ground), on_ground);
}

void camera_init(uint16_t mario_x, int16_t feet_y) BANKED {
    camera_pos_anchor = (uint8_t)kCamFollowX;
    look_dir = 0U;
    look_hold = 0U;
    look_base = 0U;
    camera_pos_y = band_for(feet_y);
    step_x(mario_x);
}

void camera_update(uint16_t mario_x, int16_t feet_y, uint8_t on_ground, uint8_t standing,
                   uint8_t keys) BANKED {
    step_anchor(keys);
    step_x(mario_x);
    // the window works in bytes, and a pit drops him past 255: without the saturation his feet wrap
    // to the top of the level and the view goes chasing them straight up, off the ground he just
    // fell off. saturated, the window keeps asking for the bottom of the level, which is right
    step_y(feet_y < 0 ? 0U : (feet_y > 255 ? 255U : (uint8_t)feet_y), on_ground, standing, keys);
}
