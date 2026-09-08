# vram tile id map

purpose: a checked-in ledger of every tile id this game's art claims, per vram bank, per address
space (bg vs sprite) and per screen — so two people working art in parallel do not pick the same
id for two different things. seeded only from the `#define`s and comments in
`games/mario/src/mario.h` and `assets.h`, and from the loader calls that actually run in
`assets_data.c`, `hud.c`, `toad.c`, `mapscreen.c`, `hazards.c`, `enemies.c`, `player.c`, `title.c`,
`title_art.c`, `file_art.c`, `map_art.c` and `states.c`. nothing here is invented; every range below is a real `#define` or a
real `set_bkg_data`/`set_sprite_data` call, and every gap called FREE was checked against both.

## the addressing rule every table below assumes

lcdc bit 4 is clear (gbdk's default), so:

- a bg tile id 0x00-0x7f reads out of 0x9000-0x97ff (signed indexing from 0x9000).
- a bg tile id 0x80-0xff reads out of 0x8800-0x8fff.
- a sprite tile id is always unsigned off 0x8000, so a sprite id 0x80-0xff *also* reads
  0x8800-0x8fff.

consequence: in a given vram bank, a bg id and a sprite id of the same value >= 0x80 are the same
bytes. that is the collision every "bg id X shares bytes with sprite id X" note below is about, and
it is why the hud font (bg, bank 1) and a chunk of bank-1 sprite ids (0x80-0x8c) can never both be
loaded with different art. bg ids under 0x80 have no such conflict — bank 0's font (0x00-0x5f) and
super mario's sprite frames (0x60-0x7f) sit in disjoint address windows even though both are under
0x80.

a cgb bg map attribute byte picks a cell's vram bank (`kCamAttrVram1`), so "bank 1 bg" tiles are
real background art, not a spare copy — see `assets_load_scenery_tiles` and `map_art_load`.

---

## bank 0, background tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x00-0x5f | gbdk ibm font, ascii 0x20-0x7f (`kFontFirstTile`-`kFontLastTile`) | `font_init()`/`font_load(font_ibm)`, `main()` | every screen | loaded exactly once at boot, never reloaded; `kTileSky` (0x00) is the font's own space glyph, reused as "blank" everywhere |
| 0x60-0x9f | — | — | — | unclaimed as bg; would share bytes with bank-0 sprite ids in the same sub-range (the green koopa, small mario's death and climb poses, the flower, the hazards, the debris — see the sprite table below), so nothing loads bg data here |
| 0xa0-0xbf | pinned terrain block: ground/brick/question/spent/pipe/coin quadrants (`kTileGroundTopL`..`kTileCoinBr`) | `assets_load_bg_tiles` | level play (all three types), world map | exactly full per mario.h, and now genuinely full: m23's rip pass took the pipe from 9 tiles to 12 (`kTilePipeLipRr`, `kTilePipeLipRbr`, `kTilePipeBodyRr` at 0xb9-0xbb, which were this block's last free ids), because the smbd capture's pipe is neither left-right mirrored nor shares a tile column between its two cells. a castle load overwrites the ground family's 4 upper ids (0xa0-0xa3) in place via `assets_load_bg_tiles_castle`, and an underground load overwrites the brick's upper pair (0xa4-0xa5) via `assets_load_bg_tiles_underground` — no new ids either way, just different pixels under the same ids |
| 0xc0-0xf7 | — | — | — | unclaimed as bg; shares bytes with bank-0 sprite families (enemy 0xc0-0xc7, item 0xd0-0xdf, small mario 0xe0-0xf7) |
| 0xf8-0xff | ground fill (lower half) + hard block + thin platform (`kTileGroundFillBl`..`kTileThinUnder`) | `assets_load_bg_tiles` | level play, world map | butts directly against the mario sprite family ending at 0xf7 — no gap, no overlap |

## bank 0, sprite tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x60-0x6f | green koopa: two 16x24 walk frames, each bottom-aligned in a 16x32 box, 8 tiles a frame (`kTileKoopaWalk0`/`kTileKoopaWalk1`, `gen/koopa_green.c`) | `assets_load_enemy_tiles` | level play | m22's exact art made the koopa taller than 16x16, so it left the pinned 0xc0 family and took the run big mario gave back when he moved to bank 1 |
| 0x70-0x73 | small mario's death pose, facing the viewer with both hands up (`kTileMarioDeath`, `gen/mario_small.c`'s seventh frame) | `assets_load_sprite_tiles` | level play (the death beat) | drawn only by `player_draw`'s dying branch, and only ever small: a big mario shrinks before he can die |
| 0x74-0x77 | **FREE** (4 ids) | — | — | small mario's climb grip has the same four ids in **bank 1**; nothing claims them here |
| 0x78-0x7f | **FREE** (8 ids) | — | — | the tail of what big mario's old 0x60-0x7f block gave back |
| 0x80-0x83 | fire flower (`kTileFlowerFirst`, `gen/flower.c`, 4 tiles) | `assets_load_item_tiles` | level play | wears `kPalOneup` now — the smbd flower's white/yellow/green is the 1-up's index order, not the star's |
| 0x84-0x87 | piranha plant: a 16x24 box's left column, top pair then bottom pair (`kTilePiranha`, `gen/piranha.c`) | `assets_load_hazard_tiles` | level play | left-right symmetric, so only the left half is stored and the right is the same pair drawn `S_FLIPX`; four slots of oam, two rows of two |
| 0x88-0x8b | firebar flame + lift deck plank, one 8x16 pair each with a blank lower tile (`kTileFlame`, `kTileLiftDeck`, hand art in `assets_data.c`) | `assets_load_hazard_tiles` | level play | both moved up two ids when the piranha grew from one pair to two |
| 0x8c-0x91 | throwaway anims: brick debris + fireball puffs (`kTileDebris`, `kTilePuffA`, `kTilePuffB`, 6 tiles) | `assets_load_item_tiles` | level play | |
| 0x92-0x97 | world-map node markers: three 8x16 sprites, the red body, the blue one and the rim they share (`kTileMapMarkerRed`.., 6 tiles) | `map_art_load` | world map only | rings the three path nodes only - the reference leaves its castle bare. a marker is a solid 8x8 in four colors, one more than an obj palette's three opaque slots, so a body rides over a rim carrying only its orange edge |
| 0x98-0x9f | **FREE** (8 ids) | — | — | what is left of the run mario.h calls unclaimed |
| 0xa0-0xbf | — | — | — | belongs to bg's pinned terrain block; nothing sprite-side claims it |
| 0xc0-0xc7 | enemy family: goomba walk0 as a full 16x16 box (`kTileGoombaWalk0`, `gen/goomba.c`, 4 tiles), squashed goomba (`kTileGoombaSquash`, `gen/goomba_squash.c`), green shell (`kTileShell`, `gen/shell_green.c`) | `assets_load_enemy_tiles` | level play | the goomba's walk1 is walk0's exact mirror in the smbd rip, so it costs no ids: `enemies_draw` swaps the halves and sets `S_FLIPX`. the pancake and the shell are symmetric, one pair each |
| 0xc8-0xcf | **FREE** (8 ids) | — | — | held the koopa while it was 16x16; freed when m22 moved it to 0x60 |
| 0xd0-0xdf | item family: mushroom/star/1-up (16x16 each, `gen/items.c`), coin pop (hand art), fireball frame A (`gen/fireball.c`'s first pair) — `kTileItemFirst`..`kTileFireball`, 16 tiles | `assets_load_item_tiles` | level play | exactly full; `kTileFireball` (0xde) is also loaded in **bank 1** as the projectile's second spin frame (see below). the star's tiles are drawn in the mushroom's index order, so it wears `kPalMushroom` |
| 0xe0-0xf7 | small mario, the six pinned 16x16 poses — idle, walk0, walk1, walk2, skid, jump (`kTileMarioFirst`, `gen/mario_small.c`, 24 tiles) | `assets_load_sprite_tiles` | level play, world map | the seventh frame of the generated run, the death pose, goes to 0x70 above |
| 0xf8-0xff | — | — | — | belongs to bg's extra terrain ids |

---

## bank 1, background tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x00-0x5d | the whole 20x18 world map frame plus its runtime tile run (see the world map section below) | `map_art_load` | world map only | the title's and the file select's own frames reuse the same low ids while they are up; each screen reloads what it draws |
| 0x0a-0x11 | 1-3's tree canopy + trunk (`kTileTreeFirst`, 8 tiles) | `assets_load_scenery_tiles` | level play (only levels with a `tree` terrain run use it; currently 1-3) | also resident (but unused) on the world map — that screen calls `assets_load_scenery_tiles` too, see per-screen summary |
| 0x12-0x18 | castle masonry courses, axe, bridge, deep-lava fill (`kTileCastleBrickLower`..`kTileLavaDeep`, 7 tiles) | `assets_load_scenery_tiles` | level play (castle type for the masonry/axe/bridge; any level type with a >1-deep lava pit for the lava fill) | |
| 0x19-0x1f | **FREE** (7 ids) | — | — | called out unclaimed in mario.h |
| 0x20-0x5d | scenery run: lava top, castle wall/window/door-frame, flag ball/cloth/pole-adjacent cells, clouds, hills, bushes, pole shaft, inner crenel, blank + ball-right (`kTileSceneryFirst`-`kTileSceneryLast`, exactly full) | `assets_load_scenery_tiles` | level play (whichever pieces a level's type/decor use), world map (loaded, unused) | m23 re-cut every tile in this run from the capture without moving one id. the one thing that did move inside it is the flag ball: its two halves sit at the run's two ends, `kTileFlagBallL` (0x30) and `kTileFlagBallR` (0x5d), so they are one generated family (`gen/flag_ball.c`) written into the two ids by two `set_bkg_data` calls, and `gen/flag_head.c` is now the four pennant tiles at 0x31-0x34 rather than five at 0x30. the ball is also the only flag piece off the sky palette slot — the capture draws it in the pipe's greens over black, which is what `kPaletteRom` pins `kBlockFlagBall` to |
| 0x5e-0x5f | **FREE** (2 ids) | — | — | see correction below — mario.h's own comment near `kTileCastleBrickLower` is wrong about this range |
| 0x72-0x7a | sideways pipe, 9 tiles (`kTilePipeSideTl`-`kTilePipeSideBodyB`) | `assets_load_scenery_tiles` | level play (levels with a `pipe_side` terrain entry; currently 1-2), world map (loaded, unused) | vram bank 0 had no ids left for this, per mario.h |
| 0x7b-0x7f | **FREE** (5 ids) | — | — | between the side-pipe run and the hud font |
| 0x80-0x8c | hud row glyphs: 10 digits, blank, coin icon, the letter x (`kTileHudDigitFirst`-`kTileHudLetterFirst`, 13 tiles) | `assets_load_hud_font` | level play only (the window-layer hud row is drawn only during play) | shares bytes with bank-1 **sprite** ids 0x80-0x8c — see the sprite table |
| 0x8d-0xcf | **FREE** (67 ids) | — | — | mario.h reserves headroom up to 0x94 for the hud run (see note below) but nothing loads past 0x8c; the rest of this span is empty. note that 0x96-0xbd is bowser on the **sprite** side of this bank, and a bank-1 bg id at or above 0x80 shares its bytes with the sprite id of the same number — which is why m23's run below starts at 0xd0 rather than at 0x8d |
| 0xd0-0xdd | m23's rip run: the right cloud cap (8 tiles, `kTileCloudCapRtl`..`kTileCloudCapRfr`), the right bush cap (4, `kTileBushCapRtl`..`kTileBushCapRbr`) and the right half of each castle crenel (`kTileCastleCrenelRight`, `kTileCastleCrenelInnerRight`) | `assets_load_scenery_tiles` | level play (whichever pieces a level's decor uses), world map (loaded, unused) | the scenery run at 0x20-0x5d assumed three of smb's pieces were their left twin drawn with `kCamAttrXFlip`; the smbd capture says only the right hill slope actually is. these fourteen are the ones that are not — the right cloud cap is 28 px from the left one mirrored, the right bush cap 13 px, the two crenel halves 56 px and 23 px apart |
| 0xde-0xeb | **FREE** (14 ids) | — | — | what is left between m23's run and the toad sign glyphs |

m23 added no bg id beyond the fourteen at 0xd0-0xdd. `kBlockCastleWindowRight`, the kind it appended
to the block tables (mario.h), needs none: it is `kBlockCastleWindow`'s own four
tiles with the two columns swapped, because the capture puts the tower's two window openings either
side of its middle column and an `kCamAttrXFlip` of the left cell would move the masonry's mortar
joint.

m24's scenery pass added **no bg id at all**, and 0x8d-0xcf / 0xde-0xeb are still free. it re-cut
the cloud, hill and bush art inside the existing 0x20-0x5d run and at 0xd0-0xdd without moving one
id — the anchors an earlier pass read the cloud family from were one column off and gave all six
cloud kinds the middle cell's art, and the hill peak's anchor was the fill cell under the dome —
and it appended one kind, `kBlockHillCore` (mario.h, `kBlockKindCount` 52). that kind needs no id
either: smb shades a hill's interior with two dark pixels in each cell's upper right and leaves the
middle of the five-wide dome's bottom row flat, so the flat cell is `kTileHillFillTl` (0x4d) in all
four quadrants and only the *kind* is new. a side effect of the re-cut is that several ids in the
cloud and hill runs now hold blank (all-backdrop) or duplicated bytes — smb draws a cloud offset
half a cell, so a cap cell is drawn in one quadrant and sky in the other three, and the hill's
flat tile repeats across the fill and slope cells. the ids stay allocated per family so that the
`kTile*Rom` tables and every test that pins an id keep working.
| 0xec-0xfd | toad-room sign glyphs, one id per distinct character across the three sign lines (`kTileSignFirst`, 18 tiles) | `assets_load_toad_tiles` | toad room only (a castle whose bible names a `toad_x`, entered by touching the axe) | |
| 0xfe-0xff | **FREE** (2 ids) | — | — | |

correction: the comment directly above `kTileCastleBrickLower` in mario.h (around the "m20's
castle run" block) says *"bank-1 bg 0x19-0x1f and 0x5c-0x5f are still unclaimed"*. that was true
when it was written, but `kTileScenBlank` (0x5c) and `kTileFlagBallR` (0x5d) — both defined and
loaded earlier in the same file, at the tail of the scenery run — claim 0x5c-0x5d. only 0x5e-0x5f
are actually free. treat the table above, not that comment, as current.

## bank 1, sprite tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x00-0x3f | super/fire mario: all eight 16x32 poses — idle, walk0, walk1, walk2, skid, jump, crouch, climb — 8 tiles each (`kTileSuperFirst`, `gen/mario_big.c`, 64 tiles) | `assets_load_sprite_tiles` | level play, file select, world map | m22's exact art gives every pose its own upper half (the old set shared one slab across six of them, which is why its jump could not raise an arm), so the set is 1 KB and bank 0 could never hold it. both mario sets stay resident, which is what lets the grow animation alternate them without a vram write. the crouch is pose 6, whose 22-tall fold is bottom-aligned in the same box, and the flagpole grip pose 7 |
| 0x40-0x4f | red paratroopa: two 16x24-in-a-16x32 fly frames, 8 tiles each (`kTileParaFly0`/`kTileParaFly1`, `gen/paratroopa_red.c`) | `assets_load_enemy_tiles` | level play (levels with a `koopa_para_red` enemy; currently 1-3) | the koopa's body under a wing that beats across both columns of the box, in `kPalStar`'s white/orange/red |
| 0x50-0x51 | red shell, one symmetric 8x16 pair (`kTileShellRed`, `gen/shell_red.c`) | `assets_load_enemy_tiles` | level play | what a stomped red koopa or paratroopa leaves; the green koopa's shell is bank 0's 0xc6 |
| 0x52-0x73 | **FREE** (34 ids) | — | — | bg ids below 0x80 do not share bytes with a sprite id (see addressing rule), so bank 1's bg scenery run is no obstacle here |
| 0x74-0x77 | flagpole climb pose, small mario (`kTileClimbSmall`, `gen/mario_small_climb.c`, 4 tiles) | `assets_load_sprite_tiles` | level play (clear sequence) | it used to ride at 0xe0, the id his idle pose holds in bank 0. a framebuffer tile id carries the tile and a sprite bit but not the bank, so a host test could not tell the grip from the idle pose and so could not know which pose's art bounds it was measuring; four ids of its own fixed that |
| 0x78-0x7f | **FREE** (8 ids) | — | — | between the climb grip and the reserved hud-font window |
| 0x80-0x8c | **RESERVED, not loaded as sprite data** — shares bytes with bank-1 **bg** hud font (0x80-0x8c above) | — | level play | a sprite must never use these ids while the hud row is on screen (i.e. ever, during play) |
| 0x8d-0x94 | **RESERVED, unused headroom** | — | — | mario.h's hud-font comment (near `kTileHudDigitFirst`) reserves the whole 0x80-0x94 span as off-limits to a future sprite, even though the actual hud font only reaches 0x8c today; keep new sprite work out of 0x8d-0x94 too unless the hud font grows into it first |
| 0x95 | **FREE** | — | — | the one id between the reserved hud headroom and the bowser run; called out explicitly in mario.h |
| 0x96-0xbd | bowser: two 32x32 body frames + fire-breath dart + open-jaw tell (`kTileBowserFirst`..`kTileBowserJaw`, 40 tiles total) | `assets_load_hazard_tiles` | level play (castle type only) | **correction below** |
| 0xbe-0xbf | **FREE** (2 ids) | — | — | the gap between the jaw tile and the toad run; not called out anywhere in code, found by inspection |
| 0xc0-0xc7 | **FREE** (8 ids) | — | — | held the paratroopa while each fly frame was four tiles; freed when m22's 16x24 art moved the pair to 0x40 |
| 0xc8-0xcf | toad-room retainer sprite, 8 tiles (`kTileToadFirst`) | `assets_load_toad_tiles` | toad room only | |
| 0xd0-0xdd | **RESERVED, not loaded as sprite data** — shares bytes with bank-1 **bg** ids 0xd0-0xdd, m23's rip run (see the bg table) | — | level play | a sprite must never use these ids while a level's scenery is resident, i.e. ever during play |
| 0xde-0xdf | fireball's second spin frame (`kTileFireball`, reusing the id bank 0's item family already owns) | `assets_load_item_tiles` | level play (fire mario) | drawn with `S_BANK` set to alternate with bank 0's frame A at the same id — a deliberate dual-bank reuse, not a collision |
| 0xe0-0xeb | **FREE** (12 ids) | — | — | held small mario's climb grip (0xe0-0xe3, now at 0x74) and big mario's own (0xe4-0xeb, now just pose 7 of the 0x00 set) |
| 0xec-0xff | **FREE** (20 ids) | — | — | the toad sign glyphs at the same numeric ids are bg, not sprite (see bg table) |

correction (m22): the bank-1 sprite table above is where every id big mario, the red paratroopa,
the red shell and small mario's climb grip live now. anything in an older commit's comment that
still puts super mario at bank-0 0x60-0x7f, his jump slab at bank-1 0x7c-0x7f, the paratroopa at
bank-1 0xc0-0xc7, the koopa at bank-0 0xc8-0xcf, the piranha at 0x84-0x85, the flame at 0x86 or the
lift deck at 0x88 is stale; treat this file and `mario.h`'s current `#define`s as the truth.

correction: the section heading and inline comments around `kTileBowserFirst` in mario.h say
*"m20's bowser run, 0x96-0xbb"* and *"so 0x96-0xbb collides with none of them"*. that range only
covers the two 32x32 body frames (0x96-0xb5, 32 tiles) plus the fire-breath dart (0xb6-0xbb, 6
tiles). `kTileBowserJaw` is defined separately at 0xbc and `assets_load_hazard_tiles` loads 2 tiles
there (`set_sprite_data(kTileBowserJaw, 2, kBowserJawTiles)`), so the run bowser actually occupies
is **0x96-0xbd**, two ids past what the heading claims. update any future edit of that heading to
say 0x96-0xbd.

---

## oam: the forty sprite slots

vram ids are only half the contract — two actors that agree about tile ids and disagree about oam
slots still overwrite each other. m22 made a koopa, a paratroopa and a piranha 16x24 bottom-aligned
in a 16x32 box, which is two rows of two 8x16 sprites — four slots where the old 16x16 art took two
— and oam has no spare forty-first slot, so the enemy pool and the hazards pool now **meet in the
middle** instead of each owning a fixed run.

| slots | owner | claimed by | notes |
|---|---|---|---|
| 0-3 | mario | `player_draw`, `mapscreen.c`'s `file_draw_mario` | small parks the lower row; every big pose is two rows of two |
| 4-8 | brick debris (4) then the fireball's puff (1) | `debris.c` | the world map borrows 4-11 for its four node markers and the toad room 4-7 for the retainer, both on screens with no debris, no items and no enemies |
| 9-10 | the loose item a block paid out | `blocks_draw.c` | |
| 11 | the coin pop | `blocks_draw.c` | |
| 12-13 | the two fireballs | `powerup.c` | |
| 14-33 | **the enemy pool** — allocated fresh every frame, walking the live pool in order and giving each entry 2 slots if its art is 16x16 (goomba, pancake, either shell) or 4 if it is a 16x32 box (koopa, paratroopa, piranha, and any of the three upside down as a corpse) | `enemies_draw` | five pool entries times four slots is the ceiling, hence 20. nothing else hands out a slot inside this run, so no two enemies can ever share one. `enemies_oam_top()` publishes the first slot past what the pool actually used |
| 24-39 | **the hazards pool** — a floor, not a fixed start: `hazards_draw` begins at `max(24, enemies_oam_top())` and hands what is above it to the deck planks (from slot 39 counting down), then bowser's body (from the floor counting up, 8 slots), then his breath (3 more above him), then a firebar's flames into whatever is left | `hazards.c`'s `claim_slot`/`settle_slots` | one owner byte per slot, so a slot that changes hands has its tile and palette written again and one nobody claims is parked |

the two ends only actually collide on a frame with five live enemies of which three are tall **and**
a lift or a bowser in view: 1-4 carries no walking enemy at all, 1-2's two lift shafts are 15 blocks
apart so only one deck is ever on screen, and 1-3's two decks in one pit want 8 slots against an
enemy pool that would have to reach 32 to touch them. a claimant that cannot get its slots is
dropped for that frame — the same graceful degradation the pool already gave a third firebar.

per-scanline cost did not change: a tall enemy's two sprite rows never share a scanline, so a line
crossing a row of enemies still pays 2 sprites each and `kEnemyRowCap` (4, plus mario's 2 = the
hardware's 10) still holds. only the slot count doubled.

---

## per-screen summary

what is actually resident in vram when each screen is up — i.e. which loader functions have run
and not since been overwritten. a screen not listed as calling a loader draws with whatever the
previous screen left behind; only the bg map cells and window-layer content change.

- **erase confirm card, pause card, game over card, clear card** — none of these calls any
  `assets_load_*` function (`title.c`/`states.c` only call `card_begin`/`card_print_*`, which write
  bg map cell ids out of the resident font). they draw text with the gbdk ibm font loaded once at
  boot (bank 0 bg 0x00-0x5f) and nothing else. whatever tile data the world map or a level loaded
  earlier is still sitting in vram, just not referenced by any cell these cards paint.
- **file select** (`mapscreen.c` `file_show`) — the second card that loads art of its own:
  `file_art_load` writes bank 1 bg 0x00-0x32 and three bg palettes, and the screen then calls
  `assets_load_sprite_tiles` and `assets_load_sprite_palettes` for the standing super mario it puts
  on a pipe. see the file select section below.
- **title card** (`title.c` `title_show`) — the one card that loads art of its own: `title_art_load`
  writes bank 1 bg 0x00-0x8e, bank 0 sprite 0x80-0xa7, seven bg palettes and one obj palette. see
  the title screen section above.
- **world map** (`mapscreen.c` `map_reset`) — calls, in order: `assets_load_sprite_tiles` (bank 0
  small mario, bank 1 big mario and the climb grip), `assets_load_sprite_palettes`, then `map_art_load` (bank 1
  bg 0x00-0x5d, bank 0 sprite 0x92-0x97, all eight bg palettes and three obj ones). the sprite
  loaders run first on purpose: `assets_load_sprite_palettes` writes all eight obj slots, and
  `map_art_load` wants the last three of them for the markers. it loads no terrain, scenery or
  block tables at all any more — nothing on this screen is drawn out of them — so those ranges hold
  whatever the last level left in them.
- **level play — overworld** (`flow_enter_level` via `terrain_init`, `hazards_load_level`,
  `enemies_load_level`, `player_init`, `hud_enter_level`) — the common set every level type loads:
  `assets_load_block_tables`, `assets_load_bg_tiles`, `assets_load_scenery_tiles`,
  `assets_load_bg_palettes`, `assets_load_sprite_tiles`, `assets_load_sprite_palettes`,
  `assets_load_item_tiles`, `assets_load_item_palettes`, `assets_load_enemy_tiles`,
  `assets_load_enemy_palettes`, `assets_load_hazard_tiles`, `assets_load_hud_font`. all run
  unconditionally, whether or not the level actually has an enemy/hazard/item of that kind.
- **level play — underground** — same call list as overworld, but `flow.c`'s `load_palette_set`
  calls `assets_load_bg_palettes_underground` instead of the plain one. no tile-id difference from
  overworld — only the 8 cgb bg palettes differ.
- **level play — castle** — same common call list, plus `assets_load_bg_tiles_castle` right after
  `assets_load_bg_tiles` (overwrites the ground family's 4 upper ids in place with masonry — no new
  ids) and `assets_load_enemy_palettes_castle` instead of the plain enemy palette loader (retints
  the koopa/star sprite palette slots for the fake bowser and the fire ramp — since m22 read the
  koopa's own three colours off the enemy sheet, and they are exactly bowser's, the koopa half of
  that retint now writes what the overworld already wrote and is kept only for the contract). a castle is also the
  only level type that can reach the toad room and the only one whose hazard set uses the bank-1
  bowser run (0x96-0xbd) and fire-breath tiles for real.
- **toad room** (`toad.c` `toad_frame`, tick 0 only) — additionally calls `assets_load_toad_tiles`
  (bank 1 sprite 0xc8-cf, bank 1 bg 0xec-fd). reached only from a castle level whose bible names a
  `toad_x`/`toad_column`, by touching the axe; loaded once, on the first frame of the room, not at
  level load.

---

## the title screen (issue #10)

the smbd title frame is generated art, not text: `games/mario/tools/png2tiles.py` turns
`art/title/title_screen.png` and `art/title/title_deluxe_sheet.png` into `src/gen/title_screen.c`
and `src/gen/title_deluxe.c`, and `title_art.c` (bank 7, with the data) loads them.

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| bank 1 bg 0x00-0x8e | the whole 20x18 title frame, 143 deduplicated tiles (`kTitleScreenFirstTile`, `kTitleScreenTileCount`) | `title_art_load` | title card only | every cell of the frame carries attribute bit 3, so the map reads bank 1 |
| bank 0 sprite 0x80-0xa7 | the gold "Deluxe" script and its sparkle, 20 8x16 sprites (`kTitleDeluxeTileCount`, 40 tiles) | `title_art_load` | title card only | oam slots 0-19, `SPRITES_8x16` |

both runs are transient: they sit on top of art every other screen reloads. bank 1 bg 0x00-0x8e
covers the map castle icon, the tree, the castle masonry, the scenery run, the map art, the side
pipe and the hud font, all of which `map_reset` or a level load put back. bank 0 sprite 0x80-0xa7
covers the flower, the hazards, the debris/puff run and the low end of the pinned bg terrain block
(bg ids 0xa0-0xa7 are the same bytes as sprite ids 0xa0-0xa7), all of which `assets_load_*` put
back on the way into a level. nothing else may claim these ids while the title is up, and the title
must never be repainted over a live level.

the seven cgb bg palettes the frame plans out come from the png too, and they overwrite bg palette
slots 0-6 - `card_begin`'s own `load_palettes` puts slots 0-2 back for every other card.

---

## the file select (issue #11)

the smbd SELECT FILE screen is generated art too: `art/file_select/file_select.png` (the whole
frame, with mario's 16x32 box and the label row left black for the rom to fill) and
`art/file_select/file_labels.png` (the five 32x8 label strips NEW, 1-1, 1-2, 1-3, 1-4) become
`src/gen/file_select.c` and `src/gen/file_labels.c`, and `file_art.c` (bank 7, with the data) loads
them.

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| bank 1 bg 0x00-0x1e | the whole 20x18 file select frame, 31 deduplicated tiles (`kFileSelectFirstTile`, `kFileSelectTileCount`) | `file_art_load` | file select only | every cell of the frame carries attribute bit 3, so the map reads bank 1 |
| bank 1 bg 0x1f-0x32 | the five label strips, four tiles each, loaded straight after the frame's own tiles (`kFileLabelsTileCount`, 20 tiles) | `file_art_load` | file select only | `file_art_label` writes four cells per slot: strip 0 is NEW, strip N is 1-N. cut per label rather than per letter because a 24 px word centred over a 32 px pipe needs a 4 px offset no bg cell can carry |
| bank 1 sprite 0x00-0x3f | the level's own super mario, standing on the chosen pipe and arcing between them | `assets_load_sprite_tiles` | file select, world map, level play | not new art: the file select reuses the play set so its mario is the same figure, in oam slots 0-3 |

this run overlaps the title's own bank-1 bg 0x00-0x8e reservation, which is fine because the two
screens never coexist and each reloads everything it draws from - the same way a level load puts
the castle icon, the tree and the scenery run back over both. bg palette slots 0-1 are the two the
png plans; slot 2 is the black-and-white pair `file_art.c` appends for the labels, because
neither planned palette puts white on color 1.

---

## the world map (issue #5)

world one's map is generated art too, ripped from a real smb deluxe frame:
`art/map/map_screen.png` (the whole 160x144 screen, with the node markers taken off, the header's
level digits and the footer's lives digits left black for the rom to fill, and the rip's own
eight-world by four-level clear list baked in with every dash hollow),
`art/map/map_glyphs.png` (the ten white digits and the world-two card's three border pieces),
`art/map/map_lives.png` (the same ten digits again, each pre-shifted into a 2x2 block - the rip's
lives readout sits half a tile off the grid in both axes), `art/map/map_sprites.png` (the node
markers), `art/map/map_water_frames.png` (four sparkle frames each for the strip's two animated
water tiles) and `art/map/map_list_fill.png` (world one's four dash cells with the dash solid).
they become `src/gen/map_screen.c`, `map_glyphs.c`, `map_lives.c`, `map_sprites.c`,
`map_water_frames.c` and `map_list_fill.c`, and `map_art.c` (bank 7, with the data) loads them.

world one fits the 160 px strip whole, so nothing on this screen scrolls.

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| bank 1 bg 0x00-0x46 | the whole 20x18 map frame, 71 deduplicated tiles (`kMapScreenFirstTile`, `kMapScreenTileCount`) | `map_art_load` | world map only | every cell of the frame carries attribute bit 3, so the map reads bank 1 |
| bank 1 bg 0x47-0x53 | the runtime glyph run, 13 tiles (`kMapGlyphsTileCount`), straight past the frame's own | `map_art_load` | world map only | ten digits, then the world-two card's corner/h-edge/v-edge, each reused by flip for the other sides |
| bank 1 bg 0x54-0x59 | the lives readout, three cells by two (`kMapLivesFirst`) | `map_art_lives` | world map only | ids with no art of their own: the readout writes a digit's pre-shifted quadrants into them, and the middle cell of a two-digit readout is the or of both digits' halves |
| bank 1 bg 0x5a-0x5d | the clear list's four fill variants (`kMapListFillTileCount`) | `map_art_load` | world map only | world one's own dash cells with the dash solid, one per column, so a cleared level's cell swaps id and keeps the frame's attribute byte |
| bank 0 sprite 0x92-0x97 | the node markers, three 8x16 sprites (`kMapSpritesTileCount`, 6 tiles) | `map_art_load` | world map only | oam slots 4-9, two per path node: the body over the rim |
| bank 1 sprite 0x00-0x3f, bank 0 sprite 0xe0-0xf7 | the level's own mario, walking the map | `assets_load_sprite_tiles` | file select, world map, level play | oam slots 0-3, the big set - not new art |

the four sparkle frames are never resident under an id of their own: `map_art_animate` swaps one
straight into the tile id the frame already planned a palette for, which is why
`map_water_frames.png` is an indexed png (its 2bpp value is the palette index, so the frame drops
in without a color match). `map_list_fill.png` is indexed for the same reason: a fill variant sits
in a cell under the frame's own attribute byte.

six of the eight cgb bg palettes are the ones the png plans; `map_art.c` appends two - slot 6, the
band's near-black under white ink on both odd indices, which serves both the glyph run and the
world-two card's font text, and slot 7, the gold band its call to action sits on. three of the
eight obj palettes (5-7, the level's goomba/koopa/fire slots) carry the markers; `player_init`'s
own `assets_load_sprite_palettes` puts the real set back on the way into a level.
