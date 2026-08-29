# world 1 level bible — schema

data files: `level-1-1.json`, `level-1-2.json`, `level-1-3.json`, `level-1-4.json`.

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
    {"kind": "stairs", "x0": 150, "x1": 158, "step_height": 8, "confidence": "..."},
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
     "terrain": [...], "blocks": [...], "notes": "..."}
  ],
  "flag": {"x": <int or null>, "confidence": "..."},
  "castle_end": {"x": <int>, "notes": "..."},
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

- `terrain.kind`: `ground`, `gap`, `pipe`, `stairs`, `elevation`, `lift_platform`, `island`
- `blocks.kind`: `question`, `brick`, `hidden`, `hard`
- `blocks.contents`: `coin`, `mushroom_fire`, `star`, `oneup`, `multicoin`, `vine`, `nothing`
- `enemies.kind`: `goomba`, `koopa_green`, `koopa_red`, `koopa_para_green`, `koopa_para_red`,
  `piranha`, `firebar`, `bowser_fake`, `lift_horizontal`, `lift_vertical`
- `confidence`: `sourced` (fact came directly from cited text), `approx` (position is
  sequence-derived, see above; or a count/detail is a reasonable paraphrase of source
  text), `unknown` (no source data at all — placeholder only)

## smbd-specific notes (apply to all 4 files)

- challenge mode adds exactly 5 red coins and 1 hidden yoshi egg per level, plus a
  score-based medal (bronze/silver/gold tiers keyed off `medal_score`), per
  mariowiki/strategywiki's Super Mario Bros. Deluxe/Challenge coverage. medal score
  targets and jp variants are recorded per-level where sourced.
- smbd is known (per mariowiki 1-2 coverage) to alter at least one nes layout for the
  smaller gbc screen: 1-2's warp zone room is replaced with a room of ? blocks/coins
  instead of the 3-pipe warp select. other smbd layout deltas were not found in the
  sources used for this pass and are marked `"unknown"` rather than assumed identical
  to nes.
