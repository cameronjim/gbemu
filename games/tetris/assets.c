#include "assets.h"

// low byte then high byte per row; both planes set gives pixel index 3 everywhere
const uint8_t kBorderTile[16] = {
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
};

// bevel: index1 light top/left edge, index2 dark bottom/right edge, index0 face in between
const uint8_t kGridTile[16] = {
    0xFF, 0x00, // 11111111
    0x80, 0x01, // 1000000 2
    0x80, 0x01, // 1000000 2
    0x80, 0x01, // 1000000 2
    0x80, 0x01, // 1000000 2
    0x80, 0x01, // 1000000 2
    0x80, 0x01, // 1000000 2
    0x00, 0xFF, // 22222222
};

// low plane only: index 1 everywhere
const uint8_t kBackdropTile[16] = {
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
};

// high plane only: index 2 everywhere
const uint8_t kWallTile[16] = {
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
};

const uint8_t kBlockTile[16] = {
    0xFF, 0xFF, // 33333333
    0x81, 0xFE, // 32222221
    0x81, 0xFE, // 32222221
    0x81, 0xFE, // 32222221
    0x81, 0xFE, // 32222221
    0x81, 0xFE, // 32222221
    0x81, 0xFE, // 32222221
    0xFF, 0x00, // 11111111
};

// -######-  serifed I: ink in cell columns 1-6 top and bottom, 3-4 for the stem
// ---##---
// ---##---
// ---##---
// ---##---
// ---##---
// -######-
// --------
const uint8_t kPanelGlyphI[16] = {
    0xFF, 0x7E, 0xFF, 0x18, 0xFF, 0x18, 0xFF, 0x18,
    0xFF, 0x18, 0xFF, 0x18, 0xFF, 0x7E, 0xFF, 0x00,
};

// ---##---  serifed 1: the stock flag kept, a base serif added so the digit sits on the same
// --###---  6 pixel rhythm as 0 and 2-9 instead of leaving a void beside its neighbours
// ---##---
// ---##---
// ---##---
// ---##---
// -######-
// --------
const uint8_t kPanelGlyphOne[16] = {
    0xFF, 0x18, 0xFF, 0x38, 0xFF, 0x18, 0xFF, 0x18,
    0xFF, 0x18, 0xFF, 0x18, 0xFF, 0x7E, 0xFF, 0x00,
};

// slot 0 is unused by the block tile, so it holds the well black the piece sits over
const palette_color_t kPiecePalettes[28] = {
    RGB(1, 1, 3), RGB(0, 12, 16), RGB(0, 24, 28), RGB(20, 31, 31), // i cyan
    RGB(1, 1, 3), RGB(16, 14, 0), RGB(30, 28, 0), RGB(31, 31, 20), // o yellow
    RGB(1, 1, 3), RGB(10, 0, 16), RGB(20, 4, 28), RGB(28, 18, 31), // t purple
    RGB(1, 1, 3), RGB(0, 13, 2),  RGB(4, 26, 6),  RGB(20, 31, 18), // s green
    RGB(1, 1, 3), RGB(15, 0, 0),  RGB(29, 4, 4),  RGB(31, 18, 16), // z red
    RGB(1, 1, 3), RGB(0, 2, 15),  RGB(4, 8, 29),  RGB(16, 20, 31), // j blue
    RGB(1, 1, 3), RGB(17, 6, 0),  RGB(31, 14, 0), RGB(31, 24, 14), // l orange
};

// nearest rgb555 of the old frontend's argb grid: face 0x101014, light 0x1e1e26, dark 0x050508
const palette_color_t kChromePalette[4] = {RGB(2, 2, 2), RGB(4, 4, 5), RGB(1, 1, 1), RGB(31, 31, 31)};
