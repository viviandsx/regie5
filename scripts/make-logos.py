#!/usr/bin/env python3
# Regie5 — logo proposals: three 512 x 512 tile icons in the "liquid glass"
# spirit (frosted, lit from inside, a rim of light), each with its tile
# background (1920 x 1080), and a sheet showing them at home-screen sizes.
# The identity is Regie5's own: a control room ("régie") in warm amber,
# coral and teal on graphite, nothing of Jelly5's symbol or blue-violet.
#   scripts/make-logos.py <new output folder>      e.g. build/logos
# Needs Pillow. Drawn at 4x, then reduced, for clean edges.
# SPDX-License-Identifier: GPL-3.0-or-later
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOLD = os.path.join(ROOT, "assets", "fonts", "Inter-Bold.ttf")
SEMI = os.path.join(ROOT, "assets", "fonts", "Inter-SemiBold.ttf")

S = 4                      # supersampling
N = 512 * S                # icon side while drawing
GRAPHITE = (11, 12, 17)
TEXT = (246, 244, 240)


# ---- small tools ------------------------------------------------------------

def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def glow(size, centre, radius, colour, strength=1.0):
    """A soft round light, as an RGBA layer."""
    w, h = size
    layer = Image.new("L", size, 0)
    d = ImageDraw.Draw(layer)
    x, y = centre
    d.ellipse((x - radius, y - radius, x + radius, y + radius), fill=int(255 * strength))
    layer = layer.filter(ImageFilter.GaussianBlur(radius * 0.55))
    out = Image.new("RGBA", size, colour + (0,))
    out.putalpha(layer)
    return out


def background(size, lights):
    """Graphite lit by a few coloured glows."""
    img = Image.new("RGBA", size, GRAPHITE + (255,))
    for centre, radius, colour, strength in lights:
        img = Image.alpha_composite(img, glow(size, centre, radius, colour, strength))
    return img


def vgrad(size, top, bottom):
    w, h = size
    g = Image.new("RGBA", size)
    px = g.load()
    for y in range(h):
        c = lerp(top, bottom, y / max(1, h - 1))
        for x in range(w):
            px[x, y] = c
    return g


def rounded_mask(size, box, radius):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=radius, fill=255)
    return m


def glass(base, mask, tint, edge=(255, 255, 255), body=0.16, rim=0.9, shine=0.55, depth=None):
    """Frosted glass over base, in the shape of mask: the backdrop blurred and
    tinted, light from below inside (depth), a shine on the upper part, a rim
    of light brighter at the top, and a soft shadow under it."""
    size = base.size
    bbox = mask.getbbox()
    if not bbox:
        return base
    top_y, h = bbox[1], bbox[3] - bbox[1]
    # shadow under the pane
    sh = ImageChops.offset(mask, 0, int(h * 0.04)).filter(ImageFilter.GaussianBlur(h * 0.08))
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    shadow.putalpha(sh.point(lambda v: int(v * 0.6)))
    base = Image.alpha_composite(base, shadow)
    # the backdrop seen through: blurred, tinted
    seen = base.filter(ImageFilter.GaussianBlur(max(2, h * 0.08)))
    seen = Image.blend(seen, Image.new("RGBA", size, tint + (255,)), body)
    if depth:
        cx = (bbox[0] + bbox[2]) / 2
        seen = Image.alpha_composite(seen, glow(size, (cx, bbox[3]), h * 0.55, depth, 0.45))
    out = base.copy()
    out.paste(seen, (0, 0), mask)
    # shine: white fading from the top edge down to 45 % of the height
    ramp = Image.new("L", size, 0)
    rp = ImageDraw.Draw(ramp)
    span = max(1, int(h * 0.45))
    for i in range(span):
        rp.line((0, top_y + i, size[0], top_y + i), fill=int(255 * shine * 0.40 * (1 - i / span) ** 1.6))
    sl = Image.new("RGBA", size, edge + (0,))
    sl.putalpha(ImageChops.multiply(ramp, mask))
    out = Image.alpha_composite(out, sl)
    # rim: the mask's edge, bright at the top, faint at the bottom
    inner = mask.filter(ImageFilter.MinFilter(int(S * 3) | 1))
    ring = ImageChops.subtract(mask, inner).filter(ImageFilter.GaussianBlur(S * 0.5))
    fade = Image.new("L", size, 0)
    fd = ImageDraw.Draw(fade)
    for y in range(top_y, bbox[3] + 1):
        t = (y - top_y) / max(1, h)
        fd.line((0, y, size[0], y), fill=int(255 * rim * (1.0 - 0.75 * t)))
    rl = Image.new("RGBA", size, edge + (0,))
    rl.putalpha(ImageChops.multiply(ring, fade))
    return Image.alpha_composite(out, rl)


def text_mask(size, text, font, centre_x, baseline):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).text((centre_x, baseline), text, font=font, fill=255, anchor="ms")
    return m


def wordmark(img, y, accent, size_px=58):
    """'Régie5' under the symbol, the 5 in the accent, as the app writes it."""
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(BOLD, size_px * S)
    parts = [("Régie", TEXT), ("5", accent)]
    widths = [d.textlength(t, font=f) for t, _ in parts]
    x = (img.size[0] - sum(widths)) / 2
    for (t, c), w in zip(parts, widths):
        d.text((x, y), t, font=f, fill=c + (255,), anchor="ls")
        x += w


def finish(img):
    return img.convert("RGB").resize((512, 512), Image.LANCZOS)


# ---- variant A: the faders ------------------------------------------------------

AMBER, CORAL, TEAL = (255, 178, 64), (255, 106, 72), (46, 214, 190)


def variant_faders():
    img = background((N, N), [((N * 0.10, N * 1.00), N * 0.45, CORAL, 0.30),
                              ((N * 0.95, N * 0.02), N * 0.40, TEAL, 0.22)])
    # the desk: a frosted rounded panel
    panel = (N * 0.17, N * 0.12, N * 0.83, N * 0.70)
    img = glass(img, rounded_mask((N, N), panel, N * 0.08), (20, 21, 28), body=0.55, depth=AMBER)
    # three slots and their knobs, at three heights
    xs = [N * 0.34, N * 0.50, N * 0.66]
    knobs = [0.30, 0.52, 0.40]          # as a fraction of the slot, from the top
    colours = [TEAL, AMBER, CORAL]
    top, bottom = N * 0.19, N * 0.63
    d = ImageDraw.Draw(img)
    for x, k, c in zip(xs, knobs, colours):
        d.rounded_rectangle((x - N * 0.012, top, x + N * 0.012, bottom), radius=N * 0.012, fill=(4, 4, 8, 230))
        ky = top + (bottom - top) * k
        # the level below the knob, lit
        lit = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        ImageDraw.Draw(lit).rounded_rectangle((x - N * 0.008, ky, x + N * 0.008, bottom - N * 0.004),
                                              radius=N * 0.008, fill=c + (255,))
        img = Image.alpha_composite(img, lit.filter(ImageFilter.GaussianBlur(S * 1.2)))
        img = Image.alpha_composite(img, glow((N, N), (x, ky), N * 0.07, c, 0.7))
        knob = (x - N * 0.062, ky - N * 0.030, x + N * 0.062, ky + N * 0.030)
        img = glass(img, rounded_mask((N, N), knob, N * 0.026), c, body=0.55, rim=1.0, shine=0.8)
        d = ImageDraw.Draw(img)
        d.line((x - N * 0.04, ky, x + N * 0.04, ky), fill=(255, 255, 255, 200), width=S * 2)
    wordmark(img, N * 0.86, AMBER)
    return finish(img)


# ---- variant B: R5 in glass -----------------------------------------------------

def variant_glass_r5():
    img = background((N, N), [((N * 0.95, N * 0.95), N * 0.45, TEAL, 0.25),
                              ((N * 0.05, N * 0.05), N * 0.40, AMBER, 0.18)])
    # colour behind the glyphs, so the glass has something to bend
    img = Image.alpha_composite(img, glow((N, N), (N * 0.33, N * 0.40), N * 0.15, CORAL, 0.95))
    img = Image.alpha_composite(img, glow((N, N), (N * 0.50, N * 0.62), N * 0.12, AMBER, 0.85))
    img = Image.alpha_composite(img, glow((N, N), (N * 0.68, N * 0.45), N * 0.15, TEAL, 0.95))
    f = ImageFont.truetype(BOLD, int(N * 0.50))
    m = text_mask((N, N), "R5", f, N * 0.5, N * 0.68)
    m = m.filter(ImageFilter.MaxFilter(S * 3 | 1))   # a touch fuller, as glass is
    img = glass(img, m, (255, 255, 255), body=0.14, rim=1.0, shine=1.0, depth=None)
    wordmark(img, N * 0.88, AMBER)
    return finish(img)


# ---- variant C: the meter -------------------------------------------------------

def variant_meter():
    img = background((N, N), [((N * 0.50, N * 1.05), N * 0.50, AMBER, 0.28),
                              ((N * 0.02, N * 0.05), N * 0.35, TEAL, 0.20)])
    pane = (N * 0.14, N * 0.13, N * 0.86, N * 0.71)
    img = glass(img, rounded_mask((N, N), pane, N * 0.10), (20, 21, 28), body=0.55, depth=CORAL)
    # an arc of lights, green to amber to red, most of them on
    cx, cy, r = N * 0.50, N * 0.62, N * 0.30
    n = 11
    on = 8
    for i in range(n):
        a = math.radians(200 + i * (140 / (n - 1)))
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        t = i / (n - 1)
        c = lerp(TEAL, AMBER, t / 0.65) if t < 0.65 else lerp(AMBER, CORAL, (t - 0.65) / 0.35)
        lit = i < on
        if lit:
            img = Image.alpha_composite(img, glow((N, N), (x, y), N * 0.045, c, 0.85))
        d = ImageDraw.Draw(img)
        rr = N * 0.022
        d.ellipse((x - rr, y - rr, x + rr, y + rr), fill=(c + (255,)) if lit else (60, 60, 70, 255))
        d.ellipse((x - rr * 0.45, y - rr * 0.7, x + rr * 0.25, y - rr * 0.1), fill=(255, 255, 255, 120 if lit else 40))
    # the needle and its hub
    a = math.radians(200 + (on - 0.5) * (140 / (n - 1)))
    d = ImageDraw.Draw(img)
    d.line((cx, cy, cx + r * 0.80 * math.cos(a), cy + r * 0.80 * math.sin(a)), fill=TEXT + (255,), width=S * 5)
    hub = (cx - N * 0.05, cy - N * 0.05, cx + N * 0.05, cy + N * 0.05)
    hm = Image.new("L", (N, N), 0)
    ImageDraw.Draw(hm).ellipse(hub, fill=255)
    img = glass(img, hm, AMBER, body=0.6, rim=1.0, shine=0.8)
    wordmark(img, N * 0.86, CORAL)
    return finish(img)


# ---- tile backgrounds ---------------------------------------------------------------

def tile_background(lights):
    w, h = 1920 * 2, 1080 * 2
    img = background((w, h), [((x * w, y * h), r * w, c, s) for (x, y), r, c, s in lights])
    return img.convert("RGB").resize((1920, 1080), Image.LANCZOS)


VARIANTS = [
    ("a-faders", "Curseurs (table de mixage)", variant_faders,
     [((0.05, 1.0), 0.40, CORAL, 0.30), ((0.95, 0.0), 0.35, TEAL, 0.20)]),
    ("b-verre-r5", "R5 en verre", variant_glass_r5,
     [((0.95, 1.0), 0.40, TEAL, 0.30), ((0.05, 0.0), 0.35, AMBER, 0.18)]),
    ("c-vumetre", "Vumètre", variant_meter,
     [((0.50, 1.1), 0.45, AMBER, 0.28), ((0.0, 0.0), 0.30, TEAL, 0.18)]),
]


def sheet(icons, out):
    """The three side by side at 512, 256, 128 and 64 px on a home-screen-like dark ground."""
    w, h = 3 * 560 + 40, 1060
    img = Image.new("RGB", (w, h), (18, 18, 22))
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(SEMI, 26)
    for i, (name, label, icon) in enumerate(icons):
        x0 = 40 + i * 560
        d.text((x0, 20), f"{name}: {label}", font=f, fill=(230, 230, 230))
        img.paste(icon, (x0, 60))
        y = 600
        x = x0
        for s in (256, 128, 64):
            img.paste(icon.resize((s, s), Image.LANCZOS), (x, y))
            x += s + 24
    img.save(out)


def main():
    if len(sys.argv) != 2:
        print("usage: scripts/make-logos.py <new output folder>", file=sys.stderr)
        return 2
    out = sys.argv[1]
    os.makedirs(out, exist_ok=False)   # a new folder: nothing is ever drawn over
    icons = []
    for name, label, draw, lights in VARIANTS:
        icon = draw()
        icon.save(os.path.join(out, f"{name}-icon0.png"), optimize=True)
        tile_background(lights).save(os.path.join(out, f"{name}-pic0.png"), optimize=True)
        icons.append((name, label, icon))
        print(os.path.join(out, f"{name}-icon0.png"))
    sheet(icons, os.path.join(out, "planche.png"))
    print(os.path.join(out, "planche.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
