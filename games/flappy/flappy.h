#ifndef FLAPPY_H
#define FLAPPY_H

// tuning constants live here so logic files never carry magic numbers

// identity bgp; gbdk's font clears to index 0, so a blank screen is white
#define kTitleBgp 0xE4U
// identity obp; the bird's colors 1-3 stay distinct against the white sky
#define kBirdObp 0xE4U

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleTextY 6U
#define kPromptTextY 10U
#define kBestTextY 12U
// the start press erases the text; columns 18-19 hold the previewed pipe
#define kTitleEraseCols 18U

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U
#define kTileBytes 16U

// an inverted copy of the font's first 64 glyphs: light strokes on a solid dark cell
#define kInvFontFirstTile 0x60U
#define kInvFontTiles 64U
// inverted space is a solid dark cell, so it is both the popup's fill and its blank glyph
#define kPopupFillTileId kInvFontFirstTile

// clear of the font tiles (0-95) and the score digits reserved at 0xD0-0xD9
#define kBirdTileId 0xE0U
#define kBirdFrames 3U
#define kBirdFrameUp 0U
#define kBirdFrameGlide 1U
#define kBirdFrameDown 2U
#define kDigitTileId 0xD0U
#define kDigitCount 10U
// oam coords are offset by 8,16 from the screen; screen x stays 40
#define kBirdOamX 48U
#define kBirdOamYOffset 16U
#define kBirdScreenX 40U
#define kBirdSizePx 8U
// the art leaves the tile's bottom row empty, so hits use the visible 7 rows
#define kBirdHitPx 7U

// world tile ids; the sky is the font's space glyph so it matches a cleared screen
#define kSkyTileId 0x00U
#define kPipeBodyLeftTileId 0xA0U
#define kPipeBodyRightTileId 0xA1U
#define kPipeCapLeftTileId 0xA2U
#define kPipeCapRightTileId 0xA3U
#define kGroundTileId 0xB0U

// the bg map is a 32 column ring; columns are rewritten as they scroll off the left
#define kMapCols 32U
#define kMapRows 18U  // 16 sky rows plus the ground
#define kPlayRows 16U // sky rows above the ground
#define kGroundTopPx 128U

#define kPipeWidthCols 2U    // 16 px
#define kPipeSpacingCols 12U // 96 px between pipe pairs
#define kFirstPipeCol 18U    // first pipe pair sits at the right edge, previewed on the title
#define kGapTopMin 1U        // one row of pipe always shows above the gap
#define kGapTopSlack 1U      // and one row always shows below it

#define kPipeWidthPx 16U
#define kPipeSpacingPx 96U
#define kFirstPipeWorldX 144U // kFirstPipeCol * 8

// difficulty by score: speed is 8.8 px per frame, gap is rows of clear air
#define kDiffSteps 5U
#define kDiffScores {0U, 10U, 20U, 35U, 50U}
#define kDiffSpeeds {256U, 320U, 320U, 384U, 384U}
#define kDiffGaps {6U, 6U, 5U, 5U, 4U}

// lcg full period mod 256: multiplier is 1 mod 4 and the increment is odd
#define kRngMul 37U
#define kRngAdd 1U

// hud: three digit sprites top-center, oam coords offset by 8,16 from the screen
#define kHudFirstSprite 1U
#define kHudDigits 3U
#define kHudOamY 32U       // screen y 16
#define kHudCenterOamX 88U // screen x 80; the run is centered by half a digit per digit
#define kDigitWidthPx 8U
// a badge is 7 px of art in the 8 px tile; this pitch joins the halos into one plate
#define kDigitPitchPx 7U
#define kScoreMax 999U // three digits is all the hud and the popup can show

// game over popup: a band of bg cells over the frozen world, no window layer involved
#define kPopupTopRow 4U // rows 4..13 of 18 give the four text lines a blank row of air each
#define kPopupRows 10U
// scx can sit mid tile, so one more column than the screen may be visible
#define kPopupCols 21U
#define kPopupRowsPerFrame 2U // 42 cells is a comfortable vblank budget
#define kPopupOverRow 1U
#define kPopupScoreRow 3U
#define kPopupBestRow 5U
#define kPopupPromptRow 8U
// dead input after a crash so a panic flap cannot skip the popup
#define kOverLockoutFrames 20U

// 8.8 fixed point: 256 units is one pixel
#define kFixedShift 8
#define kBirdStartY 15360 // 60.0 px
#define kBirdCeilingY 0   // 0.0 px
#define kBirdFloorY 30976 // 121.0 px: the visible bottom row rests on the ground line
#define kGravityVy 36     // +0.14 px/frame^2
#define kFlapVy (-563)    // -2.20 px/frame
#define kTerminalVy 896   // +3.50 px/frame

// title hover: no gravity, a 64 frame bob straddling the start height
#define kTitleBobPeriod 64U
#define kTitleBobHalf 32U
#define kTitleBobStep 48   // 0.1875 px per tick, so 32 ticks spans 6 px
#define kTitleBobBias 768  // 3.0 px
#define kTitleFlapShift 4U // one wing frame every 16 ticks

// a flap plays wing up then glide then down, then rests on glide
#define kFlapAnimFrames 12U
#define kFlapAnimStep 4U

// battery sram, mbc1 bank 0: 4 magic bytes then the best score little endian
#define kSramBase 0xA000U
#define kSaveMagic0 'F'
#define kSaveMagic1 'L'
#define kSaveMagic2 'P'
#define kSaveMagic3 'Y'
#define kSaveBestOffset 4U

// apu: master on, every channel to both ears, both ears at max volume
#define kNr52On 0x80U
#define kNr51All 0xFFU
#define kNr50Max 0x77U

// flap: ch1, 50% duty, fast downward sweep, envelope decays in ~0.23 s
#define kFlapSweep 0x1EU
#define kFlapDuty 0x80U
#define kFlapEnvelope 0xF1U
#define kFlapFreqLo 0x00U
#define kFlapFreqHi 0x86U // trigger plus freq 0x600: 256 hz

// score: ch2, an octave above the flap and just as short
#define kScoreDuty 0x80U
#define kScoreEnvelope 0xF1U
#define kScoreFreqLo 0x00U
#define kScoreFreqHi 0x87U // trigger plus freq 0x700: 512 hz

// hit: ch4 noise, 15 bit lfsr, slower decay so the crash rings out
#define kHitLength 0x00U
#define kHitEnvelope 0xF3U
#define kHitPoly 0x35U
#define kHitTrigger 0x80U

#endif
