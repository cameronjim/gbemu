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
#define kBlockSpent 12U
#define kBlockCoin 13U
#define kBlockKindCount 14U
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

// bg tile ids for the level; families pinned per claude-docs/milestones/18-smbd.md's test contract.
// the exact per-tile map is recorded in games/mario/src/assets.h.
#define kTileSky kFontFirstTile // 0x00: the font's blank space glyph, same trick as flappy/crossy
// ground family 0xa0..0xa3: hard/stair share the family's single stone tile
#define kTileGroundTop 0xA0U
#define kTileGroundFill 0xA1U
#define kTileHard 0xA2U
// brick family 0xa4..
#define kTileBrick 0xA4U
// question family 0xa8..0xaf: the lit block's 2x2 face, then the spent block's
#define kTileQuestionTl 0xA8U
#define kTileQuestionTr 0xA9U
#define kTileQuestionBl 0xAAU
#define kTileQuestionBr 0xABU
#define kTileSpentTl 0xACU
#define kTileSpentTr 0xADU
#define kTileSpentBl 0xAEU
#define kTileSpentBr 0xAFU
// pipe family 0xb0..0xb3 (0xb4-0xb7 reserved for a future spent/enterable variant)
#define kTilePipeTl 0xB0U
#define kTilePipeTr 0xB1U
#define kTilePipeBodyL 0xB2U
#define kTilePipeBodyR 0xB3U
// flag/castle family 0xb8..0xbb
#define kTileFlagPole 0xB8U
#define kTileCastle 0xB9U
// coins-in-world family 0xbc..0xbf: one 16x16 coin's four quadrants
#define kTileCoinTl 0xBCU
#define kTileCoinTr 0xBDU
#define kTileCoinBl 0xBEU
#define kTileCoinBr 0xBFU

// cgb bg palette slots for the terrain: one per pinned tile family, plus a neutral one for
// flag/castle. all eight cgb bg palettes are spoken for now
#define kCamPalSky 0U
#define kCamPalGround 1U
#define kCamPalBrick 2U
#define kCamPalQuestion 3U
#define kCamPalPipe 4U
#define kCamPalNeutral 5U
#define kCamPalSpent 6U
#define kCamPalCoin 7U

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

// item sprite family 0xd0.. per the milestone's tile-id contract: three 16x16 items stored the same
// way mario's frames are (left top/bottom then right top/bottom), then the 8x16 coin pop
#define kTileItemFirst 0xD0U
#define kItemTilesPerKind 4U
#define kTileCoinPop 0xDCU
#define kItemTileCount 14U // 0xd0-0xdd

// the emerging item's kind, which also indexes its art and palette
#define kItemNone 0U
#define kItemMushroom 1U
#define kItemStar 2U
#define kItemOneup 3U

// oam slots and the cgb sprite palettes. worst case on screen is mario 2 + one item 2 + one coin
// pop 1 = 5 sprites, and mario and an item never share more than two of them on a scanline, so the
// hardware's 10-per-line limit is never approached
#define kSpriteMarioL 0U
#define kSpriteMarioR 1U
#define kSpriteItemL 2U
#define kSpriteItemR 3U
#define kSpriteCoin 4U
#define kPalMario 0U
#define kPalMushroom 1U
#define kPalStar 2U
#define kPalOneup 3U
#define kPalCoin 4U

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

// the underground sub-area's palette set: the same tile families, tinted dark and blue
#define kAreaMain 0U
#define kAreaBonus 1U

// the m2 debug camera (no player, free d-pad scroll) still ships, entered with b from the title now
// that select is the play camera's look-ahead. define this to 0 to drop the state from the rom
#ifndef kDebugCamera
#define kDebugCamera 1
#endif

#endif
