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

// four tiles at 0xb0: pipe top-left/top-right cap, then body-left/body-right
extern const uint8_t kPipeTiles[64];

// two tiles at 0xb8: flag pole, then the castle stone placeholder
extern const uint8_t kFlagCastleTiles[32];

// loads every terrain tile above into vram bank 0 at its pinned id
void assets_load_bg_tiles(void);

// loads the six cgb bg palettes the terrain streamer tags cells with (see kCamPal* in mario.h)
void assets_load_bg_palettes(void);

// per block-kind tile ids, indexed by the kBlock* constants in mario.h; one entry per 2x2 corner
extern const uint8_t kBlockTileTl[kBlockKindCount];
extern const uint8_t kBlockTileTr[kBlockKindCount];
extern const uint8_t kBlockTileBl[kBlockKindCount];
extern const uint8_t kBlockTileBr[kBlockKindCount];
// cgb bg palette slot per block kind
extern const uint8_t kBlockPalette[kBlockKindCount];

#endif
