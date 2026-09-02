#ifndef POWERUP_H
#define POWERUP_H

#include <gb/gb.h>
#include <stdint.h>

// mario's power state. super and fire share the 16x32 body; fire only changes his palette and what
// b does, which is why the size questions below answer for both
#define kPowerSmall 0U
#define kPowerSuper 1U
#define kPowerFire 2U

// once at boot, before the first level load
void powerup_init(void) BANKED;

// a run start and every death: back to small, no timers, no fireballs. smb1 carries the form from
// level to level, so a level load is not one of these - it takes the call below instead
void powerup_reset(void) BANKED;

// every level load: the star and injury windows, the grow animation and the fireball pool all
// cleared, but the form itself kept, so the level he walks into is the one he cleared the last in
void powerup_enter_level(void) BANKED;

uint8_t powerup_state(void) BANKED;

// the chain's whole per-frame answer, published as ram rather than as five banked getters: bank 5
// is a trampoline away from the game loop and this is read on every frame of every state
#define kPowerFlagBig 0x01U
#define kPowerFlagStar 0x02U
#define kPowerFlagImmune 0x04U
#define kPowerFlagFrozen 0x08U
// nothing to time, nothing to throw and nothing to blink: the game loop skips powerup_update
// outright on those frames rather than pay a trampoline into bank 5 for a counter that does not run
#define kPowerFlagBusy 0x10U
// and the same for the draw pass while no fireball is live or still parked
#define kPowerFlagDrawn 0x20U
extern uint8_t powerup_flags;
// which body the frozen animation is drawing this frame, alternating small and super
extern uint8_t powerup_pose;
// the cgb sprite palette mario's own sprites carry this frame, or kSpriteHidden on a blink frame
extern uint8_t powerup_prop;

// applies a collected kItem* effect; 1 when the pickup froze the world
uint8_t powerup_collect(uint8_t item_kind) BANKED;

// takes one hit off the chain: 1 when the hit was fatal, which is only ever small mario
uint8_t powerup_damage(void) BANKED;

// one frame of the timers, the frozen animation and the fireball pool
void powerup_update(uint8_t keys, uint16_t player_px, int16_t player_py, uint8_t facing_left,
                    uint16_t cam_x) BANKED;

// writes the live fireballs' sprites, or parks the ones that are gone
void powerup_draw(uint16_t cam_x, uint8_t cam_y) BANKED;

#endif
