// m18's art pass took the block tables from eighteen kinds to thirty-eight (forty-eight now that
// 1-2's sideways pipe, 1-3's tree and the centred flag's pennant cell are in), and six tables of
// that length is 288 bytes bank 0 no longer had. so they ride with the rest of the art and are
// staged into ram at a level load, the
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
// index 47 is the parallel flagpole pass's kBlockFlagPoleCloth: a reserved sky row here, so the
// castle's stone keeps index 48 whichever pass merges first
//
// kBlockCastleBrick comes after that, its top pair the masonry's upper course and its bottom pair
// the one offset half a brick under it, which is the running bond the rip lays its wall in; then
// kBlockLavaFill, kTileLavaDeep in all four quadrants, which is a pit's rows under the surface one
//
// kBlockCastleWindowRight closes every table: kBlockCastleWindow's own four tiles with the two
// columns swapped, which is the window the capture draws on the far side of the tower's middle
// column. it cannot be an x flip of the left cell (that moves the masonry's mortar joint), and
// swapping ids costs no vram
//
// kBlockHillCore closes every table after that: a big hill's flat belly cell, kTileHillFillTl in
// all four quadrants. the fill cell carries smb's shading mark in its upper right and the bottom
// row's centre carries none, so the two cannot be the same kind - but they can and do share the
// one flat tile, which is why the kind costs no vram
//
// kBlockBridgeChain closes every table: the cell over the bridge's last column, where the smbd
// castle capture draws the chain running up to the axe through the upper right and lower left
// quadrants (gen/chain.c) and nothing in the other two, which are kTileScenBlank
//
// kBlockPipeJointT and kBlockPipeJointB come before it: the shaft cell a
// sideways pipe's body runs into, in the mouth's top row and its bottom one. each wears its own
// left column (gen/pipe_joint.c, off the 1-2 capture) and the plain body's right column, which is
// kTilePipeBodyM copied into bank 1 because the cell's attribute byte picks one bank for all four
//
// kBlockHillSlopeR and kBlockTreeTopR are the two kinds the hardware mirrors: each carries its
// left twin's own tiles with the two columns swapped and kCamAttrXFlip in its palette byte, which
// the captures agree with to the pixel (the 1-3 capture's tree cap mirrors its twin exactly, which
// is what let m26's re-cut drop the two tiles the hand-drawn right cap used to spend). the right
// cloud caps and the right bush cap used to ride the same trick and no longer can - the capture
// draws each of them differently from its left twin (28, 26 and 13 px, see rip_tiles.py's
// relations report), so they carry tiles of their own
// clang-format off
static const uint8_t kTileTlRom[kBlockKindCount] = {
    kTileSky,            kTileGroundTopL,      kTileBrickTl,       kTileQuestionTl,
    kTileHardTl,         kTilePipeLipL,        kTilePipeLipR,      kTilePipeBodyL,
    kTilePipeBodyR,      kTileHardTl,          kTileFlagPoleL,     kTileCastleWall,
    kTileSpentTl,        kTileCoinTl,          kTileThin,          kTileLavaTop,
    kTileBridge,         kTileAxe,             kTileGroundFillTl,  kTileCastleCrenel,
    kTileCastleWindowTl, kTileCastleDoorTopTl, kTileCastleDoorTl,  kTileScenBlank,
    kTileScenBlank,      kTileCloudCapTl,      kTileCloudMidTl,    kTileCloudCapRtl,
    kTileCloudCapBl,     kTileCloudMidBl,      kTileCloudCapRbl,   kTileHillPeakTl,
    kTileHillSlopeTl,    kTileHillSlopeTr,     kTileHillFillTl,    kTileBushCapTl,
    kTileBushMidTl,      kTileBushCapRtl,
    kTilePipeSideMouth0L, kTilePipeSideMouth2L, kTilePipeSideBody0, kTilePipeSideBody2,
    kTileCastleCrenelInner,
    kTileTreeCapTl,      kTileTreeTop,         kTileTreeTop,       kTileTrunk,
    kTileFlagClothPoleT, kTileCastleBrickUpper, kTileLavaDeep,   kTileCastleWindowTr,
    kTileHillFillTl,     kTilePipeJointT0,      kTilePipeJointB0,     kTileScenBlank,
};
// clang-format on
// clang-format off
static const uint8_t kTileTrRom[kBlockKindCount] = {
    kTileSky,            kTileGroundTopR,      kTileBrickTr,       kTileQuestionTr,
    kTileHardTr,         kTilePipeLipM,        kTilePipeLipRr,     kTilePipeBodyM,
    kTilePipeBodyRr,     kTileHardTr,          kTileFlagPoleR,     kTileCastleWall,
    kTileSpentTr,        kTileCoinTr,          kTileThin,          kTileLavaTop,
    kTileBridge,         kTileAxeRight,        kTileGroundFillTr,  kTileCastleCrenelRight,
    kTileCastleWindowTr, kTileCastleDoorTopTr, kTileCastleDoorTr,  kTileScenBlank,
    kTileFlagClothT,     kTileCloudCapTr,      kTileCloudMidTr,    kTileCloudCapRtr,
    kTileCloudCapBr,     kTileCloudMidBr,      kTileCloudCapRbr,   kTileHillPeakTr,
    kTileHillSlopeTr,    kTileHillSlopeTl,     kTileHillFillTr,    kTileBushCapTr,
    kTileBushMidTr,      kTileBushCapRtr,
    kTilePipeSideMouth0R, kTilePipeSideMouth2R, kTilePipeSideBody0, kTilePipeSideBody2,
    kTileCastleCrenelInnerRight,
    kTileTreeTop,        kTileTreeTop,         kTileTreeCapTl,     kTileTrunk,
    kTileFlagPoleR,      kTileCastleBrickUpper, kTileLavaDeep,   kTileCastleWindowTl,
    kTileHillFillTl,     kTilePipeJointBody,    kTilePipeJointBody,   kTileChainTr,
};
// clang-format on
// clang-format off
static const uint8_t kTileBlRom[kBlockKindCount] = {
    kTileSky,            kTileGroundFillBl,    kTileBrickBl,       kTileQuestionBl,
    kTileHardBl,         kTilePipeLipLb,       kTilePipeLipRb,     kTilePipeBodyL,
    kTilePipeBodyR,      kTileHardBl,          kTileFlagPoleL,     kTileCastleWall,
    kTileSpentBl,        kTileCoinBl,          kTileThinUnder,     kTileLavaFill,
    kTileBridgeLower,    kTileAxeBl,           kTileGroundFillBl,  kTileCastleWall,
    kTileCastleWindowBl, kTileCastleDoorTopBl, kTileCastleDoorBl,  kTileFlagBallL,
    kTileScenBlank,      kTileCloudCapMl,      kTileCloudMidMl,    kTileCloudCapRml,
    kTileCloudCapFl,     kTileCloudMidFl,      kTileCloudCapRfl,   kTileHillPeakBl,
    kTileHillSlopeBl,    kTileHillSlopeBr,     kTileHillFillBl,    kTileBushCapBl,
    kTileBushMidBl,      kTileBushCapRbl,
    kTilePipeSideMouth1L, kTilePipeSideMouth3L, kTilePipeSideBody1, kTilePipeSideBody3,
    kTileCastleWall,
    kTileTreeCapBl,      kTileTreeBotM,        kTileTreeBot,       kTileTrunk,
    kTileFlagClothPoleB, kTileCastleBrickLower, kTileLavaDeep,   kTileCastleWindowBr,
    kTileHillFillTl,     kTilePipeJointT1,      kTilePipeJointB1,     kTileChainBl,
};
// clang-format on
// clang-format off
static const uint8_t kTileBrRom[kBlockKindCount] = {
    kTileSky,            kTileGroundFillBr,    kTileBrickBr,       kTileQuestionBr,
    kTileHardBr,         kTilePipeLipMb,       kTilePipeLipRbr,    kTilePipeBodyM,
    kTilePipeBodyRr,     kTileHardBr,          kTileFlagPoleR,     kTileCastleWall,
    kTileSpentBr,        kTileCoinBr,          kTileThinUnder,     kTileLavaFill,
    kTileBridgeLower,    kTileAxeBr,           kTileGroundFillBr,  kTileCastleWall,
    kTileCastleWindowBr, kTileCastleDoorTopBr, kTileCastleDoorBr,  kTileFlagBallR,
    kTileFlagClothB,     kTileCloudCapMr,      kTileCloudMidMr,    kTileCloudCapRmr,
    kTileCloudCapFr,     kTileCloudMidFr,      kTileCloudCapRfr,   kTileHillPeakBr,
    kTileHillSlopeBr,    kTileHillSlopeBl,     kTileHillFillBr,    kTileBushCapBr,
    kTileBushMidBr,      kTileBushCapRbr,
    kTilePipeSideMouth1R, kTilePipeSideMouth3R, kTilePipeSideBody1, kTilePipeSideBody3,
    kTileCastleWall,
    kTileTreeBot,        kTileTreeBotM,        kTileTreeCapBl,     kTileTrunk,
    kTileFlagPoleR,      kTileCastleBrickLower, kTileLavaDeep,   kTileCastleWindowBl,
    kTileHillFillTl,     kTilePipeJointBody,    kTilePipeJointBody,   kTileScenBlank,
};
// clang-format on
// sky, the flag's four cells, a world coin, the axe, lava and every scenery kind are all
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
    0, kFloorSolid, 0, 0,
    0, kFloorSolid, kFloorSolid, 0,
};
// clang-format on
// lava borrows the coin slot, which no castle grid ever paints a world coin with; the bridge takes
// the neutral one (whose unused color 3 the castle set turns into its chain's red), the axe the
// pipe slot (nothing else on that slot stands in a castle, and the castle set turns its greens into
// the blades' two oranges), the chain the masonry's ground slot, and the thin platform the neutral
// one too. the hills, the bushes and the flag
// share the pipe's greens, the castle shares the brick's browns, and the clouds and the pennant
// share the sky's whites - and the sideways pipe is the vertical one rotated, so it shares those
// greens too, and so does 1-3's tree canopy, whose four rip colors are exactly the pipe slot's
// (sky, bright green, dark green, black); its trunk takes the brick's browns - which is how eight
// cgb slots still cover all forty-eight kinds.
//
// kBlockFlagPoleCloth is the one kind whose entry only covers half its cell: its left tile is the
// pennant's white and its right tile the shaft's greens, and put_face gives the right tile column
// kBlockFlagPole's slot instead (terrain.c). the shaft's left outline is color 3 in both slots, so
// the plain pole's own left tile reads the same under either
//
// the kScen* entries add kCamAttrVram1: every kind whose art assets_load_scenery_tiles put in vram
// bank 1 has to say so here, because the attribute byte is what picks the bank a cell reads from
#define kScenSky (kCamPalSky | kCamAttrVram1)
#define kScenPipe (kCamPalPipe | kCamAttrVram1)
#define kScenBrick (kCamPalBrick | kCamAttrVram1)
// m20's castle terrain: the masonry course, the bridge's two halves and the axe's two blades all
// live in vram bank 1 too, because bank 0's bg map is out of ids. the brick takes the ground slot,
// whose colors a castle load turns into the course's own four greys; the axe takes the question
// block's gold rather than the bridge's grey, which is the orange the rip paints its blades
#define kScenGround (kCamPalGround | kCamAttrVram1)
#define kScenNeutral (kCamPalNeutral | kCamAttrVram1)
#define kScenQuestion (kCamPalQuestion | kCamAttrVram1)
// clang-format off
static const uint8_t kPaletteRom[kBlockKindCount] = {
    kCamPalSky,     kCamPalGround,  kCamPalBrick,   kCamPalQuestion,
    kCamPalBrick,   kCamPalPipe,    kCamPalPipe,    kCamPalPipe,
    kCamPalPipe,    kCamPalBrick,   kScenPipe,      kScenBrick,
    kCamPalSpent,   kCamPalCoin,    kCamPalNeutral, kCamPalCoin | kCamAttrVram1,
    kScenNeutral,   kScenPipe,      kCamPalGround,  kScenBrick,
    kScenBrick,     kScenBrick,     kScenBrick,     kScenPipe,
    kScenSky,       kScenSky,       kScenSky,       kScenSky,
    kScenSky,       kScenSky,       kScenSky,
    kScenPipe,      kScenPipe,      kScenPipe | kCamAttrXFlip,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenPipe,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenPipe,
    kScenBrick,
    kScenPipe,      kScenPipe,      kScenPipe | kCamAttrXFlip, kScenBrick,
    kScenSky,       kScenGround,   kCamPalCoin | kCamAttrVram1, kScenBrick,
    kScenPipe,      kScenPipe,      kScenPipe,      kScenGround,
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
