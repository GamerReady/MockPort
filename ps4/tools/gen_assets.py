#!/usr/bin/env python3
"""Generate the sce_sys assets for the MockPort PS4 package (stdlib only).

Writes into <out>/sce_sys:
  param.sfo   - PS4 package metadata (title, title id, content id, ...)
  icon0.png   - 512x512 package icon
  pic0.png    - 1920x1080 background shown on the game's info screen

Usage: gen_assets.py <out_dir> [--title T] [--title-id ID] [--content-id CID]
"""
import argparse
import os
import struct
import sys
import zlib

DEFAULT_TITLE = "MockPort"
DEFAULT_TITLE_ID = "MOCK00001"
DEFAULT_CONTENT_ID = "IV0000-MOCK00001_00-MOCKPORT00000000"
DEFAULT_VERSION = "01.00"

FMT_UTF8_SPECIAL = 0x0004  # not NUL terminated
FMT_UTF8 = 0x0204          # NUL terminated
FMT_INT32 = 0x0404


def build_sfo(entries):
    """entries: list of (key, fmt, value, max_len); sorted by key as the format requires."""
    entries = sorted(entries, key=lambda e: e[0])
    key_table = b""
    data_table = b""
    index = b""
    for key, fmt, value, max_len in entries:
        key_off = len(key_table)
        key_table += key.encode("ascii") + b"\0"
        data_off = len(data_table)
        if fmt == FMT_INT32:
            raw = struct.pack("<I", value)
            used = 4
        else:
            raw = value.encode("utf-8")
            if fmt == FMT_UTF8:
                raw += b"\0"
            used = len(raw)
        if len(raw) > max_len:
            raise ValueError(f"{key}: value longer than max_len {max_len}")
        data_table += raw + b"\0" * (max_len - len(raw))
        index += struct.pack("<HHIII", key_off, fmt, used, max_len, data_off)
    # Key table is padded to a multiple of 4 bytes.
    key_table += b"\0" * ((-len(key_table)) % 4)
    header_len = 20
    key_table_off = header_len + len(index)
    data_table_off = key_table_off + len(key_table)
    header = struct.pack("<4sIIII", b"\0PSF", 0x101, key_table_off,
                         data_table_off, len(entries))
    return header + index + key_table + data_table


def make_sfo(title, title_id, content_id, version):
    return build_sfo([
        ("APP_TYPE", FMT_INT32, 0, 4),
        ("APP_VER", FMT_UTF8, version, 8),
        ("ATTRIBUTE", FMT_INT32, 0, 4),
        ("CATEGORY", FMT_UTF8, "gd", 4),
        ("CONTENT_ID", FMT_UTF8, content_id, 48),
        ("DOWNLOAD_DATA_SIZE", FMT_INT32, 0, 4),
        ("FORMAT", FMT_UTF8, "obs", 4),
        ("PARENTAL_LEVEL", FMT_INT32, 1, 4),
        ("SYSTEM_VER", FMT_INT32, 0, 4),
        ("TITLE", FMT_UTF8, title, 128),
        ("TITLE_ID", FMT_UTF8, title_id, 12),
        ("VERSION", FMT_UTF8, version, 8),
    ])


def png_chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, width, height, pixel):
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # filter type 0
        for x in range(width):
            rows += bytes(pixel(x, y))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    data = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
            + png_chunk(b"IDAT", zlib.compress(bytes(rows), 9))
            + png_chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def icon_pixel(size):
    cx = cy = size / 2
    ring_out, ring_in = size * 0.38, size * 0.30

    def px(x, y):
        # Diagonal blue->purple gradient with a white ring and a centre dot.
        t = (x + y) / (2.0 * size)
        r, g, b = int(30 + 90 * t), int(60 + 20 * t), int(160 + 70 * t)
        d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
        if ring_in <= d <= ring_out or d <= size * 0.07:
            return (245, 245, 250)
        return (r, g, b)
    return px


def pic_pixel(w, h):
    def px(x, y):
        t = y / float(h)
        return (int(12 + 18 * t), int(14 + 22 * t), int(40 + 60 * t))
    return px


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--title", default=DEFAULT_TITLE)
    ap.add_argument("--title-id", default=DEFAULT_TITLE_ID)
    ap.add_argument("--content-id", default=DEFAULT_CONTENT_ID)
    ap.add_argument("--version", default=DEFAULT_VERSION)
    a = ap.parse_args()

    if len(a.content_id) != 36:
        sys.exit("content-id must be 36 characters (XXNNNN-TTTTTTTTT_00-LLLLLLLLLLLLLLLL)")
    if len(a.title_id) != 9:
        sys.exit("title-id must be 9 characters (e.g. MOCK00001)")

    sce = os.path.join(a.out_dir, "sce_sys")
    os.makedirs(sce, exist_ok=True)
    with open(os.path.join(sce, "param.sfo"), "wb") as f:
        f.write(make_sfo(a.title, a.title_id, a.content_id, a.version))
    write_png(os.path.join(sce, "icon0.png"), 512, 512, icon_pixel(512))
    write_png(os.path.join(sce, "pic0.png"), 1920, 1080, pic_pixel(1920, 1080))
    print(f"assets written to {sce}")


if __name__ == "__main__":
    main()
