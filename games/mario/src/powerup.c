// bank 0 was full again by m8a: the level table, the multi-level loader and the new collision
// paths all live there, so the powerup chain moved out beside hazards.c. it is entered a handful
// of times a frame, which is a handful of trampolines against a couple of hundred cycles
#pragma bank 5

#include "powerup.h"

#include "enemies.h"
#include "hud.h"
#include "mario.h"
#include "physics_constants.h"
#include "terrain.h"

#include <gb/gb.h>
#include <stdint.h>

uint8_t powerup_flags;
uint8_t powerup_pose;
uint8_t powerup_prop;

static uint8_t power;
// the bible's two windows, both already multiplied out of smb's 21-frame interval timer
static uint16_t star_timer;
static uint8_t injury_timer;

// the frozen grow/shrink animation: how long is left and which state it lands on
static uint8_t anim_timer;
static uint8_t anim_target;
static uint8_t anim_from_big;
static uint8_t anim_to_big;

// one shared counter drives both the injury blink and the star flash, so neither carries its own
static uint8_t phase;
// smb's rule: a b press throws, holding b keeps running, so the throw is edge triggered
static uint8_t b_prev;

typedef struct {
    uint16_t pos_x;
    int16_t pos_y;
    int8_t dir;
    int8_t dy;
    uint8_t x_force;
    uint8_t y_accum;
    // the spin: counts every frame the ball is alive. bit 2 (every 4 frames) picks which of the two
    // drawn tiles is shown, bit 3 (every 8 frames) flips the sprite both ways - the disassembly's
    // own cadence for smb's fireball, see powerup_draw
    uint8_t anim;
} Fireball;

// packed into [0, live) the way the enemy pool is, so an empty pool costs one compare a frame
static Fireball balls[kFireballSlots];
static uint8_t live;
static uint8_t shown;

static void publish(void);

static int16_t row_of(int16_t py) {
    return py < 0 ? (int16_t)-1 : (int16_t)(py >> 4);
}

static void begin_anim(uint8_t target, uint8_t from_big, uint8_t to_big) {
    anim_timer = (uint8_t)kGrowFrames;
    anim_target = target;
    anim_from_big = from_big;
    anim_to_big = to_big;
}

void powerup_init(void) BANKED {
    powerup_reset();
}

void powerup_reset(void) BANKED {
    power = kPowerSmall;
    star_timer = 0;
    injury_timer = 0;
    anim_timer = 0;
    anim_target = kPowerSmall;
    anim_from_big = 0;
    anim_to_big = 0;
    phase = 0;
    b_prev = 0;
    live = 0;
    // not the parked marker: a respawn leaves the last life's balls in oam, so the first draw of
    // the new one has to write every slot off screen
    shown = (uint8_t)kFireballSlots;
    publish();
}

uint8_t powerup_state(void) BANKED {
    return power;
}

// every read the game loop makes, worked out once here at the end of whatever changed the chain
static void publish(void) {
    const uint8_t big = power != kPowerSmall ? 1U : 0U;

    powerup_flags =
        (uint8_t)((big != 0U ? kPowerFlagBig : 0U) | (star_timer != 0U ? kPowerFlagStar : 0U) |
                  (injury_timer != 0U ? kPowerFlagImmune : 0U) | (anim_timer != 0U ? kPowerFlagFrozen : 0U) |
                  (power == kPowerFire || star_timer != 0U || injury_timer != 0U || anim_timer != 0U ||
                           live != 0U
                       ? kPowerFlagBusy
                       : 0U) |
                  (live != 0U || shown != 0U ? kPowerFlagDrawn : 0U));
    if (anim_timer == 0U) {
        powerup_pose = big;
    } else {
        powerup_pose = ((uint8_t)(anim_timer / kGrowFlipFrames) & 1U) != 0U ? anim_from_big : anim_to_big;
    }
    if (injury_timer != 0U && (phase & kBlinkMask) != 0U) {
        powerup_prop = (uint8_t)kSpriteHidden;
    } else if (star_timer != 0U && (phase & kStarFlashMask) != 0U) {
        powerup_prop = (uint8_t)kPalStar;
    } else {
        powerup_prop = power == kPowerFire ? (uint8_t)kPalFire : (uint8_t)kPalMario;
    }
}

uint8_t powerup_collect(uint8_t item_kind) BANKED {
    // roster.json pays the same flat figure for any powerup taken; the 1-up pays a life instead
    if (item_kind != kItemOneup) {
        hud_score = (uint16_t)(hud_score + kScoreTens(kPowerupPoints));
    }
    if (item_kind == kItemStar) {
        star_timer = (uint16_t)kStarFrames;
        publish();
        return 0;
    }
    if (item_kind == kItemOneup) {
        hud_add_life();
        return 0;
    }
    if (item_kind == kItemFlower) {
        // the dispenser only pays a flower to a grown mario, so this is a palette change alone;
        // smb would grow a small mario instead, which the branch below still covers
        if (power != kPowerSmall) {
            power = kPowerFire;
            publish();
            return 0;
        }
        begin_anim(kPowerFire, 0, 1);
        publish();
        return 1;
    }
    if (item_kind != kItemMushroom || power != kPowerSmall) {
        return 0;
    }
    begin_anim(kPowerSuper, 0, 1);
    publish();
    return 1;
}

uint8_t powerup_damage(void) BANKED {
    if (star_timer != 0U || injury_timer != 0U || anim_timer != 0U) {
        return 0;
    }
    // roster.json, Mario (Fire): "reverts directly to Small Mario if hit (no cushioning step)"
    if (power != kPowerSmall) {
        begin_anim(kPowerSmall, 1, 0);
        publish();
        return 0;
    }
    return 1;
}

// smb's own throw position: level with his chest, a few px in front of the box he stands in
static void throw_ball(uint16_t player_px, int16_t player_py, uint8_t facing_left) {
    Fireball* f = &balls[live];

    ++live;
    f->dir = facing_left != 0U ? (int8_t)-1 : (int8_t)1;
    if (facing_left != 0U) {
        f->pos_x = player_px > (uint16_t)kFireballLeadPx ? (uint16_t)(player_px - kFireballLeadPx) : 0U;
    } else {
        f->pos_x = (uint16_t)(player_px + kPlayerWidthPx - kFireballPx + kFireballLeadPx);
    }
    f->pos_y = (int16_t)(player_py + kPlayerHeightPx);
    f->dy = (int8_t)kFireballLaunchDy;
    f->x_force = 0;
    f->y_accum = 0;
    f->anim = 0;
}

// MoveObjectHorizontally again: high nibble whole px, low nibble sixteenths through a 1/256 carry
static void move_ball_x(Fireball* f) {
    const uint8_t raw = (uint8_t)(f->dir > 0 ? (int8_t)kFireballSubpx : (int8_t)(-(int16_t)kFireballSubpx));
    const uint16_t sum = (uint16_t)((uint16_t)f->x_force + (uint16_t)((uint16_t)(raw & 0x0FU) << 4));
    int16_t whole = (int16_t)(raw >> 4);
    int16_t next;

    if (whole >= 8) {
        whole = (int16_t)(whole - 16);
    }
    f->x_force = (uint8_t)sum;
    next = (int16_t)((int16_t)f->pos_x + whole + (int16_t)(sum >> 8));
    if (next < 0) {
        next = 0;
    }
    f->pos_x = (uint16_t)next;
}

// one frame of a live ball; 1 when it is spent and the slot should be freed
static uint8_t step_ball(Fireball* f, uint16_t cam_x) {
    uint16_t sum;
    int16_t lead;
    int16_t row;

    ++f->anim;
    move_ball_x(f);
    lead = (int16_t)(f->dir > 0 ? (int16_t)(f->pos_x + kFireballPx - 1) : (int16_t)f->pos_x);
    if (terrain_solid_at((int16_t)(lead >> 4), row_of(f->pos_y)) != 0U) {
        return 1; // a wall ends it; smb's fireball only bounces off floors
    }

    sum = (uint16_t)((uint16_t)f->y_accum + (uint16_t)kFireballGravity);
    f->y_accum = (uint8_t)sum;
    if (sum > 0xFFU) {
        f->dy = (int8_t)(f->dy + 1);
        if (f->dy > (int8_t)kFireballMaxFallPx) {
            f->dy = (int8_t)kFireballMaxFallPx;
        }
    }
    f->pos_y = (int16_t)(f->pos_y + f->dy);
    if (f->dy > 0) {
        row = row_of((int16_t)(f->pos_y + kFireballPx - 1));
        if (terrain_solid_at((int16_t)(f->pos_x >> 4), row) != 0U) {
            f->pos_y = (int16_t)(((int16_t)row << 4) - kFireballPx);
            f->dy = (int8_t)kFireballBouncePx;
            f->y_accum = 0;
        }
    }
    if (f->pos_y > (int16_t)kLevelHeightPx) {
        return 1;
    }
    if ((int16_t)f->pos_x + kFireballPx < (int16_t)cam_x ||
        (int16_t)f->pos_x > (int16_t)(cam_x + kScreenWidthPx)) {
        return 1;
    }
    return enemies_fireball_hit(f->pos_x, f->pos_y);
}

void powerup_update(uint8_t keys, uint16_t player_px, int16_t player_py, uint8_t facing_left,
                    uint16_t cam_x) BANKED {
    uint8_t i = 0;

    ++phase;
    if (anim_timer != 0U) {
        --anim_timer;
        if (anim_timer == 0U) {
            power = anim_target;
            if (anim_to_big == 0U) {
                // the shrink is the only one that leaves him blinking afterwards
                injury_timer = (uint8_t)kInjuryFrames;
            }
        }
        publish();
        return;
    }
    if (star_timer != 0U) {
        --star_timer;
    }
    if (injury_timer != 0U) {
        --injury_timer;
    }

    if (power == kPowerFire && (keys & J_B) != 0U && b_prev == 0U && live < (uint8_t)kFireballSlots) {
        throw_ball(player_px, player_py, facing_left);
    }
    b_prev = (keys & J_B) != 0U ? 1U : 0U;

    while (i < live) {
        if (step_ball(&balls[i], cam_x) != 0U) {
            --live;
            balls[i] = balls[live];
            continue;
        }
        ++i;
    }
    publish();
}

void powerup_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    uint8_t i;

    if (live == 0U && shown == 0U) {
        return;
    }
    for (i = 0; i < live; ++i) {
        const Fireball* f = &balls[i];
        const uint8_t oam = (uint8_t)(kSpriteFireFirst + i);
        const int16_t sx = (int16_t)((int16_t)f->pos_x - (int16_t)cam_x);
        // the pair's top tile is blank, so the sprite is drawn a tile higher than the ball itself
        const int16_t sy = (int16_t)(f->pos_y - (int16_t)cam_y - kFireballPx);
        // the disassembly's spin: the drawn tile alternates every 4 frames (S_BANK picks bank 1's
        // frame B in place of bank 0's frame A) and the sprite is flipped both ways every 8 - two
        // independent cadences that together read as a tumbling ball, drawn from kPalCoin's
        // saturated gold/orange rather than the star's near-white set
        const uint8_t spin_bank = ((f->anim >> 2) & 1U) != 0U ? (uint8_t)S_BANK : 0U;
        const uint8_t spin_flip = ((f->anim >> 3) & 1U) != 0U ? (uint8_t)(S_FLIPX | S_FLIPY) : 0U;

        set_sprite_tile(oam, (uint8_t)kTileFireball);
        set_sprite_prop(oam, (uint8_t)(kPalCoin | spin_bank | spin_flip));
        move_sprite(oam, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    }
    for (; i < shown; ++i) {
        move_sprite((uint8_t)(kSpriteFireFirst + i), 0, 0);
    }
    shown = live;
}
