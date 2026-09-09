#!/usr/bin/env python3
"""rips the mario character sprites out of the reference sheets into the indexed pngs the
art pipeline (png2tiles.py --mode sprites16) turns into banked c. run as:

    rip_sprites.py [--root REPO] [--check]

provenance
----------
the three sheets under games/mario/art/ref/ are the spriters resource rips of super mario
bros. deluxe (gbc):

    sheet_friendly.png  - "Friendly Characters", resource id 6811, ripped by A.J. Nitro
    sheet_enemies.png   - "Enemies",             resource id 6814, ripped by A.J. Nitro
    sheet_powerups.png  - "Power-Ups",           resource id 215198, ripped by thejohnston

they are gitignored (games/mario/art/ is local-only); the generated c under
games/mario/src/gen is what ships. the single-pose pngs from the mariowiki smbd galleries
(small_mario, super_mario, goomba, koopa_green, shell_green, mushroom, oneup, ...) were used
once to cross-check these cells pixel for pixel and are not needed to run this tool.

what it does
------------
every pose below names a rectangle on a sheet. the rectangle is dropped into the box the rom
draws (16x16, 16x32, 8x16 or 8x32) by two rules and no others:

    vertical   - the art's bottom row lands on the box's bottom row (feet on the floor)
    horizontal - left = (box_w - w) // 2, i.e. centred with the odd spare pixel on the RIGHT,
                 because mario faces right on the sheets and the rom mirrors for left

a pixel's colour picks its 2bpp index out of that pose's four-colour obj palette. colours are
compared as cgb rgb555 (each channel >> 3), which is what the hardware actually stores, so the
sheets' near-duplicates collapse the way they do on a gbc: #FFB210 == #F8B010, #DE0000 ==
#D80000, #FFFBFF == #FFFFFF. a pixel whose colour is not transparent and not in the pose's
palette aborts the run rather than being snapped to something close.

sprites that the rom draws mirrored (goomba squash, shells, piranha) are checked for left-right
symmetry and only their LEFT half is written. the red koopa/paratroopa/shell shapes are checked
against the green ones and then taken from the green row with red palette indices, exactly as
the rom does with a palette swap.

outputs land in games/mario/art/sprites/ (indexed, colour type 3, values 0..3, index 0 =
transparent) plus a per-family proof image in games/mario/art/sprites/proof/ showing each source
cell beside the same tiles run through encode_tile/decode_tile and recoloured with the planned
palette, at 4x.

--check re-encodes every pose, decodes it back through gbpng.decode_tile, and diffs it against
the source cell pixel for pixel; when games/mario/src/gen/<slug>.c already exists its committed
byte array is diffed too. it must report 0 mismatches.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

# ---------------------------------------------------------------- colours and palettes

COLORS = {
    "skin": "FFB210",
    "mario_red": "DE0000",
    "mario_dark": "736900",
    "white": "FFFFFF",
    "koopa_orange": "F8B010",
    "koopa_green": "008010",
    "shell_red": "D80000",
    "goomba_tan": "F8C098",
    "goomba_brown": "984800",
    "black": "000000",
    "item_yellow": "F5C144",
    "item_red": "E23122",
    "item_green": "3E8B29",
    "placeholder": "FF00FF",  # index 0; never drawn, the hardware treats it as transparent
}

# obj palette slot -> the three opaque colours, in the index order the pngs must use
PALETTES = {
    "mario": ("skin", "mario_red", "mario_dark"),  # slot 0 kPalMario
    "mushroom": ("white", "item_yellow", "item_red"),  # slot 1 kPalMushroom
    "star": ("white", "koopa_orange", "shell_red"),  # slot 2 kPalStar
    "oneup": ("white", "item_yellow", "item_green"),  # slot 3 kPalOneup
    "goomba": ("goomba_tan", "goomba_brown", "black"),  # slot 5 kPalGoomba
    "koopa": ("koopa_green", "koopa_orange", "white"),  # slot 6 kPalKoopa
}


def rgb555_of_hex(text):
    r, g, b = int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16)
    return gbpng.rgb555(r, g, b)


def palette_lookup(name):
    """colour555 -> 2bpp index for one obj palette. built in rgb555 so the sheets' near
    duplicates (#FFB210 vs #F8B010) collapse the way the hardware collapses them."""
    out = {}
    for index, key in enumerate(PALETTES[name], start=1):
        value = rgb555_of_hex(COLORS[key])
        if value in out:
            raise SystemExit("palette %s has two colours that are the same rgb555" % name)
        out[value] = index
    return out


def palette_hexes(name):
    return [COLORS["placeholder"]] + [COLORS[key] for key in PALETTES[name]]


# ---------------------------------------------------------------- the sheets

SHEETS = {
    # name: (file, background colours that mean "transparent"; alpha 0 always does)
    "friendly": ("sheet_friendly.png", ("6B8AFF",)),
    "enemies": ("sheet_enemies.png", ()),
    "powerups": ("sheet_powerups.png", ("8CEFFD", "003C7E")),
}

# ---------------------------------------------------------------- the poses
#
# pose = (label, sheet, (x, y, w, h), palette[, substitutions]). every rect below was found by a
# connected-component scan of the sheet and is tight around the lit pixels unless the comment
# says the surrounding blank rows are deliberate (the 16x24 enemy cells).

# the red koopa row is the green row recoloured (EQUIVALENCES proves it), so the red shapes come
# out of the green cells with the shell body swapped to red before the palette lookup
GREEN_TO_RED = {"008010": "D80000"}

# the sheet's rows are NOT gameplay order, and the first cut of this table read them as if they
# were: it put the jump pose and the death pose into the walk cycle, skidded with the third walk
# frame, jumped with the skid and died in a climbing pose. the poses below were identified by
# smbdis's PlayerGraphicsTable (14399) instead, whose shared tile ids say which poses share a head:
# walk frame 1 wears the jump's and the swim frames' head (32,33 vs 32,41), frame 3 the standing
# pose's (3a,37), frame 2 only the standing pose's right half (37); the skid's head is unique and
# the death pose is left-right symmetric (9e,9e,9f,9f). every claim was checked pixel for pixel
# against the sheet before the rectangles moved
SMALL_MARIO = [
    ("idle", "friendly", (12, 5, 12, 16), "mario"),
    ("walk0", "friendly", (69, 6, 12, 15), "mario"),
    ("walk1", "friendly", (30, 26, 15, 16), "mario"),
    ("walk2", "friendly", (86, 5, 13, 16), "mario"),
    ("skid", "friendly", (12, 26, 13, 16), "mario"),
    ("jump", "friendly", (29, 5, 16, 16), "mario"),
    ("death", "friendly", (50, 7, 14, 14), "mario"),
]

# the same table for the big set: walk frame 1 wears the standing pose's head (00,01), frame 2 the
# head every swim, climb, crouch and throw pose shares (08,09) and the legs the throw shares (0e,0f),
# frame 3 a head of its own (10,11); the skid and the jump are unique, and the jump is the one with
# the fist over the cap
BIG_MARIO = [
    ("idle", "friendly", (10, 66, 16, 32), "mario"),
    ("walk0", "friendly", (71, 103, 16, 32), "mario"),
    ("walk1", "friendly", (73, 68, 16, 30), "mario"),
    ("walk2", "friendly", (10, 103, 16, 32), "mario"),
    ("skid", "friendly", (113, 71, 14, 27), "mario"),
    ("jump", "friendly", (31, 66, 16, 32), "mario"),
    ("crouch", "friendly", (52, 76, 16, 22), "mario"),
    ("climb", "friendly", (52, 104, 14, 31), "mario"),
]

# file: (box_w, box_h, poses, half_only)
OUTPUTS = [
    ("mario_small.png", "MarioSmall", 16, 16, SMALL_MARIO, False),
    ("mario_small_climb.png", "MarioSmallClimb", 16, 16,
     [("climb", "friendly", (66, 27, 13, 15), "mario")], False),
    ("mario_big.png", "MarioBig", 16, 32, BIG_MARIO, False),
    ("koopa_green.png", "KoopaGreen", 16, 32, [
        ("walk0", "enemies", (48, 8, 16, 24), "koopa"),
        ("walk1", "enemies", (64, 8, 16, 24), "koopa"),
    ], False),
    # red paratroopa: the shapes are the green row's, the indices are kPalStar's
    ("paratroopa_red.png", "ParatroopaRed", 16, 32, [
        ("flyA", "enemies", (80, 8, 16, 24), "star", GREEN_TO_RED),
        ("flyB", "enemies", (96, 8, 16, 24), "star", GREEN_TO_RED),
    ], False),
    ("goomba.png", "Goomba", 16, 16, [
        ("walk0", "enemies", (0, 16, 16, 16), "goomba"),
    ], False),
    # the squashed goomba is a 16x8 pancake in the bottom half of its cell, and symmetric
    ("goomba_squash.png", "GoombaSquash", 16, 16, [
        ("squash", "enemies", (32, 24, 16, 8), "goomba"),
    ], True),
    # the shell is 16x14 inside a 24-tall cell (rows 9..22), and symmetric
    ("shell_green.png", "ShellGreen", 16, 16, [
        ("shell", "enemies", (112, 17, 16, 14), "koopa"),
    ], True),
    ("shell_red.png", "ShellRed", 16, 16, [
        ("shell", "enemies", (112, 17, 16, 14), "star", GREEN_TO_RED),
    ], True),
    ("piranha.png", "Piranha", 16, 32, [
        ("open", "enemies", (144, 8, 16, 24), "koopa"),
    ], True),
    ("items.png", "Items", 16, 16, [
        ("mushroom", "powerups", (108, 8, 16, 16), "mushroom"),
        ("star", "powerups", (32, 56, 16, 16), "mushroom"),
        ("oneup", "powerups", (32, 8, 16, 16), "oneup"),
    ], False),
    ("flower.png", "Flower", 16, 16, [
        ("flower", "powerups", (8, 32, 16, 16), "oneup"),
    ], False),
    # each 8x8 spin frame sits in the lower tile of its 8x16 pair; the rom flips f0/f1 for the
    # other two frames of the spin
    ("fireball.png", "Fireball", 8, 16, [
        ("frameA", "friendly", (643, 230, 8, 8), "star"),
        ("frameB", "friendly", (657, 230, 8, 8), "star"),
    ], False),
]

# structural claims this tool refuses to run without: (label, sheet, rect_a, rect_b, relation)
EQUIVALENCES = [
    ("goomba walk1 == mirror(walk0)", "enemies", (0, 16, 16, 16), (16, 16, 16, 16), "mirror"),
    ("red koopa walk0 == green", "enemies", (48, 8, 16, 24), (48, 72, 16, 24), "recolour"),
    ("red koopa walk1 == green", "enemies", (64, 8, 16, 24), (64, 72, 16, 24), "recolour"),
    ("red paratroopa A == green", "enemies", (80, 8, 16, 24), (80, 72, 16, 24), "recolour"),
    ("red paratroopa B == green", "enemies", (96, 8, 16, 24), (96, 72, 16, 24), "recolour"),
    ("red shell == green shell", "enemies", (112, 8, 16, 24), (112, 72, 16, 24), "recolour"),
]

# ---------------------------------------------------------------- sheet reading


class Sheets:
    def __init__(self, ref_dir):
        self.images = {}
        self.transparent = {}
        for name, (filename, bgs) in SHEETS.items():
            self.images[name] = gbpng.read_png(os.path.join(ref_dir, filename))
            self.transparent[name] = set(rgb555_of_hex(bg) for bg in bgs)

    def value555(self, sheet, x, y):
        """None for a transparent pixel, else the pixel's rgb555."""
        r, g, b, a = self.images[sheet].rgba(x, y)
        if a == 0:
            return None
        key = gbpng.rgb555(r, g, b)
        return None if key in self.transparent[sheet] else key

    def cell555(self, sheet, rect):
        x, y, w, h = rect
        return [[self.value555(sheet, x + cx, y + cy) for cx in range(w)] for cy in range(h)]

    def cell_rgb(self, sheet, rect):
        """the cell as rgb tuples, with transparent pixels as None so the proof image shows
        them as a checkerboard instead of whatever junk colour sits under the alpha."""
        x, y, w, h = rect
        img = self.images[sheet]
        out = []
        for cy in range(h):
            row = []
            for cx in range(w):
                value = self.value555(sheet, x + cx, y + cy)
                row.append(None if value is None else img.rgba(x + cx, y + cy)[:3])
            out.append(row)
        return out


def indices_of(cell, palette, pose_label, rect, sheet, subs=None):
    """the cell as 2bpp indices; a colour outside the pose's palette is fatal."""
    lookup = palette_lookup(palette)
    swap = {rgb555_of_hex(a): rgb555_of_hex(b) for a, b in (subs or {}).items()}
    out = []
    for cy, row in enumerate(cell):
        line = []
        for cx, value in enumerate(row):
            if value is None:
                line.append(0)
                continue
            value = swap.get(value, value)
            if value not in lookup:
                r, g, b = gbpng.rgb555_to_rgb888(value)
                raise SystemExit(
                    "%s/%s at sheet pixel %d,%d is #%02X%02X%02X, which is not in the %s palette"
                    % (sheet, pose_label, rect[0] + cx, rect[1] + cy, r, g, b, palette))
            line.append(lookup[value])
        out.append(line)
    return out


def place(indices, box_w, box_h, rect):
    """bottom aligned, centred with the spare pixel on the right."""
    w, h = rect[2], rect[3]
    if w > box_w or h > box_h:
        raise SystemExit("cell %dx%d does not fit a %dx%d box" % (w, h, box_w, box_h))
    left, top = (box_w - w) // 2, box_h - h
    box = [[0] * box_w for _ in range(box_h)]
    for y in range(h):
        for x in range(w):
            box[top + y][left + x] = indices[y][x]
    return box


def lit_bounds(box):
    xs, ys = [], []
    for y, row in enumerate(box):
        for x, value in enumerate(row):
            if value:
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return (min(xs), max(xs), min(ys), max(ys))


def left_half(box, label):
    width = len(box[0])
    for y, row in enumerate(box):
        for x in range(width):
            if row[x] != row[width - 1 - x]:
                raise SystemExit("%s is drawn mirrored but is not symmetric at %d,%d" % (label, x, y))
    return [row[: width // 2] for row in box]


# ---------------------------------------------------------------- tiles


def cut_tiles(box):
    """exactly what png2tiles.py --mode sprites16 does to this image: per 16-row band, each
    8-wide column top tile then bottom tile."""
    height, width = len(box), len(box[0])
    tiles = []
    for oy in range(0, height, 16):
        for ox in range(0, width, 8):
            for sub in (0, 8):
                tiles.append(gbpng.encode_tile([box[oy + sub + y][ox:ox + 8] for y in range(8)]))
    return tiles


def tiles_to_box(tiles, width, height):
    box = [[0] * width for _ in range(height)]
    index = 0
    for oy in range(0, height, 16):
        for ox in range(0, width, 8):
            for sub in (0, 8):
                rows = gbpng.decode_tile(tiles[index])
                index += 1
                for y in range(8):
                    for x in range(8):
                        box[oy + sub + y][ox + x] = rows[y][x]
    return box


# ---------------------------------------------------------------- proof rendering

# a 3x5 bitmap font, five row strings a glyph, so the proof images can label themselves
# without a font library
FONT = {
    "A": (".#.", "#.#", "###", "#.#", "#.#"), "B": ("##.", "#.#", "##.", "#.#", "##."),
    "C": (".##", "#..", "#..", "#..", ".##"), "D": ("##.", "#.#", "#.#", "#.#", "##."),
    "E": ("###", "#..", "##.", "#..", "###"), "F": ("###", "#..", "##.", "#..", "#.."),
    "G": (".##", "#..", "#.#", "#.#", ".##"), "H": ("#.#", "#.#", "###", "#.#", "#.#"),
    "I": ("###", ".#.", ".#.", ".#.", "###"), "J": ("..#", "..#", "..#", "#.#", ".#."),
    "K": ("#.#", "#.#", "##.", "#.#", "#.#"), "L": ("#..", "#..", "#..", "#..", "###"),
    "M": ("#.#", "###", "###", "#.#", "#.#"), "N": ("#.#", "###", "###", "###", "#.#"),
    "O": (".#.", "#.#", "#.#", "#.#", ".#."), "P": ("##.", "#.#", "##.", "#..", "#.."),
    "Q": (".#.", "#.#", "#.#", "##.", ".##"), "R": ("##.", "#.#", "##.", "#.#", "#.#"),
    "S": (".##", "#..", ".#.", "..#", "##."), "T": ("###", ".#.", ".#.", ".#.", ".#."),
    "U": ("#.#", "#.#", "#.#", "#.#", ".#."), "V": ("#.#", "#.#", "#.#", ".#.", ".#."),
    "W": ("#.#", "#.#", "###", "###", "#.#"), "X": ("#.#", "#.#", ".#.", "#.#", "#.#"),
    "Y": ("#.#", "#.#", ".#.", ".#.", ".#."), "Z": ("###", "..#", ".#.", "#..", "###"),
    "0": ("###", "#.#", "#.#", "#.#", "###"), "1": (".#.", "##.", ".#.", ".#.", "###"),
    "2": ("##.", "..#", ".#.", "#..", "###"), "3": ("##.", "..#", ".#.", "..#", "##."),
    "4": ("#.#", "#.#", "###", "..#", "..#"), "5": ("###", "#..", "##.", "..#", "##."),
    "6": (".##", "#..", "##.", "#.#", ".#."), "7": ("###", "..#", ".#.", ".#.", ".#."),
    "8": (".#.", "#.#", ".#.", "#.#", ".#."), "9": (".#.", "#.#", ".##", "..#", "##."),
    "-": ("...", "...", "###", "...", "..."), ".": ("...", "...", "...", "...", ".#."),
    ",": ("...", "...", "...", ".#.", "#.."), "/": ("..#", "..#", ".#.", "#..", "#.."),
    " ": ("...", "...", "...", "...", "..."),
}
GLYPH_W, GLYPH_H = 3, 5
PROOF_BG = (24, 24, 32)
PROOF_INK = (220, 220, 220)


def glyph_rows(char):
    return FONT.get(char.upper(), FONT[" "])


def draw_text(canvas, x, y, text, scale=2, ink=PROOF_INK):
    height, width = len(canvas), len(canvas[0])
    for index, char in enumerate(text):
        rows = glyph_rows(char)
        ox = x + index * (GLYPH_W + 1) * scale
        for gy in range(GLYPH_H):
            for gx in range(GLYPH_W):
                if rows[gy][gx] != "#":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        px, py = ox + gx * scale + sx, y + gy * scale + sy
                        if 0 <= px < width and 0 <= py < height:
                            canvas[py][px] = ink


def text_width(text, scale=2):
    return len(text) * (GLYPH_W + 1) * scale


def blit(canvas, x, y, pixels, scale):
    for py, row in enumerate(pixels):
        for px, colour in enumerate(row):
            if colour is None:
                continue
            for sy in range(scale):
                for sx in range(scale):
                    canvas[y + py * scale + sy][x + px * scale + sx] = colour


def checker(w, h, scale, x0, y0, canvas):
    for y in range(h * scale):
        for x in range(w * scale):
            shade = 60 if ((x // scale + y // scale) // 2) % 2 == 0 else 44
            canvas[y0 + y][x0 + x] = (shade, shade, shade)


def box_to_rgb(box, palette):
    hexes = palette_hexes(palette)
    rgbs = [tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in hexes]
    return [[None if v == 0 else rgbs[v] for v in row] for row in box]


# ---------------------------------------------------------------- the run


class Pose:
    def __init__(self, filename, name, label, sheet, rect, palette, box, cells):
        self.filename = filename
        self.name = name
        self.label = label
        self.sheet = sheet
        self.rect = rect
        self.palette = palette
        self.box = box
        self.cells = cells  # source cell rgb, for the proof image


def build(sheets, verbose=True):
    """every output as (filename, name, palette, poses, image rows)."""
    built = []
    for filename, name, box_w, box_h, poses, half in OUTPUTS:
        rows = []
        entries = []
        for pose_spec in poses:
            label, sheet, rect, palette = pose_spec[:4]
            subs = pose_spec[4] if len(pose_spec) > 4 else None
            cell = sheets.cell555(sheet, rect)
            indices = indices_of(cell, palette, label, rect, sheet, subs)
            box = place(indices, box_w, box_h, rect)
            if half:
                box = left_half(box, "%s/%s" % (filename, label))
            entries.append(Pose(filename, name, label, sheet, rect, palette, box,
                                sheets.cell_rgb(sheet, rect)))
            rows.extend(box)
        built.append((filename, name, entries, rows))
        if verbose:
            for pose in entries:
                bounds = lit_bounds(pose.box)
                span = "empty" if bounds is None else \
                    "left %2d right %2d top %2d bottom %2d" % bounds
                print("  %-22s %-8s box %2dx%-2d  %s"
                      % (filename, pose.label, len(pose.box[0]), len(pose.box), span))
    return built


def verify_equivalences(sheets):
    print("structural checks")
    for label, sheet, rect_a, rect_b, relation in EQUIVALENCES:
        a = sheets.cell555(sheet, rect_a)
        b = sheets.cell555(sheet, rect_b)
        if relation == "mirror":
            b = [list(reversed(row)) for row in b]
        shape_a = [[v is not None for v in row] for row in a]
        shape_b = [[v is not None for v in row] for row in b]
        if shape_a != shape_b:
            raise SystemExit("%s: FAILED (lit pixels differ)" % label)
        if relation == "mirror" and a != b:
            raise SystemExit("%s: FAILED (colours differ)" % label)
        if relation == "recolour":
            # every green pixel must map to exactly one red colour and vice versa
            mapping = {}
            for ra, rb in zip(a, b):
                for va, vb in zip(ra, rb):
                    if va is None:
                        continue
                    if mapping.setdefault(va, vb) != vb:
                        raise SystemExit("%s: FAILED (colour %d maps to two colours)" % (label, va))
        print("  ok  %s" % label)


def write_outputs(built, out_dir):
    made = []
    for filename, name, entries, rows in built:
        # png2tiles reads the INDEX, never the PLTE, so a file whose poses wear different obj
        # palettes (items.png: mushroom and star on kPalMushroom, the 1-up on kPalOneup) is fine
        # as long as the index meaning lines up, which it does: 1 white, 2 yellow, 3 accent. the
        # PLTE below is the first pose's colours, so the file previews sensibly in an image viewer
        hexes = palette_hexes(entries[0].palette)
        entries_rgb = [tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in hexes]
        target = os.path.join(out_dir, filename)
        gbpng.write_png(target, len(rows[0]), len(rows), rows, mode="P", palette=entries_rgb)
        # round trip: png2tiles must see mode "P" with 2bpp values only
        back = gbpng.read_png(target)
        if back.mode != "P":
            raise SystemExit("%s did not round trip as an indexed png" % filename)
        if back.rows != rows:
            raise SystemExit("%s round tripped to different pixels" % filename)
        for row in back.rows:
            for value in row:
                if value > 3:
                    raise SystemExit("%s has index %d; only 0..3 fit 2bpp" % (filename, value))
        made.append((filename, name, len(rows[0]), len(rows), len(cut_tiles(rows))))
    return made


def check(built):
    """decode the encoded tiles back and diff against the source cells."""
    mismatches = 0
    for filename, name, entries, rows in built:
        tiles = cut_tiles(rows)
        decoded = tiles_to_box(tiles, len(rows[0]), len(rows))
        if decoded != rows:
            for y, (ra, rb) in enumerate(zip(decoded, rows)):
                for x, (va, vb) in enumerate(zip(ra, rb)):
                    if va != vb:
                        mismatches += 1
                        print("  %s: tile round trip differs at %d,%d (%d != %d)"
                              % (filename, x, y, va, vb))
        # and against the sheet itself, pose by pose
        offset = 0
        for pose in entries:
            height = len(pose.box)
            got = decoded[offset:offset + height]
            offset += height
            expect = pose.box
            for y in range(height):
                for x in range(len(expect[0])):
                    if got[y][x] != expect[y][x]:
                        mismatches += 1
                        print("  %s/%s: pixel %d,%d is %d, sheet says %d"
                              % (filename, pose.label, x, y, got[y][x], expect[y][x]))
        print("  %-22s %3d tiles, round trip clean" % (filename, len(tiles)))
    return mismatches


GEN_ARRAY = re.compile(r"const uint8_t k(\w+)Tiles\[(\d+)\]\s*=\s*\{(.*?)\};", re.S)


def check_generated(built, gen_dir):
    """if the committed c is already there, diff its bytes too."""
    mismatches = 0
    for filename, name, entries, rows in built:
        slug = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
        slug = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", slug).lower()
        source = os.path.join(gen_dir, slug + ".c")
        if not os.path.exists(source):
            continue
        with open(source, "r", encoding="utf-8") as f:
            match = GEN_ARRAY.search(f.read())
        if not match or match.group(1) != name:
            print("  %s: no k%sTiles array found" % (source, name))
            mismatches += 1
            continue
        got = bytes(int(t, 0) for t in match.group(3).replace("\n", "").split(",") if t.strip())
        want = b"".join(cut_tiles(rows))
        if got != want:
            print("  %s: %d of %d bytes differ from the sheet"
                  % (source, sum(1 for a, b in zip(got, want) if a != b), len(want)))
            mismatches += 1
        else:
            print("  %-28s %d bytes match the sheet" % (slug + ".c", len(want)))
    return mismatches


def write_proofs(built, proof_dir, scale=4):
    made = []
    for filename, name, entries, rows in built:
        columns = []
        for pose in entries:
            cell = pose.cells
            box = box_to_rgb(pose.box, pose.palette)
            decoded = tiles_to_box(cut_tiles(pose.box), len(pose.box[0]), len(pose.box))
            decoded_rgb = box_to_rgb(decoded, pose.palette)
            label = "%s %d,%d" % (pose.label, pose.rect[0], pose.rect[1])
            width = max(len(cell[0]) * scale + 8 + len(box[0]) * scale, text_width(label))
            height = max(len(cell), len(box)) * scale
            columns.append((label, cell, decoded_rgb, width, height))
        pad, header = 10, GLYPH_H * 2 + 6
        footer = "%s  LEFT SHEET CELL  RIGHT DECODED TILES" % name.upper()
        total_w = max(pad + sum(c[3] + pad for c in columns), pad * 2 + text_width(footer))
        total_h = pad + header + max(c[4] for c in columns) + pad + GLYPH_H * 2 + pad
        canvas = [[PROOF_BG] * total_w for _ in range(total_h)]
        draw_text(canvas, pad, total_h - pad - GLYPH_H * 2, footer)
        x = pad
        for label, cell, decoded_rgb, width, height in columns:
            draw_text(canvas, x, pad, label)
            y = pad + header
            checker(len(cell[0]), len(cell), scale, x, y, canvas)
            blit(canvas, x, y, cell, scale)
            x2 = x + len(cell[0]) * scale + 8
            checker(len(decoded_rgb[0]), len(decoded_rgb), scale, x2, y, canvas)
            blit(canvas, x2, y, decoded_rgb, scale)
            x += width + pad
        target = os.path.join(proof_dir, filename)
        gbpng.write_png(target, total_w, total_h, canvas, mode="RGB")
        made.append(target)
    return made


def main():
    here = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    parser = argparse.ArgumentParser(description="rip the mario character sprites")
    parser.add_argument("--root", default=here, help="repo root (default: this file's repo)")
    parser.add_argument("--check", action="store_true",
                        help="verify only: decode the tiles back and diff against the sheets")
    args = parser.parse_args()

    ref_dir = os.path.join(args.root, "games", "mario", "art", "ref")
    out_dir = os.path.join(args.root, "games", "mario", "art", "sprites")
    proof_dir = os.path.join(out_dir, "proof")
    gen_dir = os.path.join(args.root, "games", "mario", "src", "gen")
    if not os.path.isdir(ref_dir):
        raise SystemExit("no reference sheets at %s (they are gitignored; copy them in)" % ref_dir)

    sheets = Sheets(ref_dir)
    verify_equivalences(sheets)

    if args.check:
        print("check")
        built = build(sheets, verbose=False)
        bad = check(built)
        bad += check_generated(built, gen_dir)
        print("%d mismatches" % bad)
        return 1 if bad else 0

    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(proof_dir, exist_ok=True)
    print("poses (lit bounds are inside the pose's box)")
    built = build(sheets)
    made = write_outputs(built, out_dir)
    print("wrote")
    for filename, name, w, h, tiles in made:
        print("  %-22s %2dx%-3d %3d tiles  k%sTiles[%d]" % (filename, w, h, tiles, name, tiles * 16))
    for target in write_proofs(built, proof_dir):
        print("  proof %s" % os.path.relpath(target, args.root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
