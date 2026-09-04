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
| 0x60-0x9f | — | — | — | unclaimed as bg; would share bytes with bank-0 sprite ids in the same sub-range (super mario, flower, hazards, debris — see the sprite table below), so nothing loads bg data here |
| 0xa0-0xbf | pinned terrain block: ground/brick/question/spent/pipe/coin quadrants (`kTileGroundTopL`..`kTileCoinBr`) | `assets_load_bg_tiles` | level play (all three types), world map | exactly full per mario.h; a castle load overwrites the ground family's 4 upper ids (0xa0-0xa3) in place via `assets_load_bg_tiles_castle` — no new ids, just different pixels under the same 4 ids |
| 0xc0-0xf7 | — | — | — | unclaimed as bg; shares bytes with bank-0 sprite families (enemy 0xc0-0xcf, item 0xd0-0xdf, mario 0xe0-0xf7) |
| 0xf8-0xff | ground fill (lower half) + hard block + thin platform (`kTileGroundFillBl`..`kTileThinUnder`) | `assets_load_bg_tiles` | level play, world map | butts directly against the mario sprite family ending at 0xf7 — no gap, no overlap |

## bank 0, sprite tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x60-0x7f | super/fire mario: shared upper slab + 7 leg poses (`kTileSuperFirst`, 32 tiles) | `assets_load_sprite_tiles` | level play, world map (mario's walk sprite) | both small and super frame sets stay resident together so the grow animation never needs a vram write |
| 0x80-0x83 | fire flower (`kTileFlowerFirst`, 4 tiles) | `assets_load_item_tiles` | level play | |
| 0x84-0x89 | hazards: piranha, flame, lift deck (`kTileHazardFirst`/`kTilePiranha`/`kTileFlame`/`kTileLiftDeck`, 6 tiles) | `assets_load_hazard_tiles` | level play | |
| 0x8a-0x8b | **FREE** | — | — | held the fake bowser back when he was one 16x16 pair; freed when m20 moved him to bank 1 (32x32) |
| 0x8c-0x91 | throwaway anims: brick debris + fireball puffs (`kTileDebris`, `kTilePuffA`, `kTilePuffB`, 6 tiles) | `assets_load_item_tiles` | level play | |
| 0x92-0x97 | world-map node markers: three 8x16 sprites, the red body, the blue one and the rim they share (`kTileMapMarkerRed`.., 6 tiles) | `map_art_load` | world map only | a marker is a solid 8x8 in four colors, one more than an obj palette's three opaque slots, so a body rides over a rim carrying only its orange edge |
| 0x98-0x9f | **FREE** (8 ids) | — | — | what is left of the run mario.h calls unclaimed |
| 0xa0-0xbf | — | — | — | belongs to bg's pinned terrain block; nothing sprite-side claims it |
| 0xc0-0xcf | enemy family: goomba walk/squash, shell, koopa walk (`kTileEnemyFirst`, 16 tiles) | `assets_load_enemy_tiles` | level play | exactly full |
| 0xd0-0xdf | item family: mushroom/star/1-up (16x16 each), coin pop, fireball frame A (`kTileItemFirst`..`kTileFireball`, 16 tiles) | `assets_load_item_tiles` | level play | exactly full; `kTileFireball` (0xde) is also loaded in **bank 1** as the projectile's second spin frame (see below) |
| 0xe0-0xf7 | small mario, six 16x16 frames (`kTileMarioFirst`, 24 tiles) | `assets_load_sprite_tiles` | level play, world map | |
| 0xf8-0xff | — | — | — | belongs to bg's extra terrain ids |

---

## bank 1, background tile ids

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| 0x00-0x4d | the whole 20x18 world map frame plus its runtime glyph run (see the world map section below) | `map_art_load` | world map only | the title's and the file select's own frames reuse the same low ids while they are up; each screen reloads what it draws |
| 0x0a-0x11 | 1-3's tree canopy + trunk (`kTileTreeFirst`, 8 tiles) | `assets_load_scenery_tiles` | level play (only levels with a `tree` terrain run use it; currently 1-3) | also resident (but unused) on the world map — that screen calls `assets_load_scenery_tiles` too, see per-screen summary |
| 0x12-0x18 | castle masonry courses, axe, bridge, deep-lava fill (`kTileCastleBrickLower`..`kTileLavaDeep`, 7 tiles) | `assets_load_scenery_tiles` | level play (castle type for the masonry/axe/bridge; any level type with a >1-deep lava pit for the lava fill) | |
| 0x19-0x1f | **FREE** (7 ids) | — | — | called out unclaimed in mario.h |
| 0x20-0x5d | scenery run: lava top, castle wall/window/door-frame, flag ball/cloth/pole-adjacent cells, clouds, hills, bushes, pole shaft, inner crenel, blank + ball-right (`kTileSceneryFirst`-`kTileSceneryLast`, exactly full) | `assets_load_scenery_tiles` | level play (whichever pieces a level's type/decor use), world map (loaded, unused) | |
| 0x5e-0x5f | **FREE** (2 ids) | — | — | see correction below — mario.h's own comment near `kTileCastleBrickLower` is wrong about this range |
| 0x72-0x7a | sideways pipe, 9 tiles (`kTilePipeSideTl`-`kTilePipeSideBodyB`) | `assets_load_scenery_tiles` | level play (levels with a `pipe_side` terrain entry; currently 1-2), world map (loaded, unused) | vram bank 0 had no ids left for this, per mario.h |
| 0x7b-0x7f | **FREE** (5 ids) | — | — | between the side-pipe run and the hud font |
| 0x80-0x8c | hud row glyphs: 10 digits, blank, coin icon, the letter x (`kTileHudDigitFirst`-`kTileHudLetterFirst`, 13 tiles) | `assets_load_hud_font` | level play only (the window-layer hud row is drawn only during play) | shares bytes with bank-1 **sprite** ids 0x80-0x8c — see the sprite table |
| 0x8d-0xeb | **FREE** (95 ids) | — | — | mario.h reserves headroom up to 0x94 for the hud run (see note below) but nothing loads past 0x8c; the rest of this span is genuinely empty |
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
| 0x00-0x7b | — | — | — | unclaimed as sprite ids; bg ids below 0x80 do not share bytes with anything (see addressing rule), so this range is only ever bg |
| 0x7c-0x7f | super mario's jump pose upper slab (`kTileSuperJumpUpper`, 4 tiles) | `assets_load_sprite_tiles` | level play | the one super-mario pose bank 0 had no id left for |
| 0x80-0x8c | **RESERVED, not loaded as sprite data** — shares bytes with bank-1 **bg** hud font (0x80-0x8c above) | — | level play | a sprite must never use these ids while the hud row is on screen (i.e. ever, during play) |
| 0x8d-0x94 | **RESERVED, unused headroom** | — | — | mario.h's hud-font comment (near `kTileHudDigitFirst`) reserves the whole 0x80-0x94 span as off-limits to a future sprite, even though the actual hud font only reaches 0x8c today; keep new sprite work out of 0x8d-0x94 too unless the hud font grows into it first |
| 0x95 | **FREE** | — | — | the one id between the reserved hud headroom and the bowser run; called out explicitly in mario.h |
| 0x96-0xbd | bowser: two 32x32 body frames + fire-breath dart + open-jaw tell (`kTileBowserFirst`..`kTileBowserJaw`, 40 tiles total) | `assets_load_hazard_tiles` | level play (castle type only) | **correction below** |
| 0xbe-0xbf | **FREE** (2 ids) | — | — | the gap between the jaw tile and the paratroopa run; not called out anywhere in code, found by inspection |
| 0xc0-0xc7 | paratroopa's two fly frames (`kTileParaFly0`/`kTileParaFly1`, 8 tiles) | `assets_load_enemy_tiles` | level play (levels with a `koopa_para_red` enemy; currently 1-3) | body is walk0's for both frames — only the wing beats |
| 0xc8-0xcf | toad-room retainer sprite, 8 tiles (`kTileToadFirst`) | `assets_load_toad_tiles` | toad room only | |
| 0xd0-0xdd | **FREE** (14 ids) | — | — | |
| 0xde-0xdf | fireball's second spin frame (`kTileFireball`, reusing the id bank 0's item family already owns) | `assets_load_item_tiles` | level play (fire mario) | drawn with `S_BANK` set to alternate with bank 0's frame A at the same id — a deliberate dual-bank reuse, not a collision |
| 0xe0-0xe3 | flagpole climb pose, small mario (`kTileClimbSmall`, 4 tiles) | `assets_load_sprite_tiles` | level play (clear sequence) | |
| 0xe4-0xeb | flagpole climb pose, big mario upper+lower (`kTileClimbBigUpper`, 8 tiles) | `assets_load_sprite_tiles` | level play (clear sequence) | |
| 0xec-0xff | **FREE** (20 ids) | — | — | the toad sign glyphs at the same numeric ids are bg, not sprite (see bg table) |

correction: the section heading and inline comments around `kTileBowserFirst` in mario.h say
*"m20's bowser run, 0x96-0xbb"* and *"so 0x96-0xbb collides with none of them"*. that range only
covers the two 32x32 body frames (0x96-0xb5, 32 tiles) plus the fire-breath dart (0xb6-0xbb, 6
tiles). `kTileBowserJaw` is defined separately at 0xbc and `assets_load_hazard_tiles` loads 2 tiles
there (`set_sprite_data(kTileBowserJaw, 2, kBowserJawTiles)`), so the run bowser actually occupies
is **0x96-0xbd**, two ids past what the heading claims. update any future edit of that heading to
say 0x96-0xbd.

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
  mario/super, bank 1 climb/jump poses), `assets_load_sprite_palettes`, then `map_art_load` (bank 1
  bg 0x00-0x4d, bank 0 sprite 0x92-0x97, all eight bg palettes and three obj ones). the sprite
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
  the koopa/star sprite palette slots for the fake bowser and the fire ramp). a castle is also the
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
| bank 0 sprite 0x60-0x7f, bank 1 sprite 0x7c-0x7f | the level's own super mario, standing on the chosen pipe and arcing between them | `assets_load_sprite_tiles` | file select, world map, level play | not new art: the file select reuses the play set so its mario is the same figure, in oam slots 0-3 |

this run overlaps the title's own bank-1 bg 0x00-0x8e reservation, which is fine because the two
screens never coexist and each reloads everything it draws from - the same way a level load puts
the castle icon, the tree and the scenery run back over both. bg palette slots 0-1 are the two the
png plans; slot 2 is the black-and-white pair `file_art.c` appends for the labels, because
neither planned palette puts white on color 1.

---

## the world map (issue #5)

world one's map is generated art too, ripped from a real smb deluxe frame:
`art/map/map_screen.png` (the whole 160x144 screen, with the node markers taken off, the header's
level digits and the footer's lives digits left black for the rom to fill, and the clear list's
four cells set hollow), `art/map/map_glyphs.png` (the ten white digits, the filled clear-list cell
and the world-two card's three border pieces), `art/map/map_sprites.png` (the node markers) and
`art/map/map_water_frames.png` (four sparkle frames each for the strip's two animated water
tiles). they become `src/gen/map_screen.c`, `map_glyphs.c`, `map_sprites.c` and
`map_water_frames.c`, and `map_art.c` (bank 7, with the data) loads them.

world one fits the 160 px strip whole, so nothing on this screen scrolls.

| id range | owner | loader | screen(s) | notes |
|---|---|---|---|---|
| bank 1 bg 0x00-0x43 | the whole 20x18 map frame, 68 deduplicated tiles (`kMapScreenFirstTile`, `kMapScreenTileCount`) | `map_art_load` | world map only | every cell of the frame carries attribute bit 3, so the map reads bank 1 |
| bank 1 bg 0x44-0x51 | the runtime glyph run, 14 tiles (`kMapGlyphsTileCount`), straight past the frame's own | `map_art_load` | world map only | ten digits, then the filled clear-list cell, then the world-two card's corner/h-edge/v-edge, each reused by flip for the other sides |
| bank 0 sprite 0x92-0x97 | the node markers, three 8x16 sprites (`kMapSpritesTileCount`, 6 tiles) | `map_art_load` | world map only | oam slots 4-11, two per node: the body over the rim |
| bank 0 sprite 0x60-0x7f, bank 1 sprite 0x7c-0x7f, bank 0 sprite 0xe0-0xf7 | the level's own mario, walking the map | `assets_load_sprite_tiles` | file select, world map, level play | oam slots 0-1, the small set - not new art |

the four sparkle frames are never resident under an id of their own: `map_art_animate` swaps one
straight into the tile id the frame already planned a palette for, which is why
`map_water_frames.png` is an indexed png (its 2bpp value is the palette index, so the frame drops
in without a color match).

six of the eight cgb bg palettes are the ones the png plans; `map_art.c` appends two - slot 6, the
band's near-black under white ink on both odd indices, which serves both the glyph run and the
world-two card's font text, and slot 7, the gold band its call to action sits on. three of the
eight obj palettes (5-7, the level's goomba/koopa/fire slots) carry the markers; `player_init`'s
own `assets_load_sprite_palettes` puts the real set back on the way into a level.
