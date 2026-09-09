#!/usr/bin/env python3
"""draws the score popup's tile strip, the one piece of sprite art the smbd sheets do not carry. run
as:

    draw_popup.py <out png>

the strip is nine 8x16 columns for png2tiles.py's sprites16 mode, each a glyph pair in a 3x5 pixel
font on the column's top 8 rows with the bottom 8 blank - the way smb's own floatey numbers are two
8x8 tiles (smbdis FloateyNumTileData, 1262): the five left halves 10 20 40 50 80, the right halves
0 and 00, and the 1-UP's two halves. a label is one left column beside one right one, so the eleven
values smb shows are pairs of these nine. indexed, so the index is the 2bpp value: 1 is the white of
kPalMushroom, and nothing else is used.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

GLYPHS = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "8": ("111", "101", "111", "101", "111"),
    "U": ("101", "101", "101", "101", "111"),
    "P": ("111", "101", "111", "100", "100"),
    "-": ("000", "000", "111", "000", "000"),
}

# each column's two glyphs; None leaves the right half of the column blank
COLUMNS = (("1", "0"), ("2", "0"), ("4", "0"), ("5", "0"), ("8", "0"),
           ("0", None), ("0", "0"), ("1", "-"), ("U", "P"))
GLYPH_ROW = 1
WHITE = 1


def blit(rows, x0, glyph):
    for dy, line in enumerate(GLYPHS[glyph]):
        for dx, bit in enumerate(line):
            if bit == "1":
                rows[GLYPH_ROW + dy][x0 + dx] = WHITE


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 1
    width = 8 * len(COLUMNS)
    rows = [[0] * width for _ in range(16)]
    for column, (left, right) in enumerate(COLUMNS):
        blit(rows, column * 8, left)
        if right is not None:
            blit(rows, column * 8 + 4, right)
    out = sys.argv[1]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    gbpng.write_png(out, width, 16, rows, mode="P",
                    palette=[(0, 0, 0), (255, 255, 255), (128, 128, 128), (64, 64, 64)])
    print("%s: %d columns" % (out, len(COLUMNS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
