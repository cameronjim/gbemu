// bank 0 was already full before m7, and super mario alone is 512 bytes of art. every tile and
// palette the game owns now rides with the enemy bank; only the lcd-off loaders here reach them,
// and the one table the streamer reads per column (kBlockTile*) stays behind in assets.c
#pragma bank 4

#include "assets.h"
#include "gen/bowser.h"
#include "gen/brick.h"
#include "gen/brick_underground.h"
#include "gen/bush.h"
#include "gen/bush_right.h"
#include "gen/castle.h"
#include "gen/castle_crenel_inner.h"
#include "gen/castle_crenel_right.h"
#include "gen/cloud.h"
#include "gen/cloud_right.h"
#include "gen/coin.h"
#include "gen/fireball.h"
#include "gen/flag_ball.h"
#include "gen/flag_head.h"
#include "gen/flag_pole.h"
#include "gen/flower.h"
#include "gen/goomba.h"
#include "gen/goomba_squash.h"
#include "gen/ground.h"
#include "gen/ground_lower.h"
#include "gen/hard.h"
#include "gen/hill.h"
#include "gen/items.h"
#include "gen/koopa_green.h"
#include "gen/mario_big.h"
#include "gen/mario_small.h"
#include "gen/mario_small_climb.h"
#include "gen/paratroopa_red.h"
#include "gen/pipe.h"
#include "gen/pipe_side.h"
#include "gen/pipe_joint.h"
#include "gen/piranha.h"
#include "gen/question.h"
#include "gen/scen_tail.h"
#include "gen/shell_green.h"
#include "gen/shell_red.h"
#include "gen/spent.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>
// 1-3's tree, transcribed pixel for pixel off the smb1 map rip (mariouniverse's 1-3.png, the tree
// at column 18). the canopy uses exactly four colors there - sky, a bright green body, a dark
// green accent under its scalloped bottom edge, and a black outline - which is kCamPalPipe's
// overworld set in that order, so the greens the hills and bushes already share color the tree too.
// the trunk is the brick browns: color 2 fills it and color 3 draws the dark stripes, whose 8px
// period in both axes is why the whole column is one tile
// clang-format off
static const uint8_t kTreeTiles[128] = {
    // kTileTreeCapTl 0x0a - the left cap, rounded into the sky
    0x3F, 0x3F, // ..######
    0x7F, 0x60, // .##-----
    0x7F, 0x40, // .#------
    0xFF, 0xC0, // ##------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    // kTileTreeTop 0x0b - the plain top row, also both of the middle and the right cap
    0xFF, 0xFF, // ########
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // kTileTreeCapBl 0x0c - the left cap again, rounded the other way
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x81, // #------#
    0x7E, 0x42, // .#----#.
    0x3C, 0x3C, // ..####..
    // kTileTreeBot 0x0d - a plain scalloped bottom, also the right cap left half
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x01, // -------#
    0xFE, 0x82, // #-----#.
    0x7C, 0x7C, // .#####..
    // kTileTreeBotM 0x0e - the middle's bottom, the only one with dark green in it
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x01, // -------#
    0xFE, 0x83, // #-----#+
    0x7C, 0xFF, // +#####++
    // kTileTreeCapTr 0x0f - the right cap top
    0xF8, 0xF8, // #####...
    0xFC, 0x04, // -----#..
    0xFE, 0x02, // ------#.
    0xFE, 0x02, // ------#.
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    // kTileTreeCapBr 0x10 - the right cap bottom
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x81, // #------#
    0x7E, 0x42, // .#----#.
    0x3C, 0x3C, // ..####..
    // kTileTrunk 0x11 - one tile that tiles the whole column, 8px stripe period both axes
    0x00, 0xFF, // ++++++++
    0x08, 0xFF, // ++++#+++
    0x08, 0xFF, // ++++#+++
    0x08, 0xFF, // ++++#+++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// 1-3's thin platform: a four-px plank with sky under it
// clang-format off
static const uint8_t kThinTiles[32] = {
    // thin platform deck
    0xFF, 0xFF, // ########
    0xFF, 0x00, // --------
    0x00, 0xFF, // ++++++++
    0xFF, 0xFF, // ########
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // the empty cell under it
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// 1-4's lava, painted over the death plane, off the nes rip: a white wave breaking along the top
// of the pit and flat red under it, with one row of foam still coming apart below the crest. the
// crest's peak sits at column 4 and its trough spans the tile's own last two columns and the next
// tile's first two, so an 8px repeat reads as one continuous surf line and not as a stamped shape.
// color 1 is the foam and color 2 the lava (see the castle set's kCamPalCoin)
// clang-format off
static const uint8_t kLavaTiles[32] = {
    // lava crest: a white wave over the pit
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x08, 0x00, // ....-...
    0x14, 0x08, // ...-+-..
    0x24, 0x18, // ..-++-..
    0xC4, 0x38, // --+++-..
    // lava fill: foam breaking, then flat red
    0x03, 0xFC, // ++++++--
    0x40, 0xBF, // +-++++++
    0xA1, 0x5E, // -+-++++-
    0x26, 0xD9, // ++-++--+
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// under a pit's surface cell. kLavaTiles' second tile still has the crest's foam breaking across
// its top four rows, which is exactly right for the bottom half of the cell at the surface and
// wrong for every cell below it, so those get this instead: the lava's own red, flat, all 64 px
// clang-format off
static const uint8_t kLavaDeepTile[16] = {
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// the castle's masonry: a white highlight along the top and left of each brick, a light stone face,
// a shadow down its right and along its bottom, and the black mortar the rip leaves in the joint
// column and the row under every course.
//
// the rip lays it in running bond - bricks 8 px wide and 8 tall, each course stepped half a brick
// against the one over it - so a 16 px cell is two courses and the joint falls at the same column
// in both halves of either. that is two tiles: the upper course carries its joint four columns in
// and the lower one carries it in the last column, and each stamps both halves of its own row
// (kBlockCastleBrick's top pair and bottom pair, and the ground family a castle load overwrites)
// clang-format off
static const uint8_t kCastleBrickUpperTile[16] = {
    // the cell's upper course, its joint four columns in
    0xEF, 0x20, // --#.----
    0x28, 0xE7, // ++#.-+++
    0x28, 0xE7, // ++#.-+++
    0x28, 0xE7, // ++#.-+++
    0x28, 0xE7, // ++#.-+++
    0x28, 0xE7, // ++#.-+++
    0xEF, 0xEF, // ###.####
    0x00, 0x00, // ........
};
// clang-format on
// clang-format off
static const uint8_t kCastleBrickLowerTile[16] = {
    // and the course under it, stepped half a brick along
    0xFE, 0x02, // ------#.
    0x82, 0x7E, // -+++++#.
    0x82, 0x7E, // -+++++#.
    0x82, 0x7E, // -+++++#.
    0x82, 0x7E, // -+++++#.
    0x82, 0x7E, // -+++++#.
    0xFE, 0xFE, // #######.
    0x00, 0x00, // ........
};
// clang-format on

// the bridge, a full 16px of it: the rip draws a rail of white chain links over a dark band with a
// red link running down every fourth column, then the links carrying on out of the bottom. it
// repeats every 4 px across, so the top tile stamps both upper quadrants of the block and the
// lower one both of the others. color 1 is the links' white, 2 the band and 3 the red (the castle
// set's kCamPalNeutral)
// clang-format off
static const uint8_t kBridgeTiles[32] = {
    // bridge deck: the chain links and their rail
    0x77, 0x00, // .---.---
    0x77, 0x00, // .---.---
    0x77, 0x00, // .---.---
    0x77, 0x00, // .---.---
    0x00, 0x77, // .+++.+++
    0x88, 0xFF, // #+++#+++
    0x88, 0xFF, // #+++#+++
    0x88, 0xFF, // #+++#+++
    // bridge underside: the links running out
    0x88, 0xFF, // #+++#+++
    0x88, 0xFF, // #+++#+++
    0x88, 0xFF, // #+++#+++
    0x00, 0x77, // .+++.+++
    0x77, 0x77, // .###.###
    0x77, 0x77, // .###.###
    0x77, 0x77, // .###.###
    0x77, 0x77, // .###.###
};
// clang-format on

// and the axe: a symmetric double blade on a light shaft, 16 px across, which is why it needs two
// tiles rather than one stamped twice - the old single tile drew two axes side by side. color 1 is
// the blade's orange and 2 the shaft, so it wears kCamPalQuestion rather than the bridge's slot
// clang-format off
static const uint8_t kAxeTiles[32] = {
    // axe, left blade
    0x1E, 0x01, // ...----+
    0x3E, 0x01, // ..-----+
    0x7E, 0x01, // .------+
    0x7E, 0x01, // .------+
    0x7E, 0x01, // .------+
    0x3E, 0x01, // ..-----+
    0x1E, 0x01, // ...----+
    0x00, 0x01, // .......+
    // axe, right blade
    0x78, 0x80, // +----...
    0x7C, 0x80, // +-----..
    0x7E, 0x80, // +------.
    0x7E, 0x80, // +------.
    0x7E, 0x80, // +------.
    0x7C, 0x80, // +-----..
    0x78, 0x80, // +----...
    0x00, 0x80, // +.......
};
// m18's art pass gives most blocks four distinct quadrants instead of one tile stamped four times,
// which is 99 background tiles where the old placeholder art was 21. bank 0's tile space cannot
// hold that beside the sprites, so the art is split: everything a level's terrain needs stays in
// vram bank 0, at the pinned 0xa0-0xbf block and the eight ids past mario's last sprite frame, and
// the scenery - the castle, the whole flag, and 1-1's clouds, hills and bushes - goes to vram
// bank 1, which nothing else in the game has ever used for tiles. a cgb bg map attribute picks a
// tile's bank per cell (kCamAttrVram1 in mario.h), so the two sets coexist with no id conflict at
// all: the font keeps its own glyph range in bank 0 and never has to be reloaded
//
// m23 replaced every hand-transcribed array below with one generated by
// games/mario/tools/rip_tiles.py off the smbd capture; only the pieces 1-1 never shows (the thin
// platform, lava, the bridge, the axe, 1-2's sideways pipe, 1-3's tree) are still hand-drawn
void assets_load_bg_tiles(void) BANKED {
    set_bkg_data(kTileGroundTopL, 4, kGroundTiles);
    set_bkg_data(kTileGroundFillBl, 2, kGroundLowerTiles);
    set_bkg_data(kTileBrickTl, 4, kBrickTiles);
    set_bkg_data(kTileQuestionTl, 4, kQuestionTiles);
    set_bkg_data(kTileSpentTl, 4, kSpentTiles);
    set_bkg_data(kTileHardTl, 4, kHardTiles);
    set_bkg_data(kTilePipeLipL, kPipeTileCount, kPipeTiles);
    set_bkg_data(kTileThin, 2, kThinTiles);
    set_bkg_data(kTileCoinTl, 4, kCoinTiles);
}

// an underground segment is the same masonry under the underground palette everywhere except the
// brick's top row: the overworld paints a solid tan highlight across it and the bonus room leaves
// its two mortar joints showing, which is two pixels of the cell and so two tiles of the family.
// they are written over the brick's upper pair the way a castle load writes its course over the
// ground family, and assets_load_bg_tiles above puts the overworld pair back
void assets_load_bg_tiles_underground(void) BANKED {
    set_bkg_data(kTileBrickTl, kBrickUndergroundTileCount, kBrickUndergroundTiles);
}

// a castle's floors, ceilings and walls are the same grey masonry as its scenery, not the
// overworld's tan/brown ground: rather than recompile every castle grid onto a second terrain
// kind, the ground family's own tiles are overwritten with it at a castle load. the six ids are the
// whole family - the surface block's two upper quadrants, its two lower ones, and the buried fill
// block's upper pair - and the castle bg set colors kCamPalGround to match. each of them takes the
// course kBlockCastleBrick puts in the same quadrant, so a ground cell tiles into the same running
// bond a wall of masonry does. every other level type reloads the family from assets_load_bg_tiles
// above, so nothing leaks out. the measured 1-4 has no ground cell left at all, but a castle grid
// compiled off prose still can
void assets_load_bg_tiles_castle(void) BANKED {
    set_bkg_data(kTileGroundTopL, 1, kCastleBrickUpperTile);
    set_bkg_data(kTileGroundTopR, 1, kCastleBrickUpperTile);
    set_bkg_data(kTileGroundFillTl, 1, kCastleBrickUpperTile);
    set_bkg_data(kTileGroundFillTr, 1, kCastleBrickUpperTile);
    set_bkg_data(kTileGroundFillBl, 1, kCastleBrickLowerTile);
    set_bkg_data(kTileGroundFillBr, 1, kCastleBrickLowerTile);
}

// the same call writes vram bank 1 with vbk pointing there, so the scenery lands beside the font
// rather than on top of it. bcpd and vram are both mode-locked on real hardware and terrain_init
// runs with the lcd off, which is where this is called from; vbk goes back before anything else
// touches the map, because set_bkg_tiles would otherwise write tile numbers into the attribute map
void assets_load_scenery_tiles(void) BANKED {
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileLavaTop, 2, kLavaTiles);
    set_bkg_data(kTileCastleWall, kCastleTileCount, kCastleTiles);
    set_bkg_data(kTileCastleCrenelInner, kCastleCrenelInnerTileCount, kCastleCrenelInnerTiles);
    // the ball's two halves sit at the two ends of the scenery run, 0x30 and 0x5d, so its one
    // family is written into them by two calls rather than one
    set_bkg_data(kTileFlagBallL, 1, kFlagBallTiles);
    set_bkg_data(kTileFlagBallR, 1, &kFlagBallTiles[16U]);
    set_bkg_data(kTileFlagClothT, kFlagHeadTileCount, kFlagHeadTiles);
    set_bkg_data(kTileCloudCapTl, kCloudTileCount, kCloudTiles);
    set_bkg_data(kTileHillPeakTl, kHillTileCount, kHillTiles);
    set_bkg_data(kTileBushCapTl, kBushTileCount, kBushTiles);
    set_bkg_data(kTileFlagPoleL, kFlagPoleTileCount, kFlagPoleTiles);
    set_bkg_data(kTileScenBlank, kScenTailTileCount, kScenTailTiles);
    // m23: the three pieces the capture proved are not their left twin mirrored, plus the right
    // half of each castle crenel. 0xd0-0xdd, clear of the hud-font headroom and of bowser
    set_bkg_data(kTileCloudCapRtl, kCloudRightTileCount, kCloudRightTiles);
    set_bkg_data(kTileBushCapRtl, kBushRightTileCount, kBushRightTiles);
    set_bkg_data(kTileCastleCrenelRight, kCastleCrenelRightTileCount, kCastleCrenelRightTiles);
    // the sideways pipe is solid terrain, not scenery, but vram bank 0 has no tile ids left: it
    // rides here with the scenery and reads back through kCamAttrVram1 the same way
    set_bkg_data(kTilePipeSideMouth0L, kPipeSideTileCount, kPipeSideTiles);
    // and the joint where that body meets its shaft, off the 1-2 capture, at 0xe0-0xe4
    set_bkg_data(kTilePipeJointT0, kPipeJointTileCount, kPipeJointTiles);
    // and 1-3's tree, in the eight bank-1 ids under the map screen's own castle run
    set_bkg_data(kTileTreeFirst, kTileTreeCount, kTreeTiles);
    // m20's castle run right above it: the masonry's two courses, the axe's two blades and the
    // bridge's two halves. all three are terrain rather than scenery, but bank 0 is out of bg ids
    // and a kCamAttrVram1 attribute reads them back the same way
    set_bkg_data(kTileCastleBrickLower, 1, kCastleBrickLowerTile);
    set_bkg_data(kTileCastleBrickUpper, 1, kCastleBrickUpperTile);
    set_bkg_data(kTileLavaDeep, 1, kLavaDeepTile);
    set_bkg_data(kTileAxe, 2, kAxeTiles);
    set_bkg_data(kTileBridge, 2, kBridgeTiles);
    VBK_REG = VBK_BANK_0;
}

// the overworld's eight cgb bg palettes. every slot keeps the sky in color 0 because most of these
// tiles leave part of their cell empty and that empty part is the backdrop; the ground's own
// color 0 goes unused, because a ground block is opaque across its whole 16x16 cell.
//
// m23 read every colour below off the smbd capture as rgb555, so a generated tile's colour index
// and the slot it is worn in cannot drift apart (the same rule m22 applied to the sprites). the
// four the level actually paints with are the tan 0xf8c098, the brown 0x984800, the gold
// 0xf8b840 and black, plus the pipe family's two greens 0x70f830 and 0x108800, the clouds' white
// and their scallop blue 0x30a0f8, and the sky itself. the used block wears the question block's
// own gold - smbd draws both out of one block palette - so kCamPalSpent's entries are the
// question slot's, which is also what lets the used block keep a slot of its own for free
void assets_load_bg_palettes(void) BANKED {
    // color 1 of the sky slot is the clouds' and the pennant's white, and the hud row's ink
    palette_color_t sky[4] = {kSkyRgb, RGB(31, 31, 31), RGB(6, 20, 31), RGB(0, 0, 0)};
    palette_color_t ground[4] = {kSkyRgb, RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t brick[4] = {kSkyRgb, RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t question[4] = {kSkyRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {kSkyRgb, RGB(14, 31, 6), RGB(2, 17, 0), RGB(0, 0, 0)};
    palette_color_t neutral[4] = {kSkyRgb, RGB(31, 31, 31), RGB(31, 24, 19), RGB(0, 0, 0)};
    palette_color_t spent[4] = {kSkyRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t coin[4] = {kSkyRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, coin);
}

void assets_load_bg_palettes_underground(void) BANKED {
    // the same eight slots and, bar the brick's top row, the same art: only the colors say the
    // room is below ground. m23 read them off 1-1's own bonus room in the smbd capture, where the
    // backdrop is flat black, the masonry is the teal 0x008888 over a near-white 0xb8f8f0
    // highlight, and the exit pipe keeps the overworld's greens; m25 read the rest off 1-2's own
    // capture, whose underground run is the same art under the same colours.
    //
    // the brick slot carries the near-white highlight in colour 1 for the hard block's bevel: 1-2's
    // stair-step blocks light it, the same shape as the overworld's in teal and near-white. the
    // brick itself never reaches that colour down here, because the one pair of its tiles that
    // uses colour 1 - the cell's top row - is what assets_load_bg_tiles_underground swaps out.
    //
    // color 1 is the hud row's ink (see kHudBarAttr): white here as in the overworld set, which
    // costs nothing because the only tiles pinned to this slot are the clouds and the pennant and
    // neither ever stands in an underground segment
    palette_color_t sky[4] = {kUndergroundRgb, RGB(31, 31, 31), RGB(6, 20, 31), RGB(0, 0, 0)};
    palette_color_t ground[4] = {kUndergroundRgb, RGB(23, 31, 30), RGB(0, 17, 17), RGB(0, 0, 0)};
    palette_color_t brick[4] = {kUndergroundRgb, RGB(23, 31, 30), RGB(0, 17, 17), RGB(0, 0, 0)};
    // the question block's bottom and right edges and the strokes of its glyph are colour 3, which
    // 1-2's capture draws in the masonry's teal where the overworld draws them black; the used
    // block is the same block palette, so its edge follows
    palette_color_t question[4] = {kUndergroundRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 17, 17)};
    // the pipe's outline is the one colour that is NOT the overworld's down here: every pipe in the
    // capture's bonus room draws its rim and its stripe joints in the dark green 0x004800 where the
    // overworld draws them flat black. against a black backdrop a black outline would vanish, which
    // is exactly what the room's own art relies on not happening
    palette_color_t pipe[4] = {kUndergroundRgb, RGB(14, 31, 6), RGB(2, 17, 0), RGB(0, 9, 0)};
    palette_color_t neutral[4] = {kUndergroundRgb, RGB(24, 26, 31), RGB(12, 18, 31), RGB(0, 0, 0)};
    palette_color_t spent[4] = {kUndergroundRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 17, 17)};
    // the loose coin's fourth colour is the shading inside its ring, which the room draws in its
    // own teal where the overworld draws it black
    palette_color_t coin[4] = {kUndergroundRgb, RGB(31, 23, 8), RGB(19, 9, 0), RGB(0, 17, 17)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, coin);
}

void assets_load_bg_palettes_castle(void) BANKED {
    // the same eight slots again, drained to castle stone. lava takes the coin slot, which no
    // castle grid paints a world coin with, so the one warm ramp on screen is the pit
    // color 1 is the hud row's ink again, and a castle grid paints no cloud and no pennant either
    //
    // m20 matched four of these to the nes rip. the ground slot is the masonry course's own four
    // shades and its color 0 is the black mortar, not a stone - the course leaves a mortar line
    // down its last column and along its last row, which is what makes a wall read as brickwork
    // rather than as a slab. the brick slot goes warm: the rip's hard block (a firebar's pivot)
    // and its breakable brick are both the same brown-orange inside a dark grey border, and that
    // border is color 3 in kHardTiles. the lava's own foam is white, not gold - the hud coin used
    // to borrow this slot's gold and now takes the question slot's instead (kHudCoinAttr) - and
    // the neutral slot's unused color 3 becomes the bridge chain's red
    palette_color_t sky[4] = {kCastleRgb, RGB(31, 31, 31), RGB(11, 11, 13), RGB(0, 0, 0)};
    palette_color_t ground[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(23, 23, 23), RGB(14, 14, 14)};
    palette_color_t brick[4] = {kCastleRgb, RGB(28, 12, 2), RGB(25, 9, 1), RGB(14, 14, 14)};
    palette_color_t question[4] = {kCastleRgb, RGB(31, 20, 8), RGB(31, 31, 31), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {kCastleRgb, RGB(22, 22, 24), RGB(13, 13, 15), RGB(0, 0, 0)};
    palette_color_t neutral[4] = {kCastleRgb, RGB(31, 31, 31), RGB(14, 14, 15), RGB(22, 4, 0)};
    palette_color_t spent[4] = {kCastleRgb, RGB(9, 9, 10), RGB(6, 6, 7), RGB(0, 0, 0)};
    palette_color_t lava[4] = {kCastleRgb, RGB(31, 31, 31), RGB(27, 5, 0), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, lava);
}

// m22: every colour below is read straight off the smbd sprite sheets the art was ripped from, so
// a generated tile's colour index and the slot it is worn in cannot drift apart. see mario.h's
// kPal* block for which family wears which slot
void assets_load_sprite_palettes(void) BANKED {
    // #ffb210 skin, #de0000 the cap/shirt/overall red, #736900 the dark his hair, shoes, straps
    // and outline accents are drawn in. color 0 is the sprite's transparency
    palette_color_t mario[4] = {RGB(0, 0, 0), RGB(31, 22, 2), RGB(27, 0, 0), RGB(14, 13, 0)};
    // fire mario is the same tiles under a shifted ramp, exactly as the sheet's own fire block is:
    // the red becomes #ffe3b5 cream and the dark becomes the red
    palette_color_t fire[4] = {RGB(0, 0, 0), RGB(31, 22, 2), RGB(31, 28, 22), RGB(27, 0, 0)};
    set_sprite_palette(kPalMario, 1, mario);
    set_sprite_palette(kPalFire, 1, fire);
}

void assets_load_item_palettes(void) BANKED {
    // the power-up sheet's own three: white, #f5c144 yellow, #e23122 red. the STAR's tiles are
    // drawn in this index order too, so it wears this slot rather than kPalStar (see blocks_draw)
    palette_color_t mushroom[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(30, 24, 8), RGB(28, 6, 4)};
    // white, #f8b010 koopa orange, #d80000 shell red: the red paratroopa, the red shell, the
    // fireball and its puff, and the flash a star's invincibility strobes mario with
    palette_color_t star[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(31, 22, 2), RGB(27, 0, 0)};
    // the same white and yellow over #3e8b29 green: the 1-up mushroom and the fire flower
    palette_color_t oneup[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(30, 24, 8), RGB(7, 17, 5)};
    palette_color_t coin[4] = {RGB(0, 0, 0), RGB(31, 26, 7), RGB(31, 17, 3), RGB(0, 0, 0)};
    set_sprite_palette(kPalMushroom, 1, mushroom);
    set_sprite_palette(kPalStar, 1, star);
    set_sprite_palette(kPalOneup, 1, oneup);
    set_sprite_palette(kPalCoin, 1, coin);
}

void assets_load_enemy_palettes(void) BANKED {
    // the enemy sheet's overworld row: #f8c098 the goomba's tan face and feet, #984800 its body,
    // black the outline and eyes. the koopa is #008010 green, #f8b010 orange skin and white - the
    // three colours bowser is drawn in as well, which is why the castle re-tint below is a no-op
    palette_color_t goomba[4] = {RGB(0, 0, 0), RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t koopa[4] = {RGB(0, 0, 0), RGB(0, 16, 2), RGB(31, 22, 2), RGB(31, 31, 31)};
    set_sprite_palette(kPalGoomba, 1, goomba);
    set_sprite_palette(kPalKoopa, 1, koopa);
}

// the coin a struck block pays out, the one item still drawn from hand art: one 8x16 pair, the
// oval standing in both halves. the mushroom, the star and the 1-up beside it in the 0xd0
// family are the smbd rip's own now (games/mario/src/gen/items.c) and so is the fireball's
// spin frame (gen/fireball.c). colors: 1 the shine, 2 the body, 3 the outline
// clang-format off
static const uint8_t kCoinPopTiles[32] = {
    // coin pop top
    0x3C, 0x3C, // ..####..
    0x62, 0x5E, // .#-+++#.
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    // coin pop bottom
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0x62, 0x5E, // .#-+++#.
    0x3C, 0x3C, // ..####..
};
// clang-format on

// the fire flower: white petals, a yellow heart and a dark stem, which is what the star's palette
// paints once it is borrowed for the item

// both mario sets stay resident, which is what lets the grow animation alternate them without a
// single vram write. small mario's seven poses are one generated run: the six the 0xe0 family has
// always pinned, then the death pose, which goes to the four ids super mario's old bank-0 block
// gave back. big mario's eight 16x32 poses are 1 KB and vram bank 0 could never have held them, so
// the whole set - not just a jump slab - lives in bank 1 at 0x00, and so does small mario's climb
void assets_load_sprite_tiles(void) BANKED {
    set_sprite_data(kTileMarioFirst, kMarioTileCount, kMarioSmallTiles);
    set_sprite_data(kTileMarioDeath, kMarioTilesPerFrame, &kMarioSmallTiles[kMarioTileCount * 16U]);
    // a sprite picks its tile bank from S_BANK in its own prop (see player_draw)
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileSuperFirst, kSuperTileCount, kMarioBigTiles);
    set_sprite_data(kTileClimbSmall, kMarioSmallClimbTileCount, kMarioSmallClimbTiles);
    VBK_REG = VBK_BANK_0;
}

// m19's throwaway animations, three 8x16 pairs whose lower halves are blank the way the fireball's
// own pair is. the fragment is one 7x7 chip of the brick wall - black mortar edge, a tan highlight
// course and a brown body, which is exactly what the goomba's palette paints (see debris.c) - and
// the puff is a tight burst widening into a bigger one, drawn from the star's white and yellow
// clang-format off
static const uint8_t kDebrisTiles[96] = {
    // brick fragment
    0xFC, 0xFC, // ######..
    0xFC, 0x84, // #----#..
    0xC4, 0xBC, // #-+++#..
    0xC4, 0xBC, // #-+++#..
    0xC4, 0xBC, // #-+++#..
    0x84, 0xFC, // #++++#..
    0xFC, 0xFC, // ######..
    0x00, 0x00, // ........
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // fragment lower half, blank
    // puff frame a - the tight burst
    0x00, 0x00, // ........
    0x18, 0x18, // ...##...
    0x24, 0x3C, // ..#++#..
    0x5A, 0x66, // .#+--+#.
    0x5A, 0x66, // .#+--+#.
    0x24, 0x3C, // ..#++#..
    0x18, 0x18, // ...##...
    0x00, 0x00, // ........
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // puff a lower half, blank
    // puff frame b - the same burst thrown out to the full 8px
    0x18, 0x18, // ...##...
    0x24, 0x3C, // ..#++#..
    0x5A, 0x66, // .#+--+#.
    0xDB, 0xE7, // ##+--+##
    0xDB, 0xE7, // ##+--+##
    0x5A, 0x66, // .#+--+#.
    0x24, 0x3C, // ..#++#..
    0x18, 0x18, // ...##...
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // puff b lower half, blank
};
// clang-format on

// the 0xd0 family: the three 16x16 items out of the rip, then the hand-drawn coin pop, then the
// fireball's first spin frame - the generated run stores frame A's pair first and frame B's after
// it, and B goes to the same id in vram bank 1 the way it always has, so powerup_draw still spins
// the ball by toggling S_BANK
void assets_load_item_tiles(void) BANKED {
    set_sprite_data(kTileItemFirst, kItemsTileCount, kItemsTiles);
    set_sprite_data(kTileCoinPop, 2, kCoinPopTiles);
    set_sprite_data(kTileFireball, 2, kFireballTiles);
    set_sprite_data(kTileFlowerFirst, kFlowerTileCount, kFlowerTiles);
    set_sprite_data(kTileDebris, kDebrisTileCount, kDebrisTiles);
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileFireball, 2, &kFireballTiles[2 * 16U]);
    VBK_REG = VBK_BANK_0;
}

// the two hazards still drawn from hand art, one 8x16 pair each whose lower tile is blank: a
// firebar flame in the top half of its pair and a lift plank in the top half of its own. the
// piranha that used to head this run is the smbd rip's now - a 16x24 plant bottom-aligned in a
// 16x32 box, still left-right symmetric and so still only its left column stored, in
// games/mario/src/gen/piranha.c
// clang-format off
static const uint8_t kHazardTiles[64] = {
    // a firebar segment, redrawn off the rip: a round fireball with a dark red rim, an orange body
    // and a white-hot core off centre toward the top. it wears the castle set's re-tinted
    // kPalStar (white, orange, dark red), which is also what bowser's breath burns in
    0x3C, 0x3C, // ..####..
    0x66, 0x7A, // .##++-#.
    0xCD, 0xF3, // ##++--+#
    0x99, 0xE7, // #++--++#
    0x81, 0xFF, // #++++++#
    0x42, 0x7E, // .#++++#.
    0x3C, 0x3C, // ..####..
    0x00, 0x00, // ........
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // flame lower half, blank
    0xFF, 0xFF, 0xFF, 0x00, 0xDB, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // lift deck, upper half of the pair
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // lift lower half, blank
};
// clang-format on

// m20's fake bowser, redrawn pixel for pixel off the deluxe sprite (mariowiki
// SMBDX_Bowser_sprite.png) - the same 32x32 art smb1 walks on the nes, which is where the second
// frame comes from: the nes walk gif's frame 0 is byte for byte the deluxe sprite, so its frame 1
// is deluxe's other frame.
//
// roster.json gives him 4x4 tiles, so a 32x32 body in two frames of sixteen tiles. the layout is
// the eight 8x16 sprites hazards.c draws him out of, in the order it walks them - upper row left to
// right, then the lower row - and each sprite's own pair is its upper 8x8 followed by its lower one.
// he faces left, which is the way he faces on the bridge; walking right draws the same tiles with
// S_FLIPX and the column order reversed.
//
// the sprite uses exactly three colors and so does the castle re-tint of kPalKoopa: color 1 is the
// green of his shell and hide, 2 the orange of his brow, mouth, belly plates and claws, and 3 the
// white of his horns, teeth, eyes and shell spikes. color 0 is a sprite's transparent index.
//
// the bytes themselves now come out of games/mario/art/bowser.png through png2tiles.py; edit the
// png, not gen/bowser.c

// and his fire breath, a 24x8 dart in three 8x16 pairs whose lower halves are blank. it flies
// left, so the white-hot core leads and the dark red tail frays out behind it; the palette is the
// firebar flame's, which is the same fire and never on screen at the same time
// clang-format off
static const uint8_t kBowserFireTiles[96] = {
    // breath third 0
    0x06, 0x06, // .....##.
    0x39, 0x3F, // ..###++#
    0x60, 0x5F, // .#-+++++
    0xE0, 0x9F, // #--+++++
    0xE0, 0x9F, // #--+++++
    0x60, 0x5F, // .#-+++++
    0x39, 0x3F, // ..###++#
    0x06, 0x06, // .....##.
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // breath third 1
    0x06, 0x06, // .....##.
    0xF9, 0xFF, // #####++#
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0xF9, 0xFF, // #####++#
    0x06, 0x06, // .....##.
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // breath third 2
    0x00, 0x00, // ........
    0xC0, 0xC0, // ##......
    0x70, 0xF0, // +###....
    0x1C, 0xFC, // +++###..
    0x1C, 0xFC, // +++###..
    0x70, 0xF0, // +###....
    0xC0, 0xC0, // ##......
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// the red paratroopa's two frames: the koopa's own walk0 pair with a wing drawn into the top left
// of its shell half, where the walk frame leaves the cell empty anyway. kPalKoopa paints it - color
// 1 is the koopa's cream, which is the white smb gives the wing, color 2 the green shell and 3 the
// outline - so the flyer costs no sprite palette either. red and green koopas have always
// shared this one palette in this game, and the paratroopa shares it too.
//
// the wing beats by dropping a row rather than changing shape, which is one legible flap and keeps
// the two frames' body bytes identical. vram bank 0's 0xc0 family is exactly full, so these ride at
// the same ids in bank 1 and enemies_draw sets S_BANK on the flyer's prop
// clang-format off
static const uint8_t kParaTiles[128] = {
    // paratroopa frame0 l top - the koopa shell with its wing raised
    0xC0, 0xC0, // ##......
    0xE0, 0xA0, // #-#.....
    0xF1, 0x91, // #--#...#
    0xF9, 0x89, // #---#..#
    0x79, 0x49, // .#--#..#
    0x39, 0x39, // ..###..#
    0x62, 0x7E, // .##+++#.
    0x41, 0x7F, // .#+++++#
    // paratroopa frame0 l bot
    0x80, 0xFF, // #+++++++
    0x98, 0xFF, // #++##+++
    0xA4, 0xFF, // #+#++#++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x7F, 0x7F, // .#######
    0x38, 0x00, // ..---...
    0x78, 0x00, // .----...
    // paratroopa frame0 r top
    0x78, 0x78, // .####...
    0xFC, 0x84, // #----#..
    0xFE, 0x32, // --##--#.
    0xFF, 0x31, // --##---#
    0xFF, 0x01, // -------#
    0xFE, 0x02, // ------#.
    0xFE, 0x82, // #-----#.
    0xFC, 0x84, // #----#..
    // paratroopa frame0 r bot
    0x7C, 0xC4, // +#---#..
    0x3C, 0xE4, // ++#--#..
    0x18, 0xF8, // +++##...
    0x10, 0xF0, // +++#....
    0x10, 0xF0, // +++#....
    0xE0, 0xE0, // ###.....
    0xE0, 0x00, // ---.....
    0xF0, 0x00, // ----....
    // paratroopa frame1 l top - the same wing a row lower, on the downbeat
    0x00, 0x00, // ........
    0x80, 0x80, // #.......
    0xC1, 0xC1, // ##.....#
    0xE9, 0xA9, // #-##...#
    0xF9, 0x89, // #---#..#
    0x79, 0x79, // .####..#
    0x62, 0x7E, // .##+++#.
    0x41, 0x7F, // .#+++++#
    // paratroopa frame1 l bot
    0x80, 0xFF, // #+++++++
    0x98, 0xFF, // #++##+++
    0xA4, 0xFF, // #+#++#++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x7F, 0x7F, // .#######
    0x38, 0x00, // ..---...
    0x78, 0x00, // .----...
    // paratroopa frame1 r top
    0x78, 0x78, // .####...
    0xFC, 0x84, // #----#..
    0xFE, 0x32, // --##--#.
    0xFF, 0x31, // --##---#
    0xFF, 0x01, // -------#
    0xFE, 0x02, // ------#.
    0xFE, 0x82, // #-----#.
    0xFC, 0x84, // #----#..
    // paratroopa frame1 r bot
    0x7C, 0xC4, // +#---#..
    0x3C, 0xE4, // ++#--#..
    0x18, 0xF8, // +++##...
    0x10, 0xF0, // +++#....
    0x10, 0xF0, // +++#....
    0xE0, 0xE0, // ###.....
    0xE0, 0x00, // ---.....
    0xF0, 0x00, // ----....
};
// clang-format on

// the 0xc0 family holds what is still 16x16 - the goomba's one walk frame, its pancake and the
// green shell - and the koopa's two 16x32 walk frames take the 0x60 run super mario left. the red
// paratroopa and the red shell a stomped one leaves ride in vram bank 1
void assets_load_enemy_tiles(void) BANKED {
    set_sprite_data(kTileGoombaWalk0, kGoombaTileCount, kGoombaTiles);
    set_sprite_data(kTileGoombaSquash, kGoombaSquashTileCount, kGoombaSquashTiles);
    set_sprite_data(kTileShell, kShellGreenTileCount, kShellGreenTiles);
    set_sprite_data(kTileKoopaWalk0, kKoopaGreenTileCount, kKoopaGreenTiles);
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileParaFly0, kParatroopaRedTileCount, kParatroopaRedTiles);
    set_sprite_data(kTileShellRed, kShellRedTileCount, kShellRedTiles);
    VBK_REG = VBK_BANK_0;
}

// bowser's open jaw: his head's left half (sprite 0 of the eight hazards.c draws him out of) with
// the lower jaw pulled two px down and the mouth behind it opened to the backdrop. every other
// tile of him stays the walk frame's own, so the whole tell is these two - see kTileBowserJaw
// clang-format off
static const uint8_t kBowserJawTiles[32] = {
    // his open jaw, upper 8x8
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x01, 0x00, //        1
    0x07, 0x06, //      331
    0x0F, 0x46, //  2  1331
    0x1F, 0xA6, // 2 211331
    0x1F, 0xFC, // 22233311
    0x0F, 0xF8, // 22223111
    // and its lower one: the jaw dropped two px, the mouth open behind it
    0x8C, 0x83, // 3   1122
    0x01, 0x0E, //     2221
    0x01, 0x1E, //    22221
    0x5F, 0x5A, //  3 33131
    0x38, 0x38, //   333   
    0x00, 0x00, //         
    0x02, 0x03, //       32
    0x00, 0x01, //        2
};
// clang-format on

// the mushroom retainer, transcribed cell for cell off the nes 1-4 rip's own 16x24 sprite - the
// figure standing beside mario in the room past the axe, at its columns 153 rows 11-12. two
// columns of two 8x16 sprites, so eight tiles, of which the second tile of each lower sprite is
// the eight transparent rows under his feet: an 8x16 sprite reads its id and the id after it
// whether or not the second one has anything in it.
//
// he wears the castle's kPalStar - the one sprite slot in a castle with a white in it that nothing
// else is using once the flames are gone: color 1 is his cap and trousers, 2 his face, 3 the cap's
// spots and his waistcoat, and color 0 is the transparent the rip outlines him in black with
// clang-format off
static const uint8_t kToadTiles[128] = {
    // toad, left column, sprite 0 upper
    0x03, 0x00, //       11
    0x0F, 0x00, //     1111
    0x3F, 0x1C, //   133311
    0x7F, 0x1D, //  1133313
    0x7F, 0x1B, //  1133133
    0xFF, 0xC3, // 33111133
    0xFF, 0xE3, // 33311133
    0xFF, 0xE1, // 33311113
    // toad, left column, sprite 0 lower
    0xFF, 0xE0, // 33311111
    0xF0, 0xCD, // 331122 2
    0x70, 0x1D, //  11322 2
    0x00, 0x4F, //  2  2222
    0x00, 0xEE, // 222 222 
    0x0C, 0xFF, // 22223322
    0x1F, 0x3F, //   233333
    0x3C, 0x3F, //   333322
    // toad, left column, sprite 1 upper
    0x38, 0x3F, //   333222
    0x38, 0x3F, //   333222
    0x1F, 0x00, //    11111
    0x3F, 0x00, //   111111
    0x7F, 0x70, //  3331111
    0xFF, 0xB8, // 31333111
    0xFF, 0xFC, // 33333311
    0xFF, 0xFC, // 33333311
    // toad, left column, sprite 1 lower
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    // toad, right column, sprite 0 upper
    0xC0, 0x00, // 11      
    0xF0, 0x00, // 1111    
    0xFC, 0x38, // 113331  
    0xFE, 0xB8, // 3133311 
    0xFE, 0xD8, // 3313311 
    0xFF, 0xC3, // 33111133
    0xFF, 0xC7, // 33111333
    0xFF, 0x87, // 31111333
    // toad, right column, sprite 0 lower
    0xFF, 0x07, // 11111333
    0x0F, 0xB3, // 2 221133
    0x0E, 0xB8, // 2 22311 
    0x00, 0xF2, // 2222  2 
    0x00, 0x77, //  222 222
    0x30, 0xFF, // 22332222
    0xF8, 0xFC, // 333332  
    0x3C, 0xFC, // 223333  
    // toad, right column, sprite 1 upper
    0x1C, 0xFC, // 222333  
    0x1C, 0xFC, // 222333  
    0xF8, 0x00, // 11111   
    0xFC, 0x00, // 111111  
    0xFE, 0x0E, // 1111333 
    0xFF, 0x1D, // 11133313
    0xFF, 0x3F, // 11333333
    0xFF, 0x3F, // 11333333
    // toad, right column, sprite 1 lower
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
    0x00, 0x00, //         
};
// clang-format on

void assets_load_hazard_tiles(void) BANKED {
    set_sprite_data(kTilePiranha, kPiranhaTileCount, kPiranhaTiles);
    set_sprite_data(kTileFlame, kHazardTileCount, kHazardTiles);
    // bowser's own 512 bytes go to vram BANK 1: bank 0's sprite map has eight ids left in it and
    // he wants thirty-two. hazards_draw sets S_BANK on every sprite that reads them back
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileBowserFirst, (uint8_t)(kBowserTilesPerFrame * kBowserArtFrames), kBowserTiles);
    set_sprite_data(kTileBowserFire, kBowserFireTileCount, kBowserFireTiles);
    set_sprite_data(kTileBowserJaw, 2, kBowserJawTiles);
    VBK_REG = VBK_BANK_0;
}

// the hud row's coin: a gold (color 1) oval with a darker slot (color 2) down the middle, on a
// color 0 cell, which under kHudCoinAttr is the level's own sky. gold is the low plane alone and
// the slot the high plane alone, so a row reads lo = the body, hi = the slot
// clang-format off
static const uint8_t kHudCoinTile[16] = {
    0x00, 0x00, // ........
    0x3C, 0x00, // ..oooo..
    0x66, 0x18, // .oo##oo.
    0x6E, 0x10, // .oo#ooo.
    0x6E, 0x10, // .oo#ooo.
    0x66, 0x18, // .oo##oo.
    0x3C, 0x00, // ..oooo..
    0x00, 0x00, // ........
};
// clang-format on

// the hud row's own copy of the ibm font, in vram bank 1. the resident glyphs at 0x00-0x5f draw
// their ink on color 3, which is the art's black outline in all eight level slots. so each glyph
// is read back out of bank 0 and re-encoded: ink on color 1, cell left on color 0, which under
// kHudBarAttr is white text standing on the level's own sky in every set. a 2bpp row is (lo, hi),
// so an ink pixel is the lo plane only and a cell pixel is neither plane - and taking the ink mask
// as lo|hi keeps this right whatever shade the font itself was expanded with. the space glyph
// carries no ink at all, which is what leaves a blank cell as pure backdrop
static void hud_glyph(uint8_t id, char c) {
    // gbdk leaves lcdc bit 4 clear, so a bg tile id is signed indexing from 0x9000 (pan docs,
    // "bg and window tile data"): the font's own 0x00-0x5f sit at 0x9000 up, which is the half of
    // vram the sprites at 0x8000-0x8fff cannot reach and the reason both fit at all
    const uint8_t* src = (const uint8_t*)(0x9000U + ((uint16_t)((uint8_t)c - kFontFirstChar) << 4));
    uint8_t glyph[16];
    uint8_t r;

    VBK_REG = VBK_BANK_0;
    for (r = 0; r < 16U; r = (uint8_t)(r + 2U)) {
        const uint8_t ink = (uint8_t)(src[r] | src[r + 1U]);

        glyph[r] = ink;
        glyph[r + 1U] = 0;
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(id, 1, glyph);
}

void assets_load_hud_font(void) BANKED {
    // the same list hud.c maps a character to an id with, expanded here so the copy order and the
    // ids cannot drift apart. it is one character long now: the row prints digits and an x
    static const char kLetters[] = kHudGlyphChars;
    uint8_t i;

    for (i = 0; i < 10U; ++i) {
        hud_glyph((uint8_t)(kTileHudDigitFirst + i), (char)('0' + i));
    }
    hud_glyph(kTileHudBlank, ' ');
    for (i = 0; kLetters[i] != '\0'; ++i) {
        hud_glyph((uint8_t)(kTileHudLetterFirst + i), kLetters[i]);
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileHudCoin, 1, kHudCoinTile);
    VBK_REG = VBK_BANK_0;
}

void assets_load_toad_tiles(void) BANKED {
    // the same list toad.c maps a character to an id with (kSignGlyphChars), re-encoded out of the
    // resident font by the hud row's own routine: ink on color 1, cell on color 0. a space is not
    // in the list - kTileHudBlank is already exactly that cell and the sign borrows it
    static const char kSignChars[] = kSignGlyphChars;
    uint8_t i;

    for (i = 0; kSignChars[i] != 0; ++i) {
        hud_glyph((uint8_t)(kTileSignFirst + i), kSignChars[i]);
    }
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileToadFirst, kToadTileCount, kToadTiles);
    VBK_REG = VBK_BANK_0;
}

void assets_load_enemy_palettes_castle(void) BANKED {
    // no koopa and no piranha stands in a castle, so the koopa slot is re-tinted for the fake
    // bowser. since m22 read the koopa's own three colours off the enemy sheet that re-tint writes
    // exactly what assets_load_enemy_palettes already wrote, and it is kept only so the contract
    // survives a future repaint of either. the deluxe sprite's palette chunk:
    // 0x008010 green for the shell and hide, 0xf8b010 orange for the brow, mouth, belly and claws,
    // and white for the horns, teeth, eyes and spikes. that is every color the sprite has - color 0
    // is a sprite's transparent index, so his outline is whatever is behind him, which in a castle
    // is the black deluxe outlines him in
    palette_color_t goomba[4] = {RGB(0, 0, 0), RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t bowser[4] = {RGB(0, 0, 0), RGB(0, 16, 2), RGB(31, 22, 2), RGB(31, 31, 31)};
    // and the star slot becomes the castle's one fire ramp, which the firebar flames and bowser's
    // breath both burn in: white at the core, orange through the body, dark red at the rim. no
    // castle grid holds a star or a flower - the two items that borrow this slot - and the only
    // other things wearing it are a star's palette flash and the fireball's puff, both of which
    // read as fire in this ramp too
    palette_color_t fire[4] = {RGB(0, 0, 0), RGB(31, 31, 31), RGB(31, 19, 7), RGB(27, 5, 0)};
    set_sprite_palette(kPalGoomba, 1, goomba);
    set_sprite_palette(kPalKoopa, 1, bowser);
    set_sprite_palette(kPalStar, 1, fire);
}
