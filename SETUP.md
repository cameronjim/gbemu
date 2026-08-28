# Setup

How to get gbemu building and running from a clean machine, plus how to build
the included games. This doc is Windows-first (that's where the shipped
`dist/` app comes from) with a short Linux/macOS note where things differ.

## 1. Prerequisites

- **git**
- **CMake >= 3.24**
- **Ninja**
- **A C++20 compiler.** On Windows, [winlibs mingw-w64](https://winlibs.com/)
  is the easiest route (a plain g++ that doesn't need Visual Studio). Unpack
  it somewhere like `C:\tools\mingw64` and either add its `bin` folder to
  your `PATH` or pass the full compiler path to CMake.
- **SDL2 development libraries.** Without these you still get the core
  library and unit tests, just not the playable emulator window. On Windows,
  grab the "mingw" devel package from [libsdl.org](https://www.libsdl.org/)
  and unpack it somewhere like `C:\tools\SDL2-2.32.10`. You'll point CMake
  at the architecture subfolder inside it, e.g.
  `C:\tools\SDL2-2.32.10\x86_64-w64-mingw32`.
- **[gbdk-2020](https://github.com/gbdk-2020/gbdk-2020)** — optional: only
  needed to rebuild the two homemade game roms (Flappy Bird, Crossy Road)
  from source. Prebuilt copies are committed at `assets/roms/flappy.gb` and
  `assets/roms/crossy.gb`, so playing them needs nothing extra. Unpack a
  release somewhere like `C:\tools\gbdk` if you want to hack on the games.

Nothing above needs to live at a specific path; the paths shown are just
examples used later in this doc (this machine happens to use
`C:\Users\CJ\winlibs\mingw64`, `C:\Users\CJ\opt\SDL2-2.32.10`, and
`C:\Users\CJ\opt\gbdk`).

## 2. Clone the repo

```
git clone https://github.com/cameronjim/gameboy-emulator.git
cd gameboy-emulator
```

## 3. Build the emulator

### Windows (PowerShell, Ninja, mingw)

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
    -DCMAKE_PREFIX_PATH="C:\tools\SDL2-2.32.10\x86_64-w64-mingw32"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If `gcc`/`g++` aren't on your `PATH`, use the full paths instead, e.g.
`-DCMAKE_CXX_COMPILER="C:\tools\mingw64\bin\g++.exe"`.

- **No SDL2 found** (`CMAKE_PREFIX_PATH` missing or wrong): CMake prints
  `sdl2 not found; skipping gbemu-sdl target` and configures anyway — the
  core library (`gbcore`) and unit tests still build and run, you just won't
  get the `gbemu-sdl.exe` frontend.
- **No `GBDK_HOME`**: CMake prints
  `gbdk not found (set GBDK_HOME); skipping flappy/crossy rom targets` and
  skips those two targets and their tests. Everything else builds normally.

### Linux / macOS

Install SDL2 from your package manager (e.g.
`sudo apt-get install libsdl2-dev` or `brew install sdl2`), then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

(This is the same shape CI uses — see `.github/workflows/ci.yml`.)

## 4. Tetris setup

The repo's Tetris is vendored at `assets/roms/tetris.gb` — nothing to
download. (It is Adjustris, a public-domain Tetris-style game by Dave VanEe;
see `assets/roms/README.md` for credits.)

1. Copy `assets/roms/tetris.gb` into your `dist` folder next to
   `gbemu-sdl.exe` (the one-command installer in step 7 does this for you).
2. Optional: run `tools/patch_spin_icons.py` on it. The piece editor
   shows cryptic glyphs for whether a piece spins fully or just wobbles; this
   script swaps them for a plain check mark and a bold X. It edits the rom's
   tile data in place (leaving a `.bak` copy next to it) and works on any
   unpatched copy:
   ```
   python tools/patch_spin_icons.py dist/tetris.gb
   ```

The same vendored rom doubles as the test suite's demo rom
(`tools/fetch-test-roms.sh` copies it to `tests/roms/vendor/demo.gb`) and as
the wasm build's embedded game.

## 5. Flappy Bird setup

Flappy Bird (`games/flappy/`) is a from-scratch rom built with gbdk-2020. A
prebuilt rom is already committed at `assets/roms/flappy.gb` — the steps
below are only needed if you want to hack on the game and rebuild it from
source.

1. Configure with gbdk pointed at your install:
   ```powershell
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
       -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
       -DCMAKE_PREFIX_PATH="C:\tools\SDL2-2.32.10\x86_64-w64-mingw32" `
       -DGBDK_HOME="C:\tools\gbdk"
   ```
2. Build the rom:
   ```
   cmake --build build --target flappy
   ```
   The finished rom lands at `build/flappy.gb`. Copy it into your `dist`
   folder alongside `gbemu-sdl.exe` to play it (or drag it onto the exe).
3. Its emulator-driven tests build automatically alongside it (target
   `flappy_tests`) and run with:
   ```
   ctest --test-dir build -R flappy
   ```
   or directly: `build/flappy_tests.exe`.

## 6. Crossy Road setup

Crossy Road (`games/crossy/`) is the same shape as Flappy Bird — another
from-scratch gbdk-2020 rom, gated on the same `GBDK_HOME`. A prebuilt rom is
already committed at `assets/roms/crossy.gb` — the steps below are only
needed if you want to hack on the game and rebuild it from source.

1. Configure the same way as step 5 (one `-DGBDK_HOME=...` configure builds
   both games; no separate step needed if you've already done it).
2. Build the rom:
   ```
   cmake --build build --target crossy
   ```
   The finished rom lands at `build/crossy.gb`. Copy it into `dist` next to
   `gbemu-sdl.exe` (or drag it onto the exe).
3. Run its tests:
   ```
   ctest --test-dir build -R crossy
   ```
   or directly: `build/crossy_tests.exe`.

## 7. One-command install (Windows)

`tools/make-dist.ps1` does steps 3-6 for you and assembles a ready-to-use
`dist` folder in one go:

```powershell
powershell -File tools/make-dist.ps1
```

What it does:

- Configures (if not already configured) and builds `gbemu-sdl`, `flappy`,
  and `crossy` in Release mode with Ninja/mingw. If `GBDK_HOME` isn't found,
  it warns and builds only `gbemu-sdl`, taking `flappy.gb`/`crossy.gb` from
  the committed `assets/roms/` copies instead — gbdk is not required to
  produce a working `dist`. When gbdk is available, it also refreshes those
  committed copies from the fresh build so they track the sources.
- Creates the `dist` folder if needed, and copies in `gbemu-sdl.exe`,
  `SDL2.dll`, `flappy.gb`, `crossy.gb`, and the window icon
  (`assets/icons/gbemu.bmp`). If a `gbemu-sdl.exe` already exists in `dist`,
  it's kept as `gbemu-sdl.old.exe` (or `.old2.exe`, etc.) rather than
  overwritten — it never touches your `.sav`/`.state` save files.
- Copies the vendored `tetris.gb` in (reusing one already in `dist` if
  present) and writes `tetris.cmd` / `flappy.cmd` / `crossy.cmd` launcher
  scripts into `dist`.
- Creates per-user Start Menu shortcuts named `tetris`, `flappy`, and
  `crossy`, pointing at those launchers.

Useful flags: `-Sdl2Prefix <path>` / `-GbdkHome <path>` if your installs
aren't at the script's defaults, `-DistDir <path>` to build somewhere other
than `dist`, `-NoShortcuts` to skip the Start Menu step, and `-NoTetris` to
skip copying `tetris.gb`.

Afterwards, typing **tetris**, **flappy**, or **crossy** into the Windows
search bar (or running the matching `.cmd` from a terminal) boots that game
like any other installed Windows app. To play any other `.gb` file, just
drag it onto `dist\gbemu-sdl.exe`.
