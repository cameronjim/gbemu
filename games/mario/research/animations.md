# animation audit: smb1/smbd animations in 1-1 to 1-4

what moves on screen in the four levels the rom ships, what the smb1 disassembly says each one does
frame by frame, and how close `games/mario/src` comes. the first pr of the animation issue; each
follow-up closes one row.

- source: doppelganger's `SMBDIS.ASM`, the same raw gist blob physics.md cites; line numbers refer to
  it. smbd is a port of that code and no smbd-specific frame data is sourced anywhere, so a row
  marked **documented** is smb1's count, and a thing smbd added (the lava wave, the map screen)
  is **must-measure**.
- status: **present** = the same frame count and shape as the disassembly; **approximate** =
  the beat exists but its timing, shape or frame set is ours; **missing** = nothing on screen.
- oam is 40/40 during play (see VRAM.md "oam: the forty sprite slots"), so a row that needs new
  sprites names the slots it would borrow. bank 4 is full; new tile art rides in bank 6 behind a
  banked loader, the way castle_art.c does.

## 1. mario

| animation | smb1 (smbdis.asm) | ours | status |
|---|---|---|---|
| walk / run cycle | three poses, each held for a timer picked by speed: `PlayerAnimTmrData` `02 04 07` (6192), chosen at `GetPlayerAnimSpeed` (6195-6224): abs xspeed >= 0x1c -> 2 frames a pose, >= 0x0e -> 4, else 7; the cycle itself at `AnimationControl` (14660-14676), extent 3 | `player.c` `step_anim`: `anim_accum += speed_abs`, pose steps every `kWalkAnimStepSubpx` (48) subpixels - at the run cap (40 subpx) that is a pose every 1.2 frames, at the walk cap (24) every 2, which is why his arms blur at a run | approximate -> fixed in this pr: `PlayerAnimTmrData` verbatim |
| skid | skid pose while left/right is held against `Player_MovingDir` and abs xspeed >= 0x0b (`ProcSkid`, 6210-6218) | `kFrameSkid` while `skidding` | present |
| jump | one pose for the whole air time | `kFrameJump` while `on_ground == 0` | present |
| crouch (big) | one pose, no walk while ducking | `kFrameCrouch`; we let him crawl (mario.h `kMarioCrouchWalkSubpx`, a deliberate departure for 1-2) | present |
| climb / pole grip | two poses alternating on the same timer, extent 2 (`ThreeFrameExtent`, 14655) | one grip pose, small (`kTileClimbSmall`) and big (`kFrameClimbBig`) | approximate - needs the second grip frame (4 + 8 sprite tiles) |
| fireball throw pose | the throw holds its own pose for `PlayerAnimTimerSet` frames (`FireballThrowingTimer`, 6296-6302) | none: the ball leaves and his pose does not change | missing - needs one big pose (8 tiles); bank-1 sprite 0x52-0x73 is free |
| grow | 10 steps every 4th frame through `ChangeSizeOffsetAdder` `0 1 0 1 0 1 2 0 1 2` (14689-14710): small, big and a middle size; the world freezes from `TimerControl` 0xf8 to 0xc4, 52 frames (`PlayerChangeSize`, 5732-5740) | `powerup.c`: `kGrowFrames` 64, small/big alternating every `kGrowFlipFrames` 8, no middle size | approximate - the middle frame is 8 more tiles |
| shrink | the same table read from +10: `2 0 2 0 ...`, big and middle | the grow's alternation run backwards | approximate |
| injury blink | frozen 16 frames (0xf0 down), then blinks to 0xc8 (`PlayerInjuryBlink`, 5744-5751); the invulnerability after is physics.md 3.6 | `injury_timer` flicker off one shared phase counter | approximate |
| star flash | palette cycles every frame of the timer (`CyclePlayerPalette`, 3.5 in physics.md) | `kStarFlashMask` phase | approximate |
| death | `KillPlayer` (11427): x speed 0, y speed -4, then gravity; the hold is `TimerControl`, whose start value physics.json 5.3 could not locate | `player.c`: `kDeathHoldFrames` 24, `kDeathLaunchPx` -5 | approximate, must-measure |
| flag slide | he is forced to climb down (`FlagpoleSlide`, 5810-5824) until y >= 0x9e; the flag moves 1 px + the carry of a 0xff adder, i.e. 2 px every frame but one in 256 (`FlagpoleRoutine`, 6590-6600), and the slide ends when either reaches its stop (flag 0xaa, player 0xa2) | `kClearSlidePx` 2 px a frame; the pennant was bg cells repainted a row every `kClearFlagStepFrames` 6 - 2.67 px a frame, in 16 px jumps | approximate -> fixed in this pr: the pennant is two sprites moving 2 px a frame beside him |
| flip to the far side, hop, walk in | `PlayerEndLevel` (5830-5870): walk right at 1, into the castle when x reaches the door | `kClearFlip*`, `kClearHop*`, `kClearWalkPx` 1 - our own cadence, noted in mario.h | approximate |

## 2. blocks, coins and items

| animation | smb1 | ours | status |
|---|---|---|---|
| block bounce | `BumpBlock` (7307) y speed -2, `ImposeGravityBlock` (7660) adds 0x50/256 a frame, cap 8; killed when the y low nybble drops under 5 on the way back (`BouncingBlockHandler`, 7480-7490). the arc is `-2 -4 -6 -7 -7 -8 -8 -8 -7 -6 -5 -4 -2 0`: 14 frames, apex 8 px | `blocks.c` `start_bump`: the cell's face repainted one tile row up for `kBumpFrames` 8, then put back | approximate -> fixed in this pr: 11 frames, the span of smb1's arc that rounds to the raised row |
| coin out of a block | `SetupJumpCoin`/`JCoinC` (6975-6998): y speed -5, gravity 0x50, cap 6, becomes a "200" once the speed reaches +5 (`JCoinRun`, 7046-7060): 32 frames, apex 43 px at frame 15; four spin tiles alternating every 2 frames (`JCoinGfxHandler`, 13438-13460) | `kCoinPopFrames` 30, `kCoinPopRisePx` 2 a frame up then down (a 30 px triangle), one static coin tile | approximate -> arc fixed in this pr: smb1's speed, gravity and cap. the spin is 3 more 8x16 tiles; bank-0 sprite 0x78-0x7f is free |
| the "200" after the coin | drawn in the coin's own slot from state 2 to 0x30, rising 1 px every other frame (`DrawFloateyNumber_Coin`, 13425-13436) | none | missing -> added in this pr as a score popup |
| powerup emergence | 1 px every 4 frames for 16 px (`GrowThePowerUp`, 7181-7197) | `kItemRiseFramesPerPx` 4, `kItemRisePx` 16 | present |
| flower / star palette cycle | attribute rotates every 2 frames (`DrawPowerUp`, 13520-13528) | none: one palette each | missing (palette only, no art) |
| brick shatter | four chunks, x speed +-1 px, y speeds -6 and -4, gravity 0x50, cap 8 (`SpawnBrickChunks`, 7416-7440), gone past the screen bottom | `debris.c` `kFragDx` +-2/+-1, `kFragDy` -5/-3, `kDebrisGravitySubpx` 128, `kDebrisFrames` 60, a spin every 4 frames | approximate |
| question block / coin glint | bg palette 3 entry 1 rotates through `ColorRotatePalette` `27 27 27 17 07 17` every 8 frames (`ColorRotation`, 1958-1990): a 48-frame cycle | none | missing (one bg palette write every 8 frames) |
| used block | instant swap | instant swap | present |
| hidden block | appears on the bump, then bounces | same | present |

## 3. enemies

| animation | smb1 | ours | status |
|---|---|---|---|
| goomba walk | mirrored every 8 frames (`GmbaAnim`, 13697-13706: `FrameCounter & 8`) | `kEnemyAnimFrames` 8 off the pool's shared `anim` | present |
| goomba flatten | `SetStun` (11398) drops it 2 px into the pancake; erased when its interval timer reaches 0x0e (`ChkKillGoomba`, 9360) - interval timers tick every 21 frames (physics.md 0), so roughly 20-40 frames | `kSquashFrames` 30 | approximate |
| koopa / paratroopa walk and fly | two frames alternating every 8 frames (13701) | same, `kTileKoopaWalk0/1`, `kTileParaFly0/1` | present |
| shell idle -> legs -> revive | interval timer 0x10 (`RevivalRateData`, 11361), legs drawn as the timer runs down | `kShellWakeFrames` 600 (roster.json), no legs frame | approximate - the legs are 2 tiles |
| kicked shell | no spin in smb1: the shell tile is static while it travels | static | present (the issue's "shell spin" has no source) |
| paratroopa -> koopa | demoted in place, 400 points (`ChkForDemoteKoopa`, 11480) | same, `kEnemyHitStomp` | present |
| fireball / shell kill flip | body flips vertically and falls (`CheckDefeatedState`, 13868) | `kEnemyFlipped` corpse | present |
| piranha | 1 px every other frame (`RiseFallPiranhaPlant`, 10600), 64-frame hold at each end (`EnemyFrameTimer` 0x40, 10629) | `kPlantHoldFrames` 60 | approximate |
| fireball spin | tile alternates every 4 frames and flips every 8 | same (`powerup.c`, `spin_bank`/`spin_flip`) | present |
| fireball puff | three tiles, two frames each: 6 frames (`DrawExplosion_Fireball`, 14258-14265) | `kPuffFrames` 8, one tile | approximate |
| firebar | physics.md 4.7 | present | present |
| bowser walk / jaw | two body frames off `BowserBodyControls` | `kBowserAnimFrames` 16, `kBowserJawOpenFrames` 30 | approximate |

## 4. level clear

| animation | smb1 | ours | status |
|---|---|---|---|
| score popups | `SetupFloateyNumber` (11533): timer 0x30, rising 1 px a frame (`FloateyPart`, 1325-1330) - 48 frames, 48 px; two 8x8 tiles from `FloateyNumTileData` (1262-1274): 100 200 400 500 800 1000 2000 4000 5000 8000 1-UP | none | missing -> added in this pr (`popup.c`, bank 6, oam 4-5) |
| pole score number | the band's number rises beside the flag as it comes down (`FlagpoleGfxHandler`, 13282) | none | missing -> the popup, at the grab point |
| flag score bands | `FlagpoleYPosData` `18 22 50 68 90` against the player's y (12150, 12187-12192); `FlagpoleScoreMods`/`Digits` (6573-6577) | `flow_score_flag` split the shaft evenly into five | approximate -> fixed in this pr |
| castle flag | rises 1 px a frame to y 0x72 (`RaiseFlagSetoffFWorks`, 10518), four tiles | none | missing - 4 sprite tiles, and 4 oam slots the clear can borrow from the enemy pool |
| fireworks | 1, 3 or 6 by the timer's last digit; each is 3 tiles x 8 frames and 500 points (`RunFireworks`, 10413-10430) | none | missing |
| time bonus | one tick a frame, the sound every 4 (`AwardGameTimerPoints`, 10487) | `hud_spend_time_bonus` | present |

## 5. smbd-only

| animation | ours | status |
|---|---|---|
| lava wave | one surface tile, static | must-measure (no smb1 source) |
| map screen walk | `kMapWalkAnimFrames` 8 | must-measure (see the map issue) |
