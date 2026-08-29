#include "player.h"

#include "assets.h"
#include "level_1_1.h"
#include "mario.h"
#include "physics_constants.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// the bible's tables, generated from physics.json by games/mario/tools/gen_physics.py
static const uint8_t kJumpTierBounds[kMarioJumpBoundCount] = kMarioJumpTierBoundsInit;
static const int8_t kJumpSpeed[kMarioJumpTierCount] = kMarioJumpSpeedInit;
static const uint8_t kGravityRise[kMarioJumpTierCount] = kMarioGravityRiseInit;
static const uint8_t kGravityFall[kMarioJumpTierCount] = kMarioGravityFallInit;

// horizontal: smb's signed byte, high nibble whole px + low nibble 1/16 px
static int8_t x_speed;
// 1/256-subpixel accumulator the friction/accel adder feeds; its carry moves x_speed one subpixel
static uint8_t x_accum;
// 1/256-px accumulator the speed's low nibble feeds; its carry moves x_pos one px
static uint8_t x_force;
static uint16_t x_pos;

// vertical: whole-px speed plus the 1/256-px accumulator gravity adds to
static int8_t y_speed;
static uint8_t y_accum;
static int16_t y_pos;
static int16_t jump_origin_y;

static uint8_t on_ground;
static uint8_t jump_tier;
static uint8_t a_prev;
static uint8_t facing_left;
static uint8_t skidding;
static uint8_t anim_frame;
static uint8_t anim_accum;
static uint8_t walk_step;

static uint8_t abs_speed(void) {
    return (uint8_t)(x_speed < 0 ? -x_speed : x_speed);
}

// rows above the level are open sky, so a negative y never indexes the grid
static int16_t row_of(int16_t py) {
    return py < 0 ? (int16_t)-1 : (int16_t)(py >> 4);
}

static int16_t col_of(uint16_t px) {
    return (int16_t)(px >> 4);
}

static void stop_x(void) {
    x_speed = 0;
    x_accum = 0;
    x_force = 0;
}

static void toward_zero(uint8_t delta) {
    if (x_speed > 0) {
        x_speed = (int8_t)(x_speed - (int8_t)delta);
        if (x_speed < 0) {
            x_speed = 0;
        }
    } else if (x_speed < 0) {
        x_speed = (int8_t)(x_speed + (int8_t)delta);
        if (x_speed > 0) {
            x_speed = 0;
        }
    }
}

static void accelerate(int8_t dir, uint8_t delta, uint8_t max_speed) {
    int16_t next = (int16_t)((int16_t)x_speed + (int16_t)dir * (int16_t)delta);

    if (next > (int16_t)max_speed) {
        next = (int16_t)max_speed;
    }
    if (next < -(int16_t)max_speed) {
        next = -(int16_t)max_speed;
    }
    x_speed = (int8_t)next;
}

// one frame of the bible's friction/acceleration: pick a tier, double it when reversing, run it
// through the 1/256-subpixel accumulator, then spend whatever whole subpixels carried out
static void step_speed(uint8_t keys) {
    const uint8_t run = (keys & J_B) != 0U ? 1U : 0U;
    const uint8_t speed_abs = abs_speed();
    const int8_t moving_dir = x_speed > 0 ? (int8_t)1 : (x_speed < 0 ? (int8_t)-1 : (int8_t)0);
    int8_t want_dir = 0;
    uint8_t max_speed = kMarioMaxWalkSubpx;
    uint16_t adder;
    uint16_t sum;
    uint8_t delta;

    if ((keys & J_LEFT) != 0U && (keys & J_RIGHT) == 0U) {
        want_dir = -1;
    } else if ((keys & J_RIGHT) != 0U && (keys & J_LEFT) == 0U) {
        want_dir = 1;
    }
    if (want_dir != 0) {
        facing_left = want_dir < 0 ? 1U : 0U;
    }
    skidding = (want_dir != 0 && moving_dir != 0 && want_dir != moving_dir) ? 1U : 0U;

    // below the threshold a reversal snaps the speed to zero instead of skidding
    if (skidding != 0U && speed_abs < kMarioSkidStopSubpx) {
        stop_x();
        return;
    }

    // the run tier is only earned on the ground; airborne it is inherited from the takeoff speed
    if ((on_ground != 0U && run != 0U && want_dir != 0 && (moving_dir == 0 || moving_dir == want_dir)) ||
        (on_ground == 0U && speed_abs >= kMarioAirRunTierSubpx)) {
        adder = kMarioRunAccel;
    } else if (speed_abs >= kMarioHighSpeedTierSubpx) {
        adder = kMarioHighSpeedAccel;
    } else {
        adder = kMarioWalkAccel;
    }
    if (on_ground != 0U) {
        if (run != 0U) {
            max_speed = kMarioMaxRunSubpx;
        }
    } else if (speed_abs >= kMarioAirRunTierSubpx) {
        max_speed = kMarioMaxRunSubpx;
    }
    // the disassembly's asl/rol: doubling can push a whole subpixel into the adder's high byte
    if (skidding != 0U) {
        adder = (uint16_t)(adder * (uint16_t)kMarioSkidMultiplier);
    }

    sum = (uint16_t)((uint16_t)x_accum + adder);
    x_accum = (uint8_t)sum;
    delta = (uint8_t)(sum >> 8);
    if (delta == 0U) {
        return;
    }
    if (want_dir == 0 || skidding != 0U || speed_abs > max_speed) {
        toward_zero(delta);
    } else {
        accelerate(want_dir, delta, max_speed);
    }
}

// MoveObjectHorizontally: the low nibble feeds a 1/256-px accumulator, the sign-extended high
// nibble is whole pixels, and the accumulator's carry adds the sixteenth that just completed
static void move_x(void) {
    const uint8_t raw = (uint8_t)x_speed;
    const uint16_t sum = (uint16_t)((uint16_t)x_force + (uint16_t)((uint16_t)(raw & 0x0FU) << 4));
    int16_t whole = (int16_t)(raw >> 4);

    if (whole >= 8) {
        whole = (int16_t)(whole - 16);
    }
    x_force = (uint8_t)sum;
    x_pos = (uint16_t)((int16_t)x_pos + whole + (int16_t)(sum >> 8));
}

static void collide_x(void) {
    const int16_t top_row = row_of(y_pos);
    const int16_t bottom_row = row_of((int16_t)(y_pos + kPlayerHeightPx - 1));
    int16_t col;

    if (x_speed > 0) {
        col = col_of((uint16_t)(x_pos + kPlayerWidthPx - 1));
        if (terrain_solid_at(col, top_row) != 0U || terrain_solid_at(col, bottom_row) != 0U) {
            x_pos = (uint16_t)((uint16_t)((uint16_t)col << 4) - kPlayerWidthPx);
            stop_x();
        }
    } else if (x_speed < 0) {
        col = col_of(x_pos);
        if (terrain_solid_at(col, top_row) != 0U || terrain_solid_at(col, bottom_row) != 0U) {
            x_pos = (uint16_t)((uint16_t)(col + 1) << 4);
            stop_x();
        }
    }
}

static uint8_t tier_for(uint8_t speed_abs) {
    uint8_t i;

    for (i = 0; i < kMarioJumpBoundCount; ++i) {
        if (speed_abs <= kJumpTierBounds[i]) {
            return i;
        }
    }
    return (uint8_t)(kMarioJumpTierCount - 1U);
}

// the bible's variable-height rule: weak rising gravity survives only while a stays held from the
// previous frame, and the first pixel of the rise is exempt from the release cut
static uint8_t gravity_now(uint8_t a_held) {
    if (y_speed < 0) {
        if ((a_held != 0U && a_prev != 0U) || (int16_t)(jump_origin_y - y_pos) < kMarioMinRisePx) {
            return kGravityRise[jump_tier];
        }
    }
    return kGravityFall[jump_tier];
}

static void step_vertical(uint8_t keys) {
    const uint8_t a_held = (keys & J_A) != 0U ? 1U : 0U;
    uint16_t sum;

    if (on_ground != 0U && a_held != 0U && a_prev == 0U) {
        jump_tier = tier_for(abs_speed());
        y_speed = kJumpSpeed[jump_tier];
        y_accum = 0;
        jump_origin_y = y_pos;
        on_ground = 0;
    }

    sum = (uint16_t)((uint16_t)y_accum + (uint16_t)gravity_now(a_held));
    y_accum = (uint8_t)sum;
    if (sum > 0xFFU) {
        y_speed = (int8_t)(y_speed + 1);
        if (y_speed > kMarioMaxFallPx) {
            y_speed = kMarioMaxFallPx;
        }
    }
    y_pos = (int16_t)(y_pos + y_speed);
}

static void collide_y(void) {
    const int16_t left_col = col_of(x_pos);
    const int16_t right_col = col_of((uint16_t)(x_pos + kPlayerWidthPx - 1));
    int16_t row;

    on_ground = 0;
    if (y_speed < 0) {
        row = row_of(y_pos);
        if (terrain_solid_at(left_col, row) != 0U || terrain_solid_at(right_col, row) != 0U) {
            y_pos = (int16_t)((row + 1) << 4);
            y_speed = 0;
            y_accum = 0;
        }
        return;
    }
    row = row_of((int16_t)(y_pos + kPlayerHeightPx - 1));
    if (terrain_solid_at(left_col, row) != 0U || terrain_solid_at(right_col, row) != 0U) {
        y_pos = (int16_t)(((int16_t)(row << 4)) - kPlayerHeightPx);
        y_speed = 0;
        y_accum = 0;
        on_ground = 1;
        return;
    }
    // already resting exactly on a block top: the box clears the row below, so probe it directly
    if ((y_pos & 15) == 0 && (terrain_solid_at(left_col, (int16_t)(row + 1)) != 0U ||
                              terrain_solid_at(right_col, (int16_t)(row + 1)) != 0U)) {
        y_speed = 0;
        y_accum = 0;
        on_ground = 1;
    }
}

static void step_anim(void) {
    const uint8_t speed_abs = abs_speed();

    if (on_ground == 0U) {
        anim_frame = kFrameJump;
        return;
    }
    if (skidding != 0U) {
        anim_frame = kFrameSkid;
        return;
    }
    if (speed_abs == 0U) {
        anim_frame = kFrameIdle;
        anim_accum = 0;
        walk_step = 0;
        return;
    }
    anim_accum = (uint8_t)(anim_accum + speed_abs);
    if (anim_accum >= kWalkAnimStepSubpx) {
        anim_accum = (uint8_t)(anim_accum - kWalkAnimStepSubpx);
        walk_step = (uint8_t)((walk_step + 1U) % kWalkFrameCount);
    }
    anim_frame = (uint8_t)(kFrameWalk0 + walk_step);
}

void player_init(void) {
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    SPRITES_8x16;

    x_speed = 0;
    x_accum = 0;
    x_force = 0;
    x_pos = (uint16_t)((uint16_t)LEVEL_1_1_START_COLUMN << 4);
    y_speed = 0;
    y_accum = 0;
    // the bible's start row is the ground surface, so mario's feet rest on top of it
    y_pos = (int16_t)((int16_t)((int16_t)LEVEL_1_1_START_ROW << 4) - kPlayerHeightPx);
    jump_origin_y = y_pos;
    on_ground = 1;
    jump_tier = 0;
    a_prev = 0;
    facing_left = 0;
    skidding = 0;
    anim_frame = kFrameIdle;
    anim_accum = 0;
    walk_step = 0;
    SHOW_SPRITES;
}

uint8_t player_update(uint8_t keys) {
    step_speed(keys);
    move_x();
    collide_x();
    step_vertical(keys);
    collide_y();
    step_anim();
    a_prev = (keys & J_A) != 0U ? 1U : 0U;
    // the whole body has to clear the level's bottom edge, so a pit fall really does go off screen
    // before the respawn; the death flow proper is m8, this is just the reset
    return y_pos >= (int16_t)(kLevelHeightPx + kPlayerHeightPx) ? 1U : 0U;
}

// oam y 0 parks a sprite entirely above the screen
static void player_hide(void) {
    move_sprite(kSpriteMarioL, 0, 0);
    move_sprite(kSpriteMarioR, 0, 0);
}

void player_draw(uint16_t cam_x, uint8_t cam_y) {
    const int16_t sx = (int16_t)((int16_t)x_pos - (int16_t)cam_x);
    const int16_t sy = (int16_t)(y_pos - (int16_t)cam_y);
    const uint8_t left_tile = (uint8_t)(kTileMarioFirst + (uint8_t)(anim_frame * kMarioTilesPerFrame));
    const uint8_t right_tile = (uint8_t)(left_tile + 2U);
    const uint8_t prop = facing_left != 0U ? (uint8_t)S_FLIPX : 0U;

    if (sy <= -(int16_t)kPlayerHeightPx || sy >= (int16_t)kScreenHeightPx || sx <= -(int16_t)kPlayerWidthPx ||
        sx >= (int16_t)kScreenWidthPx) {
        player_hide();
        return;
    }
    // flipping mirrors each 8x16 half, so the halves also swap sides
    set_sprite_tile(kSpriteMarioL, facing_left != 0U ? right_tile : left_tile);
    set_sprite_tile(kSpriteMarioR, facing_left != 0U ? left_tile : right_tile);
    set_sprite_prop(kSpriteMarioL, prop);
    set_sprite_prop(kSpriteMarioR, prop);
    move_sprite(kSpriteMarioL, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    move_sprite(kSpriteMarioR, (uint8_t)(sx + 8 + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}

uint16_t player_x(void) {
    return x_pos;
}
