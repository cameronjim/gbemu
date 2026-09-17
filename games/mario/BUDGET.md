# rom bank and frame budget

purpose: what lives in each 16 KB bank of mario.gbc and why, what must stay in bank 0, where new
work lands, and how the build and the tests catch a bank or a frame going over. numbers are from
`bank_report.py` on the build this doc shipped with; the build prints the current ones.

## how the budget is enforced

- the mario build runs `games/mario/tools/bank_report.py build-rel/mario.map --banks 16` after
  the link and fails on any bank past 16 KB. lcc only warns ("Possible overflow from Bank N") and
  links a truncated rom, whose symptom is dozens of unrelated test failures.
- the map is sdld's wide `.map` (`-Wl-m -Wl-w`), the `.noi` beside it (`-Wl-j`) is how the host
  tests find rom globals by name.
- `bank_report.py mario.map --symbols 10` lists a bank's biggest global symbols. statics fold
  into the global before them, so treat those sizes as "this and what follows it".
- the frame budget: main.c counts `frame_tick` once per pass round its loop; a picture with no
  new count is a dropped frame. `mario_world_one_play_frames_never_drop` replays routes through
  1-1 to 1-4 and requires zero, counted only while mario is drawn (a card's lcd-off rebuild is
  meant to outrun a frame). `replay_matched` reports the same number as `Match::ticks_missed`.

## the cart

- mbc5 + ram + battery, cgb only, 16 rom banks (256 KB, `-Wm-yo16`), 8 KB sram. mbc5 addresses
  up to 512 banks and the emulator's mapper masks by bank count, so growing again is the one flag.
- cpu runs at double speed (`cpu_fast()` in main.c): the merged play frame overran single speed on
  heavy jump frames before that, and a dropped frame desyncs every scripted host test.

## bank 0: the home bank, 15155 of 16384 used

bank 0 cannot grow. it is the fixed window at 0x0000 to 0x3fff. the only relief is moving code and
data into a switched bank behind a `BANKED` entry point, which costs a bank switch per call.

what is there and why:

- 0x0000 to 0x01ff: rst and interrupt vectors, the cart header.
- player.c (4559): the physics step and the terrain probes under it run every play frame.
- blocks.c (3620): solidity overrides, head bumps and item state, probed by the streamer and the
  player every frame.
- terrain.c (2252): the ring streamer, scroll and the lyc split. feeds every frame.
- main.c (1323): the play loop and `main_present`, which orders the frame's vram traffic.
- the sound hooks (about 270 across player, blocks, main): each is a `BANKED` call into bank 8,
  queued on the frame of the event and played at the top of the next.
- romcopy.c (49): `rom_copy`, the one bank switch in the engine. it must be in bank 0 because the
  code that switches banks has to still be mapped after the switch.
- gbdk's `_HOME` (2068): crt0, `memcpy`, the console and font code, and `font_ibm` (993). gbdk's
  font module is unbanked and title.c and mapscreen.c print through it (`gotoxy`, `putchar`), so
  the ibm font stays until those screens draw text from their own tiles.
- sdcc runtime (about 466): divmod, mul, the cgb helpers.

what must stay hot (do not move): `player_update` and its probes, `terrain_stream_window` and the
column streamer, `blocks_update` and the head bump path, `main_present`.

candidates to move next, largest first:

- the ibm font and console (about 1.5 KB): draw the title, file select and map text from bank 7
  tiles and drop `font_init`/`font_load`.
- player.c's off-play states (about 1 KB: the clear slide, the pipe animations, the death arc)
  behind a bank 6 pass like player_draw.c, reading player.c through accessors.
- `blocks_load_level` and `blocks_enter_area` (about 300 bytes): once a level, not once a frame.

## the switched banks

| bank | holds | used |
|---|---|---|
| 1 | 1-1 grid and lists | 3669 |
| 2 | 1-2 grid and lists | 4242 |
| 3 | hazards.c, 1-3 grid and lists | 8205 |
| 4 | enemies.c, assets.c and every ripped tile array | 16215 (full) |
| 5 | flow.c, powerup.c, title.c, hud.c, save.c, mapscreen.c, camera.c, level.c | 11713 |
| 6 | the draw passes (player_draw, blocks_draw, debris, popup), states.c, pause.c, toad.c, castle art, 1-4 grid | 8866 |
| 7 | title, file select and map screens, the level table (`kLevels`), sub-area grids | 12303 |
| 8 | sound.c: smb's sound engine and its music data, `gen/smb_audio_data.h` | 4415 |
| 9 to 15 | empty | 0 |

rules:

- new features land in banks 5, 6 or 9 and up, never bank 0. a new tile family goes in a bank
  with its own `set_bkg_data` loader, called through a `BANKED` function (castle_art.c is the
  pattern); bank 4 is full.
- a level's lists (blocks, enemies, objects, jumps, segments, area coins and warps) compile into
  the level's own bank. level.c copies the entry being played into ram at select, its lists into
  a ram arena sized to the largest level, so the per-frame scans read ram and never switch a bank.
  a longer list therefore costs every level wram, not bank 0.
- a module that needs more than one bank switch stays in bank 0 or goes through `rom_copy`.

## wram

6396 of 8192 used, 1796 free. the big residents are `level_grid` (3984) and the level arena
(about 450). the arena grows with `LEVEL_MAX_*` in levels.h.
