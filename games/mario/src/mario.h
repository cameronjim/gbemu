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
// m8a's four. a thin platform is solid only to feet coming down onto it, lava is scenery painted
// over the death plane, and the bridge and axe are what a castle ends with
#define kBlockThin 14U
#define kBlockLava 15U
#define kBlockBridge 16U
#define kBlockAxe 17U
#define kBlockKindCount 18U
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
// pipe family 0xb0..0xb7; m8a spends the four the family had left on 1-3's thin platform deck and
// on a castle's lava, which needs a surface tile and a fill tile like the ground does
#define kTilePipeTl 0xB0U
#define kTilePipeTr 0xB1U
#define kTilePipeBodyL 0xB2U
#define kTilePipeBodyR 0xB3U
#define kTileThin 0xB4U
#define kTileThinUnder 0xB5U
#define kTileLavaTop 0xB6U
#define kTileLavaFill 0xB7U
// flag/castle family 0xb8..0xbb, now full: the bridge deck and the axe take its last two
#define kTileFlagPole 0xB8U
#define kTileCastle 0xB9U
#define kTileBridge 0xBAU
#define kTileAxe 0xBBU
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
// small parks the lower row) + one item 2 + one coin pop 1 + two fireballs + five enemies x 2 is 19
// of the 40 slots; the per-scanline math is the enemy pool's problem, see kEnemyRowCap below
#define kSpriteMarioL 0U
#define kSpriteMarioR 1U
#define kSpriteMarioLowL 2U
#define kSpriteMarioLowR 3U
#define kSpriteItemL 4U
#define kSpriteItemR 5U
#define kSpriteCoin 6U
#define kSpriteFireFirst 7U
#define kSpriteEnemyFirst 9U
// m8a's three: a firebar's flames, the fake bowser, and up to two lift decks four sprites wide.
// 19 + 8 + 2 + 8 = 37 of the 40 slots. the per-scanline worst case is documented at kFirebarSlots
#define kSpriteFlameFirst 19U
#define kSpriteBowser 27U
#define kSpriteLiftFirst 29U
#define kPalMario 0U
#define kPalMushroom 1U
#define kPalStar 2U
#define kPalOneup 3U
#define kPalCoin 4U
#define kPalGoomba 5U
#define kPalKoopa 6U
// the last free slot, spent on fire mario's white-and-red outfit. the flower and the fireball have
// none left, so both borrow the star's white/yellow set
#define kPalFire 7U

// gbdk's move_sprite takes oam coordinates; the visible screen starts at (8, 16)
#define kOamXOffset 8U
#define kOamYOffset 16U

// the player's sprite box; the art keeps one transparent column inside each vertical edge
#define kPlayerWidthPx 16
#define kPlayerHeightPx 16
// super/fire mario is twice as tall, and crouching folds the body to 24 px by dropping its top edge
// eight px while the feet stay put. must-measure: smb's crouch box was not read off the disassembly
#define kPlayerBigHeightPx 32
#define kCrouchInsetPx 8
#define kPlayerCrouchHeightPx (kPlayerBigHeightPx - kCrouchInsetPx) // 24
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

// the compiled kinds, the contract with compile_level.py's ENEMY_KIND_MAP. roster.json: the red
// koopa turns at a ledge where the green one walks off, and the piranha lives in a pipe
#define kEnemyGoomba 0U
#define kEnemyKoopa 1U
#define kEnemyKoopaRed 2U
#define kEnemyPiranha 3U

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

// smb's object loader brings an enemy in as its column reaches the screen's right edge, and forgets
// it for good once it has scrolled this far past the left one - the cursor only ever advances, so
// walking back over ground already crossed never brings one of them back
#define kEnemySpawnMarginPx 0
#define kEnemyDespawnMarginPx 32

// our own cadences: the bible times neither the flattened goomba nor the enemy fall rate
#define kSquashFrames 30U
#define kEnemyGravitySubpx 24U
#define kEnemyMaxFallPx 4
#define kEnemyAnimFrames 8U
// the piranha's cycle: a whole block up, a bite, a whole block back down, then a wait. the bible
// times none of it, so all four counts are ours
#define kPlantRisePx 16
#define kPlantHoldFrames 60U
#define kPlantCycleFrames (2U * kPlantRisePx + 2U * kPlantHoldFrames) // 152
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

// lifts (games/mario/src/hazards.c). physics.json platform_lift_speeds is must-measure: the
// disassembly has a routine per lift type but the bible extracted no constant from any of them,
// so 1 px a frame either way is ours, picked to read like smb's own unhurried decks
#define kLiftSpeedPx 1
#define kLiftSlots 2U
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
// two bars can be on screen at once, six flames each, but only eight flame sprites exist: the pool
// draws the nearest bar's flames first. sprites are 8x16, so 8 px apart puts two of a vertical
// bar's flames on any one scanline - plus mario's four and a lift's four, exactly the ten the
// hardware draws per line
#define kFirebarSlots 2U
#define kFlameSlots 8U

// the fake bowser. roster.json calls him "a Little Goomba in disguise" and gives no speed, so he
// patrols at the goomba's own half a pixel a frame; his fire breath is m9's
#define kBowserWidthPx 16
#define kBowserHeightPx 16
#define kBowserSpeedSubpx 8U

// how far outside the view a lift, bar or bowser has to be before the game loop stops calling
// into bank 5 for it at all. smb runs its objects only while they are near the screen too, and
// on 1-2 that is the whole level bar the last forty columns
#define kHazardMarginPx 64U

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

// the title's level select: up and down step through world one before start begins it. without it
// a test that wants 1-4 has to clear the three levels ahead of it first, which is minutes of
// emulation for one probe. define this to 0 to drop it
#ifndef kLevelSelect
#define kLevelSelect 1
#endif

#endif
