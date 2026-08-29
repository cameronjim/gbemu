# vendored roms

- `tetris.gb` — the repo's playable Tetris, also the wasm demo rom. It is
  [Adjustris v1.1](https://github.com/tbsp/Adjustris) by Dave VanEe (tbsp),
  released to the public domain, carrying this repo's two tile patches: a
  solid block bank (the frontend colorizer fingerprints it to paint the
  pieces) and check/x piece-editor glyphs (`tools/patch_spin_icons.py`).
  a stock adjustris rom still runs, but only some pieces pick up the color
  treatment.
- `flappy.gb` — built from this repo's own `games/flappy` with gbdk-2020.
  Committed so playing needs no gbdk toolchain. `tools/make-dist.ps1` refreshes
  this copy whenever it builds with gbdk available, keeping it in sync with
  the source.
- `crossy.gb` — built from this repo's own `games/crossy` with gbdk-2020.
  Committed for the same reason as `flappy.gb`, and refreshed the same way.
