#ifndef ASSETS_H
#define ASSETS_H

#include "mario.h"

#include <stdint.h>

// three tiles at 0xa0: ground top (grass edge), ground fill (dirt), hard/stair stone
extern const uint8_t kGroundTiles[48];

// one tile at 0xa4: brick, mortar lines top/bottom
extern const uint8_t kBrickTile[16];

// four tiles at 0xa8: the lit question block's 2x2 face, top-left/top-right/bottom-left/bottom-right
extern const uint8_t kQuestionTiles[64];

// four tiles at 0xac: the spent block's 2x2 face, a flat bordered slab in the question family
extern const uint8_t kSpentTiles[64];

// four tiles at 0xb0: pipe top-left/top-right cap, then body-left/body-right
extern const uint8_t kPipeTiles[64];

// two tiles at 0xb8: flag pole, then the castle stone placeholder
extern const uint8_t kFlagCastleTiles[32];

// four tiles at 0xbc: one world coin's 2x2 quadrants, centered in an otherwise empty cell
extern const uint8_t kCoinTiles[64];

// fourteen tiles at 0xd0: mushroom, star and 1-up as 16x16 frames stored mario's way (left
// top/bottom then right top/bottom), then the 8x16 coin a block pays out
extern const uint8_t kItemTiles[224];

// twenty-four tiles at 0xe0: six 16x16 frames (idle, three walk, skid, jump), each stored as
// left-top, left-bottom, right-top, right-bottom so one 8x16 sprite pair covers a frame
extern const uint8_t kMarioTiles[384];

// sixteen tiles at 0xc0: the goomba's two walk frames and its squashed one, then the shell, all
// left-right symmetric so each is a single 8x16 pair the right sprite redraws flipped; then the
// koopa's two walk frames, which face and so carry both halves the way mario's frames do
extern const uint8_t kEnemyTiles[256];

// loads every terrain tile above into vram bank 0 at its pinned id
void assets_load_bg_tiles(void);

// loads the eight cgb bg palettes the terrain streamer tags cells with (see kCamPal* in mario.h)
void assets_load_bg_palettes(void);

// the same eight slots, darkened and blue-tinted for a pipe sub-area; bcpd is mode-locked on real
// hardware, so both loaders run with the lcd off
void assets_load_bg_palettes_underground(void);

// loads small mario's frames at 0xe0 and his warm cgb sprite palette
void assets_load_sprite_tiles(void);
void assets_load_sprite_palettes(void);

// loads the item frames at 0xd0 and one cgb sprite palette per item kind
void assets_load_item_tiles(void);
void assets_load_item_palettes(void);

// loads the enemy frames at 0xc0 and the goomba/koopa cgb sprite palettes
void assets_load_enemy_tiles(void);
void assets_load_enemy_palettes(void);

// per block-kind tile ids, indexed by the kBlock* constants in mario.h; one entry per 2x2 corner
extern const uint8_t kBlockTileTl[kBlockKindCount];
extern const uint8_t kBlockTileTr[kBlockKindCount];
extern const uint8_t kBlockTileBl[kBlockKindCount];
extern const uint8_t kBlockTileBr[kBlockKindCount];
// cgb bg palette slot per block kind
extern const uint8_t kBlockPalette[kBlockKindCount];

#endif
