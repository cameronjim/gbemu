#!/usr/bin/env python3
"""compiles a games/mario/data/*.json level bible into banked c terrain data plus a host-test
probe header. run as: compile_level.py <bible.json> <out_dir>

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
}

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

# enemies.kind -> the kEnemy* contract in games/mario/src/mario.h. only the two walkers this
# sub-milestone implements are mapped; an unmapped kind is left out of the spawn list entirely
ENEMY_KIND_MAP = {"goomba": 0, "koopa_green": 1}

# schema.md: 15 block rows, ground surface at row 13, rows 13-14 are the solid ground blocks
LEVEL_ROWS = 15
GROUND_ROW = 13
# each column is stored 16 bytes wide, one padding byte past the 15 real rows: sdcc turns a
# power-of-two inner dimension into a shift, and the engine probes a cell six times a frame
ROW_STRIDE = 16

# blocks.kind -> our render kind; "hidden" has no art of its own until something reveals it, so it
# renders as empty (per SCHEMA.md its contents still exist, physics/items are a later sub-milestone)
BLOCK_KIND_MAP = {
    "question": BLOCK_QUESTION,
    "brick": BLOCK_BRICK,
    "hard": BLOCK_HARD,
    "hidden": BLOCK_EMPTY,
}

# columns of clearance kept past the last positioned feature; length_columns is provisional until
# a rom-measure pass replaces the bible's approx/unknown positions (see SCHEMA.md)
PAD_COLUMNS = 8

# the bible's terrain list only calls out pits explicitly; ground is the implied default surface
# everywhere else (see SCHEMA.md's "a pit/gap is a run of columns where the ground terrain is
# absent" wording), so the grid starts all-ground at row 13-14 and gaps carve it away
FLAG_POLE_TOP_ROW = 4
FLAG_POLE_BOTTOM_ROW = GROUND_ROW - 1

# sub-area layout. the bible gives 1-1's bonus room no terrain at all and places none of its coins:
# only the total ("19 coins total per mariowiki") survives, in the area's notes prose. so the room is
# laid out here from that count - a walkable row of coins over the default ground, an exit pipe past
# them - and a rom-measure pass replaces the whole thing once the real room is transcribed
AREA_COIN_ROW = GROUND_ROW - 1
AREA_FIRST_COIN_COLUMN = 1
AREA_EXIT_GAP_COLUMNS = 2
AREA_EXIT_PIPE_HEIGHT = 2
AREA_PAD_COLUMNS = 4
AREA_BANK = 2


def load_bible(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def feature_max_x(bible):
    max_x = 0
    for t in bible.get("terrain", []):
        if t["kind"] in ("ground", "gap"):
            max_x = max(max_x, t["x1"])
        elif t["kind"] == "pipe":
            max_x = max(max_x, t["x"] + 1)
        elif t["kind"] == "stairs":
            max_x = max(max_x, t["x1"])
    for b in bible.get("blocks", []):
        if b.get("x") is not None:
            max_x = max(max_x, b["x"])
    for e in bible.get("enemies", []):
        if e.get("x") is not None:
            max_x = max(max_x, e["x"])
    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        max_x = max(max_x, flag["x"])
    return max_x


def padded(col):
    return list(col) + [BLOCK_EMPTY] * (ROW_STRIDE - LEVEL_ROWS)


def new_grid(length_columns):
    # column-major: grid[col] is one column's 15 row bytes, row 0 at the top
    grid = []
    for _ in range(length_columns):
        col = [BLOCK_EMPTY] * LEVEL_ROWS
        col[GROUND_ROW] = BLOCK_GROUND
        col[GROUND_ROW + 1] = BLOCK_GROUND
        grid.append(col)
    return grid


def apply_gap(grid, x0, x1):
    for x in range(x0, x1 + 1):
        grid[x][GROUND_ROW] = BLOCK_EMPTY
        grid[x][GROUND_ROW + 1] = BLOCK_EMPTY


def apply_pipe(grid, x, height):
    top_row = GROUND_ROW - height
    grid[x][top_row] = BLOCK_PIPE_TL
    grid[x + 1][top_row] = BLOCK_PIPE_TR
    for row in range(top_row + 1, GROUND_ROW):
        grid[x][row] = BLOCK_PIPE_BODY_L
        grid[x + 1][row] = BLOCK_PIPE_BODY_R


def apply_stairs(grid, x0, x1, step_height):
    for col in range(x0, x1 + 1):
        step = min(col - x0 + 1, step_height)
        for row in range(GROUND_ROW - step, GROUND_ROW):
            grid[col][row] = BLOCK_STAIR


def apply_block(grid, x, y, kind):
    grid[x][y] = BLOCK_KIND_MAP.get(kind, BLOCK_EMPTY)


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


def compile_grid(bible):
    length_columns = feature_max_x(bible) + 1 + PAD_COLUMNS
    grid = new_grid(length_columns)
    probes = []

    for t in bible.get("terrain", []):
        if t["kind"] == "ground":
            probes.append((t["x0"], GROUND_ROW, BLOCK_GROUND))
            probes.append((t["x1"], GROUND_ROW, BLOCK_GROUND))
        elif t["kind"] == "gap":
            apply_gap(grid, t["x0"], t["x1"])
            probes.append((t["x0"], GROUND_ROW, BLOCK_EMPTY))
            probes.append((t["x1"], GROUND_ROW, BLOCK_EMPTY))
        elif t["kind"] == "pipe":
            apply_pipe(grid, t["x"], t["height"])
            top_row = GROUND_ROW - t["height"]
            probes.append((t["x"], top_row, BLOCK_PIPE_TL))
            probes.append((t["x"] + 1, top_row, BLOCK_PIPE_TR))
        elif t["kind"] == "stairs":
            apply_stairs(grid, t["x0"], t["x1"], t["step_height"])
            probes.append((t["x0"], GROUND_ROW - 1, BLOCK_STAIR))
            last_step = min(t["x1"] - t["x0"] + 1, t["step_height"])
            probes.append((t["x1"], GROUND_ROW - last_step, BLOCK_STAIR))
        # elevation/lift_platform/island: not present in level-1-1.json; a future level's compile
        # pass adds handling here when the bible actually places one

    for b in bible.get("blocks", []):
        if b.get("x") is None or b.get("y") is None:
            continue
        apply_block(grid, b["x"], b["y"], b["kind"])
        probes.append((b["x"], b["y"], BLOCK_KIND_MAP.get(b["kind"], BLOCK_EMPTY)))

    flag_col = None
    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        flag_col = apply_flag(grid, flag["x"])
    if flag_col is not None:
        probes.append((flag_col, FLAG_POLE_TOP_ROW, BLOCK_FLAG_POLE))
        probes.append((flag_col, FLAG_POLE_BOTTOM_ROW, BLOCK_FLAG_POLE))

    # level-1-1.json has no castle_end: per the milestone's instructions, an unpositioned kind is
    # left out rather than invented; BLOCK_CASTLE still exists in the enum/art for levels that do

    return grid, length_columns, probes, flag_col


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


def compile_enemy_list(bible):
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
        out.append((e["x"], e["y"], kind))
    out.sort(key=lambda entry: entry[0])
    return out


def find_pipe_entry(bible):
    # the bible's enterable pipe: a terrain pipe with a dest, matched to the area it leads into by
    # entry_x first and by the area's kind second. returns (column, top_row, area_index) or None
    areas = bible.get("areas", [])
    for t in bible.get("terrain", []):
        if t["kind"] != "pipe" or not t.get("dest"):
            continue
        for index, area in enumerate(areas):
            if area.get("entry_x") == t["x"] or area.get("kind") == t["dest"]:
                return (t["x"], GROUND_ROW - t["height"], index)
    return None


def area_coin_count(area):
    # the count is prose-only in the bible ("19 coins total per mariowiki"), so it is read out of the
    # notes rather than duplicated here; a placed-coin json would make this a plain len()
    match = re.search(r"(\d+)\s+coins", area.get("notes", "") or "")
    if match:
        return int(match.group(1))
    return sum(1 for b in area.get("blocks", []) if b.get("contents") == "coin")


def compile_area(area):
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

    return {
        "grid": grid,
        "columns": length_columns,
        "coins": coin_cells,
        "exit_column": exit_column,
        "exit_top_row": exit_top_row,
        "probes": probes,
    }


def slug_for(level_id):
    return "level_" + level_id.replace("-", "_")


def write_header(out_dir, slug, length_columns, start, flag_col, source_path, blocks, enemies, entry,
                 areas):
    guard = slug.upper() + "_H"
    upper = slug.upper()
    path = os.path.join(out_dir, slug + ".h")
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("\n#include <stdint.h>\n\n")
        f.write("#define %s_ROWS %dU\n" % (slug.upper(), LEVEL_ROWS))
        f.write("// stored stride: one padding byte past the last row keeps a cell index a shift\n")
        f.write("#define %s_ROW_STRIDE %dU\n" % (slug.upper(), ROW_STRIDE))
        f.write("// the bible's start cell: the column and the ground row the player stands on top of\n")
        f.write("#define %s_START_COLUMN %dU\n" % (slug.upper(), start[0]))
        f.write("#define %s_START_ROW %dU\n" % (slug.upper(), start[1]))
        f.write(
            "// provisional: last positioned bible feature plus %d columns of padding; a rom-measure\n"
            % PAD_COLUMNS
        )
        f.write("// pass will replace this once length_columns is sourced instead of derived\n")
        f.write("#define %s_LENGTH_COLUMNS %dU\n" % (slug.upper(), length_columns))
        f.write("// the rom bank the level compiles into; terrain.c switches into it to read a column\n")
        f.write("#define %s_BANK 1U\n" % slug.upper())
        f.write("// the flag pole shaft: its column and the top/bottom rows apply_flag() filled. the\n")
        f.write("// pole's own cells are walk-through, so the engine tests contact against these\n")
        f.write("#define %s_HAS_FLAG %dU\n" % (slug.upper(), 0 if flag_col is None else 1))
        f.write("#define %s_FLAG_COLUMN %dU\n" % (slug.upper(), flag_col or 0))
        f.write("#define %s_FLAG_TOP_ROW %dU\n" % (slug.upper(), FLAG_POLE_TOP_ROW))
        f.write("#define %s_FLAG_BASE_ROW %dU\n\n" % (slug.upper(), FLAG_POLE_BOTTOM_ROW))
        f.write(
            "extern const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROW_STRIDE];\n\n"
            % (slug, slug.upper(), slug.upper())
        )

        f.write("// the head-bump reaction list: every positioned question/brick/hidden block and what\n")
        f.write("// it holds. kinds are the kBlockList* contract, contents the kContent* one\n")
        f.write("#define %s_BLOCK_COUNT %dU\n" % (upper, len(blocks)))
        f.write("extern const uint16_t %s_block_column[%s_BLOCK_COUNT];\n" % (slug, upper))
        f.write("extern const uint8_t %s_block_row[%s_BLOCK_COUNT];\n" % (slug, upper))
        f.write("extern const uint8_t %s_block_kind[%s_BLOCK_COUNT];\n" % (slug, upper))
        f.write("extern const uint8_t %s_block_content[%s_BLOCK_COUNT];\n\n" % (slug, upper))

        f.write("// the position-triggered enemy spawn list, sorted by column so one cursor walks it.\n")
        f.write("// row is the surface row the enemy stands on top of, the start cell's own convention\n")
        f.write("#define %s_ENEMY_COUNT %dU\n" % (upper, len(enemies)))
        f.write("extern const uint16_t %s_enemy_column[%s_ENEMY_COUNT];\n" % (slug, upper))
        f.write("extern const uint8_t %s_enemy_row[%s_ENEMY_COUNT];\n" % (slug, upper))
        f.write("extern const uint8_t %s_enemy_kind[%s_ENEMY_COUNT];\n\n" % (slug, upper))

        f.write("// the bible's enterable pipe and the sub-area it leads into\n")
        f.write("#define %s_HAS_PIPE_ENTRY %dU\n" % (upper, 0 if entry is None else 1))
        f.write("#define %s_PIPE_ENTRY_COLUMN %dU\n" % (upper, 0 if entry is None else entry[0]))
        f.write("#define %s_PIPE_ENTRY_TOP_ROW %dU\n" % (upper, 0 if entry is None else entry[1]))
        f.write("#define %s_AREA_COUNT %dU\n\n" % (upper, len(areas)))

        for index, area in enumerate(areas):
            name = "%s_AREA%d" % (upper, index)
            f.write("// sub-area %d: its own banked grid, entered from the pipe above and exited at\n" % index)
            f.write("// the room's own pipe, which returns to the linked main-area column\n")
            f.write("#define %s_COLUMNS %dU\n" % (name, area["columns"]))
            f.write("#define %s_BANK %dU\n" % (name, AREA_BANK))
            f.write("#define %s_START_COLUMN 0U\n" % name)
            f.write("#define %s_START_ROW %dU\n" % (name, GROUND_ROW))
            f.write("#define %s_EXIT_COLUMN %dU\n" % (name, area["exit_column"]))
            f.write("#define %s_EXIT_TOP_ROW %dU\n" % (name, area["exit_top_row"]))
            f.write("#define %s_RETURN_COLUMN %dU\n" % (name, area["return_column"]))
            f.write("#define %s_RETURN_TOP_ROW %dU\n" % (name, area["return_top_row"]))
            f.write("#define %s_COIN_COUNT %dU\n" % (name, len(area["coins"])))
            f.write(
                "extern const uint8_t %s_area%d_blocks[%s_COLUMNS][%s_ROW_STRIDE];\n"
                % (slug, index, name, upper)
            )
            f.write("extern const uint8_t %s_area%d_coin_column[%s_COIN_COUNT];\n" % (slug, index, name))
            f.write("extern const uint8_t %s_area%d_coin_row[%s_COIN_COUNT];\n\n" % (slug, index, name))

        f.write("#endif\n")


def write_objects(out_dir, slug, blocks, enemies, areas, source_path):
    # the reaction/coin lists are tens of bytes and are scanned every frame, so they stay in bank 0
    # where no bank switch stands between the engine and a solidity probe
    upper = slug.upper()
    path = os.path.join(out_dir, slug + "_objects.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write('\n#include "%s.h"\n\n' % slug)
        f.write(
            "const uint16_t %s_block_column[%s_BLOCK_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(b[0]) for b in blocks))
        )
        f.write(
            "const uint8_t %s_block_row[%s_BLOCK_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(b[1]) for b in blocks))
        )
        f.write(
            "const uint8_t %s_block_kind[%s_BLOCK_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(b[2]) for b in blocks))
        )
        f.write(
            "const uint8_t %s_block_content[%s_BLOCK_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(b[3]) for b in blocks))
        )
        f.write(
            "const uint16_t %s_enemy_column[%s_ENEMY_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(e[0]) for e in enemies))
        )
        f.write(
            "const uint8_t %s_enemy_row[%s_ENEMY_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(e[1]) for e in enemies))
        )
        f.write(
            "const uint8_t %s_enemy_kind[%s_ENEMY_COUNT] = {%s};\n"
            % (slug, upper, ", ".join(str(e[2]) for e in enemies))
        )
        for index, area in enumerate(areas):
            name = "%s_AREA%d" % (upper, index)
            f.write(
                "const uint8_t %s_area%d_coin_column[%s_COIN_COUNT] = {%s};\n"
                % (slug, index, name, ", ".join(str(c[0]) for c in area["coins"]))
            )
            f.write(
                "const uint8_t %s_area%d_coin_row[%s_COIN_COUNT] = {%s};\n"
                % (slug, index, name, ", ".join(str(c[1]) for c in area["coins"]))
            )


def write_areas(out_dir, slug, areas, source_path):
    upper = slug.upper()
    path = os.path.join(out_dir, slug + "_areas.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// one column-major block grid per sub-area, banked away from the main level's own\n")
        f.write("#pragma bank %d\n\n" % AREA_BANK)
        f.write('#include "%s.h"\n\n' % slug)
        for index, area in enumerate(areas):
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


def write_source(out_dir, slug, grid, source_path):
    path = os.path.join(out_dir, slug + ".c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// column-major block grid; kind values are the kBlock* contract in games/mario/src/mario.h\n")
        f.write("#pragma bank 1\n\n")
        f.write('#include "%s.h"\n\n' % slug)
        f.write(
            "const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROW_STRIDE] = {\n"
            % (slug, slug.upper(), slug.upper())
        )
        for col in grid:
            row_bytes = ", ".join(str(v) for v in padded(col))
            names = "/".join(KIND_NAMES[v] for v in col if v != BLOCK_EMPTY)
            comment = (" // %s" % names) if names else ""
            f.write("    {%s},%s\n" % (row_bytes, comment))
        f.write("};\n")


def write_grid(out_dir, slug, grid, source_path, blocks, enemies, areas):
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
        f.write(
            "inline constexpr uint8_t k%sGrid[%d][%d] = {\n" % (camel, len(grid), LEVEL_ROWS)
        )
        for col in grid:
            f.write("    {%s},\n" % ", ".join(str(v) for v in col))
        f.write("};\n\n")

        # the host twin needs the reaction list too: a hidden block is sky in the grid above and only
        # this list says a bump would materialize one there
        f.write("struct LevelBlock {\n")
        f.write("    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n    uint8_t content;\n};\n\n")
        f.write("inline constexpr LevelBlock k%sBlocks[] = {\n" % camel)
        for column, row, kind, content in blocks:
            f.write("    {%d, %d, %d, %d},\n" % (column, row, kind, content))
        f.write("};\n\n")
        f.write(
            "inline constexpr uint16_t k%sBlockCount = sizeof(k%sBlocks) / sizeof(k%sBlocks[0]);\n\n"
            % (camel, camel, camel)
        )

        # the enemy twin plans against exactly the list the rom's spawn cursor walks
        f.write("struct LevelEnemy {\n")
        f.write("    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n};\n\n")
        f.write("inline constexpr LevelEnemy k%sEnemies[] = {\n" % camel)
        for column, row, kind in enemies:
            f.write("    {%d, %d, %d},\n" % (column, row, kind))
        f.write("};\n\n")
        f.write(
            "inline constexpr uint16_t k%sEnemyCount = sizeof(k%sEnemies) / sizeof(k%sEnemies[0]);\n\n"
            % (camel, camel, camel)
        )

        for index, area in enumerate(areas):
            f.write(
                "inline constexpr uint8_t k%sArea%dGrid[%d][%d] = {\n"
                % (camel, index, area["columns"], LEVEL_ROWS)
            )
            for col in area["grid"]:
                f.write("    {%s},\n" % ", ".join(str(v) for v in col))
            f.write("};\n\n")
        f.write("#endif\n")


def write_probes(out_dir, slug, probes, source_path, areas):
    path = os.path.join(out_dir, slug + "_probes.h")
    guard = slug.upper() + "_PROBES_H"
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write(
            "// golden probes derived directly from the bible json's terrain runs and blocks; the host\n"
        )
        f.write("// test reads these instead of hand-duplicating level-1-1.json's positions\n")
        f.write("\n#include <cstdint>\n\n")
        f.write("struct LevelProbe {\n    uint16_t column;\n    uint8_t row;\n    uint8_t kind;\n};\n\n")
        f.write("inline constexpr LevelProbe k%sProbes[] = {\n" % to_camel(slug))
        for column, row, kind in probes:
            f.write("    {%d, %d, %d}, // %s\n" % (column, row, kind, KIND_NAMES[kind]))
        f.write("};\n\n")
        f.write(
            "inline constexpr uint16_t k%sProbeCount = sizeof(k%sProbes) / sizeof(k%sProbes[0]);\n\n"
            % (to_camel(slug), to_camel(slug), to_camel(slug))
        )
        for index, area in enumerate(areas):
            camel = "%sArea%d" % (to_camel(slug), index)
            f.write("inline constexpr LevelProbe k%sProbes[] = {\n" % camel)
            for column, row, kind in area["probes"]:
                f.write("    {%d, %d, %d}, // %s\n" % (column, row, kind, KIND_NAMES[kind]))
            f.write("};\n\n")
            f.write(
                "inline constexpr uint16_t k%sProbeCount = sizeof(k%sProbes) / sizeof(k%sProbes[0]);\n\n"
                % (camel, camel, camel)
            )
        f.write("#endif\n")


def to_camel(slug):
    return "".join(part.capitalize() for part in slug.split("_"))


def main():
    if len(sys.argv) != 3:
        print("usage: compile_level.py <bible.json> <out_dir>", file=sys.stderr)
        return 1

    in_path, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    bible = load_bible(in_path)
    slug = slug_for(bible["level"])
    grid, length_columns, probes, flag_col = compile_grid(bible)
    start = (bible["start"]["x"], bible["start"]["y"])
    blocks = compile_block_list(bible)
    enemies = compile_enemy_list(bible)
    entry = find_pipe_entry(bible)

    areas = []
    for area in bible.get("areas", []):
        compiled = compile_area(area)
        # the bible's exit prose for 1-1 is "same pipe, returns to overworld near entry", so the
        # return column is the entry pipe's own; a bible that names another column overrides it here
        compiled["return_column"] = area.get("entry_x", 0 if entry is None else entry[0])
        compiled["return_top_row"] = 0 if entry is None else entry[1]
        areas.append(compiled)

    write_header(out_dir, slug, length_columns, start, flag_col, in_path, blocks, enemies, entry, areas)
    write_source(out_dir, slug, grid, in_path)
    write_objects(out_dir, slug, blocks, enemies, areas, in_path)
    write_areas(out_dir, slug, areas, in_path)
    write_grid(out_dir, slug, grid, in_path, blocks, enemies, areas)
    write_probes(out_dir, slug, probes, in_path, areas)

    print(
        "compiled %s: %d columns, %d probes, %d blocks, %d enemies, %d areas, flag column %s -> %s"
        % (bible["level"], length_columns, len(probes), len(blocks), len(enemies), len(areas), flag_col,
           out_dir)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
