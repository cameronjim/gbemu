#!/usr/bin/env python3
"""rips the block and terrain art of 1-1 and 1-2 out of the reference screenshots into the indexed
pngs the art pipeline (png2tiles.py --mode tiles) turns into banked c. run as:

    rip_tiles.py [--root REPO] [--generated DIR] [--check] [--report]

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
found by position (see ANCHORS) and every one of them is checked, by --check, against every cell
of the compiled level grids that says it holds that kind. the cell's colours are resolved against
its family's cgb palette slot, as rgb555 (each channel >> 3), which is what the hardware stores;
a colour that is not in the slot aborts the run rather than being snapped to something close.

the 1-1 grid and its capture do not agree on where everything stands - the capture is missing a
column (see COLUMN_SHIFT) - so --check reports per-kind agreement rather than demanding it. see
the AUDIT notes below.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gbpng

CELL = 16

# ---------------------------------------------------------------- the reference frames

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

# the sheet lays its global blocks out on a 17px pitch from x 4, all on one row at y 197
SHEET_BLOCK_Y = 197
SHEET_QUESTION_X = 4  # frame 1, the bright "?" - the frame an unhit block rests on
SHEET_SPENT_X = 55  # the used block, which no unhit level can show
SHEET_COIN_X = 155  # the world coin's first spin frame

SKY = "6888F8"

# the sheet was ripped from a different dump than the capture and renders the same four block
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

# ---------------------------------------------------------------- the palettes
#
# eight cgb bg palette slots, pinned per kind by kPaletteRom in assets.c. colour 0 is the
# backdrop every partly-transparent cell shows through, and colour 1 of the sky slot is also the
# hud row's ink (see kHudBarAttr), which is why it has to stay white. every colour is read
# straight off the capture.
SLOTS = {
    # clouds and the flag's pennant. 30A0F8 is the scallop shading under a cloud
    "sky": [SKY, "F8F8F8", "30A0F8", "000000"],
    # the ground block is opaque across its whole cell, so its colour 0 never reaches a pixel
    "ground": [SKY, "F8C098", "984800", "000000"],
    # the brick, the hard block and the whole castle are the same three browns
    "brick": [SKY, "F8C098", "984800", "000000"],
    # the question block's gold, which the used block shares - see kCamPalSpent below
    "question": [SKY, "F8B840", "984800", "000000"],
    # pipes, hills, bushes and the flagpole
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
UNDERGROUND = "000000"
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

# the one colour in 1-1 that no slot can hold. the pennant's emblem is drawn in the koopa green
# 008010, and the cell it sits in is pinned to the sky slot, whose four colours are the backdrop,
# the white the hud row also needs, the clouds' scallop blue and black. re-slotting the pennant
# onto the pipe slot - the only one that carries a green - would cost it its white, so the emblem
# takes the slot's black instead and reads as a dark disc rather than a dark green one
APPROXIMATIONS = {("sky", "008010"): "000000"}

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

# ---------------------------------------------------------------- the anchors
#
# one clean, sprite-free instance of each family, by cell. "rip" cells are (column, RIP row) in
# 1-1's overworld frame; "bonus" cells are (column, LEVEL row) in its room's frame; "rip12" cells
# are (column, RIP row) in 1-2's upper band and "room12" cells (column, LEVEL row) in its coin
# room; "sheet" cells are (x, y) pixel origins on the tileset sheet.
ANCHORS = {
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
    # the 1-2 capture. the underground run's own cells, read by the report and the 1-2 audit, and
    # every one of them 1-1's art under the underground colours: the floor is the ground block, the
    # roof and the walls the room's brick, the stair-steps the hard block, the pipes the pipe
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


def dark_frame(spec):
    """whether a cell comes off a black backdrop: either coin room, or 1-2's underground run (its
    ending, from RIP12_ENDING_COLUMN on, is open sky like 1-1)."""
    return spec[0] in ("bonus", "room12") or (spec[0] == "rip12" and spec[1] < RIP12_ENDING_COLUMN)


def Q(anchor, qx, qy):
    """one 8x8 quadrant of an anchor cell: qx/qy are 0 for the left/top half."""
    return (anchor, qx, qy)


# ---------------------------------------------------------------- the generated families
#
# (png stem, png2tiles --name, palette slot, [quadrant per tile]). each png is one column of 8x8
# tiles so that png2tiles' reading order IS the order set_bkg_data wants, and each list below is
# written in the rom's own tile-id order - read the loader in assets_data.c beside it.
FAMILIES = [
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
    # bank 1 0xe0-0xe4: the shaft joint, where that body runs into its vertical shaft. the shaft's
    # left cell there wears the body's rim and joint down its own left column and the plain
    # body's right column past it, so each of the two joint kinds needs two tiles of its own; the
    # fifth is a bank-1 copy of kTilePipeBodyM for the right column, because a kind's attribute
    # byte picks one vram bank for all four quadrants and the body's own tile lives in bank 0
    ("pipe_joint", "PipeJoint", "pipe", [
        Q("pipe_joint_t", 0, 0), Q("pipe_joint_t", 0, 1),
        Q("pipe_joint_b", 0, 0), Q("pipe_joint_b", 0, 1),
        Q("pipe_body_l", 1, 0)]),
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

# the kinds whose art the captures teach: kind -> (label, anchor, palette slot). kind numbers
# are kBlock* from mario.h and must not move; the slot is the one kPaletteRom pins the kind to
KIND_CELLS = {
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
    33: ("hill slope right", "hill_slope_r", "pipe"),
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

# AUDIT: the compiled 1-1 grid and its capture do not stand in the same columns everywhere, and
# the defect is the CAPTURE's. it is a stitch of screen-sized grabs, and it lost one column at its
# left edge and gained a duplicate one around its fifth screen: the level's own column 0 is not in
# the image at all. so a level column reads one column LEFT of itself for the first third of the
# level and level-for-level after the seam. the nes map and the 1-2 pass both agree with the
# json's geometry, so the json is right and this table is the correction:
COLUMN_SHIFT = ((0, 75, -1), (76, 207, 0))
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
#
# the 1-2 capture stands level for level: the underground run's column c is level c + 24 (the
# 24-column start segment is in no smbd rip) and the ending's is c + 25, because level 216 - the
# right column of the coin room's exit shaft, which the bible carries up to row 0 to seal the seam
# - is not in the image and capture column 192 is already the ending's first column of sky. its
# one defect is at capture column 63, level 87, where the hanging wall's right column is drawn
# eight pixels wide over black on rows 1-7: a stitch that lost half a cell, since nothing in the
# game is drawn on an 8px grid and both the nes map and the rest of the capture keep the wall two
# columns wide
COLUMN_SHIFT12 = ((24, 215, -24), (217, 248, -25))


def rip_column(level_column):
    """the 1-1 capture column a level column stands in. level column 0 comes back as -1: the
    capture does not hold it, and refs.cell() answers None for it."""
    for lo, hi, shift in COLUMN_SHIFT:
        if lo <= level_column <= hi:
            return level_column + shift
    return level_column


def rip12_column(level_column):
    """the 1-2 capture column a level column stands in, or None for the start segment and the
    exit shaft's right column, which it does not hold."""
    for lo, hi, shift in COLUMN_SHIFT12:
        if lo <= level_column <= hi:
            return level_column + shift
    return None


# ---------------------------------------------------------------- reading the references


class Refs:
    def __init__(self, ref_dir):
        self.rip = gbpng.read_png(os.path.join(ref_dir, "smbd_ch_1-1.png"))
        self.rip12 = gbpng.read_png(os.path.join(ref_dir, "smbd_ch_1-2.png"))
        sheet = os.path.join(ref_dir, "sheet_tileset.png")
        self.sheet = gbpng.read_png(sheet) if os.path.exists(sheet) else None
        self.verify_frame()
        self.verify_frame12()

    def verify_frame(self):
        """the y offset is only trustworthy if the ground's top edge really is at y 178, so the
        cell above rip row 11 is open sky and rip row 11 itself has no sky left in it. column 5
        is chosen because nothing stands on it and no sprite walks over it."""
        above = {self._hex(self.rip, 5 * CELL + x, RIP_Y0 + 10 * CELL + y)
                 for x in range(CELL) for y in range(CELL)}
        if above != {SKY}:
            raise SystemExit("the cell above the ground at column 5 is not sky; the y offset moved")
        row = {self._hex(self.rip, 5 * CELL + x, RIP_Y0 + 11 * CELL + y)
               for x in range(CELL) for y in range(CELL)}
        if SKY in row or self._hex(self.rip, 5 * CELL, RIP_Y0 + 11 * CELL - 1) != SKY:
            raise SystemExit("y=178 is not the ground's top edge; the capture's y offset moved")

    def verify_frame12(self):
        """the same check on the 1-2 capture: the floor's top edge at y 184 under a cell of black
        in column 5 (inside the entry shaft, where nothing stands), the ending's first column of
        open sky at 192, and the coin room's roof brick at the top left of the lower band."""
        above = {self._hex(self.rip12, 5 * CELL + x, RIP12_Y0 + 10 * CELL + y)
                 for x in range(CELL) for y in range(CELL)}
        if above != {UNDERGROUND}:
            raise SystemExit("the cell above 1-2's floor at column 5 is not black; the y offset moved")
        edge = RIP12_Y0 + 11 * CELL
        if (self._hex(self.rip12, 5 * CELL, edge - 1) != UNDERGROUND
                or self._hex(self.rip12, 5 * CELL, edge) != "008888"
                or self._hex(self.rip12, 5 * CELL + 1, edge) != "B8F8F0"):
            raise SystemExit("y=184 is not 1-2's floor top edge; the capture's y offset moved")
        ending = {self._hex(self.rip12, RIP12_ENDING_COLUMN * CELL + x, RIP12_Y0 + y)
                  for x in range(CELL) for y in range(CELL)}
        if ending != {SKY}:
            raise SystemExit("1-2's column 192 is not the ending's sky; the capture's stitch moved")
        roof = self.cell(("room12", 3, ROOM12_TOP_ROW))
        if roof is None or {p for row in roof for p in row} != {UNDERGROUND, "008888"}:
            raise SystemExit("1-2's coin room roof is not at x 1680, y 201; the band moved")

    @staticmethod
    def _hex(img, x, y):
        r, g, b, _ = img.rgba(x, y)
        return "%02X%02X%02X" % (r, g, b)

    def cell(self, spec):
        """a 16x16 cell as rows of RRGGBB, in whichever frame the anchor names."""
        frame = spec[0]
        if frame == "rip":
            img, x0, y0 = self.rip, spec[1] * CELL, RIP_Y0 + spec[2] * CELL
        elif frame == "bonus":
            img = self.rip
            x0 = BONUS_X0 + spec[1] * CELL
            y0 = BONUS_GROUND_Y - CELL * (BONUS_GROUND_ROW - spec[2])
        elif frame == "rip12":
            img, x0, y0 = self.rip12, spec[1] * CELL, RIP12_Y0 + spec[2] * CELL
        elif frame == "room12":
            # the band is sixteen columns wide and starts at level row 2: the room's own column
            # 16, its shaft's right side, is past its edge, and its wall's rows 0-1 are above it,
            # where the image holds the level's ending and the run's floor
            if spec[1] >= ROOM12_COLUMNS or spec[2] < ROOM12_TOP_ROW:
                return None
            img = self.rip12
            x0 = ROOM12_X0 + spec[1] * CELL
            y0 = ROOM12_Y0 + CELL * (spec[2] - ROOM12_TOP_ROW)
        elif frame == "sheet":
            if self.sheet is None:
                return None
            img, x0, y0 = self.sheet, spec[1], spec[2]
        else:
            raise SystemExit("unknown frame %r" % (frame,))
        if x0 < 0 or y0 < 0 or x0 + CELL > img.width or y0 + CELL > img.height:
            return None
        rows = [[self._hex(img, x0 + x, y0 + y) for x in range(CELL)] for y in range(CELL)]
        if frame == "sheet":
            rows = [[SHEET_TO_RIP.get(p, p) for p in row] for row in rows]
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
        return ANCHORS[name][0] != "sheet" or self.sheet is not None


def quadrant(rows, qx, qy):
    return [row[qx * 8:qx * 8 + 8] for row in rows[qy * 8:qy * 8 + 8]]


def mirror(tile):
    return [list(reversed(row)) for row in tile]


def indices(tile, slot, where):
    """an 8x8 of RRGGBB as 2bpp values in its slot; an alien colour is fatal."""
    palette = SLOTS[slot]
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


def build(refs):
    """every family as (stem, Name, slot, [8x8 tiles of 2bpp values]), and the stems of the
    families that could not be built because the sheet they are cut from is not there."""
    built = []
    skipped = []
    for stem, name, slot, sources in FAMILIES:
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

# relations the rom leans on: a quadrant two kinds share, or a kind the rom draws as its twin
# mirrored (kCamAttrXFlip). each is checked against the capture and reported either way
SHARING = [
    ("hill slope right == mirror(hill slope left)", "hill_slope_r", "mirror", "hill_slope"),
    ("cloud cap right top == mirror(cap left top)", "cloud_capr_t", "mirror", "cloud_cap_t"),
    ("cloud cap right bottom == mirror(cap left bottom)", "cloud_capr_b", "mirror", "cloud_cap_b"),
    ("bush cap right == mirror(bush cap left)", "bush_capr", "mirror", "bush_cap"),
    ("pipe lip right == mirror(pipe lip left)", "pipe_lip_r", "mirror", "pipe_lip_l"),
    ("pipe body right == mirror(pipe body left)", "pipe_body_r", "mirror", "pipe_body_l"),
    ("castle window right == mirror(window left)", "castle_window_right", "mirror",
     "castle_window"),
]


def whole(anchor):
    return [(anchor, 0, 0), (anchor, 1, 0), (anchor, 0, 1), (anchor, 1, 1)]


QUADRANT_SHARING = [
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


def structural_report(refs):
    lines = []
    for label, left, relation, right in SHARING:
        a = refs.cell(left) if isinstance(left, tuple) else refs.anchor(left)
        b = refs.anchor(right)
        if relation == "mirror":
            b = mirror(b)
        differ = sum(1 for ra, rb in zip(a, b) for x, y in zip(ra, rb) if x != y)
        lines.append((label, differ))
    quads = []
    for entry in QUADRANT_SHARING:
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
            differ = sum(1 for t in lefts for row in t for p in row if p != SKY)
        else:
            rights = [quadrant(refs.anchor(a, recolour=lift), qx, qy) for a, qx, qy in right]
            differ = sum(1 for ta, tb in zip(lefts, rights)
                         for ra, rb in zip(ta, tb) for x, y in zip(ra, rb) if x != y)
        quads.append((label, differ))
    return lines, quads


# ---------------------------------------------------------------- the position audit

# the compiled grids: level -> (its slug in the generated c, its main grid's array name)
LEVEL_GRIDS = {"1-1": ("level_1_1", "kLevel11Grid"), "1-2": ("level_1_2", "kLevel12Grid")}


def load_grid(generated, level):
    """the level's grid out of the compiled header, plus its first sub-area's own grid."""
    slug, array = LEVEL_GRIDS[level]
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
    if os.path.exists(apath):
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
    apart. row is the surface row the enemy stands on top of."""
    slug = LEVEL_GRIDS[level][0]
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

    columns, rows = array("column"), array("row")
    covered = set()
    for column, row in zip(columns, rows):
        # an enemy is up to two cells wide, walks between cells, and its feet overhang the cell it
        # stands on, so the three columns around its spawn are all suspect
        for dx in (-1, 0, 1):
            for dy in (-1, 0):
                covered.add((column + dx, row + dy))
    return covered


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


def sources_for(level, grid, area, enemies):
    """every grid cell as (capture spec, overlaid) in the level's own frames, or None for a cell
    the captures do not hold."""
    out = []
    if grid:
        for column, strip in enumerate(grid):
            for level_row, cell_kind in enumerate(strip):
                if level_row - 2 > LAST_RIP_ROW or level_row < 2:
                    out.append((cell_kind, None, False))
                    continue
                if level == "1-1":
                    spec = ("rip", rip_column(column), level_row - 2)
                else:
                    rip_col = rip12_column(column)
                    spec = None if rip_col is None else ("rip12", rip_col, level_row - 2)
                out.append((cell_kind, spec, (column, level_row) in enemies))
    if area:
        frame = "bonus" if level == "1-1" else "room12"
        for column, strip in enumerate(area):
            for level_row, cell_kind in enumerate(strip):
                out.append((cell_kind, (frame, column, level_row), False))
    return out


def audit(refs, gen_dir, level, grid, area, enemies):
    """every grid cell that names a kind the captures teach, against that kind's anchor.

    a cell lands in one of four buckets: `same`, `overlay` (the capture draws a sprite, a red
    coin, a challenge marker or a caption over it, so it is not tile truth), `differ` (the
    capture holds different art there - for 1-1 almost always the json standing in a different
    cell than smb does, see COLUMN_SHIFT; for 1-2 the question blocks, whose faces the challenge
    rip covers with their contents in the slot's own colours) and `off frame`."""
    sources = sources_for(level, grid, area, enemies)
    rows = []
    for kind in sorted(KIND_CELLS):
        label, anchor_name, slot = KIND_CELLS[kind]
        if slot is None:
            rows.append((kind, label, 0, 0, 0, 0, 0, []))
            continue
        want = want_room = None
        if refs.has(anchor_name):
            # an anchor cut from a black-backdrop frame is stored in that frame's own colours, so
            # it has to be lifted into its slot before either comparison can read it
            want = refs.anchor(anchor_name, recolour=True)
            if kind == 33:  # the right hill slope is the left one mirrored, as the rom draws it
                want = mirror(refs.anchor("hill_slope"))
            # the room's brick is the one family whose ART differs down there, not just its
            # colours: rip_tiles generates the room's own upper pair and the rom loads it over the
            # overworld's, so the audit has to compare a dark cell against the room's own anchor
            want_room = refs.anchor("ug_brick", recolour=True) if kind == 2 else want
            want_over = collapse(want, slot, SLOTS[slot])
            want_room = collapse(want_room, slot, SLOTS[slot])
        else:
            want_over = want_room = gen_cell(gen_dir, SHEET_FAMILIES[anchor_name])
            if want_over is None:
                rows.append((kind, label, 0, 0, 0, 0, 0, ["no sheet and no generated c"]))
                continue
        # an underground slot can name one colour twice, so the anchor's own 2bpp values are
        # folded the way the slot folds them before comparing
        under = UNDERGROUND_SLOTS[slot]
        fold = [min(j for j in range(4) if under[j] == under[i]) for i in range(4)]
        want_under = [[fold[v] for v in row] for row in want_room]
        same = overlay = differ = off = 0
        misses = []
        for cell_kind, spec, covered in sources:
            if cell_kind != kind:
                continue
            got = refs.cell(spec) if spec is not None else None
            if got is None:
                off += 1
                continue
            dark = dark_frame(spec)
            palette = UNDERGROUND_SLOTS[slot] if dark else SLOTS[slot]
            expect = want_under if dark else want_over
            values = collapse(got, slot, palette)
            if values is None or covered:
                overlay += 1
            elif values == expect:
                same += 1
            else:
                differ += 1
                if len(misses) < 4:
                    misses.append(spec)
        rows.append((kind, label, same + overlay + differ + off, same, overlay, differ, off,
                     misses))
    return rows


# ---------------------------------------------------------------- output


def png_rows(tiles, slot):
    out = []
    for tile in tiles:
        out.extend([row[:] for row in tile])
    palette = [tuple(int(SLOTS[slot][i][j:j + 2], 16) for j in (0, 2, 4)) for i in range(4)]
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
    """how many cells of the anchor's own upper band are pixel for pixel this anchor. an anchor
    that is really a family's art turns up once per instance of that family in the level, which
    is evidence the json's own placement cannot give."""
    want = refs.anchor(name)
    frame = ANCHORS[name][0]
    img = refs.rip if frame == "rip" else refs.rip12
    count = 0
    for column in range(img.width // CELL):
        for row in range(LAST_RIP_ROW + 1):
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


def main():
    here = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    parser = argparse.ArgumentParser(description="rip mario's 1-1 and 1-2 terrain art")
    parser.add_argument("--root", default=here, help="repo root (default: this file's repo)")
    parser.add_argument("--generated", default=None,
                        help="the compiled levels (default: ROOT/build-mario/generated)")
    parser.add_argument("--check", action="store_true",
                        help="verify only: diff the generated c and audit the grids")
    parser.add_argument("--report", action="store_true", help="print the structural report")
    args = parser.parse_args()

    ref_dir = os.path.join(args.root, "games", "mario", "art", "ref")
    out_dir = os.path.join(args.root, "games", "mario", "art", "tiles")
    gen_dir = os.path.join(args.root, "games", "mario", "src", "gen")
    generated = args.generated or os.path.join(args.root, "build-mario", "generated")
    if not os.path.isdir(ref_dir):
        raise SystemExit("no reference images at %s (they are gitignored; copy them in)" % ref_dir)

    refs = Refs(ref_dir)
    built, skipped = build(refs)
    if skipped:
        print("no tileset sheet: %s keep their committed c" % ", ".join(skipped))

    if args.report or args.check:
        print("structural relations (0 = the capture agrees with what the rom assumes)")
        flips, quads = structural_report(refs)
        for label, differ in flips + quads:
            verdict = "no sheet" if differ is None else ("yes" if differ == 0 else "no (%d px)" % differ)
            print("  %-64s %s" % (label, verdict))
        print("how often each anchor's exact cell occurs in its capture")
        for name in sorted(ANCHORS):
            if ANCHORS[name][0] not in ("rip", "rip12"):
                continue
            print("  %-22s %3d cells" % (name, occurrences(refs, name)))
        for level in sorted(LEVEL_GRIDS):
            grid, area = load_grid(generated, level)
            if not grid:
                print("no compiled %s grid at %s - skipping its position audit" % (level, generated))
                continue
            enemies = load_enemies(generated, level)
            print("per-kind agreement of %s against the compiled grid" % level)
            print("  %-22s %5s %6s %8s %7s %6s"
                  % ("kind", "cells", "same", "overlay", "differ", "off"))
            for kind, label, total, same, over, differ, off, misses in audit(
                    refs, gen_dir, level, grid, area, enemies):
                note = "" if not misses else "  first %s" % (misses[0],)
                print("  %-22s %5d %6d %8d %7d %6d%s"
                      % (label, total, same, over, differ, off, note))

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
