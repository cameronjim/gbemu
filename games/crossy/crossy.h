#ifndef CROSSY_H
#define CROSSY_H

// tuning constants live here so logic files never carry magic numbers

// identity bgp; gbdk's font clears to index 0, so a blank screen is white
#define kTitleBgp 0xE4U
// identity obp; the chick's colors 1-3 stay distinct against the grass
#define kChickObp 0xE4U

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U

// the grid: a lane is 16 px (2 tile rows), a column is 16 px (2 tile cols)
#define kCellPx 16U
#define kGridCols 10U
#define kMaxCol 9U

// bg terrain tile ids, per the milestone's tile contract
#define kGrassTileId 0xA0U
#define kTreeTileId 0xA1U
#define kRoadTileId 0xA2U
#define kRoadStripeTileId 0xA3U
#define kWaterTileId 0xA4U
#define kRailTileId 0xA5U
#define kRailWarnTileId 0xA6U
// odd world lanes take the second grass tile, so a lane boundary reads even in grayscale
#define kGrassAltTileId 0xA7U
// water follows the same parity rule: one tile fills a whole lane, so a boundary is a shade step
#define kWaterDarkTileId 0xA8U
// the train's middle is bg, not sprites: one carriage over a track lane's two tile rows
#define kTrainBodyUpperTileId 0xA9U
#define kTrainBodyLowerTileId 0xAAU

// a lane is one of four kinds; the kind is cached per ring slot
#define kLaneGrass 0U
#define kLaneRoad 1U
#define kLaneWater 2U
#define kLaneTrack 3U

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U
#define kTileBytes 16U

// an inverted copy of the font's first 64 glyphs: light strokes on a solid dark cell
#define kInvFontFirstTile 0x60U
#define kInvFontTiles 64U
// inverted space is a solid dark cell, so it is both the popup's fill and its blank glyph
#define kPopupFillTileId kInvFontFirstTile

// every sprite is 8x16, so a tile id's low bit is ignored and art comes in consecutive pairs
#define kTilesPerSprite 2U
// sprites: the chick at rest and in flight, then the ten hud digits
#define kChickTileId 0xE0U
#define kChickHopTileId 0xE2U
#define kChickTileCount 4U
#define kDigitTileId 0xC8U
#define kDigitCount 10U
#define kDigitTileCount 20U

// the bg ring is 32 rows tall, so 16 lanes are buffered and scy wraps for free
#define kMapRows 32U
#define kRingLanes 16U
#define kRingLaneMask 15U

// screen y of the camera lane's top row; 9 lanes fit on screen, 6 of them ahead
#define kCamLaneScreenY 96U
#define kLanesAhead 6U
#define kInitLanes 7U // lanes 0..6 are visible the frame the run starts
// the chick may trail the camera by two lanes and still be fully on screen
#define kMaxLanesBehind 2U

// the chick is one 8x16 sprite centered across its 16 px cell; its art is 13 px tall
#define kChickCellInset 4U
#define kChickHalfPx 4U
// two px of grass under the sprite's top edge centers the art in the cell
#define kChickLaneInset 2U
// on water the chick sits at the lane's top edge, so its head clears the log's crown
#define kChickWaterInset 0U
#define kChickSpawnCol 4U
// oam coords are offset by 8,16 from the screen
#define kOamXOffset 8U
#define kOamYOffset 16U
#define kSpritePx 8U

// a hop slides 16 px over 8 frames
#define kHopSlidePx 16
#define kHopStepPx 2
// one cell of travel in the chick's 8.8 pixel x
#define kCellFixed 0x1000U

// generation: the first lanes are plain so the run always starts clear
#define kPlainLanes 3U
// lanes alternate grass with a danger chunk; danger lanes never carry trees
#define kRoadChunkMin 1U
#define kRoadChunkSpan 3U
// water clusters tighter than road: three logs a mover is twice the sprite cost
#define kWaterChunkMin 1U
#define kWaterChunkSpan 2U
// two grass lanes minimum keeps the visible danger lanes inside the oam budget
#define kGrassChunkMin 2U
#define kGrassChunkSpan 2U
// the guaranteed-open column wanders at most two columns and never hugs an edge
#define kGapWanderSpan 5U // rng % 5 gives -2..+2 after the bias
#define kGapWanderBias 2
#define kGapMin 1
#define kGapMax 8

// lcg full period mod 256: multiplier is 1 mod 4 and the increment is odd
#define kRngMul 37U
#define kRngAdd 1U
// bit 7 runs the lcg's full period; bit 0 merely alternates
#define kRngTopBit 0x80U

// hud: three digit sprites top-center, oam coords offset by 8,16 from the screen
#define kChickSprite 0U
#define kHudFirstSprite 1U
#define kHudDigits 3U
#define kHudOamY 24U       // screen y 8
#define kHudCenterOamX 88U // screen x 80; the run is centered by half a digit per digit
// the badge fills the whole cell, so digits at this pitch read as one unbroken strip
#define kDigitWidthPx 8U
#define kDigitHalfPx 4U
#define kScoreMax 999U // three digit sprites is all the hud can show
// the hover banner reuses the hud's three sprites, so any best is centered whatever its length
// the banner's lanes draw no movers, so those scanlines carry three sprites and nothing else
#define kHoverBestOamY 56U // screen y 40, the banner's last row
#define kHoverBestCenterOamX 88U

// movers: cars and logs ride one wrapping track per lane, drawn from the lane's top edge
// an 8x16 sprite parked there covers exactly the lane, so one scanline still sees one lane
// two movers share a lane's speed half a lap apart, so their gap never closes
#define kMoversPerLane 2U
// a car's lap is the whole 256 px an 8.8 position wraps through for free
#define kCarTrackPx 256U
#define kCarPhase 0x8000U
// water's lap is shorter, so the worst wait for a log at one column drops by a third
#define kWaterTrackPx 192U
#define kWaterTrackFixed 0xC000U
#define kWaterPhase 0x6000U
// 0x10000 minus the lap: what a step below zero overshoots by when the 16 bit position wraps instead
#define kWaterWrapSlack 0x4000U
// the track doubles as oam x, so a mover slides off either edge before it wraps
#define kMoverDrawLimit 176U
// dmg breaks a sprite tie by smaller x, so a mover on the hud's scanlines can cover the digits
// band 0 is six lanes ahead of the chick and unreachable, so its movers are simply not drawn
// oam y 32 is the band 0/1 seam: a sprite there starts at screen y 16, clear of the digits
// the eagle starts its dive at that same seam, for that same scanline reason
#define kMoverMinOamY 32U
// worst case in nine visible lanes is five water lanes: water chunks cap at 2, grass at 2
#define kMoverFirstSprite 4U
#define kMoverSprites 30U
// 5 water lanes x 6 log parts is the pool exactly, as is 4 water lanes plus one 6 slot train
// 36 of 40 oam with the eagle; 8x16 changed no count, and a mover still spans only its own lane
// so a scanline sees at most 6 movers, and never on the hud's own lines: those carry 3 digits alone
// the swoop hides the digits, leaving 6 + 2 eagle + 1 chick: still inside the dmg's 10

// cars: 16x16 of two 8x16 sprites, its left half then its right
#define kCarTileId 0xB0U
#define kCarTileCount 4U
#define kCarSprites 2U
#define kCarHalfPx 8U
// five speeds an eighth of a pixel apart, off whichever minimum the ramp tier gives
#define kCarSpeedStep 32U
#define kCarSpeedSteps 5U
// centers closer than this collide: 8 px of car plus 4 px of chick, minus a little mercy
#define kCarHitPx 10

// logs: 24x16 of three 8x16 sprites, one tile pair each, left end to right end
#define kLogTileId 0xB4U
#define kLogTileCount 6U
#define kLogSprites 3U
#define kLogHalfPx 12U
// four speeds an eighth of a pixel apart, off whichever minimum the ramp tier gives
#define kLogSpeedStep 32U
#define kLogSpeedSteps 4U
// the chick rides while its center is within 12 px of a log's
#define kLogRidePx 12
// a ride snaps the chick onto the log's own 8 px sprite grid, so their oam x match exactly
// dmg breaks an oam x tie by index and the chick is sprite 0, so it draws over its log
#define kLogSnapPx 8
// 8.8 left x 152, so the chick's center reaches 156: the last px a ride survives
#define kRideMaxFixed 0x9800U

// difficulty ramp: four tiers keyed by the lane being generated, six lanes ahead of the chick
#define kRampTiers 4U
// first lane of each tier
#define kRampLaneList {0U, 12U, 30U, 60U}
// 8.8 px per frame, slowest car of a tier; four steps up from it reach 1.0, 1.25, 1.5, 1.75
#define kRampCarMinList {128U, 192U, 256U, 320U}
// 8.8 px per frame, slowest log of a tier; three steps up from it reach 0.78, 0.87, 0.97, 0.97
#define kRampLogMinList {104U, 128U, 152U, 152U}
// trees a grass lane may carry; the span rule still leaves the guaranteed path open
#define kRampTreesList {4U, 4U, 5U, 5U}

// train tracks: a danger chunk may roll a single track lane once the run has some distance on it
#define kTrackFirstLane 15U
// one danger chunk in three past that lane is a track; the roll only happens there, so early
// worlds keep the exact rng sequence they had before tracks existed
#define kTrackOdds 3U
#define kTrackChunkLanes 1U
// the warning light is one 8x8 cell, a tile right of the lane's center and clear of the spawn column
#define kTrackWarnCol 10U

// the per lane phase machine, seeded from the lane rng so two tracks are never in step
#define kTrackQuiet 0U
#define kTrackWarn 1U
#define kTrackTrain 2U
#define kTrackQuietMin 180U
#define kTrackQuietSpan 241U // 180..420 frames of calm
#define kTrackWarnFrames 60U
// the light swaps art every quarter of the warning, so it blinks four times before the train
#define kTrackBlinkFrames 15U
// the second ding lands halfway through the warning
#define kTrackBellGap 30U

// the train: 1.6 screens of it, drawn from four tile pairs at 0xc0
#define kTrainTileId 0xC0U
#define kTrainTileCount 8U
// a whole train of sprites is impossible: dmg draws ten a scanline, so only its two ends are oam
// head 4 plus tail 2 is the same six pool slots a water lane's two logs take, so pool math is unchanged
// worst window is 4 water lanes (24) plus one track (6) = 30, the pool exactly
// a scanline sees head 4 or tail 2; a top lane track parks its ends, so the digits keep their line
#define kTrainSprites 6U
#define kTrainHeadSprites 4U
#define kTrainTailSprites 2U
#define kTrainHeadPx 32
#define kTrainTailPx 16
// the whole block, ends included; the 208 px between them are bg tiles
#define kTrainSpanPx 256
// 5 px a frame from just off the right edge walks all 416 px of travel past the left one in 84 frames
#define kTrainSpeedPx 5
#define kTrainStartX 160
// a position the sweep never takes, so one value means "no train on this lane"
#define kTrainOffX 512
#define kScreenWidthPx 160
// both column cursors start past the rightmost column and walk down to zero as the block passes
#define kTrainColIdle kScreenCols

// the camera advances a lane of its own every four seconds; any hop driven advance restarts it
#define kCreepFrames 240U

// the score chimes every tenth lane
#define kScoreChime 10U

// the eagle: ten seconds without reaching a new lane and the run is over
#define kEagleIdleFrames 600U
// one 16x16 bird of two 8x16 sprites in tile id order
#define kEagleTileId 0xBCU
#define kEagleTileCount 4U
#define kEagleSprites 2U
#define kEagleHalfPx 8U
// two fixed slots past the mover pool: 36 of 40 oam in the worst case
#define kEagleFirstSprite 34U
// the swoop starts just under the hud band, so its two sprites never join the digits' scanlines
#define kEagleStartY 16U
#define kEagleDivePx 4U

// game over popup: a band of bg cells over the frozen world, no window layer involved
#define kPopupTopRow 4U // rows 4..13 of 18 give the four text lines a blank row of air each
#define kPopupRows 10U
// the hover banner is the same machinery over a live world: rows 1..5, digits on the last
#define kBannerTopRow 1U
#define kBannerRows 5U
#define kBannerTitleRow 1U
#define kBannerPromptRow 2U
#define kBannerBestRow 3U
// hover parks the camera on lane 0, so screen rows 0..5 are lanes 6, 5 and 4
#define kBannerLaneLo 4U
#define kBannerLanes 3U
// scx is always zero here, so the band is exactly the visible columns wide
#define kPopupCols 20U
#define kPopupRowsPerFrame 2U // 40 cells is a comfortable vblank budget
#define kPopupOverRow 1U
#define kPopupScoreRow 3U
#define kPopupBestRow 5U
#define kPopupPromptRow 8U
// dead input after a death so a panic hop cannot skip the popup
#define kOverLockoutFrames 20U

// battery sram, mbc1 bank 0: 4 magic bytes then the best score little endian
#define kSramBase 0xA000U
#define kSaveMagic0 'C'
#define kSaveMagic1 'R'
#define kSaveMagic2 'S'
#define kSaveMagic3 'Y'
#define kSaveBestOffset 4U

#endif
