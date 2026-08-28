#!/usr/bin/env python3
# the tetris rom ships cryptic glyphs for the piece editor's spin/wobble state;
# swap them for a check mark (spin) and a bold x (wobble). works on any
# unpatched copy: finds the original tile bitmaps and replaces them in place,
# leaving a .bak copy next to the rom. tile data lives outside the header,
# so the cartridge header checksum is unaffected.
import sys

SPIN_OLD = bytes.fromhex("000007020D071B0EF65CACF8D8707020")
WOBBLE_OLD = bytes.fromhex("E7C3BDE7DB7E663C663CDB7EBDE7E7C3")


def tile_2bpp(rows):
    # solid white glyph: both bitplanes carry the same pattern
    out = bytearray()
    for r in rows:
        out += bytes((r, r))
    return bytes(out)


SPIN_NEW = tile_2bpp([0x00, 0x01, 0x03, 0x86, 0xCC, 0x78, 0x30, 0x00])  # check mark
WOBBLE_NEW = tile_2bpp([0xC3, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0xC3, 0x00])  # bold x


def main():
    if len(sys.argv) != 2:
        print("usage: patch_spin_icons.py <tetris rom>")
        return 1
    path = sys.argv[1]
    rom = open(path, "rb").read()
    for name, old in (("spin", SPIN_OLD), ("wobble", WOBBLE_OLD)):
        n = rom.count(old)
        if n != 1:
            print(f"{name} glyph found {n} times, expected 1 - wrong or already patched rom")
            return 1
    open(path + ".bak", "wb").write(rom)
    rom = rom.replace(SPIN_OLD, SPIN_NEW).replace(WOBBLE_OLD, WOBBLE_NEW)
    open(path, "wb").write(rom)
    print(f"patched {path} (backup at {path}.bak)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
