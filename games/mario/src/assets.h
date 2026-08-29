#ifndef ASSETS_H
#define ASSETS_H

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// every tile and palette this game owns lives in games/mario/src/assets_data.c, banked beside the
// enemy code: m7's super mario alone is 512 bytes of art and bank 0 was already full. nothing but
// the loaders below reaches any of it, and each of them runs with the lcd off at a level load

// loads every terrain tile into vram bank 0 at its pinned id
void assets_load_bg_tiles(void) BANKED;

// loads the eight cgb bg palettes the terrain streamer tags cells with (see kCamPal* in mario.h)
void assets_load_bg_palettes(void) BANKED;

// the same eight slots, darkened and blue-tinted for a pipe sub-area or an underground level, and
// drained to stone with a lava ramp for a castle; bcpd is mode-locked on real hardware, so every
// one of these runs with the lcd off
void assets_load_bg_palettes_underground(void) BANKED;
void assets_load_bg_palettes_castle(void) BANKED;

// loads small mario's six 16x16 frames at 0xe0 and super mario's 32 tiles at 0x60, then the warm
// and fire cgb sprite palettes
void assets_load_sprite_tiles(void) BANKED;
void assets_load_sprite_palettes(void) BANKED;

// loads the item frames at 0xd0 (mushroom, star, 1-up, coin pop, fireball) plus the flower at 0x80,
// then one cgb sprite palette per item kind
void assets_load_item_tiles(void) BANKED;
void assets_load_item_palettes(void) BANKED;

// loads the enemy frames at 0xc0 and the goomba/koopa cgb sprite palettes
void assets_load_enemy_tiles(void) BANKED;
void assets_load_enemy_palettes(void) BANKED;

// m8a's piranha/flame/lift/bowser pairs at 0x84, and the castle re-tint that turns the koopa's
// palette into the fake bowser's
void assets_load_hazard_tiles(void) BANKED;
void assets_load_enemy_palettes_castle(void) BANKED;

// m8b's ten hud digit pairs at 0x8c; they borrow the coin and star sprite palettes, so there is
// no palette loader beside this one
void assets_load_digit_tiles(void) BANKED;

// per block-kind tile ids, indexed by the kBlock* constants in mario.h; one entry per 2x2 corner.
// the streamer reads these on every column it paints, so they are the one asset table bank 0 keeps
extern const uint8_t kBlockTileTl[kBlockKindCount];
extern const uint8_t kBlockTileTr[kBlockKindCount];
extern const uint8_t kBlockTileBl[kBlockKindCount];
extern const uint8_t kBlockTileBr[kBlockKindCount];
// cgb bg palette slot per block kind
extern const uint8_t kBlockPalette[kBlockKindCount];

// what a block kind is to a body standing on it: kFloorSolid, kFloorThin, or neither. m8a's four
// new kinds turned terrain_solid_at's compare chain into six tests on a path the engine walks
// twenty times a frame, so the answer is one indexed load again
#define kFloorSolid 1U
#define kFloorThin 2U
extern const uint8_t kBlockFloor[kBlockKindCount];

#endif
