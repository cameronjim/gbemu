#ifndef MARIO_H
#define MARIO_H

// tuning constants live here so logic files never carry magic numbers

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleRow 6U
#define kPromptRow 10U

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
#define kBlockKindCount 12U

// bg tile ids for the level; families pinned per claude-docs/milestones/18-smbd.md's test contract.
// the exact per-tile map is recorded in games/mario/src/assets.h.
#define kTileSky kFontFirstTile // 0x00: the font's blank space glyph, same trick as flappy/crossy
// ground family 0xa0..0xa3: hard/stair share the family's single stone tile
#define kTileGroundTop 0xA0U
#define kTileGroundFill 0xA1U
#define kTileHard 0xA2U
// brick family 0xa4..
#define kTileBrick 0xA4U
// question family 0xa8..0xab: four unique tiles for the lit block's 2x2 face
#define kTileQuestionTl 0xA8U
#define kTileQuestionTr 0xA9U
#define kTileQuestionBl 0xAAU
#define kTileQuestionBr 0xABU
// pipe family 0xb0..0xb3 (0xb4-0xb7 reserved for a future spent/enterable variant)
#define kTilePipeTl 0xB0U
#define kTilePipeTr 0xB1U
#define kTilePipeBodyL 0xB2U
#define kTilePipeBodyR 0xB3U
// flag/castle family 0xb8..
#define kTileFlagPole 0xB8U
#define kTileCastle 0xB9U

// cgb bg palette slots for the terrain: one per pinned tile family, plus a neutral one for flag/castle
#define kCamPalSky 0U
#define kCamPalGround 1U
#define kCamPalBrick 2U
#define kCamPalQuestion 3U
#define kCamPalPipe 4U
#define kCamPalNeutral 5U

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

// oam slots and the cgb sprite palette small mario draws with
#define kSpriteMarioL 0U
#define kSpriteMarioR 1U
#define kPalMario 0U

// gbdk's move_sprite takes oam coordinates; the visible screen starts at (8, 16)
#define kOamXOffset 8U
#define kOamYOffset 16U

// the player's sprite box; the art keeps one transparent column inside each vertical edge
#define kPlayerWidthPx 16
#define kPlayerHeightPx 16
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
// vertical: the default band is the window that leaves mario's feet this far above its bottom edge,
// sampled only while he is supported, so a jump never pans the view. must-measure: 32 px is tuned
#define kCamGroundOffsetPx 32
// both the manual pan and the drift back to the default band move scy this much per frame
#define kCamPanStepPx 2
// scy on the level's flat opening ground, which is where the default band lands there
#define kPlayScy kScyMax

// the level-clear sequence, all our own cadence (smbd's exact frame counts are unsourced)
#define kClearSlidePx 2
#define kClearHopFrames 12
#define kClearHopPx 2
#define kClearWalkPx 1
#define kClearWalkAnimFrames 8U
// 1-1's bible places no castle_end, so the walk off the pole ends this many blocks along the
// closing ground, where a castle would stand. must-measure once the data names a castle column
#define kClearWalkBlocks 5
#define kClearHoldFrames 120U

// the m2 debug camera (no player, free d-pad scroll) still ships, entered with b from the title now
// that select is the play camera's look-ahead. define this to 0 to drop the state from the rom
#ifndef kDebugCamera
#define kDebugCamera 1
#endif

#endif
