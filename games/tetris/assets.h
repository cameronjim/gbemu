#ifndef ASSETS_H
#define ASSETS_H

#include <gb/cgb.h>
#include <stdint.h>

// one solid 8x8 tile at 0x60, every pixel index 3; the border strip's own cgb palette colours it
extern const uint8_t kBorderTile[16];
// the empty well cell: a beveled grid square, light top/left edge, dark bottom/right, chrome palette
extern const uint8_t kGridTile[16];
// every pixel index 1: the panel backdrop either side of the well
extern const uint8_t kBackdropTile[16];
// every pixel index 2: the well walls
extern const uint8_t kWallTile[16];
// a bevelled block: index 3 highlight, index 2 body, index 1 shadow, no transparent pixel
extern const uint8_t kBlockTile[16];

// seven four-colour palettes, one per piece, in i o t s z j l order
extern const palette_color_t kPiecePalettes[28];
// well black, panel dark, wall slate, flash white
extern const palette_color_t kChromePalette[4];

#endif
