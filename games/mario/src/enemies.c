// the engine module that pays the banking cost: bank 0 is full of the rest of the engine, and
// this one is entered twice a frame, so two trampolines is the whole price
#pragma bank 4

#include "enemies.h"

#include "assets.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "physics_constants.h"
#include "popup.h"
#include "sound.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

// one pool slot. exactly 16 bytes, so sdcc reaches a slot with a shift instead of a multiply, and
// every routine below takes the pointer once rather than re-indexing per field - the same reason
// blocks.c keeps its hot lookups in row tables
typedef struct {
    uint8_t state;
    uint8_t kind;
    int8_t dir;
    uint8_t grounded;
    uint8_t x_force;
    uint8_t y_accum;
    int8_t dy;
    // the columns the last terrain probe answered for. a walker crosses one every 32 frames, so
    // gating the wall and ledge probes on these leaves a quiet enemy costing pure arithmetic
    uint8_t lead_col;
    uint8_t foot_col;
    // frames a shell ignores him for: the stomp that made it leaves him overlapping it for a few,
    // and a kick would otherwise send it off and kill him with the same touch
    uint8_t grace;
    uint16_t timer;
    uint16_t pos_x;
    int16_t pos_y;
} Enemy;

static Enemy pool[kEnemySlots];

// change-only oam, the rule blocks.c already follows: an unchanged slot writes no tile or property,
// and a walker crossing half a pixel a frame only actually moves on every other one, so its two
// move_sprite calls are skipped on the frames between
static uint8_t drawn_tile[kEnemySlots];
static uint8_t drawn_prop[kEnemySlots];
static uint8_t drawn_x[kEnemySlots];
static uint8_t drawn_y[kEnemySlots];
// m22 made a koopa, a paratroopa and a piranha 16x24 in a 16x32 box, which is four oam slots where
// the old 16x16 art took two. the pool cannot afford a fixed four per entry - that is twenty of the
// forty slots even for five goombas - so the slots are handed out FRESH EVERY FRAME, in pool order,
// two to a 16x16 kind and four to a tall one, skipping whatever is off screen. drawn_slot is the
// slot each entry got last frame (0xff when it drew nothing): a slot that moved has to have its
// tiles and properties written again, which is what makes the change-only cache above safe under a
// layout that shifts. see mario.h's oam map
static uint8_t drawn_slot[kEnemySlots];
// the first oam slot past everything the pool wrote last frame. it is what a shrinking pass parks
// down to, and hazards.c reads it through enemies_oam_top() to find the floor of its own pool
static uint8_t oam_used;

// the roster's own point tables, generated from games/mario/research/roster.json
// in tens, the unit hud_score keeps: a runtime divide here would cost bank 0 sdcc's whole
// 16-bit division helper for one add
static const uint16_t kStompChain[kStompChainCount] = kStompChainTensInit;
static const uint16_t kShellChain[kShellChainCount] = kShellChainTensInit;

#if kEnemyLab
// the lab roster, on the long flat run 1-1 keeps past its pits so the plain route planner can reach
// it with nothing in the way: five on one row for the scanline cap, then a koopa with two goombas
// downstream of it for the shell chain and the wake timer
#define kLabCount 8U
static const uint16_t kLabColumn[kLabCount] = {106, 108, 110, 112, 114, 126, 128, 130};
static const uint8_t kLabRow[kLabCount] = {13, 13, 13, 13, 13, 13, 13, 13};
static const uint8_t kLabKind[kLabCount] = {kEnemyGoomba, kEnemyGoomba, kEnemyGoomba, kEnemyGoomba,
                                            kEnemyGoomba, kEnemyKoopa,  kEnemyGoomba, kEnemyGoomba};
static uint8_t use_lab;
#endif

// the list the spawn scan walks, sorted by column by compile_level.py
static const uint16_t* roster_column;
static const uint8_t* roster_row;
static const uint8_t* roster_kind;
static uint8_t roster_count;
// a sub-area holds no enemies, so the pool sits idle rather than track a camera it has no list for
static uint8_t enabled;

// one byte per roster entry, and the deliberate departure from smb1's advancing cursor: an enemy
// that leaves the view alive is only parked, and comes back at its own roster cell when that cell
// scrolls into the spawn band again from either side. only a kill is permanent, and only until the
// level reloads
#define kRosterPending 0U // never spawned this life, or armed to come back
#define kRosterLive 1U    // holds a pool slot
#define kRosterDead 2U    // stomped, burned, shelled, starred or fallen out: gone for this life
#define kRosterWait 3U    // left the view alive; re-arms once its cell leaves the band
static uint8_t roster_state[LEVEL_MAX_ENEMIES];
// which entry each pool slot came from. beside the pool rather than in it, so a slot stays 16 bytes
static uint8_t slot_roster[kEnemySlots];
// the entries whose cells lie inside the spawn band, [frontier_lo, frontier_hi). the roster is
// sorted by column, so each index walks one step at a time with the camera and no frame scans the
// whole list
static uint8_t frontier_lo;
static uint8_t frontier_hi;

// one shared walk phase, so every walker steps together and no slot carries its own counter
static uint8_t anim;
static uint8_t stomp_chain;
static uint8_t shell_chain;

// the idle fast path. every frame of this engine sits near the budget already, so a pool with
// nothing in it has to cost near nothing or the loop misses a vsync. live counts the busy slots
// and oam_used how far up oam the last draw reached; the frontier pair above is what keeps the
// spawn test off the roster
static uint8_t live;

static int16_t row_of(int16_t py) {
    return py < 0 ? (int16_t)-1 : (int16_t)(py >> 4);
}

// the leading edge's column: the side the enemy is walking into
static uint8_t lead_of(const Enemy* e) {
    return (uint8_t)((e->dir > 0 ? (uint16_t)(e->pos_x + kEnemyWidthPx - 1U) : e->pos_x) >> 4);
}

static uint8_t foot_of(const Enemy* e) {
    return (uint8_t)((uint16_t)(e->pos_x + (kEnemyWidthPx / 2U)) >> 4);
}

static void award(const uint16_t* table, uint8_t count, uint8_t* chain, const Enemy* e) {
    const uint8_t step = (*chain < count) ? *chain : (uint8_t)(count - 1U);

    hud_score = (uint16_t)(hud_score + table[step]);
    // roster.json ends both sequences in a 1-up; m8b pays that life once the table runs out, and
    // the label over the victim says which it was
    popup_show(e->pos_x, e->pos_y, *chain >= count ? (uint16_t)kPopupOneUp : table[step]);
    if (*chain >= count) {
        hud_add_life();
    }
    if (*chain < 0xFFU) {
        ++(*chain);
    }
}

// MoveObjectHorizontally, the same signed byte the player's speed uses: high nibble whole px, low
// nibble sixteenths fed through a 1/256-px accumulator
static void move_x(Enemy* e, int8_t speed) {
    const uint8_t raw = (uint8_t)speed;
    const uint16_t sum = (uint16_t)((uint16_t)e->x_force + (uint16_t)((uint16_t)(raw & 0x0FU) << 4));
    int16_t whole = (int16_t)(raw >> 4);
    int16_t next;

    if (whole >= 8) {
        whole = (int16_t)(whole - 16);
    }
    e->x_force = (uint8_t)sum;
    next = (int16_t)((int16_t)e->pos_x + whole + (int16_t)(sum >> 8));
    if (next < 0) {
        next = 0;
    }
    e->pos_x = (uint16_t)next;
}

static void reverse(Enemy* e) {
    e->dir = (int8_t)-e->dir;
    e->x_force = 0;
    e->lead_col = lead_of(e);
}

// the bible's walk speed is exactly half a pixel a frame, so the general nibble math above reduces
// to one accumulator add and a conditional step - and a walker is what the pool is nearly always full of
static void move_walk(Enemy* e) {
    const uint16_t sum = (uint16_t)((uint16_t)e->x_force + 128U);

    e->x_force = (uint8_t)sum;
    if (e->dir > 0) {
        e->pos_x = (uint16_t)(e->pos_x + (uint8_t)(sum >> 8));
    } else if (sum <= 0xFFU && e->pos_x != 0U) {
        --e->pos_x;
    }
}

static void step_ground(Enemy* e, uint8_t speed) {
    uint8_t lead;
    uint8_t foot;

    if (speed == (uint8_t)kEnemyWalkSubpx) {
        move_walk(e);
    } else {
        move_x(e, e->dir > 0 ? (int8_t)speed : (int8_t)(-(int16_t)speed));
    }
    lead = lead_of(e);
    if (lead != e->lead_col) {
        if (terrain_solid_at((int16_t)lead, row_of(e->pos_y)) != 0U ||
            terrain_solid_at((int16_t)lead, row_of((int16_t)(e->pos_y + kEnemyHeightPx - 1))) != 0U) {
            // step back out of the cell that refused, so reversing cannot leave it inside the wall
            e->pos_x = (e->dir > 0) ? (uint16_t)(((uint16_t)lead << 4) - kEnemyWidthPx)
                                    : (uint16_t)((uint16_t)(lead + 1U) << 4);
            reverse(e);
        } else {
            e->lead_col = lead;
        }
    }
    if (e->grounded == 0U) {
        return;
    }
    foot = foot_of(e);
    if (foot == e->foot_col) {
        return;
    }
    e->foot_col = foot;
    // smb walks a goomba off a ledge instead of turning it, and the centre column leaving solid
    // ground is the frame it goes over the edge
    if (terrain_floor_at((int16_t)foot, row_of((int16_t)(e->pos_y + kEnemyHeightPx))) != 0U) {
        return;
    }
    // physics.json red_koopa_edge_turning: the red koopa skips the state change that lets the
    // others walk off and falls through to the direction flip instead
    if (e->kind == kEnemyKoopaRed && e->state == kEnemyWalk) {
        e->pos_x = (e->dir > 0) ? (uint16_t)(((uint16_t)foot << 4) - (kEnemyWidthPx / 2U))
                                : (uint16_t)(((uint16_t)(foot + 1U) << 4) - (kEnemyWidthPx / 2U));
        e->foot_col = foot_of(e);
        reverse(e);
        return;
    }
    e->grounded = 0;
    e->dy = 0;
    e->y_accum = 0;
}

// roster.json: the plant rises out of its pipe, bites, sinks, and will not come up at all while
// the player is standing on or beside the cap. the whole cycle is one counter, so the host twin
// mirrors it exactly. the frame counts are ours - the bible times neither the bite nor the wait
static void step_plant(Enemy* e, uint16_t player_px) {
    // a plant never moves sideways, so pos_x is its pipe and foot_col is the cap row it climbs out of
    const uint16_t base = e->pos_x;
    const int16_t floor_y = (int16_t)((int16_t)e->foot_col << 4);

    if (e->timer == 0U) {
        // adjacent is the pipe's own two columns plus one either side
        const uint16_t left = (base > (uint16_t)kBlockPx) ? (uint16_t)(base - kBlockPx) : 0U;

        if ((uint16_t)(player_px + kPlayerWidthPx) > left &&
            player_px < (uint16_t)(base + 3U * (uint16_t)kBlockPx)) {
            return;
        }
    }
    ++e->timer;
    if (e->timer >= (uint16_t)kPlantCycleFrames) {
        e->timer = 0;
    }
    if (e->timer < (uint16_t)kPlantRisePx) {
        e->pos_y = (int16_t)(floor_y - (int16_t)e->timer);
        e->state = kEnemyPlantUp;
    } else if (e->timer < (uint16_t)(kPlantRisePx + kPlantHoldFrames)) {
        e->pos_y = (int16_t)(floor_y - kPlantRisePx);
        e->state = kEnemyPlantUp;
    } else if (e->timer < (uint16_t)(2U * kPlantRisePx + kPlantHoldFrames)) {
        e->pos_y = (int16_t)(floor_y - (int16_t)(2U * kPlantRisePx + kPlantHoldFrames - (uint16_t)e->timer));
        e->state = kEnemyPlantUp;
    } else {
        e->pos_y = floor_y;
        e->state = kEnemyPlantHidden;
    }
}

// the red paratroopa's flight. it holds its column and probes no terrain at all, so the whole step
// is two adds and a compare: pos_y moves a pixel a frame, y_accum tracks how far into the band it
// is and turns it round at either end. see kParaBandPx in mario.h for why no slot field was added
static void step_fly(Enemy* e) {
    e->pos_y = (int16_t)(e->pos_y + e->dy);
    e->y_accum = (uint8_t)((int16_t)e->y_accum + e->dy);
    if (e->y_accum == 0U) {
        e->dy = 1;
    } else if (e->y_accum >= (uint8_t)kParaSpanPx) {
        e->dy = -1;
    }
}

// smb's defeat animation for anything a fireball or a moving shell takes: the body flips over,
// pops upward and falls out of the level instead of blinking away. it stops colliding with mario
// and with the pool on this very frame, and its slot frees when despawn sees it leave the level
static void flip_kill(uint8_t i) {
    Enemy* e = &pool[i];

    // the corpse is only scenery from here: its roster cell is spent for the rest of this life
    roster_state[slot_roster[i]] = kRosterDead;
    e->state = kEnemyFlipped;
    e->dy = (int8_t)kEnemyFlipPopPx;
    e->y_accum = 0;
    e->grounded = 0;
    e->grace = 0;
}

// the corpse's own fall: no terrain probe at all, so it drops straight through whatever it was
// standing on the way smb's does
static void step_flip(Enemy* e) {
    const uint16_t sum = (uint16_t)((uint16_t)e->y_accum + (uint16_t)kEnemyFlipGravitySubpx);

    e->y_accum = (uint8_t)sum;
    if (sum > 0xFFU) {
        e->dy = (int8_t)(e->dy + 1);
        if (e->dy > kEnemyFlipMaxFallPx) {
            e->dy = kEnemyFlipMaxFallPx;
        }
    }
    e->pos_y = (int16_t)(e->pos_y + e->dy);
}

static void step_fall(Enemy* e) {
    const uint16_t sum = (uint16_t)((uint16_t)e->y_accum + (uint16_t)kEnemyGravitySubpx);
    int16_t row;

    e->y_accum = (uint8_t)sum;
    if (sum > 0xFFU) {
        e->dy = (int8_t)(e->dy + 1);
        if (e->dy > kEnemyMaxFallPx) {
            e->dy = kEnemyMaxFallPx;
        }
    }
    e->pos_y = (int16_t)(e->pos_y + e->dy);
    e->foot_col = foot_of(e);
    row = row_of((int16_t)(e->pos_y + kEnemyHeightPx - 1));
    if (terrain_solid_at((int16_t)e->foot_col, row) == 0U) {
        return;
    }
    e->pos_y = (int16_t)(((int16_t)row << 4) - kEnemyHeightPx);
    e->dy = 0;
    e->y_accum = 0;
    e->grounded = 1;
}

// the pool stays packed into [0, live), so every loop below runs only over the busy slots and an
// empty pool costs nothing at all. taking a slot out moves the last one into the hole, which is why
// the oam cache at that index has to be forced to redraw
static void remove_at(uint8_t i) {
    --live;
    pool[i] = pool[live];
    slot_roster[i] = slot_roster[live];
    drawn_slot[i] = 0xFF; // never a real slot, so the next draw rewrites tile, prop and position
}

// a slot leaving the pool. a cell whose enemy was killed stays dead however the slot goes away, so
// a corpse that drifts off the camera before it has finished falling is not parked by mistake
static void release_at(uint8_t i, uint8_t next) {
    uint8_t* state = &roster_state[slot_roster[i]];

    if (*state != kRosterDead) {
        *state = next;
    }
    remove_at(i);
}

// a slot leaving the pool alive. while its cell is still inside the spawn band the enemy has to
// wait for that cell to scroll out before it re-arms, or it would be dropped straight back onto a
// cell the camera never left. a cell already outside the band re-arms at once instead: outside the
// band is exactly where nothing spawns, so there is nothing there to guard against - and the cell
// often leaves the band well before the walker standing on it has finished wandering out of view
static void park_at(uint8_t i) {
    const uint8_t idx = slot_roster[i];

    release_at(i, (idx >= frontier_lo && idx < frontier_hi) ? kRosterWait : kRosterPending);
}

static uint8_t row_load(uint8_t top_row) {
    uint8_t i;
    uint8_t n = 0;

    for (i = 0; i < live; ++i) {
        // a falling corpse is not on any row: it must not hold the scanline cap against a spawn.
        // a flyer answers for whatever row it is passing through this frame, which is the honest
        // answer - the cap is about how many 16px sprites a scanline crosses, and a paratroopa
        // crossing a walker's row costs that scanline exactly what a walker there would
        if (pool[i].state != kEnemyFlipped && (uint8_t)(pool[i].pos_y >> 4) == top_row) {
            ++n;
        }
    }
    return n;
}

// is this cell far enough off the left of the spawn band to count as behind the camera
static uint8_t left_of_band(uint8_t i, uint16_t cam_x) {
    const uint16_t px = (uint16_t)((uint16_t)roster_column[i] << 4);

    return ((uint16_t)(px + kEnemyWidthPx + kEnemySpawnMarginPx) < cam_x) ? 1U : 0U;
}

// a cell leaving the band is what re-arms whatever is parked at it. without that gap between the
// spawn band and the wider despawn one, a walker that stepped off the left edge would be dropped
// straight back onto a cell the camera is still sitting on, over and over
static void rearm(uint8_t i) {
    if (roster_state[i] == kRosterWait) {
        roster_state[i] = kRosterPending;
    }
}

// the two frontier indices, walked onto this frame's camera. each of the four loops usually runs
// zero or one step, so the roster itself is never scanned end to end
static void frontier_step(uint16_t cam_x) {
    const uint16_t right = (uint16_t)(cam_x + kScreenWidthPx + kEnemySpawnMarginPx);

    while (frontier_hi < roster_count && (uint16_t)((uint16_t)roster_column[frontier_hi] << 4) <= right) {
        ++frontier_hi;
    }
    while (frontier_hi != 0U && (uint16_t)((uint16_t)roster_column[frontier_hi - 1U] << 4) > right) {
        --frontier_hi;
        rearm(frontier_hi);
    }
    while (frontier_lo < roster_count && left_of_band(frontier_lo, cam_x) != 0U) {
        rearm(frontier_lo);
        ++frontier_lo;
    }
    while (frontier_lo != 0U && left_of_band((uint8_t)(frontier_lo - 1U), cam_x) == 0U) {
        --frontier_lo;
    }
}

// the idle test the fast path needs: is anything inside the band still waiting to come in. the band
// is eleven columns wide and the rosters are sparse, so this is a handful of byte compares
static uint8_t band_pending(void) {
    uint8_t i;

    for (i = frontier_lo; i < frontier_hi; ++i) {
        if (roster_state[i] == kRosterPending) {
            return 1U;
        }
    }
    return 0U;
}

// the object loader: an enemy comes in as its cell reaches the screen's right edge the way smb's
// does, and - the user's own departure from smb1 - also as it reaches the mirrored band off the
// left edge, so backtracking brings a parked one home to the cell it started on
static void spawn(void) {
    uint8_t i;

    for (i = frontier_lo; i < frontier_hi; ++i) {
        uint8_t top_row;
        Enemy* e;

        if (roster_state[i] != kRosterPending) {
            continue;
        }
        // the roster names the surface row it stands on; its box top is the row above that
        top_row = (uint8_t)(roster_row[i] - 1U);
        if (row_load(top_row) >= kEnemyRowCap) {
            return; // the scanline cap: it waits right here until a slot on its row frees
        }
        if (live >= kEnemySlots) {
            return;
        }
        e = &pool[live];
        slot_roster[live] = i;
        ++live;
        roster_state[i] = kRosterLive;
        e->state = kEnemyWalk;
        e->kind = roster_kind[i];
        e->pos_x = (uint16_t)((uint16_t)roster_column[i] << 4);
        e->pos_y = (int16_t)((int16_t)top_row << 4);
        e->dir = -1; // smb starts every walker off to the left
        e->x_force = 0;
        e->dy = 0;
        e->y_accum = 0;
        e->grounded = 1;
        e->timer = 0;
        e->grace = 0;
        e->lead_col = lead_of(e);
        e->foot_col = foot_of(e);
        if (e->kind == kEnemyPiranha) {
            // the plant starts hidden inside its pipe: foot_col carries the cap row it rises out of
            e->state = kEnemyPlantHidden;
            e->foot_col = roster_row[i];
            e->pos_y = (int16_t)((int16_t)e->foot_col << 4);
            // roster.json names the pipe's left of its two 16px columns; the plant's own box is one
            // enemy width (16px), so recentre it across the pipe's full 32px span - see
            // kPlantCenterOffsetPx in mario.h
            e->pos_x = (uint16_t)(e->pos_x + kPlantCenterOffsetPx);
        } else if (e->kind == kEnemyKoopaParaRed) {
            // the flyer comes in at the centre of its band and starts by rising. it never stands on
            // anything, so grounded goes off and the gravity accumulator becomes its band offset
            e->grounded = 0;
            e->y_accum = (uint8_t)kParaBandPx;
            e->dy = -1;
        }
    }
}

// both sides free the slot, and neither is a kill: the cell the enemy came from parks it instead,
// so walking back to that cell brings it in again. the bottom of the level is the one exit that
// counts as a death. the three bounds are worked out once, not per slot
static void despawn(uint16_t cam_x) {
    const uint16_t gone_left = (cam_x > (kEnemyWidthPx + kEnemyDespawnMarginPx))
                                   ? (uint16_t)(cam_x - (kEnemyWidthPx + kEnemyDespawnMarginPx))
                                   : 0U;
    const uint16_t gone_right = (uint16_t)(cam_x + kScreenWidthPx + kEnemyDespawnMarginPx);
    uint8_t i = 0;

    while (i < live) {
        const Enemy* e = &pool[i];

        if (e->pos_y > (int16_t)kLevelHeightPx) {
            release_at(i, kRosterDead);
            continue;
        }
        if (e->pos_x < gone_left || e->pos_x > gone_right) {
            park_at(i);
            continue;
        }
        ++i;
    }
}

static uint8_t boxes_meet(const Enemy* a, const Enemy* b) {
    if (a->pos_x + kEnemyWidthPx <= b->pos_x || b->pos_x + kEnemyWidthPx <= a->pos_x) {
        return 0;
    }
    return (a->pos_y + kEnemyHeightPx > b->pos_y && b->pos_y + kEnemyHeightPx > a->pos_y) ? 1U : 0U;
}

// pure arithmetic over the live pairs only, no terrain probe, so this runs unconditionally
static void collide_enemies(void) {
    uint8_t i = 0;
    uint8_t j;

    while ((uint8_t)(i + 1U) < live) {
        Enemy* a = &pool[i];

        if (a->state == kEnemySquashed || a->state == kEnemyFlipped || a->kind == kEnemyPiranha) {
            ++i;
            continue;
        }
        j = (uint8_t)(i + 1U);
        while (j < live) {
            Enemy* b = &pool[j];
            Enemy* left;
            Enemy* right;

            if (b->state == kEnemySquashed || b->state == kEnemyFlipped || b->kind == kEnemyPiranha ||
                boxes_meet(a, b) == 0U) {
                ++j;
                continue;
            }
            if (a->state == kEnemyShellMove && b->state != kEnemyShellMove) {
                // the kill leaves the body in its slot flipping out of the level, so the shell
                // walks on over it rather than the pool closing up behind it
                flip_kill(j);
                award(kShellChain, kShellChainCount, &shell_chain, b);
                ++j;
                continue;
            }
            if (b->state == kEnemyShellMove && a->state != kEnemyShellMove) {
                flip_kill(i);
                award(kShellChain, kShellChainCount, &shell_chain, a);
                break;
            }
            // a flyer holds its column: the nudge below must not shove it sideways, and it is not
            // walking into anything either, so it only ever meets the pool as a shell's victim
            if (a->kind == kEnemyKoopaParaRed || b->kind == kEnemyKoopaParaRed) {
                ++j;
                continue;
            }
            // two walkers meeting turn each other around; the nudge stops them flipping again
            left = (a->pos_x <= b->pos_x) ? a : b;
            right = (left == a) ? b : a;
            right->pos_x = (uint16_t)(left->pos_x + kEnemyWidthPx);
            if (left->dir > 0) {
                reverse(left);
            }
            if (right->dir < 0) {
                reverse(right);
            }
            ++j;
        }
        ++i;
    }
}

static uint8_t stomp(Enemy* e) {
    sfx_square1(kSfxEnemyStomp);
    if (e->state == kEnemyShellMove) {
        // re-stomping a travelling shell stops it dead and starts its wake over
        e->state = kEnemyShellIdle;
        e->timer = kShellWakeFrames;
        e->grace = kShellGraceFrames;
        e->x_force = 0;
        shell_chain = 0;
        return kEnemyHitShellStomp;
    }
    award(kStompChain, kStompChainCount, &stomp_chain, e);
    // roster.json: a paratroopa stomps down into a plain koopa. it keeps the slot and the position
    // it was hit at, loses the wings, and falls out of the air to whatever is under it - a second
    // stomp is what puts it in its shell. the points are the koopa's, paid by the award above
    if (e->kind == kEnemyKoopaParaRed) {
        e->kind = kEnemyKoopaRed;
        e->grounded = 0;
        e->dy = 0;
        e->y_accum = 0;
        e->lead_col = lead_of(e);
        e->foot_col = foot_of(e);
        // without this, mario's own stomp bounce keeps his feet over the koopa while it is still
        // falling barely at all, and an unguarded second touch on the very next bounce puts it
        // straight into its frozen shell-idle state mid-air - the "floats" bug: shell-idle never
        // steps gravity at all, so it hangs exactly where it was hit for kShellWakeFrames
        e->grace = kShellGraceFrames;
        return kEnemyHitStomp;
    }
    if (e->kind == kEnemyKoopa || e->kind == kEnemyKoopaRed) {
        e->state = kEnemyShellIdle;
        e->timer = kShellWakeFrames;
        e->grace = kShellGraceFrames;
        e->x_force = 0;
        return kEnemyHitStomp;
    }
    e->state = kEnemySquashed;
    e->timer = kSquashFrames;
    return kEnemyHitStomp;
}

// a fireball or a star kill pays a flat per-kind figure and starts no chain. roster.json says the
// star's consecutive-defeat scoring escalates but calls its smb1-era values must-verify, so the
// escalation is left out rather than invented
static void award_kill(const Enemy* e) {
    sfx_square1(kSfxEnemySmack);
    const uint16_t tens = e->kind == kEnemyGoomba ? (uint16_t)kScoreTens(kGoombaKillPoints)
                                                  : (uint16_t)kScoreTens(kKoopaKillPoints);

    hud_score = (uint16_t)(hud_score + tens);
    popup_show(e->pos_x, e->pos_y, tens);
}

static uint8_t collide_player(uint16_t player_px, int16_t player_py, uint8_t player_h, int8_t player_dy,
                              uint8_t flags) {
    const uint16_t left = (uint16_t)(player_px + kPlayerHitInsetPx);
    const uint16_t right = (uint16_t)(left + kPlayerHitWidthPx);
    const int16_t feet = (int16_t)(player_py + player_h);
    uint8_t hit = kEnemyHitNone;
    uint8_t i = 0;

    while (i < live) {
        Enemy* e = &pool[i];
        uint16_t enemy_left;
        uint8_t from_above;
        uint8_t code;

        // a flattened goomba is scenery for the frames it has left, and so is a body already
        // falling out of the level: neither can hurt him and neither can be hit again
        if (e->state == kEnemySquashed || e->state == kEnemyFlipped) {
            ++i;
            continue;
        }
        enemy_left = (uint16_t)(e->pos_x + kEnemyHitInsetPx);
        if (right <= enemy_left || (uint16_t)(enemy_left + kEnemyHitWidthPx) <= left) {
            ++i;
            continue;
        }
        if (feet <= e->pos_y || (int16_t)(e->pos_y + kEnemyHeightPx) <= player_py) {
            ++i;
            continue;
        }
        if (e->grace != 0U) {
            ++i;
            continue;
        }
        // smb decides a stomp by where his feet are, not which way he is moving: a paratroopa that
        // rises into him at the top of his jump still goes under his boots
        from_above = (feet <= (int16_t)(e->pos_y + kEnemyStompLinePx)) ? 1U : 0U;
        // the star takes anything it touches off the pool outright, stomp or not
        if ((flags & kEnemyFlagStar) != 0U && (from_above == 0U || e->kind == kEnemyPiranha)) {
            award_kill(e);
            release_at(i, kRosterDead);
            continue;
        }
        // roster.json: a piranha plant cannot be stomped, only burned or run into
        if (e->kind == kEnemyPiranha) {
            if ((flags & kEnemyFlagImmune) != 0U) {
                ++i;
                continue;
            }
            return kEnemyHitDamage;
        }
        if (e->state == kEnemyShellIdle) {
            // away from him: smb sends the shell out the side he touched it from
            e->dir =
                ((uint16_t)(player_px + (kPlayerWidthPx / 2U)) < (uint16_t)(e->pos_x + (kEnemyWidthPx / 2U)))
                    ? (int8_t)1
                    : (int8_t)-1;
            e->state = kEnemyShellMove;
            sfx_square1(kSfxEnemySmack);
            e->grace = kShellGraceFrames;
            e->x_force = 0;
            e->lead_col = lead_of(e);
            shell_chain = 0;
            // landing on a resting shell kicks it and bounces him off; walking into one only kicks
            if (from_above != 0U && hit < kEnemyHitShellStomp) {
                hit = kEnemyHitShellStomp;
            }
            ++i;
            continue;
        }
        if (from_above != 0U) {
            code = stomp(e);
            // a shell or a de-winged flyer is still alive; a flattened goomba is not
            if (e->state == kEnemySquashed) {
                roster_state[slot_roster[i]] = kRosterDead;
            }
            if (code > hit) {
                hit = code;
            }
            ++i;
            continue;
        }
        // the injury window swallows the touch entirely: neither of them is hurt by it
        if ((flags & kEnemyFlagImmune) != 0U) {
            ++i;
            continue;
        }
        return kEnemyHitDamage;
    }
    return hit;
}

uint8_t enemies_fireball_hit(uint16_t px, int16_t py) BANKED {
    uint8_t i;

    for (i = 0; i < live; ++i) {
        Enemy* e = &pool[i];
        const uint16_t enemy_left = (uint16_t)(e->pos_x + kEnemyHitInsetPx);

        if (e->state == kEnemySquashed || e->state == kEnemyFlipped) {
            continue;
        }
        if ((uint16_t)(px + kFireballPx) <= enemy_left || (uint16_t)(enemy_left + kEnemyHitWidthPx) <= px) {
            continue;
        }
        if ((int16_t)(py + kFireballPx) <= e->pos_y || (int16_t)(e->pos_y + kEnemyHeightPx) <= py) {
            continue;
        }
        award_kill(e);
        flip_kill(i);
        return 1;
    }
    return 0;
}

void enemies_set_lab(uint8_t on) BANKED {
#if kEnemyLab
    use_lab = on;
#else
    (void)on;
#endif
}

void enemies_load_level(void) BANKED {
    uint8_t i;

    assets_load_enemy_tiles();
    popup_art_load();
    if (level->type == (uint8_t)kLevelTypeCastle) {
        assets_load_enemy_palettes_castle();
    } else {
        assets_load_enemy_palettes();
    }

    roster_column = level->enemy_column;
    roster_row = level->enemy_row;
    roster_kind = level->enemy_kind;
    roster_count = level->enemy_count;
#if kEnemyLab
    if (use_lab != 0U) {
        roster_column = kLabColumn;
        roster_row = kLabRow;
        roster_kind = kLabKind;
        roster_count = (uint8_t)kLabCount;
    }
#endif

    for (i = 0; i < kEnemySlots; ++i) {
        // not the parked marker: a respawn leaves the last life's enemies sitting in oam, so the
        // first draw of the new one has to write every slot off screen
        drawn_slot[i] = 0xFF;
        drawn_prop[i] = 0xFE;
        drawn_tile[i] = 0;
        drawn_x[i] = 0xFF;
    }
    for (i = 0; i < roster_count; ++i) {
        roster_state[i] = kRosterPending;
    }
    frontier_lo = 0;
    frontier_hi = 0;
    live = 0;
    oam_used = (uint8_t)(kSpriteEnemyFirst + kEnemyOamMax);
    anim = 0;
    stomp_chain = 0;
    shell_chain = 0;
    enabled = 1;
}

void enemies_enter_area(uint8_t area) BANKED {
    // a pipe is not a kill: everything on screen parks at its own cell, so coming back up into a
    // stretch already walked finds it populated the way scrolling back into it does
    while (live != 0U) {
        park_at((uint8_t)(live - 1U));
    }
    enabled = (area == kAreaMain) ? 1U : 0U;
}

uint8_t enemies_update(uint16_t player_px, int16_t player_py, uint8_t player_h, int8_t player_dy,
                       uint8_t flags, uint16_t cam_x) BANKED {
    uint8_t i;

    if (enabled == 0U) {
        return kEnemyHitNone;
    }
    frontier_step(cam_x);
    // the fast path: nothing alive and no cell inside the band still waiting to come in
    if (live == 0U && band_pending() == 0U) {
        return kEnemyHitNone;
    }
    // landing ends a stomp chain: the escalation only runs while he stays off the ground
    if ((flags & kEnemyFlagGrounded) != 0U) {
        stomp_chain = 0;
    }
    ++anim;
    spawn();

    i = 0;
    while (i < live) {
        Enemy* e = &pool[i];

        if (e->grace != 0U) {
            --e->grace;
        }
        // a corpse is checked before its kind is: a burned piranha falls out of its pipe too
        if (e->state == kEnemyFlipped) {
            step_flip(e);
            ++i;
            continue;
        }
        if (e->kind == kEnemyPiranha) {
            step_plant(e, player_px);
            ++i;
            continue;
        }
        // a stomped flyer has already become a plain koopa, so this only runs while it still flies
        if (e->kind == kEnemyKoopaParaRed) {
            step_fly(e);
            ++i;
            continue;
        }
        if (e->state == kEnemySquashed) {
            --e->timer;
            if (e->timer == 0U) {
                release_at(i, kRosterDead);
                continue;
            }
            ++i;
            continue;
        }
        if (e->state == kEnemyShellIdle) {
            --e->timer;
            if (e->timer == 0U) {
                // smb wakes an untouched shell back into its koopa, walking the way it last faced
                e->state = kEnemyWalk;
                e->lead_col = lead_of(e);
            }
            // an idle shell is normally already standing on the ground, but a de-winged koopa can
            // be re-stomped into one before it ever lands - gravity still has to run for it, or it
            // hangs frozen mid-air for the whole wake timer instead of falling
            if (e->grounded == 0U) {
                step_fall(e);
            }
            ++i;
            continue;
        }
        if (e->state == kEnemyShellMove) {
            step_ground(e, (uint8_t)kEnemyShellSubpx);
        } else {
            step_ground(e, (uint8_t)kEnemyWalkSubpx);
        }
        if (e->grounded == 0U) {
            step_fall(e);
        }
        ++i;
    }

    if (live > 1U) {
        collide_enemies();
    }
    i = (live != 0U) ? collide_player(player_px, player_py, player_h, player_dy, flags)
                     : (uint8_t)kEnemyHitNone;
    despawn(cam_x);
    return i;
}

// the art box a pool entry needs. a koopa, a paratroopa and a piranha are 16x24 bottom-aligned in
// a 16x32 box, so they cost two rows of two 8x16 sprites - four oam slots; a goomba, a squashed
// goomba and either shell are 16x16 and cost two. a corpse keeps its own kind's box, upside down
static uint8_t entry_tall(const Enemy* e) {
    if (e->kind == kEnemyPiranha) {
        return 1;
    }
    if (e->kind == kEnemyGoomba || e->state == kEnemySquashed || e->state == kEnemyShellIdle ||
        e->state == kEnemyShellMove) {
        return 0;
    }
    return 1;
}

// the first oam slot past everything the pool drew last frame, which is the floor hazards.c starts
// its own pool at (see mario.h's oam map)
uint8_t enemies_oam_top(void) BANKED {
    return oam_used;
}

// the tiles and palette of one 16 px row of an enemy: two 8x16 sprites side by side. mirror != 0
// swaps which tile draws on which side and sets S_FLIPX on both, which is how a facing frame is
// turned round. symmetric != 0 means only the LEFT half of the row is in vram - a left-right
// symmetric frame stores half the tiles it draws - so the right sprite is that same tile with
// S_FLIPX of its own while the left keeps the prop it was given
static void pair_art(uint8_t slot, uint8_t left, uint8_t right, uint8_t prop, uint8_t mirror,
                     uint8_t symmetric) {
    uint8_t left_prop = prop;
    uint8_t right_prop = prop;

    if (mirror != 0U) {
        const uint8_t swap = left;

        left = right;
        right = swap;
        left_prop = (uint8_t)(prop | (uint8_t)S_FLIPX);
        right_prop = left_prop;
    }
    if (symmetric != 0U) {
        right = left;
        right_prop = (uint8_t)(left_prop ^ (uint8_t)S_FLIPX);
    }
    set_sprite_tile(slot, left);
    set_sprite_tile((uint8_t)(slot + 1U), right);
    set_sprite_prop(slot, left_prop);
    set_sprite_prop((uint8_t)(slot + 1U), right_prop);
}

// and where that row sits. positions move every frame an enemy walks; tiles and props only change
// when its pose does, which is why the two are written apart
static void pair_move(uint8_t slot, uint8_t px, uint8_t py) {
    move_sprite(slot, px, py);
    move_sprite((uint8_t)(slot + 1U), (uint8_t)(px + 8U), py);
}

void enemies_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    // the walk phase is one shared counter, so both walk frames are picked once a frame, not once
    // a slot, and the loop below is left with a three-way pick
    const uint8_t swap = (uint8_t)((anim & kEnemyAnimFrames) != 0U ? 1U : 0U);
    const uint8_t koopa_tile = swap != 0U ? (uint8_t)kTileKoopaWalk1 : (uint8_t)kTileKoopaWalk0;
    const uint8_t para_tile = swap != 0U ? (uint8_t)kTileParaFly1 : (uint8_t)kTileParaFly0;
    uint8_t next = (uint8_t)kSpriteEnemyFirst;
    uint8_t i;

    // the same fast path: an empty pool with nothing left in oam writes nothing at all - but for
    // the score label, which draws in the slots right past the pool and so is placed from here
    if (live == 0U && oam_used == (uint8_t)kSpriteEnemyFirst) {
        if (popup_busy != 0U) {
            popup_frame(cam_y, (uint8_t)kSpriteEnemyFirst);
        }
        return;
    }

    for (i = 0; i < live; ++i) {
        const Enemy* e = &pool[i];
        const int16_t sx = (int16_t)((int16_t)e->pos_x - (int16_t)cam_x);
        const int16_t sy = (int16_t)(e->pos_y - (int16_t)cam_y);
        // the pose: a tile base, whether the box is tall, whether its right half is its left half
        // mirrored, and the prop every one of its sprites shares
        uint8_t base;
        uint8_t prop;
        uint8_t tall;
        uint8_t symmetric = 0;
        uint8_t mirror = 0;
        uint8_t flip_y = 0;
        uint8_t slot;
        uint8_t fresh;
        int16_t art_y;
        int16_t art_top;
        int16_t art_bot;

        tall = entry_tall(e);
        // where the box's top row lands. a walker's feet are at sy + 16 and its 24 px of art is
        // bottom-aligned in a 32 px box, so the box starts 16 px above its own hitbox. the plant is
        // the exception: its art runs rows 9..31 of the box and has to sit exactly on its hitbox,
        // or the 8 px of stem the pipe is meant to hide would poke out over the cap
        art_y = tall != 0U ? (int16_t)(sy - (e->kind == kEnemyPiranha ? (int16_t)kPlantArtRisePx
                                                                      : (int16_t)kEnemyTallRisePx))
                           : sy;
        // and the rows it can actually paint, which is what the cull has to be measured against:
        // a tall entry's art starts a whole 16 px above its own hitbox, so culling on the hitbox
        // alone chopped a koopa off the top of the screen while half of it was still visible. the
        // plant is measured off its lit rows instead of its box - the 8 blank ones at the top of
        // its box are the stem the pipe hides
        art_top = (int16_t)(e->kind == kEnemyPiranha ? sy : art_y);
        art_bot = (int16_t)(art_top + (e->kind == kEnemyPiranha ? (int16_t)kPlantArtRowsPx
                                                                : (tall != 0U ? (int16_t)(2 * kEnemyHeightPx)
                                                                              : (int16_t)kEnemyHeightPx)));
        if (art_bot <= 0 || art_top >= (int16_t)kScreenHeightPx || sx <= -(int16_t)kEnemyWidthPx ||
            sx >= (int16_t)kScreenWidthPx) {
            drawn_slot[i] = 0xFF;
            continue;
        }
        // the corpse is picked before the kind is, so a burned piranha falls out of its pipe
        // upside down rather than staying tucked behind it
        if (e->state == kEnemyFlipped) {
            flip_y = (uint8_t)S_FLIPY;
            if (e->kind == kEnemyGoomba) {
                base = (uint8_t)kTileGoombaWalk0;
            } else if (e->kind == kEnemyPiranha) {
                base = (uint8_t)kTilePiranha;
                symmetric = 1;
            } else {
                base = (uint8_t)kTileKoopaWalk0;
            }
        } else if (e->kind == kEnemyPiranha) {
            base = (uint8_t)kTilePiranha;
            symmetric = 1;
        } else if (e->state == kEnemySquashed) {
            base = (uint8_t)kTileGoombaSquash;
            symmetric = 1;
        } else if (e->state != kEnemyWalk) {
            // smb paints a red koopa's shell red and a green one's green, and a stomped red
            // paratroopa has already become a red koopa (see collide_player)
            base = (e->kind == kEnemyKoopa) ? (uint8_t)kTileShell : (uint8_t)kTileShellRed;
            symmetric = 1;
        } else if (e->kind == kEnemyKoopaParaRed) {
            base = para_tile;
        } else if (e->kind == kEnemyGoomba) {
            // the rip's walk1 is walk0's exact mirror, so the second frame is the first drawn with
            // its halves swapped - and the facing flip rides on top of that
            base = (uint8_t)kTileGoombaWalk0;
            mirror = swap;
        } else {
            base = koopa_tile;
        }
        if (base == (uint8_t)kTileGoombaWalk0 || base == (uint8_t)kTileGoombaSquash) {
            prop = (uint8_t)kPalGoomba;
        } else if (e->kind == kEnemyKoopaParaRed || base == (uint8_t)kTileShellRed) {
            // a red enemy wears kPalStar, but S_BANK belongs to the TILE and not to the kind: a
            // red paratroopa killed while still flying keeps its kind and falls as the bank-0
            // koopa corpse, so reading the bank off the kind sent that corpse to unclaimed
            // bank-1 ids instead of to the art it is actually drawing
            prop = (uint8_t)kPalStar;
            if (base == (uint8_t)kTileParaFly0 || base == (uint8_t)kTileParaFly1 ||
                base == (uint8_t)kTileShellRed) {
                prop = (uint8_t)(prop | (uint8_t)S_BANK);
            }
        } else {
            prop = (uint8_t)kPalKoopa;
        }
        prop = (uint8_t)(prop | flip_y);
        if (base == (uint8_t)kTilePiranha && flip_y == 0U) {
            // OAM background-priority: the hardware draws this sprite behind bg colors 1-3 and only
            // over color 0. every bg palette keeps color 0 as the level's plain backdrop hue (see
            // assets_load_bg_palettes) and the pipe's own colors 1-3 are its opaque body, so as the
            // plant sinks the pipe tiles progressively cover it instead of it sitting on top
            prop = (uint8_t)(prop | (uint8_t)S_PRIORITY);
        }
        if (symmetric == 0U && e->dir < 0) {
            mirror = (uint8_t)(mirror ^ 1U);
        }
        slot = next;
        next = (uint8_t)(next + (tall != 0U ? (uint8_t)kEnemyOamTall : (uint8_t)kEnemyOamShort));
        fresh = (uint8_t)((drawn_slot[i] != slot) ? 1U : 0U);
        drawn_slot[i] = slot;

        {
            const uint8_t px = (uint8_t)(sx + kOamXOffset);
            const uint8_t py = (uint8_t)(art_y + kOamYOffset);
            const uint8_t moved = (uint8_t)((fresh != 0U || drawn_x[i] != px || drawn_y[i] != py) ? 1U : 0U);
            const uint8_t redrawn =
                (uint8_t)((fresh != 0U || drawn_tile[i] != base || drawn_prop[i] != prop) ? 1U : 0U);

            // a 16x32 box, whose eight tiles are [UL, UL+1, UR, UR+1, LL, LL+1, LR, LR+1]: the
            // upper row is base/base+2 and the lower base+4/base+6. a symmetric one stores only its
            // left column, top pair then bottom pair, so the two rows are base and base+2 and every
            // right half is its own mirror. a 16x16 box is just the upper row. flipping vertically
            // swaps the two rows as well as the rows inside each tile, which is the whole of a
            // corpse's animation
            const uint8_t lower = symmetric != 0U ? (uint8_t)(base + 2U) : (uint8_t)(base + 4U);
            const uint8_t up_right = symmetric != 0U ? base : (uint8_t)(base + 2U);
            const uint8_t low_right = symmetric != 0U ? lower : (uint8_t)(base + 6U);
            const uint8_t swapped = (uint8_t)(tall != 0U && flip_y != 0U ? 1U : 0U);
            const uint8_t low_y = (uint8_t)(py + kEnemyHeightPx);

            if (moved == 0U && redrawn == 0U) {
                continue;
            }
            drawn_tile[i] = base;
            drawn_prop[i] = prop;
            drawn_x[i] = px;
            drawn_y[i] = py;
            // the two are written apart on purpose: an enemy that merely walked keeps the tiles and
            // props it already has, and only a pose change (or a slot changing hands) rewrites them
            if (redrawn != 0U) {
                pair_art(slot, swapped != 0U ? lower : base, swapped != 0U ? low_right : up_right, prop,
                         mirror, symmetric);
                if (tall != 0U) {
                    pair_art((uint8_t)(slot + 2U), swapped != 0U ? base : lower,
                             swapped != 0U ? up_right : low_right, prop, mirror, symmetric);
                }
            }
            if (moved != 0U) {
                pair_move(slot, px, py);
                if (tall != 0U) {
                    pair_move((uint8_t)(slot + 2U), px, low_y);
                }
            }
        }
    }
    // past the pool's live end nothing can be drawn, and neither can anything the loop skipped
    for (; i < kEnemySlots; ++i) {
        drawn_slot[i] = 0xFF;
    }
    // and the slots this frame gave back, which the last one still shows an enemy in
    for (i = next; i < oam_used; ++i) {
        move_sprite(i, 0, 0);
    }
    oam_used = next;
    if (popup_busy != 0U) {
        popup_frame(cam_y, next);
    }
}
