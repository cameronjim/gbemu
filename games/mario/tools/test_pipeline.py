#!/usr/bin/env python3
"""the art pipeline's own gate: the committed generated c must be exactly what the tools produce.
run as: test_pipeline.py <source_dir> <tmp_dir>

bowser is the round trip - his tiles go out to a png and back and must come back byte identical -
and a synthetic image exercises the screen mode's palette planner, which no committed asset uses
yet.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng
import tiles2png

TOOLS = os.path.dirname(os.path.abspath(__file__))
FAILURES = []


def check(condition, message):
    if not condition:
        FAILURES.append(message)


def run(script, *args):
    cmd = [sys.executable, os.path.join(TOOLS, script)] + [str(a) for a in args]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode:
        FAILURES.append("%s failed: %s%s" % (script, result.stdout, result.stderr))
    return result


def read_defines(path):
    out = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) == 3 and parts[0] == "#define":
                out[parts[1]] = int(parts[2].rstrip("U"))
    return out


def bowser_round_trip(source_dir, tmp_dir):
    committed = os.path.join(source_dir, "games", "mario", "src", "gen", "bowser.c")
    expected = tiles2png.extract_array(committed, "kBowserTiles")
    check(len(expected) == 512, "committed kBowserTiles is %d bytes, want 512" % len(expected))

    png = os.path.join(tmp_dir, "bowser.png")
    run("tiles2png.py", committed, "kBowserTiles", png, "--mode", "sprites16", "--cols", 4)
    run("png2tiles.py", png, tmp_dir, "--name", "Bowser", "--mode", "sprites16", "--bank", 4)
    actual = tiles2png.extract_array(os.path.join(tmp_dir, "bowser.c"), "kBowserTiles")
    check(actual == expected, "bowser did not survive the png round trip")


def screen_mode(tmp_dir):
    # five cells that plan to exactly three palettes: two cells agree, two more bring four colors
    # each of their own, and the last is a subset of the first and has to join it rather than
    # open a fourth
    sets = [
        [(0, 0, 0), (248, 0, 0), (0, 248, 0), (0, 0, 248)],
        [(0, 0, 0), (248, 0, 0), (0, 248, 0), (0, 0, 248)],
        [(8, 8, 8), (248, 248, 0), (0, 248, 248), (248, 0, 248)],
        [(16, 16, 16), (248, 128, 0), (128, 248, 0), (0, 128, 248)],
        [(0, 0, 0), (248, 0, 0)],
    ]
    width, height = 8 * len(sets), 8
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            cell = sets[x // 8]
            row.append(cell[((x % 8) // 2) % len(cell)])
        rows.append(row)
    png = os.path.join(tmp_dir, "screen.png")
    gbpng.write_png(png, width, height, rows, mode="RGB")

    run("png2tiles.py", png, tmp_dir, "--name", "Screen", "--mode", "screen", "--first-id", 16,
        "--vram-bank", 1)
    defines = read_defines(os.path.join(tmp_dir, "screen.h"))
    check(defines.get("kScreenCols") == len(sets), "screen cols wrong: %r" % defines.get("kScreenCols"))
    check(defines.get("kScreenRows") == 1, "screen rows wrong: %r" % defines.get("kScreenRows"))
    check(defines.get("kScreenPaletteCount") == 3,
          "expected 3 planned palettes, got %r" % defines.get("kScreenPaletteCount"))
    check(defines.get("kScreenFirstTile") == 16, "first tile not honoured")

    source = os.path.join(tmp_dir, "screen.c")
    palettes = tiles2png.extract_values(source, "kScreenPalettes")
    tile_map = tiles2png.extract_array(source, "kScreenMap")
    attrs = tiles2png.extract_array(source, "kScreenAttrs")
    check(len(tile_map) == len(sets), "map has %d cells, want %d" % (len(tile_map), len(sets)))
    check(min(tile_map) >= 16, "map ids are not offset by first-id")
    check(len(palettes) == 12, "palette array is %d words, want 12" % len(palettes))
    check(all(a & 0x08 for a in attrs), "vram bank 1 bit missing from the attribute map")
    check(sorted({a & 0x07 for a in attrs}) == [0, 1, 2], "attrs do not name all three palettes")
    check(tile_map[0] == tile_map[1], "the two identical cells were not deduped")
    check(attrs[0] == attrs[1], "the two identical cells landed on different palettes")


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    source_dir, tmp_dir = sys.argv[1], sys.argv[2]
    os.makedirs(tmp_dir, exist_ok=True)
    bowser_round_trip(source_dir, tmp_dir)
    screen_mode(tmp_dir)
    for failure in FAILURES:
        print("FAIL: %s" % failure, file=sys.stderr)
    if FAILURES:
        return 1
    print("mario art pipeline round trip ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
