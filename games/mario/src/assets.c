#include "assets.h"

#include <stdint.h>

// index = kBlock* from mario.h; empty and the two pipe-body kinds fill all four corners alike.
// a thin platform is a deck across its top half and sky below, so only its upper corners are lit
const uint8_t kBlockTileTl[kBlockKindCount] = {
    kTileSky,     kTileGroundTop, kTileBrick,     kTileQuestionTl, kTileHard,     kTilePipeTl,
    kTilePipeTr,  kTilePipeBodyL, kTilePipeBodyR, kTileHard,       kTileFlagPole, kTileCastle,
    kTileSpentTl, kTileCoinTl,    kTileThin,      kTileLavaTop,    kTileBridge,   kTileAxe,
};
const uint8_t kBlockTileTr[kBlockKindCount] = {
    kTileSky,     kTileGroundTop, kTileBrick,     kTileQuestionTr, kTileHard,   kTilePipeTl,
    kTilePipeTr,  kTilePipeBodyL, kTilePipeBodyR, kTileHard,       kTileSky,    kTileCastle,
    kTileSpentTr, kTileCoinTr,    kTileThin,      kTileLavaTop,    kTileBridge, kTileAxe,
};
const uint8_t kBlockTileBl[kBlockKindCount] = {
    kTileSky,     kTileGroundFill, kTileBrick,     kTileQuestionBl, kTileHard,     kTilePipeTl,
    kTilePipeTr,  kTilePipeBodyL,  kTilePipeBodyR, kTileHard,       kTileFlagPole, kTileCastle,
    kTileSpentBl, kTileCoinBl,     kTileThinUnder, kTileLavaFill,   kTileSky,      kTileSky,
};
const uint8_t kBlockTileBr[kBlockKindCount] = {
    kTileSky,     kTileGroundFill, kTileBrick,     kTileQuestionBr, kTileHard, kTilePipeTl,
    kTilePipeTr,  kTilePipeBodyL,  kTilePipeBodyR, kTileHard,       kTileSky,  kTileCastle,
    kTileSpentBr, kTileCoinBr,     kTileThinUnder, kTileLavaFill,   kTileSky,  kTileSky,
};
// sky, the flag pole, a world coin, the axe and lava are all walk-through; a thin platform stops
// only feet that crossed its deck line, which is the caller's test to make
const uint8_t kBlockFloor[kBlockKindCount] = {
    0,           kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid,
    kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid, 0,           kFloorSolid,
    kFloorSolid, 0,           kFloorThin,  0,           kFloorSolid, 0,
};
// lava borrows the coin slot, which no castle grid ever paints a world coin with; the bridge and
// the thin platform are both wood and take the ground's
const uint8_t kBlockPalette[kBlockKindCount] = {
    kCamPalSky,   kCamPalGround, kCamPalBrick,  kCamPalQuestion, kCamPalGround,  kCamPalPipe,
    kCamPalPipe,  kCamPalPipe,   kCamPalPipe,   kCamPalGround,   kCamPalNeutral, kCamPalNeutral,
    kCamPalSpent, kCamPalCoin,   kCamPalGround, kCamPalCoin,     kCamPalGround,  kCamPalNeutral,
};
