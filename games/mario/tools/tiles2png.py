#!/usr/bin/env python3
"""dumps a hand-typed 2bpp tile array back out to an editable png, which is how existing art
migrates onto the png pipeline. run as:
tiles2png.py <file.c> <ArrayName> <out.png> --mode tiles|sprites16 [--cols N]

the png is indexed with four greys in 2bpp order, so its palette index IS the pixel value and
png2tiles.py re-encodes it byte for byte.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

DEFAULT_COLS = {"tiles": 16, "sprites16": 8}


def extract_values(path, name):
    """the integers out of a `const <type> kName[...] = { ... };` initializer, comments dropped."""
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    text = re.sub(r"//[^\n]*", "", text)
    m = re.search(r"\b(?:static\s+)?const\s+\w+\s+" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        raise SystemExit("array %s not found in %s" % (name, path))
    end = text.index("}", m.end())
    return [int(tok, 0) for tok in re.findall(r"0[xX][0-9a-fA-F]+|\d+", text[m.end():end])]


def extract_array(path, name):
    return bytes(extract_values(path, name))


def tile_rows(data):
    if len(data) % 16:
        raise SystemExit("array is %d bytes, not a whole number of 8x8 tiles" % len(data))
    return [gbpng.decode_tile(data[i:i + 16]) for i in range(0, len(data), 16)]


def lay_out(tiles, mode, cols):
    # tiles mode packs 8x8 cells cols-per-row; sprites16 packs 8x16 sprites, each one its own
    # upper tile stacked on its lower one, which is the pair order gbdk's 8x16 mode reads
    unit_h = 8 if mode == "tiles" else 16
    per_unit = 1 if mode == "tiles" else 2
    if len(tiles) % per_unit:
        raise SystemExit("sprites16 needs an even tile count, got %d" % len(tiles))
    units = len(tiles) // per_unit
    rows_of_units = (units + cols - 1) // cols
    width, height = cols * 8, rows_of_units * unit_h
    pixels = [[0] * width for _ in range(height)]
    for unit in range(units):
        ox = (unit % cols) * 8
        oy = (unit // cols) * unit_h
        for part in range(per_unit):
            tile = tiles[unit * per_unit + part]
            for y in range(8):
                for x in range(8):
                    pixels[oy + part * 8 + y][ox + x] = tile[y][x]
    return width, height, pixels


def main():
    args = sys.argv[1:]
    mode = "tiles"
    cols = None
    if "--mode" in args:
        i = args.index("--mode")
        mode = args[i + 1]
        del args[i:i + 2]
    if "--cols" in args:
        i = args.index("--cols")
        cols = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 3 or mode not in DEFAULT_COLS:
        print(__doc__, file=sys.stderr)
        return 1
    source, name, out_path = args
    if cols is None:
        cols = DEFAULT_COLS[mode]

    tiles = tile_rows(extract_array(source, name))
    width, height, pixels = lay_out(tiles, mode, cols)
    out_dir = os.path.dirname(os.path.abspath(out_path))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    gbpng.write_png(out_path, width, height, pixels, mode="P", palette=gbpng.GREY_PALETTE)
    print("%s: %d tiles -> %s (%dx%d)" % (name, len(tiles), out_path, width, height))
    return 0


if __name__ == "__main__":
    sys.exit(main())
