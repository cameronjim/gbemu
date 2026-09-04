#!/usr/bin/env python3
"""the png codec and 2bpp tile codec the mario art pipeline is built on. imported by
png2tiles.py and tiles2png.py; not run on its own.

stdlib only on purpose: ci has no pillow, so the pipeline has to carry its own reader and
writer. 8-bit non-interlaced pngs only - grayscale, rgb, rgba and indexed.
"""

import struct
import zlib

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"

# the four greys tiles2png writes, one per 2bpp value, so an index png's index IS its pixel value
GREY_PALETTE = [(0x00, 0x00, 0x00), (0x55, 0x55, 0x55), (0xAA, 0xAA, 0xAA), (0xFF, 0xFF, 0xFF)]

CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


class PngError(Exception):
    pass


class Image:
    """width x height of pixels. mode "P" rows hold palette indices, every other mode holds
    (r, g, b, a) tuples."""

    def __init__(self, width, height, mode, rows, palette=None):
        self.width = width
        self.height = height
        self.mode = mode
        self.rows = rows
        self.palette = palette or []

    def pixel(self, x, y):
        return self.rows[y][x]

    def rgba(self, x, y):
        if self.mode == "P":
            index = self.rows[y][x]
            if index >= len(self.palette):
                raise PngError("palette index %d out of range" % index)
            r, g, b = self.palette[index]
            return (r, g, b, 255)
        return self.rows[y][x]


def _chunks(data):
    if data[:8] != PNG_MAGIC:
        raise PngError("not a png")
    pos = 8
    while pos + 8 <= len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        yield kind, body
        pos += 12 + length


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter(raw, width, height, bpp):
    stride = width * bpp
    out = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        filt = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if filt == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                upleft = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(left, prev[i], upleft)) & 0xFF
        elif filt != 0:
            raise PngError("unknown png filter %d" % filt)
        out.append(line)
        prev = line
    return out


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    width = height = depth = color = None
    palette = []
    trns = b""
    idat = bytearray()
    for kind, body in _chunks(data):
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8:
                raise PngError("only 8-bit pngs are supported (got %d)" % depth)
            if interlace:
                raise PngError("interlaced pngs are not supported")
            if color not in CHANNELS:
                raise PngError("unsupported png color type %d" % color)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if width is None:
        raise PngError("png has no IHDR")
    bpp = CHANNELS[color]
    lines = _unfilter(zlib.decompress(bytes(idat)), width, height, bpp)

    if color == 3:
        rows = [list(line) for line in lines]
        return Image(width, height, "P", rows, palette)

    rows = []
    for line in lines:
        row = []
        for x in range(width):
            px = line[x * bpp:(x + 1) * bpp]
            if color == 0:
                row.append((px[0], px[0], px[0], 255))
            elif color == 4:
                row.append((px[0], px[0], px[0], px[1]))
            elif color == 2:
                row.append((px[0], px[1], px[2], 255))
            else:
                row.append((px[0], px[1], px[2], px[3]))
        rows.append(row)
    mode = "RGBA" if color in (4, 6) or trns else "RGB"
    return Image(width, height, mode, rows)


def _chunk(kind, body):
    out = struct.pack(">I", len(body)) + kind + body
    return out + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def write_png(path, width, height, rows, mode="RGB", palette=None):
    """rows is height lists of width entries: palette indices for mode "P", (r, g, b) tuples
    otherwise."""
    if mode == "P":
        color, raw = 3, bytearray()
        for row in rows:
            raw.append(0)
            raw += bytes(row)
    elif mode == "RGB":
        color, raw = 2, bytearray()
        for row in rows:
            raw.append(0)
            for px in row:
                raw += bytes(px[:3])
    else:
        raise PngError("write_png supports RGB and P only")
    body = PNG_MAGIC
    body += _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color, 0, 0, 0))
    if color == 3:
        flat = bytearray()
        for entry in (palette or GREY_PALETTE):
            flat += bytes(entry[:3])
        body += _chunk(b"PLTE", bytes(flat))
    body += _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    body += _chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(body)


def rgb555(r, g, b):
    # cgb drops the low three bits of each channel; the ripped pngs are written back out as
    # v << 3, so this round-trips them exactly
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def rgb555_to_rgb888(value):
    return (((value >> 0) & 0x1F) << 3, ((value >> 5) & 0x1F) << 3, ((value >> 10) & 0x1F) << 3)


def encode_tile(pixels):
    """pixels is 8 rows of 8 values in 0..3 -> the 16 planar bytes gb vram wants."""
    out = bytearray()
    for row in pixels:
        lo = hi = 0
        for value in row:
            # msb is the leftmost pixel; low plane is bit 0, high plane bit 1
            lo = ((lo << 1) | (value & 1)) & 0xFF
            hi = ((hi << 1) | ((value >> 1) & 1)) & 0xFF
        out.append(lo)
        out.append(hi)
    return bytes(out)


def decode_tile(data):
    """the inverse of encode_tile: 16 planar bytes -> 8 rows of 8 values."""
    rows = []
    for y in range(8):
        lo, hi = data[y * 2], data[y * 2 + 1]
        rows.append([((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1) for x in range(8)])
    return rows
