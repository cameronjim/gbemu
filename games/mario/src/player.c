#include "player.h"

#include "assets.h"
#include "blocks.h"
#include "flow.h"
#include "hazards.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
#include "terrain.h"
#include "toad.h"

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

// the 16x32 body, and the crouch that folds it to 16 px. the power state itself lives in powerup.c;
// this module only ever needs the size it implies
static uint8_t big;
static uint8_t crouched;

// the level-clear sequence's own little state machine, driven by player_clear_update
static uint8_t clear_phase;
static uint8_t clear_timer;
// a castle has no pole to slide down, so its clear walks him off from the axe
static uint8_t clear_axe;
// 1 while he is holding the pole, which is the one pose that comes out of vram bank 1
static uint8_t climbing;
// and 1 once he has stepped into the castle door, which draws him nowhere at all
static uint8_t clear_gone;
// 1 once the pennant flow_flag_step is walking down the pole has reached its base
static uint8_t clear_flag_done;

// the box top before this frame's vertical step: a thin platform is solid only to feet that
// crossed its deck line, so the collision needs where they were as well as where they are
static int16_t prev_y;
// the lift deck he is standing on, or 0xff
static uint8_t riding;

// the tile and palette each of mario's four slots last carried; a walk frame lasts eight frames and
// his palette changes twice a level, so most frames owe nothing but the two moves
static uint8_t drawn_mario_tile[4];
static uint8_t drawn_mario_prop[4];

// the death beat: how far into the hold he is, and whether he still has a leap to make
static uint8_t death_timer;
static uint8_t death_leaps;

// the pipe transition: which way mario is travelling and how far he has gone
static uint8_t pipe_phase;
static uint8_t pipe_travel;
// while he is inside a pipe his sprites draw behind the bg, which is what hides him in it
static uint8_t behind_bg;

// the feet always sit this far below y_pos, crouching included: folding the body moves its top edge
// down rather than lifting him off the floor
static uint8_t foot_h(void) {
    return big != 0U ? (uint8_t)kPlayerBigHeightPx : (uint8_t)kPlayerHeightPx;
}

static int16_t head_y(void) {
    return (int16_t)(y_pos + (crouched != 0U ? kCrouchInsetPx : 0));
}

// the hitbox spans [x_pos + inset, x_pos + inset + width - 1]; the sprite still spans the full 16
static uint16_t hit_left(void) {
    return (uint16_t)(x_pos + kPlayerHitInsetPx);
}

static uint16_t hit_right(void) {
    return (uint16_t)(x_pos + kPlayerHitInsetPx + kPlayerHitWidthPx - 1U);
}

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
    // a mario holding down keeps no walk of his own past a crawl: smb gives him none at all, but
    // that leaves one who came to a stop flush against 1-2's brick pillar at 78/79 no way into the
    // one block of crawl space under it - the slide wants momentum he no longer has, and standing
    // back up only puts him against the same brick. anything above the crawl cap still sheds
    // through the friction path below, so a duck-slide out of a run is momentum first and cannot be
    // steered faster; one still folded because a ceiling will not let him stand walks normally
    const uint8_t crawling = (crouched != 0U && (keys & J_DOWN) != 0U) ? 1U : 0U;
    int8_t want_dir = 0;
    uint8_t max_speed = crawling != 0U ? (uint8_t)kMarioCrouchWalkSubpx : (uint8_t)kMarioMaxWalkSubpx;
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

    // standing still with nothing held has nothing to accelerate and nothing to shed, so the
    // subpixel accumulator is parked instead of left ticking: otherwise how long mario happened to
    // stand somewhere would shift the phase of his next step off the line by a pixel
    if (want_dir == 0 && x_speed == 0) {
        x_accum = 0;
        return;
    }

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
    // the crawl cap outranks both run tiers: b earns nothing while he is folded
    if (crawling == 0U) {
        if (on_ground != 0U) {
            if (run != 0U) {
                max_speed = kMarioMaxRunSubpx;
            }
        } else if (speed_abs >= kMarioAirRunTierSubpx) {
            max_speed = kMarioMaxRunSubpx;
        }
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
    int16_t next;

    if (whole >= 8) {
        whole = (int16_t)(whole - 16);
    }
    x_force = (uint8_t)sum;
    next = (int16_t)((int16_t)x_pos + whole + (int16_t)(sum >> 8));
    // the level's opening edge is a wall like every other column boundary, and x_pos is unsigned:
    // walking off it would wrap the position instead of stopping. now that the camera scrolls back,
    // that edge is reachable, so the clamp lives here rather than in collide_x's column probe
    if (next < 0) {
        next = 0;
        stop_x();
    }
    x_pos = (uint16_t)next;
}

// a 32 px body can straddle three block rows, so the row between his head and his feet is probed
// too - without it a lone block at chest height would let him walk straight through
static uint8_t blocked_at(int16_t col) {
    const int16_t top = head_y();

    if (terrain_solid_at(col, row_of(top)) != 0U ||
        terrain_solid_at(col, row_of((int16_t)(y_pos + foot_h() - 1))) != 0U) {
        return 1;
    }
    // a folded body is one cell tall, so the head and foot probes already cover it
    return (big != 0U && crouched == 0U &&
            terrain_solid_at(col, row_of((int16_t)(top + kPlayerHeightPx))) != 0U)
               ? 1U
               : 0U;
}

// 1 when the cells his head would come back up into are clear. standing puts the box top back at
// y_pos, so it is the fold's own cell - and the next one down when he is not cell-aligned
static uint8_t head_room(void) {
    const int16_t left = col_of(hit_left());
    const int16_t right = col_of(hit_right());
    const int16_t row = row_of(y_pos);

    if (terrain_solid_at(left, row) != 0U || terrain_solid_at(right, row) != 0U) {
        return 0;
    }
    if ((y_pos & 15) == 0) {
        return 1;
    }
    return (terrain_solid_at(left, (int16_t)(row + 1)) != 0U ||
            terrain_solid_at(right, (int16_t)(row + 1)) != 0U)
               ? 0U
               : 1U;
}

static void collide_x(void) {
    int16_t col;

    if (x_speed > 0) {
        col = col_of(hit_right());
        if (blocked_at(col) != 0U) {
            x_pos = (uint16_t)((uint16_t)((uint16_t)col << 4) - kPlayerHitInsetPx - kPlayerHitWidthPx);
            stop_x();
        }
    } else if (x_speed < 0) {
        col = col_of(hit_left());
        if (blocked_at(col) != 0U) {
            x_pos = (uint16_t)(((uint16_t)(col + 1) << 4) - kPlayerHitInsetPx);
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
    const int16_t left_col = col_of(hit_left());
    const int16_t right_col = col_of(hit_right());
    const uint8_t height = foot_h();
    uint8_t floor;
    int16_t row;

    on_ground = 0;
    if (y_speed < 0) {
        uint8_t left_hit;
        uint8_t right_hit;

        row = row_of(head_y());
        // a hidden block is absent from the grid until a rising head finds it, so it is probed here
        // as well as through terrain_solid_at; materializing it is what stops the rise
        left_hit = (terrain_solid_at(left_col, row) != 0U || blocks_hidden_at(left_col, row) != 0U) ? 1U : 0U;
        right_hit =
            (terrain_solid_at(right_col, row) != 0U || blocks_hidden_at(right_col, row) != 0U) ? 1U : 0U;
        if (left_hit != 0U || right_hit != 0U) {
            blocks_head_bump(left_hit != 0U ? left_col : right_col, row);
            y_pos = (int16_t)(((row + 1) << 4) - (crouched != 0U ? kCrouchInsetPx : 0));
            y_speed = 0;
            y_accum = 0;
        }
        return;
    }
    row = row_of((int16_t)(y_pos + height - 1));
    // one probe a side: a solid cell always stops him, and a thin platform - 1-3's tree top -
    // stops only feet that crossed its deck line this frame, so a jump from under it passes through
    floor = terrain_floor_at(left_col, row);
    if ((floor & kFloorSolid) == 0U && right_col != left_col) {
        floor = (uint8_t)(floor | terrain_floor_at(right_col, row));
    }
    if ((floor & kFloorSolid) != 0U ||
        ((floor & kFloorThin) != 0U && (int16_t)(prev_y + height) <= (int16_t)(row << 4))) {
        y_pos = (int16_t)(((int16_t)(row << 4)) - height);
        y_speed = 0;
        y_accum = 0;
        on_ground = 1;
        return;
    }
    // already resting exactly on a block top: the box clears the row below, so probe it directly
    if ((y_pos & 15) == 0 && (terrain_floor_at(left_col, (int16_t)(row + 1)) != 0U ||
                              terrain_floor_at(right_col, (int16_t)(row + 1)) != 0U)) {
        y_speed = 0;
        y_accum = 0;
        on_ground = 1;
    }
}

// the lift decks, probed after the terrain: the same crossed-the-deck-line rule a thin platform
// uses, and the slot he lands on is the one that carries him next frame
static void collide_lifts(void) {
    const uint8_t height = foot_h();
    const int16_t feet = (int16_t)(y_pos + height);
    const int16_t was = (int16_t)(prev_y + height);
    uint8_t i;

    riding = 0xFF;
    if (y_speed < 0) {
        return;
    }
    for (i = 0; i < hazard_lift_count; ++i) {
        const int16_t deck = hazard_lift_y[i];

        if (feet < deck || was > deck) {
            continue;
        }
        if ((uint16_t)(x_pos + kPlayerHitInsetPx + kPlayerHitWidthPx) <= hazard_lift_x[i] ||
            (uint16_t)(hazard_lift_x[i] + kLiftWidthPx) <= (uint16_t)(x_pos + kPlayerHitInsetPx)) {
            continue;
        }
        y_pos = (int16_t)(deck - height);
        y_speed = 0;
        y_accum = 0;
        on_ground = 1;
        riding = i;
        return;
    }
}

static void step_anim(void) {
    const uint8_t speed_abs = abs_speed();

    if (crouched != 0U) {
        anim_frame = kFrameCrouch;
        return;
    }
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

// the pole's cells are walk-through, so contact is a box overlap against the compiled shaft rather
// than a terrain_solid_at hit. the span tested is his drawn box against the shaft's whole cell, not
// against the shaft's own pixels: the hard block at the pole's foot stops his hit box a column
// short and the shaft stands in the middle of its cell, so a running mario can never reach the lit
// columns themselves - smb grabs the pole the moment he is up against that block, and so does this. the base row is the shaft's last cell, and the block under it is the row he
// can still be standing on when he reaches it
static uint8_t touching_flag(void) {
    if (level->has_flag == 0U) {
        return 0;
    }
    if (col_of(x_pos) > (int16_t)level->flag_column ||
        col_of((uint16_t)(x_pos + kPlayerWidthPx - 1U)) < (int16_t)level->flag_column) {
        return 0;
    }
    return (row_of(head_y()) <= (int16_t)(level->flag_base_row + 1U) &&
            row_of((int16_t)(y_pos + foot_h() - 1)) >= (int16_t)level->flag_top_row)
               ? 1U
               : 0U;
}

void player_place(uint16_t column, uint8_t surface_row) {
    x_pos = (uint16_t)(column << 4);
    stop_x();
    crouched = 0;
    // the surface row is the ground the feet rest on top of, the bible's own start convention
    y_pos = (int16_t)((int16_t)((int16_t)surface_row << 4) - (int16_t)foot_h());
    y_speed = 0;
    y_accum = 0;
    prev_y = y_pos;
    riding = 0xFF;
    clear_axe = 0;
    jump_origin_y = y_pos;
    on_ground = 1;
    jump_tier = 0;
    a_prev = 0;
    facing_left = 0;
    skidding = 0;
    anim_frame = kFrameIdle;
    anim_accum = 0;
    walk_step = 0;
    behind_bg = 0;
    clear_phase = kClearSlide;
    clear_timer = 0;
    climbing = 0;
    clear_gone = 0;
}

void player_init(void) {
    uint8_t i;

    for (i = 0; i < 4U; ++i) {
        // never a real tile or property, so the first draw of a fresh level writes all four
        drawn_mario_tile[i] = 0xFF;
        drawn_mario_prop[i] = 0xFF;
    }
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    assets_load_item_tiles();
    assets_load_item_palettes();
    SPRITES_8x16;

    big = 0;
    crouched = 0;
    player_place((uint16_t)level->start_column, (uint8_t)level->start_row);
    SHOW_SPRITES;
}

// the feet stay where they are and the box grows or shrinks upward, which is also what makes the
// grow animation's alternating poses read as one body swelling in place
void player_set_big(uint8_t next) {
    if (next == big) {
        return;
    }
    if (next != 0U) {
        y_pos = (int16_t)(y_pos - (kPlayerBigHeightPx - kPlayerHeightPx));
        if (y_pos < 0) {
            y_pos = 0;
        }
    } else {
        y_pos = (int16_t)(y_pos + (kPlayerBigHeightPx - kPlayerHeightPx));
        crouched = 0;
    }
    big = next;
}

uint8_t player_over_pipe(uint16_t column, uint8_t top_row) {
    if (on_ground == 0U) {
        return 0;
    }
    if (row_of((int16_t)(y_pos + foot_h())) != (int16_t)top_row) {
        return 0;
    }
    // his feet only have to be on the cap: nothing else stands at that row beside a pipe, so an
    // overlap with its two columns already means he is on top of it and nowhere else
    return (col_of(hit_left()) <= (int16_t)(column + 1U) && col_of(hit_right()) >= (int16_t)column) ? 1U : 0U;
}

void player_begin_pipe_down(void) {
    stop_x();
    y_speed = 0;
    y_accum = 0;
    on_ground = 0;
    riding = 0xFF;
    facing_left = 0;
    skidding = 0;
    anim_frame = kFrameIdle;
    pipe_phase = kPipeDown;
    pipe_travel = 0;
    behind_bg = 1;
}

void player_begin_pipe_up(uint16_t column, uint8_t top_row) {
    // a full reset first: the jump tier and the a-held edge he carried into the pipe must not
    // survive the trip, or his first jump out of it answers to the room he just left
    player_place(column, top_row);
    // he starts a full body height down the shaft and rises out of it
    y_pos = (int16_t)(y_pos + kPipeTravelPx);
    on_ground = 0;
    pipe_phase = kPipeUp;
    pipe_travel = 0;
    behind_bg = 1;
}

// the sink reset again, with the phase and the pose swapped: he walks into the mouth rather than
// standing on a cap, so the frame he is hidden for is a walk frame
void player_begin_pipe_side(void) {
    player_begin_pipe_down();
    pipe_phase = kPipeSide;
    anim_frame = kFrameWalk0;
}

uint8_t player_pipe_update(void) {
    if (pipe_phase == kPipeSide) {
        x_pos = (uint16_t)(x_pos + kPipeStepPx);
    } else {
        y_pos = (int16_t)(y_pos + (pipe_phase == kPipeDown ? kPipeStepPx : -kPipeStepPx));
    }
    pipe_travel = (uint8_t)(pipe_travel + kPipeStepPx);
    if (pipe_travel < (uint8_t)kPipeTravelPx) {
        return 0;
    }
    if (pipe_phase == kPipeUp) {
        on_ground = 1;
        behind_bg = 0;
    }
    return 1;
}

uint8_t player_update(uint8_t keys) {
    // the deck he was on has already moved this frame, so his carry is applied before anything
    // else: the crossy insight, pixel for pixel rather than re-snapped afterwards
    if (riding < hazard_lift_count) {
        const int16_t carried = (int16_t)((int16_t)x_pos + hazard_lift_dx[riding]);

        x_pos = (carried < 0) ? 0U : (uint16_t)carried;
        y_pos = (int16_t)(y_pos + hazard_lift_dy[riding]);
    }
    // crouching is a grounded pose only; must-measure whether smbd keeps it through a jump. down
    // released under a solid cell leaves him folded rather than popping his head into it, which is
    // what makes a duck-slide through a one-block gap survive the far side of it
    crouched = (big != 0U && on_ground != 0U && ((keys & J_DOWN) != 0U || head_room() == 0U)) ? 1U : 0U;
    step_speed(keys);
    move_x();
    collide_x();
    prev_y = y_pos;
    step_vertical(keys);
    collide_y();
    if (hazard_lift_count != 0U) {
        collide_lifts();
    } else {
        riding = 0xFF;
    }
    step_anim();
    a_prev = (keys & J_A) != 0U ? 1U : 0U;
    // the whole body has to clear the level's bottom edge, so a pit fall really does go off screen
    // before the respawn; the death flow proper is m8, this is just the reset
    if (y_pos >= (int16_t)(kLevelHeightPx + kPlayerHeightPx)) {
        return kPlayerFell;
    }
    return touching_flag() != 0U ? kPlayerFlag : kPlayerAlive;
}

// smb's death: the world stops, mario holds a beat, then leaps and drops clean through the floor.
// a pit or a lava pool has already carried him under the level, so that one only holds
void player_begin_death(uint8_t from) {
    stop_x();
    y_speed = 0;
    y_accum = 0;
    on_ground = 0;
    riding = 0xFF;
    crouched = 0;
    skidding = 0;
    behind_bg = 0;
    anim_frame = kFrameJump;
    death_timer = 0;
    death_leaps = (from == (uint8_t)kDeathFromPit) ? 0U : 1U;
}

uint8_t player_death_update(void) {
    ++death_timer;
    if (death_timer < (uint8_t)kDeathHoldFrames) {
        return 0;
    }
    if (death_timer == (uint8_t)kDeathHoldFrames) {
        y_speed = death_leaps != 0U ? (int8_t)kDeathLaunchPx : (int8_t)0;
    }
    // a plain ramp, not the physics accumulator: the beat owes nothing to the bible and bank 0 had
    // no room for a second gravity path
    if ((death_timer & kDeathGravityMask) == 0U && y_speed < (int8_t)kDeathMaxFallPx) {
        y_speed = (int8_t)(y_speed + 1);
    }
    y_pos = (int16_t)(y_pos + y_speed);
    return (y_pos >= (int16_t)(kLevelHeightPx + kPlayerBigHeightPx)) ? 1U : 0U;
}

void player_begin_clear(uint8_t from) {
    clear_axe = (from == (uint8_t)kClearFromAxe) ? 1U : 0U;
    if (clear_axe == 0U) {
        // beside the shaft rather than straddling it, so the climb pose's hand lands on the pole
        x_pos = (uint16_t)(((uint16_t)level->flag_column << 4) - (uint16_t)kClearPoleOffsetPx);
    }
    stop_x();
    y_speed = 0;
    y_accum = 0;
    on_ground = 0;
    facing_left = 0;
    skidding = 0;
    riding = 0xFF;
    walk_step = 0;
    clear_gone = 0;
    // he grabs the pole on contact and holds it all the way down; a castle has no pole to come
    // down, so its clear starts at the walk-off the slide would have fed into
    climbing = (uint8_t)(clear_axe != 0U ? 0U : 1U);
    anim_frame = clear_axe != 0U ? (uint8_t)kFrameWalk0 : (uint8_t)kFrameJump;
    clear_phase = clear_axe != 0U ? (uint8_t)kClearWalk : (uint8_t)kClearSlide;
    clear_timer = 0;
    // the pennant comes down with him, out of bank 5; a castle's clear has none to bring down
    clear_flag_done = clear_axe;
    flow_flag_arm();
}

// the pole's base: the hard block under the shaft's last cell is what the slide comes to rest on
static int16_t clear_base_y(void) {
    return (int16_t)((int16_t)((int16_t)(level->flag_base_row + 1U) << 4) - (int16_t)foot_h());
}

// and the hop off the pole clears that block: he lands a row lower, on the ground the walk to the
// castle runs along
static int16_t clear_walk_y(void) {
    return (int16_t)(clear_base_y() + kBlockPx);
}

// the column the pole walk-off ends at: the castle's door when the level closes with one, and
// otherwise a fixed run along the closing ground. kept inside the compiled level however short its
// tail is. the axe walk has its own end, out of bank 6 - see toad_walk_end
static uint16_t clear_walk_x(void) {
    uint16_t column;

    if (level->has_castle != 0U) {
        column = (uint16_t)(level->castle_column + (uint16_t)kCastleDoorOffset);
    } else {
        column = (uint16_t)(level->flag_column + (uint16_t)kClearWalkBlocks);
    }
    if (column > (uint16_t)(level_columns - 1U)) {
        column = (uint16_t)(level_columns - 1U);
    }
    return (uint16_t)(column << 4);
}

uint8_t player_clear_update(void) {
    switch (clear_phase) {
    case kClearSlide: {
        // down the pole in the climb pose, the pennant coming down the column beside him. the
        // pole's base is worked out here rather than in the prologue: the axe phases never want it
        const int16_t base_y = clear_base_y();

        if (clear_flag_done == 0U) {
            clear_flag_done = flow_flag_step();
        }
        y_pos = (int16_t)(y_pos + kClearSlidePx);
        if (y_pos >= base_y) {
            y_pos = base_y;
            // smb's beat: at the bottom he swings round to the pole's far side and faces back at it
            x_pos = (uint16_t)(x_pos + kClearFlipPx);
            facing_left = 1;
            clear_phase = kClearFlip;
            clear_timer = 0;
        }
        break;
    }
    case kClearFlip:
        // and waits there while the flag finishes coming down, which is what holds this phase open
        if (clear_flag_done == 0U) {
            clear_flag_done = flow_flag_step();
        }
        ++clear_timer;
        if (clear_timer >= (uint8_t)kClearFlipFrames && clear_flag_done != 0U) {
            climbing = 0;
            facing_left = 0;
            anim_frame = kFrameJump;
            clear_phase = kClearHop;
            clear_timer = 0;
        }
        break;
    case kClearHop:
        // he lets go and hops off to the right: up for the first half of the window, down for the
        // second
        y_pos =
            (int16_t)(y_pos + (clear_timer < (uint8_t)(kClearHopFrames / 2U) ? -kClearHopPx : kClearHopPx));
        x_pos = (uint16_t)(x_pos + kClearWalkPx);
        ++clear_timer;
        if (clear_timer >= (uint8_t)kClearHopFrames) {
            y_pos = clear_walk_y();
            clear_phase = kClearWalk;
            clear_timer = 0;
        }
        break;
    case kClearWalk:
        if (clear_axe != 0U) {
            // the axe walk is the one that has somewhere to fall: the pedestal the axe stood on is
            // four rows over the floor of the room past it, and smb's mario walks off the edge and
            // drops in. so this one is driven through the physics pass with right held rather than
            // sliding x_pos along - that is where gravity, the landing and the walk cycle already
            // live, and bank 0 has no room for a second copy of any of them. where it ends, and
            // therefore which phase it ends in, is bank 6's to work out
            uint8_t next;

            (void)player_update((uint8_t)J_RIGHT);
            next = toad_walk_end();
            if (next != 0U) {
                stop_x();
                anim_frame = kFrameIdle;
                // he stands beside the retainer for the whole of the sign; a castle whose bible
                // named no toad room has nothing to stand in front of and keeps the vanishing act
                clear_gone = (uint8_t)(next == (uint8_t)kClearDoor);
                clear_phase = next;
                clear_timer = 0;
            }
            break;
        }
        x_pos = (uint16_t)(x_pos + kClearWalkPx);
        anim_frame = (uint8_t)(kFrameWalk0 + walk_step);
        ++clear_timer;
        if (clear_timer >= (uint8_t)kClearWalkAnimFrames) {
            clear_timer = 0;
            walk_step = (uint8_t)((walk_step + 1U) % kWalkFrameCount);
        }
        if (x_pos >= clear_walk_x()) {
            anim_frame = kFrameIdle;
            // he steps into the doorway and is drawn nowhere from here on
            clear_gone = 1;
            clear_phase = kClearDoor;
            clear_timer = 0;
        }
        break;
    case kClearToad: {
        // the retainer, the sign over him and the hold, all of it out of bank 6. the tick is this
        // side's, so that side keeps no state at all
        const uint8_t tick = clear_timer;

        ++clear_timer;
        return toad_frame(tick);
    }
    case kClearDoor:
        ++clear_timer;
        if (clear_timer >= (uint8_t)kClearDoorFrames) {
            clear_phase = kClearHold;
            clear_timer = 0;
        }
        break;
    default:
        // the level-clear beat: the empty castle holds the screen while the state runs out
        ++clear_timer;
        if (clear_timer >= (uint8_t)kClearHoldFrames) {
            return 1;
        }
        break;
    }
    return 0;
}

// oam y 0 parks a sprite entirely above the screen
static void player_hide(void) {
    move_sprite(kSpriteMarioL, 0, 0);
    move_sprite(kSpriteMarioR, 0, 0);
    move_sprite(kSpriteMarioLowL, 0, 0);
    move_sprite(kSpriteMarioLowR, 0, 0);
}

// one 16 px sprite row: flipping mirrors each 8x16 half, so the halves also swap sides
static void draw_row(uint8_t slot, uint8_t tile, uint8_t prop, int16_t sx, int16_t sy) {
    const uint8_t left_tile = facing_left != 0U ? (uint8_t)(tile + 2U) : tile;
    const uint8_t right_tile = facing_left != 0U ? tile : (uint8_t)(tile + 2U);

    if (drawn_mario_tile[slot] != left_tile || drawn_mario_prop[slot] != prop) {
        drawn_mario_tile[slot] = left_tile;
        drawn_mario_prop[slot] = prop;
        set_sprite_tile(slot, left_tile);
        set_sprite_tile((uint8_t)(slot + 1U), right_tile);
        set_sprite_prop(slot, prop);
        set_sprite_prop((uint8_t)(slot + 1U), prop);
    }
    move_sprite(slot, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    move_sprite((uint8_t)(slot + 1U), (uint8_t)(sx + 8 + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}

void player_draw(uint16_t cam_x, uint8_t cam_y, uint8_t palette) {
    const int16_t sx = (int16_t)((int16_t)x_pos - (int16_t)cam_x);
    const int16_t sy = (int16_t)(y_pos - (int16_t)cam_y);
    const uint8_t prop = (uint8_t)(palette | (facing_left != 0U ? (uint8_t)S_FLIPX : 0U) |
                                   (behind_bg != 0U ? (uint8_t)S_PRIORITY : 0U));

    if (palette == (uint8_t)kSpriteHidden || clear_gone != 0U || sy <= -(int16_t)foot_h() ||
        sy >= (int16_t)kScreenHeightPx || sx <= -(int16_t)kPlayerWidthPx || sx >= (int16_t)kScreenWidthPx) {
        player_hide();
        return;
    }
    // the climb and jump poses live in vram bank 1, so their rows carry S_BANK in the prop. the
    // slot cache compares (tile, prop) and the prop is what tells the two banks apart, so a pose
    // that shares an id with a bank-0 one still reads as a change
    if (climbing != 0U) {
        const uint8_t climb_prop = (uint8_t)(prop | (uint8_t)S_BANK);

        if (big == 0U) {
            draw_row(kSpriteMarioL, (uint8_t)kTileClimbSmall, climb_prop, sx, sy);
            move_sprite(kSpriteMarioLowL, 0, 0);
            move_sprite(kSpriteMarioLowR, 0, 0);
            return;
        }
        draw_row(kSpriteMarioL, (uint8_t)kTileClimbBigUpper, climb_prop, sx, sy);
        draw_row(kSpriteMarioLowL, (uint8_t)kTileClimbBigLower, climb_prop, sx,
                 (int16_t)(sy + kPlayerHeightPx));
        return;
    }
    if (big == 0U) {
        draw_row(kSpriteMarioL, (uint8_t)(kTileMarioFirst + (uint8_t)(anim_frame * kMarioTilesPerFrame)),
                 prop, sx, sy);
        move_sprite(kSpriteMarioLowL, 0, 0);
        move_sprite(kSpriteMarioLowR, 0, 0);
        return;
    }
    if (crouched != 0U) {
        // the fold is one 16x16 pose of its own, so the upper row parks; 0xff is never a real
        // tile, so the cache cannot skip the set_sprite_tile the next standing frame owes
        drawn_mario_tile[kSpriteMarioL] = 0xFF;
        drawn_mario_prop[kSpriteMarioL] = 0xFF;
        move_sprite(kSpriteMarioL, 0, 0);
        move_sprite(kSpriteMarioR, 0, 0);
        draw_row(kSpriteMarioLowL, (uint8_t)(kTileSuperLowerFirst + kFrameCrouch * kSuperTilesPerFrame), prop,
                 sx, (int16_t)(sy + kCrouchInsetPx));
        return;
    }
    // every standing pose but the jump shares one upper slab, which is why the jump could not raise
    // an arm: its own slab is the one thing bank 1 holds for him
    if (anim_frame == (uint8_t)kFrameJump) {
        draw_row(kSpriteMarioL, (uint8_t)kTileSuperJumpUpper, (uint8_t)(prop | (uint8_t)S_BANK), sx, sy);
    } else {
        draw_row(kSpriteMarioL, (uint8_t)kTileSuperUpper, prop, sx, sy);
    }
    draw_row(kSpriteMarioLowL, (uint8_t)(kTileSuperLowerFirst + (uint8_t)(anim_frame * kSuperTilesPerFrame)),
             prop, sx, (int16_t)(sy + kPlayerHeightPx));
}

uint16_t player_x(void) {
    return x_pos;
}

int16_t player_y(void) {
    return y_pos;
}

int8_t player_y_speed(void) {
    return y_speed;
}

// the same launch a jump takes, only the speed comes from the bible's stomp table instead of the
// tier's own impulse; the tier still follows his horizontal speed, so the hold rule works off it
void player_stomp_bounce(int8_t speed) {
    y_speed = speed;
    y_accum = 0;
    jump_origin_y = y_pos;
    jump_tier = tier_for(abs_speed());
    on_ground = 0;
}

uint8_t player_on_ground(void) {
    return on_ground;
}

uint8_t player_standing(void) {
    return (on_ground != 0U && x_speed == 0) ? 1U : 0U;
}

uint8_t player_facing_left(void) {
    return facing_left;
}

int16_t player_box_top(void) {
    return head_y();
}

int16_t player_feet(void) {
    return (int16_t)(y_pos + foot_h());
}

uint8_t player_box_height(void) {
    return (uint8_t)(foot_h() - (crouched != 0U ? kCrouchInsetPx : 0));
}

uint8_t player_riding(void) {
    return riding;
}
