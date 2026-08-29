#!/usr/bin/env python3
"""compiles a games/mario/data/*.json level bible into banked c terrain data plus a host-test
probe header. run as: compile_level.py <bible.json> <out_dir> [--bank N] [--area-bank M]

the block-kind numbers below are a contract with games/mario/src/mario.h's kBlock* defines and
must be kept numerically identical by hand; nothing here reads mario.h.
"""

import json
import os
import re
import sys

BLOCK_EMPTY = 0
BLOCK_GROUND = 1
BLOCK_BRICK = 2
BLOCK_QUESTION = 3
BLOCK_HARD = 4
BLOCK_PIPE_TL = 5
BLOCK_PIPE_TR = 6
BLOCK_PIPE_BODY_L = 7
BLOCK_PIPE_BODY_R = 8
BLOCK_STAIR = 9
BLOCK_FLAG_POLE = 10
BLOCK_CASTLE = 11
BLOCK_SPENT = 12
BLOCK_COIN = 13
# sub-milestone 8a's four: a thin platform is solid to feet coming down onto it and to nothing
# else, lava is scenery over the death plane, and the bridge/axe are 1-4's ending
BLOCK_THIN = 14
BLOCK_LAVA = 15
BLOCK_BRIDGE = 16
BLOCK_AXE = 17
# milestone 18's background pass. a surface ground block wears two rows of grass, so the rows
# under it are their own kind; the castle became five kinds instead of one slab; the flag grew a
# ball and a pennant; and the last twelve are scenery, stamped only where the level left sky
BLOCK_GROUND_FILL = 18
BLOCK_CASTLE_CRENEL = 19
BLOCK_CASTLE_WINDOW = 20
BLOCK_CASTLE_DOOR_TOP = 21
BLOCK_CASTLE_DOOR = 22
BLOCK_FLAG_BALL = 23
BLOCK_FLAG_CLOTH = 24
BLOCK_CLOUD_TL = 25
BLOCK_CLOUD_T = 26
BLOCK_CLOUD_TR = 27
BLOCK_CLOUD_BL = 28
BLOCK_CLOUD_B = 29
BLOCK_CLOUD_BR = 30
BLOCK_HILL_PEAK = 31
BLOCK_HILL_SLOPE_L = 32
BLOCK_HILL_SLOPE_R = 33
BLOCK_HILL_FILL = 34
BLOCK_BUSH_L = 35
BLOCK_BUSH_M = 36
BLOCK_BUSH_R = 37

KIND_NAMES = {
    BLOCK_EMPTY: "EMPTY",
    BLOCK_GROUND: "GROUND",
    BLOCK_BRICK: "BRICK",
    BLOCK_QUESTION: "QUESTION",
    BLOCK_HARD: "HARD",
    BLOCK_PIPE_TL: "PIPE_TL",
    BLOCK_PIPE_TR: "PIPE_TR",
    BLOCK_PIPE_BODY_L: "PIPE_BODY_L",
    BLOCK_PIPE_BODY_R: "PIPE_BODY_R",
    BLOCK_STAIR: "STAIR",
    BLOCK_FLAG_POLE: "FLAG_POLE",
    BLOCK_CASTLE: "CASTLE",
    BLOCK_SPENT: "SPENT",
    BLOCK_COIN: "COIN",
    BLOCK_THIN: "THIN",
    BLOCK_LAVA: "LAVA",
    BLOCK_BRIDGE: "BRIDGE",
    BLOCK_AXE: "AXE",
    BLOCK_GROUND_FILL: "GROUND_FILL",
    BLOCK_CASTLE_CRENEL: "CASTLE_CRENEL",
    BLOCK_CASTLE_WINDOW: "CASTLE_WINDOW",
    BLOCK_CASTLE_DOOR_TOP: "CASTLE_DOOR_TOP",
    BLOCK_CASTLE_DOOR: "CASTLE_DOOR",
    BLOCK_FLAG_BALL: "FLAG_BALL",
    BLOCK_FLAG_CLOTH: "FLAG_CLOTH",
    BLOCK_CLOUD_TL: "CLOUD_TL",
    BLOCK_CLOUD_T: "CLOUD_T",
    BLOCK_CLOUD_TR: "CLOUD_TR",
    BLOCK_CLOUD_BL: "CLOUD_BL",
    BLOCK_CLOUD_B: "CLOUD_B",
    BLOCK_CLOUD_BR: "CLOUD_BR",
    BLOCK_HILL_PEAK: "HILL_PEAK",
    BLOCK_HILL_SLOPE_L: "HILL_SLOPE_L",
    BLOCK_HILL_SLOPE_R: "HILL_SLOPE_R",
    BLOCK_HILL_FILL: "HILL_FILL",
    BLOCK_BUSH_L: "BUSH_L",
    BLOCK_BUSH_M: "BUSH_M",
    BLOCK_BUSH_R: "BUSH_R",
}

# every kind a body walks straight through, which is what surface_row has to skip past and what
# the decor stamper is allowed to overwrite nothing of
WALK_THROUGH = frozenset(
    [BLOCK_EMPTY, BLOCK_FLAG_POLE, BLOCK_FLAG_BALL, BLOCK_FLAG_CLOTH, BLOCK_COIN, BLOCK_AXE,
     BLOCK_CASTLE, BLOCK_CASTLE_CRENEL, BLOCK_CASTLE_WINDOW, BLOCK_CASTLE_DOOR_TOP,
     BLOCK_CASTLE_DOOR]
    + list(range(BLOCK_CLOUD_TL, BLOCK_BUSH_R + 1))
)

# the reaction list's own kind numbering, a contract with games/mario/src/mario.h's kBlockList*.
# it is separate from the render kinds above because a hidden block renders as sky until it is bumped
LIST_QUESTION = 0
LIST_BRICK = 1
LIST_HIDDEN = 2

LIST_KIND_MAP = {"question": LIST_QUESTION, "brick": LIST_BRICK, "hidden": LIST_HIDDEN}

# blocks.contents -> the kContent* contract in games/mario/src/mario.h
CONTENT_MAP = {
    "nothing": 0,
    "coin": 1,
    "mushroom_fire": 2,
    "star": 3,
    "oneup": 4,
    "multicoin": 5,
    "vine": 6,
}

# enemies.kind -> the kEnemy* contract in games/mario/src/mario.h. a kind with no number here is
# left out of the spawn list entirely rather than guessed at
ENEMY_KIND_MAP = {
    "goomba": 0,
    "koopa_green": 1,
    "koopa_red": 2,
    # roster.json: a paratroopa stomps down into a plain koopa. the wings are m9's, so the bible's
    # red paratroopas compile to red koopas at the same cell and fall to whatever is under them
    "koopa_para_red": 2,
    "piranha": 3,
}

# the object list's kinds, the contract with games/mario/src/mario.h's kObj* defines
OBJ_PIPE = 0
OBJ_LIFT_H = 1
OBJ_LIFT_V = 2
OBJ_FIREBAR = 3
OBJ_BOWSER = 4
OBJ_AXE = 5

OBJ_NAMES = {
    OBJ_PIPE: "PIPE",
    OBJ_LIFT_H: "LIFT_H",
    OBJ_LIFT_V: "LIFT_V",
    OBJ_FIREBAR: "FIREBAR",
    OBJ_BOWSER: "BOWSER",
    OBJ_AXE: "AXE",
}

# level types, the contract with games/mario/src/mario.h's kLevelType*
TYPE_OVERWORLD = 0
TYPE_UNDERGROUND = 1
TYPE_CASTLE = 2
TYPE_MAP = {"overworld": TYPE_OVERWORLD, "underground": TYPE_UNDERGROUND, "castle": TYPE_CASTLE}

# area kinds, the contract with games/mario/src/mario.h's kArea* defines
AREA_BONUS = 0
AREA_WARP = 1
AREA_KIND_MAP = {"bonus_room": AREA_BONUS, "underground": AREA_BONUS, "warp_zone": AREA_WARP}

# schema.md: 15 block rows, ground surface at row 13, rows 13-14 are the solid ground blocks
LEVEL_ROWS = 15
GROUND_ROW = 13
# each column is stored 16 bytes wide, one padding byte past the 15 real rows: sdcc turns a
# power-of-two inner dimension into a shift, and the engine probes a cell six times a frame
ROW_STRIDE = 16

# blocks.kind -> our render kind; "hidden" has no art of its own until something reveals it, so it
# renders as empty (per SCHEMA.md its contents still exist)
BLOCK_KIND_MAP = {
    "question": BLOCK_QUESTION,
    "brick": BLOCK_BRICK,
    "hard": BLOCK_HARD,
    "hidden": BLOCK_EMPTY,
}

# columns of clearance kept past the last positioned feature; length_columns is provisional until
# a rom-measure pass replaces the bible's approx/unknown positions (see SCHEMA.md)
PAD_COLUMNS = 8

FLAG_POLE_TOP_ROW = 4
FLAG_POLE_BOTTOM_ROW = GROUND_ROW - 1

# the castle that closes an overworld level: five blocks wide, five tall, standing this far past
# the pole. smb puts it a short walk beyond the flag with clear sky between the two
CASTLE_WIDTH = 5
CASTLE_HEIGHT = 5
CASTLE_FLAG_GAP = 3

# scenery geometry. a hill or a bush stands on the row the ground's grass is about to grow out of;
# a cloud carries its own row. smb's narrowest bush is its two caps back to back
DECOR_BASE_ROW = GROUND_ROW - 1
DECOR_BUSH_MIN_WIDTH = 2
DECOR_CLOUD_MIN_WIDTH = 2

# an underground level gets a two-row roof the bible never describes: SCHEMA.md's grid has rows
# above the playfield and mariowiki calls 1-2 an enclosed cave, but no source places the ceiling.
# compiler-invented, must-verify
CEILING_ROWS = 2

# a firebar's pivot is a hard block; the roster calls the short bar "about 6 fireball segments"
FIREBAR_SEGMENTS = 6

# lift geometry. roster.json gives the lift no size and physics.json's per-type speeds are
# must-measure, so both the 2-block deck and the travel spans below are ours
LIFT_BLOCKS = 2
LIFT_MIN_SPAN = 4
LIFT_SPAN_MASK = 0x3F
# the second lift of a pair starts at the far end of the same track and runs the other way, so one
# of them is always somewhere a rider can reach
LIFT_REVERSE = 0x80
# a horizontal lift's deck rides one row above the surface it bridges, which is a hop up onto it
HORIZONTAL_LIFT_ROW = GROUND_ROW - 1
# a vertical pair runs between these rows; the bible only says "left descends, right ascends"
VERTICAL_LIFT_TOP_ROW = 3
VERTICAL_LIFT_SPAN = 8

# a castle lava pit wider than this many columns is spanned by a horizontal lift rather than left
# as a jump: the bible's castle gap extents are "sourced" only as prose, so a pit it happens to
# place beyond mario's running jump would make the level impossible. compiler rule, must-verify
CASTLE_LIFT_GAP = 5

# the bridge that ends a castle: the last sourced ground run becomes bridge over lava, and the two
# columns past it carry the axe on solid ground
BRIDGE_TAIL_COLUMNS = 2

# sub-area layout. the bible gives 1-1's bonus room no terrain at all and places none of its coins:
# only the total ("19 coins total per mariowiki") survives, in the area's notes prose. so the room is
# laid out here from that count - a walkable row of coins over the default ground, an exit pipe past
# them - and a rom-measure pass replaces the whole thing once the real room is transcribed
AREA_COIN_ROW = GROUND_ROW - 1
AREA_FIRST_COIN_COLUMN = 1
AREA_EXIT_GAP_COLUMNS = 2
AREA_EXIT_PIPE_HEIGHT = 2
AREA_PAD_COLUMNS = 4
# the warp room's three pipes stand this far apart, starting this far in
AREA_WARP_FIRST_COLUMN = 3
AREA_WARP_SPACING = 4
AREA_WARP_PIPE_HEIGHT = 2

DEFAULT_BANK = 1
DEFAULT_AREA_BANK = 7


def load_bible(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def feature_max_x(bible):
    max_x = 0
    for t in bible.get("terrain", []):
        if t["kind"] in ("ground", "gap", "stairs", "elevation", "island", "lift_platform"):
            max_x = max(max_x, t["x1"])
        elif t["kind"] == "pipe":
            max_x = max(max_x, t["x"] + 1)
    for b in bible.get("blocks", []):
        if b.get("x") is not None:
            max_x = max(max_x, b["x"])
    for e in bible.get("enemies", []):
        if e.get("x") is not None:
            max_x = max(max_x, e["x"])
    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        # the castle stands past the pole, and the level has to be long enough to hold it and
        # still leave the camera somewhere to stop
        max_x = max(max_x, flag["x"] + CASTLE_FLAG_GAP + CASTLE_WIDTH)
    castle = bible.get("castle_end") or {}
    if castle.get("x") is not None:
        max_x = max(max_x, castle["x"])
    return max_x


def padded(col):
    return list(col) + [BLOCK_EMPTY] * (ROW_STRIDE - LEVEL_ROWS)


def new_grid(length_columns, base_ground=True):
    # column-major: grid[col] is one column's 15 row bytes, row 0 at the top
    grid = []
    for _ in range(length_columns):
        col = [BLOCK_EMPTY] * LEVEL_ROWS
        if base_ground:
            col[GROUND_ROW] = BLOCK_GROUND
            col[GROUND_ROW + 1] = BLOCK_GROUND
        grid.append(col)
    return grid


def clamp_span(grid, x0, x1):
    return max(0, x0), min(len(grid) - 1, x1)


def settle_ground(grid, probes=None):
    # a ground block wears two rows of grass along its top, so only the topmost block of a stack is
    # a surface: everything with ground directly above it becomes the plain rubble kind. run this
    # last, once nothing else is still going to write ground, and it rewrites any probe it moves
    # out from under so the golden set keeps describing the cell it names
    moved = set()
    for x, col in enumerate(grid):
        for row in range(LEVEL_ROWS - 1, 0, -1):
            if col[row] == BLOCK_GROUND and col[row - 1] == BLOCK_GROUND:
                col[row] = BLOCK_GROUND_FILL
                moved.add((x, row))
    if probes is not None:
        for i, (column, row, kind) in enumerate(probes):
            if kind == BLOCK_GROUND and (column, row) in moved:
                probes[i] = (column, row, BLOCK_GROUND_FILL)


def apply_gap(grid, x0, x1, fill=BLOCK_EMPTY):
    x0, x1 = clamp_span(grid, x0, x1)
    for x in range(x0, x1 + 1):
        grid[x][GROUND_ROW] = fill
        grid[x][GROUND_ROW + 1] = fill


def apply_ground(grid, x0, x1):
    x0, x1 = clamp_span(grid, x0, x1)
    for x in range(x0, x1 + 1):
        grid[x][GROUND_ROW] = BLOCK_GROUND
        grid[x][GROUND_ROW + 1] = BLOCK_GROUND


def apply_ceiling(grid):
    for col in grid:
        for row in range(CEILING_ROWS):
            col[row] = BLOCK_GROUND


def apply_pipe(grid, x, height):
    if x + 1 >= len(grid):
        return
    top_row = GROUND_ROW - height
    grid[x][top_row] = BLOCK_PIPE_TL
    grid[x + 1][top_row] = BLOCK_PIPE_TR
    for row in range(top_row + 1, GROUND_ROW):
        grid[x][row] = BLOCK_PIPE_BODY_L
        grid[x + 1][row] = BLOCK_PIPE_BODY_R


def apply_stairs(grid, x0, x1, step_height):
    # a positive step_height climbs one row per column up to its cap; a negative one is the bible's
    # descending castle opening, which starts |step_height| rows up and walks back down to the floor
    x0, x1 = clamp_span(grid, x0, x1)
    span = max(1, x1 - x0)
    for col in range(x0, x1 + 1):
        if step_height >= 0:
            step = min(col - x0 + 1, step_height)
        else:
            top = -step_height
            step = top - (top * (col - x0)) // span
        for row in range(GROUND_ROW - step, GROUND_ROW):
            grid[col][row] = BLOCK_STAIR


def apply_elevation(grid, x0, x1, y):
    x0, x1 = clamp_span(grid, x0, x1)
    for col in range(x0, x1 + 1):
        for row in range(y, GROUND_ROW + 2):
            grid[col][row] = BLOCK_GROUND


def apply_island(grid, x0, x1, y):
    # SCHEMA.md's island: a surface at row y over the death plane. at the ground row it is plain
    # ground; above it, it is the athletic level's thin tree/mushroom platform
    x0, x1 = clamp_span(grid, x0, x1)
    if y >= GROUND_ROW:
        apply_ground(grid, x0, x1)
        return
    for col in range(x0, x1 + 1):
        grid[col][y] = BLOCK_THIN


def apply_block(grid, x, y, kind):
    if 0 <= x < len(grid):
        grid[x][y] = BLOCK_KIND_MAP.get(kind, BLOCK_EMPTY)


def apply_flag_head(grid, col):
    # the ball caps the shaft one row above it and the pennant hangs off the pole's left, at the
    # row under the ball - smb's own arrangement. neither is ever forced: a cell that already
    # holds something keeps it, and the shaft's own rows (and so the scoring bands, which key off
    # FLAG_POLE_TOP_ROW/FLAG_POLE_BOTTOM_ROW and not off any cell's kind) are left alone
    ball_row = FLAG_POLE_TOP_ROW - 1
    if ball_row >= 0 and grid[col][ball_row] == BLOCK_EMPTY:
        grid[col][ball_row] = BLOCK_FLAG_BALL
    if col > 0 and grid[col - 1][FLAG_POLE_TOP_ROW] == BLOCK_EMPTY:
        grid[col - 1][FLAG_POLE_TOP_ROW] = BLOCK_FLAG_CLOTH


def apply_castle(grid, x0):
    # smb's small castle: a three-wide tower over a five-wide keep, its bottom row resting on the
    # row above the ground. every cell is scenery, so it is only ever painted over sky
    rows = [
        [None, BLOCK_CASTLE_CRENEL, BLOCK_CASTLE_CRENEL, BLOCK_CASTLE_CRENEL, None],
        [None, BLOCK_CASTLE_WINDOW, BLOCK_CASTLE, BLOCK_CASTLE_WINDOW, None],
        [BLOCK_CASTLE_CRENEL] * 5,
        [BLOCK_CASTLE, BLOCK_CASTLE, BLOCK_CASTLE_DOOR_TOP, BLOCK_CASTLE, BLOCK_CASTLE],
        [BLOCK_CASTLE, BLOCK_CASTLE, BLOCK_CASTLE_DOOR, BLOCK_CASTLE, BLOCK_CASTLE],
    ]
    top_row = GROUND_ROW - CASTLE_HEIGHT
    placed = None
    for dy, row in enumerate(rows):
        for dx, kind in enumerate(row):
            col = x0 + dx
            if kind is None or col < 0 or col >= len(grid):
                continue
            if grid[col][top_row + dy] != BLOCK_EMPTY:
                continue
            grid[col][top_row + dy] = kind
            if placed is None:
                placed = (col, top_row + dy, kind)
    return placed


def apply_flag(grid, x):
    # only the column is sourced ("unknown" confidence per SCHEMA.md); the pole's height is a
    # provisional placeholder. the bible's approx flag x can land inside the end-of-level staircase
    # (1-1: stairs x1 and flag x are both 158) while its own note places the pole "just after the
    # sourced closing staircase". a pole always stands on ground, so rather than truncating it into
    # whatever cells the stairs left over, walk right to the first column that has intact ground and
    # a clear pole shaft. the stairs/ground are never overwritten. returns the column used, or None
    for col in range(x, len(grid)):
        if grid[col][GROUND_ROW] != BLOCK_GROUND:
            continue
        if all(grid[col][row] == BLOCK_EMPTY for row in range(FLAG_POLE_TOP_ROW, FLAG_POLE_BOTTOM_ROW + 1)):
            for row in range(FLAG_POLE_TOP_ROW, FLAG_POLE_BOTTOM_ROW + 1):
                grid[col][row] = BLOCK_FLAG_POLE
            return col
    return None


def surface_row(grid, column, first_row=0):
    # the topmost solid row of a column, which is the row a body standing there rests on top of.
    # first_row skips an underground level's roof, which is solid but is nobody's floor
    if column < 0 or column >= len(grid):
        return GROUND_ROW
    for row in range(first_row, LEVEL_ROWS):
        if grid[column][row] not in WALK_THROUGH:
            return row
    return GROUND_ROW


def entry_pipe_x(bible, level_type):
    # 1-2's opening pipe is the roof the player falls through, and its column is the start cell. the
    # entry cutscene is m8b's; until then he walks in, so that pipe is not compiled at all
    if level_type != TYPE_UNDERGROUND:
        return None
    start_x = bible.get("start", {}).get("x")
    for t in bible.get("terrain", []):
        if t["kind"] == "pipe" and t["x"] == start_x:
            return start_x
    return None


def lift_span(x0, x1):
    return min(LIFT_SPAN_MASK, max(LIFT_MIN_SPAN, x1 - x0 - LIFT_BLOCKS))


def fit_lift(grid, obj):
    # a horizontal lift standing over open air rides all the way to the first column that has a
    # floor again, so its deck ends flush with the lip a rider steps off onto. a lift the bible
    # places over solid ground is decoration and keeps the span it was given
    column, row, kind, param = obj
    if kind != OBJ_LIFT_H or column >= len(grid):
        return obj
    if grid[column][GROUND_ROW] not in (BLOCK_EMPTY, BLOCK_LAVA):
        return obj
    edge = column
    while edge < len(grid) and grid[edge][GROUND_ROW] in (BLOCK_EMPTY, BLOCK_LAVA):
        edge += 1
    span = min(LIFT_SPAN_MASK, max(1, edge - LIFT_BLOCKS - column))
    return (column, row, kind, (param & LIFT_REVERSE) | span)


def decor_cells(item):
    # one bible decor entry expanded into (column, row, kind) cells. SCHEMA.md's decor grid is the
    # level grid: x is a block column, a cloud's y is a block row, and a hill or a bush always
    # stands on DECOR_BASE_ROW, the row the grass is about to grow out of
    kind = item.get("kind")
    x = item.get("x")
    if x is None:
        return []
    if kind == "big_hill":
        # peak / slope-fill-slope / slope-fill-fill-fill-slope, five wide and three tall
        return [
            (x + 2, DECOR_BASE_ROW - 2, BLOCK_HILL_PEAK),
            (x + 1, DECOR_BASE_ROW - 1, BLOCK_HILL_SLOPE_L),
            (x + 2, DECOR_BASE_ROW - 1, BLOCK_HILL_FILL),
            (x + 3, DECOR_BASE_ROW - 1, BLOCK_HILL_SLOPE_R),
            (x + 0, DECOR_BASE_ROW, BLOCK_HILL_SLOPE_L),
            (x + 1, DECOR_BASE_ROW, BLOCK_HILL_FILL),
            (x + 2, DECOR_BASE_ROW, BLOCK_HILL_FILL),
            (x + 3, DECOR_BASE_ROW, BLOCK_HILL_FILL),
            (x + 4, DECOR_BASE_ROW, BLOCK_HILL_SLOPE_R),
        ]
    if kind == "small_hill":
        return [
            (x + 1, DECOR_BASE_ROW - 1, BLOCK_HILL_PEAK),
            (x + 0, DECOR_BASE_ROW, BLOCK_HILL_SLOPE_L),
            (x + 1, DECOR_BASE_ROW, BLOCK_HILL_FILL),
            (x + 2, DECOR_BASE_ROW, BLOCK_HILL_SLOPE_R),
        ]
    if kind == "bush":
        # the narrowest bush smb draws is its two caps back to back, so a width of one still
        # spends two columns rather than inventing a stubbier shape
        width = max(DECOR_BUSH_MIN_WIDTH, int(item.get("width", DECOR_BUSH_MIN_WIDTH)))
        cells = [(x, DECOR_BASE_ROW, BLOCK_BUSH_L)]
        cells += [(x + i, DECOR_BASE_ROW, BLOCK_BUSH_M) for i in range(1, width - 1)]
        cells.append((x + width - 1, DECOR_BASE_ROW, BLOCK_BUSH_R))
        return cells
    if kind == "cloud":
        y = item.get("y")
        if y is None:
            return []
        width = max(DECOR_CLOUD_MIN_WIDTH, int(item.get("width", DECOR_CLOUD_MIN_WIDTH)))
        top = [BLOCK_CLOUD_TL] + [BLOCK_CLOUD_T] * (width - 2) + [BLOCK_CLOUD_TR]
        bottom = [BLOCK_CLOUD_BL] + [BLOCK_CLOUD_B] * (width - 2) + [BLOCK_CLOUD_BR]
        cells = []
        for i in range(width):
            cells.append((x + i, y, top[i]))
            cells.append((x + i, y + 1, bottom[i]))
        return cells
    return []


def reserved_cells(bible):
    # a hidden block renders as sky but its cell is spoken for, and the engine's reaction list
    # still names it. scenery must not move into one
    out = set()
    for b in bible.get("blocks", []):
        if b.get("x") is not None and b.get("y") is not None:
            out.add((b["x"], b["y"]))
    return out


def apply_decor(grid, bible):
    # scenery never displaces anything, and it is placed whole or not at all. a hill is a dome over
    # a pair of slopes and a bush is two caps around a run of middles: drop one cell of either and
    # what is left is not a smaller hill, it is a broken one. so a shape whose every cell is not
    # open sky - inside the compiled level, not already terrain, not a cell the reaction list has
    # claimed for a hidden block - is skipped entirely and the bible's column is the thing to move.
    #
    # returns one probe per decor kind that actually landed, which is what pins the rendered
    # families in the host test without making the golden set walk the camera past every cloud
    reserved = reserved_cells(bible)
    probes = []
    seen = set()
    for item in bible.get("decor", []):
        cells = decor_cells(item)
        fits = all(0 <= column < len(grid) and 0 <= row < LEVEL_ROWS
                   and grid[column][row] == BLOCK_EMPTY and (column, row) not in reserved
                   for column, row, kind in cells)
        if not fits:
            continue
        for column, row, kind in cells:
            grid[column][row] = kind
            if kind not in seen:
                seen.add(kind)
                probes.append((column, row, kind))
    return probes


def compile_grid(bible, level_type):
    terrain = bible.get("terrain", [])
    has_islands = any(t["kind"] == "island" for t in terrain)
    length_columns = feature_max_x(bible) + 1 + PAD_COLUMNS
    # an athletic level is islands over open air: only the runs the bible names are solid, so the
    # default ground is dropped and everything the level does not place is a pit
    grid = new_grid(length_columns, base_ground=not has_islands)
    probes = []
    objects = []
    skip_pipe = entry_pipe_x(bible, level_type)
    gap_fill = BLOCK_LAVA if level_type == TYPE_CASTLE else BLOCK_EMPTY
    ground_runs = []

    first_row = CEILING_ROWS if level_type == TYPE_UNDERGROUND else 0
    if level_type == TYPE_UNDERGROUND:
        apply_ceiling(grid)
        probes.append((0, 0, BLOCK_GROUND))
        probes.append((length_columns - 1, CEILING_ROWS - 1, BLOCK_GROUND))

    for t in terrain:
        if t["kind"] == "ground":
            apply_ground(grid, t["x0"], t["x1"])
            ground_runs.append((t["x0"], t["x1"]))
            probes.append((t["x0"], GROUND_ROW, BLOCK_GROUND))
            probes.append((t["x1"], GROUND_ROW, BLOCK_GROUND))
        elif t["kind"] == "gap":
            apply_gap(grid, t["x0"], t["x1"], gap_fill)
            probes.append((t["x0"], GROUND_ROW, gap_fill))
            probes.append((t["x1"], GROUND_ROW, gap_fill))
            if level_type == TYPE_CASTLE and t["x1"] - t["x0"] >= CASTLE_LIFT_GAP:
                objects.append((t["x0"] + 1, HORIZONTAL_LIFT_ROW, OBJ_LIFT_H, lift_span(t["x0"], t["x1"])))
        elif t["kind"] == "pipe":
            if t["x"] == skip_pipe:
                continue
            apply_pipe(grid, t["x"], t["height"])
            top_row = GROUND_ROW - t["height"]
            probes.append((t["x"], top_row, BLOCK_PIPE_TL))
            probes.append((t["x"] + 1, top_row, BLOCK_PIPE_TR))
        elif t["kind"] == "stairs":
            # the bible calls a level's closing staircase its "final stone platform": on an island
            # level, where nothing is solid unless the bible places it, the staircase and the run
            # out to the flag are the ground the pole and the walk-off need
            if has_islands:
                apply_ground(grid, t["x0"], length_columns - 1)
            apply_stairs(grid, t["x0"], t["x1"], t["step_height"])
            if t["step_height"] >= 0:
                probes.append((t["x0"], GROUND_ROW - 1, BLOCK_STAIR))
                last_step = min(t["x1"] - t["x0"] + 1, t["step_height"])
                probes.append((t["x1"], GROUND_ROW - last_step, BLOCK_STAIR))
            else:
                probes.append((t["x0"], GROUND_ROW + t["step_height"], BLOCK_STAIR))
        elif t["kind"] == "elevation":
            apply_elevation(grid, t["x0"], t["x1"], t["y"])
            probes.append((t["x0"], t["y"], BLOCK_GROUND))
        elif t["kind"] == "island":
            apply_island(grid, t["x0"], t["x1"], t["y"])
            kind = BLOCK_GROUND if t["y"] >= GROUND_ROW else BLOCK_THIN
            row = GROUND_ROW if t["y"] >= GROUND_ROW else t["y"]
            probes.append((t["x0"], row, kind))
            probes.append((t["x1"], row, kind))
        elif t["kind"] == "lift_platform":
            # the bible's 1-2 run is a pair of vertical lifts ("left descends, right ascends"), its
            # 1-3 run a pair of horizontal ones. the ground under a horizontal pair is carved away,
            # because riding across is the whole point; a vertical pair keeps whatever is under it
            span = lift_span(t["x0"], t["x1"])
            if level_type == TYPE_UNDERGROUND:
                objects.append((t["x0"], VERTICAL_LIFT_TOP_ROW, OBJ_LIFT_V,
                                VERTICAL_LIFT_SPAN | LIFT_REVERSE))
                objects.append((t["x1"] - LIFT_BLOCKS, VERTICAL_LIFT_TOP_ROW, OBJ_LIFT_V,
                                VERTICAL_LIFT_SPAN))
            else:
                # the track is the carved pit itself, so a lift at either extent leaves a hop-sized
                # gap to the island beside it rather than a jump the bible never promised
                gx0 = t["x0"] - 1
                gx1 = t["x1"] + 1
                apply_gap(grid, gx0, gx1, gap_fill)
                span = min(LIFT_SPAN_MASK, max(LIFT_MIN_SPAN, gx1 - LIFT_BLOCKS + 1 - gx0))
                objects.append((gx0, HORIZONTAL_LIFT_ROW, OBJ_LIFT_H, span))
                objects.append((gx0, HORIZONTAL_LIFT_ROW, OBJ_LIFT_H, span | LIFT_REVERSE))

    for b in bible.get("blocks", []):
        if b.get("x") is None or b.get("y") is None:
            continue
        apply_block(grid, b["x"], b["y"], b["kind"])
        probes.append((b["x"], b["y"], BLOCK_KIND_MAP.get(b["kind"], BLOCK_EMPTY)))

    bridge = None
    axe_column = None
    if level_type == TYPE_CASTLE and ground_runs:
        bridge, axe_column = apply_bridge(grid, ground_runs[-1])
        probes.append((bridge[0], GROUND_ROW, BLOCK_BRIDGE))
        probes.append((bridge[1], GROUND_ROW, BLOCK_BRIDGE))
        probes.append((axe_column, GROUND_ROW - 1, BLOCK_AXE))
        objects.append((axe_column, GROUND_ROW - 1, OBJ_AXE, 0))

    for e in bible.get("enemies", []):
        if e.get("x") is None or e.get("y") is None:
            continue
        if e["kind"] == "firebar":
            row = min(e["y"], GROUND_ROW - 1)
            grid[e["x"]][row] = BLOCK_HARD
            probes.append((e["x"], row, BLOCK_HARD))
            objects.append((e["x"], row, OBJ_FIREBAR, FIREBAR_SEGMENTS))
        elif e["kind"] == "bowser_fake":
            # the bible puts him on the bridge; the patrol is the bridge's own last stretch, so he
            # stands between the player and the axe exactly as the source describes
            span = max(2, (bridge[1] - bridge[0]) // 2) if bridge is not None else 4
            column = (bridge[1] - span) if bridge is not None else e["x"]
            objects.append((column, surface_row(grid, column, first_row) - 1, OBJ_BOWSER, span))
        elif e["kind"] == "lift_horizontal":
            objects.append((e["x"], e["y"], OBJ_LIFT_H, LIFT_MIN_SPAN))
        elif e["kind"] == "lift_vertical":
            objects.append((e["x"], e["y"], OBJ_LIFT_V, LIFT_MIN_SPAN))

    # a horizontal lift only earns its keep if its deck actually reaches the far lip of the pit it
    # spans, and the pit's real width is only known once every terrain run has been laid down
    objects = [fit_lift(grid, o) for o in objects]

    flag_col = None
    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        flag_col = apply_flag(grid, flag["x"])
    if flag_col is not None:
        probes.append((flag_col, FLAG_POLE_TOP_ROW, BLOCK_FLAG_POLE))
        probes.append((flag_col, FLAG_POLE_BOTTOM_ROW, BLOCK_FLAG_POLE))
        apply_flag_head(grid, flag_col)
        probes.append((flag_col, FLAG_POLE_TOP_ROW - 1, BLOCK_FLAG_BALL))
        if flag_col > 0:
            probes.append((flag_col - 1, FLAG_POLE_TOP_ROW, BLOCK_FLAG_CLOTH))
        castle = apply_castle(grid, flag_col + CASTLE_FLAG_GAP)
        if castle is not None:
            probes.append(castle)

    # the bible's own scenery, stamped last so it can only ever land on sky
    probes.extend(apply_decor(grid, bible))
    settle_ground(grid, probes)

    return {
        "grid": grid,
        "columns": length_columns,
        "probes": probes,
        "flag_column": flag_col,
        "objects": objects,
        "bridge": bridge,
        "axe_column": axe_column,
    }


def apply_bridge(grid, run):
    # the castle's ending: the bible's last ground run carries the bridge, lava runs under it, and
    # the two columns past it stay solid so the axe has something to stand on
    x0, x1 = run
    x0, x1 = clamp_span(grid, x0, x1)
    end = max(x0, x1 - BRIDGE_TAIL_COLUMNS)
    for col in range(x0, end):
        grid[col][GROUND_ROW] = BLOCK_BRIDGE
        grid[col][GROUND_ROW + 1] = BLOCK_LAVA
    axe_column = end
    grid[axe_column][GROUND_ROW - 1] = BLOCK_AXE
    return (x0, end - 1), axe_column


def compile_block_list(bible):
    # the reaction list the rom scans on a head bump: every positioned question/brick/hidden block
    # with its contents. hard blocks never react, so they stay out of it
    out = []
    for b in bible.get("blocks", []):
        if b.get("x") is None or b.get("y") is None:
            continue
        kind = LIST_KIND_MAP.get(b["kind"])
        if kind is None:
            continue
        out.append((b["x"], b["y"], kind, CONTENT_MAP.get(b.get("contents", "nothing"), 0)))
    return out


def compile_enemy_list(bible, grid, first_row):
    # the spawn list the rom walks with a single advancing cursor, so it must be sorted by column:
    # the engine only ever compares the cursor's own entry against the camera's right edge. the
    # bible's count_only entries carry no position and are dropped rather than invented
    out = []
    for e in bible.get("enemies", []):
        if e.get("x") is None or e.get("y") is None:
            continue
        kind = ENEMY_KIND_MAP.get(e["kind"])
        if kind is None:
            continue
        row = e["y"]
        if e["kind"] == "piranha":
            # the plant lives in a pipe: the roster's rise/sink runs out of the cap it sits on
            row = surface_row(grid, e["x"], first_row)
        out.append((e["x"], row, kind))
    out.sort(key=lambda entry: entry[0])
    return out


def find_pipe_entries(bible, areas):
    # every terrain pipe the bible gives a dest, matched to the area it leads into by entry_x first
    # and by the area's kind second, plus any area whose entry_x names a column with no such pipe
    out = []
    claimed = set()
    for t in bible.get("terrain", []):
        if t["kind"] != "pipe" or not t.get("dest"):
            continue
        for index, area in enumerate(areas):
            if index in claimed:
                continue
            if area.get("entry_x") == t["x"] or area.get("kind") == t["dest"]:
                out.append((t["x"], GROUND_ROW - t["height"], index))
                claimed.add(index)
                break
    for index, area in enumerate(areas):
        if index in claimed or area.get("entry_x") is None:
            continue
        # the bible's warp-zone room is reached "via the lift platforms through a hidden roof
        # opening" and smbd is confirmed to have changed the room entirely, so the entry is
        # compiled as a plain ground pipe at the bible's entry_x. must-verify against the rom
        out.append((area["entry_x"], GROUND_ROW - AREA_EXIT_PIPE_HEIGHT, index))
        claimed.add(index)
    out.sort(key=lambda entry: entry[0])
    return out


def area_coin_count(area):
    # the count is prose-only in the bible ("19 coins total per mariowiki"), so it is read out of the
    # notes rather than duplicated here; a placed-coin json would make this a plain len()
    match = re.search(r"(\d+)\s+coins", area.get("notes", "") or "")
    if match:
        return int(match.group(1))
    return sum(1 for b in area.get("blocks", []) if b.get("contents") == "coin")


def warp_target(to_level, level_ids):
    # the bible's warp targets are worlds 2-4, which do not exist yet, and the minus world is a
    # clipping trick rather than a pipe. anything we cannot name clamps to the last level we have
    if to_level in level_ids:
        return level_ids.index(to_level)
    if re.match(r"^\d+-\d+$", to_level or ""):
        return len(level_ids) - 1
    return None


def compile_area(area, bible, level_ids):
    kind = AREA_KIND_MAP.get(area.get("kind"), AREA_BONUS)
    if kind == AREA_WARP:
        return compile_warp_area(area, bible, level_ids)

    coins = area_coin_count(area)
    exit_column = AREA_FIRST_COIN_COLUMN + coins + AREA_EXIT_GAP_COLUMNS
    length_columns = exit_column + 2 + AREA_PAD_COLUMNS
    grid = new_grid(length_columns)
    probes = []
    coin_cells = []

    for i in range(coins):
        column = AREA_FIRST_COIN_COLUMN + i
        grid[column][AREA_COIN_ROW] = BLOCK_COIN
        coin_cells.append((column, AREA_COIN_ROW))
    apply_pipe(grid, exit_column, AREA_EXIT_PIPE_HEIGHT)

    # the area's own bible blocks render, but only its coins are collectible this pass
    for b in area.get("blocks", []):
        if b.get("x") is None or b.get("y") is None:
            continue
        apply_block(grid, b["x"], b["y"], b["kind"])
        probes.append((b["x"], b["y"], BLOCK_KIND_MAP.get(b["kind"], BLOCK_EMPTY)))

    exit_top_row = GROUND_ROW - AREA_EXIT_PIPE_HEIGHT
    probes.append((0, GROUND_ROW, BLOCK_GROUND))
    if coin_cells:
        probes.append((coin_cells[0][0], AREA_COIN_ROW, BLOCK_COIN))
        probes.append((coin_cells[-1][0], AREA_COIN_ROW, BLOCK_COIN))
    probes.append((exit_column, exit_top_row, BLOCK_PIPE_TL))
    probes.append((exit_column + 1, exit_top_row, BLOCK_PIPE_TR))
    settle_ground(grid, probes)

    return {
        "kind": kind,
        "grid": grid,
        "columns": length_columns,
        "coins": coin_cells,
        "warps": [],
        "exit_column": exit_column,
        "exit_top_row": exit_top_row,
        "probes": probes,
    }


def compile_warp_area(area, bible, level_ids):
    # the room the bible describes as three pipes side by side, one per destination world. the
    # ordering (left to right) is the sourced part; the room's own geometry is not
    warps = []
    for w in bible.get("warps", []):
        if w.get("via") != "warp_zone":
            continue
        target = warp_target(w.get("to_level"), level_ids)
        if target is None:
            continue
        warps.append(target)
    warps = warps[:3]

    length_columns = AREA_WARP_FIRST_COLUMN + max(1, len(warps)) * AREA_WARP_SPACING + AREA_PAD_COLUMNS
    grid = new_grid(length_columns)
    probes = [(0, GROUND_ROW, BLOCK_GROUND)]
    cells = []
    for i, target in enumerate(warps):
        column = AREA_WARP_FIRST_COLUMN + i * AREA_WARP_SPACING
        apply_pipe(grid, column, AREA_WARP_PIPE_HEIGHT)
        probes.append((column, GROUND_ROW - AREA_WARP_PIPE_HEIGHT, BLOCK_PIPE_TL))
        cells.append((column, target))

    settle_ground(grid, probes)

    # the room has no exit pipe of its own: the three warps are the way out. the exit fields still
    # carry the last pipe so a player who takes none of them is not stranded
    exit_column = cells[-1][0] if cells else 0
    return {
        "kind": AREA_WARP,
        "grid": grid,
        "columns": length_columns,
        "coins": [],
        "warps": cells,
        "exit_column": exit_column,
        "exit_top_row": GROUND_ROW - AREA_WARP_PIPE_HEIGHT,
        "probes": probes,
    }


def slug_for(level_id):
    return "level_" + level_id.replace("-", "_")


def to_camel(slug):
    return "".join(part.capitalize() for part in slug.split("_"))


def write_header(out_dir, slug, level, source_path):
    guard = slug.upper() + "_H"
    upper = slug.upper()
    path = os.path.join(out_dir, slug + ".h")
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("\n#include <stdint.h>\n\n")
        f.write("#define %s_ROWS %dU\n" % (upper, LEVEL_ROWS))
        f.write("// stored stride: one padding byte past the last row keeps a cell index a shift\n")
        f.write("#define %s_ROW_STRIDE %dU\n" % (upper, ROW_STRIDE))
        f.write("// the level's own kLevelType* value, which picks its palette set\n")
        f.write("#define %s_TYPE %dU\n" % (upper, level["type"]))
        f.write("// the bible's start column, and the surface row the compiled grid actually put\n")
        f.write("// there - castles start partway up a staircase the bible only guessed the row of\n")
        f.write("#define %s_START_COLUMN %dU\n" % (upper, level["start_column"]))
        f.write("#define %s_START_ROW %dU\n" % (upper, level["start_row"]))
        f.write(
            "// provisional: last positioned bible feature plus %d columns of padding; a rom-measure\n"
            % PAD_COLUMNS
        )
        f.write("// pass will replace this once length_columns is sourced instead of derived\n")
        f.write("#define %s_LENGTH_COLUMNS %dU\n" % (upper, level["columns"]))
        f.write("// the rom bank the level compiles into; level.c switches into it to unpack the grid\n")
        f.write("#define %s_BANK %dU\n" % (upper, level["bank"]))
        f.write("// the bible's countdown, in smb ticks; the hud spends one every kTimerFramesPerTick\n")
        f.write("#define %s_TIMER %dU\n" % (upper, level["timer"]))
        f.write("// the flag pole shaft: its column and the top/bottom rows apply_flag() filled. the\n")
        f.write("// pole's own cells are walk-through, so the engine tests contact against these\n")
        f.write("#define %s_HAS_FLAG %dU\n" % (upper, 0 if level["flag_column"] is None else 1))
        f.write("#define %s_FLAG_COLUMN %dU\n" % (upper, level["flag_column"] or 0))
        f.write("#define %s_FLAG_TOP_ROW %dU\n" % (upper, FLAG_POLE_TOP_ROW))
        f.write("#define %s_FLAG_BASE_ROW %dU\n" % (upper, FLAG_POLE_BOTTOM_ROW))
        f.write("// a castle ends at the axe instead: touching it drops the bridge span below\n")
        bridge = level["bridge"] or (0, 0)
        f.write("#define %s_HAS_AXE %dU\n" % (upper, 0 if level["axe_column"] is None else 1))
        f.write("#define %s_AXE_COLUMN %dU\n" % (upper, level["axe_column"] or 0))
        f.write("#define %s_AXE_ROW %dU\n" % (upper, GROUND_ROW - 1))
        f.write("#define %s_BRIDGE_X0 %dU\n" % (upper, bridge[0]))
        f.write("#define %s_BRIDGE_X1 %dU\n" % (upper, bridge[1]))
        f.write("#define %s_BRIDGE_ROW %dU\n\n" % (upper, GROUND_ROW))
        f.write(
            "extern const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROW_STRIDE];\n\n" % (slug, upper, upper)
        )

        f.write("// the head-bump reaction list: every positioned question/brick/hidden block and what\n")
        f.write("// it holds. kinds are the kBlockList* contract, contents the kContent* one\n")
        f.write("#define %s_BLOCK_COUNT %dU\n" % (upper, len(level["blocks"])))
        # unsized declarations: a bible that places none of something still needs an object to
        # point at, and write_objects pads an empty list with one entry the count never reaches
        f.write("extern const uint16_t %s_block_column[];\n" % slug)
        f.write("extern const uint8_t %s_block_row[];\n" % slug)
        f.write("extern const uint8_t %s_block_kind[];\n" % slug)
        f.write("extern const uint8_t %s_block_content[];\n\n" % slug)

        f.write("// the position-triggered enemy spawn list, sorted by column so one cursor walks it.\n")
        f.write("// row is the surface row the enemy stands on top of, the start cell's own convention\n")
        f.write("#define %s_ENEMY_COUNT %dU\n" % (upper, len(level["enemies"])))
        f.write("extern const uint16_t %s_enemy_column[];\n" % slug)
        f.write("extern const uint8_t %s_enemy_row[];\n" % slug)
        f.write("extern const uint8_t %s_enemy_kind[];\n\n" % slug)

        f.write("// the object list: pipes, lifts, firebars, the fake bowser and the axe, all kept in\n")
        f.write("// one array because every one of them is a column, a row, a kObj* kind and a span\n")
        f.write("#define %s_OBJECT_COUNT %dU\n" % (upper, len(level["objects"])))
        f.write("extern const uint16_t %s_object_column[];\n" % slug)
        f.write("extern const uint8_t %s_object_row[];\n" % slug)
        f.write("extern const uint8_t %s_object_kind[];\n" % slug)
        f.write("extern const uint8_t %s_object_param[];\n\n" % slug)

        f.write("#define %s_AREA_COUNT %dU\n\n" % (upper, len(level["areas"])))
        for index, area in enumerate(level["areas"]):
            name = "%s_AREA%d" % (upper, index)
            f.write("// sub-area %d: its own banked grid, entered from a pipe in the object list\n" % index)
            f.write("#define %s_KIND %dU\n" % (name, area["kind"]))
            f.write("#define %s_COLUMNS %dU\n" % (name, area["columns"]))
            f.write("#define %s_BANK %dU\n" % (name, level["area_bank"]))
            f.write("#define %s_START_COLUMN 0U\n" % name)
            f.write("#define %s_START_ROW %dU\n" % (name, GROUND_ROW))
            f.write("#define %s_EXIT_COLUMN %dU\n" % (name, area["exit_column"]))
            f.write("#define %s_EXIT_TOP_ROW %dU\n" % (name, area["exit_top_row"]))
            f.write("#define %s_RETURN_COLUMN %dU\n" % (name, area["return_column"]))
            f.write("#define %s_RETURN_TOP_ROW %dU\n" % (name, area["return_top_row"]))
            f.write("#define %s_COIN_COUNT %dU\n" % (name, len(area["coins"])))
            f.write("#define %s_WARP_COUNT %dU\n" % (name, len(area["warps"])))
            f.write(
                "extern const uint8_t %s_area%d_blocks[%s_COLUMNS][%s_ROW_STRIDE];\n"
                % (slug, index, name, upper)
            )
            f.write("extern const uint8_t %s_area%d_coin_column[];\n" % (slug, index))
            f.write("extern const uint8_t %s_area%d_coin_row[];\n" % (slug, index))
            f.write("extern const uint8_t %s_area%d_warp_column[];\n" % (slug, index))
            f.write("extern const uint8_t %s_area%d_warp_level[];\n\n" % (slug, index))

        f.write("#endif\n")


def c_array(f, decl, values):
    # an empty run still gets one byte of storage, so the header's unsized extern has an object to
    # name and nothing has to special-case a bible that placed none of something
    body = ", ".join(str(v) for v in values) if values else "0"
    f.write("%s[] = {%s};\n" % (decl, body))


def write_objects(out_dir, slug, level, source_path):
    # the reaction/object lists are scanned every frame, so they stay in bank 0 where no bank switch
    # stands between the engine and a solidity probe; only the grids pay the banking cost
    upper = slug.upper()
    path = os.path.join(out_dir, slug + "_objects.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write('\n#include "%s.h"\n\n' % slug)
        blocks = level["blocks"]
        c_array(f, "const uint16_t %s_block_column" % slug, [b[0] for b in blocks])
        c_array(f, "const uint8_t %s_block_row" % slug, [b[1] for b in blocks])
        c_array(f, "const uint8_t %s_block_kind" % slug, [b[2] for b in blocks])
        c_array(f, "const uint8_t %s_block_content" % slug, [b[3] for b in blocks])
        enemies = level["enemies"]
        c_array(f, "const uint16_t %s_enemy_column" % slug, [e[0] for e in enemies])
        c_array(f, "const uint8_t %s_enemy_row" % slug, [e[1] for e in enemies])
        c_array(f, "const uint8_t %s_enemy_kind" % slug, [e[2] for e in enemies])
        objects = level["objects"]
        c_array(f, "const uint16_t %s_object_column" % slug, [o[0] for o in objects])
        c_array(f, "const uint8_t %s_object_row" % slug, [o[1] for o in objects])
        c_array(f, "const uint8_t %s_object_kind" % slug, [o[2] for o in objects])
        c_array(f, "const uint8_t %s_object_param" % slug, [o[3] for o in objects])
        for index, area in enumerate(level["areas"]):
            name = "%s_AREA%d" % (upper, index)
            c_array(f, "const uint8_t %s_area%d_coin_column" % (slug, index),
                    [c[0] for c in area["coins"]])
            c_array(f, "const uint8_t %s_area%d_coin_row" % (slug, index),
                    [c[1] for c in area["coins"]])
            c_array(f, "const uint8_t %s_area%d_warp_column" % (slug, index),
                    [w[0] for w in area["warps"]])
            c_array(f, "const uint8_t %s_area%d_warp_level" % (slug, index),
                    [w[1] for w in area["warps"]])


def write_areas(out_dir, slug, level, source_path):
    upper = slug.upper()
    path = os.path.join(out_dir, slug + "_areas.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// one column-major block grid per sub-area, banked away from the main level's own\n")
        f.write("#pragma bank %d\n\n" % level["area_bank"])
        f.write('#include "%s.h"\n\n' % slug)
        for index, area in enumerate(level["areas"]):
            name = "%s_AREA%d" % (upper, index)
            f.write(
                "const uint8_t %s_area%d_blocks[%s_COLUMNS][%s_ROW_STRIDE] = {\n"
                % (slug, index, name, upper)
            )
            for col in area["grid"]:
                names = "/".join(KIND_NAMES[v] for v in col if v != BLOCK_EMPTY)
                comment = (" // %s" % names) if names else ""
                f.write("    {%s},%s\n" % (", ".join(str(v) for v in padded(col)), comment))
            f.write("};\n\n")


def write_source(out_dir, slug, level, source_path):
    path = os.path.join(out_dir, slug + ".c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// column-major block grid; kind values are the kBlock* contract in games/mario/src/mario.h\n")
        f.write("#pragma bank %d\n\n" % level["bank"])
        f.write('#include "%s.h"\n\n' % slug)
        f.write(
            "const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROW_STRIDE] = {\n"
            % (slug, slug.upper(), slug.upper())
        )
        for col in level["grid"]:
            row_bytes = ", ".join(str(v) for v in padded(col))
            names = "/".join(KIND_NAMES[v] for v in col if v != BLOCK_EMPTY)
            comment = (" // %s" % names) if names else ""
            f.write("    {%s},%s\n" % (row_bytes, comment))
        f.write("};\n")


def write_grid(out_dir, slug, level, source_path):
    # the same grid the rom reads out of its banked copy, as a host constant: the traversal tests
    # plan routes against it instead of re-deriving the level from the bible json
    path = os.path.join(out_dir, slug + "_grid.h")
    guard = slug.upper() + "_GRID_H"
    camel = to_camel(slug)
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("\n#include <cstdint>\n\n")
        f.write("#ifndef MARIO_LEVEL_TYPES\n#define MARIO_LEVEL_TYPES\n")
        f.write("struct LevelBlock {\n")
        f.write("    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n    uint8_t content;\n};\n")
        f.write("struct LevelEnemy {\n    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n};\n")
        f.write("struct LevelObject {\n")
        f.write("    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n    uint8_t param;\n};\n")
        f.write("struct LevelWarp {\n    uint8_t column;\n    uint8_t level;\n};\n")
        f.write("#endif\n\n")

        f.write("inline constexpr uint8_t k%sGrid[%d][%d] = {\n" % (camel, len(level["grid"]), LEVEL_ROWS))
        for col in level["grid"]:
            f.write("    {%s},\n" % ", ".join(str(v) for v in col))
        f.write("};\n\n")

        # every list ends in one padding entry the count never reaches: a level that places none of
        # something would otherwise declare a zero-length array
        f.write("inline constexpr LevelBlock k%sBlocks[] = {\n" % camel)
        for column, row, kind, content in level["blocks"]:
            f.write("    {%d, %d, %d, %d},\n" % (column, row, kind, content))
        f.write("    {0, 0, 0, 0},\n};\n")
        f.write("inline constexpr uint16_t k%sBlockCount = %d;\n\n" % (camel, len(level["blocks"])))

        f.write("inline constexpr LevelEnemy k%sEnemies[] = {\n" % camel)
        for column, row, kind in level["enemies"]:
            f.write("    {%d, %d, %d},\n" % (column, row, kind))
        f.write("    {0, 0, 0},\n};\n")
        f.write("inline constexpr uint16_t k%sEnemyCount = %d;\n\n" % (camel, len(level["enemies"])))

        f.write("inline constexpr LevelObject k%sObjects[] = {\n" % camel)
        for column, row, kind, param in level["objects"]:
            f.write("    {%d, %d, %d, %d}, // %s\n" % (column, row, kind, param, OBJ_NAMES[kind]))
        f.write("    {0, 0, 0, 0},\n};\n")
        f.write("inline constexpr uint16_t k%sObjectCount = %d;\n\n" % (camel, len(level["objects"])))

        for index, area in enumerate(level["areas"]):
            f.write(
                "inline constexpr uint8_t k%sArea%dGrid[%d][%d] = {\n"
                % (camel, index, area["columns"], LEVEL_ROWS)
            )
            for col in area["grid"]:
                f.write("    {%s},\n" % ", ".join(str(v) for v in col))
            f.write("};\n\n")
            f.write("inline constexpr LevelWarp k%sArea%dWarps[] = {\n" % (camel, index))
            for column, target in area["warps"]:
                f.write("    {%d, %d},\n" % (column, target))
            f.write("    {0, 0},\n};\n")
            f.write("inline constexpr uint16_t k%sArea%dWarpCount = %d;\n\n"
                    % (camel, index, len(area["warps"])))
        f.write("#endif\n")


def write_probes(out_dir, slug, level, source_path):
    path = os.path.join(out_dir, slug + "_probes.h")
    guard = slug.upper() + "_PROBES_H"
    camel = to_camel(slug)
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// golden probes derived directly from the bible json's terrain runs and blocks; the host\n")
        f.write("// test reads these instead of hand-duplicating the level json's positions\n")
        f.write("\n#include <cstdint>\n\n")
        f.write("#ifndef MARIO_LEVEL_PROBE\n#define MARIO_LEVEL_PROBE\n")
        f.write("struct LevelProbe {\n    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n};\n")
        f.write("#endif\n\n")
        f.write("inline constexpr LevelProbe k%sProbes[] = {\n" % camel)
        for column, row, kind in level["probes"]:
            f.write("    {%d, %d, %d}, // %s\n" % (column, row, kind, KIND_NAMES[kind]))
        f.write("};\n\n")
        f.write(
            "inline constexpr uint16_t k%sProbeCount = sizeof(k%sProbes) / sizeof(k%sProbes[0]);\n\n"
            % (camel, camel, camel)
        )
        for index, area in enumerate(level["areas"]):
            name = "%sArea%d" % (camel, index)
            f.write("inline constexpr LevelProbe k%sProbes[] = {\n" % name)
            for column, row, kind in area["probes"]:
                f.write("    {%d, %d, %d}, // %s\n" % (column, row, kind, KIND_NAMES[kind]))
            f.write("};\n\n")
            f.write(
                "inline constexpr uint16_t k%sProbeCount = sizeof(k%sProbes) / sizeof(k%sProbes[0]);\n\n"
                % (name, name, name)
            )
        f.write("#endif\n")


# the world we can actually warp into; anything else clamps to its last level
LEVEL_IDS = ["1-1", "1-2", "1-3", "1-4"]


def compile_level(bible, bank, area_bank):
    level_type = TYPE_MAP.get(bible.get("type"), TYPE_OVERWORLD)
    built = compile_grid(bible, level_type)
    grid = built["grid"]
    areas = []
    bible_areas = bible.get("areas", [])
    entries = find_pipe_entries(bible, bible_areas)
    entry_of = {index: (column, top_row) for column, top_row, index in entries}
    # an area the bible reaches some other way than a terrain pipe still needs one to stand in
    for column, top_row, _ in entries:
        if grid[column][top_row] != BLOCK_PIPE_TL:
            apply_pipe(grid, column, GROUND_ROW - top_row)
            built["probes"].append((column, top_row, BLOCK_PIPE_TL))

    for index, area in enumerate(bible_areas):
        compiled = compile_area(area, bible, LEVEL_IDS)
        # the bible's exit prose for 1-1 is "same pipe, returns to overworld near entry", so the
        # return column is the entry pipe's own; an area with no compiled pipe returns to the start
        entry = entry_of.get(index, (bible.get("start", {}).get("x", 0), GROUND_ROW - 2))
        compiled["return_column"] = entry[0]
        compiled["return_top_row"] = entry[1]
        areas.append(compiled)

    objects = built["objects"]
    for column, top_row, index in entries:
        objects.append((column, top_row, OBJ_PIPE, index))
    objects.sort(key=lambda o: (o[0], o[2]))

    start_column = bible["start"]["x"]
    first_row = CEILING_ROWS if level_type == TYPE_UNDERGROUND else 0
    return {
        "type": level_type,
        "bank": bank,
        "area_bank": area_bank,
        "timer": bible["timer"],
        "grid": grid,
        "columns": built["columns"],
        "probes": built["probes"],
        "flag_column": built["flag_column"],
        "bridge": built["bridge"],
        "axe_column": built["axe_column"],
        "start_column": start_column,
        "start_row": surface_row(grid, start_column, first_row),
        "blocks": compile_block_list(bible),
        "enemies": compile_enemy_list(bible, grid, first_row),
        "objects": objects,
        "areas": areas,
    }


def main():
    args = sys.argv[1:]
    bank = DEFAULT_BANK
    area_bank = DEFAULT_AREA_BANK
    positional = []
    i = 0
    while i < len(args):
        if args[i] == "--bank":
            bank = int(args[i + 1])
            i += 2
        elif args[i] == "--area-bank":
            area_bank = int(args[i + 1])
            i += 2
        else:
            positional.append(args[i])
            i += 1
    if len(positional) != 2:
        print("usage: compile_level.py <bible.json> <out_dir> [--bank N] [--area-bank M]", file=sys.stderr)
        return 1

    in_path, out_dir = positional
    os.makedirs(out_dir, exist_ok=True)

    bible = load_bible(in_path)
    slug = slug_for(bible["level"])
    level = compile_level(bible, bank, area_bank)

    write_header(out_dir, slug, level, in_path)
    write_source(out_dir, slug, level, in_path)
    write_objects(out_dir, slug, level, in_path)
    write_areas(out_dir, slug, level, in_path)
    write_grid(out_dir, slug, level, in_path)
    write_probes(out_dir, slug, level, in_path)

    print(
        "compiled %s: type %d bank %d, %d columns, %d probes, %d blocks, %d enemies, %d objects, "
        "%d areas, flag column %s -> %s"
        % (bible["level"], level["type"], bank, level["columns"], len(level["probes"]),
           len(level["blocks"]), len(level["enemies"]), len(level["objects"]), len(level["areas"]),
           level["flag_column"], out_dir)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
