# gbemu

A from-scratch Game Boy (DMG) emulator, with three playable games: Tetris, Flappy Bird, and Crossy Road.

**First time here?** Follow [SETUP.md](SETUP.md) — it covers cloning,
building, and a one-command installer that sets everything below up.

## Running the games

- Type **tetris**, **flappy**, or **crossy** in the Windows search bar (or run
  the matching `.cmd` from a terminal).
- Any other `.gb` file: drag it onto `dist\gbemu-sdl.exe`.

## Controls

| Key | Game Boy button |
| --- | --- |
| Arrows or `WASD` | D-pad |
| `F` or `Space` (or `Z`) | A |
| `G` (or `X`) | B |
| `E` or `Enter` | Start |
| `R` (or `Backspace`) | Select |
| `Esc` | Back / pause menu |

## Emulator keys

- `P` — pause. Hold `Tab` — fast-forward.
- `F5` / `F8` — save / load state. `F10` — screenshot (`framebuffer.ppm`).
- `T` — tile viewer. Resize the window freely.

## Tuning

- Volume: run from a terminal with `--volume 0-100` (default 25), e.g.
  `dist\gbemu-sdl.exe --volume 60 dist\flappy.gb`.
- Battery saves (`.sav`) and save states (`.state`) live next to each rom and
  are automatic — delete one to reset that game.
- Window icon: a game named `<rom>.icon.bmp` next to its rom overrides the
  default `gbemu.bmp`.

## Credits

Tetris is [Adjustris](https://github.com/tbsp/Adjustris) by Dave VanEe
(tbsp), public domain, vendored at `assets/roms/adjustris.gb`. Flappy Bird
and Crossy Road are this repo's own gbdk-2020 roms (`games/`).
