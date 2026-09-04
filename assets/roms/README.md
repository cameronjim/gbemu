# vendored roms

- `tetris.gb` — built from this repo's own `games/tetris` with gbdk-2020, and
  also the wasm demo rom. Committed so playing needs no gbdk toolchain.
  `tools/make-dist.ps1` (and `tools/make-dist.sh`) refreshes this copy whenever
  it builds with gbdk available, keeping it in sync with the source.
- `flappy.gb` — built from this repo's own `games/flappy` with gbdk-2020.
  Committed so playing needs no gbdk toolchain. `tools/make-dist.ps1` refreshes
  this copy whenever it builds with gbdk available, keeping it in sync with
  the source.
- `crossy.gb` — built from this repo's own `games/crossy` with gbdk-2020.
  Committed for the same reason as `flappy.gb`, and refreshed the same way.
- `mario.gbc` — built from this repo's own `games/mario` with gbdk-2020. A
  Game Boy Color rom (mbc5+ram+battery), committed and refreshed the same way
  as the others.
