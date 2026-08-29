#!/usr/bin/env bash
# downloads test roms (free homebrew) into tests/roms/vendor, gitignored
set -euo pipefail
cd "$(dirname "$0")/.."
vendor=tests/roms/vendor
mkdir -p "$vendor"

# ci runners occasionally drop dns for a moment; retry instead of failing the job
fetch() {
    curl -sfL --retry 5 --retry-all-errors --retry-delay 3 "$1" -o "$2"
}

if [ ! -d "$vendor/gb-test-roms" ]; then
    for attempt in 1 2 3; do
        if git clone -q --depth 1 https://github.com/retrio/gb-test-roms "$vendor/gb-test-roms"; then
            break
        fi
        rm -rf "$vendor/gb-test-roms"
        [ "$attempt" = 3 ] && exit 128
        sleep 5
    done
    echo "fetched blargg (gb-test-roms)"
fi

if [ ! -d "$vendor/mooneye" ]; then
    mts=mts-20240127-1204-74ae166
    fetch "https://gekkio.fi/files/mooneye-test-suite/$mts/$mts.zip" "$vendor/mts.zip"
    unzip -q "$vendor/mts.zip" -d "$vendor"
    mv "$vendor/$mts" "$vendor/mooneye"
    rm "$vendor/mts.zip"
    echo "fetched mooneye test suite"
fi

if [ ! -f "$vendor/dmg-acid2.gb" ]; then
    fetch "https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb" \
        "$vendor/dmg-acid2.gb"
    echo "fetched dmg-acid2"
fi

if [ ! -f "$vendor/cgb-acid2.gbc" ]; then
    fetch "https://github.com/mattcurrie/cgb-acid2/releases/download/v1.1/cgb-acid2.gbc" \
        "$vendor/cgb-acid2.gbc"
    echo "fetched cgb-acid2"
fi

# the vendored tetris doubles as the demo rom
if [ ! -f "$vendor/demo.gb" ]; then
    cp assets/roms/tetris.gb "$vendor/demo.gb"
    echo "copied tetris demo"
fi

echo "test roms ready in $vendor"
