#!/usr/bin/env python3
"""rips the block and terrain art of the levels that have a reference capture out of those
captures and into the indexed pngs the art pipeline (png2tiles.py --mode tiles) turns into
banked c. run as:

    rip_tiles.py [--level 1-1|1-2|1-3|1-4|all] [--root REPO] [--generated DIR] [--check] [--report]
                 [--proof DIR [--panorama FILE:SCY ...]]

everything that is specific to one level - its capture, that capture's grid offsets and column
stitch, the anchor cell of every family the level teaches, the kinds it stands, the sprites and
captions drawn over it - lives in one entry of LEVELS below. the learning engine, the palette
resolution, the png writing, the structural report, the position audit and the proof images are
shared and know nothing about any particular level.

provenance
----------
games/mario/art/ref/smbd_ch_1-1.png is mariouniverse's challenge-mode rip of super mario bros.
deluxe 1-1, a real gbc capture: 3334x394, 16px cells on a grid with x offset 0 and y offset 2.
the capture drops the two sky rows the nes level has, so rip row r is level row r + 2 and the
ground's top edge sits at y = 178 = 2 + 16 * 11 (rip row 11, level row 13). the level's own
bottom row - level row 14, the buried ground fill - is below the capture's last scanline, which
is why the fill block wears the surface block's art (they are the same 16x16 unit in smb).

games/mario/art/ref/smbd_ch_1-2.png is the same site's challenge-mode rip of 1-2: 3584x393, the
same 16px cells with x offset 0 but y offset 8, so the floor's top edge is y = 184 and its bottom
six scanlines run to y 199. its upper band is the underground run (capture columns 0-191, level
columns 24-215) butted straight against the above-ground ending (capture 192-223, level 217-248):
the exit shaft's right column, level 216, is not in the image at all, and column 192 is already the
ending's first column of sky. the level's 24-column above-ground start is in no smbd rip. the pipe
coin room hangs under the run in a second band whose left edge is x 1632 and top edge y 201, and
its cells fall on the area grid's own (column, level row) from row 2. it is drawn on flat black
like 1-1's bonus room and turns out to be the same art under the same colours, which is what the
report and the 1-2 audit below prove - the one thing the run draws that no 1-1 cell holds is the
joint where a sideways pipe's body runs into its shaft (pipe_joint_*, and 1-1's own room has it).

games/mario/art/ref/smbd_ch_1-3.png is the same site's challenge-mode rip of 1-3: 2560x200, 16px
cells with x offset 0 and y offset 8 - the same frame 1-2's upper band uses, and the same rip row
r = level row r + 2, verified against the ground's top edge at y = 184 = 8 + 16 * 11. it is 160
columns, exactly the json's length, and it needs no column correction at all: every one of the
level's own columns stands where the json puts it (see the 1-3 AUDIT note). the level is 12 rows
of capture, level rows 2-13; row 14's buried ground fill is again below the last scanline. the one
family the capture teaches that no earlier one holds is 1-3's tree - a green canopy in a left cap,
a middle and a right cap over a column of striped trunk.

games/mario/art/ref/smbd_ch_1-4.png is the same site's challenge-mode rip of 1-4, the castle:
2247x223, 16px cells on a grid with NO offset in either axis (the seven spare columns of pixels and
fifteen spare rows are a trailing margin). it is 140 columns and 13 rows: its column c is level
column c + 16 (the opening platform, its steps and the first lava pit, level columns 0-15, are in no
smbd rip) and its row r is level row r + 1, so the roof is rip row 1 and the floor rip row 12; level
row 14 is below its last scanline as in every other capture. it is drawn on flat black under the
castle palette set, whose colours this ripper wrote unquantised (FFFFFF, BFBFBF, 7F7F7F where a gbc
grab reads F8F8F8, B8B8B8, 787878) - CASTLE_SLOTS lists the capture's own values and the rom stores
each channel >> 3. six families come off it: the masonry course pair, the castle's own solid block
(a brown face in a grey border, nothing like the overworld's bevelled hard block), the lava's wave,
the bridge, a 16px axe whose handle fills the cell's lower half, and the chain that runs from the
bridge's far end up to the axe, which the bible had no cell for. its stitch is not clean at the end:
the wall past the axe (level 143) is drawn two pixels short in the rows around the bridge, and the
twelve columns past it hold the smbd ending room, which the bible does not follow (it carries the
nes rip's room, see level-1-4.json); both are declared overlays rather than tile truth.

games/mario/art/ref/sheet_tileset.png is the spriters resource "Tileset" sheet for the same
game (resource id 171364, ripped by Depressed Mario). the capture is challenge mode, which
draws each question block's CONTENTS over it (a coin, a mushroom, a fire flower) instead of the
"?", and a level nobody has played has no used block at all, so those two blocks - and only
those two - are taken from the sheet. the sheet is optional: without it the three families cut
from it (question, spent, coin) are neither written nor checked and the committed c stands. all
reference images are gitignored; the generated c under games/mario/src/gen is what ships.

what it does
------------
every tile below names a quadrant of one 16x16 cell of one of the references. the cells were
found by position (see each level's "anchors") and every one of them is checked, by --check,
against every cell of the compiled level grids that says it holds that kind. the cell's colours
are resolved against its family's cgb palette slot, as rgb555 (each channel >> 3), which is what
the hardware stores; a colour that is not in the slot aborts the run rather than being snapped to
something close.

the 1-1 grid and its capture do not agree on where everything stands - the capture is missing a
column (see the 1-1 "columns" table) - so --check reports per-kind agreement rather than demanding
it. see the AUDIT notes below.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

CELL = 16

SKY = "6888F8"
# below ground, and in every coin room, the backdrop is flat black
UNDERGROUND = "000000"

# ---------------------------------------------------------------- the palettes
#
# eight cgb bg palette slots, pinned per kind by kPaletteRom in assets.c. colour 0 is the
# backdrop every partly-transparent cell shows through, and colour 1 of the sky slot is also the
# hud row's ink (see kHudBarAttr), which is why it has to stay white. every colour is read
# straight off the capture, and all three captures so far draw the same set: 1-3 adds no colour
# and needs no slot of its own (its canopy is exactly the pipe slot's four and its trunk the
# brick slot's browns)
SLOTS = {
    # clouds and the flag's pennant. 30A0F8 is the scallop shading under a cloud
    "sky": [SKY, "F8F8F8", "30A0F8", "000000"],
    # the ground block is opaque across its whole cell, so its colour 0 never reaches a pixel
    "ground": [SKY, "F8C098", "984800", "000000"],
    # the brick, the hard block, the whole castle and 1-3's tree trunk are the same three browns
    "brick": [SKY, "F8C098", "984800", "000000"],
    # the question block's gold, which the used block shares - see kCamPalSpent below
    "question": [SKY, "F8B840", "984800", "000000"],
    # pipes, hills, bushes, the flagpole and 1-3's tree canopy
    "pipe": [SKY, "70F830", "108800", "000000"],
    # the used block wears the question block's own colours; the rip of the sheet's used block
    # only ever lights colours 2 and 3
    "spent": [SKY, "F8B840", "984800", "000000"],
    # the world coin. colour 3 is the shading inside the coin's ring, which the overworld
    # draws black and the bonus room draws in its own teal
    "coin": [SKY, "F8B840", "984800", "000000"],
}

# below ground it is the same art under different colours: a black backdrop, masonry in the teal
# 008888 under a near-white B8F8F0 highlight. the brick slot carries that highlight too, for the
# hard block's bevel (1-2's stair-step blocks light it, 1-1's room has no hard block at all): the
# brick's own tiles never use colour 1, because the underground load replaces the one pair that
# did - the cell's top row - with the room's own, mortar joints showing
UNDERGROUND_SLOTS = {
    "sky": [UNDERGROUND, "F8F8F8", "30A0F8", "000000"],
    "ground": [UNDERGROUND, "B8F8F0", "008888", "000000"],
    "brick": [UNDERGROUND, "B8F8F0", "008888", "000000"],
    # the question block's bottom and right edges are colour 3, black above ground and the
    # masonry's teal below it (1-2's blocks, whose faces the challenge rip covers with their coins)
    "question": [UNDERGROUND, "F8B840", "984800", "008888"],
    # every pipe the room draws outlines itself in this dark green rather than the overworld's
    # black - a black rim would be invisible against the room's own black backdrop
    "pipe": [UNDERGROUND, "70F830", "108800", "004800"],
    "spent": [UNDERGROUND, "F8B840", "984800", "008888"],
    "coin": [UNDERGROUND, "F8B840", "984800", "008888"],
}

# the castle set, read off the 1-4 capture. the masonry is the ground slot - black mortar, a white
# highlight, a light face and a dark shadow - and every other castle piece borrows a slot nothing
# else in a castle uses: the castle's own solid block wears the brick slot (brown face, grey-green
# border, backdrop corners), the question block keeps its gold face and brown bevel by choice with
# the capture's grey edge (the capture draws the one ? block teal-faced; the nes rip draws it
# orange), the axe takes the pipe slot's two oranges and grey (nothing else on
# that slot stands in a castle), the bridge the neutral slot's white, grey and red, and the lava the
# coin slot's white foam over red. colour 0 of every slot is the backdrop; kCastleRgb keeps it one
# shade off black on purpose so the host tests can still tell a castle from the underground by its
# sky alone, which is the one deliberate deviation from the capture's flat black
CASTLE = "000000"
CASTLE_SLOTS = {
    "sky": [CASTLE, "FFFFFF", "7F7F7F", "000000"],
    "ground": [CASTLE, "FFFFFF", "BFBFBF", "7F7F7F"],
    "brick": [CASTLE, "984800", "606860", "000000"],
    # the capture's one ? block is teal-faced; the rom keeps the gold face and brown bevel on purpose
    # (see assets_load_bg_palettes_castle), so this slot is what the rom draws, not the capture
    "question": [CASTLE, "F8B840", "984800", "7F7F7F"],
    "pipe": [CASTLE, "FFA347", "E75F13", "7F7F7F"],
    "neutral": [CASTLE, "FFFFFF", "7F7F7F", "F83800"],
    # no used block is in the capture; the slot keeps the coin's gold and brown for the hud icon
    "spent": [CASTLE, "F8B840", "984800", "7F7F7F"],
    "coin": [CASTLE, "FFFFFF", "F83800", "000000"],
}

# the three palette sets by name, which is how a frame says which one reads its cells
SLOT_SETS = {"overworld": SLOTS, "underground": UNDERGROUND_SLOTS, "castle": CASTLE_SLOTS}

# the one colour in 1-1 that no slot can hold. the pennant's emblem is drawn in the koopa green
# 008010, and the cell it sits in is pinned to the sky slot, whose four colours are the backdrop,
# the white the hud row also needs, the clouds' scallop blue and black. re-slotting the pennant
# onto the pipe slot - the only one that carries a green - would cost it its white, so the emblem
# takes the slot's black instead and reads as a dark disc rather than a dark green one
APPROXIMATIONS = {("sky", "008010"): "000000"}

# the sheet was ripped from a different dump than the captures and renders the same four block
# colours a shade off. these are the only sheet colours the two blocks use, and each maps to the
# capture's own value so a sheet-sourced tile lands in the same cgb palette entry as a rip one
SHEET_TO_RIP = {
    "FFB43C": "F8B840",  # the question block's gold
    "A84400": "984800",  # the block body brown
    "000000": "000000",
    # the sheet's own backdrop, which shows through the two pixels smbd rounds off a block's top
    # corners - the capture shows sky in exactly the same two pixels
    "004B77": SKY,
    "007CB6": SKY,
}

# a tile cut from a black-backdrop frame has to land on the same 2bpp values as the overworld tile
# it stands in for, because the room reaches vram through the same palette slot with the colours
# below swapped in. teal is the masonry body (colour 2) and the room's near-white the highlight
BONUS_TO_SLOT = {"008888": "984800", "B8F8F0": "F8C098", "000000": "000000"}
# the sideways pipe's only instance in the 1-1 capture is IN the room, so its tiles are cut from
# there and have to be lifted into the overworld pipe slot's four colours: the room's backdrop is
# that slot's colour 0 and the room's dark green rim is its colour 3. the shaft joint, cut from
# 1-2's run, is lifted the same way
BONUS_PIPE_TO_SLOT = {"000000": SKY, "70F830": "70F830", "108800": "108800", "004800": "000000"}
# and a loose coin's ring shading, teal down there and black above
BONUS_COIN_TO_SLOT = {"000000": SKY, "F8B840": "F8B840", "984800": "984800", "008888": "000000"}
BONUS_RECOLOUR = {
    "pipe_side_mouth_t": BONUS_PIPE_TO_SLOT,
    "pipe_side_mouth_b": BONUS_PIPE_TO_SLOT,
    "pipe_side_body_t": BONUS_PIPE_TO_SLOT,
    "pipe_side_body_b": BONUS_PIPE_TO_SLOT,
    "pipe_joint_t": BONUS_PIPE_TO_SLOT,
    "pipe_joint_b": BONUS_PIPE_TO_SLOT,
    "bonus_joint_t": BONUS_PIPE_TO_SLOT,
    "bonus_joint_b": BONUS_PIPE_TO_SLOT,
    "ug_pipe_lip_l12": BONUS_PIPE_TO_SLOT,
    "ug_pipe_lip_r12": BONUS_PIPE_TO_SLOT,
    "ug_pipe_body_l12": BONUS_PIPE_TO_SLOT,
    "ug_pipe_body_r12": BONUS_PIPE_TO_SLOT,
    "ug_coin12": BONUS_COIN_TO_SLOT,
}

# ---------------------------------------------------------------- the reference frames
#
# a frame is one addressing rule into one reference image. an anchor spec is (frame, a, b), where
# a/b mean whatever that frame's origin() says they mean - a cell column and a RIP row for the
# level bands, a cell column and a LEVEL row for the coin rooms, raw pixels for the sheet.

IMAGES = {
    "1-1": "smbd_ch_1-1.png",
    "1-2": "smbd_ch_1-2.png",
    "1-3": "smbd_ch_1-3.png",
    "1-4": "smbd_ch_1-4.png",
    "sheet": "sheet_tileset.png",
}
# the sheet is the one reference the tool runs without
OPTIONAL_IMAGES = ("sheet",)

# the 1-1 capture's overworld frame: cell (col, row) is 16x16 at (16 * col, 2 + 16 * row), row
# being a RIP row - level row 2 is rip row 0
RIP_Y0 = 2
# the bonus room hangs under the level on its own black background. its left edge is x 879 and
# its ground row's top edge y 378, and from there its cells fall on the level's own 16px pitch,
# so a room cell is addressed by the AREA grid's own (column, level row)
BONUS_X0 = 879
BONUS_GROUND_Y = 378
BONUS_GROUND_ROW = 13

# the 1-2 capture's upper band: cell (col, row) is at (16 * col, 8 + 16 * row), row a RIP row
# again with level row 2 at rip row 0, so the roof is rip row 0 and the floor rip row 11. from
# this column on the band is the above-ground ending, drawn on sky rather than black
RIP12_Y0 = 8
RIP12_ENDING_COLUMN = 192
# the pipe coin room in the band under it, addressed by the AREA grid's (column, level row)
ROOM12_X0 = 1632
ROOM12_Y0 = 201
ROOM12_TOP_ROW = 2
ROOM12_COLUMNS = 16

# the 1-3 capture: 1-2's band offsets exactly, on open sky the whole way
RIP13_Y0 = 8

# the 1-4 capture: no offset at all, 140 columns and 13 rows, rip row 0 being level row 1
RIP14_COLUMNS = 140
RIP14_ROWS = 13

# the sheet lays its global blocks out on a 17px pitch from x 4, all on one row at y 197
SHEET_BLOCK_Y = 197
SHEET_QUESTION_X = 4  # frame 1, the bright "?" - the frame an unhit block rests on
SHEET_SPENT_X = 55  # the used block, which no unhit level can show
SHEET_COIN_X = 155  # the world coin's first spin frame


class Frame:
    """one addressing rule into one reference image.

    origin(a, b) answers the top-left pixel of the cell, or None when the frame does not hold it.
    dark(a, b) says whether that cell sits on a black backdrop rather than sky, which decides
    which palette set reads it. remap is applied to every pixel (the sheet's own colour shift)."""

    def __init__(self, image, origin, dark=False, remap=None, band=False, palette=None):
        self.image = image
        self._origin = origin
        self._dark = dark
        self.remap = remap
        # the palette set that reads this frame's cells; a dark frame is the underground's unless
        # it says otherwise (the castle is drawn on black too, under a set of its own)
        self._palette = palette
        # a "band" frame is a whole level laid out on the capture's own 16px grid, which is the
        # only kind of frame the occurrence count can sweep
        self.band = band

    def origin(self, a, b):
        return self._origin(a, b)

    def dark(self, a, b):
        return self._dark(a, b) if callable(self._dark) else self._dark

    def palette(self, a, b):
        if self._palette is not None:
            return self._palette
        return "underground" if self.dark(a, b) else "overworld"


FRAMES = {
    "rip": Frame("1-1", lambda c, r: (c * CELL, RIP_Y0 + r * CELL), band=True),
    "bonus": Frame("1-1",
                   lambda c, r: (BONUS_X0 + c * CELL,
                                 BONUS_GROUND_Y - CELL * (BONUS_GROUND_ROW - r)),
                   dark=True),
    "rip12": Frame("1-2", lambda c, r: (c * CELL, RIP12_Y0 + r * CELL),
                   dark=lambda c, r: c < RIP12_ENDING_COLUMN, band=True),
    # the band is sixteen columns wide and starts at level row 2: the room's own column 16, its
    # shaft's right side, is past its edge, and its wall's rows 0-1 are above it, where the image
    # holds the level's ending and the run's floor
    "room12": Frame("1-2",
                    lambda c, r: None if (c >= ROOM12_COLUMNS or r < ROOM12_TOP_ROW)
                    else (ROOM12_X0 + c * CELL, ROOM12_Y0 + CELL * (r - ROOM12_TOP_ROW)),
                    dark=True),
    "rip13": Frame("1-3", lambda c, r: (c * CELL, RIP13_Y0 + r * CELL), band=True),
    "rip14": Frame("1-4",
                   lambda c, r: None if (c >= RIP14_COLUMNS or r >= RIP14_ROWS) else (c * CELL, r * CELL),
                   dark=True, band=True, palette="castle"),
    "sheet": Frame("sheet", lambda x, y: (x, y), remap=SHEET_TO_RIP),
}


def dark_frame(spec):
    """whether a cell comes off a black backdrop: either coin room, 1-2's underground run (its
    ending, from RIP12_ENDING_COLUMN on, is open sky like 1-1), or the castle."""
    return FRAMES[spec[0]].dark(spec[1], spec[2])


def palette_of(spec):
    """the name of the palette set that reads a cell: overworld, underground or castle."""
    return FRAMES[spec[0]].palette(spec[1], spec[2])


def slot_set(slot):
    """a family's slot may name its set, "castle:ground"; a bare slot is the overworld's."""
    if ":" in slot:
        set_name, slot = slot.split(":", 1)
        return SLOT_SETS[set_name], slot
    return SLOTS, slot


def Q(anchor, qx, qy):
    """one 8x8 quadrant of an anchor cell: qx/qy are 0 for the left/top half."""
    return (anchor, qx, qy)


def whole(anchor):
    return [(anchor, 0, 0), (anchor, 1, 0), (anchor, 0, 1), (anchor, 1, 1)]


# ---------------------------------------------------------------- 1-1
#
# anchors: one clean, sprite-free instance of each family, by cell. "rip" cells are (column, RIP
# row) in 1-1's overworld frame; "bonus" cells are (column, LEVEL row) in its room's frame;
# "sheet" cells are (x, y) pixel origins on the tileset sheet.
ANCHORS_11 = {
    # the ground row is 179 identical cells out of 208; the rest carry a goomba or a pit
    "ground": ("rip", 5, 11),
    "brick": ("rip", 19, 7),  # the left end of the first brick run
    "question": ("sheet", SHEET_QUESTION_X, SHEET_BLOCK_Y),
    "spent": ("sheet", SHEET_SPENT_X, SHEET_BLOCK_Y),
    "hard": ("rip", 183, 10),  # inside the big staircase, where nothing walks
    "pipe_lip_l": ("rip", 27, 9),
    "pipe_lip_r": ("rip", 28, 9),
    "pipe_body_l": ("rip", 27, 10),
    "pipe_body_r": ("rip", 28, 10),
    # the five-cell cloud over the first pipe, capture columns 26-30 on rip rows 2-3: a left cap,
    # three middles and a right cap. an earlier pass anchored these three columns in, on 27-29,
    # and so gave all six cloud kinds the MIDDLE cell's art - the caps were the bug this pass
    # found. smb draws a cloud offset half a cell, which is why a cap cell carries art in one
    # quadrant only and the middles carry the puffs
    "cloud_cap_t": ("rip", 26, 2),
    "cloud_mid_t": ("rip", 27, 2),
    "cloud_capr_t": ("rip", 30, 2),
    "cloud_cap_b": ("rip", 26, 3),
    "cloud_mid_b": ("rip", 27, 3),
    "cloud_capr_b": ("rip", 30, 3),
    # the five-cell hill before the first mid staircase, capture columns 144-148 standing on rip
    # row 10. its peak is one row ABOVE the row the slopes and the upper fill share, which the same
    # earlier pass missed: it anchored the peak at (146, 9), the fill cell under the dome. and the
    # interior is two cells, not one - "fill" carries smb's two-pixel shading mark in its upper
    # right and "core", the middle of the bottom row, is flat green across
    "hill_slope": ("rip", 145, 9),
    "hill_peak": ("rip", 146, 8),
    "hill_slope_r": ("rip", 147, 9),
    "hill_fill": ("rip", 146, 9),
    "hill_core": ("rip", 146, 10),
    # the five-cell bush the level opens with, capture columns 10-14. a bush is the cloud's own
    # shape in the pipe's greens, so its caps are half-cells too
    "bush_cap": ("rip", 10, 10),
    "bush_mid": ("rip", 11, 10),
    "bush_capr": ("rip", 14, 10),
    # the flag: ball, pennant, shaft, and the castle behind it
    "flag_ball": ("rip", 196, 0),
    "flag_cloth": ("rip", 195, 1),
    "flag_cloth_pole": ("rip", 196, 1),
    "flag_pole": ("rip", 196, 4),
    "castle_wall": ("rip", 202, 7),
    "castle_crenel": ("rip", 201, 6),
    "castle_crenel_inner": ("rip", 202, 8),
    "castle_window": ("rip", 201, 7),
    # the tower's other window. the opening hugs the middle column from the right, so this cell
    # is the left one's two tiles swapped and NOT its x flip - see the SHARING report
    "castle_window_right": ("rip", 203, 7),
    "castle_door_top": ("rip", 202, 9),
    "castle_door": ("rip", 202, 10),
    # the coin's first spin frame off the sheet; the bonus room's loose coins are the same
    # shape under the room's own colours, which the audit below proves
    "coin": ("sheet", SHEET_COIN_X, SHEET_BLOCK_Y),
    # the room's own masonry, only ever read by --check
    "ug_brick": ("bonus", 0, 3),
    "ug_ground": ("bonus", 0, 13),
    # the bonus room's exit: a SIDEWAYS pipe, its mouth facing left at room columns 13-14 on rows
    # 11-12 and its body running right into the shaft at column 15. it is not the vertical pipe
    # transposed - the vertical pipe's cross section is 2px of backdrop, a 1px rim, 3 light, 2
    # dark, 5 light, 1 dark, 2 light, and the sideways one's is a 1px rim, 4 light, 2 dark, 5
    # light, 1 dark, 3 light. no backdrop margin and one more light row: the roof sits flush on the
    # cell's own top edge
    "pipe_side_mouth_t": ("bonus", 13, 11),
    "pipe_side_body_t": ("bonus", 14, 11),
    "pipe_side_mouth_b": ("bonus", 13, 12),
    "pipe_side_body_b": ("bonus", 14, 12),
    # and the shaft cell the body runs into, in both rows of the mouth: the body's rim and joint
    # continue over the shaft's own left column, so it is neither a body cell nor a side one
    "bonus_joint_t": ("bonus", 15, 11),
    "bonus_joint_b": ("bonus", 15, 12),
}

# (png stem, png2tiles --name, palette slot, [quadrant per tile]). each png is one column of 8x8
# tiles so that png2tiles' reading order IS the order set_bkg_data wants, and each list below is
# written in the rom's own tile-id order - read the loader in assets_data.c beside it.
FAMILIES_11 = [
    # 0xa0-0xa3: the surface block's upper pair, then the fill block's, which is the same pair
    ("ground", "Ground", "ground", [
        Q("ground", 0, 0), Q("ground", 1, 0), Q("ground", 0, 0), Q("ground", 1, 0)]),
    # 0xf8-0xf9: the lower pair both blocks end on
    ("ground_lower", "GroundLower", "ground", [Q("ground", 0, 1), Q("ground", 1, 1)]),
    ("brick", "Brick", "brick", [
        Q("brick", 0, 0), Q("brick", 1, 0), Q("brick", 0, 1), Q("brick", 1, 1)]),
    ("question", "Question", "question", [
        Q("question", 0, 0), Q("question", 1, 0), Q("question", 0, 1), Q("question", 1, 1)]),
    ("spent", "Spent", "spent", [
        Q("spent", 0, 0), Q("spent", 1, 0), Q("spent", 0, 1), Q("spent", 1, 1)]),
    ("hard", "Hard", "brick", [
        Q("hard", 0, 0), Q("hard", 1, 0), Q("hard", 0, 1), Q("hard", 1, 1)]),
    # the bonus room's brick is the overworld's under the room's own colours EXCEPT along its
    # top row, where the overworld paints a solid highlight over the two mortar joints and the
    # room leaves them showing. that is two tiles, and an underground load writes them over
    # kTileBrickTl/kTileBrickTr the way a castle load writes its masonry over the ground family
    ("brick_underground", "BrickUnderground", "brick", [
        Q("ug_brick", 0, 0), Q("ug_brick", 1, 0)]),
    # 0xb0-0xbb, in id order: the lip's left cell, the lip's right cell and the body's two cells
    # all get their own four quadrants, because the capture's pipe is neither left-right mirrored
    # nor does it share a tile column between the two cells (see the SHARING report)
    ("pipe", "Pipe", "pipe", [
        Q("pipe_lip_l", 0, 0), Q("pipe_lip_l", 1, 0), Q("pipe_lip_r", 0, 0),
        Q("pipe_lip_l", 0, 1), Q("pipe_lip_l", 1, 1), Q("pipe_lip_r", 0, 1),
        Q("pipe_body_l", 0, 0), Q("pipe_body_l", 1, 0), Q("pipe_body_r", 0, 0),
        Q("pipe_lip_r", 1, 0), Q("pipe_lip_r", 1, 1), Q("pipe_body_r", 1, 0)]),
    ("coin", "Coin", "coin", [
        Q("coin", 0, 0), Q("coin", 1, 0), Q("coin", 0, 1), Q("coin", 1, 1)]),
    # bank 1 0x72-0x7d: the sideways pipe, four 8px bands down a 32px tall run. the mouth column
    # needs a left and a right tile per band (the rim runs down its left edge and the joint to the
    # body down its right); the body column is uniform across its 16px, so one tile per band
    # serves both of its halves. twelve tiles, where the hand-drawn array this replaces had nine -
    # it shared one middle pair between the mouth's two cells and the capture does not
    ("pipe_side", "PipeSide", "pipe", [
        Q("pipe_side_mouth_t", 0, 0), Q("pipe_side_mouth_t", 1, 0),
        Q("pipe_side_mouth_t", 0, 1), Q("pipe_side_mouth_t", 1, 1),
        Q("pipe_side_mouth_b", 0, 0), Q("pipe_side_mouth_b", 1, 0),
        Q("pipe_side_mouth_b", 0, 1), Q("pipe_side_mouth_b", 1, 1),
        Q("pipe_side_body_t", 0, 0), Q("pipe_side_body_t", 0, 1),
        Q("pipe_side_body_b", 0, 0), Q("pipe_side_body_b", 0, 1)]),
    # bank 1 scenery. 0x35-0x44 is the cloud's left cap and middle, over two cells each
    ("cloud", "Cloud", "sky", [
        Q("cloud_cap_t", 0, 0), Q("cloud_cap_t", 1, 0),
        Q("cloud_cap_t", 0, 1), Q("cloud_cap_t", 1, 1),
        Q("cloud_mid_t", 0, 0), Q("cloud_mid_t", 1, 0),
        Q("cloud_mid_t", 0, 1), Q("cloud_mid_t", 1, 1),
        Q("cloud_cap_b", 0, 0), Q("cloud_cap_b", 1, 0),
        Q("cloud_cap_b", 0, 1), Q("cloud_cap_b", 1, 1),
        Q("cloud_mid_b", 0, 0), Q("cloud_mid_b", 1, 0),
        Q("cloud_mid_b", 0, 1), Q("cloud_mid_b", 1, 1)]),
    # 0x8d-0x94: the right cap, which is NOT the left one mirrored
    ("cloud_right", "CloudRight", "sky", [
        Q("cloud_capr_t", 0, 0), Q("cloud_capr_t", 1, 0),
        Q("cloud_capr_t", 0, 1), Q("cloud_capr_t", 1, 1),
        Q("cloud_capr_b", 0, 0), Q("cloud_capr_b", 1, 0),
        Q("cloud_capr_b", 0, 1), Q("cloud_capr_b", 1, 1)]),
    # 0x45-0x50: peak, slope, fill. the right slope IS the left one mirrored and keeps its flip
    ("hill", "Hill", "pipe", [
        Q("hill_peak", 0, 0), Q("hill_peak", 1, 0),
        Q("hill_peak", 0, 1), Q("hill_peak", 1, 1),
        Q("hill_slope", 0, 0), Q("hill_slope", 1, 0),
        Q("hill_slope", 0, 1), Q("hill_slope", 1, 1),
        Q("hill_fill", 0, 0), Q("hill_fill", 1, 0),
        Q("hill_fill", 0, 1), Q("hill_fill", 1, 1)]),
    ("bush", "Bush", "pipe", [
        Q("bush_cap", 0, 0), Q("bush_cap", 1, 0),
        Q("bush_cap", 0, 1), Q("bush_cap", 1, 1),
        Q("bush_mid", 0, 0), Q("bush_mid", 1, 0),
        Q("bush_mid", 0, 1), Q("bush_mid", 1, 1)]),
    # 0x95-0x98: the right bush cap, which is not the left one mirrored either
    ("bush_right", "BushRight", "pipe", [
        Q("bush_capr", 0, 0), Q("bush_capr", 1, 0),
        Q("bush_capr", 0, 1), Q("bush_capr", 1, 1)]),
    # 0x22-0x2f: the wall (one tile the whole cell repeats), the crenel's left half, then the
    # window, the door's arch and the door, four quadrants each
    ("castle", "Castle", "brick", [
        Q("castle_wall", 0, 0), Q("castle_crenel", 0, 0),
        Q("castle_window", 0, 0), Q("castle_window", 1, 0),
        Q("castle_window", 0, 1), Q("castle_window", 1, 1),
        Q("castle_door_top", 0, 0), Q("castle_door_top", 1, 0),
        Q("castle_door_top", 0, 1), Q("castle_door_top", 1, 1),
        Q("castle_door", 0, 0), Q("castle_door", 1, 0),
        Q("castle_door", 0, 1), Q("castle_door", 1, 1)]),
    ("castle_crenel_inner", "CastleCrenelInner", "brick", [Q("castle_crenel_inner", 0, 0)]),
    # 0x99-0x9a: the right half of each crenel, which the capture draws differently from the left
    ("castle_crenel_right", "CastleCrenelRight", "brick", [
        Q("castle_crenel", 1, 0), Q("castle_crenel_inner", 1, 0)]),
    # 0x59-0x5a: the shaft, whose top and bottom halves are the same so one row of tiles repeats
    ("flag_pole", "FlagPole", "pipe", [Q("flag_pole", 0, 0), Q("flag_pole", 1, 0)]),
    # the ball, whose two tiles the rom holds at the two ends of the scenery run - 0x30 and 0x5d -
    # so one family is loaded into them with two calls. it sits in the LOWER half of its cell (the
    # rom's table puts kTileFlagBallL/R in the cell's Bl/Br), and it is the one flag piece that is
    # not on the sky slot: the capture draws it in the pipe's own two greens over black, and
    # kPaletteRom pins kBlockFlagBall to kScenPipe to match
    ("flag_ball", "FlagBall", "pipe", [Q("flag_ball", 0, 1), Q("flag_ball", 1, 1)]),
    # 0x31-0x34: the pennant's two tiles and the pennant-over-shaft cell's two
    ("flag_head", "FlagHead", "sky", [
        Q("flag_cloth", 1, 0), Q("flag_cloth", 1, 1),
        Q("flag_cloth_pole", 0, 0), Q("flag_cloth_pole", 0, 1)]),
    # 0x5c: a blank scenery cell, which every decorative kind's unused half reads
    ("scen_tail", "ScenTail", "sky", [None]),
]

# the kinds the level stands, as kind -> (label, anchor, palette slot[, mirrored]). kind numbers
# are kBlock* from mario.h and must not move; the slot is the one kPaletteRom pins the kind to;
# "mirrored" says the rom draws the kind as its anchor's twin under kCamAttrXFlip, so the audit
# has to compare against the mirror of the anchor rather than the anchor
KINDS_11 = {
    1: ("ground", "ground", "ground"),
    2: ("brick", "brick", "brick"),
    3: ("question", "question", "question"),
    4: ("hard", "hard", "brick"),
    5: ("pipe lip left", "pipe_lip_l", "pipe"),
    6: ("pipe lip right", "pipe_lip_r", "pipe"),
    7: ("pipe body left", "pipe_body_l", "pipe"),
    8: ("pipe body right", "pipe_body_r", "pipe"),
    9: ("stair", "hard", "brick"),
    10: ("flag pole", "flag_pole", "pipe"),
    11: ("castle wall", "castle_wall", "brick"),
    13: ("coin", "coin", "coin"),
    18: ("ground fill", "ground", "ground"),
    19: ("castle crenel", "castle_crenel", "brick"),
    20: ("castle window", "castle_window", "brick"),
    21: ("castle door top", "castle_door_top", "brick"),
    22: ("castle door", "castle_door", "brick"),
    23: ("flag ball", "flag_ball", "pipe"),
    24: ("flag cloth", "flag_cloth", "sky"),
    25: ("cloud top left", "cloud_cap_t", "sky"),
    26: ("cloud top mid", "cloud_mid_t", "sky"),
    27: ("cloud top right", "cloud_capr_t", "sky"),
    28: ("cloud bottom left", "cloud_cap_b", "sky"),
    29: ("cloud bottom mid", "cloud_mid_b", "sky"),
    30: ("cloud bottom right", "cloud_capr_b", "sky"),
    31: ("hill peak", "hill_peak", "pipe"),
    32: ("hill slope left", "hill_slope", "pipe"),
    33: ("hill slope right", "hill_slope", "pipe", True),
    34: ("hill fill", "hill_fill", "pipe"),
    51: ("hill core", "hill_core", "pipe"),
    35: ("bush left", "bush_cap", "pipe"),
    36: ("bush mid", "bush_mid", "pipe"),
    37: ("bush right", "bush_capr", "pipe"),
    38: ("pipe side mouth top", "pipe_side_mouth_t", "pipe"),
    39: ("pipe side mouth bottom", "pipe_side_mouth_b", "pipe"),
    40: ("pipe side body top", "pipe_side_body_t", "pipe"),
    41: ("pipe side body bottom", "pipe_side_body_b", "pipe"),
    42: ("castle crenel inner", "castle_crenel_inner", "brick"),
    50: ("castle window right", "castle_window_right", "brick"),
    52: ("pipe joint top", "pipe_joint_t", "pipe"),
    53: ("pipe joint bottom", "pipe_joint_b", "pipe"),
    # the pennant-over-shaft cell is the one kind whose two tile columns wear different slots
    # (terrain.c hands its right column kBlockFlagPole's), so no single slot can audit it
    47: ("flag pole + cloth", "flag_cloth_pole", None),
}

# relations the rom leans on: a quadrant two kinds share, or a kind the rom draws as its twin
# mirrored (kCamAttrXFlip). each is checked against the capture and reported either way
RELATIONS_11 = [
    ("hill slope right == mirror(hill slope left)", "hill_slope_r", "mirror", "hill_slope"),
    ("cloud cap right top == mirror(cap left top)", "cloud_capr_t", "mirror", "cloud_cap_t"),
    ("cloud cap right bottom == mirror(cap left bottom)", "cloud_capr_b", "mirror", "cloud_cap_b"),
    ("bush cap right == mirror(bush cap left)", "bush_capr", "mirror", "bush_cap"),
    ("pipe lip right == mirror(pipe lip left)", "pipe_lip_r", "mirror", "pipe_lip_l"),
    ("pipe body right == mirror(pipe body left)", "pipe_body_r", "mirror", "pipe_body_l"),
    ("castle window right == mirror(window left)", "castle_window_right", "mirror",
     "castle_window"),
]

QUADRANTS_11 = [
    ("pipe lip: left cell's right column == right cell's left column",
     [(("pipe_lip_l", 1, 0)), (("pipe_lip_l", 1, 1))],
     [(("pipe_lip_r", 0, 0)), (("pipe_lip_r", 0, 1))]),
    ("pipe body: left cell's right tile == right cell's left tile",
     [("pipe_body_l", 1, 0)], [("pipe_body_r", 0, 0)]),
    ("pipe body: the cell's top half repeats as its bottom half",
     [("pipe_body_l", 0, 0), ("pipe_body_l", 1, 0)],
     [("pipe_body_l", 0, 1), ("pipe_body_l", 1, 1)]),
    ("castle wall: one 8x8 tile fills the whole cell",
     [("castle_wall", 0, 0), ("castle_wall", 0, 0), ("castle_wall", 0, 0)],
     [("castle_wall", 1, 0), ("castle_wall", 0, 1), ("castle_wall", 1, 1)]),
    ("castle crenel: its lower half is the wall tile",
     [("castle_crenel", 0, 1), ("castle_crenel", 1, 1)],
     [("castle_wall", 0, 0), ("castle_wall", 0, 0)]),
    ("castle crenel inner: its lower half is the wall tile",
     [("castle_crenel_inner", 0, 1), ("castle_crenel_inner", 1, 1)],
     [("castle_wall", 0, 0), ("castle_wall", 0, 0)]),
    ("castle crenel: its two top tiles are the same",
     [("castle_crenel", 0, 0)], [("castle_crenel", 1, 0)]),
    ("castle crenel inner: its two top tiles are the same",
     [("castle_crenel_inner", 0, 0)], [("castle_crenel_inner", 1, 0)]),
    ("castle window right: its two tiles are the left window's, swapped",
     [("castle_window_right", 0, 0), ("castle_window_right", 1, 0),
      ("castle_window_right", 0, 1), ("castle_window_right", 1, 1)],
     [("castle_window", 1, 0), ("castle_window", 0, 0),
      ("castle_window", 1, 1), ("castle_window", 0, 1)]),
    ("flag shaft: the cell's top half repeats as its bottom half",
     [("flag_pole", 0, 0), ("flag_pole", 1, 0)],
     [("flag_pole", 0, 1), ("flag_pole", 1, 1)]),
    ("flag ball: its lower half is the shaft's upper half",
     [("flag_ball", 0, 1), ("flag_ball", 1, 1)],
     [("flag_pole", 0, 0), ("flag_pole", 1, 0)]),
    ("flag cloth: the cell left of the shaft has a blank left half",
     [("flag_cloth", 0, 0), ("flag_cloth", 0, 1)], None),
    ("flag cloth over shaft: its right column is the plain shaft",
     [("flag_cloth_pole", 1, 0), ("flag_cloth_pole", 1, 1)],
     [("flag_pole", 1, 0), ("flag_pole", 1, 0)]),
    # the scenery families, re-measured this pass. smb draws a cloud (and a bush, which is the same
    # shape in the pipe's greens) offset half a cell, so a cap cell is drawn in exactly one of its
    # four quadrants and every other quadrant of it is sky
    ("cloud cap top: only its lower right is drawn",
     [("cloud_cap_t", 0, 0), ("cloud_cap_t", 1, 0), ("cloud_cap_t", 0, 1)], None),
    ("cloud cap right top: only its lower left is drawn",
     [("cloud_capr_t", 0, 0), ("cloud_capr_t", 1, 0), ("cloud_capr_t", 1, 1)], None),
    ("cloud cap bottom: only its upper right is drawn",
     [("cloud_cap_b", 0, 0), ("cloud_cap_b", 0, 1), ("cloud_cap_b", 1, 1)], None),
    ("cloud cap right bottom: only its upper left is drawn",
     [("cloud_capr_b", 1, 0), ("cloud_capr_b", 0, 1), ("cloud_capr_b", 1, 1)], None),
    ("cloud middle bottom: its lower half is sky",
     [("cloud_mid_b", 0, 1), ("cloud_mid_b", 1, 1)], None),
    ("cloud middle top: its lower two quadrants are the same tile",
     [("cloud_mid_t", 0, 1)], [("cloud_mid_t", 1, 1)]),
    ("bush cap: only its lower right is drawn",
     [("bush_cap", 0, 0), ("bush_cap", 1, 0), ("bush_cap", 0, 1)], None),
    ("bush cap right: only its lower left is drawn",
     [("bush_capr", 0, 0), ("bush_capr", 1, 0), ("bush_capr", 1, 1)], None),
    ("bush middle: its lower two quadrants are the same tile",
     [("bush_mid", 0, 1)], [("bush_mid", 1, 1)]),
    ("hill peak: its upper half is sky", [("hill_peak", 0, 0), ("hill_peak", 1, 0)], None),
    ("hill slope: its right column repeats one tile",
     [("hill_slope", 1, 0)], [("hill_slope", 0, 1)]),
    ("hill slope: its lower right is the hill's flat tile",
     [("hill_slope", 1, 1)], [("hill_core", 0, 0)]),
    ("hill core: all four quadrants are the hill's flat tile",
     [("hill_core", 1, 0), ("hill_core", 0, 1), ("hill_core", 1, 1)],
     [("hill_core", 0, 0), ("hill_core", 0, 0), ("hill_core", 0, 0)]),
    ("hill fill: only its upper right carries smb's shading mark",
     [("hill_fill", 0, 0), ("hill_fill", 0, 1), ("hill_fill", 1, 1)],
     [("hill_core", 0, 0), ("hill_core", 0, 0), ("hill_core", 0, 0)]),
    ("bonus room ground == overworld ground", whole("ug_ground"), whole("ground"), "recolour"),
    ("bonus room brick == overworld brick", whole("ug_brick"), whole("brick"), "recolour"),
    # the sideways pipe. the body column is flat across its width, which is what lets the rom draw
    # each of its 8px bands with one tile in both halves of the cell; the mouth column is not, and
    # its two cells do not share the middle pair the hand-drawn array used to give them
    ("pipe side body top: its left and right halves are the same tiles",
     [("pipe_side_body_t", 1, 0), ("pipe_side_body_t", 1, 1)],
     [("pipe_side_body_t", 0, 0), ("pipe_side_body_t", 0, 1)]),
    ("pipe side body bottom: its left and right halves are the same tiles",
     [("pipe_side_body_b", 1, 0), ("pipe_side_body_b", 1, 1)],
     [("pipe_side_body_b", 0, 0), ("pipe_side_body_b", 0, 1)]),
    ("pipe side mouth: the two cells share their middle pair",
     [("pipe_side_mouth_t", 0, 1), ("pipe_side_mouth_t", 1, 1)],
     [("pipe_side_mouth_b", 0, 0), ("pipe_side_mouth_b", 1, 0)]),
]

# AUDIT: the compiled 1-1 grid and its capture do not stand in the same columns everywhere, and
# the defect is the CAPTURE's. it is a stitch of screen-sized grabs, and it lost one column at its
# left edge and gained a duplicate one around its fifth screen: the level's own column 0 is not in
# the image at all. so a level column reads one column LEFT of itself for the first third of the
# level and level-for-level after the seam. the nes map and the 1-2 pass both agree with the
# json's geometry, so the json is right and this table is the correction:
COLUMNS_11 = ((0, 75, -1), (76, 207, 0))
# the pins, in order along the level: the opening big hill, whose bottom row is
# slope-fill-CORE-fill-slope and whose fill/core/fill cells sit at capture 0/1/2 - which puts the
# missing column at level 0 and not level 1; the first bush, five wide at level 11 and capture 10;
# the question block at level 16 (capture 15); the four pipes at level 28/38/46/57 (capture
# 27/37/45/56); the first ground gap at level 69-70 (capture 68-69); the three-wide bush at level
# 71-73 (capture 70-72); then, past the seam, the two lone bricks with a question block between
# them at level 77/78/79 (capture 77/78/79), the second gap at level 86-88 (capture 86-88) and the
# third at level 153-154 (capture 153-154). the duplicated column is pinned by the scenery the
# seam runs through: the capture draws a SIX-cell cloud at capture 74-79 where every other cloud in
# the level is three, four or five, and its 48-column neighbours at level 27, 123 and 171 are all
# five. so capture 75 is the duplicate - one middle cell of that cloud, repeated - and the cloud is
# level 75-79, five wide. that is the only feature in capture 73-76; everything else there is open
# sky in every row


# ---------------------------------------------------------------- 1-2
#
# "rip12" cells are (column, RIP row) in 1-2's upper band and "room12" cells (column, LEVEL row)
# in its coin room.
ANCHORS_12 = {
    # the underground run's own cells, read by the report and the 1-2 audit, and every one of them
    # 1-1's art under the underground colours: the floor is the ground block, the roof and the
    # walls the room's brick, the stair-steps the hard block, the pipes the pipe
    "ug_ground12": ("rip12", 5, 11),
    "ug_brick12": ("rip12", 0, 0),
    "ug_hard12": ("rip12", 23, 7),
    "ug_pipe_lip_l12": ("rip12", 109, 7),
    "ug_pipe_lip_r12": ("rip12", 110, 7),
    "ug_pipe_body_l12": ("rip12", 168, 0),
    "ug_pipe_body_r12": ("rip12", 169, 0),
    "ug_coin12": ("rip12", 180, 2),
    # its ending, on sky: the same ground block in the overworld's tan and brown, and the stair
    "end_ground12": ("rip12", 197, 11),
    "end_stair12": ("rip12", 203, 4),
    # the one family cut from this capture: the joint at the underground exit's shaft (real column
    # 168, the two rows of the mouth). the shaft's left cell there carries the sideways body's rim
    # along its top (or bottom) and the joint's dark line down its own left column, then the plain
    # body's right column past it. 1-2 stands two of these pairs and 1-1's room one
    "pipe_joint_t": ("rip12", 168, 6),
    "pipe_joint_b": ("rip12", 168, 7),
}

FAMILIES_12 = [
    # bank 1 0xe0-0xe4: the shaft joint, where that body runs into its vertical shaft. the shaft's
    # left cell there wears the body's rim and joint down its own left column and the plain
    # body's right column past it, so each of the two joint kinds needs two tiles of its own; the
    # fifth is a bank-1 copy of kTilePipeBodyM for the right column, because a kind's attribute
    # byte picks one vram bank for all four quadrants and the body's own tile lives in bank 0
    ("pipe_joint", "PipeJoint", "pipe", [
        Q("pipe_joint_t", 0, 0), Q("pipe_joint_t", 0, 1),
        Q("pipe_joint_b", 0, 0), Q("pipe_joint_b", 0, 1),
        Q("pipe_body_l", 1, 0)]),
]

RELATIONS_12 = []

QUADRANTS_12 = [
    # the 1-2 capture against 1-1's, through the palette swap: every underground cell the run is
    # built of is 1-1's art, and the ending's ground and stair are 1-1's own cells exactly
    ("1-2 floor == 1-1 ground", whole("ug_ground12"), whole("ground"), "recolour"),
    ("1-2 roof brick == 1-1 bonus room brick", whole("ug_brick12"), whole("ug_brick")),
    ("1-2 stair-step block == 1-1 hard block", whole("ug_hard12"), whole("hard"), "recolour"),
    ("1-2 pipe lip left == 1-1's", whole("ug_pipe_lip_l12"), whole("pipe_lip_l"), "recolour"),
    ("1-2 pipe lip right == 1-1's", whole("ug_pipe_lip_r12"), whole("pipe_lip_r"), "recolour"),
    ("1-2 pipe body left == 1-1's", whole("ug_pipe_body_l12"), whole("pipe_body_l"), "recolour"),
    ("1-2 pipe body right == 1-1's", whole("ug_pipe_body_r12"), whole("pipe_body_r"), "recolour"),
    ("1-2 loose coin == the sheet's coin", whole("ug_coin12"), whole("coin"), "recolour"),
    ("1-2 ending ground == 1-1 ground", whole("end_ground12"), whole("ground")),
    ("1-2 ending stair == 1-1 hard block", whole("end_stair12"), whole("hard")),
    # the shaft joint. its right column is the plain body's, its two cells are not each other's,
    # and 1-1's bonus room stands the identical pair at its own shaft
    ("pipe joint top: its right column is the plain body's right column",
     [("pipe_joint_t", 1, 0), ("pipe_joint_t", 1, 1)],
     [("pipe_body_l", 1, 0), ("pipe_body_l", 1, 0)], "recolour"),
    ("pipe joint bottom: its right column is the plain body's right column",
     [("pipe_joint_b", 1, 0), ("pipe_joint_b", 1, 1)],
     [("pipe_body_l", 1, 0), ("pipe_body_l", 1, 0)], "recolour"),
    ("pipe joint top: its left column is not the plain body's",
     [("pipe_joint_t", 0, 0), ("pipe_joint_t", 0, 1)],
     [("ug_pipe_body_l12", 0, 0), ("ug_pipe_body_l12", 0, 1)]),
    ("pipe joint: the two cells are not the same",
     [("pipe_joint_t", 0, 0), ("pipe_joint_t", 0, 1)],
     [("pipe_joint_b", 0, 0), ("pipe_joint_b", 0, 1)]),
    ("1-1 bonus room shaft joint top == 1-2's", whole("bonus_joint_t"), whole("pipe_joint_t")),
    ("1-1 bonus room shaft joint bottom == 1-2's", whole("bonus_joint_b"), whole("pipe_joint_b")),
]

# AUDIT: the 1-2 capture stands level for level: the underground run's column c is level c + 24
# (the 24-column start segment is in no smbd rip) and the ending's is c + 25, because level 216 -
# the right column of the coin room's exit shaft, which the bible carries up to row 0 to seal the
# seam - is not in the image and capture column 192 is already the ending's first column of sky.
# its one defect is at capture column 63, level 87, where the hanging wall's right column is drawn
# eight pixels wide over black on rows 1-7: a stitch that lost half a cell, since nothing in the
# game is drawn on an 8px grid and both the nes map and the rest of the capture keep the wall two
# columns wide
COLUMNS_12 = ((24, 215, -24), (217, 248, -25))


# ---------------------------------------------------------------- 1-3
#
# "rip13" cells are (column, RIP row) in 1-3's band, which uses 1-2's offsets and 1-1's row rule.
ANCHORS_13 = {
    # the tree. the four-wide canopy at level columns 18-21 on level row 12, with its trunk under
    # it: nothing walks there, no coin hangs over it and the nearest spawn is nine columns away, so
    # all four cells are clean. the canopy is a black outline over a flat bright green with a
    # scalloped underside whose notches have an 8px period; the trunk is the brick browns with a
    # dark stripe pair whose period is 8px in BOTH axes, so one tile stamped four times is the
    # whole column
    "tree_cap": ("rip13", 18, 10),
    "tree_mid": ("rip13", 19, 10),
    "tree_cap_r": ("rip13", 21, 10),
    "trunk": ("rip13", 19, 11),
    # the cells 1-3 shares with 1-1, so the report can say so cell for cell rather than only
    # kind by kind: the ground row, the flagpole's base block (kBlockHard, level 149 row 12), the
    # big castle's wall and a cloud middle
    "ground13": ("rip13", 5, 11),
    "hard13": ("rip13", 149, 10),
    "castle_wall13": ("rip13", 152, 6),
    "cloud_mid_t13": ("rip13", 39, 4),
}

FAMILIES_13 = [
    # bank 1 0x0a-0x0e: the canopy. five tiles is all it costs, because the shape repeats and the
    # right cap is the left one's exact mirror (see the relations): the left cap's rounded corner
    # pair, the plain top row that serves the cap's inner column and both of the middle's, the
    # cap's outer scalloped bottom, the plain scalloped bottom, and the middle's own bottom - the
    # only tile with the dark-green notch accent in it. kBlockTreeTopR draws these four with the
    # columns swapped and kCamAttrXFlip, which is why the right cap needs no tile of its own
    ("tree", "Tree", "pipe", [
        Q("tree_cap", 0, 0), Q("tree_cap", 1, 0),
        Q("tree_cap", 0, 1), Q("tree_cap", 1, 1),
        Q("tree_mid", 0, 1)]),
    # bank 1 0x0f: the trunk, one tile for the whole column, on the brick slot's browns
    ("trunk", "Trunk", "brick", [Q("trunk", 0, 0)]),
]

# 1-3 stands 1-1's kinds - every one of them reads pixel for pixel identical in this capture -
# and adds the four the tree costs. kBlockTreeTopR is drawn as the left cap under kCamAttrXFlip,
# which is what the "mirrored" flag says
KINDS_13 = dict(KINDS_11)
KINDS_13.update({
    43: ("tree canopy left", "tree_cap", "pipe"),
    44: ("tree canopy mid", "tree_mid", "pipe"),
    45: ("tree canopy right", "tree_cap", "pipe", True),
    46: ("tree trunk", "trunk", "brick"),
})

RELATIONS_13 = [
    # 1-1 taught us a right cap usually is NOT its left twin mirrored (the cloud's is 28 px out,
    # the bush's 13). the tree's is, to the pixel, which is why kBlockTreeTopR carries
    # kCamAttrXFlip instead of tiles of its own
    ("tree canopy right == mirror(canopy left)", "tree_cap_r", "mirror", "tree_cap"),
]

QUADRANTS_13 = [
    ("tree canopy middle: its two top tiles are the same",
     [("tree_mid", 0, 0)], [("tree_mid", 1, 0)]),
    ("tree canopy middle: its two bottom tiles are the same",
     [("tree_mid", 0, 1)], [("tree_mid", 1, 1)]),
    ("tree canopy: the left cap's inner top is the middle's plain top",
     [("tree_cap", 1, 0)], [("tree_mid", 0, 0)]),
    ("tree canopy: the left cap's inner bottom is NOT the middle's (the notch stops at a cap)",
     [("tree_cap", 1, 1)], [("tree_mid", 0, 1)]),
    ("tree trunk: one 8x8 tile fills the whole cell",
     [("trunk", 1, 0), ("trunk", 0, 1), ("trunk", 1, 1)],
     [("trunk", 0, 0), ("trunk", 0, 0), ("trunk", 0, 0)]),
    # and 1-3 against 1-1: every block and terrain family but the tree is 1-1's own art, which is
    # why this pass adds two families and no palette
    ("1-3 ground == 1-1 ground", whole("ground13"), whole("ground")),
    ("1-3 flagpole base block == 1-1 hard block", whole("hard13"), whole("hard")),
    ("1-3 castle wall == 1-1 castle wall", whole("castle_wall13"), whole("castle_wall")),
    ("1-3 cloud middle top == 1-1's", whole("cloud_mid_t13"), whole("cloud_mid_t")),
]

# AUDIT: the 1-3 capture needs no column correction. it is 160 columns wide, exactly the json's
# compiled length, and every feature stands where the json puts it: the opening castle at 0-4, the
# first tree at 18-21, the lift pits, the flagpole at 149 and the big castle at 152-160 all line up
# column for column, and the per-kind agreement below is 100% on every kind once the challenge
# mode's sprites are taken out. the only cells the capture and the json disagreed about were
# scenery the compiler had dropped rather than terrain: three clouds whose lower row the capture
# draws behind a canopy or the castle, which the bible now marks "clip" (see level-1-3.json)
COLUMNS_13 = None

# the challenge mode's own overlays, cell by cell, with the reason each one is not tile truth.
# every one of these was found by scanning the capture for a cell the grid calls empty that is not
# pure sky, and then identified against the level's spawn/object lists and the smbd block of the
# bible; the enemies themselves come out of the compiled spawn list instead (see load_enemies)
OVERLAYS_13 = [
    ("the ripper's credit, drawn over the sky at the top left",
     [(column, 2) for column in range(10)]),
    # challenge mode reddens one coin of a run and adds loose ones; the bible's smbd.red_coins
    # names all five, two of them over a bg coin cell the grid does hold
    ("challenge mode's red coin", [(28, 4), (56, 13), (88, 5), (112, 12), (134, 12)]),
    ("the challenge mode's yoshi egg", [(97, 7)]),
    # the three lifts are sprites in the rom (kTileLiftDeck); the capture caught each deck part way
    # along its track, so the cells it drew them in are not the cells the object list spawns them
    # at (47/82/89)
    ("a lift deck, a sprite in the rom",
     [(55, 6), (56, 6), (57, 6), (86, 8), (87, 8), (88, 8), (92, 9), (93, 9), (94, 9)]),
    # the one question block in the level. challenge mode draws a block's CONTENTS over its face
    # instead of the "?", which is why the question family is cut from the tileset sheet
    ("challenge mode draws the block's contents over its face", [(59, 10)]),
]


# ---------------------------------------------------------------- 1-4
#
# "rip14" cells are (column, RIP row) in the castle capture, rip row r being level row r + 1 and
# capture column c level column c + 16.
ANCHORS_14 = {
    # the masonry: a running bond of 8px bricks, white along each brick's top and left, light stone
    # face, dark shadow down its right and along its bottom, black mortar. an 8px period across, so
    # the cell is two tiles - an upper course and a lower one stepped half a brick. this cell is in
    # the roof, where nothing is ever drawn over it
    "masonry": ("rip14", 20, 1),
    # the castle's own solid block, the lone one over the corridor's mouth (level 23, row 6): the
    # seven that carry a firebar have the flame's top rows drawn into them, this one and the other
    # three lone blocks are clean. a brown face inside a grey-green border with a dot in each
    # corner and the backdrop showing through the corners: not the overworld's bevelled hard block
    # at all, though the rom draws it with that block's kind. a castle load writes these four over
    # kTileHardTl..Br the way it writes the masonry over the ground family
    "castle_hard": ("rip14", 7, 5),
    # the lava, at the second pit's left column (level 26, row 13): a white wave breaking along the
    # cell's top eight rows and flat red below, with an 8px period across
    "lava": ("rip14", 10, 12),
    # the bridge (level 128, row 10): four rows of white plank over a grey rail, red chain links
    # down every fourth column, and the links running out below. a 4px period across
    "bridge": ("rip14", 112, 9),
    # the axe (level 141, row 8): a double blade in two oranges over a grey haft, 16 px across and
    # its handle running down through the cell's lower half - four tiles, where the hand-drawn axe
    # was two blades over an empty lower half
    "axe": ("rip14", 125, 7),
    # the chain, from the bridge's far end (level 140, row 9) up to the axe: one diagonal line of
    # white over light stone through the cell's upper right and lower left quadrants, the other two
    # empty. the bible had no cell for it at all
    "chain": ("rip14", 124, 8),
}

FAMILIES_14 = [
    # bank 1 0x17 and 0x12: the upper course and the lower, in that order; the castle load writes
    # the same pair over the ground family's six ids. cut in the castle set's ground slot
    ("castle_brick", "CastleBrick", "castle:ground", [Q("masonry", 0, 0), Q("masonry", 0, 1)]),
    # bank 0 0xfa-0xfd at a castle load: the castle's own solid block, four quadrants
    ("castle_hard", "CastleHard", "castle:brick", [
        Q("castle_hard", 0, 0), Q("castle_hard", 1, 0),
        Q("castle_hard", 0, 1), Q("castle_hard", 1, 1)]),
    # bank 1 0x20-0x21: the wave and the flat red under it; kTileLavaDeep is the flat tile again
    ("lava", "Lava", "castle:coin", [Q("lava", 0, 0), Q("lava", 0, 1)]),
    # bank 1 0x15-0x16: the deck's upper and lower halves, each stamped across both quadrants
    ("bridge", "Bridge", "castle:neutral", [Q("bridge", 0, 0), Q("bridge", 0, 1)]),
    # bank 1 0x13-0x14 and 0x19-0x1a: the axe's four quadrants, blades over haft
    ("axe", "Axe", "castle:pipe", [
        Q("axe", 0, 0), Q("axe", 1, 0), Q("axe", 0, 1), Q("axe", 1, 1)]),
    # bank 1 0x1b-0x1c: the chain's two drawn quadrants, upper right then lower left
    ("chain", "Chain", "castle:ground", [Q("chain", 1, 0), Q("chain", 0, 1)]),
]

# the kinds a castle stands, every one of them read against this capture in the castle set. the
# question block's face is under its mushroom in the challenge rip, so it is an overlay below and
# not a kind here
KINDS_14 = {
    4: ("castle block", "castle_hard", "brick"),
    15: ("lava", "lava", "coin"),
    16: ("bridge", "bridge", "neutral"),
    17: ("axe", "axe", "pipe"),
    48: ("masonry", "masonry", "ground"),
    54: ("bridge chain", "chain", "ground"),
}

RELATIONS_14 = [
    # the castle's block is symmetric both ways, so its right half is its left half mirrored
    ("castle block right half == mirror(left half)", ("rip14", 7, 5), "mirror", "castle_hard"),
]

QUADRANTS_14 = [
    ("masonry: each course is one 8px tile stamped twice",
     [("masonry", 1, 0), ("masonry", 1, 1)], [("masonry", 0, 0), ("masonry", 0, 1)]),
    ("lava: the wave repeats every 8px",
     [("lava", 1, 0), ("lava", 1, 1)], [("lava", 0, 0), ("lava", 0, 1)]),
    ("lava: its lower half is flat red",
     [("lava", 0, 1)], [("lava", 0, 1)]),
    ("bridge: the deck repeats every 8px (and every 4 inside that)",
     [("bridge", 1, 0), ("bridge", 1, 1)], [("bridge", 0, 0), ("bridge", 0, 1)]),
    ("chain: its upper left and lower right quadrants are empty",
     [("chain", 0, 0), ("chain", 1, 1)], None),
    # expected "no": the hand-drawn axe this replaces left the lower half empty, and the capture
    # draws the haft there
    ("axe: its lower half is empty",
     [("axe", 0, 1), ("axe", 1, 1)], None),
    ("castle block: its lower half is its upper half upside down",
     [("castle_hard", 0, 1), ("castle_hard", 1, 1)],
     [("castle_hard", 0, 0), ("castle_hard", 1, 0)], "vflip"),
]

# AUDIT: the 1-4 capture stands column for column at +16 from its own left edge: its 140 columns
# are level 16-155, the opening platform and steps (level 0-15) come off the nes rip alone, and
# level 156-159 are past its right edge. every pivot, pit, stub, the bridge and the axe land where
# the bible puts them; what does not read as tile truth is declared below
COLUMNS_14 = ((16, 155, -16),)

# the pivots the seven firebars hang from, each with its chain of flame drawn straight down through
# the three cells under it; the pivot cells themselves are clean
FIREBAR_PIVOTS_14 = [(30, 10), (49, 6), (60, 6), (67, 6), (76, 9), (84, 9), (88, 4)]

OVERLAYS_14 = [
    ("the ripper's credit, drawn over the backdrop at the top left",
     [(column, 1) for column in range(16, 26)] + [(17, 2), (19, 2), (23, 2)]),
    ("a firebar's flame, drawn hanging straight down from its pivot",
     [(x, y + dy) for x, y in FIREBAR_PIVOTS_14 for dy in (1, 2, 3)]),
    ("challenge mode's red coin", [(30, 3), (50, 6), (86, 6), (77, 9), (136, 5)]),
    ("the challenge mode's yoshi egg", [(110, 5)]),
    ("the ripper's marker for a hidden block: a coin in a dotted outline",
     [(106, 9), (109, 9), (112, 9), (107, 5), (113, 5)]),
    ("challenge mode draws the block's contents over its face", [(30, 6)]),
    ("bowser, a sprite in the rom, and the deck cell under his feet",
     [(135, 8), (136, 8), (135, 9), (136, 9), (136, 10)]),
    ("a bowser fireball in flight", [(104, 10), (105, 10)]),
    ("the lift deck, a sprite in the rom, caught part way along its track", [(138, 6), (139, 6)]),
    # the stitch. around level 79-80 rows 3-4 the capture is a pixel out - the block at 80/4 and
    # the roof cell over it read one column left of the grid - and past the axe its last screen is
    # drawn two pixels short: the wall at 143 loses its last two columns of pixels in every row
    ("a stitch seam one pixel out", [(79, 3), (79, 4), (80, 3), (80, 4)]),
    ("the capture's last screen is drawn two pixels short of the wall at 143",
     [(143, row) for row in range(2, 14)] + [(141, 12), (142, 12), (141, 13), (142, 13)]),
    # and the room past it, which the smbd rip draws with no roof, a lower floor and its own
    # furniture where the nes rip - the one the bible follows, see level-1-4.json - draws the toad
    # room's masonry out to column 159
    ("the smbd ending room, which the bible does not follow",
     [(column, 2) for column in range(144, 156)] + [(column, 13) for column in range(144, 156)]
     + [(column, row) for column in range(146, 152) for row in (10, 11, 12)]),
]


# ---------------------------------------------------------------- the per-level table

LEVELS = {
    "1-1": {
        "image": "1-1",
        "frame": "rip",
        "area_frame": "bonus",
        "grid": ("level_1_1", "kLevel11Grid"),
        "columns": COLUMNS_11,
        "anchors": ANCHORS_11,
        "families": FAMILIES_11,
        "kinds": KINDS_11,
        "relations": RELATIONS_11,
        "quadrants": QUADRANTS_11,
        # the room's brick is the one family whose ART differs down there, not just its colours:
        # rip_tiles generates the room's own upper pair and the rom loads it over the overworld's,
        # so a dark cell of that kind is audited against the room's own anchor
        "room_anchors": {2: "ug_brick"},
        "overlays": [],
    },
    "1-2": {
        "image": "1-2",
        "frame": "rip12",
        "area_frame": "room12",
        "grid": ("level_1_2", "kLevel12Grid"),
        "columns": COLUMNS_12,
        "anchors": ANCHORS_12,
        "families": FAMILIES_12,
        # 1-2 stands 1-1's kinds and adds none of its own
        "kinds": KINDS_11,
        "relations": RELATIONS_12,
        "quadrants": QUADRANTS_12,
        "room_anchors": {2: "ug_brick"},
        "overlays": [],
    },
    "1-3": {
        "image": "1-3",
        "frame": "rip13",
        "area_frame": None,
        "grid": ("level_1_3", "kLevel13Grid"),
        "columns": COLUMNS_13,
        "anchors": ANCHORS_13,
        "families": FAMILIES_13,
        # 1-1's kinds plus the tree, whose right cap the rom mirrors off the left one
        "kinds": KINDS_13,
        "relations": RELATIONS_13,
        "quadrants": QUADRANTS_13,
        "room_anchors": {},
        "overlays": OVERLAYS_13,
        # how many cells tall a spawn's sprite is, per kEnemy* kind (default 1). a red koopa is
        # 16x24 and a red paratroopa slides a cell up and down around its spawn row, so both of
        # them cover rows the one-cell default does not. 1-1's and 1-2's tables leave this empty on
        # purpose: their passes measured their agreement with the one-cell mask and the numbers
        # they reported are the numbers this table still reproduces
        "enemy_rows": {2: 2, 4: 3},
    },
    "1-4": {
        "image": "1-4",
        "frame": "rip14",
        "area_frame": None,
        "grid": ("level_1_4", "kLevel14Grid"),
        "columns": COLUMNS_14,
        "anchors": ANCHORS_14,
        "families": FAMILIES_14,
        "kinds": KINDS_14,
        "relations": RELATIONS_14,
        "quadrants": QUADRANTS_14,
        "room_anchors": {},
        "overlays": OVERLAYS_14,
        # the castle capture holds level rows 1-13 in its thirteen rows, one more than the others
        "row0": 1,
        "rows": RIP14_ROWS,
    },
}

LEVEL_ORDER = ["1-1", "1-2", "1-3", "1-4"]

# every anchor of every level in one namespace: the relations deliberately reach across levels
# (1-2's whole audit is "this is 1-1's art under other colours", and 1-3's says the same), and a
# family's quadrant list names anchors by name alone. the frame in each spec says which capture the
# cell was cut from, so nothing here is ambiguous
ANCHORS = {}
for _level in LEVEL_ORDER:
    for _name, _spec in LEVELS[_level]["anchors"].items():
        if _name in ANCHORS:
            raise SystemExit("anchor %s is declared by two levels" % _name)
        ANCHORS[_name] = _spec

# and every family, in level order. one png per family, so the order only affects the report
FAMILIES = [family for level in LEVEL_ORDER for family in LEVELS[level]["families"]]


def level_column(level, column):
    """the capture column a level column stands in, or None for a column the capture does not
    hold. a level with no "columns" table stands column for column."""
    table = LEVELS[level]["columns"]
    if table is None:
        return column
    for lo, hi, shift in table:
        if lo <= column <= hi:
            return column + shift
    # 1-1 answers the level column itself outside its table (its own column 0 comes back as -1 and
    # falls off the image); 1-2 answers None, because the columns outside its table are the start
    # segment and the exit shaft, neither of which is in the image at all
    return column if level == "1-1" else None


def level_rows(level):
    """(the level row the capture's rip row 0 stands at, how many rip rows it holds)."""
    entry = LEVELS[level]
    return entry.get("row0", 2), entry.get("rows", LAST_RIP_ROW + 1)


# ---------------------------------------------------------------- reading the references


class Refs:
    def __init__(self, ref_dir, levels):
        self.images = {}
        wanted = {"sheet"} | {LEVELS[level]["image"] for level in levels}
        for key, name in IMAGES.items():
            if key not in wanted:
                continue
            path = os.path.join(ref_dir, name)
            if os.path.exists(path):
                self.images[key] = gbpng.read_png(path)
            elif key not in OPTIONAL_IMAGES:
                raise SystemExit("no reference image at %s (they are gitignored; copy it in)" % path)
        for level in levels:
            VERIFY[level](self)

    @property
    def sheet(self):
        return self.images.get("sheet")

    @staticmethod
    def _hex(img, x, y):
        r, g, b, _ = img.rgba(x, y)
        return "%02X%02X%02X" % (r, g, b)

    def pixel(self, frame_name, x, y):
        return self._hex(self.images[FRAMES[frame_name].image], x, y)

    def cell(self, spec):
        """a 16x16 cell as rows of RRGGBB, in whichever frame the anchor names."""
        frame = FRAMES.get(spec[0])
        if frame is None:
            raise SystemExit("unknown frame %r" % (spec[0],))
        img = self.images.get(frame.image)
        if img is None:
            return None
        where = frame.origin(spec[1], spec[2])
        if where is None:
            return None
        x0, y0 = where
        if x0 < 0 or y0 < 0 or x0 + CELL > img.width or y0 + CELL > img.height:
            return None
        rows = [[self._hex(img, x0 + x, y0 + y) for x in range(CELL)] for y in range(CELL)]
        if frame.remap:
            rows = [[frame.remap.get(p, p) for p in row] for row in rows]
        return rows

    def anchor(self, name, recolour=False):
        rows = self.cell(ANCHORS[name])
        if rows is None:
            raise SystemExit("anchor %s falls outside its reference image" % name)
        if recolour and dark_frame(ANCHORS[name]):
            table = BONUS_RECOLOUR.get(name, BONUS_TO_SLOT)
            rows = [[table.get(p, p) for p in row] for row in rows]
        return rows

    def has(self, name):
        return self.images.get(FRAMES[ANCHORS[name][0]].image) is not None


def verify_11(refs):
    """the y offset is only trustworthy if the ground's top edge really is at y 178, so the
    cell above rip row 11 is open sky and rip row 11 itself has no sky left in it. column 5
    is chosen because nothing stands on it and no sprite walks over it."""
    above = {refs.pixel("rip", 5 * CELL + x, RIP_Y0 + 10 * CELL + y)
             for x in range(CELL) for y in range(CELL)}
    if above != {SKY}:
        raise SystemExit("the cell above the ground at column 5 is not sky; the y offset moved")
    row = {refs.pixel("rip", 5 * CELL + x, RIP_Y0 + 11 * CELL + y)
           for x in range(CELL) for y in range(CELL)}
    if SKY in row or refs.pixel("rip", 5 * CELL, RIP_Y0 + 11 * CELL - 1) != SKY:
        raise SystemExit("y=178 is not the ground's top edge; the capture's y offset moved")


def verify_12(refs):
    """the same check on the 1-2 capture: the floor's top edge at y 184 under a cell of black
    in column 5 (inside the entry shaft, where nothing stands), the ending's first column of
    open sky at 192, and the coin room's roof brick at the top left of the lower band."""
    above = {refs.pixel("rip12", 5 * CELL + x, RIP12_Y0 + 10 * CELL + y)
             for x in range(CELL) for y in range(CELL)}
    if above != {UNDERGROUND}:
        raise SystemExit("the cell above 1-2's floor at column 5 is not black; the y offset moved")
    edge = RIP12_Y0 + 11 * CELL
    if (refs.pixel("rip12", 5 * CELL, edge - 1) != UNDERGROUND
            or refs.pixel("rip12", 5 * CELL, edge) != "008888"
            or refs.pixel("rip12", 5 * CELL + 1, edge) != "B8F8F0"):
        raise SystemExit("y=184 is not 1-2's floor top edge; the capture's y offset moved")
    ending = {refs.pixel("rip12", RIP12_ENDING_COLUMN * CELL + x, RIP12_Y0 + y)
              for x in range(CELL) for y in range(CELL)}
    if ending != {SKY}:
        raise SystemExit("1-2's column 192 is not the ending's sky; the capture's stitch moved")
    roof = refs.cell(("room12", 3, ROOM12_TOP_ROW))
    if roof is None or {p for row in roof for p in row} != {UNDERGROUND, "008888"}:
        raise SystemExit("1-2's coin room roof is not at x 1680, y 201; the band moved")


def verify_13(refs):
    """1-3's frame, derived the same way and checked the same way: the ground's top edge at
    y 184 = 8 + 16 * 11 with a clear cell of sky over it in column 5, which is open ground
    nothing stands on. the capture is exactly 160 columns and 12 rows of cell, so its last
    scanline IS the ground row's last - level row 14 is not in it."""
    above = {refs.pixel("rip13", 5 * CELL + x, RIP13_Y0 + 10 * CELL + y)
             for x in range(CELL) for y in range(CELL)}
    if above != {SKY}:
        raise SystemExit("the cell above 1-3's ground at column 5 is not sky; the y offset moved")
    edge = RIP13_Y0 + 11 * CELL
    if refs.pixel("rip13", 5 * CELL, edge - 1) != SKY:
        raise SystemExit("y=184 is not 1-3's ground top edge; the capture's y offset moved")
    row = {refs.pixel("rip13", 5 * CELL + x, edge + y) for x in range(CELL) for y in range(CELL)}
    if SKY in row:
        raise SystemExit("1-3's ground row at column 5 still has sky in it; the y offset moved")
    img = refs.images["1-3"]
    if img.width != 160 * CELL or img.height != RIP13_Y0 + 12 * CELL:
        raise SystemExit("1-3's capture is %dx%d, not the 2560x200 the frame assumes"
                         % (img.width, img.height))


def verify_14(refs):
    """1-4's frame has no offset, so the check is the cells themselves: the roof at rip row 1 and
    the floor at rip row 12 are masonry in column 20 (level 36, deep in the corridor), the second
    lava pit's left column (level 26) reads red along its top, and the image is 140 columns and
    13 rows plus a margin."""
    img = refs.images["1-4"]
    if img.width < RIP14_COLUMNS * CELL or img.height < RIP14_ROWS * CELL:
        raise SystemExit("1-4's capture is %dx%d, too small for %d columns and %d rows"
                         % (img.width, img.height, RIP14_COLUMNS, RIP14_ROWS))
    for row in (1, 12):
        cell = refs.cell(("rip14", 20, row))
        if {p for line in cell for p in line} != {CASTLE, "FFFFFF", "BFBFBF", "7F7F7F"}:
            raise SystemExit("1-4's rip row %d at column 20 is not masonry; the frame moved" % row)
    if refs.pixel("rip14", 10 * CELL, 12 * CELL + 4) != "F83800":
        raise SystemExit("1-4's lava pit at column 10 does not read red at row 12; the frame moved")


VERIFY = {"1-1": verify_11, "1-2": verify_12, "1-3": verify_13, "1-4": verify_14}


def quadrant(rows, qx, qy):
    return [row[qx * 8:qx * 8 + 8] for row in rows[qy * 8:qy * 8 + 8]]


def mirror(tile):
    return [list(reversed(row)) for row in tile]


def indices(tile, slot, where):
    """an 8x8 of RRGGBB as 2bpp values in its slot (a "set:slot" name picks the palette set, the
    bare slot the overworld's); an alien colour is fatal."""
    slots, slot = slot_set(slot)
    palette = slots[slot]
    lookup = {}
    for index, colour in enumerate(palette):
        lookup.setdefault(colour, index)
    out = []
    for y, row in enumerate(tile):
        line = []
        for x, colour in enumerate(row):
            colour = APPROXIMATIONS.get((slot, colour), colour)
            if colour not in lookup:
                raise SystemExit("%s pixel %d,%d is #%s, which is not in the %s palette (%s)"
                                 % (where, x, y, colour, slot, ",".join(palette)))
            line.append(lookup[colour])
        out.append(line)
    return out


BLANK = [[0] * 8 for _ in range(8)]


def build(refs, families):
    """every family as (stem, Name, slot, [8x8 tiles of 2bpp values]), and the stems of the
    families that could not be built because the sheet they are cut from is not there."""
    built = []
    skipped = []
    for stem, name, slot, sources in families:
        if not all(source is None or refs.has(source[0]) for source in sources):
            skipped.append(stem)
            continue
        tiles = []
        for source in sources:
            if source is None:
                tiles.append([row[:] for row in BLANK])
                continue
            anchor, qx, qy = source
            cell = refs.anchor(anchor, recolour=True)
            tiles.append(indices(quadrant(cell, qx, qy), slot, "%s/%s" % (stem, anchor)))
        built.append((stem, name, slot, tiles))
    return built, skipped


# ---------------------------------------------------------------- the structural report


def structural_report(refs, relations, quadrants):
    lines = []
    for label, left, relation, right in relations:
        a = refs.cell(left) if isinstance(left, tuple) else refs.anchor(left)
        b = refs.anchor(right)
        if relation == "mirror":
            b = mirror(b)
        differ = sum(1 for ra, rb in zip(a, b) for x, y in zip(ra, rb) if x != y)
        lines.append((label, differ))
    quads = []
    for entry in quadrants:
        label, left, right = entry[0], entry[1], entry[2]
        mode = entry[3] if len(entry) > 3 else "exact"
        names = [a for a, _, _ in left] + [a for a, _, _ in (right or [])]
        if not all(refs.has(a) for a in names):
            quads.append((label, None))
            continue
        # "recolour" lifts a black-backdrop cell into the overworld slot's colours first, so a
        # room cell and an overworld one compare pixel for pixel through the palette swap
        lift = mode == "recolour"
        lefts = [quadrant(refs.anchor(a, recolour=lift), qx, qy) for a, qx, qy in left]
        if right is None:
            # "drawn" cells count pixels that are not their own frame's backdrop
            backdrop = UNDERGROUND if dark_frame(ANCHORS[left[0][0]]) else SKY
            differ = sum(1 for t in lefts for row in t for p in row if p != backdrop)
        else:
            rights = [quadrant(refs.anchor(a, recolour=lift), qx, qy) for a, qx, qy in right]
            if mode == "vflip":
                rights = [list(reversed(t)) for t in rights]
            differ = sum(1 for ta, tb in zip(lefts, rights)
                         for ra, rb in zip(ta, tb) for x, y in zip(ra, rb) if x != y)
        quads.append((label, differ))
    return lines, quads


# ---------------------------------------------------------------- the position audit


def load_grid(generated, level):
    """the level's grid out of the compiled header, plus its first sub-area's own grid."""
    slug, array = LEVELS[level]["grid"]
    path = os.path.join(generated, slug + "_grid.h")
    if not os.path.exists(path):
        return None, None
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    match = re.search(array + r"\[(\d+)\]\[(\d+)\] = \{(.*?)\n\};", text, re.S)
    if not match:
        return None, None
    grid = [[int(v) for v in line.strip().strip(",").strip("{}").split(",")]
            for line in match.group(3).strip().splitlines()]

    area = None
    apath = os.path.join(generated, slug + "_areas.c")
    if LEVELS[level]["area_frame"] and os.path.exists(apath):
        with open(apath, "r", encoding="utf-8") as f:
            atext = f.read()
        amatch = re.search(slug + r"_area0_blocks\[[^\]]*\]\[[^\]]*\] = \{(.*?)\n\};",
                           atext, re.S)
        if amatch:
            area = []
            for line in amatch.group(1).strip().splitlines():
                body = line.split("//")[0].strip().strip(",").strip("{}")
                area.append([int(v) for v in body.split(",")])
    return grid, area


def load_enemies(generated, level):
    """the level's spawn list, so a ground cell a goomba stands on is not read as tile truth.
    the goomba's browns are the ground family's own, so nothing but the list can tell them
    apart. row is the surface row the enemy stands on top of, and the level's "enemy_rows" says
    how many cells above it a given kind's sprite reaches."""
    slug = LEVELS[level]["grid"][0]
    heights = LEVELS[level].get("enemy_rows", {})
    path = os.path.join(generated, slug + "_objects.c")
    if not os.path.exists(path):
        return set()
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()

    def array(name):
        match = re.search(slug + r"_enemy_%s\[[^\]]*\]\s*=\s*\{(.*?)\};" % name, text, re.S)
        if not match:
            return []
        return [int(v, 0) for v in match.group(1).replace("\n", "").split(",") if v.strip()]

    columns, rows, kinds = array("column"), array("row"), array("kind")
    covered = set()
    for index, (column, row) in enumerate(zip(columns, rows)):
        kind = kinds[index] if index < len(kinds) else 0
        tall = heights.get(kind, 1)
        # an enemy is up to two cells wide, walks between cells, and its feet overhang the cell it
        # stands on, so the three columns around its spawn are all suspect
        for dx in (-1, 0, 1):
            for dy in range(-tall, 1):
                covered.add((column + dx, row + dy))
    return covered


def load_overlays(level):
    """the level's declared sprite/caption cells as cell -> reason."""
    out = {}
    for reason, cells in LEVELS[level].get("overlays", []):
        for cell in cells:
            out.setdefault(cell, reason)
    return out


def collapse(cell, slot, palette):
    """the cell as 2bpp values under `palette`, so an overworld cell and an underground cell can
    be compared: a slot may name one colour twice, which is exactly how a cell down there can
    lose a highlight the overworld's has."""
    lookup = {}
    for index, colour in enumerate(palette):
        lookup.setdefault(colour, index)
    out = []
    for row in cell:
        line = []
        for colour in row:
            colour = APPROXIMATIONS.get((slot, colour), colour)
            if colour not in lookup:
                return None
            line.append(lookup[colour])
        out.append(line)
    return out


GEN_ARRAY = re.compile(r"const uint8_t k(\w+)Tiles\[(\d+)\]\s*=\s*\{(.*?)\};", re.S)


def gen_cell(gen_dir, name):
    """a whole block's 2bpp values out of its committed c, for a family whose anchor is on the
    missing sheet: its first four tiles are the cell's quadrants in Tl, Tr, Bl, Br order."""
    source = os.path.join(gen_dir, snake(name) + ".c")
    if not os.path.exists(source):
        return None
    with open(source, "r", encoding="utf-8") as f:
        match = GEN_ARRAY.search(f.read())
    if not match:
        return None
    data = bytes(int(t, 0) for t in match.group(3).replace("\n", "").split(",") if t.strip())
    if len(data) < 4 * 16:
        return None
    tiles = [gbpng.decode_tile(data[i * 16:(i + 1) * 16]) for i in range(4)]
    return [tiles[0][y] + tiles[1][y] for y in range(8)] + [tiles[2][y] + tiles[3][y] for y in range(8)]


# the captures only reach rip rows 0..11 (level rows 2..13); level row 14, the buried ground
# fill, is below their last scanline and no cell there can be audited
LAST_RIP_ROW = 11

# the families whose anchor is a sheet cell, and the generated c that stands in for it when the
# sheet is not there
SHEET_FAMILIES = {"question": "Question", "spent": "Spent", "coin": "Coin"}


def sources_for(level, grid, area, enemies, overlays):
    """every grid cell as (kind, capture spec, overlaid) in the level's own frames, with a None
    spec for a cell the captures do not hold."""
    frame = LEVELS[level]["frame"]
    area_frame = LEVELS[level]["area_frame"]
    row0, rows = level_rows(level)
    out = []
    if grid:
        for column, strip in enumerate(grid):
            for level_row, cell_kind in enumerate(strip):
                if level_row - row0 >= rows or level_row < row0:
                    out.append((cell_kind, None, False))
                    continue
                rip_col = level_column(level, column)
                spec = None if rip_col is None else (frame, rip_col, level_row - row0)
                covered = (column, level_row) in enemies or (column, level_row) in overlays
                out.append((cell_kind, spec, covered))
    if area and area_frame:
        for column, strip in enumerate(area):
            for level_row, cell_kind in enumerate(strip):
                out.append((cell_kind, (area_frame, column, level_row), False))
    return out


def wanted_cell(refs, level, kind, gen_dir):
    """what a kind's cell should look like under each palette set, as {set name: 2bpp values}, or
    None when neither the capture nor the committed c can say. an anchor cut from the castle is
    only ever wanted in the castle set; every other anchor is wanted above ground and, folded the
    way the underground slots fold, below it."""
    entry = LEVELS[level]["kinds"][kind]
    label, anchor_name, slot = entry[0], entry[1], entry[2]
    mirrored = len(entry) > 3 and entry[3]
    if slot is None:
        return None
    if refs.has(anchor_name) and palette_of(ANCHORS[anchor_name]) == "castle":
        want = refs.anchor(anchor_name)
        if mirrored:
            want = mirror(want)
        return {"castle": collapse(want, slot, CASTLE_SLOTS[slot])}
    if refs.has(anchor_name):
        # an anchor cut from a black-backdrop frame is stored in that frame's own colours, so it
        # has to be lifted into its slot before either comparison can read it
        want = refs.anchor(anchor_name, recolour=True)
        if mirrored:
            want = mirror(want)
        room_anchor = LEVELS[level].get("room_anchors", {}).get(kind)
        want_room = refs.anchor(room_anchor, recolour=True) if room_anchor else want
        want_over = collapse(want, slot, SLOTS[slot])
        want_room = collapse(want_room, slot, SLOTS[slot])
    else:
        want_over = want_room = gen_cell(gen_dir, SHEET_FAMILIES[anchor_name])
        if want_over is None:
            return None
    # an underground slot can name one colour twice, so the anchor's own 2bpp values are folded
    # the way the slot folds them before comparing
    under = UNDERGROUND_SLOTS[slot]
    fold = [min(j for j in range(4) if under[j] == under[i]) for i in range(4)]
    return {"overworld": want_over, "underground": [[fold[v] for v in row] for row in want_room]}


def audit(refs, gen_dir, level, grid, area, enemies, overlays):
    """every grid cell that names a kind the captures teach, against that kind's anchor.

    a cell lands in one of four buckets: `same`, `overlay` (the capture draws a sprite, a red
    coin, a challenge marker or a caption over the cell AND the cell no longer reads as this
    kind's art, so it is not tile truth), `differ` (the
    capture holds different art there - for 1-1 almost always the json standing in a different
    cell than smb does, see the level's "columns" table; for 1-2 the question blocks, whose faces
    the challenge rip covers with their contents in the slot's own colours) and `off frame`."""
    sources = sources_for(level, grid, area, enemies, overlays)
    kinds = LEVELS[level]["kinds"]
    rows = []
    for kind in sorted(kinds):
        label, slot = kinds[kind][0], kinds[kind][2]
        if slot is None:
            rows.append((kind, label, 0, 0, 0, 0, 0, []))
            continue
        wanted = wanted_cell(refs, level, kind, gen_dir)
        if wanted is None:
            rows.append((kind, label, 0, 0, 0, 0, 0, ["no sheet and no generated c"]))
            continue
        same = overlay = differ = off = 0
        misses = []
        for cell_kind, spec, covered in sources:
            if cell_kind != kind:
                continue
            got = refs.cell(spec) if spec is not None else None
            if got is None:
                off += 1
                continue
            set_name = palette_of(spec)
            palette = SLOT_SETS[set_name][slot]
            expect = wanted.get(set_name)
            values = collapse(got, slot, palette)
            if expect is None:
                # a castle anchor audited off a sky or underground cell, or the reverse: no
                # comparison is meaningful, so the cell is neither same nor differ
                off += 1
                continue
            # a cell whose colours the slot cannot even hold is a sprite for certain. otherwise a
            # covered cell EXCUSES a mismatch, it does not discard a match: if the capture's cell
            # still is this kind's art pixel for pixel, the sprite drawn over it did not touch the
            # part that matters and the cell is evidence like any other
            if values is None:
                overlay += 1
            elif values == expect:
                same += 1
            elif covered:
                overlay += 1
            else:
                differ += 1
                if len(misses) < 4:
                    misses.append(spec)
        rows.append((kind, label, same + overlay + differ + off, same, overlay, differ, off,
                     misses))
    return rows


# ---------------------------------------------------------------- the proof images
#
# a panorama is what the panorama tool (scratch, drives the title's debug camera and stitches
# framebuffer_color) writes: a P6 ppm of the whole level at one vertical scroll. the camera pans
# 144 px of a 240 px level, so one sweep cannot hold every row and the proof takes a list of
# (file, scy) - a level row is read from whichever sweep frames it.

CLASS_COLOURS = {
    "green": (32, 176, 64),  # the rom's cell is the capture's, pixel for pixel
    "yellow": (240, 208, 32),  # the capture drew a sprite or a caption here: not tile truth
    "blue": (48, 112, 240),  # the capture draws art the grid has none of
    "red": (216, 32, 32),  # a real mismatch
    "grey": (56, 56, 56),  # sky in both, or a row/column no reference holds
}
CLASS_ORDER = ["green", "yellow", "blue", "red", "grey"]


def quantise(rgb):
    """an 8-bit triple on the cgb's own 5-bit lattice, the way gbpng.rgb555 quantises."""
    return tuple((c >> 3) << 3 for c in rgb)


def read_ppm(path):
    """a P6 ppm as (width, height, rows of (r, g, b))."""
    with open(path, "rb") as f:
        data = f.read()
    fields, i = [], 0
    while len(fields) < 4:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b"#":
            while i < len(data) and data[i:i + 1] != b"\n":
                i += 1
            continue
        j = i
        while j < len(data) and not data[j:j + 1].isspace():
            j += 1
        fields.append(data[i:j])
        i = j
    if fields[0] != b"P6" or fields[3] != b"255":
        raise SystemExit("%s is not an 8-bit P6 ppm" % path)
    i += 1
    width, height = int(fields[1]), int(fields[2])
    rows = []
    for y in range(height):
        base = i + y * width * 3
        rows.append([(data[base + x * 3], data[base + x * 3 + 1], data[base + x * 3 + 2])
                     for x in range(width)])
    return width, height, rows


class Panorama:
    """the rom's own rendering of a level, addressed by level row.

    the panorama tool writes each rgb555 channel back out as (c5 << 3) | (c5 >> 2), which puts a
    full channel at 255 where the captures - which are real gbc grabs quantised the way gbpng's
    rgb555 helper does it - put it at 248. so every pixel read here is snapped back onto the
    captures' own 5-bit lattice (c5 << 3) before anything compares it, which is exactly the
    quantisation the hardware itself applies."""

    SCREEN_H = 144

    def __init__(self, sweeps):
        self.sweeps = []
        for path, scy in sweeps:
            width, height, rows = read_ppm(path)
            if height != self.SCREEN_H:
                raise SystemExit("%s is %d rows, not the screen's %d" % (path, height, self.SCREEN_H))
            self.sweeps.append((scy, width, rows))

    def width(self):
        return min(width for _scy, width, _rows in self.sweeps)

    def cell(self, column, level_row):
        """the 16x16 cell as rows of RRGGBB, from whichever sweep frames that level row."""
        top = level_row * CELL
        for scy, width, rows in self.sweeps:
            y0 = top - scy
            x0 = column * CELL
            if y0 < 0 or y0 + CELL > self.SCREEN_H or x0 + CELL > width:
                continue
            return [["%02X%02X%02X" % quantise(rows[y0 + y][x0 + x]) for x in range(CELL)]
                    for y in range(CELL)]
        return None


# the colours no slot can hold, which the generator resolves onto a colour the slot does have
# (see APPROXIMATIONS). the proof has to resolve the capture's cell the same way before comparing
# it, or a documented approximation reads as a mismatch - and it says which cells it did that to
APPROXIMATED = {colour: onto for (_slot, colour), onto in APPROXIMATIONS.items()}


def classify(refs, level, grid, enemies, overlays, pano):
    """one class per (column, level row) of the main grid: what the diff heatmap paints, plus the
    cells where a capture colour had to go through APPROXIMATIONS to be comparable at all."""
    frame = LEVELS[level]["frame"]
    row0, rows = level_rows(level)
    out = {}
    approximated = []
    for column, strip in enumerate(grid):
        for level_row, kind in enumerate(strip):
            rip_col = level_column(level, column)
            got = None
            if rip_col is not None and row0 <= level_row < row0 + rows:
                got = refs.cell((frame, rip_col, level_row - row0))
            if got is None:
                out[(column, level_row)] = ("grey", "no reference for this cell")
                continue
            backdrop = UNDERGROUND if dark_frame((frame, rip_col, level_row - row0)) else SKY
            covered = (column, level_row) in enemies or (column, level_row) in overlays
            reason = overlays.get((column, level_row), "a sprite the spawn list places here")
            if kind == 0:
                if {p for row in got for p in row} == {backdrop}:
                    out[(column, level_row)] = ("grey", "backdrop in both")
                elif covered:
                    out[(column, level_row)] = ("yellow", reason)
                else:
                    out[(column, level_row)] = ("blue", "the capture draws art the grid has none of")
                continue
            if covered:
                out[(column, level_row)] = ("yellow", reason)
                continue
            if any(p in APPROXIMATED for row in got for p in row):
                approximated.append((column, level_row,
                                     sorted({p for row in got for p in row if p in APPROXIMATED})))
                got = [[APPROXIMATED.get(p, p) for p in row] for row in got]
            mine = pano.cell(column, level_row) if pano else None
            if mine is None:
                out[(column, level_row)] = ("grey", "no panorama sweep frames this row")
            elif mine == got:
                out[(column, level_row)] = ("green", "identical")
            else:
                differ = sum(1 for ra, rb in zip(mine, got) for a, b in zip(ra, rb) if a != b)
                out[(column, level_row)] = ("red", "%d px differ from the capture" % differ)
    return out, approximated


def write_diff(classes, columns, out_path, scale=6):
    """the per-cell heatmap: one `scale`-px block per grid cell, gridded so a cell is countable."""
    rows_out = []
    height = max(row for _c, row in classes) + 1
    for row in range(height):
        line = []
        for column in range(columns):
            colour = CLASS_COLOURS[classes.get((column, row), ("grey", ""))[0]]
            line.extend([colour] * (scale - 1))
            line.append((16, 16, 16))
        for _ in range(scale - 1):
            rows_out.append(line[:])
        rows_out.append([(16, 16, 16)] * (columns * scale))
    gbpng.write_png(out_path, columns * scale, height * scale, rows_out, mode="RGB")
    return columns * scale, height * scale


def write_side_by_side(refs, level, pano, out_dir, chunks=4, scale=2):
    """the level in `chunks` slices, the capture's strip over the rom's, at `scale`x. only the
    rows both hold are drawn - the capture's 12 (level rows 2-13), which is also the band the
    diff compares; the hud row and level row 14 are in neither."""
    frame = LEVELS[level]["frame"]
    image = refs.images[LEVELS[level]["image"]]
    row0, rows = level_rows(level)
    top_row, bottom_row = row0, row0 + rows - 1
    band = (bottom_row - top_row + 1) * CELL
    columns = pano.width() // CELL
    per = (columns + chunks - 1) // chunks
    made = []
    for index in range(chunks):
        first = index * per
        last = min(columns, first + per)
        if first >= last:
            continue
        width = (last - first) * CELL
        rows_out = []
        for source in ("rip", "rom"):
            for y in range(band):
                line = []
                for x in range(width):
                    column = first + x // CELL
                    if source == "rip":
                        rip_col = level_column(level, column)
                        px = (255, 0, 255)
                        if rip_col is not None:
                            where = FRAMES[frame].origin(rip_col, 0)
                            sx, sy = where[0] + x % CELL, where[1] + y
                            if 0 <= sx < image.width and 0 <= sy < image.height:
                                r, g, b, _ = image.rgba(sx, sy)
                                px = (r, g, b)
                    else:
                        cell = pano.cell(column, top_row + y // CELL)
                        if cell is None:
                            px = (255, 0, 255)
                        else:
                            hexed = cell[y % CELL][x % CELL]
                            px = tuple(int(hexed[i:i + 2], 16) for i in (0, 2, 4))
                    line.extend([px] * scale)
                for _ in range(scale):
                    rows_out.append(line[:])
            if source == "rip":
                for _ in range(2 * scale):
                    rows_out.append([(255, 255, 255)] * (width * scale))
        target = os.path.join(out_dir, "side_by_side_%d.png" % index)
        gbpng.write_png(target, width * scale, len(rows_out), rows_out, mode="RGB")
        made.append((target, first, last - 1))
    return made


# ---------------------------------------------------------------- output


def png_rows(tiles, slot):
    out = []
    for tile in tiles:
        out.extend([row[:] for row in tile])
    slots, slot = slot_set(slot)
    palette = [tuple(int(slots[slot][i][j:j + 2], 16) for j in (0, 2, 4)) for i in range(4)]
    return out, palette


def write_pngs(built, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    made = []
    for stem, name, slot, tiles in built:
        rows, palette = png_rows(tiles, slot)
        target = os.path.join(out_dir, stem + ".png")
        gbpng.write_png(target, 8, len(rows), rows, mode="P", palette=palette)
        back = gbpng.read_png(target)
        if back.mode != "P" or back.rows != rows:
            raise SystemExit("%s did not round trip as an indexed png" % target)
        made.append((stem, name, len(tiles), target))
    return made


def snake(name):
    s = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s).lower()


def check_generated(built, gen_dir):
    bad = 0
    for stem, name, slot, tiles in built:
        source = os.path.join(gen_dir, snake(name) + ".c")
        if not os.path.exists(source):
            print("  %-26s not generated yet" % (snake(name) + ".c"))
            continue
        with open(source, "r", encoding="utf-8") as f:
            match = GEN_ARRAY.search(f.read())
        if not match or match.group(1) != name:
            print("  %s: no k%sTiles array found" % (source, name))
            bad += 1
            continue
        got = bytes(int(t, 0) for t in match.group(3).replace("\n", "").split(",") if t.strip())
        want = b"".join(gbpng.encode_tile(t) for t in tiles)
        if got != want:
            print("  %s: %d of %d bytes differ from the reference"
                  % (source, sum(1 for a, b in zip(got, want) if a != b), len(want)))
            bad += 1
        else:
            print("  %-26s %4d bytes match the reference" % (snake(name) + ".c", len(want)))
    return bad


def occurrences(refs, name):
    """how many cells of the anchor's own band are pixel for pixel this anchor. an anchor that is
    really a family's art turns up once per instance of that family in the level, which is
    evidence the json's own placement cannot give."""
    want = refs.anchor(name)
    frame = ANCHORS[name][0]
    img = refs.images[FRAMES[frame].image]
    rows = RIP14_ROWS if frame == "rip14" else LAST_RIP_ROW + 1
    count = 0
    for column in range(img.width // CELL):
        for row in range(rows):
            if refs.cell((frame, column, row)) == want:
                count += 1
    return count


def round_trip(built):
    bad = 0
    for stem, name, slot, tiles in built:
        for index, tile in enumerate(tiles):
            if gbpng.decode_tile(gbpng.encode_tile(tile)) != tile:
                print("  %s tile %d does not survive the 2bpp round trip" % (stem, index))
                bad += 1
    return bad


def report_level(refs, gen_dir, generated, level):
    print("structural relations for %s (0 = the capture agrees with what the rom assumes)" % level)
    flips, quads = structural_report(refs, LEVELS[level]["relations"], LEVELS[level]["quadrants"])
    for label, differ in flips + quads:
        verdict = "no sheet" if differ is None else ("yes" if differ == 0 else "no (%d px)" % differ)
        print("  %-72s %s" % (label, verdict))
    print("how often each %s anchor's exact cell occurs in its capture" % level)
    for name in sorted(LEVELS[level]["anchors"]):
        if not FRAMES[ANCHORS[name][0]].band:
            continue
        print("  %-22s %3d cells" % (name, occurrences(refs, name)))
    grid, area = load_grid(generated, level)
    if not grid:
        print("no compiled %s grid at %s - skipping its position audit" % (level, generated))
        return None, None
    enemies = load_enemies(generated, level)
    overlays = load_overlays(level)
    print("per-kind agreement of %s against the compiled grid" % level)
    print("  %-22s %5s %6s %8s %7s %6s" % ("kind", "cells", "same", "overlay", "differ", "off"))
    for kind, label, total, same, over, differ, off, misses in audit(
            refs, gen_dir, level, grid, area, enemies, overlays):
        note = "" if not misses else "  first %s" % (misses[0],)
        print("  %-22s %5d %6d %8d %7d %6d%s"
              % (label, total, same, over, differ, off, note))
    return grid, (enemies, overlays)


def main():
    here = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    parser = argparse.ArgumentParser(description="rip mario's terrain art out of the smbd captures")
    parser.add_argument("--level", default="all", choices=LEVEL_ORDER + ["all"],
                        help="which level's table to work with (default: all of them)")
    parser.add_argument("--root", default=here, help="repo root (default: this file's repo)")
    parser.add_argument("--generated", default=None,
                        help="the compiled levels (default: ROOT/build-mario/generated)")
    parser.add_argument("--check", action="store_true",
                        help="verify only: diff the generated c and audit the grids")
    parser.add_argument("--report", action="store_true", help="print the structural report")
    parser.add_argument("--proof", default=None,
                        help="write the proof images (diff.png, side_by_side_*.png) into this dir")
    parser.add_argument("--panorama", action="append", default=[], metavar="FILE:SCY",
                        help="a panorama ppm of the level and the scy it was swept at; repeatable")
    args = parser.parse_args()

    levels = LEVEL_ORDER if args.level == "all" else [args.level]
    ref_dir = os.path.join(args.root, "games", "mario", "art", "ref")
    out_dir = os.path.join(args.root, "games", "mario", "art", "tiles")
    gen_dir = os.path.join(args.root, "games", "mario", "src", "gen")
    generated = args.generated or os.path.join(args.root, "build-mario", "generated")
    if not os.path.isdir(ref_dir):
        raise SystemExit("no reference images at %s (they are gitignored; copy them in)" % ref_dir)

    # the relations reach across levels, so every capture a requested level's anchors name has to
    # be loaded - which, given 1-2's and 1-3's audits are "this is 1-1's art", is all of them
    refs = Refs(ref_dir, LEVEL_ORDER)
    families = [family for level in levels for family in LEVELS[level]["families"]]
    built, skipped = build(refs, families)
    if skipped:
        print("no tileset sheet: %s keep their committed c" % ", ".join(skipped))

    if args.proof:
        if len(levels) != 1:
            raise SystemExit("--proof needs one --level")
        level = levels[0]
        sweeps = []
        for entry in args.panorama:
            path, _, scy = entry.rpartition(":")
            if not path:
                raise SystemExit("--panorama wants FILE:SCY, got %r" % entry)
            sweeps.append((path, int(scy)))
        if not sweeps:
            raise SystemExit("--proof needs at least one --panorama FILE:SCY")
        grid, _area = load_grid(generated, level)
        if not grid:
            raise SystemExit("no compiled %s grid at %s" % (level, generated))
        pano = Panorama(sweeps)
        os.makedirs(args.proof, exist_ok=True)
        classes, approximated = classify(refs, level, grid, load_enemies(generated, level),
                                         load_overlays(level), pano)
        counts = {name: 0 for name in CLASS_ORDER}
        for name, _reason in classes.values():
            counts[name] += 1
        width, height = write_diff(classes, len(grid), os.path.join(args.proof, "diff.png"))
        print("diff.png %dx%d  %s" % (width, height,
                                      "  ".join("%s %d" % (n, counts[n]) for n in CLASS_ORDER)))
        for name in ("red", "blue"):
            for cell, (klass, reason) in sorted(classes.items()):
                if klass == name:
                    print("  %-6s column %3d row %2d  %s" % (name, cell[0], cell[1], reason))
        for cell, (klass, reason) in sorted(classes.items()):
            if klass == "yellow":
                print("  yellow column %3d row %2d  %s" % (cell[0], cell[1], reason))
        for column, row, colours in approximated:
            print("  approx column %3d row %2d  the capture paints #%s here, which the kind's "
                  "palette slot cannot hold: resolved to #%s (APPROXIMATIONS)"
                  % (column, row, ",#".join(colours),
                     ",#".join(APPROXIMATED[c] for c in colours)))
        for target, first, last in write_side_by_side(refs, level, pano, args.proof):
            print("%s  columns %d-%d" % (target, first, last))
        return 0

    if args.report or args.check:
        for level in levels:
            report_level(refs, gen_dir, generated, level)

    if args.check:
        print("check")
        bad = round_trip(built)
        bad += check_generated(built, gen_dir)
        print("%d mismatches" % bad)
        return 1 if bad else 0

    print("wrote")
    for stem, name, count, target in write_pngs(built, out_dir):
        print("  %-24s %2d tiles  k%sTiles[%d]" % (stem + ".png", count, name, count * 16))
    return 0


if __name__ == "__main__":
    sys.exit(main())
