# vendored roms

- `adjustris.gb` — [Adjustris v1.1](https://github.com/tbsp/Adjustris) by Dave
  VanEe (tbsp), released to the public domain. Shipped as the repo's playable
  Tetris (`tetris.gb` in dist) and as the wasm demo rom. Unmodified release
  binary; `tools/patch_spin_icons.py` can optionally swap its piece-editor
  glyphs.
- `flappy.gb` — built from this repo's own `games/flappy` with gbdk-2020.
  Committed so playing needs no gbdk toolchain. `tools/make-dist.ps1` refreshes
  this copy whenever it builds with gbdk available, keeping it in sync with
  the source.
- `crossy.gb` — built from this repo's own `games/crossy` with gbdk-2020.
  Committed for the same reason as `flappy.gb`, and refreshed the same way.
