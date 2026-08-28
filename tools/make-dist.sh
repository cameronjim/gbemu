#!/usr/bin/env bash
# builds gbemu-sdl (+ flappy/crossy roms) and assembles a dist folder on linux/macos.
# the counterpart of make-dist.ps1. sdl2 is linked against the system copy, so
# nothing is bundled here.
set -euo pipefail

# repo root is the parent of this script's directory (tools/..)
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

gbdk_home=${GBDK_HOME:-}
build_dir=build-rel
dist_dir=dist
no_install=0

step() {
    printf '==> %s\n' "$1"
}

warn() {
    printf 'warning: %s\n' "$1" >&2
}

fail() {
    printf 'error: %s\n' "$1" >&2
    exit 1
}

usage() {
    cat <<'EOF'
usage: tools/make-dist.sh [options]

  --gbdk-home PATH   gbdk-2020 install, optional (also read from $GBDK_HOME)
  --build-dir PATH   build directory (default: build-rel)
  --dist-dir PATH    output directory (default: dist)
  --no-install       do not symlink the launchers into ~/.local/bin
  -h, --help         show this help
EOF
}

# --- options ---
while [ $# -gt 0 ]; do
    case "$1" in
        --gbdk-home)
            [ $# -ge 2 ] || fail "--gbdk-home needs a path"
            gbdk_home=$2
            shift 2
            ;;
        --gbdk-home=*)
            gbdk_home=${1#*=}
            shift
            ;;
        --build-dir)
            [ $# -ge 2 ] || fail "--build-dir needs a path"
            build_dir=$2
            shift 2
            ;;
        --build-dir=*)
            build_dir=${1#*=}
            shift
            ;;
        --dist-dir)
            [ $# -ge 2 ] || fail "--dist-dir needs a path"
            dist_dir=$2
            shift 2
            ;;
        --dist-dir=*)
            dist_dir=${1#*=}
            shift
            ;;
        --no-install)
            no_install=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            fail "unknown option: $1"
            ;;
    esac
done

command -v cmake > /dev/null 2>&1 || fail "cmake not found on PATH"

# gbdk is optional; without it we play from the vendored roms
have_gbdk=0
if [ -n "$gbdk_home" ] && [ -x "$gbdk_home/bin/lcc" ]; then
    have_gbdk=1
else
    if [ -n "$gbdk_home" ]; then
        warn "no lcc under $gbdk_home; using the vendored roms"
    else
        warn "gbdk not found (set GBDK_HOME or pass --gbdk-home); using the vendored roms"
    fi
fi

# --- configure ---
if [ -f "$build_dir/CMakeCache.txt" ]; then
    step "reusing existing configure in $build_dir"
else
    step "configuring $build_dir (release)"
    if [ "$have_gbdk" -eq 1 ]; then
        cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DGBDK_HOME="$gbdk_home" ||
            fail "cmake configure failed"
    else
        cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Release || fail "cmake configure failed"
    fi
fi

# --- build ---
if [ "$have_gbdk" -eq 1 ]; then
    step "building gbemu-sdl, flappy, crossy"
    cmake --build "$build_dir" --target gbemu-sdl flappy crossy -j || fail "build failed"
else
    step "building gbemu-sdl"
    cmake --build "$build_dir" --target gbemu-sdl -j || fail "build failed"
fi

emu_src="$build_dir/gbemu-sdl"
[ -x "$emu_src" ] || fail "expected build output missing: $emu_src (is sdl2 installed?)"

if [ "$have_gbdk" -eq 1 ]; then
    flappy_src="$build_dir/flappy.gb"
    crossy_src="$build_dir/crossy.gb"
    [ -f "$flappy_src" ] || fail "expected build output missing: $flappy_src"
    [ -f "$crossy_src" ] || fail "expected build output missing: $crossy_src"
else
    # no gbdk build; play from the committed roms instead
    flappy_src="assets/roms/flappy.gb"
    crossy_src="assets/roms/crossy.gb"
    [ -f "$flappy_src" ] || fail "expected vendored rom missing: $flappy_src"
    [ -f "$crossy_src" ] || fail "expected vendored rom missing: $crossy_src"
fi

# --- dist dir ---
if [ ! -d "$dist_dir" ]; then
    step "creating $dist_dir"
    mkdir -p "$dist_dir"
fi

# copy through a temp name so a running emulator does not block the replace
install_file() {
    local src=$1 dest=$2
    cp -f "$src" "$dest.new"
    mv -f "$dest.new" "$dest"
}

step "copying emulator, roms, icon"
install_file "$emu_src" "$dist_dir/gbemu-sdl"
chmod +x "$dist_dir/gbemu-sdl"
install_file "$flappy_src" "$dist_dir/flappy.gb"
install_file "$crossy_src" "$dist_dir/crossy.gb"

# gbdk built fresh roms; keep the committed copies in sync with the sources
if [ "$have_gbdk" -eq 1 ]; then
    step "refreshing vendored roms in assets/roms from this build"
    install_file "$flappy_src" assets/roms/flappy.gb
    install_file "$crossy_src" assets/roms/crossy.gb
fi

if [ -f assets/icons/gbemu.bmp ]; then
    install_file assets/icons/gbemu.bmp "$dist_dir/gbemu.bmp"
else
    warn "assets/icons/gbemu.bmp not found, skipping window icon"
fi

# --- tetris.gb ---
# saves and states live next to the roms, so an existing tetris.gb is left alone
if [ -f "$dist_dir/tetris.gb" ]; then
    step "tetris.gb already present in dist, leaving it alone"
else
    step "copying assets/roms/tetris.gb into dist"
    [ -f assets/roms/tetris.gb ] || fail "expected vendored rom missing: assets/roms/tetris.gb"
    install_file assets/roms/tetris.gb "$dist_dir/tetris.gb"
fi

# --- launcher scripts ---
step "writing launcher scripts"
for game in tetris flappy crossy; do
    cat > "$dist_dir/$game" <<EOF
#!/usr/bin/env bash
# runs $game.gb with the emulator sitting next to this script
set -euo pipefail
# follow symlinks so the launcher works from ~/.local/bin too
src=\${BASH_SOURCE[0]}
while [ -L "\$src" ]; do
    link_dir=\$(cd -P "\$(dirname "\$src")" && pwd)
    src=\$(readlink "\$src")
    case "\$src" in
        /*) ;;
        *) src="\$link_dir/\$src" ;;
    esac
done
here=\$(cd -P "\$(dirname "\$src")" && pwd)
exec "\$here/gbemu-sdl" "\$here/$game.gb" "\$@"
EOF
    chmod +x "$dist_dir/$game"
done

dist_abs=$(cd "$dist_dir" && pwd)

# --- install into ~/.local/bin ---
if [ "$no_install" -eq 1 ]; then
    step "skipping ~/.local/bin symlinks (--no-install)"
else
    bin_dir="$HOME/.local/bin"
    step "linking launchers into $bin_dir"
    mkdir -p "$bin_dir"
    for game in tetris flappy crossy; do
        ln -sf "$dist_abs/$game" "$bin_dir/$game"
    done
    case ":${PATH:-}:" in
        *":$bin_dir:"*)
            printf 'type tetris, flappy, or crossy in a terminal to play.\n'
            ;;
        *)
            printf '%s is not on your PATH yet.\n' "$bin_dir"
            printf 'add this line to ~/.zshrc (or ~/.bashrc), then open a new terminal:\n'
            printf '    export PATH="$HOME/.local/bin:$PATH"\n'
            printf 'until then, run the games as %s/tetris.\n' "$dist_dir"
            ;;
    esac
fi

printf '\ndone. dist folder: %s\n' "$dist_abs"
