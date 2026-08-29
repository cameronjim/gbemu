# Super Mario Bros. Deluxe (GBC) — Deluxe-Specific Systems Research

Scope: everything SMB Deluxe (SMBD) does differently from NES Super Mario Bros. 1.
Confidence markers: **documented** (stated directly by a cited source), **inferred** (pieced
together from multiple partial sources, not stated outright), **must-verify** (no source found,
or sources conflict — confirm against the real ROM in-emulator before relying on it).

---

## 1. Camera / Viewport

- The GBC screen is smaller than the NES/Famicom screen, so SMBD's visible play area is cropped
  relative to NES SMB1; this crops out course elements that were visible on NES. **documented**
  — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- One community source states the GBC view shows roughly 1/3 of the original NES screen's visible
  play area. **inferred** (single secondary source, not an official spec) —
  https://tcrf.net/Super_Mario_Bros._Deluxe (via search snippet)
- Vertical camera control: pressing Up or Down on the d-pad while stationary pans the camera
  up/down to reveal off-screen space above/below Mario, compensating for the cropped screen.
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe and
  https://themushroomkingdom.net/manuals/smbdx.txt (official manual: "Control Pad Up/Down: Adjust
  field of view; duck or enter pipes")
- Horizontal look-ahead: pressing SELECT during gameplay shifts Mario's position on screen to look
  further ahead (scrolls the screen forward without moving Mario). **documented** —
  https://themushroomkingdom.net/manuals/smbdx.txt (manual: "SELECT: Change Mario's screen
  position") corroborated by a GameFAQs FAQ excerpt: "the Select Button moves the camera forward"
  — https://gamefaqs.gamespot.com/gbc/198850-super-mario-bros-deluxe/faqs/5890
- No-backtracking rule change: unlike NES SMB1 (where the screen locks and cannot scroll back once
  advanced), SMBD allows the player to scroll/back up a limited distance to see or reach the
  portion of the level cropped off-screen. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
  ("the player is able to backtrack in the course a little bit") and reinforced by a review
  excerpt: "the screen scrolls, meaning you can go backward if need be and collect some items you
  walked past" (found via WebSearch, secondary/review source, exact publication not confirmed) —
  treat the review-sourced framing as **inferred**, the mariowiki backtracking claim as
  **documented**.
- HUD layout on the small screen: only score, coins, and time are shown during a level; a clock
  icon replaces the word "TIME". Mario's/Luigi's remaining lives and the current level name are
  moved off the in-level HUD and onto the Pause screen instead. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Reception note: reviewers criticized that adjusting the screen (panning) while jumping over
  vertical platform sections was awkward/a "major pain," implying panning is not automatic and
  must be manually triggered by the player mid-jump for tall vertical layouts. **documented**
  (paraphrased critic sentiment) — https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
- RAM-level confirmation of a distinct level-scroll position separate from the player's own
  position: the community RAM map lists `FFA7` "X position of Level" and `FFA9` "Y position" as
  camera/level scroll coordinates, distinct from `C1CA`/`C1CC` (player X/Y) and `FF99` ("X position
  of Player"). This confirms the engine tracks camera and player position independently, which is
  what enables manual pan-ahead/pan-up without moving Mario. **documented** (community
  reverse-engineering, not Nintendo-published — treat exact addresses as **must-verify** against
  our own ROM disassembly) — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe/RAM_map

---

## 2. Modes

### Original 1985 Mode
- Faithful recreation of NES SMB1: 8 worlds x 4 levels = 32 levels, playable as Mario or Luigi
  (switch via SELECT on the world map). **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Player starts with 5 lives instead of NES's 3 (the Fortune Teller extra can raise starting lives
  to 10 — see Toy Box section). **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Life counter caps at a maximum of 127. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe

### Challenge Mode
- Unlocks after clearing courses in Original mode (exact unlock threshold not separately specified
  beyond "after earning points" language used for other modes — **must-verify** exact unlock
  condition for Challenge Mode itself, as distinct from Lost Levels/Boo unlocks). —
  https://themushroomkingdom.net/manuals/smbdx.txt
- All 32 Original-mode levels become selectable in Challenge Mode. **documented** —
  https://themushroomkingdom.net/manuals/smbdx.txt
- Objective per level: collect 5 hidden Red Coins, find 1 hidden Yoshi Egg, and beat a preset
  target score, each independently rewarding a medal: Red Coin Medal, Egg Medal, High Score Medal.
  **documented** — https://www.mariowiki.com/Medal_(Super_Mario_Bros._Deluxe) and
  https://themushroomkingdom.net/manuals/smbdx.txt
- Toad gives a performance rating/evaluation at the end of each Challenge Mode level based on medal
  thresholds reached. **documented** — https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
- Level-select screen shows overall Challenge Mode progress as a bar that fills completely at
  1,160,000 total points. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Challenge-mode-specific scoring overrides: a 1-up Mushroom is worth 2,000 points (instead of an
  extra life); an action that would normally grant a 1-up (e.g., 8 consecutive shell/stomp kills)
  instead grants 10,000 points; collecting every coin in a level ("Perfect Bonus") awards 10,000
  points. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe

### You vs. Boo (race mode)
- Unlocks at 100,000 points earned in Original 1985 mode. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Single-player race against an AI-controlled Boo across a set of levels; sources agree there are
  8 stages available. **documented** — https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe and
  https://themushroomkingdom.net/manuals/smbdx.txt (manual calls it "VS Boo Mode," single-player,
  "after meeting certain Original mode conditions")
- Boo's pace is driven by the player's own best completion time for that level: the Boo's speed is
  set to match the player's previous best time, and finishing faster than Boo upgrades a
  color-coded rank (white -> green -> red -> black) reflecting improvement. **documented** —
  https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe and https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Course layouts include extra hazards not in the base Original-mode version of the level, e.g.
  springboards/jump blocks positioned to help or hinder the race. **documented** (level content
  paraphrase) — https://www.mariowiki.com/Super_Mario_Bros_Deluxe — exact per-level Boo-mode layout
  edits are **must-verify** against ROM data.

### VS Game (link cable multiplayer)
- Requires two Game Boy Colors, two copies of the game, and a Game Boy Color Link Cable.
  **documented** — https://themushroomkingdom.net/manuals/smbdx.txt
- Race format: both players play simultaneously on a shared/mirrored course; first to reach the
  goal (flagpole) wins. **documented** — https://themushroomkingdom.net/manuals/smbdx.txt
- VS-exclusive level elements: reversible/vanishing blocks and spike blocks that one player can
  trigger to help themselves or hinder the opponent (walls/spikes toggle on shared course
  geography). **documented** — https://themushroomkingdom.net/manuals/smbdx.txt and
  https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
- 8 unique VS courses. **documented** — https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
  (search-snippet paraphrase; treat exact count as **inferred** pending direct manual confirmation
  of "8" versus the Boo-mode "8" possibly being conflated by summarizers — **must-verify**)
- Not available at all in the 3DS Virtual Console re-release (no link cable / no second cart).
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Separate from VS Game: two linked players can also just exchange/compare high scores over
  infrared (GBC's IR port), a non-race social feature. This IR score exchange does not work on 3DS
  VC because the 3DS's IR hardware is incompatible. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe

### Super Mario Bros. for Super Players (Lost Levels)
- Unlocks after accumulating a total of 300,000 points in Original 1985 mode. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe and https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
- This is SMBD's adaptation of the Japanese Super Mario Bros. 2 (aka "The Lost Levels" in the
  West), not the US SMB2. **inferred from context/naming** — https://en.wikipedia.org/wiki/Super_Mario_Bros._Deluxe
- Only 1 save slot is available for this mode (vs. 3 slots for Original mode). **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Two documented mechanical removals versus the source Lost Levels game: the wind gameplay feature
  is removed, and Luigi's distinct (floatier/slippier) physics from Lost Levels is removed —
  in SMBD, Luigi plays identically to Mario. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Worlds 9 through D (extra worlds that exist in some form in the ROM) are unused/unreachable in
  the finished game; TCRF documents them as incomplete, likely cut for time. **documented (as
  "unused content," reverse-engineered)** — https://tcrf.net/Super_Mario_Bros._Deluxe (via search
  snippet: "the sheer amount of missing or incomplete elements related to Worlds 9 through D...")
- An unused/hidden level-select screen for the "For Super Players" mode can be reached via cheat
  codes (GameShark), and from it all levels — including the unused Worlds 9-D — can be loaded.
  **documented (reverse-engineered, not in-game accessible normally)** —
  https://tcrf.net/Super_Mario_Bros._Deluxe (via search snippet)
- An unused "free movement" debug mode exists, enabled by setting RAM address `0xC1C1` to `0x02`,
  which lets the player move anywhere in a level including through solid blocks. **documented
  (reverse-engineered)** — https://tcrf.net/Super_Mario_Bros._Deluxe (via search snippet). Note:
  `C1C1` is documented elsewhere as the general player-state byte (ground/air/dead/etc. — see RAM
  map in section 5), so this debug value is a special-cased state on that same byte. **must-verify**
  by cross-referencing both TCRF and the Data Crystal RAM map directly against ROM disassembly.

### Toy Box / Album / Calendar / extras
- Fortune Teller: available from game start; gives one of five fortunes (Extremely Lucky down to
  Extremely Unlucky) that affects the new save file, e.g. "Extremely Lucky" starts a new game with
  10 lives instead of 5. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Mystery Room: 8 unlockable extras (Game Boy Printer banners, animations, graphics, a story
  creator) opened progressively by rescuing Mushroom Retainers/Princess Peach in castle levels.
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Calendar: lets the player mark up to 12 dates; 3 default marked dates correspond to the Japanese
  release dates of the original arcade Mario Bros., the Famicom, and the Game Boy. **documented**
  — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- "Yoshi Is Here!": unlocks after finding at least one Yoshi Egg in Challenge Mode; functions as a
  roulette that rapidly flashes through levels to help hint at which levels still have an
  uncollected egg. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Album: 6 unlockable award pictures (Bowser, Mario, Peach, Toad, Yoshi — one title unspecified in
  source) earned by completing Original mode, Challenge Mode goals, and Lost Levels.
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Printable Icons: 8 character/system icons unlock for Game Boy Printer output after rescuing the
  Toad at World 5-4. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Pictures gallery: 21 unlockable in-game photos, triggered by specific achievements (defeating
  certain enemies, collecting specific items, completing modes, playing multiplayer).
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Game Boy Printer Banners: unlock via rescues at World 1-4, 3-4, and 7-4; multiple
  Nintendo-themed banner designs printable. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe

---

## 3. Structure

### Overworld / level select
- Unlike NES SMB1's purely linear world-by-world progression (no overworld map at all — you simply
  play 1-1, 1-2, ... in sequence), SMBD adds an explicit world map screen per world showing the
  player's progress through that world's levels. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- On the world map, pressing SELECT switches the active playable character between Mario and
  Luigi. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Beating a castle (level 4 of a world) triggers a short non-interactive cutscene of Mario jumping
  on the castle repeatedly until it collapses, shown on the map. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- In Challenge Mode, the equivalent "map"/select screen is a flat level-select list (all 32 levels
  open at once) with the aggregate progress bar described in section 2, rather than a
  world-gated overworld map. **inferred**, stitched from https://themushroomkingdom.net/manuals/smbdx.txt
  and https://www.mariowiki.com/Super_Mario_Bros_Deluxe — exact screen layout is **must-verify**.

### Save system
- Original 1985 mode: 3 save file slots. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Super Players (Lost Levels) mode: only 1 save slot, separate from Original mode's 3.
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- The game can be saved at any time from the Pause screen (manual explicitly restricts this to
  Original mode — Challenge/VS/Boo modes are not stated to be independently saveable mid-run).
  **documented** — https://themushroomkingdom.net/manuals/smbdx.txt and https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Save granularity: the game saves the specific current level the player is on, not just the
  current world (a difference from how one might expect a coarser world-only checkpoint to work).
  **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Region difference in what's saved: the Japanese version additionally preserves Mario/Luigi's
  current power-up form (small/big/fire) and current score across a save/reload. In the English
  (and by extension presumably PAL) version, reloading a save resets the player to Small form and
  resets score to zero even though level progress is kept. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Underlying storage: SRAM-backed save (cartridge is "ROM + MBC5 + RAM + Battery", 8 KiB SRAM per
  Data Crystal's board-level ROM info). **documented (community reverse-engineering / mapper ID)**
  — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe

### Continue / game-over rules vs. NES
- Mid-level restart point: per the manual, once Mario is roughly halfway through a level, dying
  restarts him from that midpoint rather than the level start; the first half has no such
  restart-from-middle. Castle levels and all of World 8 always restart from the very beginning
  regardless of progress. **documented** — https://themushroomkingdom.net/manuals/smbdx.txt.
  Note: NES SMB1 itself already has this same implicit halfway checkpoint behavior, so this is
  *not* a Deluxe-introduced change — flagging so we don't misattribute it as new. **documented**
  (general Mario-series checkpoint knowledge, not Deluxe-specific) —
  https://www.mariowiki.com/Checkpoint
- Lives: starts at 5 (vs NES's 3), max 127, as noted in section 2. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Exact Game Over / continue behavior (e.g., does running out of lives in Original mode return to
  title, or offer a continue prompt like NES's "Continue?" screen) is **must-verify** — no source
  found explicitly describing this for SMBD specifically.

### Score / high-score handling
- Two linked players can view and exchange each other's high scores via GBC infrared, as noted
  above. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Challenge Mode uses a separate scoring ruleset from Original mode (see section 2 point-value
  overrides). **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Whether Original-mode score persists as a single running high-score record across save slots, or
  is per-slot, is **must-verify**.

---

## 4. Level differences vs. NES SMB1 (documented list)

- General framing: SMBD is described as remaining very faithful to NES SMB1 with the exception of
  the added overworld map, Challenge Mode, "somewhat iffy physics," and heavy "screen crunch."
  **documented** — https://tcrf.net/Super_Mario_Bros._Deluxe (via search snippet)
- Physics tightened versus NES: the manual/wiki note the widest gap in World 8-1 (the one
  requiring a running long jump) can now be crossed without needing to grab the coins floating
  above the gap (implying either jump arc or gap width was adjusted). **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Flagpole placement changed: the end-of-level flagpole sits 7 blocks from the end of the final
  staircase, instead of NES's 8 blocks. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- End-of-level staircase geometry: the staircase is one block shorter and narrower than the NES
  version. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Bug/glitch fixes: numerous NES-era glitches were patched, explicitly including the famous Minus
  World (World -1) glitch, which cannot be triggered in SMBD. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Visual-only changes carried at the tile/palette level (not layout, but still a documented
  per-asset difference): Fire Flower has a slightly different color palette; water in ground-type
  courses and lava are now animated (they were static on NES); Princess Toadstool and the Mushroom
  Retainers have idle talking animations in their end-of-castle cutscenes; Luigi's palette was
  changed to mirror Mario's palette but in green (both normal and Fire forms), rather than Luigi's
  original distinct NES palette. **documented** — https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- Added sound effects for actions that were silent on NES: jumping/spring board bounces, Lakitu's
  egg-throw, Mario's skid sound, and Cheep-cheep splash/jump noises. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- World 1-3 is specifically called out as gaining extra difficulty purely from the cropped
  viewport (screen crunch), not from an intentional redesign — i.e., the level geometry may be the
  same as NES but is harder to play because less of it is visible at once. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe
- No source found enumerating a complete, level-by-level list of geometry changes (removed rooms,
  shortened vertical shafts, moved platforms, etc.) beyond the World 8-1 gap and staircase/flagpole
  changes above. TCRF's Notes page is reported (via search) to contain "a full list of differences
  and GameShark codes to access them," but the page could not be fetched directly (connection
  blocked in this environment — tcrf.net requests returned `ECONNREFUSED` for both `https://` and
  `http://`, and web.archive.org fetches are also blocked for this session). **must-verify** — the
  page to check when direct fetch is available: https://tcrf.net/Notes:Super_Mario_Bros._Deluxe
  (also see https://tcrf.net/Prerelease:Super_Mario_Bros._Deluxe for early/beta layout diffs).

---

## 5. GBC technical facts (engine-relevant)

- Cartridge/mapper: ROM + MBC5 + RAM + Battery. **documented (community ROM-header
  reverse-engineering)** — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe
- ROM size: 1 MiB. **documented (ROM header)** — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe
- SRAM size: 8 KiB. **documented (ROM header)** — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe
- No Super Game Boy (SGB) enhancement support flagged in the header info. **documented (ROM
  header)** — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe
- Multiple ROM revisions exist with distinct header checksums for USA/EUR (0xBFDF, 0xEFB4), EUR
  Rev 2 (0x3C4A), and Japan (0x4392, 0xABBC); internal cart name differs by region ("MARIO
  DELUXAHYEA" USA/EUR vs "MARIO DELUXAHYJA" Japan — accented/special characters approximated here).
  **documented (ROM header dump)** — https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe
- GBC double-speed CPU mode usage: **must-verify**. No source found stating whether SMBD engages
  GBC double-speed mode; searches for this specifically turned up only ROM-download sites, no
  technical writeups. Given it's a 2D platformer ported from an 8-bit NES original, double-speed
  mode is plausible but unconfirmed — do not assume it in our engine without checking the ROM's
  boot/STOP-mode-switch sequence ourselves.
- Level/world count: 32 levels (8 worlds x 4) in Original mode, all 32 reusable in Challenge Mode;
  plus the separate Super Players (Lost Levels) mode; plus unused/incomplete Worlds 9-D data
  present in ROM but not reachable in normal play. **documented** —
  https://www.mariowiki.com/Super_Mario_Bros_Deluxe and https://tcrf.net/Super_Mario_Bros._Deluxe
  (via search snippet)
- Partial community RAM map (reverse-engineered, unofficial — cross-check against our own
  disassembly before relying on exact addresses):
  - `FFA7` (1 byte): level/camera X scroll position — **documented (community)**
  - `FFA9` (1 byte): level/camera Y scroll position — **documented (community)**
  - `FF99` (1 byte): player X position (separate, possibly screen-relative, byte) — **documented (community)**
  - `C1B9`/`C1BA` (1 byte each): player Y/X position, described as "(map)" — likely the overworld
    map-screen position rather than in-level — **documented (community)**, semantics **must-verify**
  - `C1CA` (2 bytes) / `C1CC` (2 bytes): player X/Y position (in-level, full 16-bit) — **documented (community)**
  - `C1C1` (1 byte): player state (ground/air/dead/etc.); also doubles as the byte TCRF's free-move
    cheat pokes to `0x02` — **documented (community)**
  - `C1C2` (1 byte): player pose/animation (standing/walking/jumping/swimming/climbing)  — **documented (community)**
  - `C1C3` (1 byte): facing direction — **documented (community)**
  - `C1C5` (1 byte): growth flag (small/big) — **documented (community)**
  - `C1C7` (1 byte): jump height/velocity — **documented (community)**
  - `C1D5` (1 byte): invulnerability timer — **documented (community)**
  - `C1DA` (2 bytes): star-power timer — **documented (community)**
  - `6000`, 8192 bytes: current level's metatile data — **documented (community)**
  - `C160` (1 byte): active level set (Original SMB vs. Super Players) — **documented (community)**
  - `C162` (1 byte): current level index — **documented (community)**
  - `C100`, 19 bytes: current HUD tile buffer — **documented (community)**
  - `C17A`, 3 bytes: score digits — **documented (community)**
  - `C17D`, 2 bytes: time digits — **documented (community)**
  - `C17F` (1 byte): lives count — **documented (community)**
  - `C180` (1 byte): countdown timer value — **documented (community)**
  - `C181` (1 byte): timer-disabled flag — **documented (community)**
  - `C1F2` (1 byte): coin count — **documented (community)**
  - `D000`-`D00F`, repeating 15/16-byte blocks: per-sprite state/ID slots — **documented (community)**
  - `FFB5` (1 byte): current game mode — **documented (community)**
  - Source for the whole RAM map table: https://datacrystal.tcrf.net/wiki/Super_Mario_Bros._Deluxe/RAM_map
    — treat all addresses as **must-verify** against our own ROM/emulator trace before hardcoding
    any equivalent logic; this is fan reverse-engineering, not an official Nintendo source.
- Scrolling engine internals (tile compression format, how far ahead of the visible window the
  engine buffers tiles, whether it reuses the NES SMB1 nametable-based scroll approach or a custom
  GBC tilemap-window scheme): **must-verify** — no source with this level of detail was found or
  reachable (TCRF's RAM map/ROM map pages that likely have this were not fetchable in this
  session).

---

## 6. Controls (full button map)

Source for this whole table: official manual — https://themushroomkingdom.net/manuals/smbdx.txt,
cross-checked against https://www.mariowiki.com/Super_Mario_Bros_Deluxe. **documented**.

### In a level (Original / Challenge / Boo / VS)
| Button | Action |
|---|---|
| D-pad Left/Right | Move Mario/Luigi horizontally |
| D-pad Down | Duck (as Super/Fire form); enter a pipe when standing over one |
| D-pad Up | Enter a pipe when applicable; otherwise pans the camera view upward while stationary |
| D-pad Down (held, stationary) | Pans the camera view downward |
| A | Jump / swim stroke (hold longer for a higher jump, as in NES SMB1) |
| B | Run (hold) / throw a fireball (as Fire Mario/Luigi, tap) |
| SELECT | Shift Mario's on-screen position to look/scroll further ahead |
| START | Pause the game; from Pause, access Save (Original mode) |

### World map / menus
| Button | Action |
|---|---|
| D-pad | Move the cursor / navigate the map or menu |
| SELECT | On the world map: swap the active playable character between Mario and Luigi |
| START | Confirm / enter a level, or resume from Pause |
| A | Confirm selection in most menu contexts (**inferred** by convention, not explicitly spelled out per-menu in the sources found — **must-verify** per-screen) |

---

## Summary of the ten most engine-shaping facts

1. Camera and player position are tracked as separate state (community RAM map: level scroll at
   `FFA7`/`FFA9` vs. player position at `C1CA`/`C1CC`), which is the mechanism that makes manual
   pan-up/down and SELECT-to-look-ahead possible without moving Mario. **documented (community)**
2. Vertical pan (D-pad Up/Down while stationary) and horizontal look-ahead (SELECT) are two
   distinct, manually-triggered camera controls — this is not an automatic dynamic camera, the
   player must invoke it, and reviewers called the vertical version awkward specifically during
   jumps over vertical platforming. **documented**
3. Unlike NES SMB1's hard "no backtrack" scroll lock, SMBD allows scrolling backward a limited
   distance within a level, to see/reach content hidden by the cropped viewport. **documented**
4. HUD is reduced to score/coins/time-only during play; lives and level name move to the Pause
   screen — our HUD layout needs a Pause-screen state, not just an in-level HUD.
5. Save system is per-level (not per-world) granularity, 3 slots for Original mode + 1 separate
   slot for Lost Levels mode, SRAM-backed (8 KiB, MBC5+RAM+Battery cartridge type). **documented**
6. Region-dependent save behavior: EN version resets form/score on reload, JP version preserves
   them — an explicit fork we should pick one behavior for (recommend matching JP behavior unless
   we're specifically emulating US ROM quirks). **documented**
7. Challenge Mode is a full alternate ruleset over the same 32 levels: different scoring table
   (2,000 for 1-ups, 10,000 for chain-kill 1-up substitution, 10,000 Perfect Bonus), plus 3
   independent per-level medal conditions (5 red coins, 1 hidden egg, target score). **documented**
8. Two extra single-purpose modes reuse the same 8 (or level-subset) courses with modified rules:
   You vs. Boo (ghost pacer keyed to player's best time) and VS Game (link-cable race with
   toggleable blocks/spikes) — both need a "second dynamic actor on the same course" system.
   **documented**
9. Documented, concrete level-geometry diffs vs. NES are narrow: 7-block vs 8-block flagpole
   offset, shorter/narrower end staircase, and a resized/repositioned World 8-1 long-jump gap. No
   comprehensive per-level geometry diff list was reachable this session (TCRF Notes page blocked)
   — treat full level parity with NES as the working assumption except at these three confirmed
   points, and re-verify against TCRF's Notes page and our own emulator once reachable.
   **documented for the three specifics; must-verify for completeness**
10. GBC technical unknowns that directly affect our core engine design: whether SMBD runs GBC
    double-speed mode is **unconfirmed** (must-verify by inspecting the ROM's speed-switch STOP
    sequence ourselves), and the actual tile/scroll buffering scheme (how the engine manages the
    off-screen tile window for the pan-ahead/pan-up features) is **undocumented** anywhere we could
    reach — this is the single biggest gap and should be answered by disassembling the real ROM
    rather than guessed at.
