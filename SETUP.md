# Setup

Quick guide to get gbemu running on Windows (see below for MacOS/Linux).

## 1. Install four things

Each of these is a normal download, either an installer or a zip you unpack
anywhere. The examples below use `C:\tools`.

- **[git](https://git-scm.com/downloads).** Run the installer, defaults are
  fine.
- **[CMake](https://cmake.org/download/).** Run the installer. When asked
  about PATH, choose "Add CMake to the system PATH".
- **[Ninja](https://github.com/ninja-build/ninja/releases).** Download
  `ninja-win.zip`, unzip it anywhere, and put that folder on your PATH (see
  below). It's just one file, `ninja.exe`.
- **[winlibs mingw-w64](https://winlibs.com/).** The compiler. Download a
  zip release, unzip it to `C:\tools\mingw64`, and put its `bin` folder on
  your PATH.
- **SDL2 development package.** Lets the emulator open a window. Download
  the "mingw" devel zip from the
  [SDL releases page](https://github.com/libsdl-org/SDL/releases) and
  unzip it to `C:\tools\SDL2-2.32.10`. No PATH needed, just remember the
  folder.
- **[gbdk-2020](https://github.com/gbdk-2020/gbdk-2020/releases).**
  Optional, only needed if you want to change the Flappy Bird or Crossy
  Road game code. Skip it if you just want to play: ready-made game files
  are already in the repo (`assets/roms/`).

**Adding a folder to PATH:** press the Windows key, type `env`, open "Edit
environment variables for your account", pick `Path` under your user
variables, click Edit, then New, paste the folder path, then OK everywhere.

## 2. Get the code

Open PowerShell and run the following command where you want to store the source code:

```powershell
git clone https://github.com/cameronjim/gameboy-emulator.git
cd gameboy-emulator
```

## 3. The easy path

Open PowerShell inside the `gameboy-emulator` folder and run:

```powershell
powershell -File tools/make-dist.ps1
```

If your SDL2 folder isn't where the script expects it, point it there:

```powershell
powershell -File tools/make-dist.ps1 -Sdl2Prefix "C:\tools\SDL2-2.32.10\x86_64-w64-mingw32"
```

This one command:

- Builds the emulator. If you installed gbdk-2020 it also rebuilds the
  games from source. Otherwise it uses the game files already in the repo.
- Fills a `dist` folder with the emulator, all three games (`tetris.gb`,
  `flappy.gb`, `crossy.gb`), and a launcher for each one.
- Creates Start Menu shortcuts for all three games.

Afterward, type **tetris**, **flappy**, or **crossy** in the Windows search
bar to start that game. Drag any other `.gb` file onto
`dist\gbemu-sdl.exe` to play it.

## 4. Building by hand

For developers who want to run the build steps themselves instead of the
script above.

Windows (PowerShell, Ninja, mingw):

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
    -DCMAKE_PREFIX_PATH="C:\tools\SDL2-2.32.10\x86_64-w64-mingw32"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Linux/macOS (install SDL2 from your package manager first, e.g.
`sudo apt-get install libsdl2-dev` or `brew install sdl2`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build --output-on-failure
```

No SDL2 found skips the `gbemu-sdl` window (the core library and tests
still build). No `GBDK_HOME` skips rebuilding Flappy Bird and Crossy Road.
To rebuild just one game, add `-DGBDK_HOME="C:\tools\gbdk"` at configure
time, then run `cmake --build build --target flappy` (swap in `crossy` for
the other game). The finished rom lands at `build/flappy.gb` (or
`build/crossy.gb`).

## 5. Optional

Tetris: swap the piece editor's spin-direction glyphs for a plain check
mark and X:

```
python tools/patch_spin_icons.py dist/tetris.gb
```
