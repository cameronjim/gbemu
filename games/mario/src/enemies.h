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

// loads the enemy art/palettes, empties the pool and rewinds the spawn cursor to the level's first
// enemy; the level load and every respawn share it
void enemies_load_level(void) BANKED;

// debug: the next enemies_load_level seeds the lab roster instead of the compiled one. 1-1 places
// its one koopa alone and never four enemies on a row, so the chain and per-scanline-cap tests
// need a denser one to watch. see kEnemyLab in mario.h
void enemies_set_lab(uint8_t on) BANKED;

// points the pool at the kArea* grid being played; a sub-area holds no enemies, so entering one
// empties the pool and coming back leaves the cursor exactly where it was - smb never re-triggers
void enemies_enter_area(uint8_t area) BANKED;

// one frame of spawning, motion, enemy-vs-enemy and enemy-vs-mario; returns a kEnemyHit* code
uint8_t enemies_update(uint16_t player_px, int16_t player_py, int8_t player_dy, uint8_t on_ground,
                       uint16_t cam_x) BANKED;

// writes each live enemy's two 8x16 sprites, or parks the ones that are gone or off screen
void enemies_draw(uint16_t cam_x, uint8_t cam_y) BANKED;

// the internal score the hud will read in m8; nothing displays it yet
uint16_t enemies_points(void) BANKED;

#endif
