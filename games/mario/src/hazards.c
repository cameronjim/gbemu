// the second banked engine module. bank 0 holds the rest of the engine and had a few hundred bytes
// left after m7, so m8a's lifts, firebars, fake bowser and axe ride in bank 5 beside powerup.c
#pragma bank 5

#include "hazards.h"

#include "assets.h"
#include "camera.h"
#include "level.h"
#include "mario.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

uint16_t hazard_lift_x[kLiftSlots];
int16_t hazard_lift_y[kLiftSlots];
int8_t hazard_lift_dx[kLiftSlots];
int8_t hazard_lift_dy[kLiftSlots];
uint8_t hazard_lift_count;
uint16_t hazard_min_x;
uint16_t hazard_max_x;
// the bar the contact pass picked this frame, so the draw does not walk the list a second time
static uint8_t live_bar = 0xFFU;

// a lift's own track and which way along it the deck is running
static uint16_t lift_min[kLiftSlots];
static uint16_t lift_max[kLiftSlots];
static int8_t lift_dir[kLiftSlots];
static uint8_t lift_vertical[kLiftSlots];

// the firebars, all spinning on one shared phase: physics.json names two raw rates but never says
// which bar takes which, so every bar here takes the slow one
static uint8_t bar_column[LEVEL_MAX_OBJECTS];
static uint8_t bar_row[LEVEL_MAX_OBJECTS];
static uint8_t bar_count;
static uint8_t spin_accum;
static uint8_t spin_step;

// a 32-step circle at an 8 px radius, so segment k of a bar sits k steps of this out from the
// pivot. values are round(8 * cos) and round(8 * sin) with step 0 pointing right
static const int8_t kSpinX[kFirebarSteps] = {8,  8,  7,  6,  6,  4,  3,  2,  0, -2, -3, -4, -6, -6, -7, -8,
                                             -8, -8, -7, -6, -6, -4, -3, -2, 0, 2,  3,  4,  6,  6,  7,  8};
static const int8_t kSpinY[kFirebarSteps] = {0, 2,  3,  4,  6,  6,  7,  8,  8,  8,  7,  6,  6,  4,  3,  2,
                                             0, -2, -3, -4, -6, -6, -7, -8, -8, -8, -7, -6, -6, -4, -3, -2};

// the fake bowser: roster.json calls him a goomba in disguise, so he walks the goomba's own speed
static uint16_t bowser_x;
static int16_t bowser_y;
static uint16_t bowser_min;
static uint16_t bowser_max;
static int8_t bowser_dir;
static uint8_t bowser_force;
static uint8_t bowser_live;

static uint16_t axe_x;
static int16_t axe_y;
static uint8_t axe_live;

// oam bookkeeping: the slots written last frame, so a quiet frame writes none at all. a flame and
// a lift plank never change tile or palette, so a slot already in use is only ever moved
static uint8_t flames_shown;
static uint8_t lift_sprites_shown;
static uint8_t bowser_shown;

// this frame's segment offsets, worked out once for every bar rather than once per bar: seven bars
// times six segments was forty two multiplies a frame on a budget that had none to give
static int8_t seg_x[kFirebarSegments + 1U];
static int8_t seg_y[kFirebarSegments + 1U];
static uint8_t seg_step = 0xFF;

static void note_segments(void) {
    const int8_t dx = kSpinX[spin_step];
    const int8_t dy = kSpinY[spin_step];
    uint8_t k;

    if (seg_step == spin_step) {
        return;
    }
    seg_step = spin_step;
    // k times the step, by adding rather than multiplying: sdcc calls __mulint for an int product
    // and twelve of those every six frames was the whole of the castle's missing budget
    seg_x[0] = 0;
    seg_y[0] = 0;
    for (k = 1; k <= (uint8_t)kFirebarSegments; ++k) {
        seg_x[k] = (int8_t)(seg_x[k - 1U] + dx);
        seg_y[k] = (int8_t)(seg_y[k - 1U] + dy);
    }
}

// the circle a whole bar sweeps, plus the widest box that could touch it
#define kBarReachPx (kFirebarSegments * kFirebarRadiusPx + kBlockPx)

// the bible spaces 1-4's bars six columns apart, so two of them can share the ten column screen -
// twelve flames, and the corridor was already the heaviest stretch the engine has. only the bar
// nearest the given world x is live: it is the only one drawn and the only one that can burn him,
// which keeps the two answers the same. its neighbour reaches at most to the midpoint between them,
// so the strip it goes cold over is a few pixels wide. must-verify against the rom pass
static uint16_t bar_centre_x(uint8_t i);
static int16_t bar_centre_y(uint8_t i);

static uint8_t nearest_bar(uint16_t px) {
    uint8_t best = 0xFFU;
    uint16_t closest = 0xFFFFU;
    uint8_t i;

    for (i = 0; i < bar_count; ++i) {
        const uint16_t cx = bar_centre_x(i);
        const uint16_t gap = (cx > px) ? (uint16_t)(cx - px) : (uint16_t)(px - cx);

        if (gap < closest) {
            closest = gap;
            best = i;
        }
    }
    return (closest <= (uint16_t)kBarReachPx) ? best : 0xFFU;
}

static uint8_t boxes_overlap(uint16_t ax, int16_t ay, uint8_t aw, uint8_t ah, uint16_t bx, int16_t by,
                             uint8_t bw, uint8_t bh) {
    if ((uint16_t)(ax + aw) <= bx || (uint16_t)(bx + bw) <= ax) {
        return 0;
    }
    return ((int16_t)(ay + ah) > by && (int16_t)(by + bh) > ay) ? 1U : 0U;
}

void hazards_load_level(void) BANKED {
    uint8_t i;
    uint8_t span;

    assets_load_hazard_tiles();
    hazard_lift_count = 0;
    hazard_min_x = 0xFFFFU;
    hazard_max_x = 0;
    bar_count = 0;
    bowser_live = 0;
    axe_live = 0;
    spin_accum = 0;
    spin_step = 0;
    seg_step = 0xFF;
    flames_shown = 0;
    lift_sprites_shown = 0;
    bowser_shown = 0;
    live_bar = 0xFFU;

    for (i = 0; i < level->object_count; ++i) {
        const uint16_t column = level->object_column[i];
        const uint8_t row = level->object_row[i];
        const uint8_t kind = level->object_kind[i];
        const uint8_t param = level->object_param[i];

        if (kind != (uint8_t)kObjPipe) {
            const uint16_t px = (uint16_t)(column << 4);
            const uint16_t far = (uint16_t)(px + ((uint16_t)(param & kLiftSpanMask) << 4));

            if (px < hazard_min_x) {
                hazard_min_x = px;
            }
            if (far > hazard_max_x) {
                hazard_max_x = far;
            }
        }
        if (kind == kObjFirebar) {
            if (bar_count < (uint8_t)LEVEL_MAX_OBJECTS) {
                bar_column[bar_count] = (uint8_t)column;
                bar_row[bar_count] = row;
                ++bar_count;
            }
        } else if (kind == kObjLiftH || kind == kObjLiftV) {
            const uint8_t slot = hazard_lift_count;

            if (slot >= (uint8_t)kLiftSlots) {
                continue;
            }
            span = (uint8_t)(param & kLiftSpanMask);
            lift_vertical[slot] = (kind == kObjLiftV) ? 1U : 0U;
            if (lift_vertical[slot] != 0U) {
                lift_min[slot] = (uint16_t)((uint16_t)row << 4);
                lift_max[slot] = (uint16_t)((uint16_t)(row + span) << 4);
                hazard_lift_x[slot] = (uint16_t)(column << 4);
            } else {
                lift_min[slot] = (uint16_t)(column << 4);
                lift_max[slot] = (uint16_t)((uint16_t)(column + span) << 4);
                hazard_lift_y[slot] = (int16_t)((int16_t)row << 4);
            }
            // the pair's second deck starts at the far end running the other way
            if ((param & kLiftReverse) != 0U) {
                lift_dir[slot] = -1;
                if (lift_vertical[slot] != 0U) {
                    hazard_lift_y[slot] = (int16_t)lift_max[slot];
                } else {
                    hazard_lift_x[slot] = lift_max[slot];
                }
            } else {
                lift_dir[slot] = 1;
                if (lift_vertical[slot] != 0U) {
                    hazard_lift_y[slot] = (int16_t)lift_min[slot];
                } else {
                    hazard_lift_x[slot] = lift_min[slot];
                }
            }
            hazard_lift_dx[slot] = 0;
            hazard_lift_dy[slot] = 0;
            ++hazard_lift_count;
        } else if (kind == kObjBowser) {
            bowser_live = 1;
            bowser_x = (uint16_t)(column << 4);
            bowser_y = (int16_t)((int16_t)row << 4);
            bowser_min = bowser_x;
            bowser_max = (uint16_t)((uint16_t)(column + param) << 4);
            bowser_dir = -1;
            bowser_force = 0;
        } else if (kind == kObjAxe) {
            axe_live = 1;
            axe_x = (uint16_t)(column << 4);
            axe_y = (int16_t)((int16_t)row << 4);
        }
    }
}

uint8_t hazards_count(void) BANKED {
    return (uint8_t)(hazard_lift_count + bar_count + bowser_live + axe_live);
}

static void step_lift(uint8_t i) {
    uint16_t pos = (lift_vertical[i] != 0U) ? (uint16_t)hazard_lift_y[i] : hazard_lift_x[i];
    const uint16_t before = pos;

    if (lift_dir[i] > 0) {
        pos = (uint16_t)(pos + kLiftSpeedPx);
        if (pos >= lift_max[i]) {
            pos = lift_max[i];
            lift_dir[i] = -1;
        }
    } else {
        pos = (uint16_t)(pos - kLiftSpeedPx);
        if (pos <= lift_min[i]) {
            pos = lift_min[i];
            lift_dir[i] = 1;
        }
    }
    if (lift_vertical[i] != 0U) {
        hazard_lift_y[i] = (int16_t)pos;
        hazard_lift_dy[i] = (int8_t)((int16_t)pos - (int16_t)before);
        hazard_lift_dx[i] = 0;
    } else {
        hazard_lift_x[i] = pos;
        hazard_lift_dx[i] = (int8_t)((int16_t)pos - (int16_t)before);
        hazard_lift_dy[i] = 0;
    }
}

static void step_bowser(void) {
    // the same half-pixel-a-frame accumulator a goomba walks on
    const uint16_t sum = (uint16_t)((uint16_t)bowser_force + 128U);

    bowser_force = (uint8_t)sum;
    if (sum <= 0xFFU) {
        return;
    }
    if (bowser_dir > 0) {
        ++bowser_x;
        if (bowser_x >= bowser_max) {
            bowser_x = bowser_max;
            bowser_dir = -1;
        }
    } else {
        --bowser_x;
        if (bowser_x <= bowser_min) {
            bowser_x = bowser_min;
            bowser_dir = 1;
        }
    }
}

void hazards_step(void) BANKED {
    uint8_t i;

    for (i = 0; i < hazard_lift_count; ++i) {
        step_lift(i);
    }
    if (bar_count != 0U) {
        const uint16_t sum = (uint16_t)((uint16_t)spin_accum + (uint16_t)kFirebarSpinRaw);

        spin_accum = (uint8_t)sum;
        if (sum > 0xFFU) {
            spin_step = (uint8_t)((spin_step + 1U) & (kFirebarSteps - 1U));
        }
    }
    if (bowser_live != 0U) {
        step_bowser();
    }
}

// the pivot cell's centre, which every segment measures out from
static uint16_t bar_centre_x(uint8_t i) {
    return (uint16_t)(((uint16_t)bar_column[i] << 4) + (kBlockPx / 2U));
}

static int16_t bar_centre_y(uint8_t i) {
    return (int16_t)(((int16_t)bar_row[i] << 4) + (kBlockPx / 2));
}

uint8_t hazards_contact(uint16_t player_px, int16_t player_py, uint8_t player_h, uint8_t immune) BANKED {
    const uint16_t left = (uint16_t)(player_px + kPlayerHitInsetPx);
    uint8_t i;
    uint8_t k;

    if (axe_live != 0U &&
        boxes_overlap(left, player_py, kPlayerHitWidthPx, player_h, axe_x, axe_y, kBlockPx, kBlockPx) != 0U) {
        return kHazardAxe;
    }
    if (immune != 0U) {
        return kHazardNone;
    }
    if (bowser_live != 0U && boxes_overlap(left, player_py, kPlayerHitWidthPx, player_h, bowser_x, bowser_y,
                                           kBowserWidthPx, kBowserHeightPx) != 0U) {
        return kHazardDamage;
    }
    i = nearest_bar((uint16_t)(left + (kPlayerHitWidthPx / 2U)));
    live_bar = i;
    if (i == 0xFFU) {
        return kHazardNone;
    }
    note_segments();
    {
        // the pivot's own corner, worked out once: the segment offsets are all that change per flame
        const int16_t bx = (int16_t)((int16_t)bar_centre_x(i) - (kFlamePx / 2));
        const int16_t by = (int16_t)(bar_centre_y(i) - (kFlamePx / 2));

        for (k = 1; k <= (uint8_t)kFirebarSegments; ++k) {
            const int16_t fx = (int16_t)(bx + seg_x[k]);
            const int16_t fy = (int16_t)(by + seg_y[k]);

            if (fx < 0) {
                continue;
            }
            if (boxes_overlap(left, player_py, kPlayerHitWidthPx, player_h, (uint16_t)fx, fy, kFlamePx,
                              kFlamePx) != 0U) {
                return kHazardDamage;
            }
        }
    }
    return kHazardNone;
}

void hazards_drop_bridge(void) BANKED {
    int16_t column;

    axe_live = 0;
    bowser_live = 0;
    for (column = (int16_t)level->bridge_x0; column <= (int16_t)level->bridge_x1; ++column) {
        terrain_clear_cell(column, (int16_t)level->bridge_row);
    }
}

uint8_t hazards_spin_step(void) BANKED {
    return spin_step;
}

uint8_t hazards_bowser_live(void) BANKED {
    return bowser_live;
}

static void park(uint8_t slot) {
    move_sprite(slot, 0, 0);
}

static void draw_lifts(uint16_t cam_x, uint8_t cam_y) {
    uint8_t drawn = 0;
    uint8_t i;
    uint8_t half;

    for (i = 0; i < hazard_lift_count; ++i) {
        const int16_t sx = (int16_t)((int16_t)hazard_lift_x[i] - (int16_t)cam_x);
        const int16_t sy = (int16_t)(hazard_lift_y[i] - (int16_t)cam_y);

        if (sy <= -kLiftDeckPx || sy >= (int16_t)kScreenHeightPx || sx <= -(int16_t)kLiftWidthPx ||
            sx >= (int16_t)kScreenWidthPx) {
            continue;
        }
        // a deck is 32 px of 8 px sprites; the pair's lower tile is blank so the plank reads 8 tall.
        // a slot already showing a plank keeps its tile and palette and is only moved
        for (half = 0; half < 4U; ++half) {
            const uint8_t slot = (uint8_t)(kSpriteLiftFirst + drawn);

            if (drawn >= lift_sprites_shown) {
                set_sprite_tile(slot, (uint8_t)kTileLiftDeck);
                set_sprite_prop(slot, (uint8_t)kPalGoomba);
            }
            move_sprite(slot, (uint8_t)(sx + (int16_t)(half * 8U) + kOamXOffset),
                        (uint8_t)(sy + kOamYOffset));
            ++drawn;
        }
    }
    for (i = drawn; i < lift_sprites_shown; ++i) {
        park((uint8_t)(kSpriteLiftFirst + i));
    }
    lift_sprites_shown = drawn;
}

static void draw_flames(uint16_t cam_x, uint8_t cam_y) {
    uint8_t drawn = 0;
    uint8_t i;
    uint8_t k;

    i = live_bar;
    if (i == 0xFFU) {
        for (i = drawn; i < flames_shown; ++i) {
            park((uint8_t)(kSpriteFlameFirst + i));
        }
        flames_shown = 0;
        return;
    }
    note_segments();
    {
        // the pivot's screen corner, camera and oam offset folded in once for the whole bar: the
        // flame loop is the heaviest thing on a castle frame and it was rebuilding this six times
        const int16_t bx = (int16_t)((int16_t)bar_centre_x(i) - (kFlamePx / 2) - (int16_t)cam_x);
        const int16_t by = (int16_t)(bar_centre_y(i) - (kFlamePx / 2) - (int16_t)cam_y);

        for (k = 1; k <= (uint8_t)kFirebarSegments && drawn < (uint8_t)kFlameSlots; ++k) {
            const int16_t fx = (int16_t)(bx + seg_x[k]);
            const int16_t fy = (int16_t)(by + seg_y[k]);
            uint8_t slot;

            if (fx <= -kFlamePx || fx >= (int16_t)kScreenWidthPx || fy <= -kFlamePx ||
                fy >= (int16_t)kScreenHeightPx) {
                continue;
            }
            slot = (uint8_t)(kSpriteFlameFirst + drawn);
            if (drawn >= flames_shown) {
                set_sprite_tile(slot, (uint8_t)kTileFlame);
                set_sprite_prop(slot, (uint8_t)kPalStar);
            }
            move_sprite(slot, (uint8_t)(fx + kOamXOffset), (uint8_t)(fy + kOamYOffset));
            ++drawn;
        }
    }
    for (i = drawn; i < flames_shown; ++i) {
        park((uint8_t)(kSpriteFlameFirst + i));
    }
    flames_shown = drawn;
}

void hazards_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    draw_lifts(cam_x, cam_y);
    draw_flames(cam_x, cam_y);

    if (bowser_live != 0U) {
        const int16_t sx = (int16_t)((int16_t)bowser_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(bowser_y - (int16_t)cam_y);

        if (sx > -(int16_t)kBowserWidthPx && sx < (int16_t)kScreenWidthPx && sy > -(int16_t)kBowserHeightPx &&
            sy < (int16_t)kScreenHeightPx) {
            set_sprite_tile((uint8_t)kSpriteBowser, (uint8_t)kTileBowser);
            set_sprite_tile((uint8_t)(kSpriteBowser + 1U), (uint8_t)kTileBowser);
            set_sprite_prop((uint8_t)kSpriteBowser, (uint8_t)kPalKoopa);
            set_sprite_prop((uint8_t)(kSpriteBowser + 1U), (uint8_t)(kPalKoopa | (uint8_t)S_FLIPX));
            move_sprite((uint8_t)kSpriteBowser, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
            move_sprite((uint8_t)(kSpriteBowser + 1U), (uint8_t)(sx + 8 + kOamXOffset),
                        (uint8_t)(sy + kOamYOffset));
            bowser_shown = 1;
            return;
        }
    }
    if (bowser_shown != 0U) {
        bowser_shown = 0;
        park((uint8_t)kSpriteBowser);
        park((uint8_t)(kSpriteBowser + 1U));
    }
}
