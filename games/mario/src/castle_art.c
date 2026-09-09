// the castle's art and the one call that loads it, in bank 6 beside 1-4's own grid: bank 4 holds
// every other tile array and was 130 bytes from full when m27 cut these six families off the smbd
// 1-4 capture, so they ride here and a BANKED call reaches them
#pragma bank 6

#include "castle_art.h"

#include "gen/axe.h"
#include "gen/bridge.h"
#include "gen/castle_brick.h"
#include "gen/castle_hard.h"
#include "gen/chain.h"
#include "gen/lava.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>

void castle_art_load(void) BANKED {
    // the masonry over the ground family's six ids: the upper course in the surface block's and
    // the fill block's top pairs, the lower course in the shared bottom pair, so a ground cell tiles
    // into the same running bond a wall of masonry does
    set_bkg_data(kTileGroundTopL, 1, kCastleBrickTiles);
    set_bkg_data(kTileGroundTopR, 1, kCastleBrickTiles);
    set_bkg_data(kTileGroundFillTl, 1, kCastleBrickTiles);
    set_bkg_data(kTileGroundFillTr, 1, kCastleBrickTiles);
    set_bkg_data(kTileGroundFillBl, 1, &kCastleBrickTiles[16U]);
    set_bkg_data(kTileGroundFillBr, 1, &kCastleBrickTiles[16U]);
    // and the castle's own solid block over the hard block's four ids: the capture draws a brown
    // face in a grey border with a dot in each corner, not the overworld's bevelled block
    set_bkg_data(kTileHardTl, kCastleHardTileCount, kCastleHardTiles);
    // the rest is bank 1, like the scenery: vbk goes back before anything touches the map
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileCastleBrickLower, 1, &kCastleBrickTiles[16U]);
    set_bkg_data(kTileCastleBrickUpper, 1, kCastleBrickTiles);
    // the lava's wave and its flat red, and the flat red again under every pit's surface cell
    set_bkg_data(kTileLavaTop, kLavaTileCount, kLavaTiles);
    set_bkg_data(kTileLavaDeep, 1, &kLavaTiles[16U]);
    // the axe's blades, then its haft in the two ids past the lava
    set_bkg_data(kTileAxe, 2, kAxeTiles);
    set_bkg_data(kTileAxeBl, 2, &kAxeTiles[32U]);
    set_bkg_data(kTileBridge, kBridgeTileCount, kBridgeTiles);
    set_bkg_data(kTileChainTr, kChainTileCount, kChainTiles);
    VBK_REG = VBK_BANK_0;
}
