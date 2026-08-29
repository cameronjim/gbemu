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
// the scy the view drifts to whenever mario is moving; resampled only while he is supported, which
// is what keeps a jump from panning the camera (smbd's own auto behaviour is undocumented)
static uint8_t band;

// the window that leaves mario's feet kCamGroundOffsetPx above its bottom edge, clamped to the pan.
// the feet rather than the box top, so growing to 16x32 does not jerk the view up half a block
static uint8_t band_for(int16_t feet_y) {
    int16_t want = (int16_t)(feet_y + kCamGroundOffsetPx - (int16_t)kScreenHeightPx);

    if (want < 0) {
        want = 0;
    }
    if (want > (int16_t)kScyMax) {
        want = (int16_t)kScyMax;
    }
    return (uint8_t)want;
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

static void step_y(int16_t feet_y, uint8_t on_ground, uint8_t standing, uint8_t keys) {
    const uint8_t up = (keys & J_UP) != 0U ? 1U : 0U;
    const uint8_t down = (keys & J_DOWN) != 0U ? 1U : 0U;
    int16_t next = (int16_t)camera_pos_y;

    if (on_ground != 0U) {
        band = band_for(feet_y);
    }
    if (standing != 0U) {
        // the manual pan: only a standing mario may move the view, and it stays where he left it
        if (up != 0U && down == 0U) {
            next -= (int16_t)kCamPanStepPx;
        } else if (down != 0U && up == 0U) {
            next += (int16_t)kCamPanStepPx;
        } else {
            return;
        }
    } else if (camera_pos_y < band) {
        next += (int16_t)kCamPanStepPx;
        if (next > (int16_t)band) {
            next = (int16_t)band;
        }
    } else if (camera_pos_y > band) {
        next -= (int16_t)kCamPanStepPx;
        if (next < (int16_t)band) {
            next = (int16_t)band;
        }
    } else {
        return;
    }

    if (next < 0) {
        next = 0;
    }
    if (next > (int16_t)kScyMax) {
        next = (int16_t)kScyMax;
    }
    camera_pos_y = (uint8_t)next;
}

void camera_init(uint16_t mario_x, int16_t feet_y) BANKED {
    camera_pos_anchor = (uint8_t)kCamFollowX;
    band = band_for(feet_y);
    camera_pos_y = band;
    step_x(mario_x);
}

void camera_update(uint16_t mario_x, int16_t feet_y, uint8_t on_ground, uint8_t standing,
                   uint8_t keys) BANKED {
    step_anchor(keys);
    step_x(mario_x);
    step_y(feet_y, on_ground, standing, keys);
}
