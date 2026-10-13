#!/usr/bin/env python3
# Regie5 — draws what the PS5 home screen shows for the app, from the logo
# Vivian chose (B, « R5 » in glass, scripts/make-logos.py draws it):
#   icon0.png  512 x 512, the tile
#   pic0.dds   3840 x 2160, BC7, the background while the tile is selected
#              (the system writes the title and the buttons on its left: the
#              glass « R5 » stands on the right)
#   pic1.dds   3840 x 2160, BC7, the background while the app starts
#              (the « R5 » and « Régie5 » in the middle)
# The backgrounds are BC7 by scripts/bc7.py (texconv is Windows only), read
# back and compared with what was drawn.
#   scripts/make-icons.py <folder>                the three files, new (never over existing ones)
#   scripts/make-icons.py <folder> --png          also pic0.png and pic1.png, to look at
#   scripts/make-icons.py <folder> --backgrounds  pic0.dds and pic1.dds only
# icon0.png is kept in the repository (ps5/app/sce_sys/, where the homebrew
# catalog links it): redraw it here after a change to the logo. The
# backgrounds (8 MiB each) are drawn by scripts/build-ps5.sh at each build,
# into its temporary sce_sys. Needs Pillow and numpy.
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import os
import sys

from PIL import Image, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, file))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


logos = load("regie5_logos", "make-logos.py")
bc7 = load("regie5_bc7", "bc7.py")

W, H = 3840, 2160
AMBER, CORAL, TEAL = logos.AMBER, logos.CORAL, logos.TEAL
# The tile background of variant B, as make-logos.py lights it.
LIGHTS = [((0.95, 1.0), 0.40, TEAL, 0.30), ((0.05, 0.0), 0.35, AMBER, 0.18)]


def backdrop():
    return logos.background((W, H), [((x * W, y * H), r * W, c, s) for (x, y), r, c, s in LIGHTS])


def glass_r5(img, cx, baseline, height):
    """The glass « R5 » of the icon, its glyphs 'height' tall, centred on cx:
    the same three lights behind it, the same glass, at this size."""
    k = height / (logos.N * 0.50)          # the icon draws its glyphs N * 0.50 tall
    n = logos.N
    # The icon's lights, placed around the glyphs as there (centre N/2, baseline N * 0.68).
    def at(x, y):
        return (cx + (x - 0.5) * n * k, baseline + (y - 0.68) * n * k)
    img = Image.alpha_composite(img, logos.glow(img.size, at(0.33, 0.40), n * 0.15 * k, CORAL, 0.95))
    img = Image.alpha_composite(img, logos.glow(img.size, at(0.50, 0.62), n * 0.12 * k, AMBER, 0.85))
    img = Image.alpha_composite(img, logos.glow(img.size, at(0.68, 0.45), n * 0.15 * k, TEAL, 0.95))
    f = ImageFont.truetype(logos.BOLD, int(height))
    m = logos.text_mask(img.size, "R5", f, cx, baseline)
    m = m.filter(ImageFilter.MaxFilter(max(3, int(logos.S * 3 * k)) | 1))
    return logos.glass(img, m, (255, 255, 255), body=0.14, rim=1.0, shine=1.0, depth=None)


def pic0():
    img = glass_r5(backdrop(), W * 0.74, H * 0.62, H * 0.40)
    return img.convert("RGB")


def pic1():
    img = glass_r5(backdrop(), W * 0.50, H * 0.58, H * 0.42)
    # « Régie5 » under it, the 5 in amber, as on the icon (wordmark draws at size_px * S)
    logos.wordmark(img, H * 0.78, AMBER, size_px=int(H * 0.085 / logos.S))
    return img.convert("RGB")


def main():
    args = [a for a in sys.argv[1:] if a not in ("--png", "--backgrounds")]
    if len(args) != 1:
        print("usage: scripts/make-icons.py <folder> [--png] [--backgrounds]", file=sys.stderr)
        return 2
    out = args[0]
    os.makedirs(out, exist_ok=True)
    if "--backgrounds" not in sys.argv:
        with open(os.path.join(out, "icon0.png"), "xb") as f:   # "x": never over an existing file
            logos.variant_glass_r5().save(f, format="PNG", optimize=True)
    for name, draw in (("pic0", pic0), ("pic1", pic1)):
        img = draw()
        if "--png" in sys.argv:
            with open(os.path.join(out, name + ".png"), "xb") as f:
                img.save(f, format="PNG", optimize=True)
        psnr = bc7.write_dds(img, os.path.join(out, name + ".dds"))
        if psnr < 40:
            print(f"{name}.dds: BC7 too far from the drawing (PSNR {psnr:.1f} dB)", file=sys.stderr)
            return 1
        print(f"{name}.dds: BC7 3840x2160, PSNR {psnr:.1f} dB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
