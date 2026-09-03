// the second banked engine module. bank 0 holds the rest of the engine and had a few hundred bytes
// left after m7, so m8a's lifts, firebars, fake bowser and axe were banked out of it.
//
// bank 3 rather than the bank 5 they started in, which m22 filled: every function this module
// publishes is BANKED and every function it calls is either bank 0's, gbdk's own or BANKED, so it
// is the one module in the set that can be moved at all. the rest of bank 5 - flow.c, title.c,
// hud.c, save.c, camera.c, powerup.c - publishes plain calls to plain functions in its neighbours,
// which land on the right bytes only because they are all in the one bank that is switched in
#pragma bank 3

#include "hazards.h"

#include "assets.h"
#include "camera.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
#include "player.h"
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
// the bars whose sweep can reach the view this frame, nearest the camera centre first: the draw
// hands its oam out in this order and the contact pass walks the whole list. worked out once per
// camera position, so the contact pass and the draw that follows it share the one answer
static uint8_t live_bars[LEVEL_MAX_OBJECTS];
// each one's distance to the camera centre, kept only to insert the next bar in the right place.
// a bar in view is at most 80 px plus its own reach from the centre, which is inside a byte
static uint8_t live_gap[LEVEL_MAX_OBJECTS];
static uint8_t live_count;
static uint16_t live_cam = 0xFFFFU;

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
// how many frames of the jaw throw's swoop are left: a dart that left the mouth sinks a px a frame
// for kBowserFireDropFrames, off the jaw's own height and onto the band a body on the deck fills.
// a zone dart is aimed at a row of mario's own to begin with and gets none
static uint8_t fire_drop;

static uint16_t axe_x;
static int16_t axe_y;
static uint8_t axe_live;

// the bridge coming apart: the cell the next drop takes, counting down from the axe end
static int16_t collapse_column;
static uint8_t collapse_timer;
uint8_t hazard_clear_busy;

// oam bookkeeping. slots kHazardPoolFirst..+kHazardPoolSlots (24-39) are one pool shared by the
// deck planks, bowser's body, his breath and a firebar's flames, and which of them holds a slot
// changes from frame to frame. so the pool carries an owner byte each: slot_owner is what wrote a
// slot's tile last, slot_claim is who wants it this frame. a claimant whose slot already held its
// own art only moves the sprite; one taking a slot off another owner writes tile and palette
// again; and a slot nobody claims is parked, which is what keeps a stale flame from lingering
enum { kOwnerNone = 0, kOwnerDeck, kOwnerBowser, kOwnerFire, kOwnerFlame };
static uint8_t slot_owner[kHazardPoolSlots];
static uint8_t slot_claim[kHazardPoolSlots];

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

static uint16_t bar_centre_x(uint8_t i);
static int16_t bar_centre_y(uint8_t i);

// how far out from its pivot a bar of this length reaches, plus the flame's own 8 px: a pivot this
// far outside the view can still put a flame on screen. it is the bar's own length and not the
// longest a level may hold, so a level's long bar does not wake its short ones up early
static uint16_t bar_reach(uint8_t i) {
    return (uint16_t)(((uint16_t)bar_segments(i) * kFirebarRadiusPx) + kFlamePx);
}

// every bar whose sweep can reach the view, in order of how near its pivot is to the camera
// centre. smb draws them all: a bar's rotation is the whole of its timing, so one that only wakes
// up when mario is under it cannot be jumped, and the pivot is on screen long before that. the
// phases are shared per rate (see hazards_step), so a bar going live costs nothing to step
static void note_live_bars(uint16_t cam_x) {
    const uint16_t centre = (uint16_t)(cam_x + (kScreenWidthPx / 2U));
    uint8_t i;

    if (live_cam == cam_x) {
        return;
    }
    live_cam = cam_x;
    live_count = 0;
    for (i = 0; i < bar_count; ++i) {
        const uint16_t reach = bar_reach(i);
        const uint16_t cx = bar_centre_x(i);
        uint8_t gap;
        uint8_t at;

        if ((uint16_t)(cx + reach) < cam_x || cx >= (uint16_t)(cam_x + kScreenWidthPx + reach)) {
            continue;
        }
        gap = (uint8_t)((cx > centre) ? (uint16_t)(cx - centre) : (uint16_t)(centre - cx));
        // an insertion sort over a handful of bars, so the nearest gets the first slots
        at = live_count;
        while (at != 0U && live_gap[at - 1U] > gap) {
            live_bars[at] = live_bars[at - 1U];
            live_gap[at] = live_gap[at - 1U];
            --at;
        }
        live_bars[at] = i;
        live_gap[at] = gap;
        ++live_count;
    }
}

// the box every test in this module is against - mario's, or a fireball's when it is his fireball
// asking. parked here rather than passed to each of the six calls: eight arguments a call was more
// of bank 5 than the whole module's arithmetic, and the left box never changes inside a pass
static uint16_t hit_x;
static int16_t hit_y;
static uint8_t hit_w;
static uint8_t hit_h;

static uint8_t hits_box(uint16_t bx, int16_t by, uint8_t bw, uint8_t bh) {
    if ((uint16_t)(hit_x + hit_w) <= bx || (uint16_t)(bx + bw) <= hit_x) {
        return 0;
    }
    return ((int16_t)(hit_y + hit_h) > by && (int16_t)(by + bh) > hit_y) ? 1U : 0U;
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
    // the gate main.c works off this has to be open over the whole of the fake bowser's fire zone,
    // which starts screens left of his own cell: so the floor starts there rather than at the
    // no-hazard sentinel, and the object scan below pulls it further left if anything is. a
    // level that compiled no zone carries 0xffff here, which is that sentinel exactly
    hazard_min_x = level->bowser_fire_x;
    hazard_max_x = 0;
    bar_count = 0;
    bowser_live = 0;
    axe_live = 0;
    spin_accum[0] = 0;
    spin_accum[1] = 0;
    spin_phase[0] = 0;
    spin_phase[1] = 0;
    seg_step = 0xFF;
    live_count = 0;
    live_cam = 0xFFFFU;
    collapse_column = -1;
    collapse_timer = 0;
    hazard_clear_busy = 0;
    fire_ttl = 0;

    // hazards_draw only runs once a hazard is back in range (kHazardMarginPx), so a level load far
    // from any of them would otherwise leave the last life's lift/flame/bowser sprites sitting in
    // oam forever. park the whole pool up front, regardless of hazard_near
    for (i = 0; i < (uint8_t)kHazardPoolSlots; ++i) {
        park((uint8_t)(kHazardPoolFirst + i));
        slot_owner[i] = (uint8_t)kOwnerNone;
        slot_claim[i] = (uint8_t)kOwnerNone;
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
    uint16_t edge;

    if (fire_ttl != 0U) {
        const uint16_t sum = (uint16_t)(fire_accum + (uint16_t)kBowserFireSubpx);

        fire_accum = (uint16_t)(sum & 0xFFU);
        if ((uint16_t)(sum >> 8) >= fire_x) {
            fire_ttl = 0;
            return;
        }
        fire_x = (uint16_t)(fire_x - (uint16_t)(sum >> 8));
        // and the swoop out of the jaw, which is what carries it down into a walker's band
        if (fire_drop != 0U) {
            --fire_drop;
            ++fire_y;
        }
        --fire_ttl;
        return;
    }
    ++fire_timer;
    if (fire_timer < (uint8_t)kBowserFireFrames) {
        return;
    }
    fire_timer = 0;
    fire_drop = 0;
    edge = (uint16_t)(camera_pos_x + kScreenWidthPx);
    if (bowser_x >= edge) {
        // the zone throw: while his body is still off the right edge the dart comes in at that
        // edge instead, on whichever screen of the zone the camera has reached. smb1's spawner
        // works the same way, and only hands the job back to his jaw once he is in view. the
        // level's own zone start, read straight out of the ram copy rather than kept in a
        // variable of its own - bank 5 had the bytes for neither
        if (camera_pos_x < level->bowser_fire_x) {
            return;
        }
        fire_x = edge;
        // the block row his feet stand in, or one of the three over it: kBowserFireZoneRowMask
        // off his own walk tick picks which, so the table of heights costs no state and no two
        // darts in a row come in at the same height. no clamp - fire_y is signed, and a dart over
        // the roof simply flies where nothing can be. edge is done with and carries the sum
        edge = (uint16_t)player_feet();
        edge = (uint16_t)(((edge - 1U) & 0xFFF0U) - (uint16_t)(bowser_tick & kBowserFireZoneRowMask));
        fire_y = (int16_t)(edge + kBowserFireZoneInsetPx);
    } else {
        // out of his jaw and to the left, which is the way the player always comes at him. the
        // dart's right edge starts at his mouth - kBowserJawPx into the box he faces left out of -
        // so it is seen to leave the jaw rather than appearing already clear of his body, and it
        // sinks from the jaw's height onto a walker's band over the swoop above
        if (bowser_x + (uint16_t)kBowserJawPx < (uint16_t)kBowserFireWidthPx) {
            return;
        }
        fire_x = (uint16_t)(bowser_x + (uint16_t)kBowserJawPx - (uint16_t)kBowserFireWidthPx);
        fire_y = (int16_t)(bowser_y + kBowserFireJawPx);
        fire_drop = (uint8_t)kBowserFireDropFrames;
    }
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
            if (bowser_live != 0U && bowser_falling == 0U && cell < (uint16_t)(bowser_x + kBowserWidthPx) &&
                (uint16_t)(cell + kBlockPx) > bowser_x) {
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
    uint8_t i;
    uint8_t k;

    hit_x = (uint16_t)(player_px + kPlayerHitInsetPx);
    hit_y = player_py;
    hit_w = (uint8_t)kPlayerHitWidthPx;
    hit_h = player_h;
    if (axe_live != 0U && hits_box(axe_x, axe_y, kBlockPx, kBlockPx) != 0U) {
        return kHazardAxe;
    }
    if (immune != 0U) {
        return kHazardNone;
    }
    if (bowser_live != 0U && bowser_falling == 0U &&
        hits_box(bowser_x, bowser_y, kBowserWidthPx, kBowserHeightPx) != 0U) {
        return kHazardDamage;
    }
    // his breath burns whether or not its sprites found oam to draw in
    if (fire_ttl != 0U && hits_box(fire_x, fire_y, kBowserFireWidthPx, kBowserFireHeightPx) != 0U) {
        return kHazardDamage;
    }
    // every live bar's whole segment list, however few of them the draw found oam for: what burns
    // him is never a question of what fitted in sprites
    note_live_bars(camera_pos_x);
    for (i = 0; i < live_count; ++i) {
        const uint8_t bar = live_bars[i];
        // the pivot's own corner, worked out once: the segment offsets are all that change per
        // flame. every call this needs is made before any of the arithmetic and its result parked
        // in a local: sdcc will otherwise keep an operand of the subtraction in a register across
        // one of them and read back whatever the callee left there (see draw_flames)
        uint8_t segments;
        uint16_t centre_x;
        int16_t centre_y;
        int16_t bx;
        int16_t by;

        note_segments(bar_step(bar));
        segments = bar_segments(bar);
        centre_x = bar_centre_x(bar);
        centre_y = bar_centre_y(bar);
        bx = (int16_t)((int16_t)centre_x - (kFlamePx / 2));
        by = (int16_t)(centre_y - (kFlamePx / 2));

        for (k = 1; k <= segments; ++k) {
            const int16_t fx = (int16_t)(bx + seg_x[k]);
            const int16_t fy = (int16_t)(by + seg_y[k]);

            if (fx < 0) {
                continue;
            }
            if (hits_box((uint16_t)fx, fy, kFlamePx, kFlamePx) != 0U) {
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
    hit_x = px;
    hit_y = py;
    hit_w = (uint8_t)kFireballPx;
    hit_h = (uint8_t)kFireballPx;
    if (hits_box(bowser_x, bowser_y, kBowserWidthPx, kBowserHeightPx) == 0U) {
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

// takes a pool slot for `owner` and says whether its tile and palette have to be written again: a
// slot keeps whatever art it already has for as long as its owner does not change
static uint8_t claim_slot(uint8_t slot, uint8_t owner) {
    const uint8_t at = (uint8_t)(slot - kHazardPoolFirst);
    const uint8_t fresh = (uint8_t)((slot_owner[at] != owner) ? 1U : 0U);

    slot_claim[at] = owner;
    return fresh;
}

static uint8_t slot_free(uint8_t slot) {
    return (uint8_t)((slot_claim[slot - kHazardPoolFirst] == (uint8_t)kOwnerNone) ? 1U : 0U);
}

// the end of the frame's oam pass: a slot that changed hands keeps its new owner, and one nobody
// claimed goes back to 0,0. this is what parks a bar's flames when it scrolls out of view or a
// nearer bar takes its slots, without any of the four claimants knowing about the others
static void settle_slots(void) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kHazardPoolSlots; ++i) {
        if (slot_claim[i] == (uint8_t)kOwnerNone) {
            if (slot_owner[i] != (uint8_t)kOwnerNone) {
                park((uint8_t)(kHazardPoolFirst + i));
                slot_owner[i] = (uint8_t)kOwnerNone;
            }
            continue;
        }
        slot_owner[i] = slot_claim[i];
        slot_claim[i] = (uint8_t)kOwnerNone;
    }
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
        // a deck is 32 px of 8 px sprites; the pair's lower tile is blank so the plank reads 8 tall
        for (half = 0; half < 4U; ++half) {
            const uint8_t slot = lift_slot(drawn);

            if (claim_slot(slot, (uint8_t)kOwnerDeck) != 0U) {
                set_sprite_tile(slot, (uint8_t)kTileLiftDeck);
                set_sprite_prop(slot, (uint8_t)kPalGoomba);
            }
            move_sprite(slot, (uint8_t)(sx + (int16_t)(half * 8U) + kOamXOffset),
                        (uint8_t)(sy + kOamYOffset));
            ++drawn;
        }
    }
}

// every live bar, nearest the camera centre first, into whatever the decks, bowser and his breath
// left of the pool. the cap is kFlameDrawCap - two full bars, which is what 1-4's pair four
// columns apart needs - and a third bar in view draws however many flames are still going rather
// than none, so it still reads as a hazard while the two in front of it hold their sprites
static void draw_flames(uint16_t cam_x, uint8_t cam_y) {
    uint8_t drawn = 0;
    uint8_t next = 0;
    uint8_t n;

    for (n = 0; n < live_count && drawn < (uint8_t)kFlameDrawCap; ++n) {
        const uint8_t bar = live_bars[n];
        // every call this pass needs, made before a single subtraction: the pivot's screen corner
        // used to be one expression with bar_centre_x() inside it, and sdcc kept cam_x in a
        // register across that call and then subtracted whatever the callee had left there - a
        // bar's flames all landed twenty thousand pixels off screen and none was ever drawn
        uint8_t segments;
        uint16_t centre_x;
        int16_t centre_y;
        int16_t bx;
        int16_t by;
        uint8_t on_bar = 0;
        uint8_t k;

        note_segments(bar_step(bar));
        segments = bar_segments(bar);
        centre_x = bar_centre_x(bar);
        centre_y = bar_centre_y(bar);
        // the pivot's screen corner, camera and oam offset folded in once for the whole bar: the
        // flame loop is the heaviest thing on a castle frame and it was rebuilding this six times
        bx = (int16_t)((int16_t)centre_x - (kFlamePx / 2) - (int16_t)cam_x);
        by = (int16_t)(centre_y - (kFlamePx / 2) - (int16_t)cam_y);

        for (k = 1; k <= segments && on_bar < (uint8_t)kFlameSlots && drawn < (uint8_t)kFlameDrawCap; ++k) {
            const int16_t fx = (int16_t)(bx + seg_x[k]);
            const int16_t fy = (int16_t)(by + seg_y[k]);
            uint8_t slot;

            if (fx <= -kFlamePx || fx >= (int16_t)kScreenWidthPx || fy <= -kFlamePx ||
                fy >= (int16_t)kScreenHeightPx) {
                continue;
            }
            while (next < (uint8_t)kHazardPoolSlots && slot_claim[next] != (uint8_t)kOwnerNone) {
                ++next;
            }
            if (next >= (uint8_t)kHazardPoolSlots) {
                return;
            }
            slot = (uint8_t)(kHazardPoolFirst + next);
            ++next;
            if (claim_slot(slot, (uint8_t)kOwnerFlame) != 0U) {
                set_sprite_tile(slot, (uint8_t)kTileFlame);
                set_sprite_prop(slot, (uint8_t)kPalStar);
            }
            move_sprite(slot, (uint8_t)(fx + kOamXOffset), (uint8_t)(fy + kOamYOffset));
            ++drawn;
            ++on_bar;
        }
    }
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
    // the tell: over the last stretch of the wait for a throw his head's own sprite swaps for the
    // open-jaw pair, which is smb's half-second of mouth before the flame. read off fire_timer, so
    // it costs no state - and it is only up while there is a throw coming, not during a flight
    const uint8_t jaw =
        (uint8_t)(fire_ttl == 0U && fire_timer + (uint8_t)kBowserJawOpenFrames >= (uint8_t)kBowserFireFrames);
    uint8_t i;

    // one run of eight rather than two of four: the low two bits of the index are the column and
    // the third is the row, which costs two masks where the pair of loops cost a multiply a sprite
    for (i = 0; i < 8U; ++i) {
        const uint8_t slot = (uint8_t)(kSpriteBowserFirst + i);

        // his tile changes with the walk frame, so this one always writes rather than asking
        (void)claim_slot(slot, (uint8_t)kOwnerBowser);
        set_sprite_tile(slot,
                        i == 0U && jaw != 0U ? (uint8_t)kTileBowserJaw : (uint8_t)(base + (uint8_t)(i << 1)));
        set_sprite_prop(slot, prop);
        move_sprite(slot, (uint8_t)(sx + (int16_t)((uint16_t)(i & 3U) << 3) + kOamXOffset),
                    (uint8_t)(sy + (int16_t)((uint16_t)(i >> 2) << 4) + kOamYOffset));
    }
}

// the breath, three 8x16 sprites off the top of the pool. draw_lifts has already claimed this
// frame, so the test is against the decks actually on screen rather than the ones the level
// loaded: 1-4 carries two lifts a hundred and thirty columns apart and never shows both, so the
// breath always has its slots. on a frame that did show two the dart still flies and still burns,
// it is only not drawn
static void draw_bowser_fire(uint16_t cam_x, uint8_t cam_y) {
    int16_t sx;
    int16_t sy;
    uint8_t i;

    if (fire_ttl == 0U) {
        return;
    }
    for (i = 0; i < (uint8_t)kSpriteBowserFireCount; ++i) {
        if (slot_free((uint8_t)(kSpriteBowserFireFirst + i)) == 0U) {
            return;
        }
    }
    sx = (int16_t)((int16_t)fire_x - (int16_t)cam_x);
    sy = (int16_t)(fire_y - (int16_t)cam_y);
    if (sx <= -(int16_t)kBowserFireWidthPx || sx >= (int16_t)kScreenWidthPx || sy <= -8 ||
        sy >= (int16_t)kScreenHeightPx) {
        return;
    }
    for (i = 0; i < (uint8_t)kSpriteBowserFireCount; ++i) {
        const uint8_t slot = (uint8_t)(kSpriteBowserFireFirst + i);

        if (claim_slot(slot, (uint8_t)kOwnerFire) != 0U) {
            set_sprite_tile(slot, (uint8_t)(kTileBowserFire + (i << 1)));
            set_sprite_prop(slot, (uint8_t)((uint8_t)kPalStar | (uint8_t)S_BANK));
        }
        move_sprite(slot, (uint8_t)(sx + (int16_t)((uint16_t)i << 3) + kOamXOffset),
                    (uint8_t)(sy + kOamYOffset));
    }
}

// the frame's whole oam pass over the shared pool, claimants in priority order: the decks first,
// then bowser's body, then his breath, and the flames into whatever is left. he beats a bar for
// the front of the pool because a bowser drawn as six of his sixteen tiles would read as nothing
// at all, where a bar drawing four flames instead of six still reads as a bar - and smb never
// puts the two in one room anyway. settle_slots then parks whatever nobody took
void hazards_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    draw_lifts(cam_x, cam_y);
    if (bowser_live != 0U) {
        const int16_t sx = (int16_t)((int16_t)bowser_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(bowser_y - (int16_t)cam_y);

        if (sx > -(int16_t)kBowserWidthPx && sx < (int16_t)kScreenWidthPx && sy > -(int16_t)kBowserHeightPx &&
            sy < (int16_t)kScreenHeightPx) {
            draw_bowser(sx, sy);
        }
        draw_bowser_fire(cam_x, cam_y);
    }
    note_live_bars(cam_x);
    draw_flames(cam_x, cam_y);
    settle_slots();
}
