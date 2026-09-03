#ifndef MARIO_H
#define MARIO_H

// tuning constants live here so logic files never carry magic numbers

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleRow 6U
#define kPromptRow 11U
// every card banner is a padding row, the text row, then another padding row, so the tinted band
// reads as a band rather than exactly the glyph height. kPromptRow sits 5 rows below kTitleRow so
// the two banners (each reaching one row above/below their text) still leave a 2-row sky gap
#define kBannerRows 3U
// the pause card's own heading row, 2 rows above kTitleRow: it carries more lines (world, score,
// lives, footer) than the title/game-over/clear cards do, so it sits higher to leave the footer
// some breathing room above the bottom of the 18-row screen
#define kPauseRow 4U
// its RESUME/QUIT menu, on the same two-row pitch the readout above it uses, and the footer hint
// on the last row of the screen. the widest entry plus the cursor column is 7 glyphs, and both
// lines are padded to it so the cursor does not shift the text when it moves
#define kPauseMenuRow 13U
#define kPauseItemStep 2U
#define kPauseItemCount 2U
#define kPauseItemWidth 7U
#define kPauseHintRow 17U

// the SELECT FILE card (games/mario/src/mapscreen.c). its heading sits high because the three
// slots each take two rows - the slot line and its score - and three hint lines close the screen
#define kFileHeadRow 2U
#define kFileFirstRow 5U
#define kFileRowStep 3U
#define kFileHintRow 14U
// every slot line is padded to this many glyphs so the cursor column does not shift between a
// "NEW" slot and a "WORLD 1-2" one
#define kFileLineWidth 12U
// the erase confirm is its own card over the same machinery
#define kEraseRow 5U

// the front end lockout (games/mario/src/mapscreen.c). a fresh screen ignores start/a/b for this
// many frames, and a button already held when it opens stays ignored until it is released - without
// it every screen confirms on its own first live frame, so three quick taps of the space bar (title,
// then file select, then the map) drop the player straight into a level before the map is even seen
#define kFrontLockFrames 20U

// the world one map (games/mario/src/mapscreen.c), a single static 20x18-cell screen, three bands
// stacked top to bottom: a solid-black WORLD/1-N header, a 10-block-wide map strip, then a solid-
// black lives/CLEAR LIST footer. nothing scrolls, so every position is either a tile row (the bands)
// or a block cell (the map strip)
#define kMapBlockCols 10U
// the header is rows 0-3; the map strip starts at tile row 4, i.e. block row 2 (put_block's `by`
// is a tile-row-pair index, same units the level's own put_block uses)
#define kMapBandFirstRow 2U
#define kMapBandBlockRows 4U
// the footer starts right after the strip: (2 + 4) * 2 = tile row 12, leaving rows 12-17 for it
#define kMapFooterFirstTileRow ((kMapBandFirstRow + kMapBandBlockRows) * kTilesPerBlock)
// the four level nodes: columns 1, 3, 5 and 7, with the castle filling 8-9
#define kMapNodeFirstCol 1U
#define kMapNodeStepCol 2U
// markers and mario share the strip's third block row (tile rows 8-9): the path runs under both,
// the way the reference's round stops sit right on the road rather than floating above it
#define kMapMarkerRow (kMapBandFirstRow + 2U)
#define kMapWalkRow kMapMarkerRow
// a node step is kMapNodeStepCol blocks = 32 px, walked a pixel a frame: 32 frames, about half a
// second, which is a walk rather than a teleport
#define kMapWalkPx 1
#define kMapWalkAnimFrames 8U
// the header text: "WORLD" then "1-N", one padding row above and below across the 4-row band
#define kMapWorldRow 1U
#define kMapLevelRow 2U

// the footer, rows 12-17: row 12 is padding, 13-16 hold the lives readout and the bordered CLEAR
// LIST panel side by side (cols 0-6 and 7-19), 17 carries the button hint. the lives readout is
// plain text, not a mario-shaped icon: a host probe finds mario anywhere on screen by his sprite tile family
// alone, so a second mario-family sprite here would feed into every test that reads his position off the map
// (see map_draw_lives in mapscreen.c)
#define kMapLivesTextRow 14U
#define kMapLivesTextCol 1U
// the footer's button hint, on the last row of the black band under the CLEAR LIST panel
#define kMapHintRow 17U
#define kMapHintCol 1U
#define kMapListLeftCol 7U
#define kMapListWidth 13U
#define kMapListTopRow 13U
#define kMapListHeadRow 14U
#define kMapListCellsRow 15U
#define kMapListBottomRow 16U

// color 0 of the map's own "black band" slot (kCamPalSky, repurposed - see
// assets_load_map_bg_palettes). near-black rather than literal (0,0,0) only so a host probe can
// tell "the map is up, showing its band" apart from "the lcd is off" without relying on an exact
// zero; the eye cannot tell the difference from true black
#define kMapSkyRgb RGB(1, 1, 1)

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U
#define kFontLastTile 0x5FU
// the font foreground/background shade indices passed to font_color
#define kFontFore 3U
#define kFontBack 0U

// cgb bg palette slots: sky backdrop, warm wordmark, green accent
#define kPalSky 0U
#define kPalWordmark 1U
#define kPalAccent 2U

// debug camera step, applied to scx/scy per frame while a d-pad direction is held
#define kCamStepPx 2U
// screen geometry in px; the level lives entirely in vram (30 tile rows = 240 px), so only scy pans it
#define kScreenWidthPx 160U
#define kScreenHeightPx 144U
#define kLevelHeightPx 240U
#define kScyMax (kLevelHeightPx - kScreenHeightPx) // 96

// a block is 16px = 2x2 tiles; the level's 15 block rows fill 30 of the ring's 32 tile rows
#define kBlockPx 16U
#define kTilesPerBlock 2U
#define kLevelBlockRows 15U
#define kBgRows (kLevelBlockRows * kTilesPerBlock) // 30
// the bg map ring is the hardware's full 32 tile columns; 16 block columns fit in it at once
#define kRingTileCols 32U
// the hardware bg map is square, so a full wipe walks this many rows of kRingTileCols cells
#define kBgMapRows 32U
#define kRingBlocks (kRingTileCols / kTilesPerBlock) // 16
// 10 block columns fill the 160px screen; the rest of the ring buffers lookahead/lookbehind
#define kVisibleBlocks (kScreenWidthPx / kBlockPx) // 10
#define kWindowLeftMargin 3U

// block kind enum: the contract shared with games/mario/tools/compile_level.py's BLOCK_* constants
// and the generated level data's byte values. keep both sides in sync by hand.
#define kBlockEmpty 0U
#define kBlockGround 1U
#define kBlockBrick 2U
#define kBlockQuestion 3U
#define kBlockHard 4U
#define kBlockPipeTl 5U
#define kBlockPipeTr 6U
#define kBlockPipeBodyL 7U
#define kBlockPipeBodyR 8U
#define kBlockStair 9U
#define kBlockFlagPole 10U
#define kBlockCastle 11U
#define kBlockSpent 12U
#define kBlockCoin 13U
// m8a's four. a thin platform is solid only to feet coming down onto it, lava is scenery painted
// over the death plane, and the bridge and axe are what a castle ends with
#define kBlockThin 14U
#define kBlockLava 15U
#define kBlockBridge 16U
#define kBlockAxe 17U
// m18's background pass. a surface ground block wears two rows of grass, so the rows under it are
// their own kind; the castle stopped being one uniform slab and became five; the flag grew a ball
// and a pennant; and the last twelve are pure scenery, painted only where the bible left sky
#define kBlockGroundFill 18U
#define kBlockCastleCrenel 19U
#define kBlockCastleWindow 20U
#define kBlockCastleDoorTop 21U
#define kBlockCastleDoor 22U
#define kBlockFlagBall 23U
#define kBlockFlagCloth 24U
// a cloud is two block rows: a rounded cap, a repeatable middle, and the cap again mirrored
#define kBlockCloudTl 25U
#define kBlockCloudT 26U
#define kBlockCloudTr 27U
#define kBlockCloudBl 28U
#define kBlockCloudB 29U
#define kBlockCloudBr 30U
#define kBlockHillPeak 31U
#define kBlockHillSlopeL 32U
#define kBlockHillSlopeR 33U
#define kBlockHillFill 34U
#define kBlockBushL 35U
#define kBlockBushM 36U
#define kBlockBushR 37U
// 1-2's sideways pipe mouth, the classic L: a 2-row-tall mouth facing left, then a column of
// horizontal body, then an ordinary vertical shaft. all four are solid, and their art is the
// vertical pipe's own tiles turned on their side (see kPipeSideTiles in assets_data.c)
#define kBlockPipeSideTl 38U
#define kBlockPipeSideBl 39U
#define kBlockPipeSideBodyT 40U
#define kBlockPipeSideBodyB 41U
// the keep's own battlement, the five-wide row the tower stands on. the outer crenel leaves the
// notches between its merlons transparent, which let sky through directly under the three-wide
// tower and made the tower read as floating; this kind fills those notches with the castle's own
// black instead, so the tower stands on masonry. scenery, like every other castle kind
#define kBlockCastleCrenelInner 42U
// 1-3's tree tops. smb1 draws a tree as a one-block-tall canopy - a rounded left cap, a repeatable
// middle and a rounded right cap - standing on a column of trunk, and the canopy is an ordinary
// solid block on all four sides while the trunk is pure scenery you walk straight through. the
// trunk is stamped by the terrain pass rather than the decor one, so it stays outside the
// [kBlockFirstDecor, kBlockLastDecor] range even though nothing about it is solid
#define kBlockTreeTopL 43U
#define kBlockTreeTopM 44U
#define kBlockTreeTopR 45U
#define kBlockTrunk 46U
// the one pole cell the pennant is hanging at. the shaft is centred in its block now, so the
// pennant's 16 px body reaches 8 px into the pole's own cell to touch it - and that half of the
// cell is the flag's white while the other half is the shaft's greens, which is one palette more
// than a cell gets. so this kind says "pole with the cloth's near half", and put_face gives its
// right tile column the plain pole's palette (terrain.c). the descent swaps it down the shaft cell
// by cell against kBlockFlagPole, the way the cloth column swaps against sky
#define kBlockFlagPoleCloth 47U
// 1-4's masonry: the cut stone the castle's roof, walls and floors are built out of. both 1-4 rips
// draw it as two big blocks across a cell where 1-2's cave brick is running bond, so it cannot
// borrow kBlockBrick's art - and it must not borrow its behaviour either, because blocks_head_bump
// answers a grid brick straight off the grid and a grown mario breaks one, which in a castle would
// mean punching a hole through the wall into the lava. solid, unbreakable, never in a block list
#define kBlockCastleBrick 48U
#define kBlockKindCount 49U
// the decorative kinds - non-solid, and only ever stamped into a cell the compiled level left
// empty - are the closed range [kBlockFirstDecor, kBlockLastDecor]. they were the tail of the
// enum until the side pipe was appended past them, so anything testing for decor has to take the
// range rather than "everything from here up"
#define kBlockFirstDecor kBlockCloudTl
#define kBlockLastDecor kBlockBushR
// what blocks_kind_override returns when the compiled grid still stands unaltered
#define kBlockNoOverride 0xFFU

// the head-bump reaction list's own kinds, the contract with compile_level.py's LIST_* constants.
// separate from the render kinds because a hidden block renders as sky until a bump reveals it
#define kBlockListQuestion 0U
#define kBlockListBrick 1U
#define kBlockListHidden 2U

// block contents, the contract with compile_level.py's CONTENT_MAP
#define kContentNothing 0U
#define kContentCoin 1U
#define kContentMushroom 2U
#define kContentStar 3U
#define kContentOneup 4U
#define kContentMulticoin 5U
#define kContentVine 6U

// bg tile ids for the level. m18's art pass gives most blocks four distinct quadrants instead of
// one tile stamped four times, which is 99 tiles rather than 21, so the background now lives in
// three runs: the pinned 0xa0-0xbf block and the eight ids past mario's last sprite frame, both in
// vram bank 0, and a scenery run in vram bank 1 (see assets_load_scenery_tiles).
#define kTileSky kFontFirstTile // 0x00: the font's blank space glyph, same trick as flappy/crossy

// --- the pinned terrain block, 0xa0-0xbf, exactly full -----------------------------------------
// ground family 0xa0..0xa3: a surface block's grass-capped upper half, then the rubble's upper half
#define kTileGroundTopL 0xA0U
#define kTileGroundTopR 0xA1U
#define kTileGroundFillTl 0xA2U
#define kTileGroundFillTr 0xA3U
// brick family 0xa4..0xa7: two courses in running bond
#define kTileBrickTl 0xA4U
#define kTileBrickTr 0xA5U
#define kTileBrickBl 0xA6U
#define kTileBrickBr 0xA7U
// question family 0xa8..0xaf: the lit block's 2x2 face, then the spent block's
#define kTileQuestionTl 0xA8U
#define kTileQuestionTr 0xA9U
#define kTileQuestionBl 0xAAU
#define kTileQuestionBr 0xABU
#define kTileSpentTl 0xACU
#define kTileSpentTr 0xADU
#define kTileSpentBl 0xAEU
#define kTileSpentBr 0xAFU
// pipe family 0xb0..0xb8. a pipe is 32px wide and its shading runs in columns across the whole of
// it, so the lip is a left edge, a middle repeated twice and a right edge, top half then bottom
#define kTilePipeLipL 0xB0U
#define kTilePipeLipM 0xB1U
#define kTilePipeLipR 0xB2U
#define kTilePipeLipLb 0xB3U
#define kTilePipeLipMb 0xB4U
#define kTilePipeLipRb 0xB5U
#define kTilePipeBodyL 0xB6U
#define kTilePipeBodyM 0xB7U
#define kTilePipeBodyR 0xB8U
// the bridge deck, the axe, and the world coin's four quadrants. 0xb9 held the pole shaft until the
// shaft was centred in its cell: a centred shaft straddles the two tiles of its block and bank 0
// had no second id free, so the whole flag went to bank 1 and 0xb9 is the one spare id here
#define kTileBridge 0xBAU
#define kTileAxe 0xBBU
#define kTileCoinTl 0xBCU
#define kTileCoinTr 0xBDU
#define kTileCoinBl 0xBEU
#define kTileCoinBr 0xBFU

// --- the eight ids past mario's frames, 0xf8-0xff, exactly full --------------------------------
#define kTileGroundFillBl 0xF8U
#define kTileGroundFillBr 0xF9U
#define kTileHardTl 0xFAU
#define kTileHardTr 0xFBU
#define kTileHardBl 0xFCU
#define kTileHardBr 0xFDU
#define kTileThin 0xFEU
#define kTileThinUnder 0xFFU

// --- the scenery run, 0x20-0x5d, in VRAM BANK 1 -------------------------------------------------
// these ids overlap the font's glyphs and half the sprite families, and collide with none of them:
// a cgb bg map attribute picks a tile's vram bank per cell, so a block tagged kCamAttrVram1 reads
// its four tiles out of bank 1, which nothing else in this game has ever stored a tile in. bank 0
// keeps the font and the sprites exactly as they were
#define kTileSceneryFirst 0x20U
#define kTileLavaTop 0x20U
#define kTileLavaFill 0x21U
#define kTileCastleWall 0x22U
#define kTileCastleCrenel 0x23U
#define kTileCastleWindowTl 0x24U
#define kTileCastleWindowTr 0x25U
#define kTileCastleWindowBl 0x26U
#define kTileCastleWindowBr 0x27U
#define kTileCastleDoorTopTl 0x28U
#define kTileCastleDoorTopTr 0x29U
#define kTileCastleDoorTopBl 0x2AU
#define kTileCastleDoorTopBr 0x2BU
#define kTileCastleDoorTl 0x2CU
#define kTileCastleDoorTr 0x2DU
#define kTileCastleDoorBl 0x2EU
#define kTileCastleDoorBr 0x2FU
// the flag, all of it in bank 1 since the shaft was centred. the pennant is 16 px wide and its
// right edge is the shaft's own left outline, so it straddles two cells: its left half is the cloth
// cell's right tile column and its right half is the pole cell's left one
#define kTileFlagBallL 0x30U
#define kTileFlagClothT 0x31U
#define kTileFlagClothB 0x32U
#define kTileFlagClothPoleT 0x33U
#define kTileFlagClothPoleB 0x34U
// a cloud is one 16x32 mass split into two block rows: the cap's, then the middle's
#define kTileCloudCapTl 0x35U
#define kTileCloudCapTr 0x36U
#define kTileCloudCapMl 0x37U
#define kTileCloudCapMr 0x38U
#define kTileCloudMidTl 0x39U
#define kTileCloudMidTr 0x3AU
#define kTileCloudMidMl 0x3BU
#define kTileCloudMidMr 0x3CU
#define kTileCloudCapBl 0x3DU
#define kTileCloudCapBr 0x3EU
#define kTileCloudCapFl 0x3FU
#define kTileCloudCapFr 0x40U
#define kTileCloudMidBl 0x41U
#define kTileCloudMidBr 0x42U
#define kTileCloudMidFl 0x43U
#define kTileCloudMidFr 0x44U
#define kTileHillPeakTl 0x45U
#define kTileHillPeakTr 0x46U
#define kTileHillPeakBl 0x47U
#define kTileHillPeakBr 0x48U
#define kTileHillSlopeTl 0x49U
#define kTileHillSlopeTr 0x4AU
#define kTileHillSlopeBl 0x4BU
#define kTileHillSlopeBr 0x4CU
#define kTileHillFillTl 0x4DU
#define kTileHillFillTr 0x4EU
#define kTileHillFillBl 0x4FU
#define kTileHillFillBr 0x50U
#define kTileBushCapTl 0x51U
#define kTileBushCapTr 0x52U
#define kTileBushCapBl 0x53U
#define kTileBushCapBr 0x54U
#define kTileBushMidTl 0x55U
#define kTileBushMidTr 0x56U
#define kTileBushMidBl 0x57U
#define kTileBushMidBr 0x58U
// the shaft, split across the two tiles of its block so it can stand in the middle of it: the left
// tile carries its black left outline at px 7 and the right tile the two lit columns at px 8-9. the
// pair is contiguous because the tests read the lit column off the right tile (pole_face)
#define kTileFlagPoleL 0x59U
#define kTileFlagPoleR 0x5AU
// the inner crenel's one tile, the outer merlon redrawn with its notch and its cap filled in
#define kTileCastleCrenelInner 0x5BU
// the empty half of the cloth cell, and the ball's right half. a bank-1 cell cannot borrow bank 0's
// sky tile for its blank quadrants, so it gets one of its own
#define kTileScenBlank 0x5CU
#define kTileFlagBallR 0x5DU
#define kTileSceneryLast 0x5DU

// --- 1-3's tree run, 0x0a-0x11, also in VRAM BANK 1 ---------------------------------------------
// the scenery run above is contiguous and exactly full: 0x5e-0x5f is only two ids and 0x60-0x71
// is the map screen's own bank-1 art. so the tree takes the eight free ids under the map screen's
// castle instead (bank-1 bg 0x00-0x09, see assets.h), which nothing else in either screen touches.
//
// eight is all the canopy costs because the shape repeats: a plain top row serves the left cap's
// right quadrant, both of the middle's and the right cap's left one, and one plain scalloped
// bottom serves the left cap's inner quadrant and the right cap's. the rip's dark-green notch
// accents only appear under the middle, so its bottom is its own tile. the trunk's stripes have an
// 8px period in both axes, so one tile stamped four times is the whole column
#define kTileTreeFirst 0x0AU
#define kTileTreeCapTl 0x0AU
#define kTileTreeTop 0x0BU
#define kTileTreeCapBl 0x0CU
#define kTileTreeBot 0x0DU
#define kTileTreeBotM 0x0EU
#define kTileTreeCapTr 0x0FU
#define kTileTreeCapBr 0x10U
#define kTileTrunk 0x11U
#define kTileTreeCount 8U

// the sideways pipe's nine tiles, the vertical pipe's own lip and body turned on their side - a
// transpose, not a rotation, so smb's light stays at the top left: the cap's top outline becomes
// the rim line down the mouth's left edge and the highlight runs along the pipe's top for its
// whole horizontal length (see kPipeSideTiles in assets_data.c). vram bank 0 has no tile ids left
// at all, so they ride in bank 1 past the map screen's own run (0x60-0x71, declared in assets.h),
// and their kBlockPalette entries carry kCamAttrVram1 the way every scenery kind's does.
// the mouth is one block column of two block rows - top quadrants then bottom - and the body the
// same shape again, so nine tiles cover all four kinds
#define kTilePipeSideTl 0x72U
#define kTilePipeSideTr 0x73U
#define kTilePipeSideMl 0x74U
#define kTilePipeSideMr 0x75U
#define kTilePipeSideBl 0x76U
#define kTilePipeSideBr 0x77U
#define kTilePipeSideBodyT 0x78U
#define kTilePipeSideBodyM 0x79U
#define kTilePipeSideBodyB 0x7AU

// cgb bg palette slots for the terrain: one per pinned tile family, plus a neutral one for
// bridge/axe/platform. all eight cgb bg palettes are spoken for
#define kCamPalSky 0U
#define kCamPalGround 1U
#define kCamPalBrick 2U
#define kCamPalQuestion 3U
#define kCamPalPipe 4U
#define kCamPalNeutral 5U
#define kCamPalSpent 6U
#define kCamPalCoin 7U
// a cgb bg map attribute's flip bits, or'd into a kBlockPalette entry. put_face tags all four of a
// block's tiles with the one byte, so a mirrored block kind is the same four tiles with its left
// and right columns swapped and this bit set - which is what lets one cloud cap, one hill slope
// and one bush cap each serve both ends of the shape they cap
#define kCamAttrXFlip 0x20U
// the vertical twin of kCamAttrXFlip, bit 6 of a cgb bg map attribute byte: the map screen's
// round node marker is one quadrant tile stamped four times, x- and y-flipped for the other three
#define kCamAttrYFlip 0x40U
// and the bank bit, which is what puts a scenery block's tiles in vram bank 1
#define kCamAttrVram1 0x08U

// color 0 of each palette set's sky slot. it is the one color on screen in every level and in no
// two of them the same, so it is also what the host tests read to name the palette set that is up.
// only assets_data.c expands these; they live here so the three sets cannot drift apart
#define kSkyRgb RGB(13, 17, 31)
#define kUndergroundRgb RGB(1, 1, 6)
#define kCastleRgb RGB(1, 1, 3)

// sprite family 0xe0.. per the milestone's tile-id contract. small mario is 16x16 = two 8x16
// sprites, so one animation frame costs four 8x8 tiles: left top/bottom then right top/bottom.
#define kTileMarioFirst 0xE0U
#define kMarioTilesPerFrame 4U
#define kMarioFrameCount 6U
#define kMarioTileCount (kMarioFrameCount * kMarioTilesPerFrame) // 24, ids 0xe0-0xf7
// frame order inside the table; the three walk frames are consecutive so the cycle is one add
#define kFrameIdle 0U
#define kFrameWalk0 1U
#define kFrameWalk1 2U
#define kFrameWalk2 3U
#define kFrameSkid 4U
#define kFrameJump 5U
#define kWalkFrameCount 3U

// super/fire mario, 16x32 = four 8x16 sprites. the pinned 0xe0-0xf7 family holds 24 tiles and one
// 16x32 pose costs eight, so the set cannot share it: m7 takes the block between the font's last
// glyph (0x5f) and the terrain families (0xa0..), which no other family has ever used. both sets
// stay resident, which is what lets the grow animation alternate them without a single vram write
#define kTileSuperFirst 0x60U
#define kSuperTilesPerFrame 4U
// every pose reuses one upper 16x16 slab, so only the legs cost tiles: 1 + 7 poses = 32 tiles
#define kTileSuperUpper kTileSuperFirst
#define kTileSuperLowerFirst (kTileSuperFirst + kSuperTilesPerFrame) // 0x64
#define kSuperFrameCount 7U
#define kSuperTileCount ((kSuperFrameCount + 1U) * kSuperTilesPerFrame) // 32, ids 0x60-0x7f
// the crouch pose sits past the six small-mario poses share their order with
#define kFrameCrouch 6U

// the three poses vram bank 0 has no ids left for. they live in CGB VRAM BANK 1 and are drawn with
// S_BANK set in the sprite's own prop, the way the fireball's second spin frame already is; each
// keeps an id inside the family it belongs to so a host test reading tile numbers still names the
// sprite. kTileSuperJumpUpper is the one upper slab super mario's shared one cannot be (an arm goes
// up in it); the two climb poses are the flagpole grip, one per body size
#define kTileSuperJumpUpper 0x7CU
#define kTileClimbSmall 0xE0U
#define kTileClimbBigUpper 0xE4U
#define kTileClimbBigLower 0xE8U

// the fire flower, in the same m7 block right after super mario
#define kTileFlowerFirst 0x80U
#define kFlowerTileCount 4U

// m8a's four new actors. the pinned 0xc0 enemy family is exactly full and the 0xd0 item family is
// too, but the run between the flower and the terrain families still had 28 tiles nothing claimed.
// every one of these is left-right symmetric, so each costs a single 8x16 pair and its right half
// is the same tile drawn flipped
#define kTileHazardFirst 0x84U
#define kTilePiranha 0x84U
#define kTileFlame 0x86U
#define kTileLiftDeck 0x88U
#define kTileBowser 0x8AU
#define kHazardTileCount 8U // 0x84-0x8b

// m19's throwaway animations take six of the twenty ids m8b's hud digit sprites left free at 0x8c
// (the bar draws its own digits out of the bg font now, see kTileHudDigitFirst). each is an 8x16
// pair whose lower half is blank, the way the fireball's own pair already is: one quarter-brick
// fragment, drawn spinning by cycling its flip bits, and the fireball's two-frame puff.
// 0x92-0x9f, fourteen ids, is still unclaimed
#define kTileDebris 0x8CU // and 0x8d, its blank lower half
#define kTilePuffA 0x8EU  // 0x8f blank
#define kTilePuffB 0x90U  // 0x91 blank
#define kDebrisTileCount 6U // 0x8c-0x91

// the fragments a broken brick throws out (games/mario/src/debris.c). smb throws four quarter
// bricks: one to each side of the upper half fast and high, one to each side of the lower half
// slower, all under gravity and all spinning. one break is animated at a time - a second replaces
// the first - so the whole effect is four oam slots and no per-slot bookkeeping.
//
// the counts are ours: the bible times neither the throw nor its arc. they are picked so the set
// clears a mid-screen brick's view well inside the timer rather than winking out mid-air
#define kDebrisCount 4U
#define kDebrisFrames 60U
#define kDebrisGravitySubpx 128U
#define kDebrisGainCap 12
// how many frames each of the four spin orientations holds for
#define kDebrisSpinFrames 4U
// and the fireball's puff, two frames of a widening burst
#define kPuffFrames 8U

// item sprite family 0xd0.. per the milestone's tile-id contract: three 16x16 items stored the same
// way mario's frames are (left top/bottom then right top/bottom), then the 8x16 coin pop, then the
// fireball as one 8x16 pair whose top tile is empty - the family is exactly full at 0xd0-0xdf
#define kTileItemFirst 0xD0U
#define kItemTilesPerKind 4U
#define kTileCoinPop 0xDCU
#define kTileFireball 0xDEU
#define kItemTileCount 16U // 0xd0-0xdf

// the emerging item's kind, which also indexes its art and palette
#define kItemNone 0U
#define kItemMushroom 1U
#define kItemStar 2U
#define kItemOneup 3U
#define kItemFlower 4U
#define kItemKindCount 5U

// oam slots and the cgb sprite palettes. mario 4 (super's 16x32 is two rows of two 8x16 sprites;
// small parks the lower row) + one item 2 + one coin pop 1 + two fireballs + five enemies x 2 is
// 19 of the 40 slots; the per-scanline math is the enemy pool's problem, see kEnemyRowCap below.
// slots 4-8 are m19's: m8b's five hud digit sprites held them until the bar moved to the window
// layer, and the throwaway animations took them over - the four brick fragments and the fireball's
// puff, in that order (see debris.c). hud_enter_level still parks all five at a level load, which
// is what clears whatever the last life left in them
#define kSpriteMarioL 0U
#define kSpriteMarioR 1U
#define kSpriteMarioLowL 2U
#define kSpriteMarioLowR 3U
#define kSpriteFreeFirst 4U
#define kSpriteFreeCount 5U
#define kSpriteItemL 9U
#define kSpriteItemR 10U
#define kSpriteCoin 11U
#define kSpriteFireFirst 12U
#define kSpriteEnemyFirst 14U
// m8a's three: a firebar's flames, the fake bowser, and up to two lift decks four sprites wide.
// 24 + 6 + 2 + 8 = 40 of the 40 slots, exactly full. the per-scanline worst case is documented at
// kFirebarSlots
#define kSpriteFlameFirst 24U
#define kSpriteBowser 30U
#define kSpriteLiftFirst 32U
#define kSpriteLiftCount (kLiftSlotsShared * 4U) // 8
// 1-3 draws a third deck, and oam has nothing left. the flame and bowser slots are the only ones
// idle on a level with no firebar and no bowser, so a third deck borrows them: hazards.c refuses
// the third lift when either is loaded, and compile_level.py refuses to build such a level at all
#define kSpriteLiftOverflowFirst kSpriteFlameFirst
// the hardware's whole oam, which the map screen parks every slot of before drawing its two
#define kOamSlots 40U
#define kPalMario 0U
#define kPalMushroom 1U
#define kPalStar 2U
#define kPalOneup 3U
#define kPalCoin 4U
#define kPalGoomba 5U
#define kPalKoopa 6U
// the last free slot, spent on fire mario's white-and-red outfit. the flower item has none left, so
// it borrows the star's white/yellow set; the fireball projectile borrows kPalCoin's gold/orange
// instead, which is what it wants and the star's near-white set is not (see powerup_draw)
#define kPalFire 7U

// gbdk's move_sprite takes oam coordinates; the visible screen starts at (8, 16)
#define kOamXOffset 8U
#define kOamYOffset 16U

// the player's sprite box; the art keeps one transparent column inside each vertical edge
#define kPlayerWidthPx 16
#define kPlayerHeightPx 16
// super/fire mario is twice as tall, and crouching folds the body down to a small mario's box by
// dropping its top edge a whole cell while the feet stay put - which is what lets a duck-slide
// carry him through a one-block gap the way smb's does
#define kPlayerBigHeightPx 32
#define kCrouchInsetPx 16
#define kPlayerCrouchHeightPx (kPlayerBigHeightPx - kCrouchInsetPx) // 16
// ours, not the bible's: smb gives a ducking mario no walk at all, which leaves a big mario who
// came to a stop flush against 1-2's brick pillar at 78/79 no way into the one block of crawl
// space under it - the slide needs momentum he no longer has, and standing back up only puts him
// against the same brick. half the walk cap: a crawl, and slow enough that the duck-slide out of a
// run is still all momentum for as long as it lasts
#define kMarioCrouchWalkSubpx 12
// the horizontal hitbox is 12 px centered in the 16 px sprite, so a 2 px shoulder overhangs each
// wall before it bites; feet/head spans stay the full sprite height. must-measure: the inset is
// tuned to make 1-1's pit lips and pipe faces feel right, not read off the bible
#define kPlayerHitInsetPx 2
#define kPlayerHitWidthPx (kPlayerWidthPx - 2 * kPlayerHitInsetPx) // 12
// our own cadence, not the bible's: the walk cycle advances once this many subpixels have passed
#define kWalkAnimStepSubpx 48U

// the play camera (games/mario/src/camera.c). horizontal: mario is held at kCamFollowX and holding
// select slides that anchor toward kCamLookAheadX to show more of what is ahead; both directions
// scroll, so walking left really does scroll back (smbd allows what the nes locked out).
#define kCamFollowX 64
#define kCamLookAheadX 24
#define kCamAnchorStepPx 2
// vertical: the view is a deadzone window in SCREEN space, not a band pinned to mario. his feet are
// left wherever they are as long as they sit between kCamWindowTopPx and kCamWindowBottomPx down the
// screen, so hopping onto a pipe or a two-block ledge does not move the picture and the ground he
// came from stays in it. Only a climb of four blocks or more pushes his feet past the top edge and
// asks the view to rise. kCamGroundOffsetPx is what is left of the old band: camera_init still snaps
// onto mario with his feet that far above the bottom, and it is the level's opening scy.
#define kCamGroundOffsetPx 32
#define kCamWindowTopPx 64
#define kCamWindowBottomPx (kScreenHeightPx - kCamGroundOffsetPx) // 112
// airborne the window is wider at the top so a whole jump arc moves nothing at all; the bottom edge
// is shared with the grounded one, so landing back where he took off is already inside both.
#define kCamSafeTopPx 32
#define kCamSafeBottomPx (kScreenHeightPx - kCamGroundOffsetPx)
// the view never snaps: each frame it closes distance>>shift of the gap, capped at kCamEaseMaxPx.
// distance-proportional means a long correction decelerates as it arrives instead of stopping dead.
// the cap is mario's own max fall speed, so a fall can draw level with the camera but never outrun
// it; the airborne shift is the faster of the two so a long drop settles his feet well inside the
// picture rather than riding the bottom edge.
#define kCamEaseMaxPx 4
#define kCamEaseShift 3
#define kCamAirEaseShift 2
// up/down are bounded peeks (smb deluxe's own control), offsets from where the view already sits.
// they only engage after the key has been held this long with mario standing perfectly still, so a
// tap on the way into a jump cannot yank the picture.
#define kCamLookUpPx 32
#define kCamLookDownPx 24
#define kCamLookDelayFrames 24U
// scy on the level's flat opening ground, which is where the default band lands there
#define kPlayScy kScyMax

// the level-clear sequence, all our own cadence (smbd's exact frame counts are unsourced). smb's
// beat: grab the pole, slide down it with the flag coming down alongside, flip to the pole's far
// side and wait there while the flag finishes, hop off, walk to the castle and step into the door
#define kClearSlidePx 2
// the shaft's lit column is px 8 of its block - the middle of it (kFlagPoleTiles in assets_data.c)
// - so his box sits this far left of that block while he climbs: it lands the last lit column of
// the climb pose on the shaft with his body beside the pole rather than straddling it
#define kFlagShaftPx 8
#define kClearPoleOffsetPx 6
// and the step across to the pole's far side, which is where smb's mario finishes the slide
#define kClearFlipPx 16
// the pause on that side. in smb it lasts as long as the flag needs, so this is a floor and the
// flag's own descent can hold the phase open past it
#define kClearFlipFrames 48U
// the pennant comes down one 16 px cell per this many frames. cell-granular because oam is full
// (40/40) and the flag has to be repainted bg cells rather than a sprite
#define kClearFlagStepFrames 6U
#define kClearHopFrames 12
#define kClearHopPx 2
#define kClearWalkPx 1
#define kClearWalkAnimFrames 8U
// the walk ends at the castle's door column when the level has a castle. a level whose compiler
// placed none falls back to this many blocks along the closing ground
#define kClearWalkBlocks 5
// he is inside the doorway, drawn nowhere, for this long before the card takes over
#define kClearDoorFrames 24U
// the door's own column inside the keep, the contract with compile_level.py's CASTLE_DOOR_OFFSET
// and with the door rows of both castle templates: it lands on the small keep's one arch and on
// the leftmost of the big keep's three
#define kCastleDoorOffset 2U
// m18 stood the pole on smb's hard block, which ends the slide a row higher and took the whole
// beat just under the four seconds it is meant to run for; the empty-castle hold makes it back
#define kClearHoldFrames 72U

// block reactions (games/mario/src/blocks.c). the bounce is a bg rewrite, not a sprite: the struck
// cell's 2x2 face is redrawn one tile row higher for kBumpFrames and then put back, which costs two
// 2x3 vram writes on the bump frame and two on the restore frame - a fraction of one streamed column
#define kBumpRisePx 8U
#define kBumpFrames 8U
// level-1-1.json: the ten-coin brick. the bible gives no per-hit timeout, only the total
#define kMulticoinBudget 10U

// physics.json timers.powerup_emergence (smbdis.asm:7181-7197 GrowThePowerUp): the item rises a
// whole block over 64 frames at 1 px every 4, then walks at 1 px/frame
#define kItemRisePx 16
#define kItemRiseFramesPerPx 4U
#define kItemWalkPx 1
// our own cadence: the bible documents no star bounce, so the hop is tuned rather than sourced
#define kItemGravitySubpx 24U
#define kItemMaxFallPx 4
#define kStarBouncePx -4
// the star's own gravity: smb1's star hops about two blocks, not the mushroom's much longer arc, so
// it falls back to earth faster than kItemGravitySubpx would let it (apex ~32px, ~16 frames up)
#define kStarGravitySubpx 64U
// the coin a block pays out pops straight up and falls back over kCoinPopFrames; smb's own arc
// length is unsourced in the bible, so this cadence is ours
#define kCoinPopFrames 30U
#define kCoinPopRisePx 2
// an item that walks this far off either side of the camera is despawned
#define kItemDespawnMarginPx 32

// the pipe transition. physics.json timers.pipe_transition is must-measure (only the 0.75 px/frame
// pipe-intro speed cap is documented), so the 1 px/frame sink over a full block is our own
#define kPipeStepPx 1
#define kPipeTravelPx 16

// enemies (games/mario/src/enemies.c). sprite family 0xc0.. per the milestone's tile-id contract,
// and the whole 16-tile family is spent: a goomba and a shell are left-right symmetric, so their
// frames cost one 8x16 pair each and the right half is the same tile drawn flipped; the koopa
// faces, so its two walk frames carry both halves the way mario's frames do
#define kTileEnemyFirst 0xC0U
#define kTileGoombaWalk0 0xC0U
#define kTileGoombaWalk1 0xC2U
#define kTileGoombaSquash 0xC4U
#define kTileShell 0xC6U
#define kTileKoopaWalk0 0xC8U
#define kTileKoopaWalk1 0xCCU
#define kEnemyTileCount 16U // 0xc0-0xcf

// the paratroopa's two frames. smb draws it as the koopa with a white wing, so each frame is the
// koopa's own facing pair with the wing baked into the shell half - four tiles a frame, eight in
// all, and the 0xc0 family in vram bank 0 has none left. they ride at the same ids in VRAM BANK 1
// and are drawn with S_BANK in the sprite's own prop, the way super mario's jump slab already is.
// the body is walk0's for both frames: a flyer's feet never shuffle, only the wing beats
#define kTileParaFly0 0xC0U
#define kTileParaFly1 0xC4U
#define kParaTileCount 8U // bank 1, 0xc0-0xc7

// the compiled kinds, the contract with compile_level.py's ENEMY_KIND_MAP. roster.json: the red
// koopa turns at a ledge where the green one walks off, and the piranha lives in a pipe
#define kEnemyGoomba 0U
#define kEnemyKoopa 1U
#define kEnemyKoopaRed 2U
#define kEnemyPiranha 3U
// 1-3's red paratroopa. it holds its spawn column, ignores gravity and terrain entirely, and
// slides up and down around the row it came in on
#define kEnemyKoopaParaRed 4U

// a pool slot's state. the pool is kept packed, so kEnemyOff never sits in a live slot: it is the
// value a slot is cleared to and the one the host twin starts a fresh slot at
#define kEnemyOff 0U
#define kEnemyWalk 1U
#define kEnemySquashed 2U
#define kEnemyShellIdle 3U
#define kEnemyShellMove 4U
// a piranha never walks: it rises out of its pipe and sinks back, and roster.json says it refuses
// to come up at all while the player is standing on or beside the cap
#define kEnemyPlantHidden 5U
#define kEnemyPlantUp 6U
// a body defeated by a fireball or by a moving shell: smb turns it upside down, pops it upward and
// drops it out of the level rather than blinking it away. it collides with nothing from the frame
// of the hit on, and its pool slot frees the moment it leaves the level or the camera
#define kEnemyFlipped 7U

// the milestone doc's oam trap: five slots x 2 sprites plus mario's 2 is 12 on screen, but a
// scanline crossing a row of 16x16 enemies pays 2 sprites for each of them, so four on one row is
// 8 plus mario's 2 = 10 exactly, the hardware's per-line ceiling. the spawner refuses a fifth
// same-row activation and leaves that enemy pending until a slot on the row frees
#define kEnemySlots 5U
#define kEnemyRowCap 4U

#define kEnemyWidthPx 16
#define kEnemyHeightPx 16
// the same 2 px shoulder inset the player's hitbox keeps, so neither dies on a pixel of overlap
#define kEnemyHitInsetPx 2
#define kEnemyHitWidthPx (kEnemyWidthPx - 2 * kEnemyHitInsetPx) // 12
// feet above this line inside the enemy's box make the contact a stomp instead of damage
#define kEnemyStompLinePx (kEnemyHeightPx / 2) // 8

// the two bands the object loader works in. an enemy comes in as its roster cell reaches the spawn
// band - out to the screen's right edge plus this margin, and mirrored back to one enemy width plus
// this margin off the left edge - and frees its pool slot once it is this much further out, either
// side.
//
// the gap between the two is what stops a walker that just stepped off the left edge being dropped
// straight back onto a cell the camera is still parked on: a cell has to leave the spawn band
// entirely before whatever is waiting at it re-arms.
//
// a deliberate departure from smb1, and the user's choice: the bible's loader walks the roster with
// a single cursor that only ever advances, so scrolling back over ground already crossed leaves it
// empty. here an enemy that leaves the view alive is only parked - it comes back at its own roster
// cell when that cell scrolls into range again from either side, a shell coming back as a fresh
// walking koopa and a paratroopa back on the wing. only a kill (stomp, fireball, shell, star, or a
// fall out of the bottom of the level) is permanent, and only until the level reloads.
// see the kRoster* states in games/mario/src/enemies.c
#define kEnemySpawnMarginPx 0
#define kEnemyDespawnMarginPx 32

// our own cadences: the bible times neither the flattened goomba nor the enemy fall rate
#define kSquashFrames 30U
#define kEnemyGravitySubpx 24U
#define kEnemyMaxFallPx 4
#define kEnemyAnimFrames 8U
// and the flip-fall's own arc, which is deliberately brisker than a walker's fall: the corpse is
// scenery and wants to be off the screen quickly. smb gives it a small upward pop first
#define kEnemyFlipPopPx -3
#define kEnemyFlipGravitySubpx 160U
#define kEnemyFlipMaxFallPx 8
// the piranha's cycle: a whole block up, a bite, a whole block back down, then a wait. the bible
// times none of it, so all four counts are ours
#define kPlantRisePx 16
#define kPlantHoldFrames 60U
#define kPlantCycleFrames (2U * kPlantRisePx + 2U * kPlantHoldFrames) // 152
// the centering bug: compile_level.py's bible entry for a piranha names the same column as the
// pipe it sits in (roster.json measures pipes and plants together off the map rip), which is the
// pipe's left of its two 16px-wide columns. the plant's own box is one enemy width (16px), so
// enemies.c's spawn() places it flush on that column - hugging the pipe's left lip - instead of
// centred; adding this to its pos_x centres a 16px box across the pipe's full 32px span. this is
// wired up in spawn()'s existing `if (e->kind == kEnemyPiranha)` branch, right after pos_y is set
// from foot_col - that branch runs once per plant, as the camera reaches it, not once a frame, so
// it is nowhere near the engine's per-frame instruction budget. moving the plant is a real gameplay
// change, though, so it retuned two frame-exact host tests that depended on the old off-centre
// position: mario_star_invincibility and mario_autopilot_completes_1_2
#define kPlantCenterOffsetPx (kEnemyWidthPx / 2)
// the paratroopa's own band, all ours: the bible times no paratroopa at all. smb1's red one slides
// up and down over about three blocks with no horizontal motion, so it is a constant one pixel a
// frame between spawn_y - kParaBandPx and spawn_y + kParaBandPx - a 96-frame round trip. the slot
// needs no new field for it: y_accum carries how far into the band the flyer is (0..2*kParaBandPx,
// starting at the centre) and dy which way it is going, neither of which a flyer uses otherwise
#define kParaBandPx 24
#define kParaSpanPx (2 * kParaBandPx) // 48
// must-measure: smb wakes an untouched shell after about ten seconds, which is this many frames at
// 60fps. no disassembly line for it was found in the bible, so the count is ours until one is
#define kShellWakeFrames 600U
// and the frames a freshly kicked shell cannot hurt the player, so the kick itself is not a death
#define kShellGraceFrames 8U

// the powerup chain (games/mario/src/powerup.c). the two invincibility windows come from the bible
// through gen_physics.py; everything below is our own cadence, which the bible does not time
#define kGrowFrames 64U
#define kGrowFlipFrames 8U
// how fast the injury blink and the star's palette flash alternate
#define kBlinkMask 0x02U
#define kStarFlashMask 0x04U
// the prop value powerup_sprite_prop returns on a frame the blink hides him entirely
#define kSpriteHidden 0xFFU
// smb hands out three lives; nothing displays the counter until m8's hud
#define kStartLives 3U

// fireballs: at most two live at once, the bible's own limit for fire mario
#define kFireballSlots 2U
#define kFireballPx 8
// the bible documents no landing bounce, so this rebound is ours. must-measure
#define kFireballBouncePx -4
// how far in front of his box centre a thrown ball starts
#define kFireballLeadPx 6

// which grid is loaded: the level's own, or one of its compiled sub-areas by index
#define kAreaMain 0xFFU
// an area's kind, the contract with compile_level.py's AREA_* constants
#define kAreaKindBonus 0U
#define kAreaKindWarp 1U

// a level's own type, the contract with compile_level.py's TYPE_* constants; it picks the palette
// set the streamer tints every tile family with
#define kLevelTypeOverworld 0U
#define kLevelTypeUnderground 1U
#define kLevelTypeCastle 2U

// the object list's kinds, the contract with compile_level.py's OBJ_* constants
#define kObjPipe 0U
#define kObjLiftH 1U
#define kObjLiftV 2U
#define kObjFirebar 3U
#define kObjBowser 4U
#define kObjAxe 5U
// milestone 1-2 rebuild: a pipe that teleports within the SAME main grid to another column range
// (a "segment") rather than switching to a sub-area's own banked grid. object_param indexes the
// level's jump_target_column/jump_target_row arrays. used for 1-2's above-ground/underground/
// above-ground three-segment layout: pressing down over one of these cuts to the target column
// with the lcd off, exactly like entering a sub-area, but level_grid never reloads because the
// whole level was already unpacked into it at level_load - only the vram ring and bg palette catch up
#define kObjPipeJump 6U
// and the same teleport walked into sideways rather than dropped into: object_column is the mouth
// rim's column, object_row the mouth's top row (the mouth is that row and the one under it), and
// object_param indexes jump_target_column/jump_target_row exactly like kObjPipeJump's does. it is
// how 1-2 leaves the underground - right into the mouth, out of the ending's pipe cap
#define kObjPipeSide 7U

// flow_pipe_under_player()'s sub-area index and a same-grid jump index share one uint8_t return
// value (0xff means neither): a jump index is this bit set over the low bits, kept well clear of
// 0xff even with every low bit set, so main.c's state machine never has to know the difference -
// it always just carries the value forward into enter_sub_area(), which is what actually branches
// on this flag. no level ever comes close to 64 sub-areas or 64 jumps, so the split is free
#define kJumpAreaFlag 0x40U

// lifts (games/mario/src/hazards.c). physics.json platform_lift_speeds is must-measure: the
// disassembly has a routine per lift type but the bible extracted no constant from any of them,
// so 1 px a frame either way is ours, picked to read like smb's own unhurried decks
#define kLiftSpeedPx 1
// two decks fit in oam on their own (kSpriteLiftFirst); the third only exists on a level with no
// firebar and no bowser, whose slots it takes over
#define kLiftSlotsShared 2U
#define kLiftSlots 3U
#define kLiftBlocks 2U
#define kLiftWidthPx (kLiftBlocks * kBlockPx) // 32
#define kLiftDeckPx 8
// compile_level.py packs a lift's travel into one byte: the low bits are its span in columns and
// the top bit starts it at the far end running the other way
#define kLiftSpanMask 0x3FU
#define kLiftReverse 0x80U

// firebars. physics.json firebar_rotation gives two raw rates (0x28 slow, 0x38 fast) and calls the
// conversion to degrees/frame must-measure, and it never says which bar takes which rate. so every
// bar here spins at the slow one, fed as a 1/256 sub-step into a 32-step circle: 6.4 frames a step,
// 205 frames a revolution, which is about smb's own. must-measure
#define kFirebarSpinRaw 0x28U
#define kFirebarSteps 32U
#define kFirebarSegments 6U
#define kFirebarRadiusPx 8
#define kFlamePx 8
// only the bar nearest mario is ever live, so the pool needs one slot per segment and no more:
// m8b took the two spare slots back for the hud. sprites are 8x16, so 8 px apart puts two of a
// vertical bar's flames on any one scanline - plus mario's four and the hud's five, which is
// eleven, one past the ten the hardware draws per line. the hud and mario are earlier in oam, so
// the flame is what drops, and only where a bar's top segment reaches the hud band in 1-4
#define kFirebarSlots 2U
#define kFlameSlots kFirebarSegments

// the fake bowser. roster.json calls him "a Little Goomba in disguise" and gives no speed, so he
// patrols at the goomba's own half a pixel a frame; his fire breath is m9's
#define kBowserWidthPx 16
#define kBowserHeightPx 16
#define kBowserSpeedSubpx 8U

// how far outside the view a lift, bar or bowser has to be before the game loop stops calling
// into bank 5 for it at all. smb runs its objects only while they are near the screen too, and
// on 1-2 that is the whole level bar the last forty columns
#define kHazardMarginPx 64U

// the hud (games/mario/src/hud.c): one tile row of the WINDOW layer drawn straight over the sky,
// whose map is 0x9c00 while the level's ring keeps 0x9800. sprites are not an option - oam was
// exactly full at 40 slots - and the window costs no slot at all. the row has to stop after 8 px,
// so an lyc stat handler at scanline kHudBarLines drops LCDCF_WINON and the vbl handler puts it
// back; both live in terrain.c beside the scroll shadows because an isr has to be resident in
// bank 0 (see terrain_install_isrs).
//
// there is no black bar: every cell of the row carries kHudBarAttr, whose color 0 is the level's
// own sky, so an unlit cell is the backdrop and the readout floats on it rather than on a band.
// at the camera the game actually plays at (kPlayScy, and kCamLookUpPx above it at most) the top
// 8 px of the view are open sky in all four levels - compile_level.py's highest solid rows are
// the underground roof at block row 2, the flag ball at row 2 and a brick platform at row 5, and
// the view opens on block rows 6-14. the two things the row can pass in front of are the top half
// of a block row 6 cloud and, with the vertical window pushed to its limit, the underground roof;
// either way it hides 8 px of them, which is what smb's own bar does to the same tiles
// the row sits one tile down from the top edge: flush against it read as cramped
#define kHudRowTopPx 8U
#define kHudBarRows 2U
#define kHudBarLines (kHudRowTopPx + 8U)
// the one live row, and the second the isr's own latency can leak onto (the stat interrupt lands a
// few dots into scanline 8, so the ppu may already have fetched that line): painted with blanks,
// which are sky, so the leak is invisible
#define kHudRow 0U
// the layout across the twenty columns: coin icon, an x and the two coin digits on the left, the
// six score digits centred, the three time digits on the right. no labels - the user wants the
// numbers and nothing else
#define kHudCoinIconCol 0U
#define kHudCoinXCol 1U
#define kHudCoinCol 2U
#define kHudScoreCol 7U
#define kHudTimeCol 17U
#define kHudCoinDigits 2U
#define kHudTimeDigits 3U
// hud_score counts tens, so five digits and the trailing zero the cards also print
#define kHudScoreDigits 5U

// the row's glyphs: the digits and the one x it prints, re-encoded into vram bank 1 with the ink
// on color 1 and the cell left on color 0. kCamPalSky's color 0 is the backdrop in all three level
// palette sets and its color 1 is white in each of them - the overworld's clouds and pennant are
// what put white there, and no underground or castle tile draws the sky slot's color 1 at all
// (clouds and the pennant only ever stand in an overworld segment), so assets_data.c sets those
// two sets' color 1 to the same white the overworld already had. color 0 of the sky slot is never
// touched: it is what the host tests read to name the palette set.
//
// the ids are 0x80-0x94 in vram BANK 1. a bg id past 0x7f reads out of 0x8800.. (lcdc bit 4 is
// clear), the same bytes bank 1's sprite ids 0x80.. name, so the run has to dodge the bank-1
// sprite art too: the climb poses at kTileClimbSmall (0xe0..) and the fireball's spin frame at
// kTileFireball. 0x80-0x94 collides with none of them and with no terrain family - a bg id is
// all a host probe sees, so a hud glyph sharing an id with a block face would count as that block
#define kTileHudDigitFirst 0x80U // 0x80-0x89, '0' up
#define kTileHudBlank 0x8AU      // the space glyph, which re-encodes to an all-sky cell
// a hand-drawn tile rather than a font one: a gold coin (color 1) with a darker slot (color 2) on
// a transparent cell, which is what tells the coin count apart from the score. it wears
// kHudCoinAttr instead of kHudBarAttr: kCamPalCoin also keeps the sky in color 0, and its colors
// 1 and 2 are the gold ramp in the overworld and underground and the lava ramp in the castle
#define kTileHudCoin 0x8BU
// and one id per character of the list below, in that order
#define kTileHudLetterFirst 0x8CU // 0x8c, just the x
#define kHudGlyphChars "x"
#define kHudBarAttr ((uint8_t)(kCamPalSky | kCamAttrVram1))
#define kHudCoinAttr ((uint8_t)(kCamPalCoin | kCamAttrVram1))

// the countdown. the bible pins one tick every 24 frames, but that reads as a broken clock, so
// ours ticks once per real second (60 frames), from the level json's timer field. hurrying up is
// music, which is m10's, so a low timer changes nothing but the digits until it reaches zero,
// which kills him
#define kTimerFramesPerTick 60U
#define kTimerMax 999U
// the coin counter rolls over rather than resets, and the life it pays is smb's own rule
#define kCoinsMax 99U
#define kLivesMax 99U
#define kScoreMax 9999U

// the death beat. smb freezes the world, holds, then leaps mario up and drops him through the
// floor; none of the three counts is sourced, so all of them are ours
#define kDeathHoldFrames 24U
#define kDeathLaunchPx -5
// the leap sheds a pixel of speed every fourth frame rather than through a subpixel accumulator
#define kDeathGravityMask 0x03U
#define kDeathMaxFallPx 5
// a pit or a lava pool has already taken him below the level, so that death only holds
#define kDeathFromHit 0U
#define kDeathFromPit 1U

// the cards, all our own cadence. the clear card counts the remaining time into points at smb's
// own 50 a tick, a few ticks a frame so a full 400 does not outlast the card
#define kGameOverFrames 120U
#define kClearCardFrames 90U
#define kTimeBonusTicksPerFrame 8U

// sram (games/mario/src/save.c). the cart is MBC5+RAM+BATTERY with one 8 kb bank, so three slots
// cost 32 of the 8192 bytes and no bank switching. layout:
//
//   0x00..0x03  magic "MAR2" - the 4th byte is the version, so a layout change is a magic change
//   0x08..0x0f  slot 0: [0] in use, [1] furthest level, [2..3] score, [4..7] reserved
//   0x10..0x17  slot 1
//   0x18..0x1f  slot 2
//
// systems.md: smbd saves per level, and the english build resets form and score on a reload - so a
// slot carries the furthest level reached plus the score standing when it was written, and picking
// the file starts that level small with a fresh three lives.
//
// "furthest" is the highest UNLOCKED node, 0..kLevelCount: node i is cleared when i < furthest and
// still to do when i == furthest, so clearing the last level leaves it at kLevelCount with every
// node marked done. the map clamps it back to the last node when it places mario
#define kSramBase 0xA000U
#define kSaveMagic0 'M'
#define kSaveMagic1 'A'
#define kSaveMagic2 'R'
#define kSaveMagic3 '2'
#define kSaveSlots 3U
#define kSaveSlotBase 8U
#define kSaveSlotStride 8U
#define kSaveSlotUsedOffset 0U
#define kSaveSlotLevelOffset 1U
#define kSaveSlotScoreOffset 2U
#define kSaveBytes (kSaveSlotBase + kSaveSlots * kSaveSlotStride) // 32
// the one-slot layout that shipped before this: magic "MAR1", then the furthest level and the
// score at 4 and 5. save_init migrates it into slot 1 rather than discarding it - the two fields
// mean exactly what they still mean, so there is nothing to guess, and a player who had progress
// keeps it. the level byte's old meaning was also "furthest unlocked", so it carries over as is
#define kSaveLegacyMagic3 '1'
#define kSaveLegacyLevelOffset 4U
#define kSaveLegacyScoreOffset 5U
// no file picked: a debug/lab run records nothing, so a lab clear cannot invent a save
#define kSaveNoSlot 0xFFU

// the m2 debug camera (no player, free d-pad scroll) still ships, entered with b from the title now
// that select is the play camera's look-ahead. define this to 0 to drop the state from the rom
#ifndef kDebugCamera
#define kDebugCamera 1
#endif

// the enemy lab: select from the title starts 1-1 with a denser roster than the bible places
// anywhere in it - 1-1's one koopa stands alone and no row of it ever holds four enemies, so the
// shell-chain and scanline-cap tests would have nothing to watch. define this to 0 to drop it
#ifndef kEnemyLab
#define kEnemyLab 1
#endif

// the title's level select: left and right step through world one before start begins it. it moved
// off up/down in m8b, which the new-game/continue menu now owns. without it a test that wants 1-4
// has to clear the three levels ahead of it first, which is minutes of emulation for one probe.
// define this to 0 to drop it
#ifndef kLevelSelect
#define kLevelSelect 1
#endif

// the timer lab: select with down held from the title starts the selected level with a countdown
// of kShortTimerTicks instead of the level's own. a real 400 is 24000 frames of idling for one
// probe, which is minutes of host emulation. plain a is off limits here: the frontend maps the
// space bar to a, and space is the advertised start key. define this to 0 to drop it
#ifndef kTimerLab
#define kTimerLab 1
#endif
#define kShortTimerTicks 70U

#endif
