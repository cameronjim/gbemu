# world 1 level bible — schema

data files: `level-1-1.json`, `level-1-2.json`, `level-1-3.json`, `level-1-4.json`.

**all four levels are measured.** 1-1's terrain, blocks, enemies, decor, flagpole, castle and
bonus room, 1-2's terrain skeleton (floor pits, ceiling, pipes, warp zone, staircase, flagpole,
castle), the whole of 1-3 (every tree, its coins, its one block, its roster, its clouds, both
lift rows, the staircase, the pole and the castle) and the whole of 1-4 (every wall run, all four
lava pits, its eleven blocks, all seven firebars, the bridge, the axe and bowser) were extracted
pixel by pixel from the official nes and smbd map images and carry `"confidence": "measured"`; see
"measured levels" below. everything written about sequence-derived positions applies to 1-2's finer
detail (coins/blocks/enemies, `"approx"`), to 1-3's start column and lift travel, and to 1-4's
start column and its firebars' rotation speed/direction, which no still frame can show.

this is a research/transcription pass, not a rom dump. positions were reconstructed from
text descriptions and image maps (see "how positions were derived" below), not measured
pixel-by-pixel off the rom. a later pass must verify every coordinate against the actual
game running in our emulator before any of this is trusted for level-building.

## coordinate convention

- unit: tiles, 16px each (dmg/gbc tile size).
- `x`: column, 0-based, counted from the start of the level (not the start of the screen).
- `y`: row, 0-based, counted from the **top of the classic 15-row nes playfield**. the
  visible playfield in the original smb is 13 rows of terrain/objects plus the status bar;
  under this convention **ground surface is row 13** (the two rows below, 13-14, are the
  solid ground blocks; row 14 is the bottom of the screen). a pit/gap is a run of columns
  where the ground terrain is absent at y13-14.
- this is the nes/smb convention, reused as-is for smbd (gbc) even though the gbc screen
  shows fewer rows at once (smbd scrolls vertically to reveal rows above/below the nes
  window) — smbd does not change the underlying tile grid, only the camera.
- all `x` values are per-level, not per-screen. a "screen" is 16 columns wide if you need
  to convert to the classic nes screen-buffer numbering used by some guides.

## decor (optional, overworld only)

`decor` is the background scenery a level paints behind its terrain: the hills, bushes and
clouds that give an overworld its depth. it is optional — a level that omits the key gets no
scenery, which is what 1-2 and 1-4 do, and a sub-area never gets any. 1-1 has hills, bushes and
clouds; 1-3, which is trees over open air, has clouds only.

- scenery never displaces anything. the compiler stamps it last and skips any cell that is
  already terrain, an object, or a hidden block's reserved cell, and clips anything past the
  compiled level length. so a placement that collides with a staircase simply loses the cells
  it collided with, and the rest of the shape still lands.
- `x` is the left column of the shape and uses the same grid every other coordinate does.
- a `big_hill` is 5 columns wide and 3 rows tall, a `small_hill` 3 wide and 2 tall; both stand
  on row 12, the row the ground's grass grows out of, and their rows are derived, never given.
- a `bush` is `width` columns on row 12: a left cap, `width - 2` middles, a right cap. a width
  of 1 still spends two columns, because smb's narrowest bush is its two caps back to back.
- a `cloud` is the only kind that carries a row. `y` is its **top** row on this same grid and
  the cloud occupies `y` and `y + 1`; `width` counts the top row's blocks including both caps.
  a source map measured against the 13-row visible strip is 2 rows short of this grid, so add
  2 when transcribing one.

## how positions were derived (read before trusting a coordinate)

- mariouniverse.com map images (nes `smb` set and gbc `smbd` challenge/boo-race sets) were
  fetched as sources but could not be pixel-parsed by the tools available in this pass —
  they are cited per level as image references for a future manual/ocr pass, not as the
  source of any coordinate below.
- themushroomkingdom.net supplied the map *legend* (hidden-block types, lift types,
  firebar rotation convention) but no coordinates or level-specific text.
- vgmaps.de (vgmaps.com's current domain) map pages were fetched; they are image files with
  only attribution/caption metadata, no coordinate text. cited as image references only.
- mariowiki.com (Super Mario Wiki) provided the actual transcribable content: ordered
  prose walkthroughs of terrain/blocks/enemies, smbd challenge-mode red coin descriptions,
  yoshi egg descriptions, and medal score targets. **all block/enemy/terrain facts in
  these files come from mariowiki.com text**, fetched through an automated
  fetch-and-summarize tool (not read as raw html), so exact counts/wording should be
  spot-checked against the live pages if something looks off.
- mariowiki's prose gives *sequence* (this comes after that) and *relationship*
  (e.g. "the hidden 1-up is between the 4th pipe and the first pit") but essentially never
  gives literal tile numbers. every `x`/`y` pair in these files was therefore reconstructed
  by walking that sequence and assigning evenly-spaced columns — it encodes ORDER
  faithfully, not measured position.
- consequence: **every coordinate in these files is `"confidence": "approx"`.** nothing
  in this pass earns `"confidence": "sourced"` for position. `"sourced"` is reserved for
  non-positional facts (a block's contents, an enemy's kind, a red coin's existence) that
  came directly from cited wiki text. `"unknown"` is used where even the relative order
  could not be determined from any source.
- repeated identical enemies (e.g. "16 Little Goombas scattered throughout") could not be
  individually placed; only the goombas mariowiki calls out by name/landmark are given
  their own entry, and a `"count_only"` enemy entry records the remainder with
  `confidence: "unknown"` position and a note of the total count from the source.

## per-level json shape

```
{
  "level": "1-1",
  "type": "overworld | underground | castle | water",
  "timer": <int, starting countdown seconds>,
  "length_columns": <int or null>,
  "length_columns_confidence": "sourced | approx | unknown",
  "start": {"x": 0, "y": 13},
  "terrain": [
    {"kind": "ground", "x0": 0, "x1": 20, "confidence": "..."},
    {"kind": "gap", "x0": 68, "x1": 70, "confidence": "..."},
    {"kind": "pipe", "x": 28, "height": 2, "dest": "bonus_room|overworld|null", "confidence": "..."},
    {"kind": "stairs", "x0": 150, "x1": 158, "step_height": 8,
     "heights": [1, 2, 3, 4, 5, 6, 7, 8, 8], "confidence": "..."},
    {"kind": "elevation", "x0": .., "x1": .., "y": .., "confidence": "..."}
  ],
  "blocks": [
    {"x": 22, "y": 9, "kind": "question|brick|hidden|hard",
     "contents": "coin|mushroom_fire|star|oneup|multicoin|vine|nothing",
     "confidence": "sourced|approx|unknown", "notes": "..."}
  ],
  "enemies": [
    {"x": 22, "y": 13, "kind": "goomba|koopa_green|koopa_red|koopa_para_red|piranha|firebar|bowser_fake",
     "confidence": "approx|unknown", "notes": "..."}
  ],
  "areas": [
    {"id": "bonus-1", "kind": "bonus_room|underground|warp_zone", "entry_x": .., "exit": "...",
     "columns": 18, "start": {"x": 2, "y": 13},
     "terrain": [
       {"kind": "bricks", "x0": 0, "x1": 0, "y0": 2, "y1": 12, "confidence": "..."},
       {"kind": "pipe", "x": 13, "height": 2, "dest": "overworld", "confidence": "..."}
     ],
     "blocks": [...], "coins": [{"x": 5, "y": 5}, ...], "notes": "..."}
  ],
  "decor": [
    {"kind": "big_hill",   "x": 0,  "confidence": "..."},
    {"kind": "small_hill", "x": 15, "confidence": "..."},
    {"kind": "bush",  "x": 10, "width": 4, "confidence": "..."},
    {"kind": "cloud", "x": 1, "y": 6, "width": 3, "confidence": "..."}
  ],
  "flag": {"x": <int or null>, "confidence": "..."},
  "castle_end": {"x": <int>, "big": <bool>, "notes": "..."},
  "warps": [
    {"from_x": .., "via": "pipe|warp_zone", "to_level": "2-1", "confidence": "..."}
  ],
  "smbd": {
    "red_coins": [{"x": .., "y": .., "notes": "landmark this is near", "confidence": "..."} , ...5 total],
    "yoshi_egg": {"x": .., "y": .., "notes": "...", "confidence": "..."},
    "medal_score": <int, english release>,
    "medal_score_jp": <int or null>
  },
  "smbd_deltas": "prose note on how the gbc smbd layout is known/believed to differ from the nes smb layout described above",
  "sources": ["https://..."],
  "confidence_notes": "level-specific caveats beyond the file-wide ones in SCHEMA.md"
}
```

## enums

- `terrain.kind`: `ground`, `gap`, `pipe`, `pipe_side`, `stairs`, `elevation`, `lift_platform`,
  `lift_vertical`, `bricks`, `castle`, `island`, `tree`, `bridge`,
  `ceiling_gap` (underground levels only: `{"x0": .., "x1": ..}` clears the roof's one solid row
  (`CEILING_ROW`) over that span, the only way to carve a hole back out of the roof `apply_ceiling()`
  otherwise stamps across every underground-typed column - 1-2 needs one for the entry shaft, one
  for the lift shaft that carries a rider up to the walkable roof over the warp zone, and one for a
  narrow annotation-only notch just past it), `bricks` (a solid rectangle stamped straight into the
  compiled grid - `{"x0": .., "x1": .., "y0": .., "y1": .., "material": "brick" | "hard" |
  "question" | "coin" | "castle" | "lava"}`, `material` defaults to `"brick"`. this is terrain, not a `blocks[]` entry: no
  interactive state, so it cannot be bumped or broken and pays out nothing - the right primitive
  for the ~450 cells of pure background structure a measured underground interior has that nothing
  there ever needs to hit, without inflating `LEVEL_MAX_BLOCKS`, and also for a `?` block a level's
  own per-level block list has no more room left for (see the bank note in level-1-2.json's
  confidence_notes). the same primitive an `areas[].terrain` entry's own `"bricks"` kind already
  used, now available to a main level's terrain list too)
- a castle `gap` takes an optional `"lift": false`. a prose-derived castle got a horizontal deck
  thrown across any pit wider than `CASTLE_LIFT_GAP` columns, so an unverified extent could not
  make the level impossible; a measured pit says `false` and gets what the rip draws, which for
  1-4's thirteen-column bridge pit is a bridge and no deck at all.
- `decor.kind`: `big_hill`, `small_hill`, `bush`, `cloud`
- `blocks.kind`: `question`, `brick`, `hidden`, `hard`
- `blocks.contents`: `coin`, `mushroom_fire`, `star`, `oneup`, `multicoin`, `vine`, `nothing`
- `enemies.kind`: `goomba`, `koopa_green`, `koopa_red`, `koopa_para_green`, `koopa_para_red`,
  `piranha`, `firebar`, `bowser_fake`, `lift_horizontal`, `lift_vertical`
- a `firebar` entry carries three optional fields of its own: `length` (segments, default 6),
  `speed` (`"slow"` | `"fast"`, default slow - physics.json's 0x28 and 0x38 raw rates) and
  `direction` (`"cw"` | `"ccw"`, default cw). compile_level packs them into the object's param as
  `(length & 0x0F) | (0x10 if fast) | (0x20 if ccw)`; the pivot cell itself still compiles to a
  hard block under the bar.
- a `bowser_fake` entry's own `x`/`y` are honoured when the bible measured them: he starts on that
  cell and paces out to the bridge's last column. without them his patrol is derived from the
  bridge's own last stretch, which is what a prose-derived castle got.
- a `bowser_fake` entry may also carry `fire_from`: the column his flame spawner arms at. smb1
  starts throwing flames at mario several screens before the bridge, from the right edge of the
  view rather than out of a jaw that is still off screen, so this is the column that band begins
  at - read off the leftmost flame the level's own rip draws in flight, one screen further left.
  it compiles to the level's `BOWSER_FIRE_COLUMN`, and to the `BOWSER_FIRE_X` the engine reads
  (that column in px less one, so a level that names none comes out 0xffff). 0 means no zone.
- `confidence`: `measured` (the coordinate was extracted from a map image pixel by pixel —
  see "measured levels" below), `sourced` (fact came directly from cited text), `approx`
  (position is sequence-derived, see above; or a count/detail is a reasonable paraphrase of
  source text), `unknown` (no source data at all — placeholder only)

## warp destinations that do not exist yet

`kLevelCount` is 4 (world one only). a `warps[].to_level` naming a level in that table compiles to
that level's index and works. one that parses as a `world-level` pair but is not in the table
(1-2's real warp zone sends the player to worlds 2, 3 and 4) compiles to `WARP_UNBUILT` (0xFF) in
`compile_level.py`: the pipe is still built at its real column, in its real left-to-right order,
and the room's signage still names the real destination in the bible - only the jump itself is
inert. `flow_warp_under_player()` returns `WARP_UNBUILT` like any other "no warp here" result and
`main.c`'s existing `!= 0xFF` guard already treats it that way, so standing on the pipe and
pressing down is a polite no-op, never a crash and never a silent teleport into a level that
happens to already exist (an earlier version of this compiler clamped an unresolvable target to
the last level of world one, which is exactly the bug this replaces). a `to_level` that is not a
`world-level` pair at all (1-2's minus-world entry, `"-1"`, a wall-clip trick rather than a pipe)
compiles to nothing and never gets a pipe.

## measured levels

none of the four levels is sequence-derived any more. every terrain run, block, enemy, decor shape, the
flagpole and the castle were extracted from the official nes map images by classifying each 16x16
cell against a tile sheet cut from the same image, and carry `"confidence": "measured"` wherever
the extraction is solid (1-2's skeleton: floor pits, ceiling runs, pipe columns, the warp zone's
order and destinations, the closing staircase, the flagpole and the castle). 1-2's finer detail -
individual coin/block/enemy placement inside the underground run and its bonus room - remains
`"approx"`: the tile classifier that produced the measurement is reliable on the skeleton and
rough on small objects (see `confidence_notes` in `level-1-2.json`). 1-3 was transcribed the same
way from both rips at once - the nes one and the smbd challenge one, classified separately and
diffed column by column - and the smbd geometry is what got built wherever they differ (see that
file's `smbd_deltas`); what stays `approx` there is its start column, which no rip draws, and each
lift's travel, because a rip catches a moving deck at one instant. 1-4 was transcribed the same way
again, and there the two rips turn out to be the same level - the smbd one is the nes one with its
first sixteen columns cropped away - so there was no geometry to choose between; what stays `approx`
there is its start column and each firebar's rotation speed and direction, which a still frame
cannot show (see that file's `confidence_notes`). the fields a measured level uses that a
prose-derived one does not:

- `length_columns` is honoured rather than derived. when it is an integer the compiler builds
  exactly that many columns; when it is null the length is still the last positioned feature
  plus the compiler's own padding. it must not exceed the grid the engine allocates
  (`LEVEL_GRID_COLUMNS` in the generated levels.h, sized by the longest level).
- `terrain.stairs.heights` is an optional array of one block height per column, left to right.
  it is the only way to write a descending flight or a flat-topped one; `step_height` stays
  beside it as the shape's nominal height and is what a bible without `heights` still uses.
- `castle_end.x` places the castle's left column. without it the castle stands a fixed short
  walk past the flagpole, which is what the other three levels still do.
- `castle_end.big` picks the eleven-row, nine-column keep every x-3 in smb ends with instead of
  the five-by-five one. same six castle kinds either way, and the clear walk stops at the same
  `CASTLE_DOOR_OFFSET` column, which is a real arch in both. a keep whose columns run past
  `length_columns` is clipped there, which is what both 1-3 rips do at the map edge.
- `start.y`, when given, is the surface row the player's feet rest on top of, and it is honoured
  instead of scanning the column for its topmost solid cell. a castle needs it: 1-4's roof is
  three rows of masonry directly over the column he starts on, so a scan from the top of the grid
  lands on the ceiling rather than the platform under his feet. the three levels that carry
  `"y": 13` name the row the scan would have found anyway.

## measured sub-areas

an `areas` entry that carries `columns` is laid out cell by cell instead of being synthesised
from a coin count in its prose. such an entry uses:

- `columns`: the room's width. the room always has the same 15 rows and the same two ground
  rows at the bottom as a level does.
- `start`: the cell the pipe drops the player onto. it is compiled into the area's
  `START_COLUMN`/`START_ROW` and is where `flow_enter_sub_area` places him, so a room with a
  wall down its left edge starts him past the wall rather than inside it.
- `terrain[].kind = "bricks"`: a solid rectangle of brick from (`x0`,`y0`) to (`x1`,`y1`).
- `terrain[].kind = "pipe"`: as in a level. the one carrying `"dest": "overworld"` is the
  room's exit, and its column and cap row become the area's `EXIT_COLUMN`/`EXIT_TOP_ROW`.
- `coins`: every collectible coin, one `{"x":..,"y":..}` per coin. an area without this key
  falls back to the old behaviour, where the count is read out of the room's `notes` prose and
  a flat row of coins is invented for it (which is what 1-2's bonus room still does).

## 1-2's own primitives

1-2 is transcribed cell by cell from the smbd map (see `level-1-2.json`'s `confidence_notes`), and
a handful of things the real level does needed bible fields nothing else uses:

- `coins`: a top-level list of `{"x":..,"y":..}` cells, loose coins standing in the level's own grid
  (the underground run has twenty-six). they compile to coin cells the engine collects on contact.
- `terrain[].kind = "pipe_side"`: the sideways pipe mario walks into. `x` is the mouth's rim column
  and `y` the mouth's top row (the mouth is always two rows tall); `shaft_x` is the left column of
  the ordinary vertical shaft the mouth joins, `shaft_top` the row that shaft rises to (default: the
  ceiling row). columns between the rim and the shaft are horizontal body. `jump_to`/`jump_to_row`
  are the destination, as for a `pipe` with `jump_to`; the engine's trigger is walking right into the
  rim while standing on the mouth's floor.
- `terrain[].pipe.jump_to_row` (optional, with `jump_to`): the landing row. name a pipe cap's row and
  the player rises out of that pipe; name an open-air row and he drops in from there and falls
  (1-2's entrance shaft). omitted, he lands on the target column's floor.
- `terrain[].kind = "lift_vertical"`: a measured vertical lift. `x` is the deck's left column, `y0`
  the top row of its travel, `span` the rows it covers, `reverse` whether it starts at the bottom
  running up. the engine has two lift slots, so a level places at most two.
- an `areas[]` entry's `start.y`, when given, is honoured instead of the column's floor, so a room
  can drop the player in from the top of its shaft; and `return_x` names the main-grid pipe the
  room's exit brings him up out of when it is not the one he went down.
- the seam between two segments is sealed by the bible's own terrain (1-2 uses the brick wall at
  column 24 and the coin room's exit shaft at 215-216, both carried up to row 0 so the roof walk
  cannot leak across), not by a compiler-invented wall.

## 1-3's own primitives

1-3 is transcribed cell by cell from the nes and smbd 1-3 map rips (see `level-1-3.json`'s
`confidence_notes`), and the level is built out of one shape nothing else uses:

- `terrain[].kind = "tree"`: `{"x0", "x1", "y"}` is a tree. row `y` from `x0` to `x1` is the canopy
  - a left cap, `x1 - x0 - 1` middles, a right cap (`kBlockTreeTopL/M/R`) - and the columns
  `x0 + 1` to `x1 - 1` carry a trunk (`kBlockTrunk`) from `y + 1` down to the bottom row, which is
  the inset the real map draws at every canopy width from three up (a two-wide canopy is the two
  caps back to back and has no trunk at all). the canopy is **solid on every side**: smb has no
  one-way platform, and its tree tops stop a head coming up from underneath, which is what
  separates them from the `island` kind's `kBlockThin` deck. the trunk is scenery and only ever
  fills sky, so a tall tree whose trunk hangs down through a shorter tree's canopy (1-3 stacks two
  at columns 59-63 and two more at 24-31) leaves that canopy standing.
- a level with any `tree` run is an athletic level exactly as one with an `island` run is: the
  default ground is dropped and every column the bible does not place is open air.
- `terrain[].kind = "lift_platform"` with a `y`: the measured form of a horizontal deck. `x0`/`x1`
  are the travel the deck's own width rides between and `y` is its row, instead of the older form's
  "carve a pit and put a pair of decks in it". a map rip catches a moving platform at one instant,
  so the row and the drawn columns are measured and the travel is the pit it has to bridge.
  `"reverse": true` starts the deck at the far end of that travel, which is how two decks sharing
  one pit stay off each other. `"live": false` records a deck the map draws that the engine has no
  slot left for - `kLiftSlots` is three, and the third borrows the firebar/bowser oam slots, so a
  level may carry a third deck or a bar/bowser and never both (compile_level.py refuses one that
  wants both).
- 1-3's decor is clouds and nothing else. over open air there is no ground for a hill or a bush to
  stand on, and neither rip draws one. two of its clouds have their lower row behind a canopy in
  both rips, and `apply_decor`'s whole-shape rule drops a cloud it cannot place entire.

## smbd-specific notes (apply to all 4 files)

- challenge mode adds exactly 5 red coins and 1 hidden yoshi egg per level, plus a
  score-based medal (bronze/silver/gold tiers keyed off `medal_score`), per
  mariowiki/strategywiki's Super Mario Bros. Deluxe/Challenge coverage. medal score
  targets and jp variants are recorded per-level where sourced.
- the smbd maps differ from the nes ones in several measured ways, all built here: 1-2's warp
  zone room is a coin room (brick platforms, three question blocks, coins, its own sideways
  exit pipe) instead of the three-pipe warp select, and a closing staircase is shorter with the
  flagpole eight columns past its last column instead of nine (1-1 and 1-2: eight columns and
  seven blocks tall against the nes's nine and eight; 1-3, whose staircase is three two-column
  tiers rather than one-column steps: six columns and seven blocks tall at 136-141 against the
  nes's six and eight at 138-143). 1-3 also moves every platform past its lift pit two columns
  left, raises one tree three rows and drops one of the nes map's four lift decks - the full list
  is in `level-1-3.json`'s `smbd_deltas`. 1-1, 1-2 and 1-3 follow the smbd geometry. 1-4 is the one
  level where the two rips agree cell for cell: the smbd challenge rip is the nes rip with its
  first sixteen columns cropped off, so nothing there moved between smb1 and smbd, and the opening
  platform and its steps come off the nes rip, the only one that draws them.
