"""per-bank rom usage from sdld's wide .map, and a hard error where lcc only warns.

sdldgb prints "Possible overflow from Bank N" and links anyway, dropping the tail of the bank; the
symptom is dozens of unrelated test failures. this runs after the link, prints what each bank
holds, and fails the build on any bank past its 16 KB window or under --min-free.

usage: bank_report.py mario.map [--banks 16] [--min-free 0] [--symbols N]
"""

import argparse
import re
import sys
from collections import defaultdict

BANK_BYTES = 0x4000
WRAM_END = 0xE000
# rom areas only; everything at or past 0xC000 in the home window is ram
RAM_FLOOR = 0xC000

AREA_RE = re.compile(r"^(\S+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s+=\s+(\d+)\. bytes \(([A-Z,]+)\)")
SYMBOL_RE = re.compile(r"^\s+([0-9A-F]{8})\s+(\S+)(?:\s+(\S+))?\s*$")


def parse(path):
    areas = {}
    current = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = AREA_RE.match(line)
            if m:
                name, addr, size, _, attrs = m.groups()
                current = areas.setdefault(name, {"addr": int(addr, 16), "size": int(size, 16),
                                                  "attrs": attrs, "symbols": {}})
                continue
            m = SYMBOL_RE.match(line)
            if m and current is not None and not m.group(2).startswith("-"):
                value, name, module = m.groups()
                current["symbols"].setdefault(name, (int(value, 16), module or "?"))
    return areas


def rom_bank(area):
    addr = area["addr"]
    low = addr & 0xFFFF
    if area["size"] == 0 or low >= 0x8000:
        return None
    if low >= RAM_FLOOR:
        return None
    return addr >> 16 if low >= BANK_BYTES else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("map")
    ap.add_argument("--banks", type=int, default=16)
    ap.add_argument("--min-free", type=int, default=0, help="fail a bank under this many free bytes")
    ap.add_argument("--symbols", type=int, default=0,
                    help="list a bank's biggest global symbols; statics fold into the global before them")
    args = ap.parse_args()

    areas = parse(args.map)
    ends = defaultdict(int)
    symbols_by_bank = defaultdict(list)
    truncated = []
    for name, area in areas.items():
        bank = rom_bank(area)
        if bank is None:
            continue
        low = area["addr"] & 0xFFFF
        window = 0 if bank == 0 else BANK_BYTES
        ends[bank] = max(ends[bank], low + area["size"] - window)
        if area["size"] > BANK_BYTES:
            truncated.append(name)
        symbols = sorted(area["symbols"].items(), key=lambda kv: kv[1][0])
        for i, (symbol, (value, module)) in enumerate(symbols):
            nxt = symbols[i + 1][1][0] if i + 1 < len(symbols) else area["addr"] + area["size"]
            symbols_by_bank[bank].append((nxt - value, symbol, module))

    failed = False
    print("bank   used   free")
    for bank in range(args.banks):
        used = ends.get(bank, 0)
        free = BANK_BYTES - used
        flag = ""
        if used > BANK_BYTES:
            flag = "  OVERFLOW"
            failed = True
        elif free < args.min_free:
            flag = "  UNDER MIN FREE"
            failed = True
        print(f"{bank:4d}  {used:5d}  {free:5d}{flag}")
        for size, symbol, module in sorted(symbols_by_bank[bank], reverse=True)[:args.symbols]:
            print(f"          {size:5d}  {symbol} ({module})")
    for bank in sorted(b for b in ends if b >= args.banks):
        print(f"{bank:4d}  {ends[bank]:5d}  beyond --banks")
        failed = True

    wram_end = max([a["addr"] + a["size"] for a in areas.values() if RAM_FLOOR <= a["addr"] < WRAM_END] + [RAM_FLOOR])
    print(f"wram  {wram_end - RAM_FLOOR:5d}  {WRAM_END - wram_end:5d}")
    for name in truncated:
        print(f"area {name} is longer than a bank; the linker dropped its tail")
        failed = True
    if failed:
        print("bank budget exceeded", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
