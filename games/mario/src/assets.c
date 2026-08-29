#include "assets.h"

#include <stdint.h>

// index = kBlock* from mario.h; empty and the two pipe-body kinds fill all four corners alike
const uint8_t kBlockTileTl[kBlockKindCount] = {
    kTileSky,       kTileGroundTop, kTileBrick, kTileQuestionTl, kTileHard,   kTilePipeTl,  kTilePipeTr,
    kTilePipeBodyL, kTilePipeBodyR, kTileHard,  kTileFlagPole,   kTileCastle, kTileSpentTl, kTileCoinTl,
};
const uint8_t kBlockTileTr[kBlockKindCount] = {
    kTileSky,       kTileGroundTop, kTileBrick, kTileQuestionTr, kTileHard,   kTilePipeTl,  kTilePipeTr,
    kTilePipeBodyL, kTilePipeBodyR, kTileHard,  kTileSky,        kTileCastle, kTileSpentTr, kTileCoinTr,
};
const uint8_t kBlockTileBl[kBlockKindCount] = {
    kTileSky,       kTileGroundFill, kTileBrick, kTileQuestionBl, kTileHard,   kTilePipeTl,  kTilePipeTr,
    kTilePipeBodyL, kTilePipeBodyR,  kTileHard,  kTileFlagPole,   kTileCastle, kTileSpentBl, kTileCoinBl,
};
const uint8_t kBlockTileBr[kBlockKindCount] = {
    kTileSky,       kTileGroundFill, kTileBrick, kTileQuestionBr, kTileHard,   kTilePipeTl,  kTilePipeTr,
    kTilePipeBodyL, kTilePipeBodyR,  kTileHard,  kTileSky,        kTileCastle, kTileSpentBr, kTileCoinBr,
};
const uint8_t kBlockPalette[kBlockKindCount] = {
    kCamPalSky,  kCamPalGround, kCamPalBrick,  kCamPalQuestion, kCamPalGround,  kCamPalPipe,  kCamPalPipe,
    kCamPalPipe, kCamPalPipe,   kCamPalGround, kCamPalNeutral,  kCamPalNeutral, kCamPalSpent, kCamPalCoin,
};
