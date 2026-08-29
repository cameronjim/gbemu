#ifndef TETRIS_H
#define TETRIS_H

// tuning constants live here so logic files never carry magic numbers

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kScreenRows 18U
#define kTitleRow 7U
#define kPromptRow 10U

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U
#define kFontLastTile 0x5FU
// the font foreground/background shade indices baked into the glyph bitmap by font_color
#define kFontFore 3U
#define kFontBack 0U

#define kTileBytes 16U

// solid tiles parked past the font's 96 glyphs
#define kBorderTileId 0x60U
#define kWellEmptyTileId 0x61U
#define kBackdropTileId 0x62U
#define kWallTileId 0x63U
#define kFlashTileId 0x64U
// locked cells own a tile id per piece so a screen test can read the board back
#define kLockTileId 0x70U
// the falling piece is four sprites sharing one block tile; its palette carries the identity
#define kPieceSpriteTileId 0xE0U

// the border strip is the top and bottom screen row of the title card
#define kBorderTopRow 0U
#define kBorderBottomRow (kScreenRows - 1U) // 17

// title card palettes; entering play reloads every slot
#define kPalTitle 0U
#define kPalBorder 1U
// play: bg and sprite palette slots 0-6 are the piece identity, 7 is the chrome
#define kPalChrome 7U

// the well sits left of center; columns 13-19 are the panel, filled in milestone 24
#define kWellCols 10U
#define kWellRows 18U
#define kWellOriginCol 2U
#define kWellOriginRow 0U
#define kWallLeftCol 1U
#define kWallRightCol 12U

#define kCellPx 8U
// oam coords are offset by 8,16 from the screen
#define kOamXOffset 8U
#define kOamYOffset 16U

#define kPieceCount 7U
#define kRotCount 4U
// four cells per tetromino, drawn by four sprites
#define kPieceSprites 4U

#define kPieceI 0U
#define kPieceO 1U
#define kPieceT 2U
#define kPieceS 3U
#define kPieceZ 4U
#define kPieceJ 5U
#define kPieceL 6U

// spawn box top-left, so the three wide pieces land centered on the ten columns
#define kSpawnCol 3
#define kSpawnRow 0

// gb original level 0: one row every 53 frames
#define kGravityFrames 53U
// hold to repeat: an initial delay then a step every few frames
#define kDasDelay 15U
#define kDasRepeat 6U

// full rows go white for half a second before the collapse
#define kFlashFrames 30U
// vblank budgets: rows of ten cells rewritten per frame
#define kFlashRowsPerFrame 2U
#define kRedrawRowsPerFrame 3U

// lcg full period mod 256: multiplier is 1 mod 4 and the increment is odd
#define kRngMul 37U
#define kRngAdd 1U

#endif
