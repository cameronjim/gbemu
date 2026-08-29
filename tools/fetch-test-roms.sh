#!/usr/bin/env bash
# downloads test roms (free homebrew) into tests/roms/vendor, gitignored
set -euo pipefail
cd "$(dirname "$0")/.."
vendor=tests/roms/vendor
mkdir -p "$vendor"

if [ ! -d "$vendor/gb-test-roms" ]; then
    git clone -q --depth 1 https://github.com/retrio/gb-test-roms "$vendor/gb-test-roms"
    echo "fetched blargg (gb-test-roms)"
fi

if [ ! -d "$vendor/mooneye" ]; then
    mts=mts-20240127-1204-74ae166
    curl -sfL "https://gekkio.fi/files/mooneye-test-suite/$mts/$mts.zip" -o "$vendor/mts.zip"
    unzip -q "$vendor/mts.zip" -d "$vendor"
    mv "$vendor/$mts" "$vendor/mooneye"
    rm "$vendor/mts.zip"
    echo "fetched mooneye test suite"
fi

if [ ! -f "$vendor/dmg-acid2.gb" ]; then
    curl -sfL "https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb" \
        -o "$vendor/dmg-acid2.gb"
    echo "fetched dmg-acid2"
fi

if [ ! -f "$vendor/cgb-acid2.gbc" ]; then
    curl -sfL "https://github.com/mattcurrie/cgb-acid2/releases/download/v1.1/cgb-acid2.gbc" \
        -o "$vendor/cgb-acid2.gbc"
    echo "fetched cgb-acid2"
fi

# the vendored tetris doubles as the demo rom
if [ ! -f "$vendor/demo.gb" ]; then
    cp assets/roms/tetris.gb "$vendor/demo.gb"
    echo "copied tetris demo"
fi

echo "test roms ready in $vendor"
