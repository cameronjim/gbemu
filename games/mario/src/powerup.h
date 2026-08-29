#ifndef POWERUP_H
#define POWERUP_H

#include <stdint.h>

// mario's power state. super and fire share the 16x32 body; fire only changes his palette and what
// b does, which is why the size questions below answer for both
#define kPowerSmall 0U
#define kPowerSuper 1U
#define kPowerFire 2U

// once at boot: seeds the lives counter a respawn must not touch
void powerup_init(void);

// every level load and respawn: back to small, no timers, no fireballs
void powerup_reset(void);

uint8_t powerup_state(void);

// 1 while he carries the 16x32 body, which is what sizes his hitbox and lets him break bricks
uint8_t powerup_big(void);

// 1 while the star is running; enemy contact kills the enemy instead of him
uint8_t powerup_star(void);

// 1 during the post-injury window, when contact does nothing at all in either direction
uint8_t powerup_immune(void);

// 1 while the grow/shrink animation owns the frame; smb freezes the whole world for it
uint8_t powerup_frozen(void);

// which body the frozen animation is drawing this frame, alternating small and super
uint8_t powerup_pose_big(void);

// applies a collected kItem* effect; 1 when the pickup froze the world
uint8_t powerup_collect(uint8_t item_kind);

// takes one hit off the chain: 1 when the hit was fatal, which is only ever small mario
uint8_t powerup_damage(void);

// one frame of the timers, the frozen animation and the fireball pool
void powerup_update(uint8_t keys, uint16_t player_px, int16_t player_py, uint8_t facing_left, uint16_t cam_x);

// writes the live fireballs' sprites, or parks the ones that are gone
void powerup_draw(uint16_t cam_x, uint8_t cam_y);

// the cgb sprite palette mario's own sprites carry this frame, or kSpriteHidden on a blink frame
uint8_t powerup_sprite_prop(void);

// the internal lives counter m8's hud will read; a 1-up is all that moves it for now
uint8_t powerup_lives(void);

#endif
