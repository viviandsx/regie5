#!/usr/bin/env python3
# Regie5 — a small BC7 encoder for the PS5 tile backgrounds (pic0.dds,
# pic1.dds: 3840 x 2160, BC7_UNORM, DX10 header, no mipmaps), so they are
# made on Linux without texconv (Windows only). Mode 6 only (one line of
# 16 colours per 4 x 4 block, 7-bit endpoints and a p-bit each): right for
# soft lights and gradients, which is what the backgrounds are. Each block's
# line follows its colours' main axis; the endpoints and indices are then
# chosen against the decoded palette. The result is read back with Pillow's
# own BC7 decoder (independent of this code) and compared with the source.
#   scripts/bc7.py <in.png> <out.dds>     writes a new file, prints the PSNR
# Needs numpy and Pillow.
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import struct
import sys

import numpy as np
from PIL import Image

WEIGHTS = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], dtype=np.int32)
DXGI_BC7_UNORM = 98


def _quantize(v):
    """8-bit endpoint colours (n, 4) -> 7-bit values and one p-bit, the pair that decodes closest."""
    best = None
    for p in (0, 1):
        q = np.clip(np.rint((v - p) / 2.0), 0, 127).astype(np.int32)
        err = ((q * 2 + p - v) ** 2).sum(axis=1)
        if best is None:
            best = (q, np.full(len(v), p, np.int32), err)
        else:
            take = err < best[2]
            best = (np.where(take[:, None], q, best[0]), np.where(take, p, best[1]), np.where(take, err, best[2]))
    return best[0], best[1]


def _encode_blocks(px):
    """px: (n, 16, 4) uint8 RGBA, one row per block -> (n, 16) bytes of BC7 mode 6."""
    n = len(px)
    f = px.astype(np.float64)
    mean = f.mean(axis=1)
    c = f - mean[:, None, :]
    cov = np.einsum("nki,nkj->nij", c, c)
    # main axis by a few power iterations; a flat block keeps the grey axis
    axis = np.tile(np.array([0.5, 0.5, 0.5, 0.5]), (n, 1))
    for _ in range(8):
        axis = np.einsum("nij,nj->ni", cov, axis)
        norm = np.linalg.norm(axis, axis=1, keepdims=True)
        axis = np.where(norm > 1e-9, axis / np.maximum(norm, 1e-9), np.array([0.5, 0.5, 0.5, 0.5]))
    t = np.einsum("nki,ni->nk", c, axis)
    e0 = np.clip(mean + t.min(axis=1)[:, None] * axis, 0, 255)
    e1 = np.clip(mean + t.max(axis=1)[:, None] * axis, 0, 255)
    q0, p0 = _quantize(e0)
    q1, p1 = _quantize(e1)
    d0 = (q0 << 1) | p0[:, None]
    d1 = (q1 << 1) | p1[:, None]
    # the 16 colours the console will see, and each pixel's nearest
    pal = ((64 - WEIGHTS)[None, :, None] * d0[:, None, :] + WEIGHTS[None, :, None] * d1[:, None, :] + 32) >> 6
    dist = ((px.astype(np.int32)[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(axis=3)
    idx = dist.argmin(axis=2).astype(np.int32)
    # the first pixel's index has an implicit 0 on top: swap the ends if needed
    flip = idx[:, 0] >= 8
    q0, q1 = np.where(flip[:, None], q1, q0), np.where(flip[:, None], q0, q1)
    p0, p1 = np.where(flip, p1, p0), np.where(flip, p0, p1)
    idx = np.where(flip[:, None], 15 - idx, idx)
    # pack 128 bits, low bit first: mode 6 (bit 6), R0 R1 G0 G1 B0 B1 A0 A1 (7 bits
    # each), P0, P1, then the indices (3 bits for the first, 4 for the others)
    lo = np.full(n, 1 << 6, dtype=np.uint64)
    hi = np.zeros(n, dtype=np.uint64)
    pos = 7

    def put(value, bits):
        nonlocal pos, lo, hi
        v = value.astype(np.uint64)
        if pos >= 64:
            hi |= v << np.uint64(pos - 64)
        elif pos + bits <= 64:
            lo |= v << np.uint64(pos)
        else:
            low_bits = 64 - pos
            lo |= (v & np.uint64((1 << low_bits) - 1)) << np.uint64(pos)
            hi |= v >> np.uint64(low_bits)
        pos += bits

    for ch in range(4):
        put(q0[:, ch], 7)
        put(q1[:, ch], 7)
    put(p0, 1)
    put(p1, 1)
    put(idx[:, 0], 3)
    for k in range(1, 16):
        put(idx[:, k], 4)
    assert pos == 128
    out = np.empty((n, 2), dtype="<u8")
    out[:, 0], out[:, 1] = lo, hi
    return out.view(np.uint8).reshape(n, 16)


def encode(img):
    """A PIL image whose sides are multiples of 4 -> BC7 blocks, row by row."""
    a = np.asarray(img.convert("RGBA"), dtype=np.uint8)
    h, w = a.shape[:2]
    assert w % 4 == 0 and h % 4 == 0, "sides must be multiples of 4"
    blocks = a.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    parts = [_encode_blocks(blocks[i:i + 16384]) for i in range(0, len(blocks), 16384)]
    return np.concatenate(parts).tobytes()


def dds_bytes(width, height, data):
    """A DDS file: the classic header, the DX10 one (BC7_UNORM, 2D, one image, one level), the blocks."""
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000   # caps height width pixelformat mipmapcount linearsize
    head = struct.pack("<4sIIIIIII", b"DDS ", 124, flags, height, width, len(data), 0, 1)
    head += b"\0" * 44                                         # reserved
    head += struct.pack("<II4s5I", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)   # pixel format: a FourCC
    head += struct.pack("<5I", 0x1000, 0, 0, 0, 0)              # caps: a texture; reserved
    head += struct.pack("<5I", DXGI_BC7_UNORM, 3, 0, 1, 0)      # DX10: format, 2D, flags, array 1
    assert len(head) == 148
    return head + data


def write_dds(img, path):
    """Writes img as BC7 to path (a new file), reads it back with Pillow; returns the PSNR in dB."""
    if os.path.exists(path):
        raise SystemExit(f"{path} exists: not written over")
    data = dds_bytes(img.width, img.height, encode(img))
    with open(path, "xb") as f:
        f.write(data)
    back = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    src = np.asarray(img.convert("RGB"), dtype=np.float64)
    mse = ((back - src) ** 2).mean()
    return float("inf") if mse == 0 else 10 * np.log10(255.0 ** 2 / mse)


def main():
    if len(sys.argv) != 3:
        print("usage: scripts/bc7.py <in.png> <out.dds>", file=sys.stderr)
        return 2
    psnr = write_dds(Image.open(sys.argv[1]), sys.argv[2])
    print(f"{sys.argv[2]}: BC7, PSNR {psnr:.1f} dB against {sys.argv[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
