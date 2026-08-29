#!/usr/bin/env python3
"""compiles a games/mario/data/*.json level bible into banked c terrain data plus a host-test
probe header. run as: compile_level.py <bible.json> <out_dir>

the block-kind numbers below are a contract with games/mario/src/mario.h's kBlock* defines and
must be kept numerically identical by hand; nothing here reads mario.h.
"""

import json
import os
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
}

# schema.md: 15 block rows, ground surface at row 13, rows 13-14 are the solid ground blocks
LEVEL_ROWS = 15
GROUND_ROW = 13

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
    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        max_x = max(max_x, flag["x"])
    return max_x


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
    # provisional placeholder. the bible's flagpole column can coincide with the end-of-level
    # staircase's own last column (both are placeholder-ish end-of-level positions), so the pole
    # only claims cells still empty - it never overwrites the more-sourced stairs/ground beneath it
    for row in range(FLAG_POLE_TOP_ROW, FLAG_POLE_BOTTOM_ROW + 1):
        if grid[x][row] == BLOCK_EMPTY:
            grid[x][row] = BLOCK_FLAG_POLE


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

    flag = bible.get("flag") or {}
    if flag.get("x") is not None:
        apply_flag(grid, flag["x"])
        probes.append((flag["x"], FLAG_POLE_TOP_ROW, BLOCK_FLAG_POLE))

    # level-1-1.json has no castle_end: per the milestone's instructions, an unpositioned kind is
    # left out rather than invented; BLOCK_CASTLE still exists in the enum/art for levels that do

    return grid, length_columns, probes


def slug_for(level_id):
    return "level_" + level_id.replace("-", "_")


def write_header(out_dir, slug, length_columns, start, source_path):
    guard = slug.upper() + "_H"
    path = os.path.join(out_dir, slug + ".h")
    with open(path, "w", encoding="utf-8") as f:
        f.write("#ifndef %s\n" % guard)
        f.write("#define %s\n" % guard)
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("\n#include <stdint.h>\n\n")
        f.write("#define %s_ROWS %dU\n" % (slug.upper(), LEVEL_ROWS))
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
        f.write("#define %s_BANK 1U\n\n" % slug.upper())
        f.write(
            "extern const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROWS];\n\n"
            % (slug, slug.upper(), slug.upper())
        )
        f.write("#endif\n")


def write_source(out_dir, slug, grid, source_path):
    path = os.path.join(out_dir, slug + ".c")
    with open(path, "w", encoding="utf-8") as f:
        f.write("// generated by games/mario/tools/compile_level.py from %s - do not edit\n" % source_path)
        f.write("// column-major block grid; kind values are the kBlock* contract in games/mario/src/mario.h\n")
        f.write("#pragma bank 1\n\n")
        f.write('#include "%s.h"\n\n' % slug)
        f.write(
            "const uint8_t %s_blocks[%s_LENGTH_COLUMNS][%s_ROWS] = {\n"
            % (slug, slug.upper(), slug.upper())
        )
        for col in grid:
            row_bytes = ", ".join(str(v) for v in col)
            names = "/".join(KIND_NAMES[v] for v in col if v != BLOCK_EMPTY)
            comment = (" // %s" % names) if names else ""
            f.write("    {%s},%s\n" % (row_bytes, comment))
        f.write("};\n")


def write_probes(out_dir, slug, probes, source_path):
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
    grid, length_columns, probes = compile_grid(bible)
    start = (bible["start"]["x"], bible["start"]["y"])

    write_header(out_dir, slug, length_columns, start, in_path)
    write_source(out_dir, slug, grid, in_path)
    write_probes(out_dir, slug, probes, in_path)

    print(
        "compiled %s: %d columns, %d probes -> %s"
        % (bible["level"], length_columns, len(probes), out_dir)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
