#include "assets.h"

#include <gb/cgb.h>
#include <gb/gb.h>

// low byte then high byte per row, leftmost pixel in bit 7; index = (hi bit, lo bit)
// three tiles: ground top (grass edge over dirt), ground fill (dirt only), hard/stair (bordered stone)
const uint8_t kGroundTiles[48] = {
    0xFF, 0xFF, // 33333333 grass edge
    0xFF, 0xFF, // 33333333 grass edge
    0xFF, 0x00, // 11111111 dirt
    0xDD, 0x00, // 11011101
    0xFF, 0x00, // 11111111
    0xBB, 0x00, // 10111011
    0xFF, 0x00, // 11111111
    0xFF, 0x00, // 11111111
    0xFF, 0x00, // 11111111 dirt (fill tile)
    0xDD, 0x00, // 11011101
    0xFF, 0x00, // 11111111
    0xBB, 0x00, // 10111011
    0xFF, 0x00, // 11111111
    0xEE, 0x00, // 11101110
    0xFF, 0x00, // 11111111
    0x77, 0x00, // 01110111
    0xFF, 0xFF, // 33333333 border (hard/stair tile)
    0x00, 0xFF, // 22222222 fill
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0xFF, 0xFF, // 33333333 border
};

// brick: mortar lines top and mid, fill between
const uint8_t kBrickTile[16] = {
    0xFF, 0xFF, // 33333333 mortar
    0x00, 0xFF, // 22222222 fill
    0x00, 0xFF, // 22222222
    0xFF, 0xFF, // 33333333 mortar
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0xFF, 0xFF, // 33333333 mortar
    0x00, 0xFF, // 22222222
};

// question block face: solid quadrant shading stands in for the "?" glyph this pass; a future art
// pass can trace real glyph pixels once the block is interactive. tiles in id order: top-left
// (dark outline), top-right (gold fill), bottom-left (gold fill), bottom-right (dark outline)
const uint8_t kQuestionTiles[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// pipe: dark rim cap over a two-tone body. tiles in id order: top-left cap (rim + dark fill),
// top-right cap (rim + light fill), body-left (dark, no rim), body-right (light, no rim)
const uint8_t kPipeTiles[64] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                                0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                                0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                                0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00,
                                0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};

// flag pole: a thin vertical line down the tile's center, sky everywhere else
// castle: stone block, same shape as hard but its own tile id per the family map
const uint8_t kFlagCastleTiles[32] = {
    0x18, 0x18, // ...33... pole
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0x18, 0x18, // ...33...
    0xFF, 0xFF, // 33333333 border
    0x00, 0xFF, // 22222222 fill
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0x00, 0xFF, // 22222222
    0xFF, 0xFF, // 33333333 border
};

void assets_load_bg_tiles(void) {
    set_bkg_data(kTileGroundTop, 3, kGroundTiles);
    set_bkg_data(kTileBrick, 1, kBrickTile);
    set_bkg_data(kTileQuestionTl, 4, kQuestionTiles);
    set_bkg_data(kTilePipeTl, 4, kPipeTiles);
    set_bkg_data(kTileFlagPole, 2, kFlagCastleTiles);
}

void assets_load_bg_palettes(void) {
    palette_color_t sky[4] = {RGB(24, 28, 31), RGB(16, 20, 31), RGB(8, 12, 28), RGB(2, 4, 16)};
    palette_color_t ground[4] = {RGB(4, 3, 2), RGB(16, 10, 5), RGB(12, 12, 10), RGB(22, 16, 8)};
    palette_color_t brick[4] = {RGB(6, 3, 2), RGB(14, 6, 3), RGB(20, 8, 4), RGB(10, 4, 2)};
    palette_color_t question[4] = {RGB(6, 4, 0), RGB(24, 18, 3), RGB(28, 22, 4), RGB(20, 14, 2)};
    palette_color_t pipe[4] = {RGB(1, 4, 1), RGB(10, 28, 10), RGB(4, 20, 6), RGB(2, 10, 3)};
    palette_color_t neutral[4] = {RGB(8, 8, 8), RGB(22, 22, 22), RGB(16, 16, 16), RGB(28, 28, 28)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
}

// index = kBlock* from mario.h; empty and the two pipe-body kinds fill all four corners alike
const uint8_t kBlockTileTl[kBlockKindCount] = {
    kTileSky,    kTileGroundTop, kTileBrick,     kTileQuestionTl, kTileHard,     kTilePipeTl,
    kTilePipeTr, kTilePipeBodyL, kTilePipeBodyR, kTileHard,       kTileFlagPole, kTileCastle,
};
const uint8_t kBlockTileTr[kBlockKindCount] = {
    kTileSky,    kTileGroundTop, kTileBrick,     kTileQuestionTr, kTileHard, kTilePipeTl,
    kTilePipeTr, kTilePipeBodyL, kTilePipeBodyR, kTileHard,       kTileSky,  kTileCastle,
};
const uint8_t kBlockTileBl[kBlockKindCount] = {
    kTileSky,    kTileGroundFill, kTileBrick,     kTileQuestionBl, kTileHard,     kTilePipeTl,
    kTilePipeTr, kTilePipeBodyL,  kTilePipeBodyR, kTileHard,       kTileFlagPole, kTileCastle,
};
const uint8_t kBlockTileBr[kBlockKindCount] = {
    kTileSky,    kTileGroundFill, kTileBrick,     kTileQuestionBr, kTileHard, kTilePipeTl,
    kTilePipeTr, kTilePipeBodyL,  kTilePipeBodyR, kTileHard,       kTileSky,  kTileCastle,
};
const uint8_t kBlockPalette[kBlockKindCount] = {
    kCamPalSky,  kCamPalGround, kCamPalBrick, kCamPalQuestion, kCamPalGround,  kCamPalPipe,
    kCamPalPipe, kCamPalPipe,   kCamPalPipe,  kCamPalGround,   kCamPalNeutral, kCamPalNeutral,
};
