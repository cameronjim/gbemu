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

// title screen best score, under press start
#define kBestRow 12U

// the well sits left of a 2-column backdrop margin; the panel fills columns 14-19
#define kWellCols 10U
#define kWellRows 18U
#define kWellOriginCol 3U
#define kWellOriginRow 0U
#define kWallLeftCol 2U
#define kWallRightCol 13U

// right panel: score, level, lines, and the next-piece box. the panel is exactly six cells,
// columns 14-19, right of the wall at column 13. well.c's draw_frame already lays the backdrop
// tile across all six, so everything below sits on the panel plate.
//
// every element is CENTERED in that six cell span. a whole cell element can only be centered
// exactly when its width is even -- (6 - width) / 2 has to be a whole number of cells -- so
// every runtime value and the next box is an even number of cells wide, and the three five
// character labels, which cannot be renamed to an even length, are baked at font build time
// into six tile strips shifted right four pixels (see kPanelStripTileId and panel.c).
#define kPanelCol 14U
#define kPanelCols 6U
#define kPanelLabelChars 5U // "SCORE" / "LEVEL" / "LINES"

#define kScoreLabelCol kPanelCol // baked strip, the full span -> 14-19
#define kScoreLabelRow 1U
#define kScoreValueRow 2U
#define kScoreValueCol kPanelCol // 6 digits, the full span -> 14-19
#define kScoreDigits 6U
#define kLevelLabelCol kPanelCol // baked strip -> 14-19
#define kLevelLabelRow 5U
#define kLevelValueRow 6U
#define kLevelValueCol 16U // 2 digits centered -> 16-17
#define kLevelDigits 2U
#define kLinesLabelCol kPanelCol // baked strip -> 14-19
#define kLinesLabelRow 9U
#define kLinesValueRow 10U
#define kLinesValueCol 15U // 4 digits centered -> 15-18
#define kLinesDigits 4U
#define kNextLabelCol 15U // "NEXT" width 4, centered -> 15-18
#define kNextLabelRow 13U
#define kNextBoxRow 15U // one blank row (14) under the label
#define kNextBoxCol 15U // 4 wide, centered -> 15-18
#define kNextBoxCols 4U
#define kNextBoxRows 2U

// the three five character labels, each composed into a six tile strip shifted right half a
// cell so it centers exactly. parked right after the panel letter block (0x8a-0x94); the next
// id in use after these is the sprite's 0xe0.
#define kPanelStripTileId 0x95U
#define kPanelStripCols kPanelCols
#define kPanelStripCount 3U
#define kPanelStripScore 0U // tiles 0x95-0x9a
#define kPanelStripLevel 1U // tiles 0x9b-0xa0
#define kPanelStripLines 2U // tiles 0xa1-0xa6
// (kPanelCols - kPanelLabelChars) * 8 / 2: half a cell
#define kPanelStripShift 4U

// a dedicated digit tile block, so tests read numbers off the bg directly. these hold recolored
// copies of the stock ibm font's own digit glyphs (see panel_build_font), not a separate font.
#define kDigitTileId 0x80U
#define kDigitCount 10U
// recolored copies of the stock font's c e i l n o r s t v x, parked right after the digits.
// same letterforms as the title and game-over card, just sitting on the panel's dark chrome
// shade instead of the font's own boxed background.
#define kPanelLetterTileId 0x8AU
#define kPanelLetterCount 11U

// classic scoring: single/double/triple/tetris, each times (level + 1)
#define kLineScoreTable {40U, 100U, 300U, 1200U}
#define kScoreCap 999999UL
// the lines field is four cells wide (an odd width cannot center in the six cell panel), so the
// cap is what four digits can show rather than the three the old field held
#define kLinesCap 9999U

// classic gb-style curve: frames per row by level, index 0..kLevelMax
#define kGravityTable                                                                                        \
    {53U, 49U, 45U, 41U, 37U, 33U, 28U, 22U, 17U, 11U, 10U, 9U, 9U, 8U, 8U, 7U, 7U, 6U, 6U, 5U, 5U}
#define kLevelMax 20U

// battery sram, mbc1 bank 0: 4 magic bytes then the u32 best score little endian
#define kSramBase 0xA000U
#define kSaveMagic0 'T'
#define kSaveMagic1 'T'
#define kSaveMagic2 'R'
#define kSaveMagic3 'S'
#define kSaveBestOffset 4U

// game over popup: a fixed band over the well; the bg never scrolls so placement is static.
// flappy's band rhythm: a blank fill row top and bottom, and a blank row between every text line.
#define kPopupCol 0U
#define kPopupTopRow 5U
#define kPopupRows 9U
#define kPopupOverRow 1U
#define kPopupScoreRow 3U
#define kPopupTopScoreRow 5U
#define kPopupPromptRow 7U
#define kPopupRowsPerFrame 2U

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

// apu: master on, every channel to both ears, both ears at max volume
#define kNr52On 0x80U
#define kNr51All 0xFFU
#define kNr50Max 0x77U

// four rows at once is a tetris and earns the bigger chime
#define kTetrisRows 4U

// rotate: ch1, 50% duty, no sweep, a bright blip that decays in ~0.23 s
#define kRotateSweep 0x00U
#define kRotateDuty 0x80U
#define kRotateEnvelope 0xF1U
#define kRotateFreqLo 0x80U
#define kRotateFreqHi 0x87U // trigger plus freq 0x780: 1024 hz

// lock: ch2, a soft short tock, quieter and quicker than the rotate blip. ch2 is free at lock
// time (the clear chime below only fires after, on a completed line). volume 6 decaying by 1
// every period at 1/64s is about 0.09s total, against the rotate blip's volume 15
#define kLockDuty 0x40U
#define kLockEnvelope 0x61U
#define kLockFreqLo 0x78U
#define kLockFreqHi 0x85U // trigger plus freq 0x578: 202 hz

// line clear: ch2, one clean tone ringing ~0.47 s
#define kClearDuty 0x80U
#define kClearEnvelope 0xF2U
#define kClearFreqLo 0x00U
#define kClearFreqHi 0x87U // trigger plus freq 0x700: 512 hz

// a four row clear gets its own note: an octave and a half up, ringing ~0.94 s
#define kTetrisEnvelope 0xF4U
#define kTetrisFreqLo 0xA0U
#define kTetrisFreqHi 0x87U // trigger plus freq 0x7a0: 1365 hz

// level up: ch1, a fast rising whoop the sweep's own overflow cuts off
#define kLevelSweep 0x15U // period 1, upward, shift 5
#define kLevelDuty 0x80U
#define kLevelEnvelope 0xF1U
#define kLevelFreqLo 0x00U
#define kLevelFreqHi 0x86U // trigger plus freq 0x600: 256 hz

// game over: ch1 sweeping down, the longest sound in the game
#define kOverSweep 0x4CU // period 4, downward, shift 4
#define kOverDuty 0x80U
#define kOverEnvelope 0xF5U
#define kOverFreqLo 0x00U
#define kOverFreqHi 0x87U // trigger plus freq 0x700: 512 hz

#endif
