#!/usr/bin/env python3
"""transcribes smb1's sound engine data out of smbdis.asm into gen/smb_audio_data.h.

the music is smb's own byte streams (square 2, square 1, triangle, noise sections per header),
kept byte for byte so sound.c can walk them the way MusicHandler does. the frequency table is the
one thing converted: nes periods become game boy period values (pandocs: square f = 131072/(2048-x),
wave f = 65536/(2048-x); nesdev: square f = 1789773/(16(T+1)), triangle f = 1789773/(32(T+1)) -
the octave between square and triangle is the same octave between ch2 and ch3, so one table serves
both). the disassembly is not committed; run this by hand and commit the header.

usage: extract_smb_audio.py <smbdis.asm> <out.h>, then clang-format -i the header before committing
"""

import re
import sys

NES_CLOCK = 1789773.0
GB_SQUARE_CLOCK = 131072.0
# nesdev: noise timer periods for the 16 ntsc period indexes
NES_NOISE_PERIODS = [4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068]

LABEL_RE = re.compile(r"^(\w+):")
DB_RE = re.compile(r"^\s*\.db\s+(.*)$")


def strip_comment(line):
    return line.split(";", 1)[0].rstrip()


def parse_region(lines, start_label, end_label):
    """bytes and label offsets from start_label up to (not including) end_label."""
    blob = []
    labels = {}
    active = False
    for raw in lines:
        line = strip_comment(raw)
        m = LABEL_RE.match(line)
        if m:
            name = m.group(1)
            if name == start_label:
                active = True
            elif name == end_label and active:
                break
            if active:
                labels[name] = len(blob)
            line = line[m.end():]
        if not active:
            continue
        m = DB_RE.match(line)
        if not m:
            continue
        for tok in m.group(1).split(","):
            tok = tok.strip()
            if not tok:
                continue
            if tok.startswith("$"):
                blob.append(int(tok[1:], 16))
            else:
                raise ValueError("unexpected token %r in region %s" % (tok, start_label))
    return blob, labels


def parse_headers(lines, blob_labels):
    """the music headers, keyed by label, with the data address turned into a blob offset."""
    headers = {}
    for raw in lines:
        line = strip_comment(raw)
        m = re.match(r"^(\w+):\s*\.db\s+(.*)$", line)
        if not m or not (m.group(1).endswith("Hdr") or m.group(1) == "ResidualHeaderData"):
            continue
        name = m.group(1)
        toks = [t.strip() for t in m.group(2).split(",")]
        out = []
        for tok in toks:
            if tok.startswith("$"):
                out.append(int(tok[1:], 16))
            elif tok.startswith("<"):
                out.append(blob_labels[tok[1:]] & 0xFF)
            elif tok.startswith(">"):
                out.append(blob_labels[tok[1:]] >> 8)
            else:
                raise ValueError("bad header token %r" % tok)
        # secondary music headers carry no noise offset; the engine never reads it for them
        while len(out) < 6:
            out.append(0)
        headers[name] = out
    return headers


def parse_header_index(lines):
    """MusicHeaderData's list of which header each queue bit and ground-music step selects."""
    names = []
    active = False
    for raw in lines:
        line = strip_comment(raw)
        if line.startswith("MusicHeaderData:"):
            active = True
            continue
        if not active:
            continue
        m = DB_RE.match(line)
        if not m:
            if names and line.strip() == "":
                continue
            if names:
                break
            continue
        for tok in m.group(1).split(","):
            tok = tok.strip()
            if tok:
                assert tok.endswith("-MHD"), tok
                names.append(tok[:-4])
    return names


def gb_period(nes_t):
    f = NES_CLOCK / (16.0 * (nes_t + 1))
    return max(0, min(2047, 2048 - int(round(GB_SQUARE_CLOCK / f))))


def nr43_for(index):
    """nearest ch4 divisor/shift for a nes noise period index (pandocs: f = 524288 / r / 2^(s+1))."""
    f = NES_CLOCK / NES_NOISE_PERIODS[index]
    best = None
    for s in range(14):
        for r in range(8):
            divisor = 0.5 if r == 0 else float(r)
            fg = 524288.0 / divisor / (1 << (s + 1))
            ratio = fg / f if fg > f else f / fg
            if best is None or ratio < best[1]:
                best = ((s << 4) | r, ratio)
    return best[0]


def c_bytes(name, values, width=12):
    out = ["static const uint8_t %s[%d] = {" % (name, len(values))]
    for i in range(0, len(values), width):
        out.append("    " + ", ".join("0x%02X" % v for v in values[i:i + width]) + ",")
    out.append("};")
    return out


def main():
    src, out_path = sys.argv[1], sys.argv[2]
    lines = open(src, encoding="utf-8", errors="replace").read().split("\n")

    music, labels = parse_region(lines, "Star_CloudMData", "FreqRegLookupTbl")
    headers = parse_headers(lines, labels)
    index = parse_header_index(lines)
    assert len(index) == 49, len(index)
    freq, _ = parse_region(lines, "FreqRegLookupTbl", "MusicLengthLookupTbl")
    assert len(freq) == 102, len(freq)
    lengths, _ = parse_region(lines, "MusicLengthLookupTbl", "EndOfCastleMusicEnvData")
    assert len(lengths) == 48, len(lengths)

    def table(label, end):
        data, _ = parse_region(lines, label, end)
        return data

    swim_env = table("SwimStompEnvelopeData", "PlayFlagpoleSlide")
    extra_life = table("ExtraLifeFreqData", "PowerUpGrabFreqData")
    powerup_grab = table("PowerUpGrabFreqData", "PUp_VGrow_FreqData")
    vgrow = table("PUp_VGrow_FreqData", "PlayCoinGrab")
    brick_freq = table("BrickShatterFreqData", "PlayBrickShatter")
    brick_env = table("BrickShatterEnvData", "NonMaskableInterrupt")
    flame_env = table("BowserFlameEnvData", "BrickShatterEnvData")
    assert len(swim_env) == 14 and len(extra_life) == 6 and len(vgrow) == 32
    assert len(brick_freq) == 16 and len(brick_env) == 16 and len(flame_env) == 32

    header_names = [n for n in headers if n != "ResidualHeaderData"]
    header_slot = {n: i for i, n in enumerate(header_names)}

    periods = []
    for i in range(0, len(freq), 2):
        hi, lo = freq[i], freq[i + 1]
        # Dump_Freq_Regs: a zero low byte means rest, whatever the high byte says
        periods.append(0 if lo == 0 else gb_period((hi << 8) | lo))

    o = []
    o.append("// generated by games/mario/tools/extract_smb_audio.py from smbdis.asm - do not edit")
    o.append("// smb1's sound engine data, byte for byte; see the format comments above MusicHeaderData,")
    o.append("// Star_CloudMData and FreqRegLookupTbl in the disassembly")
    o.append("#ifndef SMB_AUDIO_DATA_H")
    o.append("#define SMB_AUDIO_DATA_H")
    o.append("")
    o.append("#include <stdint.h>")
    o.append("")
    o.append("// every song's square 2, square 1, triangle and noise streams; headers index into it")
    o += c_bytes("kMusicData", music)
    o.append("")
    o.append("// header: length table offset, data offset lo, hi, triangle offset, square 1 offset, noise offset")
    o.append("#define kMusicHeaderBytes 6U")
    flat = []
    for n in header_names:
        flat += headers[n]
    o += c_bytes("kMusicHeaders", flat, 6)
    for n in header_names:
        o.append("// %d %s" % (header_slot[n], n))
    o.append("")
    o.append("// MusicHeaderData: event bits 1-8, area bits 9-16, then the ground theme's 33-step layout")
    o += c_bytes("kMusicHeaderIndex", [header_slot[n] for n in index])
    o.append("#define kGroundLayoutFirst 0x11U")
    o.append("#define kGroundLayoutEnd 0x32U")
    o.append("")
    o.append("// FreqRegLookupTbl as game boy period values, indexed by note byte / 2; 0 is a rest")
    o.append("static const uint16_t kNotePeriod[%d] = {" % len(periods))
    for i in range(0, len(periods), 8):
        o.append("    " + ", ".join("%d" % v for v in periods[i:i + 8]) + ",")
    o.append("};")
    o.append("")
    # sfx that poke only the period's low byte mid-sound: the stomp (note $26 then lo $9e), the smack
    # (note $28 then lo $a0) and the coin (note $42 then lo $54); precomputed so the rom never converts
    def second(note, lo):
        hi = freq[note]
        return gb_period((hi << 8) | lo)
    o.append("// ContinueSwimStomp, ContinueSmackEnemy and ContinueCGrabTTick's low-byte retunes, converted")
    o.append("#define kStompSecondPeriod %dU" % second(0x26, 0x9E))
    o.append("#define kSmackSecondPeriod %dU" % second(0x28, 0xA0))
    o.append("#define kCoinSecondPeriod %dU" % second(0x42, 0x54))
    o.append("")
    o.append("// MusicLengthLookupTbl, in frames")
    o += c_bytes("kNoteLength", lengths)
    o.append("")
    o.append("// nr43 for each nes noise period index")
    o += c_bytes("kNoiseNr43", [nr43_for(i) for i in range(16)])
    o.append("")
    o.append("// sfx tables, as the handlers index them")
    o += c_bytes("kSwimStompEnv", swim_env)
    o += c_bytes("kExtraLifeFreq", extra_life)
    o += c_bytes("kPowerUpGrabFreq", powerup_grab[:27])
    o += c_bytes("kGrowFreq", vgrow)
    o += c_bytes("kBrickShatterFreq", brick_freq)
    o += c_bytes("kBrickShatterEnv", brick_env)
    o += c_bytes("kBowserFlameEnv", flame_env)
    o.append("")
    o.append("#endif")
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(o) + "\n")
    print("music %d bytes, %d headers, %d notes" % (len(music), len(header_names), len(periods)))
    for n in header_names:
        print("  %-22s %s" % (n, " ".join("%02x" % b for b in headers[n])))


if __name__ == "__main__":
    main()
