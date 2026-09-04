#!/usr/bin/env python3
"""turns a png of art into the banked c the mario rom loads into vram. run as:
png2tiles.py <png> <out_dir> --name <Slug> --mode tiles|sprites16|screen
             [--bank N] [--palette RRGGBB,RRGGBB,RRGGBB,RRGGBB] [--first-id N]
             [--vram-bank 0|1] [--max-palettes 8] [--report]

tiles     - 8x8 cells in reading order, one palette.
sprites16 - 8x16 sprites in reading order, each emitted as its upper 8x8 then its lower one,
            which is the pair order gbdk's 8x16 sprite mode reads.
screen    - a whole background: cgb palettes are planned out of the image, identical tiles are
            shared, and a tile map plus an attribute map come out beside them.

an indexed png needs no --palette at all: its palette index IS the 2bpp value.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

MODES = ("tiles", "sprites16", "screen")
COLORS_PER_PALETTE = 4
# cgb bg attribute bit 3 selects vram bank 1 for the tile the cell names
ATTR_VRAM_BANK_1 = 0x08
MAX_TILE_IDS = 256
BYTES_PER_LINE = 12
WORDS_PER_LINE = 8


def snake(name):
    s = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s).lower()


def parse_palette(text):
    entries = [e.strip() for e in text.split(",")]
    if len(entries) != COLORS_PER_PALETTE:
        raise SystemExit("--palette needs exactly four RRGGBB colors")
    out = []
    for entry in entries:
        entry = entry.lstrip("#")
        if not re.fullmatch(r"[0-9a-fA-F]{6}", entry):
            raise SystemExit("bad --palette color %r" % entry)
        out.append(gbpng.rgb555(int(entry[0:2], 16), int(entry[2:4], 16), int(entry[4:6], 16)))
    return out


def pixel_values(img, palette, transparent_is_zero):
    """the whole image as rows of 2bpp values, resolved against whichever palette rule applies."""
    if img.mode == "P":
        rows = []
        for y in range(img.height):
            row = img.rows[y]
            for x, value in enumerate(row):
                if value > 3:
                    raise SystemExit("indexed png pixel at %d,%d has index %d; only 0..3 fit 2bpp"
                                     % (x, y, value))
            rows.append(list(row))
        return rows
    if palette is None:
        raise SystemExit("a non-indexed png needs --palette")
    lookup = {}
    for index, color in enumerate(palette):
        lookup.setdefault(color, index)
    rows = []
    for y in range(img.height):
        row = []
        for x in range(img.width):
            r, g, b, a = img.rgba(x, y)
            if transparent_is_zero and a == 0:
                row.append(0)
                continue
            key = gbpng.rgb555(r, g, b)
            if key not in lookup:
                raise SystemExit("pixel at %d,%d (#%02X%02X%02X) is not in --palette" % (x, y, r, g, b))
            row.append(lookup[key])
        rows.append(row)
    return rows


def cut_tile(values, ox, oy):
    return [values[oy + y][ox:ox + 8] for y in range(8)]


def cut_tiles(values, width, height, mode):
    if width % 8 or height % 8:
        raise SystemExit("png is %dx%d; both sides must be multiples of 8" % (width, height))
    if mode == "sprites16" and height % 16:
        raise SystemExit("sprites16 needs a height that is a multiple of 16, got %d" % height)
    step = 8 if mode == "tiles" else 16
    out = []
    for oy in range(0, height, step):
        for ox in range(0, width, 8):
            out.append(gbpng.encode_tile(cut_tile(values, ox, oy)))
            if mode == "sprites16":
                out.append(gbpng.encode_tile(cut_tile(values, ox, oy + 8)))
    return out


def plan_palettes(cell_colors, max_palettes):
    """greedy set cover: the fussiest cells (most distinct colors) claim a palette first, and
    every later cell joins the first palette it still fits inside. deterministic in cell order."""
    palettes = []
    assigned = [0] * len(cell_colors)
    order = sorted(range(len(cell_colors)), key=lambda i: (-len(cell_colors[i]), i))
    for index in order:
        colors = cell_colors[index]
        placed = False
        for slot, existing in enumerate(palettes):
            union = set(existing) | colors
            if len(union) <= COLORS_PER_PALETTE:
                for color in sorted(colors - set(existing)):
                    existing.append(color)
                assigned[index] = slot
                placed = True
                break
        if placed:
            continue
        if len(palettes) >= max_palettes:
            raise SystemExit("more than %d palettes needed for this image" % max_palettes)
        palettes.append(sorted(colors))
        assigned[index] = len(palettes) - 1
    for entry in palettes:
        while len(entry) < COLORS_PER_PALETTE:
            entry.append(entry[-1] if entry else 0)
    return palettes, assigned


def plan_screen(img, max_palettes):
    if img.width % 8 or img.height % 8:
        raise SystemExit("screen png is %dx%d; both sides must be multiples of 8"
                         % (img.width, img.height))
    cols, rows = img.width // 8, img.height // 8
    quantized = [[gbpng.rgb555(*img.rgba(x, y)[:3]) for x in range(img.width)]
                 for y in range(img.height)]
    cell_colors = []
    for cy in range(rows):
        for cx in range(cols):
            colors = {quantized[cy * 8 + y][cx * 8 + x] for y in range(8) for x in range(8)}
            if len(colors) > COLORS_PER_PALETTE:
                raise SystemExit("cell %d,%d has %d distinct colors; a cgb tile gets four"
                                 % (cx, cy, len(colors)))
            cell_colors.append(colors)
    palettes, assigned = plan_palettes(cell_colors, max_palettes)

    tiles, ids, tile_map = [], {}, []
    for cy in range(rows):
        for cx in range(cols):
            index = cy * cols + cx
            palette = palettes[assigned[index]]
            pixels = [[palette.index(quantized[cy * 8 + y][cx * 8 + x]) for x in range(8)]
                      for y in range(8)]
            data = gbpng.encode_tile(pixels)
            if data not in ids:
                ids[data] = len(tiles)
                tiles.append(data)
            tile_map.append(ids[data])
    return cols, rows, palettes, assigned, tiles, tile_map


def byte_lines(data, per_line=BYTES_PER_LINE):
    out = []
    for i in range(0, len(data), per_line):
        out.append("    " + " ".join("0x%02X," % b for b in data[i:i + per_line]))
    return out


def word_lines(values, per_line=WORDS_PER_LINE):
    out = []
    for i in range(0, len(values), per_line):
        out.append("    " + " ".join("0x%04X," % v for v in values[i:i + per_line]))
    return out


def write_files(out_dir, slug, name, png_name, bank, mode, blocks, defines, externs, cgb):
    os.makedirs(out_dir, exist_ok=True)
    banner = "// generated by games/mario/tools/png2tiles.py from %s - do not edit\n" % png_name
    guard = "GEN_%s_H" % slug.upper()

    header = ["#ifndef %s" % guard, "#define %s" % guard, banner.rstrip("\n"), ""]
    if cgb:
        header.append("#include <gb/cgb.h>")
    header += ["#include <stdint.h>", ""]
    header += defines + [""] + externs + ["", "#endif", ""]
    with open(os.path.join(out_dir, slug + ".h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(header))

    source = [banner.rstrip("\n")]
    if bank is not None:
        source.append("#pragma bank %d" % bank)
    source += ["", '#include "%s.h"' % slug, ""]
    for block in blocks:
        source += ["// clang-format off"] + block + ["// clang-format on", ""]
    with open(os.path.join(out_dir, slug + ".c"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(source))
    _ = mode, name


def emit_tiles(args, img):
    palette = parse_palette(args["palette"]) if args["palette"] else None
    values = pixel_values(img, palette, args["mode"] == "sprites16")
    tiles = cut_tiles(values, img.width, img.height, args["mode"])
    data = b"".join(tiles)
    name, slug = args["name"], snake(args["name"])
    array = "const uint8_t k%sTiles[%d]" % (name, len(data))
    block = [array + " = {"] + byte_lines(data) + ["};"]
    write_files(args["out_dir"], slug, name, os.path.basename(args["png"]), args["bank"],
                args["mode"], [block],
                ["#define k%sTileCount %dU" % (name, len(tiles))],
                ["extern %s;" % array], cgb=False)
    if args["report"]:
        print("%s: %d tiles, %d bytes" % (name, len(tiles), len(data)))


def emit_screen(args, img):
    cols, rows, palettes, assigned, tiles, tile_map = plan_screen(img, args["max_palettes"])
    first = args["first_id"]
    if first + len(tiles) > MAX_TILE_IDS:
        raise SystemExit("first-id %d plus %d tiles overflows the 256 tile ids" % (first, len(tiles)))
    name, slug = args["name"], snake(args["name"])
    attr_bank = ATTR_VRAM_BANK_1 if args["vram_bank"] else 0
    data = b"".join(tiles)
    flat = [c for entry in palettes for c in entry]

    tiles_array = "const uint8_t k%sTiles[%d]" % (name, len(data))
    pal_array = "const palette_color_t k%sPalettes[%d]" % (name, len(flat))
    map_array = "const uint8_t k%sMap[%d]" % (name, cols * rows)
    attr_array = "const uint8_t k%sAttrs[%d]" % (name, cols * rows)
    map_bytes = [first + t for t in tile_map]
    attr_bytes = [assigned[i] | attr_bank for i in range(cols * rows)]

    blocks = [
        [tiles_array + " = {"] + byte_lines(data) + ["};"],
        [pal_array + " = {"] + word_lines(flat) + ["};"],
        [map_array + " = {"] + byte_lines(map_bytes, cols if cols <= 20 else BYTES_PER_LINE) + ["};"],
        [attr_array + " = {"] + byte_lines(attr_bytes, cols if cols <= 20 else BYTES_PER_LINE) + ["};"],
    ]
    defines = [
        "#define k%sCols %dU" % (name, cols),
        "#define k%sRows %dU" % (name, rows),
        "#define k%sTileCount %dU" % (name, len(tiles)),
        "#define k%sPaletteCount %dU" % (name, len(palettes)),
        "#define k%sFirstTile %dU" % (name, first),
    ]
    externs = ["extern %s;" % a for a in (tiles_array, pal_array, map_array, attr_array)]
    write_files(args["out_dir"], slug, name, os.path.basename(args["png"]), args["bank"],
                args["mode"], blocks, defines, externs, cgb=True)
    if args["report"]:
        print("%s: %dx%d cells, %d tiles, %d palettes" % (name, cols, rows, len(tiles), len(palettes)))


def parse_args(argv):
    args = {"png": None, "out_dir": None, "name": None, "mode": None, "bank": None,
            "palette": None, "first_id": 0, "vram_bank": 0, "max_palettes": 8, "report": False}
    flags = {"--name": "name", "--mode": "mode", "--bank": "bank", "--palette": "palette",
             "--first-id": "first_id", "--vram-bank": "vram_bank", "--max-palettes": "max_palettes"}
    positional = []
    i = 0
    while i < len(argv):
        token = argv[i]
        if token == "--report":
            args["report"] = True
            i += 1
        elif token in flags:
            if i + 1 >= len(argv):
                raise SystemExit("%s needs a value" % token)
            key = flags[token]
            value = argv[i + 1]
            args[key] = value if key in ("name", "mode", "palette") else int(value, 0)
            i += 2
        elif token.startswith("--"):
            raise SystemExit("unknown option %s" % token)
        else:
            positional.append(token)
            i += 1
    if len(positional) != 2 or args["name"] is None or args["mode"] not in MODES:
        print(__doc__, file=sys.stderr)
        raise SystemExit(1)
    args["png"], args["out_dir"] = positional
    return args


def main():
    args = parse_args(sys.argv[1:])
    img = gbpng.read_png(args["png"])
    if args["mode"] == "screen":
        emit_screen(args, img)
    else:
        emit_tiles(args, img)
    return 0


if __name__ == "__main__":
    sys.exit(main())
