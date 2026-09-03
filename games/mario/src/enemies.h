#ifndef ENEMIES_H
#define ENEMIES_H

#include <gb/gb.h>
#include <stdint.h>

// what one frame of enemy contact did to mario; the caller owns his reaction, so nothing in here
// reaches into the player module
#define kEnemyHitNone 0U
#define kEnemyHitDamage 1U
#define kEnemyHitStomp 2U
#define kEnemyHitShellStomp 3U

// loads the enemy art/palettes, empties the pool and arms every roster cell again; the level load
// and every respawn share it, which is what puts a killed enemy back for the next life
void enemies_load_level(void) BANKED;

// debug: the next enemies_load_level seeds the lab roster instead of the compiled one. 1-1 places
// its one koopa alone and never four enemies on a row, so the chain and per-scanline-cap tests
// need a denser one to watch. see kEnemyLab in mario.h
void enemies_set_lab(uint8_t on) BANKED;

// points the pool at the kArea* grid being played; a sub-area holds no enemies, so entering one
// empties the pool and coming back leaves the cursor exactly where it was - smb never re-triggers
void enemies_enter_area(uint8_t area) BANKED;

// what mario's own state does to contact this frame, packed into one byte so the call stays cheap
#define kEnemyFlagGrounded 0x01U
#define kEnemyFlagStar 0x02U
#define kEnemyFlagImmune 0x04U

// one frame of spawning, motion, enemy-vs-enemy and enemy-vs-mario; returns a kEnemyHit* code
uint8_t enemies_update(uint16_t player_px, int16_t player_py, uint8_t player_h, int8_t player_dy,
                       uint8_t flags, uint16_t cam_x) BANKED;

// an 8x8 fireball at (px, py): kills whatever it overlaps and pays the roster's flat per-kind
// figure. 1 when it hit something, which is also the frame the ball itself is spent
uint8_t enemies_fireball_hit(uint16_t px, int16_t py) BANKED;

// writes each live enemy's two 8x16 sprites, or parks the ones that are gone or off screen
void enemies_draw(uint16_t cam_x, uint8_t cam_y) BANKED;

#endif
