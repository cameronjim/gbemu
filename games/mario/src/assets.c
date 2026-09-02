// m18's art pass took the block tables from eighteen kinds to thirty-eight (forty-seven now that
// 1-2's sideways pipe and 1-3's tree are in), and six tables of that length is 282 bytes bank 0 no
// longer had. so they ride with the rest of the art and are staged into ram at a level load, the
// way level.c already stages the level table: the streamer's reads stay plain loads with no bank
// switch behind them, and bank 0 carries none of the bytes
#pragma bank 4

#include "assets.h"

#include <stdint.h>
#include <string.h>

// the staged copies, which are what terrain.c actually reads
uint8_t kBlockTileTl[kBlockKindCount];
uint8_t kBlockTileTr[kBlockKindCount];
uint8_t kBlockTileBl[kBlockKindCount];
uint8_t kBlockTileBr[kBlockKindCount];
uint8_t kBlockFloor[kBlockKindCount];
uint8_t kBlockPalette[kBlockKindCount];

// index = kBlock* from mario.h. every block is 2x2 tiles and m18's art pass gives most of them
// four distinct quadrants; the ones that still repeat a tile do it because the shape genuinely
// repeats (a pipe's body has no vertical variation, a castle wall's masonry tiles at 8px, and a
// decorative cell's unused half is sky).
//
// 1-3's four tree kinds close each table: the three canopy caps wear the thin platform's deck art
// and the trunk is sky, placeholders until the art pass draws them
//
// the four mirrored kinds - the right cloud caps, the right hill slope, the right bush cap - carry
// the same tiles as their left twin with the two columns swapped, and set kCamAttrXFlip in their
// palette byte so the hardware does the mirroring, which halves what the scenery costs
// clang-format off
static const uint8_t kTileTlRom[kBlockKindCount] = {
    kTileSky,            kTileGroundTopL,      kTileBrickTl,       kTileQuestionTl,
    kTileHardTl,         kTilePipeLipL,        kTilePipeLipM,      kTilePipeBodyL,
    kTilePipeBodyM,      kTileHardTl,          kTileFlagPole,      kTileCastleWall,
    kTileSpentTl,        kTileCoinTl,          kTileThin,          kTileLavaTop,
    kTileBridge,         kTileAxe,             kTileGroundFillTl,  kTileCastleCrenel,
    kTileCastleWindowTl, kTileCastleDoorTopTl, kTileCastleDoorTl,  kTileFlagBall,
    kTileFlagClothTl,    kTileCloudCapTl,      kTileCloudMidTl,    kTileCloudCapTr,
    kTileCloudCapBl,     kTileCloudMidBl,      kTileCloudCapBr,    kTileHillPeakTl,
    kTileHillSlopeTl,    kTileHillSlopeTr,     kTileHillFillTl,    kTileBushCapTl,
    kTileBushMidTl,      kTileBushCapTr,
    kTilePipeSideTl,     kTilePipeSideMl,      kTilePipeSideBodyT, kTilePipeSideBodyM,
    kTileCastleCrenelInner,
    kTileTreeCapTl,      kTileTreeTop,         kTileTreeTop,       kTileTrunk,
};
// clang-format on
// clang-format off
static const uint8_t kTileTrRom[kBlockKindCount] = {
    kTileSky,            kTileGroundTopR,      kTileBrickTr,       kTileQuestionTr,
    kTileHardTr,         kTilePipeLipM,        kTilePipeLipR,      kTilePipeBodyM,
    kTilePipeBodyR,      kTileHardTr,          kTileSky,           kTileCastleWall,
    kTileSpentTr,        kTileCoinTr,          kTileThin,          kTileLavaTop,
    kTileBridge,         kTileAxe,             kTileGroundFillTr,  kTileCastleCrenel,
    kTileCastleWindowTr, kTileCastleDoorTopTr, kTileCastleDoorTr,  kTileScenBlank,
    kTileFlagClothTr,    kTileCloudCapTr,      kTileCloudMidTr,    kTileCloudCapTl,
    kTileCloudCapBr,     kTileCloudMidBr,      kTileCloudCapBl,    kTileHillPeakTr,
    kTileHillSlopeTr,    kTileHillSlopeTl,     kTileHillFillTr,    kTileBushCapTr,
    kTileBushMidTr,      kTileBushCapTl,
    kTilePipeSideTr,     kTilePipeSideMr,      kTilePipeSideBodyT, kTilePipeSideBodyM,
    kTileCastleCrenelInner,
    kTileTreeTop,        kTileTreeTop,         kTileTreeCapTr,     kTileTrunk,
};
// clang-format on
// clang-format off
static const uint8_t kTileBlRom[kBlockKindCount] = {
    kTileSky,            kTileGroundFillBl,    kTileBrickBl,       kTileQuestionBl,
    kTileHardBl,         kTilePipeLipLb,       kTilePipeLipMb,     kTilePipeBodyL,
    kTilePipeBodyM,      kTileHardBl,          kTileFlagPole,      kTileCastleWall,
    kTileSpentBl,        kTileCoinBl,          kTileThinUnder,     kTileLavaFill,
    kTileSky,            kTileSky,             kTileGroundFillBl,  kTileCastleWall,
    kTileCastleWindowBl, kTileCastleDoorTopBl, kTileCastleDoorBl,  kTileScenPole,
    kTileFlagClothBl,    kTileCloudCapMl,      kTileCloudMidMl,    kTileCloudCapMr,
    kTileCloudCapFl,     kTileCloudMidFl,      kTileCloudCapFr,    kTileHillPeakBl,
    kTileHillSlopeBl,    kTileHillSlopeBr,     kTileHillFillBl,    kTileBushCapBl,
    kTileBushMidBl,      kTileBushCapBr,
    kTilePipeSideMl,     kTilePipeSideBl,      kTilePipeSideBodyM, kTilePipeSideBodyB,
    kTileCastleWall,
    kTileTreeCapBl,      kTileTreeBotM,        kTileTreeBot,       kTileTrunk,
};
// clang-format on
// clang-format off
static const uint8_t kTileBrRom[kBlockKindCount] = {
    kTileSky,            kTileGroundFillBr,    kTileBrickBr,       kTileQuestionBr,
    kTileHardBr,         kTilePipeLipMb,       kTilePipeLipRb,     kTilePipeBodyM,
    kTilePipeBodyR,      kTileHardBr,          kTileSky,           kTileCastleWall,
    kTileSpentBr,        kTileCoinBr,          kTileThinUnder,     kTileLavaFill,
    kTileSky,            kTileSky,             kTileGroundFillBr,  kTileCastleWall,
    kTileCastleWindowBr, kTileCastleDoorTopBr, kTileCastleDoorBr,  kTileScenBlank,
    kTileFlagClothBr,    kTileCloudCapMr,      kTileCloudMidMr,    kTileCloudCapMl,
    kTileCloudCapFr,     kTileCloudMidFr,      kTileCloudCapFl,    kTileHillPeakBr,
    kTileHillSlopeBr,    kTileHillSlopeBl,     kTileHillFillBr,    kTileBushCapBr,
    kTileBushMidBr,      kTileBushCapBl,
    kTilePipeSideMr,     kTilePipeSideBr,      kTilePipeSideBodyM, kTilePipeSideBodyB,
    kTileCastleWall,
    kTileTreeBot,        kTileTreeBotM,        kTileTreeCapBr,     kTileTrunk,
};
// clang-format on
// sky, the flag's three cells, a world coin, the axe, lava and every scenery kind are all
// walk-through; a thin platform stops only feet that crossed its deck line, which is the caller's
// test to make. the castle is scenery too - mario's walk-off after the flag ends inside it
// clang-format off
static const uint8_t kFloorRom[kBlockKindCount] = {
    0,           kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid,
    kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid, 0,           0,
    kFloorSolid, 0,           kFloorThin,  0,           kFloorSolid, 0,
    kFloorSolid, 0,           0,           0,           0,           0,
    0,           0,           0,           0,           0,           0,
    0,           0,           0,           0,           0,           0,
    0,           0,
    kFloorSolid, kFloorSolid, kFloorSolid, kFloorSolid,
    0,
    kFloorSolid, kFloorSolid, kFloorSolid, 0,
};
// clang-format on
// lava borrows the coin slot, which no castle grid ever paints a world coin with; the bridge, the
// axe and the thin platform are wood and take the neutral one. the hills, the bushes and the flag
// share the pipe's greens, the castle shares the brick's browns, and the clouds and the pennant
// share the sky's whites - and the sideways pipe is the vertical one rotated, so it shares those
// greens too, and so does 1-3's tree canopy, whose four rip colors are exactly the pipe slot's
// (sky, bright green, dark green, black); its trunk takes the brick's browns - which is how eight
// cgb slots still cover all forty-seven kinds.
//
// the kScen* entries add kCamAttrVram1: every kind whose art assets_load_scenery_tiles put in vram
// bank 1 has to say so here, because the attribute byte is what picks the bank a cell reads from
#define kScenSky (kCamPalSky | kCamAttrVram1)
#define kScenPipe (kCamPalPipe | kCamAttrVram1)
#define kScenBrick (kCamPalBrick | kCamAttrVram1)
// clang-format off
static const uint8_t kPaletteRom[kBlockKindCount] = {
    kCamPalSky,     kCamPalGround,  kCamPalBrick,   kCamPalQuestion,
    kCamPalBrick,   kCamPalPipe,    kCamPalPipe,    kCamPalPipe,
    kCamPalPipe,    kCamPalBrick,   kCamPalPipe,    kScenBrick,
    kCamPalSpent,   kCamPalCoin,    kCamPalNeutral, kCamPalCoin | kCamAttrVram1,
    kCamPalNeutral, kCamPalNeutral, kCamPalGround,  kScenBrick,
    kScenBrick,     kScenBrick,     kScenBrick,     kScenPipe,
    kScenSky,       kScenSky,       kScenSky,       kScenSky | kCamAttrXFlip,
    kScenSky,       kScenSky,       kScenSky | kCamAttrXFlip,
    kScenPipe,      kScenPipe,      kScenPipe | kCamAttrXFlip,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenPipe | kCamAttrXFlip,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenPipe,
    kScenBrick,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenBrick,
};
// clang-format on

void assets_load_block_tables(void) BANKED {
    memcpy(kBlockTileTl, kTileTlRom, kBlockKindCount);
    memcpy(kBlockTileTr, kTileTrRom, kBlockKindCount);
    memcpy(kBlockTileBl, kTileBlRom, kBlockKindCount);
    memcpy(kBlockTileBr, kTileBrRom, kBlockKindCount);
    memcpy(kBlockFloor, kFloorRom, kBlockKindCount);
    memcpy(kBlockPalette, kPaletteRom, kBlockKindCount);
}
