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

// one solid tile parked past the font's 96 glyphs, used for the border strip
#define kBorderTileId 0x60U
#define kTileBytes 16U

// the border strip is the top and bottom screen row
#define kBorderTopRow 0U
#define kBorderBottomRow (kScreenRows - 1U) // 17

// cgb bg palette slots: white text on a dark backdrop, and the border's own colour
#define kPalTitle 0U
#define kPalBorder 1U

#endif
