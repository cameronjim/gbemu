# audio: smb's sound engine on the game boy apu

purpose: what `src/sound.c` is, where its numbers come from, and which apu facts shaped the port.
every register value traces to `smbdis.asm` (the sound engine at 15045-15960, the data at
15964-16351) or to pandocs' sound chapter. nothing here was tuned by ear.

## what it is

smb1 drives the nes apu from one routine per vblank: three sound-effect handlers (square 1,
square 2, noise), then a music handler that walks four byte streams (square 2, square 1, triangle,
noise) out of a header. `sound.c` is that routine, function for function, in bank 8. the game
queues bits during a frame (`sfx_square1`, `music_area`, ...) and `sound_frame` at the top of the
next loop pass plays them, as smb's nmi does. queue bit names and priorities are smb's own.

`tools/extract_smb_audio.py` reads the disassembly (kept local, never committed) and writes
`src/gen/smb_audio_data.h`: the music streams byte for byte (1352 bytes), the 21 headers with
their data addresses turned into offsets, the 49-entry header index (event bits, area bits, the
ground theme's 33-step layout), the length table, and the sfx tables the handlers index.

## the note table

`FreqRegLookupTbl` holds nes periods. nesdev: square f = 1789773 / (16 (T+1)), triangle f =
1789773 / (32 (T+1)). pandocs: ch1/ch2 f = 131072 / (2048 - x), ch3 f = 65536 / (2048 - x). the
triangle sits an octave under a square with the same T, and ch3 sits an octave under ch1/ch2 with
the same x, so one converted table (`kNotePeriod`) serves every channel. an entry whose low byte is
zero is a rest, because `Dump_Freq_Regs` skips the write on a zero low byte, whatever the high byte.
three sound effects retune by poking only the period's low byte mid-sound (the stomp, the smack,
the coin); those become the `k*SecondPeriod` constants so the rom never converts at runtime.

## register shapes

- **control byte** (nes `$4000`, `DDLC VVVV`): duty bits go to nrx1 as they are. d4 set is a
  constant volume and goes to nrx2 as `V << 4`. d4 clear is a hardware decay at rate n, stepping
  every (n+1)/240 s; the apu envelope steps every p/64 s, so p = round(64 (n+1) / 240), clamped to
  1..7 (`kDecayPeriod`), from full volume.
- **sweep** (nes `$4001`, `EPPP NSSS`): disabled means nr10 = 0. the nes divider runs at 120 hz
  with period P+1, nr10's at 128 hz with period P, so the game boy gets P+1 (clamped to 7). nes
  negate = 1 shortens the period (pitch up); nr10's direction bit set means subtraction (pitch
  down); so the bit is inverted. ch2 has no sweep unit, so square 2's sweeps (the blast, bowser's
  fall) play at a fixed pitch.
- **control-only writes**: smb rewrites `$4000` mid-sound for volume steps (the stomp's envelope
  table, the smack's silent spaces, the jump's three parts). the apu takes a new envelope only on a
  trigger, so those retrigger the note at its current period. the sweep restarts from that period
  too, which is why a test reads the stomp's pitch one sweep step along by the time the picture is
  out.
- **music envelopes**: smb writes a volume every frame from a table indexed by a countdown. one
  apu envelope per note stands in: area themes (`AreaMusicEnvData`, 8 then down to 0 in eight
  frames) are `0x81`; water and event themes (`WaterEventMusEnvData`, near 6 for forty frames)
  are `0x67`; the castle win (`EndOfCastleMusicEnvData`, holds 8) is `0x90`; death and the
  alternate game over skip the table on the nes too and keep the `$82` hardware decay, `0xF1`.
  square 1's death notes carry smb's `$94` sweep (`AltRegContentFlag`), set before the trigger.
- **triangle**: the linear counter reload of `$1f` runs about eight frames, `$0f` four, `$ff`
  holds; nr31 = 223, 239, or length disabled. a zero control byte, and a rest, turn ch3's dac off.
  the wave is a 32-sample triangle written once at init with ch3 off.
- **noise**: `$400E` period indexes map to the nearest nr43 divisor/shift (`kNoiseNr43`, nesdev's
  ntsc period table against pandocs' 524288 / r / 2^(s+1)). `$400F >> 3` indexes the nes length
  table: 3 is two half-frames (one frame), 11 is ten (five); nr41 = 60 or 43 with length enabled,
  unless d5 of the control byte halts it.
- **silence**: smb toggles channels off through `$4015`; the apu equivalent is nrx2 = 0 (dac off)
  and nr30 = 0, which the next note-on undoes.

## flow

- `terrain_sync_palette` (every level, area and segment entry) calls `music_level(type)`, which
  queues the type's theme only when it is not the one already playing, so a card's rebuild does not
  restart it. underground stops square 1's effect first, as `LoadAreaMusic` does.
- death queues `DeathMusic`, which stops the square effects; game over queues `GameOverMusic` when
  the lives run out; the pole slide's end queues `EndOfLevelMusic`; the axe queues `EndOfCastleMusic`
  and the bowser-fall effect; the hurry-up jingle goes out at 100 seconds and hands back to the
  area theme by itself (`AreaMusicBuffer_Alt`); the star swaps the area theme for `StarPowerMusic`
  and `music_level_again` restores it when the timer ends.
- pause runs smb's `InPause`: everything cut, the two-tone jingle, the music held where it was and
  taken up again after the unpause jingle.
- the front screens borrow smb1 pieces, since smb deluxe's own menu tunes are not in the
  disassembly: the title and file select play the ground theme (smb1's attract mode does the
  same), the map plays the coin heaven tune (`Star_CloudHdr`, the star theme's data under
  `CloudMusic`). `music_screen` holds a screen's theme back until a game over jingle or a clear
  fanfare has ended, and `music_level` starts the level theme over when coming in from a screen.

## budget

sound.c and its data are 4393 bytes of bank 8. the hooks cost bank 0 about 270 bytes of banked
call sites. the frame probe (`mario_world_one_play_frames_never_drop`) runs with the engine on
every frame and still sees no dropped picture on 1-1 to 1-4.
