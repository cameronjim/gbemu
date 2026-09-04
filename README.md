# gbemu

A Game Boy (DMG) emulator made from scratch, with four playable games: Tetris, Flappy Bird, Crossy Road, and Mario. Runs on Windows, Mac, and Linux. Flappy has a course preview title and Mario style colors, Crossy is an endless road crosser, Tetris is a Game Boy Color build with a color-per-piece well, a score/level/lines panel, and a saved best score, and Mario is a Game Boy Color rebuild of World 1 with four levels, powerups, and a boss fight.

**First time here?** Follow [SETUP.md](SETUP.md): it covers cloning,
building, and a one-command installer that sets everything below up.

## Running the games

- Type **tetris**, **flappy**, **crossy**, or **mario** in the Windows search bar **OR** see below for terminal instructions
- Mac / Linux: after `./tools/make-dist.sh`, type **tetris**, **flappy**, **crossy**, or **mario** in a terminal (or run `./dist/tetris`).
- Any other `.gb` file: drag it onto `dist\gbemu-sdl.exe`.
- Plays Game Boy Color games too - drag any `.gbc` file onto the exe.

If the search bar isn't returning any results, run any of the following inside the `dist` directory:
```bash
tetris.cmd
```
```bash
flappy.cmd
```
```bash
crossy.cmd
```
```bash
mario.cmd
```

## Controls

| Key | Game Boy button |
| --- | --- |
| Arrows or `WASD` | D-pad |
| `F` or `Space` (or `Z`) | A |
| `G` (or `X`, `Esc`) | B |
| `E` or `Enter` | Start |
| `R` (or `Backspace`) | Select |

A game controller works too, plug and play, no setup needed: d-pad or left stick moves, A is A,
B or X is B, Menu is Start, View is Select, and the right trigger fast-forwards.

## Emulator keys

- `P` - pause. Hold `Tab` - fast-forward.
- `F5` / `F8` - save / load state.
- `F10` - screenshot (`framebuffer.ppm`).
- `T` - tile viewer. Resize the window freely.

## Tuning

- Volume: run from a terminal with `--volume 0-100` (default 25), ex:
```bash
dist\gbemu-sdl.exe --volume 60 dist\flappy.gb
```
- Battery saves (`.sav`) and save states (`.state`) live next to each rom and
  are automatic - delete one to reset that game.
- Window icon: a game named `<rom>.icon.bmp` next to its rom overrides the
  default `gbemu.bmp`.

## Credits

Tetris, Flappy Bird, Crossy Road, and Mario are all this repo's own gbdk-2020 roms,
written from scratch under `games/` and vendored built at `assets/roms/`.
