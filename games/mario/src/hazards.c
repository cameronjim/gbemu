// the second banked engine module. bank 0 holds the rest of the engine and had a few hundred bytes
// left after m7, so m8a's lifts, firebars, fake bowser and axe ride in bank 5 beside powerup.c
#pragma bank 5

#include "hazards.h"

#include "assets.h"
#include "camera.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
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

// the firebars. physics.json names two raw rates and smbdis a direction per variant, and the
// level says which of each a bar takes in its object_param (kFirebarParam* in mario.h). the two
// rates share one phase each rather than one per bar: a bar's angle is only ever a function of
// its rate and its handedness, so two accumulators cover every bar a level can hold
static uint8_t bar_column[LEVEL_MAX_OBJECTS];
static uint8_t bar_row[LEVEL_MAX_OBJECTS];
// the packed param, kept whole: the segment count is a mask away and the two flags are one bit
// each, which is cheaper than three arrays of eleven bytes
static uint8_t bar_param[LEVEL_MAX_OBJECTS];
static uint8_t bar_count;
static uint8_t spin_accum[2];
static uint8_t spin_phase[2];

// a 32-step circle at an 8 px radius, so segment k of a bar sits k steps of this out from the
// pivot. values are round(8 * cos) and round(8 * sin) with step 0 pointing right
static const int8_t kSpinX[kFirebarSteps] = {8,  8,  7,  6,  6,  4,  3,  2,  0, -2, -3, -4, -6, -6, -7, -8,
                                             -8, -8, -7, -6, -6, -4, -3, -2, 0, 2,  3,  4,  6,  6,  7,  8};
static const int8_t kSpinY[kFirebarSteps] = {0, 2,  3,  4,  6,  6,  7,  8,  8,  8,  7,  6,  6,  4,  3,  2,
                                             0, -2, -3, -4, -6, -6, -7, -8, -8, -8, -7, -6, -6, -4, -3, -2};

// the fake bowser. roster.json calls him a goomba in disguise, so he walks the goomba's own speed;
// everything else about him is m20's - a 32x32 body over two frames, an occasional hop off the
// bridge deck, a 24x8 dart of fire thrown left every couple of seconds, and five fireballs or the
// axe to put him down
static uint16_t bowser_x;
static int16_t bowser_y;
// the deck line his feet come back to after a hop, so the hop needs no floor probe of its own
static int16_t bowser_deck_y;
static uint16_t bowser_min;
static uint16_t bowser_max;
static int8_t bowser_dir;
static uint8_t bowser_force;
static uint8_t bowser_live;
static uint8_t bowser_hits;
static uint8_t bowser_tick;
static uint8_t bowser_frame;
static uint8_t bowser_hop_timer;
static int8_t bowser_dy;
static uint8_t bowser_airborne;
// 1 from the moment the bridge under him goes, or the fifth fireball lands, until he is off screen
static uint8_t bowser_falling;
static uint8_t fire_timer;
static uint8_t fire_ttl;
static uint16_t fire_x;
static int16_t fire_y;
static uint16_t fire_accum;

static uint16_t axe_x;
static int16_t axe_y;
static uint8_t axe_live;

// the bridge coming apart: the cell the next drop takes, counting down from the axe end
static int16_t collapse_column;
static uint8_t collapse_timer;
uint8_t hazard_clear_busy;

// oam bookkeeping: the slots written last frame, so a quiet frame writes none at all. a flame and
// a lift plank never change tile or palette, so a slot already in use is only ever moved
static uint8_t flames_shown;
static uint8_t lift_sprites_shown;
static uint8_t bowser_shown;
static uint8_t fire_shown;

// this frame's segment offsets, worked out once for every bar rather than once per bar: seven bars
// times six segments was forty two multiplies a frame on a budget that had none to give
static int8_t seg_x[kFirebarSegmentsMax + 1U];
static int8_t seg_y[kFirebarSegmentsMax + 1U];
static uint8_t seg_step = 0xFF;

// how many segments a bar carries, and which of the two shared phases it turns on. a param of zero
// is the short bar, so a level compiled before the param contract keeps the six it always had
static uint8_t bar_segments(uint8_t i) {
    const uint8_t n = (uint8_t)(bar_param[i] & kFirebarParamSegMask);

    if (n == 0U) {
        return (uint8_t)kFirebarSegments;
    }
    return (n > (uint8_t)kFirebarSegmentsMax) ? (uint8_t)kFirebarSegmentsMax : n;
}

// a counter-clockwise bar reads the same table backwards, which costs a subtract rather than a
// second pair of sine tables
static uint8_t bar_step(uint8_t i) {
    const uint8_t phase = spin_phase[(bar_param[i] & kFirebarParamFast) != 0U ? 1U : 0U];

    if ((bar_param[i] & kFirebarParamCcw) == 0U) {
        return phase;
    }
    return (uint8_t)((uint8_t)(kFirebarSteps - phase) & (uint8_t)(kFirebarSteps - 1U));
}

static void note_segments(uint8_t step) {
    const int8_t dx = kSpinX[step];
    const int8_t dy = kSpinY[step];
    uint8_t k;

    if (seg_step == step) {
        return;
    }
    seg_step = step;
    // k times the step, by adding rather than multiplying: sdcc calls __mulint for an int product
    // and twelve of those every six frames was the whole of the castle's missing budget
    seg_x[0] = 0;
    seg_y[0] = 0;
    for (k = 1; k <= (uint8_t)kFirebarSegmentsMax; ++k) {
        seg_x[k] = (int8_t)(seg_x[k - 1U] + dx);
        seg_y[k] = (int8_t)(seg_y[k - 1U] + dy);
    }
}

// the circle a bar of the given length sweeps, plus the widest box that could touch it. it is the
// chosen bar's own length and not the longest a level may hold: a bar only ever goes live where it
// could actually burn him, so a level's long bar does not wake its short ones up early
#define kBarReachPx(segments) ((uint16_t)((segments) * kFirebarRadiusPx + kBlockPx))

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
    if (best == 0xFFU) {
        return 0xFFU;
    }
    return (closest <= kBarReachPx(bar_segments(best))) ? best : 0xFFU;
}

static uint8_t boxes_overlap(uint16_t ax, int16_t ay, uint8_t aw, uint8_t ah, uint16_t bx, int16_t by,
                             uint8_t bw, uint8_t bh) {
    if ((uint16_t)(ax + aw) <= bx || (uint16_t)(bx + bw) <= ax) {
        return 0;
    }
    return ((int16_t)(ay + ah) > by && (int16_t)(by + bh) > ay) ? 1U : 0U;
}

static void park(uint8_t slot);

// the oam slot one of a deck's four sprites takes. the first two decks own kSpriteLiftFirst; the
// third's four come out of the flame/bowser run, which a level carrying one has to leave idle
static uint8_t lift_slot(uint8_t sprite) {
    return (sprite < (uint8_t)kSpriteLiftCount)
               ? (uint8_t)(kSpriteLiftFirst + sprite)
               : (uint8_t)(kSpriteLiftOverflowFirst + sprite - (uint8_t)kSpriteLiftCount);
}

void hazards_load_level(void) BANKED {
    uint8_t i;
    uint8_t span;
    uint8_t lift_cap = (uint8_t)kLiftSlots;

    assets_load_hazard_tiles();
    hazard_lift_count = 0;
    hazard_min_x = 0xFFFFU;
    hazard_max_x = 0;
    bar_count = 0;
    bowser_live = 0;
    axe_live = 0;
    spin_accum[0] = 0;
    spin_accum[1] = 0;
    spin_phase[0] = 0;
    spin_phase[1] = 0;
    seg_step = 0xFF;
    flames_shown = 0;
    lift_sprites_shown = 0;
    bowser_shown = 0;
    fire_shown = 0;
    live_bar = 0xFFU;
    collapse_column = -1;
    collapse_timer = 0;
    hazard_clear_busy = 0;
    fire_ttl = 0;

    // hazards_draw only runs once a hazard is back in range (kHazardMarginPx), so a level load far
    // from any of them would otherwise leave the last life's lift/flame/bowser sprites sitting in
    // oam forever. park every slot this module owns up front, regardless of hazard_near
    for (i = 0; i < (uint8_t)(kLiftSlots * 4U); ++i) {
        park(lift_slot(i));
    }
    // the pool bowser's body takes covers the flame run and the two slots past it, so one loop
    // over kSpriteBowserCount parks whichever of the two the last life left drawn
    for (i = 0; i < (uint8_t)kSpriteBowserCount; ++i) {
        park((uint8_t)(kSpriteBowserFirst + i));
    }

    // the borrowed slots belong to the bar and the bowser first, so a level carrying either keeps
    // only the two decks that have oam of their own. compile_level.py rejects a level that would
    // hit this, so it never silently drops a deck a real map draws
    for (i = 0; i < level->object_count; ++i) {
        const uint8_t look = level->object_kind[i];

        if (look == (uint8_t)kObjFirebar || look == (uint8_t)kObjBowser) {
            lift_cap = (uint8_t)kLiftSlotsShared;
            break;
        }
    }

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
                bar_param[bar_count] = param;
                ++bar_count;
            }
        } else if (kind == kObjLiftH || kind == kObjLiftV) {
            const uint8_t slot = hazard_lift_count;

            if (slot >= lift_cap) {
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
            // the level puts his object row one above the deck, which stood a 16x16 body on it. a
            // 32x32 one starts a block row higher, so its feet land on the same deck line
            bowser_y = (int16_t)(((int16_t)row - 1) << 4);
            bowser_deck_y = bowser_y;
            bowser_min = bowser_x;
            bowser_max = (uint16_t)((uint16_t)(column + param) << 4);
            bowser_dir = -1;
            bowser_force = 0;
            bowser_hits = 0;
            bowser_tick = 0;
            bowser_frame = 0;
            bowser_hop_timer = 0;
            bowser_dy = 0;
            bowser_airborne = 0;
            bowser_falling = 0;
            fire_timer = 0;
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

// the fall the axe and the fifth fireball both end in: the death beat's own ramp, not the physics
// accumulator, because bank 0 has no room for a third gravity path and nothing about it is sourced
static void step_bowser_fall(void) {
    ++bowser_tick;
    if ((bowser_tick & kBowserHopGravityMask) == 0U && bowser_dy < (int8_t)kBowserMaxFallPx) {
        bowser_dy = (int8_t)(bowser_dy + 1);
    }
    bowser_y = (int16_t)(bowser_y + bowser_dy);
    if (bowser_y > (int16_t)(kLevelHeightPx + kBowserHeightPx)) {
        bowser_live = 0;
    }
}

static void step_bowser_fire(void) {
    if (fire_ttl != 0U) {
        const uint16_t sum = (uint16_t)(fire_accum + (uint16_t)kBowserFireSubpx);

        fire_accum = (uint16_t)(sum & 0xFFU);
        if ((uint16_t)(sum >> 8) >= fire_x) {
            fire_ttl = 0;
            return;
        }
        fire_x = (uint16_t)(fire_x - (uint16_t)(sum >> 8));
        --fire_ttl;
        return;
    }
    ++fire_timer;
    if (fire_timer < (uint8_t)kBowserFireFrames) {
        return;
    }
    fire_timer = 0;
    // out of his jaw and to the left, which is the way the player always comes at him
    if (bowser_x < (uint16_t)kBowserFireWidthPx) {
        return;
    }
    fire_x = (uint16_t)(bowser_x - kBowserFireWidthPx);
    fire_y = (int16_t)(bowser_y + kBowserFireJawPx);
    fire_accum = 0;
    fire_ttl = (uint8_t)kBowserFireLifeFrames;
}

static void step_bowser(void) {
    uint16_t sum;

    if (bowser_falling != 0U) {
        step_bowser_fall();
        return;
    }
    ++bowser_tick;
    if ((bowser_tick & (uint8_t)(kBowserAnimFrames - 1U)) == 0U) {
        bowser_frame = (uint8_t)(bowser_frame ^ 1U);
    }
    // the same half-pixel-a-frame accumulator a goomba walks on
    sum = (uint16_t)((uint16_t)bowser_force + (uint16_t)kBowserWalkSubpx);
    bowser_force = (uint8_t)sum;
    if (sum > 0xFFU) {
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
    // the hop, on his own beat rather than at the player: smb's bowser jumps whether or not mario
    // is under him, which is what makes the bridge hard to cross rather than a timing puzzle
    if (bowser_airborne == 0U) {
        ++bowser_hop_timer;
        if (bowser_hop_timer >= (uint8_t)kBowserHopFrames) {
            bowser_hop_timer = 0;
            bowser_airborne = 1;
            bowser_dy = (int8_t)kBowserHopLaunchPx;
        }
    } else {
        if ((bowser_tick & kBowserHopGravityMask) == 0U && bowser_dy < (int8_t)kBowserMaxFallPx) {
            bowser_dy = (int8_t)(bowser_dy + 1);
        }
        bowser_y = (int16_t)(bowser_y + bowser_dy);
        if (bowser_dy > 0 && bowser_y >= bowser_deck_y) {
            bowser_y = bowser_deck_y;
            bowser_dy = 0;
            bowser_airborne = 0;
        }
    }
    step_bowser_fire();
}

void hazards_step(void) BANKED {
    uint8_t i;

    for (i = 0; i < hazard_lift_count; ++i) {
        step_lift(i);
    }
    if (bar_count != 0U) {
        // both phases turn every frame a bar is loaded, whichever rates the level actually uses:
        // two adds is cheaper than asking which of them any bar wants
        uint16_t sum = (uint16_t)((uint16_t)spin_accum[0] + (uint16_t)kFirebarSpinRaw);

        spin_accum[0] = (uint8_t)sum;
        if (sum > 0xFFU) {
            spin_phase[0] = (uint8_t)((spin_phase[0] + 1U) & (kFirebarSteps - 1U));
        }
        sum = (uint16_t)((uint16_t)spin_accum[1] + (uint16_t)kFirebarSpinFastRaw);
        spin_accum[1] = (uint8_t)sum;
        if (sum > 0xFFU) {
            spin_phase[1] = (uint8_t)((spin_phase[1] + 1U) & (kFirebarSteps - 1U));
        }
    }
    if (bowser_live != 0U) {
        step_bowser();
    }
}

// the frames after the axe, when the play loop has already handed over to the clear sequence and
// nothing is stepping bank 5 any more: the bridge is still coming apart a cell at a time and
// bowser is still on his way into the lava. states.c calls this while hazard_clear_busy stands
void hazards_clear_step(void) BANKED {
    if (collapse_column >= (int16_t)level->bridge_x0) {
        ++collapse_timer;
        if (collapse_timer >= (uint8_t)kBridgeDropFrames) {
            const uint16_t cell = (uint16_t)((uint16_t)collapse_column << 4);

            collapse_timer = 0;
            terrain_clear_cell(collapse_column, (int16_t)level->bridge_row);
            // the cell that goes out from under him is what drops him, exactly as in smb: the
            // collapse runs back from the axe and he is standing somewhere along its way
            if (bowser_live != 0U && bowser_falling == 0U &&
                cell < (uint16_t)(bowser_x + kBowserWidthPx) && (uint16_t)(cell + kBlockPx) > bowser_x) {
                bowser_falling = 1;
                bowser_dy = 0;
            }
            --collapse_column;
        }
    }
    if (bowser_live != 0U && bowser_falling != 0U) {
        step_bowser_fall();
    }
    hazard_clear_busy = (uint8_t)((collapse_column >= (int16_t)level->bridge_x0 ||
                                   (bowser_live != 0U && bowser_falling != 0U))
                                      ? 1U
                                      : 0U);
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
    if (bowser_live != 0U && bowser_falling == 0U &&
        boxes_overlap(left, player_py, kPlayerHitWidthPx, player_h, bowser_x, bowser_y, kBowserWidthPx,
                      kBowserHeightPx) != 0U) {
        return kHazardDamage;
    }
    // his breath burns whether or not its sprites found oam to draw in
    if (fire_ttl != 0U && boxes_overlap(left, player_py, kPlayerHitWidthPx, player_h, fire_x, fire_y,
                                        kBowserFireWidthPx, kBowserFireHeightPx) != 0U) {
        return kHazardDamage;
    }
    i = nearest_bar((uint16_t)(left + (kPlayerHitWidthPx / 2U)));
    live_bar = i;
    if (i == 0xFFU) {
        return kHazardNone;
    }
    note_segments(bar_step(i));
    {
        // the pivot's own corner, worked out once: the segment offsets are all that change per
        // flame. every call this needs is made before any of the arithmetic and its result parked
        // in a local: sdcc will otherwise keep an operand of the subtraction in a register across
        // one of them and read back whatever the callee left there (see draw_flames)
        const uint8_t segments = bar_segments(i);
        const uint16_t centre_x = bar_centre_x(i);
        const int16_t centre_y = bar_centre_y(i);
        const int16_t bx = (int16_t)((int16_t)centre_x - (kFlamePx / 2));
        const int16_t by = (int16_t)(centre_y - (kFlamePx / 2));

        for (k = 1; k <= segments; ++k) {
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

// roster.json: five fireballs defeat him and pay 5000, and the axe works either way afterwards.
// every ball that lands on him is spent, hit or kill, which is what the return value says
uint8_t hazards_fireball_hit(uint16_t px, int16_t py) BANKED {
    if (bowser_live == 0U || bowser_falling != 0U) {
        return 0;
    }
    if (boxes_overlap(px, py, kFireballPx, kFireballPx, bowser_x, bowser_y, kBowserWidthPx,
                      kBowserHeightPx) == 0U) {
        return 0;
    }
    ++bowser_hits;
    if (bowser_hits >= (uint8_t)kBowserFireballHits) {
        hud_score = (uint16_t)(hud_score + kScoreTens(kBowserKillPoints));
        bowser_falling = 1;
        bowser_dy = 0;
        fire_ttl = 0;
    }
    return 1;
}

// smb does not drop the whole span at once: it pulls one cell at a time from the axe end back
// toward the far side over about a second, and bowser goes down with the cell he is standing on.
// so this only arms the collapse - hazards_clear_step walks it, one cell every kBridgeDropFrames
void hazards_drop_bridge(void) BANKED {
    axe_live = 0;
    fire_ttl = 0;
    collapse_column = (int16_t)level->bridge_x1;
    collapse_timer = 0;
    hazard_clear_busy = 1;
}

uint8_t hazards_spin_step(void) BANKED {
    return spin_phase[0];
}

uint8_t hazards_bowser_live(void) BANKED {
    return bowser_live;
}

uint8_t hazards_bowser_hits(void) BANKED {
    return bowser_hits;
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
            const uint8_t slot = lift_slot(drawn);

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
        park(lift_slot(i));
    }
    lift_sprites_shown = drawn;
}

static void draw_flames(uint16_t cam_x, uint8_t cam_y) {
    uint8_t drawn = 0;
    uint8_t i = live_bar;
    uint8_t k;
    uint8_t segments;
    uint16_t centre_x;
    int16_t centre_y;
    int16_t bx;
    int16_t by;

    if (i == 0xFFU) {
        for (i = 0; i < flames_shown; ++i) {
            park((uint8_t)(kSpriteFlameFirst + i));
        }
        flames_shown = 0;
        return;
    }
    // every call this pass needs, made before a single subtraction: the pivot's screen corner used
    // to be one expression with bar_centre_x() inside it, and sdcc kept cam_x in a register across
    // that call and then subtracted whatever the callee had left there - a bar's flames all landed
    // twenty thousand pixels off screen and none of them was ever drawn or ever burned him
    note_segments(bar_step(i));
    segments = bar_segments(i);
    centre_x = bar_centre_x(i);
    centre_y = bar_centre_y(i);
    // the pivot's screen corner, camera and oam offset folded in once for the whole bar: the flame
    // loop is the heaviest thing on a castle frame and it was rebuilding this six times
    bx = (int16_t)((int16_t)centre_x - (kFlamePx / 2) - (int16_t)cam_x);
    by = (int16_t)(centre_y - (kFlamePx / 2) - (int16_t)cam_y);

    for (k = 1; k <= segments && drawn < (uint8_t)kFlameSlots; ++k) {
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
    for (i = drawn; i < flames_shown; ++i) {
        park((uint8_t)(kSpriteFlameFirst + i));
    }
    flames_shown = drawn;
}

// his 32x32 body: two rows of four 8x16 sprites, so a scanline crossing him draws four - mario's
// two and the hud's none leave that well inside the ten the hardware manages. the art is in vram
// bank 1, hence S_BANK on every one of them.
//
// he is drawn facing left whichever way he is walking, which is smb's own bowser: he never turns
// round on the bridge, he paces backwards. that is also why the art needs no mirror pass
static void draw_bowser(int16_t sx, int16_t sy) {
    const uint8_t base = (uint8_t)(kTileBowserFirst + (bowser_frame != 0U ? kBowserTilesPerFrame : 0U));
    const uint8_t prop = (uint8_t)((uint8_t)kPalKoopa | (uint8_t)S_BANK);
    uint8_t r;
    uint8_t c;

    for (r = 0; r < 2U; ++r) {
        for (c = 0; c < 4U; ++c) {
            const uint8_t slot = (uint8_t)(kSpriteBowserFirst + (r << 2) + c);

            set_sprite_tile(slot, (uint8_t)(base + (((r << 2) + c) << 1)));
            set_sprite_prop(slot, prop);
            move_sprite(slot, (uint8_t)(sx + (int16_t)((uint16_t)c << 3) + kOamXOffset),
                        (uint8_t)(sy + (int16_t)((uint16_t)r << 4) + kOamYOffset));
        }
    }
}

static void park_bowser(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kSpriteBowserCount; ++i) {
        park((uint8_t)(kSpriteBowserFirst + i));
    }
}

// the breath, three 8x16 sprites off the top of the lift run. draw_lifts has already run this
// frame and published how many deck sprites it took, so the test is against the decks actually on
// screen rather than the ones the level loaded: 1-4 carries two lifts a hundred and thirty columns
// apart and never shows both, so the breath always has its slots. on a frame that did show two the
// dart still flies and still burns, it is only not drawn
static void draw_bowser_fire(uint16_t cam_x, uint8_t cam_y) {
    uint8_t drawn = 0;
    uint8_t i;

    if (fire_ttl != 0U && lift_sprites_shown <= (uint8_t)kSpriteLiftPerDeck) {
        const int16_t sx = (int16_t)((int16_t)fire_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(fire_y - (int16_t)cam_y);

        if (sx > -(int16_t)kBowserFireWidthPx && sx < (int16_t)kScreenWidthPx && sy > -8 &&
            sy < (int16_t)kScreenHeightPx) {
            for (i = 0; i < (uint8_t)kSpriteBowserFireCount; ++i) {
                const uint8_t slot = (uint8_t)(kSpriteBowserFireFirst + i);

                if (i >= fire_shown) {
                    set_sprite_tile(slot, (uint8_t)(kTileBowserFire + (i << 1)));
                    set_sprite_prop(slot, (uint8_t)((uint8_t)kPalStar | (uint8_t)S_BANK));
                }
                move_sprite(slot, (uint8_t)(sx + (int16_t)((uint16_t)i << 3) + kOamXOffset),
                            (uint8_t)(sy + kOamYOffset));
            }
            drawn = (uint8_t)kSpriteBowserFireCount;
        }
    }
    // a frame that took the breath's slots for a second deck must not park them again
    if (lift_sprites_shown <= (uint8_t)kSpriteLiftPerDeck) {
        for (i = drawn; i < fire_shown; ++i) {
            park((uint8_t)(kSpriteBowserFireFirst + i));
        }
    }
    fire_shown = drawn;
}

void hazards_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    uint8_t body = 0;

    draw_lifts(cam_x, cam_y);
    if (bowser_live != 0U) {
        const int16_t sx = (int16_t)((int16_t)bowser_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(bowser_y - (int16_t)cam_y);

        if (sx > -(int16_t)kBowserWidthPx && sx < (int16_t)kScreenWidthPx && sy > -(int16_t)kBowserHeightPx &&
            sy < (int16_t)kScreenHeightPx) {
            draw_bowser(sx, sy);
            body = 1;
        }
    }
    // he and a firebar share one pool of eight slots and smb never puts them in the same room, so
    // the frame simply gives it to whichever of the two is on screen. he wins a tie: a bowser
    // drawn as six of his sixteen tiles would read as nothing at all, where a bar losing its
    // flames for a few frames only loses the flames
    if (body != 0U) {
        if (flames_shown != 0U) {
            flames_shown = 0;
        }
        bowser_shown = 1;
    } else {
        if (bowser_shown != 0U) {
            bowser_shown = 0;
            park_bowser();
        }
        draw_flames(cam_x, cam_y);
    }
    draw_bowser_fire(cam_x, cam_y);
}
