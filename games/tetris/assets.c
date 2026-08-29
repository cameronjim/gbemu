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

// a centered 3x5 stroke: one blank row on top, five content rows, two blank rows on the bottom;
// three columns centered in the eight (bits 5,4,3). the low plane is 1 across the whole row, so
// every unlit pixel is index 1 - the same "panel backdrop" shade kBackdropTile paints outside
// these tiles - and only the high plane's mask bits promote a stroke pixel to index 3.
#define kGL 0x20U // left column only
#define kGM 0x10U // middle column only
#define kGR 0x08U // right column only
#define kGLR (kGL | kGR)
#define kGLM (kGL | kGM)
#define kGFull (kGL | kGM | kGR)
#define kGRow(m) 0xFFU, (uint8_t)(m)
#define kGBlank 0xFFU, 0x00U

const uint8_t kPanelDigitTiles[10][16] = {
    {kGBlank, kGRow(kGFull), kGRow(kGLR), kGRow(kGLR), kGRow(kGLR), kGRow(kGFull), kGBlank, kGBlank},   // 0
    {kGBlank, kGRow(kGM), kGRow(kGLM), kGRow(kGM), kGRow(kGM), kGRow(kGFull), kGBlank, kGBlank},        // 1
    {kGBlank, kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGBlank, kGBlank},   // 2
    {kGBlank, kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGBlank, kGBlank},   // 3
    {kGBlank, kGRow(kGLR), kGRow(kGLR), kGRow(kGFull), kGRow(kGR), kGRow(kGR), kGBlank, kGBlank},       // 4
    {kGBlank, kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGBlank, kGBlank},   // 5
    {kGBlank, kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGRow(kGLR), kGRow(kGFull), kGBlank, kGBlank},  // 6
    {kGBlank, kGRow(kGFull), kGRow(kGR), kGRow(kGM), kGRow(kGM), kGRow(kGM), kGBlank, kGBlank},         // 7
    {kGBlank, kGRow(kGFull), kGRow(kGLR), kGRow(kGFull), kGRow(kGLR), kGRow(kGFull), kGBlank, kGBlank}, // 8
    {kGBlank, kGRow(kGFull), kGRow(kGLR), kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGBlank, kGBlank},  // 9
};

// order matches panel.c's glyph lookup string "CEILNORSTVX"
const uint8_t kPanelLetterTiles[11][16] = {
    {kGBlank, kGRow(kGFull), kGRow(kGL), kGRow(kGL), kGRow(kGL), kGRow(kGFull), kGBlank, kGBlank},    // c
    {kGBlank, kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGBlank, kGBlank}, // e
    {kGBlank, kGRow(kGFull), kGRow(kGM), kGRow(kGM), kGRow(kGM), kGRow(kGFull), kGBlank, kGBlank},    // i
    {kGBlank, kGRow(kGL), kGRow(kGL), kGRow(kGL), kGRow(kGL), kGRow(kGFull), kGBlank, kGBlank},       // l
    {kGBlank, kGRow(kGLR), kGRow(kGFull), kGRow(kGFull), kGRow(kGLR), kGRow(kGLR), kGBlank, kGBlank}, // n
    {kGBlank, kGRow(kGFull), kGRow(kGLR), kGRow(kGLR), kGRow(kGLR), kGRow(kGFull), kGBlank, kGBlank}, // o
    {kGBlank, kGRow(kGFull), kGRow(kGLR), kGRow(kGFull), kGRow(kGLM), kGRow(kGLR), kGBlank, kGBlank}, // r
    {kGBlank, kGRow(kGFull), kGRow(kGL), kGRow(kGFull), kGRow(kGR), kGRow(kGFull), kGBlank, kGBlank}, // s
    {kGBlank, kGRow(kGFull), kGRow(kGM), kGRow(kGM), kGRow(kGM), kGRow(kGM), kGBlank, kGBlank},       // t
    {kGBlank, kGRow(kGLR), kGRow(kGLR), kGRow(kGLR), kGRow(kGLR), kGRow(kGM), kGBlank, kGBlank},      // v
    {kGBlank, kGRow(kGLR), kGRow(kGLR), kGRow(kGM), kGRow(kGLR), kGRow(kGLR), kGBlank, kGBlank},      // x
};

#undef kGL
#undef kGM
#undef kGR
#undef kGLR
#undef kGLM
#undef kGFull
#undef kGRow
#undef kGBlank
