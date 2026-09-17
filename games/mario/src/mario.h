#ifndef MARIO_H
#define MARIO_H

// tuning constants live here so logic files never carry magic numbers

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleRow 6U
// every card banner is a padding row, the text row, then another padding row, so the tinted band
// reads as a band rather than exactly the glyph height. the title card carries no text at all now -
// it is the generated smbd frame, see title_art.c - so kTitleRow only heads the game over and
// course clear cards
#define kBannerRows 3U
// smbd's pause screen, measured off a capture (160x144, cells of 8): PAUSE on row 1, WORLD 1-x on
// row 3 from column 6, mario's sprite on rows 5-6 at column 7 with x and his lives on row 6, then
// CONTINUE / SAVE / END every other row from row 8 with the cursor in column 6. white ink on black;
// mario's cells wear his own colours in a bg slot the level gets back on resume
#define kPauseTitleRow 1U
#define kPauseTitleCol 7U
#define kPauseWorldRow 3U
#define kPauseWorldCol 6U
#define kPauseMarioRow 5U
#define kPauseMarioCol 7U
#define kPauseLivesRow 6U
#define kPauseLivesXCol 9U
#define kPauseLivesCol 11U
#define kPauseMenuRow 8U
#define kPauseMenuStep 2U
#define kPauseMenuCol 6U
#define kPausePalMario kCamPalGround

// the SELECT FILE screen is generated art now, not a text card: its layout lives with the art in
// games/mario/src/file_art.h and its labels are drawn art cells, not printed lines
// the erase confirm is still its own text card over the shared machinery
#define kEraseRow 5U

// the front end lockout (games/mario/src/mapscreen.c). a fresh screen ignores start/a/b for this
// many frames, and a button already held when it opens stays ignored until it is released - without
// it every screen confirms on its own first live frame, so three quick taps of the space bar (title,
// then file select, then the map) drop the player straight into a level before the map is even seen
#define kFrontLockFrames 20U

// the world one map (games/mario/src/mapscreen.c), a single static 20x18-cell screen of generated
// art (games/mario/art/map/map_screen.png, loaded by map_art.c): a black WORLD/1-N header, the smbd
// world one strip, then a black lives/CLEAR LIST footer. nothing scrolls - world one fits the 160 px
// strip whole - so every position here is a plain tile row, and the node cells the walk runs between
// live in map_art.h beside the art they were measured off
// the footer starts right after the strip, at tile row 12, leaving rows 12-17 for it
#define kMapFooterFirstTileRow 12U
// the walk between two nodes, a pixel a frame along the straight line joining them: 40 to 48 px, so
// well under a second, which is a walk rather than a teleport
#define kMapWalkPx 1
#define kMapWalkAnimFrames 8U

// the "world 2 is on its way" popup, shown once a file's furthest node reaches kLevelCount (world
// one cleared) every time the map opens: a bordered modal card centered over the map's middle,
// wide and tall enough to read as a dropped-in dialog rather than a tint over the terrain. its
// border rows are kMapPopupTopRow/kMapPopupBottomRow, kMapPopupWidth columns wide and centered on
// the 20-col screen (kMapPopupLeftCol); it covers most of the strip plus one footer padding row, so
// dismissing it puts every row it covered back from the generated map rather than repainting one row
#define kMapPopupLeftCol 2U
#define kMapPopupWidth 16U
// its bottom border lands exactly on kMapFooterFirstTileRow, the one tile row the footer's own
// black band would otherwise own there - see map_draw_popup_hide in mapscreen.c for the row that
// blanks it back
#define kMapPopupBottomRow kMapFooterFirstTileRow
#define kMapPopupTopRow (uint8_t)(kMapPopupBottomRow - 7U)
// interior rows, top to bottom: a blank pad, "WORLD 2", "IS ON ITS WAY!", a blank pad, "PRESS A"
// banded in its own accent color, then a trailing blank pad before the bottom border
#define kMapPopupWorldRow 7U
#define kMapPopupWayRow 8U
#define kMapPopupPressRow 10U

// the color the map screen art paints its header and footer bands with. near-black rather than
// literal (0,0,0) only so a host probe can tell "the map is up, showing its band" apart from the
// file select, whose own frame sits on true black; the eye cannot tell the two apart
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
// m18's background pass. a surface ground block is the rip's own rubble and the rows under it are
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
// the sideways pipe mouth, the classic L: a 2-row-tall mouth facing left, then a column of
// horizontal body, then an ordinary vertical shaft. all four are solid. 1-2 has two of them and
// 1-1's bonus room is the way out of the coin room; their art is the capture's own (gen/pipe_side.c)
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
// the rest of a lava pit. kBlockLava wears the rip's breaking wave along its top and flat red under
// it, which is right for the cell at the surface and wrong for every cell below - a three-deep pit
// stamped with it alone reads as three separate pools. so the compiler stamps the surface row with
// kBlockLava and fills the rows under it with this, whose four quadrants are all flat red. scenery
// like the lava itself: the pit kills through the death plane, not through a kind
#define kBlockLavaFill 49U
// the castle window's mirror twin. the smbd capture's window is one 8px tile, not a whole cell:
// the tower carries a window either side of its middle column, and each opening hugs the middle -
// so the left cell is [masonry, opening] and the right one [opening, masonry]. that is not an
// x flip of the left cell, because the masonry's mortar joint runs down one edge of its tile and a
// flip would move it (kCamAttrXFlip costs 24 px against the capture, see rip_tiles.py's SHARING
// report). it needs no art of its own either: it is kBlockCastleWindow's own four tiles with the
// columns swapped, so it adds a kind and not one vram id
#define kBlockCastleWindowRight 50U
// a big hill's own middle. smb draws a hill's interior with a shading mark - two dark pixels in
// the upper right of each 16x16 cell - and the capture puts one in every interior cell EXCEPT the
// bottom row's centre, which is flat green all the way across. that is one cell of the five-wide
// dome (the small hill has none of it at all), and stamping kBlockHillFill there instead put a
// mark in the middle of the hill's belly where smb has none. it costs no vram id: all four of its
// quadrants are kTileHillFillTl, the fill cell's own flat tile, so the kind is the whole change
#define kBlockHillCore 51U
// the shaft joint of a sideways pipe, one kind per row of the mouth. where the horizontal body runs
// into its vertical shaft, both smbd captures draw the shaft's left cell with the body's rim running
// across it and the joint's dark line down its own left column - neither the plain body cell the
// compiler used to stamp there nor a side-pipe cell. its right column IS the plain body's, so each
// kind owns two tiles of its own and the pair shares one bank-1 copy of kTilePipeBodyM
// (gen/pipe_joint.c): a kind's attribute byte picks one vram bank for all four quadrants, and the
// body's own tile is in bank 0. 1-2 stands two pairs, 1-1's bonus room one. solid like the shaft
#define kBlockPipeJointT 52U
#define kBlockPipeJointB 53U
// the chain from the bridge's far end up to the axe, one cell over the deck's last column. the smbd
// castle capture draws it as a diagonal line of white over stone through the cell's upper right and
// lower left quadrants (gen/chain.c), the other two empty; the bible had no cell for it. scenery on
// the masonry's slot, and it goes with the axe: hazards_drop_bridge clears it the frame the bridge
// starts to come apart
#define kBlockBridgeChain 54U
#define kBlockKindCount 55U
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
// ground family 0xa0..0xa3: a surface block's upper half, then the fill block's, the same two tiles
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
// pipe family 0xb0..0xbb. m23's rip pass measured the capture's pipe and found that neither
// shortcut the old nine-tile layout took actually holds: the right half is not the left half
// mirrored (136 pixels apart on the body alone), and the two cells do not share a tile column
// down the middle (70 pixels apart on the lip). so all four cells - the lip's two and the body's
// two - carry their own quadrants, twelve tiles in all. the three extra ids are 0xb9-0xbb, which
// were the last free ids of the pinned block. the body is still one row of tiles repeated down
// the pipe, because the capture's body cell IS its own top half twice
#define kTilePipeLipL 0xB0U   // the left lip cell, top left
#define kTilePipeLipM 0xB1U   // the left lip cell, top right
#define kTilePipeLipR 0xB2U   // the right lip cell, top left
#define kTilePipeLipLb 0xB3U  // the left lip cell, bottom left
#define kTilePipeLipMb 0xB4U  // the left lip cell, bottom right
#define kTilePipeLipRb 0xB5U  // the right lip cell, bottom left
#define kTilePipeBodyL 0xB6U  // the left body cell, left column
#define kTilePipeBodyM 0xB7U  // the left body cell, right column
#define kTilePipeBodyR 0xB8U  // the right body cell, left column
#define kTilePipeLipRr 0xB9U  // the right lip cell, top right
#define kTilePipeLipRbr 0xBAU // the right lip cell, bottom right
#define kTilePipeBodyRr 0xBBU // the right body cell, right column
// the world coin's four quadrants close the pinned block
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

// --- m23's rip pass: the scenery the capture would not let the rom mirror, 0xd0-0xdd -----------
// the run above assumed three of smb's scenery pieces were their left twin flipped, and the smbd
// capture says only one of them is. the right hill slope really is the left one mirrored, pixel
// for pixel, and keeps kCamAttrXFlip; the right cloud cap (28 px apart on its top row, 26 on its
// bottom) and the right bush cap (13 px) do not, and neither do the right halves of the castle's
// two crenels (56 px and 23 px). those fourteen tiles are what this run holds.
//
// they sit at 0xd0 rather than at the 0x8d the bank-1 bg table's first free span starts at,
// because a bank-1 bg id at or above 0x80 shares its bytes with the bank-1 SPRITE id of the same
// number: 0x8d-0x94 is reserved as hud-font headroom and 0x96-0xbd is bowser. 0xd0-0xdd is free
// on both sides of that table (see VRAM.md)
#define kTileCloudCapRtl 0xD0U
#define kTileCloudCapRtr 0xD1U
#define kTileCloudCapRml 0xD2U
#define kTileCloudCapRmr 0xD3U
#define kTileCloudCapRbl 0xD4U
#define kTileCloudCapRbr 0xD5U
#define kTileCloudCapRfl 0xD6U
#define kTileCloudCapRfr 0xD7U
#define kTileBushCapRtl 0xD8U
#define kTileBushCapRtr 0xD9U
#define kTileBushCapRbl 0xDAU
#define kTileBushCapRbr 0xDBU
#define kTileCastleCrenelRight 0xDCU
#define kTileCastleCrenelInnerRight 0xDDU
#define kTileSceneryRipFirst 0xD0U
#define kTileSceneryRipCount 14U

// --- 1-3's tree run, 0x0a-0x0f, also in VRAM BANK 1 ---------------------------------------------
// the scenery run above is contiguous and exactly full: 0x5e-0x5f is only two ids and 0x60-0x71
// is the map screen's own bank-1 art. so the tree takes the free ids under the map screen's
// castle instead (bank-1 bg 0x00-0x09, see assets.h), which nothing else in either screen touches.
//
// m26 re-cut every one of these off the smbd 1-3 capture (games/mario/tools/rip_tiles.py --level
// 1-3, gen/tree.c and gen/trunk.c) and the re-cut cost two ids LESS than the hand art it replaced,
// because the capture says the canopy's right cap is the left one's EXACT mirror - the first right
// cap in this game that is (1-1's cloud cap is 28 px off its twin and its bush cap 13, which is
// why those carry tiles of their own at 0xd0-0xdb). so kBlockTreeTopR is drawn as the left cap's
// four tiles with the columns swapped and kCamAttrXFlip in its palette byte, the trick
// kBlockHillSlopeR already rides, and the two tiles the hand art spent on it (the old
// kTileTreeCapTr/kTileTreeCapBr at 0x0f-0x10) are gone. 0x10-0x11 are FREE again.
//
// five tiles is all the canopy costs because the rest of the shape repeats: a plain top row serves
// the left cap's inner quadrant and both of the middle's, and one plain scalloped bottom serves
// the left cap's inner one. the capture's dark-green notch accents only appear under the middle -
// they are where the next scallop of an unbroken run starts, and a run ends at a cap - so the
// middle's bottom is its own tile. the trunk's stripes have an 8px period in both axes, so one
// tile stamped four times is the whole column, and it is the one tree tile on the brick slot's
// browns rather than the pipe slot's greens
#define kTileTreeFirst 0x0AU
#define kTileTreeCapTl 0x0AU
#define kTileTreeTop 0x0BU
#define kTileTreeCapBl 0x0CU
#define kTileTreeBot 0x0DU
#define kTileTreeBotM 0x0EU
#define kTileTreeCount 5U
#define kTileTrunk 0x0FU

// --- the castle run, 0x12-0x1c, in VRAM BANK 1 ---------------------------------------------------
// the bg tiles the castle needs and bank 0 had no ids for. m27 cut every one of them off the smbd
// 1-4 capture (games/mario/tools/rip_tiles.py --level 1-4): the masonry course pair, the bridge's
// two halves, a 16px axe, the chain from the bridge to the axe, and the lava (0x20-0x21, in the
// scenery run below).
//
// the masonry: the capture lays its wall in running bond, 8px bricks whose courses step half a
// brick every 8 rows, so a 16px cell is one course over another offset one. the joint falls at the
// same place in both halves of a course, which is why two tiles cover it - the upper one stamps
// kBlockCastleBrick's top pair and the lower one its bottom pair (and the same pairing overwrites
// the ground family at a castle load, so a ground cell tiles into the bond too). the bridge is a
// 4px repeat across, so one tile per 8px band stamps both quadrants of its row. the axe fills its
// whole cell - two blades over a haft that runs down the lower half, which the hand-drawn axe left
// empty - so it is four tiles, the top pair at 0x13-0x14 and the bottom pair at 0x19-0x1a. the
// chain is two: the diagonal crosses the cell's upper right and lower left quadrants and the other
// two are kTileScenBlank. bank-1 bg 0x1d-0x1f and 0x5e-0x5f are still unclaimed
#define kTileCastleBrickLower 0x12U
#define kTileAxe 0x13U
#define kTileAxeRight 0x14U
#define kTileBridge 0x15U
#define kTileBridgeLower 0x16U
#define kTileCastleBrickUpper 0x17U
#define kTileCastleRunCount 6U
// the flat red the capture paints a pit below its surface cell, stamped in all four quadrants of
// kBlockLavaFill and in the surface cell's lower pair (kTileLavaFill is the same tile again)
#define kTileLavaDeep 0x18U
#define kTileAxeBl 0x19U
#define kTileAxeBr 0x1AU
#define kTileChainTr 0x1BU
#define kTileChainBl 0x1CU

// the sideways pipe's twelve tiles, ripped off the smbd capture's own bonus room (gen/pipe_side.c,
// anchors pipe_side_* in rip_tiles.py). the run is 32px tall - the mouth cell over its lower cell,
// and the body the same again - so four 8px bands cover it. the mouth column needs a left and a
// right tile per band, because the rim line runs down its left edge and the joint to the body down
// its right; the body column is flat across its 16px, so one tile per band is drawn in both halves
// of the cell. it is NOT the vertical pipe transposed, which is what the nine hand-drawn tiles this
// replaces assumed: the capture's sideways cross section has no backdrop margin and one more light
// row than the vertical one, and the mouth's two cells share no tile.
// vram bank 0 has no tile ids left at all, so they ride in bank 1 past the map screen's own run
// (0x60-0x71, declared in assets.h), and their kBlockPalette entries carry kCamAttrVram1 the way
// every scenery kind's does
#define kTilePipeSideMouth0L 0x72U
#define kTilePipeSideMouth0R 0x73U
#define kTilePipeSideMouth1L 0x74U
#define kTilePipeSideMouth1R 0x75U
#define kTilePipeSideMouth2L 0x76U
#define kTilePipeSideMouth2R 0x77U
#define kTilePipeSideMouth3L 0x78U
#define kTilePipeSideMouth3R 0x79U
#define kTilePipeSideBody0 0x7AU
#define kTilePipeSideBody1 0x7BU
#define kTilePipeSideBody2 0x7CU
#define kTilePipeSideBody3 0x7DU

// the shaft joint's five tiles, 0xe0-0xe4 in vram bank 1 (gen/pipe_joint.c, anchors pipe_joint_*
// in rip_tiles.py): each joint kind's own left column, top tile then bottom, and the one bank-1
// copy of kTilePipeBodyM both wear down their right. they stand past m23's run at 0xd0-0xdd
// because a bank-1 bg id at or above 0x80 shares its bytes with the sprite id of the same number,
// and 0xde-0xdf is the fireball's second spin frame; 0xe0-0xeb is free on both sides (VRAM.md)
#define kTilePipeJointT0 0xE0U
#define kTilePipeJointT1 0xE1U
#define kTilePipeJointB0 0xE2U
#define kTilePipeJointB1 0xE3U
#define kTilePipeJointBody 0xE4U
#define kTilePipeJointCount 5U

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
// m23 read this off the bonus room in the smbd capture, which is drawn on flat black
#define kUndergroundRgb RGB(0, 0, 0)
#define kCastleRgb RGB(1, 1, 3)

// sprite family 0xe0.. per the milestone's tile-id contract. small mario is 16x16 = two 8x16
// sprites, so one animation frame costs four 8x8 tiles: left top/bottom then right top/bottom.
// m22 redrew the whole set off the smbd sprite rip (games/mario/tools/rip_sprites.py, art in
// games/mario/src/gen/mario_small.c): the six poses below keep their pinned ids and order, and the
// seventh - the death pose, which the hand art never had - takes four of the ids super mario's old
// bank-0 block gave back
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
// the seventh small pose, facing the viewer with both hands up. only player_draw's dying branch
// reaches it, and only small mario ever dies (a big one shrinks first), so it needs no big twin.
// its frame index is 6 the way the generated table orders it - the same slot the big set spends on
// the crouch, which is why one anim_frame byte still names a pose in either body
#define kFrameDeath 6U
#define kTileMarioDeath 0x70U // 0x70-0x73

// super/fire mario, 16x32 = eight 8x8 tiles a pose: [UL top, UL bot, UR top, UR bot, LL top,
// LL bot, LR top, LR bot], which is what png2tiles' sprites16 order emits and what player_draw's
// two draw_row calls read. m22's exact art gives every pose its own upper half - the old set
// shared one slab across six of them, which is why its jump could not raise an arm - so the eight
// poses are 64 tiles and bank 0 has nowhere near that many. the whole set lives in CGB VRAM BANK 1
// at ids 0x00-0x3f, drawn with S_BANK in the sprite's own prop, and bank 0's old 0x60-0x7f block
// is handed back to the koopa and the death pose. bank-1 sprite ids below 0x80 collide with no bg
// family: a bg id under 0x80 reads out of 0x9000.., which no sprite can reach
#define kTileSuperFirst 0x00U
#define kSuperTilesPerFrame 8U
#define kSuperFrameCount 8U
#define kSuperTileCount (kSuperFrameCount * kSuperTilesPerFrame) // 64, bank 1 ids 0x00-0x3f
// the two poses past the six small mario shares its order with: the fold, whose 22-tall art is
// bottom-aligned in the same 16x32 box (rows 10..31), and the flagpole grip
#define kFrameCrouch 6U
#define kFrameClimbBig 7U

// player_pose's byte: the frame index in the low nibble, and the four flags the sprite pass wants
#define kPoseFrameMask 0x0FU
#define kPoseClimbing 0x10U
#define kPoseGone 0x20U
#define kPoseBehindBg 0x40U
#define kPoseBig 0x80U

// small mario's own flagpole grip, one 16x16 pose in VRAM BANK 1. it used to ride at the id his
// standing set holds in bank 0, which meant a host test reading framebuffer tile numbers - they
// carry the tile and a sprite bit but not the bank - could not tell the grip from his idle pose and
// so could not know which pose's art bounds it was measuring. it takes four of the ids super
// mario's old bank-0 block gave back instead, where nothing in either bank collides with it. the
// big grip is pose 7 of the set above and needs no id of its own at all
#define kTileClimbSmall 0x74U

// the pennant as it comes down the pole: the flag head's four bg tiles (bank 1, kTileFlagClothT..)
// written again as two 8x16 sprites in bank 0's 0x78-0x7b, the run big mario's old block gave back.
// the family is already stored column by column, so one call lays both pairs
#define kTilePennant 0x78U // 0x78-0x7b

// the fire flower, the one item outside the 0xd0 family
#define kTileFlowerFirst 0x80U // 0x80-0x83; gen/flower.h's kFlowerTileCount is the count

// m8a's actors. the piranha is a 16x24 plant bottom-aligned in a 16x32 box and left-right
// symmetric, so it costs two 8x16 pairs - the box's left half, top and bottom - and its right half
// is the same pair drawn S_FLIPX. the flame and the lift deck are hand art that only moved ids
#define kTilePiranha 0x84U   // 0x84-0x87, the left column's top pair then its bottom pair
#define kTileFlame 0x88U     // and 0x89, its blank lower half
#define kTileLiftDeck 0x8AU  // and 0x8b, ditto
#define kPiranhaTileCount 4U // 0x84-0x87
#define kHazardTileCount 4U  // 0x88-0x8b, the flame and the deck together
// 0x78-0x7f and 0xc8-0xcf are free in both banks, and so is bank-0 0x74-0x77

// m19's throwaway animations take six of the twenty ids m8b's hud digit sprites left free at 0x8c
// (the bar draws its own digits out of the bg font now, see kTileHudDigitFirst). each is an 8x16
// pair whose lower half is blank, the way the fireball's own pair already is: one quarter-brick
// fragment, drawn spinning by cycling its flip bits, and the fireball's two-frame puff.
// 0x92-0x9f, fourteen ids, is still unclaimed
#define kTileDebris 0x8CU   // and 0x8d, its blank lower half
#define kTilePuffA 0x8EU    // 0x8f blank
#define kTilePuffB 0x90U    // 0x91 blank
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
// fireball as one 8x16 pair whose top tile is empty - the family is exactly full at 0xd0-0xdf.
// m22 replaced the mushroom, the star and the 1-up with the smbd rip's own (gen/items.c) and the
// spin frame with gen/fireball.c; the coin pop is still hand art
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

// --- oam: the forty slots, and the cgb sprite palettes ------------------------------------------
//
// m22's exact art made two enemies tall: a koopa and a paratroopa are 16x24 bottom-aligned in a
// 16x32 box, which is two rows of two 8x16 sprites - four oam slots where the old 16x16 art took
// two - and the piranha is the same shape. that doubled the enemy pool's worst case and oam has no
// spare forty-first slot, so the pool and the hazards pool now MEET IN THE MIDDLE rather than each
// owning a fixed run:
//
//   0-3    mario. small parks the lower row; big is two rows of two, every pose (see player_draw)
//   4-8    the throwaway animations: four brick fragments then the fireball's puff (debris.c).
//          the world map borrows 4-11 for its four node markers and the toad room 4-7 for the
//          retainer, both on screens that have no debris, no items and no enemies; the clear
//          borrows 4-5 for the pennant coming down the pole (flow.c), dropping any break still in
//          the air - nothing can break a brick once the pole has him
//   9-10   the loose item a block paid out       11  the coin pop      12-13  the two fireballs
//   14-33  THE ENEMY POOL, allocated fresh every frame: the live slots are walked in pool order
//          and each takes two oam slots if its art is 16x16 (goomba, squashed goomba, either
//          shell) or four if it is a 16x32 box (koopa, paratroopa, piranha, and any of the three
//          upside down as a corpse). five slots of four is the ceiling, hence 20; nothing else may
//          hand out a slot in that run, so no two enemies can ever share one. the score popup
//          (popup.c) is the one thing drawn inside it: the two slots right past what the pool used
//          this frame, only while they lie under 24, and parked on a frame they do not
//   24-39  THE HAZARDS POOL, whose floor is whatever the enemy pool left: hazards.c starts at
//          max(24, enemies_oam_top()) each frame and hands what is above it to the deck planks,
//          bowser's body, his breath and a firebar's flames. the two ends only actually collide on
//          a frame with five live enemies of which three are tall AND a lift or a bowser in view -
//          1-4 carries no walking enemy at all and 1-2's lift shafts are one deck at a time - and
//          a claimant that cannot get its slots is dropped for that frame, the same graceful
//          degradation the pool already gave a third firebar
#define kSpriteMarioL 0U
#define kSpriteMarioR 1U
#define kSpriteMarioLowL 2U
#define kSpriteMarioLowR 3U
#define kSpriteFreeFirst 4U
#define kSpriteFreeCount 5U
#define kSpritePennantL 4U
#define kSpritePennantR 5U
// the world map's own claim on the same run: two sprites per node, four nodes, slots 4-11. it is a
// card screen - no debris, no items, no enemies - so it owns all forty and hands these back the
// moment a level loads
#define kSpriteMapMarkerFirst 4U
#define kSpriteItemL 9U
#define kSpriteItemR 10U
#define kSpriteCoin 11U
#define kSpriteFireFirst 12U
// the enemy pool's run. kEnemyOamMax is kEnemySlots * 4, the ceiling every entry being tall costs
#define kSpriteEnemyFirst 14U
#define kEnemyOamShort 2U
#define kEnemyOamTall 4U
#define kEnemyOamMax 20U
// the hazards pool: sixteen slots at the top of oam, one owner byte each (hazards.c slot_owner),
// so a slot that changes hands has its tile and palette written again and one that nobody claims
// is parked. kHazardPoolFirst is only its FLOOR - enemies_oam_top() raises it on a frame the enemy
// pool has reached that far, and hazards.c refuses to hand out anything below the raised floor
#define kSpriteFlameFirst 24U
#define kHazardPoolFirst kSpriteFlameFirst
#define kHazardPoolSlots 16U
#define kHazardPoolLast (kHazardPoolFirst + kHazardPoolSlots - 1U) // 39
// the deck sprites one visible lift costs, and where they come from: the TOP of the pool, counting
// down, so a deck keeps its slots however far the enemy pool has climbed. the first two decks fit
// in 32-39 and a third borrows 28-31, which is bowser's and the firebar's ground - hazards.c
// refuses the third lift when either is loaded, and compile_level.py refuses to build such a level
#define kSpriteLiftPerDeck 4U
#define kSpriteLiftCount (kLiftSlotsShared * 4U) // 8
#define kSpriteLiftTop kHazardPoolLast
// m20's 32x32 bowser needs eight slots: he takes the FLOOR of the pool, 24-31 on a frame with no
// enemies in it. smb never puts a firebar in the bridge room - 1-4's last bar is fifty columns
// short of it - so only one of the two is ever on screen; he claims before the flames do, so on a
// frame that had both he keeps his whole body and the bar draws in what is left. the body is two
// rows of four 8x16 sprites, so a scanline crossing him draws four
#define kSpriteBowserCount 8U
// his fire breath is three more 8x16 sprites, 24 px of dart, just above his body. only a frame
// drawing two decks at once reaches those - the decks claim by how many are ON SCREEN, not by how
// many the level loaded, and 1-4's two lifts stand a hundred and thirty columns apart - and such a
// frame drops the breath's sprites, not the hazard itself
#define kSpriteBowserFireCount 3U
// the hardware's whole oam, which the map screen parks every slot of before drawing its two
#define kOamSlots 40U
// the eight obj palette slots. index 0 is always a sprite's transparency, so each is three colours,
// and m22's exact art pinned what those three are (the sheet's own hex, see assets_load_*_palettes):
//   0 kPalMario     skin / mario red / mario dark      small and big mario
//   1 kPalMushroom  white / item yellow / item red     the mushroom AND the star, whose tiles are
//                                                      drawn in the mushroom's index order
//   2 kPalStar      white / koopa orange / shell red   the red paratroopa, the red shell, the
//                                                      fireball, its puff and mario's star flash;
//                                                      a castle re-tints it to the fire ramp
//   3 kPalOneup     white / item yellow / item green   the 1-up AND the fire flower
//   4 kPalCoin      the coin pop's gold, and nothing else since the fireball moved to kPalStar
//   5 kPalGoomba    goomba tan / goomba brown / black  the goomba, its pancake, the brick debris
//                                                      and the lift deck
//   6 kPalKoopa     koopa green / koopa orange / white the koopa, the green shell and the piranha.
//                                                      that is exactly bowser's own three colours,
//                                                      so a castle's re-tint of this slot is now a
//                                                      no-op that is kept for the contract
//   7 kPalFire      skin / fire cream / mario red      fire mario, on the same tiles kPalMario wears
#define kPalMario 0U
#define kPalMushroom 1U
#define kPalStar 2U
#define kPalOneup 3U
#define kPalCoin 4U
#define kPalGoomba 5U
#define kPalKoopa 6U
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
// smbdis DoFootCheck (11975-12005): the feet are tested before the sides, and a foot sunk less than
// 5 px into a block's top (cpy #$05) lands on it rather than being stopped by it. that is what lets
// a running jump clip the edge of a stair and keep its speed
#define kLandGracePx 4
// smbdis HandlePipeEntry (12270): down is taken only when the LEFT foot point stands on the cap's
// left half and the RIGHT foot point on its right half. the two points are BlockBuffer_X_Adder's
// (13030) entries for a small or crouching player, 3 and 12 px into his box, so his box has to sit
// within 4 px of centred over the two-cell cap
#define kPipeFootLeftPx 3U
#define kPipeFootRightPx 12U
// smbdis PlayerAnimTmrData (6192) via GetPlayerAnimSpeed (6195): a walk pose holds 2 frames at
// 0x1c subpx a frame or faster, 4 at 0x0e or faster, and 7 below that. the same 1/16 px unit as
// our speeds, so the two thresholds are the disassembly's bytes
#define kWalkAnimRunSubpx 0x1CU
#define kWalkAnimWalkSubpx 0x0EU
#define kWalkAnimRunFrames 2U
#define kWalkAnimWalkFrames 4U
#define kWalkAnimSlowFrames 7U

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

// the level-clear sequence, our own cadence but for the slide (smbd's exact frame counts are
// unsourced). smb's beat: grab the pole, slide down it with the flag coming down alongside, flip to
// the pole's far side and wait there while the flag finishes, hop off, walk to the castle and step
// into the door. the slide is smb's own 2 px a frame, his and the pennant's alike: smbdis
// FlagpoleRoutine (6590) moves the flag 1 px plus the carry of a 0xff adder every frame, and
// AutoControlPlayer climbs him down at the same rate
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
#define kClearHopFrames 12
#define kClearHopPx 2
#define kClearWalkPx 1
// 1 px a frame is 16 subpx, the middle of PlayerAnimTmrData's three: a pose every 4 frames
#define kClearWalkAnimFrames kWalkAnimWalkFrames
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
// smbdis BumpBlock (7307) sends the block up at 2 px a frame under ImposeGravityBlock's 0x50/256
// gravity (7660) and kills it when its y comes back within 5 px (7480): a 14-frame arc that sits
// 4 px or higher, i.e. rounds to the raised tile row, for 11 of them
#define kBumpFrames 11U
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
// smbdis JCoinC (6989) and JCoinRun (7046): the coin a block pays out leaves at -5 px a frame under
// 0x50/256 gravity, in smb's 8.8 fixed point, and is over the moment it is falling at 5 - 32 frames,
// 43 px up at frame 15. blocks_draw.c steps it, because bank 0 has no room for the arithmetic
#define kCoinPopLaunchDy -5
#define kCoinPopGravity 0x50U
#define kCoinPopEndDy 5

// the floating score (games/mario/src/popup.c). smbdis SetupFloateyNumber (11533) gives the label
// 0x30 frames and FloateyPart (1325) lifts it a pixel on each of them, drawn 8 px over the point it
// was set at. kPopupOneUp is the tens value that means the 1-UP label rather than a number
#define kPopupFrames 48U
#define kPopupRisePx 1
#define kPopupLiftPx 8
#define kPopupOneUp 0U
// an item that walks this far off either side of the camera is despawned
#define kItemDespawnMarginPx 32

// the pipe transition. physics.json timers.pipe_transition is must-measure (only the 0.75 px/frame
// pipe-intro speed cap is documented), so the 1 px/frame sink over a full block is our own
#define kPipeStepPx 1
#define kPipeTravelPx 16

// enemies (games/mario/src/enemies.c). the 0xc0 family holds everything 16x16: the goomba's one
// walk frame (its second is the same four tiles with the halves swapped and S_FLIPX - the smbd rip
// draws walk1 as the exact mirror of walk0), the squashed pancake and the green shell, both
// left-right symmetric and so one 8x16 pair each whose right half is the same tile flipped.
//
// the koopa is 16x24 and no longer fits there. it is bottom-aligned in a 16x32 box - eight tiles a
// walk frame, drawn as two rows of two 8x16 sprites - and takes the run super mario's old bank-0
// block gave back. 0xc8-0xcf, which held the koopa while it was 16x16, is free
#define kTileEnemyFirst 0xC0U
#define kTileGoombaWalk0 0xC0U // 0xc0-0xc3, the full 16x16 box
#define kTileGoombaSquash 0xC4U
#define kTileShell 0xC6U
#define kEnemyTileCount 8U    // 0xc0-0xc7
#define kTileKoopaWalk0 0x60U // 0x60-0x67
#define kTileKoopaWalk1 0x68U // 0x68-0x6f
#define kKoopaTilesPerFrame 8U
#define kKoopaTileCount 16U // 0x60-0x6f
// how far above its own 16x16 hitbox a 16x24 enemy's 16x32 art box starts: its feet are at the
// hitbox's bottom edge and the art is bottom-aligned in the box, so the box's top row is here
#define kEnemyTallRisePx 16

// --- m20's bowser run, 0x96-0xbb, in VRAM BANK 1 -----------------------------------------------
// roster.json gives bowser 4x4 tiles, so his body is 32x32: two frames of sixteen tiles, drawn as
// eight 8x16 sprites in two rows of four, out of the floor of the hazards pool. that is 512 bytes
// of art and vram bank 0 has nothing like that left, so it goes in bank 1 and the draw sets
// S_BANK, exactly the way the paratroopa and the whole of big mario already do.
//
// the run starts at 0x96 rather than 0x95 because an 8x16 sprite ignores the low bit of its tile
// index; every id below has to be even. bank-1 sprite 0x00-0x51 is big mario, the paratroopa and
// the red shell, 0x80-0x8c the hud font (a bg id past 0x7f reads out of the same 0x8800.. bytes,
// see kTileHudDigitFirst), 0xc8-0xcf the toad, 0xde-0xdf the fireball's second spin frame and
// 0xe0-0xe3 small mario's climb pose, so 0x96-0xbb collides with none of them
#define kTileBowserFirst 0x96U
#define kBowserTilesPerFrame 16U
#define kBowserArtFrames 2U
// and his fire breath, one 8x16 pair per third of a 24x8 dart, the lower half of each blank
#define kTileBowserFire 0xB6U
#define kBowserFireTileCount 6U // 0xb6-0xbb
// his open jaw, the telegraph the throw wears (kBowserJawOpenFrames). one 8x16 pair, the same
// sprite as his head's left half in both walk frames with the lower jaw dropped two px and the
// mouth behind it opened - the whole tell is in that one sprite, so a third full frame of him
// (sixteen more tiles) was never worth the bank-1 ids
#define kTileBowserJaw 0xBCU

// the paratroopa's two frames, the smbd rip's red ones: the koopa's body under a wing that beats
// across both columns of the box. each is the same 16x24-in-a-16x32 box the koopa walks in - eight
// tiles - so the pair is sixteen, and vram bank 0 has nothing like that left. they ride in VRAM
// BANK 1 at 0x40-0x4f, drawn with S_BANK in the sprite's own prop. bank-1 0xc0-0xc7, which held
// the old four-tile pair, is free
#define kTileParaFly0 0x40U
#define kTileParaFly1 0x48U
#define kParaTileCount 16U // bank 1, 0x40-0x4f
// and the shell a stomped red paratroopa leaves, which is red where the koopa's is green: one
// symmetric 8x16 pair of its own in bank 1, worn with kPalStar
#define kTileShellRed 0x50U // bank 1, 0x50-0x51
// the score popup's strip (popup.c, gen/popup.c): nine 8x16 columns in bank 1's free run past the
// red shell. the ids are numerically the koopa's bank-0 0x60-0x6f only for the 1-UP pair, which is
// why the strip starts here and not higher up
#define kTilePopupFirst 0x52U // bank 1, 0x52-0x63
#define kShellRedTileCount 2U

// --- the toad room, the beat past 1-4's axe (games/mario/src/toad.c) ---------------------------
// smb1 does not end a castle on the axe: the bridge goes, bowser goes with it, and mario walks
// right off the pedestal into the room past it, drops to its floor and stops in front of the
// mushroom retainer while the sign's three lines go up over him. the bible names the column he
// stands in (LevelInfo toad_column, level-1-4.json's bridge entry) and the row he stands on.
//
// the retainer is 16x24, transcribed off the nes rip cell for cell: two columns of 8x16 sprites
// and eight tiles, the lower pair of each column carrying his legs over eight transparent rows.
// he wears the castle's kPalStar - the fire ramp, white/orange/dark red - which is the only sprite
// slot with a white in it that a castle has anything else in, and nothing else is wearing it once
// the flames are gone. bank-1 sprite 0xc8-0xcf: big mario ends at 0x3f, the paratroopa at 0x4f,
// the fireball's spin frame is at 0xde and small mario's climb pose at 0xe0, so the run collides
// with none of them
#define kTileToadFirst 0xC8U
#define kToadTileCount 8U // bank 1, 0xc8-0xcf
// tiles are column-major, so a column's four are its own top-to-bottom 8x16 pair and then the pair
// under it: (0, 2) is the left column and (4, 6) the right
#define kToadTilesPerColumn 4U
#define kToadWidthPx 16
#define kToadHeightPx 24
// four oam slots, taken from the throwaway animations' own five (kSpriteFreeFirst). the brick
// fragments and the fireball's puff are the only things that ever want those, and neither can be
// alive in a room the player reaches by touching the axe
#define kSpriteToadFirst kSpriteFreeFirst
#define kSpriteToadCount 4U
// and where the walk off the pedestal stops: this many blocks short of him. one block put the two
// sprites shoulder to shoulder and dragged the view a block further left with them; two leaves a
// block of the room's floor between them and lands the camera on 147, which is a block boundary,
// so the sign's own tile columns fall where kToadSignLine0Col..2 say they do
#define kToadStopBlocks 2U

// the sign over him, in smb's own wording and smb's own line breaks: the rip prints "THANK YOU
// MARIO!", a blank line, then "BUT OUR PRINCESS IS IN" / "ANOTHER CASTLE!". nineteen tiles is the
// most the gb's twenty-column view can hold with a column of air either side, so the second
// sentence breaks one word earlier than the rip's - three lines where an earlier pass took four,
// which is a whole line of ink out of a band that only ever had seven rows to spend
#define kToadSignLines 3U
#define kToadSignLine0 "THANK YOU MARIO!"
#define kToadSignLine1 "BUT OUR PRINCESS IS"
#define kToadSignLine2 "IN ANOTHER CASTLE!"
// the longest of them, which is what the glyph run has to be able to lay down in one go
#define kToadSignCols 19U
// where the block goes. the base is this many columns left of the retainer, which on 1-4 is the
// block the camera's last view opens on: mario ends the walk kToadStopBlocks short of the retainer
// at column 151 and rides kCamFollowX four blocks into the view, so the view starts at 147 and the
// base is 153 - 6. each line then takes its own tile offset into those twenty columns, which is
// what centres it: 19 sits flush, 18 one in, 16 two in
#define kToadSignColumnsLeft 6U
#define kToadSignLine0Col 2U
#define kToadSignLine1Col 0U
#define kToadSignLine2Col 1U
// and the world tile row each prints on. tile rows rather than block ones because a glyph is 8 px,
// and the rip's own spacing: it prints its three lines four tile rows, then two tile rows apart -
// 8 px of ink with 8 px of black under the pair and 24 px under the first line.
//
// the band is boxed in on both sides and 15-21 is the whole of it. the camera settles at its lowest
// pan (kScyMax) once he is standing on the room's floor, which puts a tile row at 8R - 96 on
// screen: 15 lands the first line at 24, clear of the hud strip and of the scanline or two the
// window's own isr latency leaks under it, and 21 lands the last at 72-79, the row big mario's cap
// starts in and no lower. what the room past the axe bought is horizontal, not vertical
#define kToadSignLine0Row 15U
#define kToadSignLine1Row 19U
#define kToadSignLine2Row 21U
// and how long the whole tableau holds before the course-clear card takes over: three seconds,
// which is about what smb1 leaves it up for
#define kToadHoldFrames 180U
// the glyph run, in vram BANK 1 at bg ids 0xec-0xfd - one id per distinct character of the three
// lines above, in this order, re-encoded out of the resident font the way the hud row's digits are
// (assets_data.c hud_glyph): ink on color 1, cell on color 0, which under kToadSignAttr is white
// text standing on the castle's own black. a space needs no id of its own - kTileHudBlank already
// re-encodes to an empty cell - and neither does anything else, because these are the only
// characters the sign has
#define kTileSignFirst 0xECU
#define kSignGlyphChars "THANKYOUMRI!BPCESL"
#define kSignGlyphCount 18U // 0xec-0xfd
// the same attribute the hud row's glyphs wear (kHudBarAttr): kCamPalSky's color 0 is the level's
// backdrop in every set and its color 1 is white in every set
#define kToadSignAttr ((uint8_t)(kCamPalSky | kCamAttrVram1))

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

// the milestone doc's oam trap: a scanline crossing a row of enemies pays 2 sprites for each of
// them, so four on one row is 8 plus mario's 2 = 10 exactly, the hardware's per-line ceiling. the
// spawner refuses a fifth same-row activation and leaves that enemy pending until a slot on the
// row frees. m22's 16x32 enemies did not change that: a tall enemy's two sprite rows never share a
// scanline, so it still costs 2 per line - only its OAM SLOT count doubled, see the oam map above
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
// and where its art sits: the plant is 16x23 bottom-aligned in a 16x32 box (rows 9..31), and the
// box is placed so those lit rows start exactly on its hitbox - the 9 blank ones above them are the
// stem the pipe is meant to hide. enemies_draw rises the box by the first and culls on the second
#define kPlantArtRisePx 9
#define kPlantArtRowsPx 23
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
// two decks fit in the top of the hazards pool on their own; the third only exists on a level with
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
// and the other one the same table names, for the bars a level marks fast
#define kFirebarSpinFastRaw 0x38U
#define kFirebarSteps 32U
#define kFirebarSegments 6U
// the long bar smbdis's fifth variant is: twelve segments rather than six. only kFlameSlots of a
// bar's flames are ever drawn, which a bar this long clips into anyway, but every segment burns
#define kFirebarSegmentsMax 12U
#define kFirebarRadiusPx 8
#define kFlamePx 8
// compile_level.py packs a bar's whole variant into its object_param: the low nibble is how many
// segments it carries, then the two rate/direction bits. a param of zero is read as the short
// bar's six, so a level compiled before this contract still spins the bars it always did
#define kFirebarParamSegMask 0x0FU
#define kFirebarParamFast 0x10U
#define kFirebarParamCcw 0x20U
// how many of a bar's flames are ever drawn. a twelve-segment bar clips off the screen anyway -
// a horizontal one is 192 px across a 160 px screen - and every segment of it still burns
#define kFlameSlots kFirebarSegments
// m21: every bar whose sweep can reach the view rotates, burns AND draws, so a bar is on screen
// from the moment it scrolls in and the jump onto it can be timed. 1-4 puts two bars four columns
// apart, so the draw is capped at two full bars and hands its slots out nearest the camera centre
// first; a third bar in view draws however many flames the pool has left rather than none, and
// contact walks every live bar's whole segment list regardless of what was drawn.
//
// the per-scanline reality: sprites are 8x16, so 8 px apart puts two of a vertical bar's flames on
// any one line. two vertical bars stacked is four, plus mario's four is eight, inside the ten the
// hardware draws per line. the hud is on the WINDOW layer and costs no slot. where a third bar's
// flames land on the same line as those the line drops its tail, which is the cheapest thing on it
#define kFirebarDrawBars 2U
#define kFlameDrawCap (kFirebarDrawBars * kFlameSlots) // 12

// the fake bowser. roster.json gives him 4x4 tiles and 5000 points and says five fireballs put
// him down; it gives no speed, no hop and nothing about the breath's timing, so everything below
// but the box, the hit count and the points is ours
#define kBowserWidthPx 32
#define kBowserHeightPx 32
// the goomba's own half a pixel a frame, fed as a 1/256 sub-step. roster.json calls him "a Little
// Goomba in disguise", which is where the walk came from and is all it ever said about it
#define kBowserWalkSubpx 128U
// he hops on his own beat rather than at the player: the launch and the fall are a plain ramp, the
// death beat's own, because bank 0 has no room for a third gravity path
#define kBowserHopFrames 150U
#define kBowserHopLaunchPx -4
#define kBowserHopGravityMask 0x03U
#define kBowserMaxFallPx 4
// the breath: a 24x8 dart at the height of his own jaw, thrown every couple of seconds and
// carried left at a pixel and a half a frame until it leaves the view
#define kBowserFireFrames 130U
#define kBowserFireWidthPx 24
#define kBowserFireHeightPx 8
#define kBowserFireSubpx 384U
// and how long one dart lives: a hundred frames at that speed is 150 px, which clears the widest
// view he can be seen from. a count rather than a screen test keeps the twin's arithmetic exact
#define kBowserFireLifeFrames 100U
// where in his 32x32 box the dart leaves him. he faces left, so his jaw is the leftmost thing on
// him: the mouth in kBowserTiles opens at x 0-5, y 4-11 (the orange with the white teeth in it).
// the dart's RIGHT edge starts at kBowserJawPx, which is inside his head, so the flame is seen to
// come out of the mouth instead of appearing already clear of him - and that also buys the player
// most of his body's width in reaction time at the range the bridge fight is actually fought at
#define kBowserJawPx 8
// the mouth's own y, which the 8 px dart is centred on
#define kBowserFireJawPx 6
// and the swoop. a dart left at the jaw's height sails clean over small mario's head - his box on
// the deck starts eighteen px down bowser's - so smb1's flame, which tracks mario's row as it
// flies, is cut down to a fixed sink: one px a frame for this many frames, which lands it on the
// band the old fixed 18 put it in. the drop is the jaw throw's only; a zone dart is already aimed
#define kBowserFireDropPx 12
#define kBowserFireDropFrames 12U
// the telegraph. smb1 opens his mouth about half a second before the flame, and hazards.c swaps
// the head sprite for kTileBowserJaw over the last of the throw's wait rather than carrying a
// third animation frame: the jaw is one 8x16 sprite of his eight, so the tell costs two tiles
#define kBowserJawOpenFrames 30U
// the fire zone. smb1 arms his flame spawner several screens before the bridge and the flames come
// at mario off the RIGHT EDGE of the view while bowser himself is still off screen, at one of a
// small table of rows smb1 picks from mario's own y; only once he is on screen do they leave his
// jaw. the level says where the band starts (LevelInfo bowser_fire_x, read off 1-4's rip,
// which draws three flights of it before the bridge room). the table below is ours: the block row
// mario's feet stand in and the three over it, which is the shape smb1's has - one flame he has
// to jump, one he takes standing tall, two that sail over small mario's head. four consecutive
// rows, so hazards.c masks his walk tick for the height rather than carrying a table and an
// index into it: bank 5 had no bytes for either, and the tick is as good a shuffle as any
#define kBowserFireZoneRows 4U
// the mask that picks one of them, in px off his feet row: 0, 16, 32 or 48
#define kBowserFireZoneRowMask 0x30U
// where in a 16 px row the 8 px dart flies: centred, so a row's flame reads as that row's
#define kBowserFireZoneInsetPx 4
// roster.json: five fireballs defeat him and pay 5000. the sixth would be the axe's job
#define kBowserFireballHits 5U
// how many frames each of his two drawn frames holds for; a power of two, so the swap is a mask
#define kBowserAnimFrames 16U

// the bridge under him. smb pulls its cells one at a time from the axe end back toward the far
// side over about a second; twenty cells at this cadence is sixty frames, which is that
#define kBridgeDropFrames 3U

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
// sprite art too: bowser (0x96..), the toad (0xc8..), the fireball's second spin frame (0xde) and
// small mario's climb grip. 0x80-0x94 collides with none of them and with no terrain family - a bg
// id is
// all a host probe sees, so a hud glyph sharing an id with a block face would count as that block
#define kTileHudDigitFirst 0x80U // 0x80-0x89, '0' up
#define kTileHudBlank 0x8AU      // the space glyph, which re-encodes to an all-sky cell
// a hand-drawn tile rather than a font one: a gold coin (color 1) with a darker slot (color 2) on
// a transparent cell, which is what tells the coin count apart from the score. it wears
// kHudCoinAttr instead of kHudBarAttr: kCamPalCoin also keeps the sky in color 0, and its colors
// 1 and 2 are the gold ramp in the overworld and underground and the lava ramp in the castle
#define kTileHudCoin 0x8BU
// and one id per character of the two lists below, in that order: the first run fills the reserved
// headroom up to 0x95 (0x96 is bowser's first sprite tile, same bytes), the second sits in the gap
// between his jaw tile and the toad at 0xbe-0xc7. the pause card prints the letters
#define kTileHudLetterFirst 0x8CU // 0x8c-0x95
#define kHudGlyphChars "xWORLD-ESU"
#define kTileHudLetterSecond 0xBEU // 0xbe-0xc7
#define kHudGlyphChars2 "MAVQIT>CNP"
#define kHudBarAttr ((uint8_t)(kCamPalSky | kCamAttrVram1))
// the coin icon takes the used block's slot rather than the coin one or the question block's:
// color 0 is the level's own sky in all three sets there, colors 1 and 2 are the coin's gold and
// brown in all three (the castle set's question slot went teal with the capture's own ? block, and
// its coin slot is the lava's white-and-red ramp), and no castle stands a used block until its one
// ? block is hit
#define kHudCoinAttr ((uint8_t)(kCamPalSpent | kCamAttrVram1))

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
// smbdis GameOverInter: ScreenTimer $12, an interval timer, and IntervalTimerControl reloads $14,
// so 18 intervals of 21 frames; start ends it early (RunGameOver)
#define kGameOverFrames 378U
#define kClearCardFrames 90U
// smbdis AwardGameTimerPoints: one interval a frame at 50 points, a tick every frame d2 is set;
// then GameTimerFireworks: a last digit of 1, 3 or 6 is that many bursts. InitFireworks spaces
// them FrenzyEnemyTimer $20 apart, 48 px left of the castle flag plus its table; RunFireworks
// runs three frames of 8 and FireworksSoundScore pays 500. smb's flag pole sits at $b0 over a
// ground our grid puts 32 px lower
#define kFireworksSpacingFrames 32U
#define kFireworksBurstFrames 24U
#define kFireworksPoints 500U
#define kFireworksFlagPx 40U
#define kFireworksLeftPx 48U
#define kFireworksGroundShiftPx 32U
// smb's sky is 240 lines tall and ours 144, so the highest bursts would open over the top of the
// view: they are held to just under the hud strip instead (unmeasured against smbd)
#define kFireworksTopPx 16U

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
