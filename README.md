# gbemu

A Game Boy (DMG) emulator made from scratch, with three playable games: Tetris, Flappy Bird, and Crossy Road.

**First time here?** Follow [SETUP.md](SETUP.md): it covers cloning,
building, and a one-command installer that sets everything below up.

## Running the games

- Type **tetris**, **flappy**, or **crossy** in the Windows search bar **OR** see below for terminal instructions
- Mac / Linux: after `./tools/make-dist.sh`, type **tetris**, **flappy**, or **crossy** in a terminal (or run `./dist/tetris`).
- Any other `.gb` file: drag it onto `dist\gbemu-sdl.exe`.

If the search bar isn't returning any results, run any the following inside of the /dist directory:
```bash
tetris.cmd
```
```bash
flappy.cmd
```
```bash
crossy.cmd
```

## Controls

| Key | Game Boy button |
| --- | --- |
| Arrows or `WASD` | D-pad |
| `F` or `Space` (or `Z`) | A |
| `G` (or `X`) | B |
| `E` or `Enter` | Start |
| `R` (or `Backspace`) | Select |
| `Esc` | Back / pause menu |

A gamepad works too, plug and play, no setup needed: d-pad or left stick moves, A is A,
B or X is B, Menu is Start, View is Select, and the right trigger fast-forwards.

## Emulator keys

- `P` - pause. Hold `Tab` - fast-forward.
- `F5` / `F8` - save / load state.
- `F10` - screenshot (`framebuffer.ppm`).
- `T` - tile viewer. Resize the window freely.

## Tuning

- Volume: run from a terminal with `--volume 0-100` (default 25), ex:
```bash
`dist\gbemu-sdl.exe --volume 60 dist\flappy.gb`
```
- Battery saves (`.sav`) and save states (`.state`) live next to each rom and
  are automatic - delete one to reset that game.
- Window icon: a game named `<rom>.icon.bmp` next to its rom overrides the
  default `gbemu.bmp`.

## Credits

The included Tetris is based on [Adjustris](https://github.com/tbsp/Adjustris)
by Dave VanEe (tbsp), public domain, vendored at `assets/roms/tetris.gb`.
Flappy Bird
and Crossy Road are this repo's own gbdk-2020 roms (`games/`).
