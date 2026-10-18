#!/usr/bin/env python3
# Regie5 — makes assets/fonts/NotoSansCJK-Subset.otf, the fallback for the
# characters Inter has not (Japanese, Chinese, Korean titles and names): a
# subset of Noto Sans CJK JP Regular (SIL Open Font License 1.1) with the
# kana, the CJK punctuation and full-width forms, the ideographs of GB 2312
# (6 763, which hold the Japanese jōyō kanji too), those of JIS X 0208 level 1,
# the traditional ones of Big5 level 1, and the 2 350 hangul syllables of
# KS X 1001: about 12 400 glyphs, 2.3 MiB. Nothing is downloaded: the font
# comes from the system (Debian / Ubuntu: fonts-noto-cjk), the subsetting from
# HarfBuzz's library (libharfbuzz-subset0) through ctypes, the character sets
# from Python's own codecs.
#   scripts/make-cjk-font.py [source.ttc] [--out file] [--no-trad]
#   --no-trad: without Big5's traditional ideographs (2.0 MiB)
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import ctypes
import ctypes.util
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
FACE = 0   # Noto Sans CJK JP (the TTC's first face)


def table(codec, rows, cols=range(0xA1, 0xFF)):
    """The characters a double-byte charset holds in those rows."""
    out = set()
    for r in rows:
        for c in cols:
            try:
                ch = bytes([r, c]).decode(codec)
            except UnicodeDecodeError:
                continue
            if len(ch) == 1:
                out.add(ord(ch))
    return out


def characters(trad):
    cps = set()
    for lo, hi in [(0x3000, 0x303F), (0x3040, 0x309F), (0x30A0, 0x30FF), (0x31F0, 0x31FF), (0x3130, 0x318F),
                   (0xFF00, 0xFFEF), (0x3200, 0x32FF)]:
        cps.update(range(lo, hi + 1))
    cps |= table("gb2312", range(0xB0, 0xF8))   # 6 763 hanzi
    cps |= table("euc_jp", range(0xB0, 0xD0))   # JIS X 0208 level 1 kanji
    cps |= table("euc_kr", range(0xB0, 0xC9))   # 2 350 hangul
    if trad:
        cps |= table("big5", range(0xA4, 0xC7), list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)))
    return cps


def load(name):
    path = ctypes.util.find_library(name) or "lib%s.so.0" % name
    return ctypes.CDLL(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", nargs="?", default=SOURCE)
    ap.add_argument("--out", default=os.path.join(ROOT, "assets", "fonts", "NotoSansCJK-Subset.otf"))
    ap.add_argument("--no-trad", action="store_true")
    a = ap.parse_args()
    hb, sub = load("harfbuzz"), load("harfbuzz-subset")
    vp = ctypes.c_void_p
    hb.hb_blob_create_from_file_or_fail.restype = vp
    hb.hb_blob_create_from_file_or_fail.argtypes = [ctypes.c_char_p]
    hb.hb_face_create.restype = vp
    hb.hb_face_create.argtypes = [vp, ctypes.c_uint]
    hb.hb_set_add.argtypes = [vp, ctypes.c_uint32]
    hb.hb_face_reference_blob.restype = vp
    hb.hb_face_reference_blob.argtypes = [vp]
    hb.hb_blob_get_data.restype = ctypes.POINTER(ctypes.c_char)
    hb.hb_blob_get_data.argtypes = [vp, ctypes.POINTER(ctypes.c_uint)]
    sub.hb_subset_input_create_or_fail.restype = vp
    sub.hb_subset_input_unicode_set.restype = vp
    sub.hb_subset_input_unicode_set.argtypes = [vp]
    sub.hb_subset_input_set.restype = vp
    sub.hb_subset_input_set.argtypes = [vp, ctypes.c_int]
    sub.hb_subset_input_set_flags.argtypes = [vp, ctypes.c_uint]
    sub.hb_subset_or_fail.restype = vp
    sub.hb_subset_or_fail.argtypes = [vp, vp]

    blob = hb.hb_blob_create_from_file_or_fail(a.source.encode())
    if not blob:
        sys.exit("cannot read %s (Debian / Ubuntu: apt install fonts-noto-cjk)" % a.source)
    face = hb.hb_face_create(blob, FACE)
    inp = sub.hb_subset_input_create_or_fail()
    uni = sub.hb_subset_input_unicode_set(inp)
    cps = characters(not a.no_trad)
    for cp in sorted(cps):
        hb.hb_set_add(uni, cp)
    # Régie5 draws through FreeType alone, without shaping: the layout tables
    # (and the variant glyphs they would keep: vertical forms, other regions'
    # shapes) and the hints are left out.
    drop = sub.hb_subset_input_set(inp, 3)   # HB_SUBSET_SETS_DROP_TABLE_TAG
    for tag in ["GSUB", "GPOS", "GDEF", "BASE", "JSTF", "MATH", "VORG", "vhea", "vmtx", "DSIG"]:
        hb.hb_set_add(drop, int.from_bytes(tag.encode(), "big"))
    sub.hb_subset_input_set_flags(inp, 0x1)   # HB_SUBSET_FLAGS_NO_HINTING
    out = sub.hb_subset_or_fail(face, inp)
    if not out:
        sys.exit("subsetting failed")
    data_blob = hb.hb_face_reference_blob(out)
    n = ctypes.c_uint(0)
    data = hb.hb_blob_get_data(data_blob, ctypes.byref(n))
    payload = ctypes.string_at(data, n.value)
    with open(a.out, "wb") as f:
        f.write(payload)
    print("%s: %d characters asked, %.2f MiB" % (os.path.relpath(a.out, ROOT), len(cps), len(payload) / 1048576))


if __name__ == "__main__":
    main()
