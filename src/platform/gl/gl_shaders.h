/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The OpenGL backend's shaders. Written to the subset GLSL 3.30 core and
 * GLSL ES 3.00 share: gfx_gl.cpp puts the version line in front.
 *
 * The liquid glass is Jelly5's (app/shaders/liquid_glass.pipe, GPL-3.0-or-later),
 * whose refraction profile follows OverShifted/LiquidGlass (MIT). Only its inputs
 * changed: the pixel comes from gl_FragCoord, and the blurred backdrop is a
 * region of the screen at half resolution rather than the whole target.
 */
#pragma once

namespace gl_shaders {

/* ---- 2D: every quad, with its rounded shape cut by a signed distance ---- */

constexpr const char *kUiVert = R"(
layout(location = 0) in vec2 aPos;     /* logical px */
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;   /* premultiplied */
layout(location = 3) in vec4 aRect;    /* the shape: x, y, w, h (logical) */
layout(location = 4) in vec4 aParam;   /* radius, mode, p1, p2 */
uniform vec2 uCanvas;
out vec2 vPos;
out vec2 vUV;
out vec4 vColor;
flat out vec4 vRect;
flat out vec4 vParam;
void main() {
    vPos = aPos;
    vUV = aUV;
    vColor = aColor;
    vRect = aRect;
    vParam = aParam;
    gl_Position = vec4(aPos.x / uCanvas.x * 2.0 - 1.0, 1.0 - aPos.y / uCanvas.y * 2.0, 0.0, 1.0);
}
)";

/* mode 0: fill / texture; 1: a lit rim of width p1; 2: a soft shadow of blur p1. */
constexpr const char *kUiFrag = R"(
uniform sampler2D uTex;
uniform float uScale;   /* output px per logical px */
uniform vec2 uLight;    /* direction toward the light */
in vec2 vPos;
in vec2 vUV;
in vec4 vColor;
flat in vec4 vRect;
flat in vec4 vParam;
out vec4 oColor;

float sd_round_box(vec2 p, vec2 c, vec2 hs, float r) {
    vec2 q = abs(p - c) - (hs - vec2(r));
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 hs = 0.5 * vRect.zw;
    vec2 c = vRect.xy + hs;
    float r = min(vParam.x, min(hs.x, hs.y));
    float d = sd_round_box(vPos, c, hs, r);
    vec4 base = texture(uTex, vUV) * vColor;
    float a;
    if (vParam.y < 0.5) {
        a = clamp(0.5 - d * uScale, 0.0, 1.0);
    } else if (vParam.y < 1.5) {
        /* Brightest where it faces the light, a fainter glint opposite. */
        float w = max(vParam.z, 1.0 / uScale);
        float outer = clamp(0.5 - d * uScale, 0.0, 1.0);
        float inner = clamp(0.5 - (d + w) * uScale, 0.0, 1.0);
        vec2 rel = (vPos - c) / max(hs, vec2(1.0));
        float lit = dot(normalize(rel + vec2(1e-4)), normalize(uLight));
        float k = 0.22 + 0.78 * pow(max(lit, 0.0), 1.5) + 0.35 * pow(max(-lit, 0.0), 2.0);
        a = (outer - inner) * k;
    } else {
        float b = max(vParam.z, 0.5);
        a = 1.0 - smoothstep(-b, b, d);
    }
    oColor = base * a;
}
)";

/* ---- a quad from gl_VertexID (no buffers): the blur passes and the glass ---- */

constexpr const char *kQuadVert = R"(
uniform vec4 uQuad;     /* x0, y0, x1, y1 in clip space */
uniform vec4 uUVRect;   /* u0, v0, u1, v1 */
out vec2 vUV;
void main() {
    vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    vUV = mix(uUVRect.xy, uUVRect.zw, p);
    gl_Position = vec4(mix(uQuad.xy, uQuad.zw, p), 0.0, 1.0);
}
)";

/* A picture copied as it is: the scene to the screen, a region shrunk for
 * the blur. A drawn quad, never glBlitFramebuffer: ps5-gallium does a blit
 * on the CPU (470 ms a frame in 4K on the console). */
constexpr const char *kCopyFrag = R"(
uniform sampler2D uSrc;
in vec2 vUV;
out vec4 oColor;
void main() {
    oColor = texture(uSrc, vUV);
}
)";

/* One pass of a separable Gaussian, clamped to the region that holds the picture. */
constexpr const char *kBlurFrag = R"(
uniform sampler2D uSrc;
uniform vec2 uStep;      /* one texel along the pass, in uv */
uniform float uSigma;    /* in texels */
uniform vec4 uClampUV;   /* min u, min v, max u, max v */
in vec2 vUV;
out vec4 oColor;
void main() {
    float s = max(uSigma, 0.5);
    int n = int(min(ceil(s * 3.0), 32.0));
    vec4 acc = vec4(0.0);
    float wsum = 0.0;
    for (int i = -n; i <= n; i++) {
        float w = exp(-0.5 * float(i * i) / (s * s));
        acc += texture(uSrc, clamp(vUV + uStep * float(i), uClampUV.xy, uClampUV.zw)) * w;
        wsum += w;
    }
    oColor = acc / wsum;
}
)";

/* The liquid glass pane (Jelly5's liquid_glass.pipe, see the top of this file). */
constexpr const char *kGlassFrag = R"(
uniform vec4 uRect;    /* the pane: x0, y0, x1, y1 in target pixels (top-left origin) */
uniform vec4 uShape;   /* x = corner radius px, y = bevel px */
uniform vec4 uLight;   /* xy = direction toward the light, z = rim strength, w = refraction (0..1) */
uniform vec4 uTone;    /* x = saturation, y = brightness, z = tint amount, w = opacity */
uniform vec4 uTint;    /* rgb = tint colour, a = sheen strength */
uniform vec4 uMore;    /* x = lift (0 pane, 1 the focus drop), y = legibility, z = magnification, w = dispersion */
uniform vec4 uMap;     /* the blurred region: x0, y1 (target px), u and v per target px */
uniform vec4 uClampUV;
uniform float uTargetH;
uniform sampler2D uSource;
in vec2 vUV;
out vec4 oColor;

vec3 src(vec2 p) {
    vec2 uv = vec2((p.x - uMap.x) * uMap.z, (uMap.y - p.y) * uMap.w);
    return texture(uSource, clamp(uv, uClampUV.xy, uClampUV.zw)).rgb;
}

void main() {
    vec2 px = vec2(gl_FragCoord.x, uTargetH - gl_FragCoord.y);
    vec2 lo = uRect.xy, hi = uRect.zw;
    vec2 c = 0.5 * (lo + hi), half_size = 0.5 * (hi - lo);
    float r = min(uShape.x, min(half_size.x, half_size.y));

    /* Signed distance to the rounded box (negative inside) and its outward normal. */
    vec2 rel = px - c;
    vec2 q = abs(rel) - (half_size - vec2(r));
    vec2 qo = max(q, vec2(0.0));
    float d = length(qo) + min(max(q.x, q.y), 0.0) - r;
    vec2 n;
    if (qo.x > 0.0 || qo.y > 0.0)
        n = normalize(qo);
    else
        n = q.x > q.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    n *= sign(rel + vec2(1e-4));

    float coverage = clamp(0.5 - d, 0.0, 1.0);
    if (coverage <= 0.0)
        discard;

    /* Refraction after OverShifted/LiquidGlass (MIT): a thick lens, strongly
     * bent at the rim and flat inside. */
    float h = max(min(half_size.x, half_size.y), 1.0);
    float t = max(-d, 0.0) / h;
    float f = 1.0 - 2.3 * pow(5.2 * 2.718281828, -6.9 * t - 0.7);
    float pull = h * (1.0 - pow(clamp(f, 0.0, 1.0), 3.0)) * uLight.w;
    vec2 off = -n * pull - rel * (uMore.z / (1.0 + uMore.z));
    /* Dispersion along the rim: red bends least, blue most. */
    vec2 spread = -n * pull * uMore.w;
    vec3 col;
    col.r = src(px + off - spread).r;
    col.g = src(px + off).g;
    col.b = src(px + off + spread).b;

    /* Vibrancy, then the tint. */
    float luma = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(luma), col, uTone.x) * uTone.y;
    col = mix(col, uTint.rgb, uTone.z);
    /* Legibility: over a bright picture the glass darkens itself. */
    col *= 1.0 - uMore.y * smoothstep(0.42, 0.9, luma);
    /* The focus drop: the same glass, lifted toward white. */
    col = mix(col, vec3(1.0), 0.09 * uMore.x);

    vec2 L = normalize(uLight.xy);
    float lit = dot(n, L);
    /* Glow along the rim, by angle to the light. */
    float edge = 1.0 - smoothstep(0.0, 0.12 * h, -d);
    float ang = dot(normalize(rel / max(half_size, vec2(1.0))), L);
    col *= 1.0 + 0.32 * uLight.z * ang * edge;
    /* Thickness: a soft inner shadow just inside the rim, deepest away from the light. */
    float inner = smoothstep(1.5, 4.0, -d) * (1.0 - smoothstep(4.0, 14.0, -d));
    col *= 1.0 - inner * (0.10 + 0.12 * max(-lit, 0.0)) * uLight.z;
    /* The rim: a soft band of light where the pane turns, with a hairline of its own. */
    float band = 1.0 - smoothstep(0.0, max(0.22 * h, 3.0), -d);
    float hair = 1.0 - smoothstep(0.0, 1.4, -d);
    float side = 0.45 * pow(max(lit, 0.0), 1.5) + 0.12 * pow(max(-lit, 0.0), 1.5);
    col += vec3((band * band * side * 0.45 + hair * side * 0.6) * uLight.z * (1.0 + 0.5 * uMore.x));
    /* Sheen from above, and a little grain so it reads as a material. */
    float from_top = (px.y - lo.y) / max(hi.y - lo.y, 1.0);
    col += vec3((1.0 - smoothstep(0.0, 0.5, from_top)) * uTint.a);
    col += (fract(sin(dot(px, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * 0.02 * (1.0 - uMore.x);

    float a = coverage * uTone.w;
    oColor = vec4(clamp(col, 0.0, 1.0) * a, a);
}
)";

} // namespace gl_shaders
