/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: each held effect's own picture, drawn in a circle (the stage: the rings' middle; the
 * display: a badge in the corner), moving with what it does. u: seconds held; ph: where in the beat. */

static int kx_fx_top(void)                        /* the effect pressed last of those held, -1 none */
{
    int i, best = -1;
    uint64_t t = 0;
    for (i = 0; i < KX_NFX; i++)
        if (kx_fx_on[i] && kx_fx_t0[i] >= t)
            t = kx_fx_t0[i], best = i;
    return best;
}
static float fa_ph(void)
{
    float b = (float)beat_samples();
    return b > 1.0f ? (float)(beat_pos % (uint32_t)b) / b : 0.0f;
}
static void fa_ring(float cx, float cy, float r, float wd, uint32_t c)
{
    int i, n = 48;
    float px = cx, py = cy - r;
    for (i = 1; i <= n; i++) {
        float a = (float)i / (float)n, x = cx + vd_sin(a) * r, y = cy - vd_sin(a + 0.25f) * r;
        vg_line(px, py, x, y, wd, c);
        px = x, py = y;
    }
}
static void fa_reel(float cx, float cy, float r, float ang, uint32_t c)   /* a tape reel turned to ang (turns) */
{
    int k;
    vg_circle(cx, cy, r, c);
    vg_circle(cx, cy, r * 0.82f, 0x101012);
    vg_circle(cx, cy, r * 0.3f, c);
    for (k = 0; k < 3; k++) {
        float a = ang + (float)k / 3.0f;
        vg_line(cx + vd_sin(a) * r * 0.32f, cy - vd_sin(a + 0.25f) * r * 0.32f, cx + vd_sin(a) * r * 0.78f,
                cy - vd_sin(a + 0.25f) * r * 0.78f, r * 0.14f, c);
    }
}

static void kx_fx_art(int fx, float cx, float cy, float r, float u, float lv)
{
    float t = te_t(), ph = fa_ph();
    int i;
    uint32_t red = C_RED, hot = 0xFFB090, ink = TE_INK;
    vg_circle(cx, cy, r, 0x08080A);
    if (fx >= PF_NFX) switch (fx - PF_NFX) {      /* ---- the VIBE's own ---- */
    case PN_PRESS: {                              /* two plates slam together on the beat */
        float d = ph < 0.5f ? vd_sqrt(ph * 2.0f) : 1.0f, gap = r * (0.08f + 0.5f * d);
        vg_rect((int)(cx - r * 0.7f), (int)(cy - gap - r * 0.35f), (int)(r * 1.4f), (int)(r * 0.35f), 0x5A5A60);
        vg_rect((int)(cx - r * 0.7f), (int)(cy + gap), (int)(r * 1.4f), (int)(r * 0.35f), 0x5A5A60);
        vg_rect((int)(cx - r * 0.7f), (int)(cy - gap - 2), (int)(r * 1.4f), 2, red);
        vg_rect((int)(cx - r * 0.7f), (int)(cy + gap), (int)(r * 1.4f), 2, red);
        vg_rect((int)(cx - r * 0.5f), (int)(cy - gap + 2), (int)(r * 1.0f), (int)(gap * 2.0f - 4.0f), vg_mix(0x08080A, red, 1.0f - d));
        break;
    }
    case PN_SHRED:                                /* strips flung sideways, new each 1/32 */
        for (i = 0; i < 9; i++) {
            uint32_t q = ((uint32_t)(t * 32.0f) + (uint32_t)i * 7919u) * 2654435761u;
            float y = cy - r * 0.7f + (float)i * r * 0.16f, off = ((float)(q >> 24) / 255.0f - 0.5f) * r * 0.8f;
            vg_rect((int)(cx - r * 0.45f + off), (int)y, (int)(r * 0.9f), (int)(r * 0.1f), i & 1 ? ink : red);
        }
        break;
    case PN_BLACKOUT: {                           /* a bulb, its filament flickering, going out */
        float p = pn_pow, fl = p > 0.05f && (((uint32_t)(t * 23.0f) * 2654435761u) >> 28) < 3u ? 0.2f : 1.0f;
        float b = p * fl;
        for (i = 6; i > 0; i--)
            vg_circle(cx, cy - r * 0.1f, r * (0.3f + (float)i * 0.08f) , vg_mix(0x08080A, 0x5A3A10, b * 0.15f * (float)(7 - i)));
        vg_circle(cx, cy - r * 0.1f, r * 0.36f, vg_mix(0x1A1A1E, 0xFFE0A0, b));
        vg_rect((int)(cx - r * 0.16f), (int)(cy + r * 0.22f), (int)(r * 0.32f), (int)(r * 0.3f), 0x6A6A70);
        vg_line(cx - r * 0.12f, cy - r * 0.05f, cx, cy - r * 0.22f, 2.0f, vg_mix(0x3A1A0A, 0xFFFFFF, b));
        vg_line(cx, cy - r * 0.22f, cx + r * 0.12f, cy - r * 0.05f, 2.0f, vg_mix(0x3A1A0A, 0xFFFFFF, b));
        break;
    }
    case PN_SHORTWAVE: {                          /* a dial, its needle drifting, static round it */
        float n = 0.5f + 0.35f * vd_sin(pn_sw_ph);
        vg_rect((int)(cx - r * 0.75f), (int)(cy - r * 0.25f), (int)(r * 1.5f), (int)(r * 0.5f), 0x2A2418);
        for (i = 0; i <= 20; i++)
            vg_rect((int)(cx - r * 0.7f + (float)i * r * 0.07f), (int)(cy - r * 0.2f), 1, i % 5 ? 6 : 12, 0xC8B080);
        vg_rect((int)(cx - r * 0.7f + n * r * 1.4f), (int)(cy - r * 0.24f), 3, (int)(r * 0.48f), red);
        for (i = 0; i < 40; i++) {
            uint32_t q = ((uint32_t)(t * 30.0f) * 977u + (uint32_t)i * 7919u) * 2654435761u;
            float a = (float)(q >> 20 & 1023u) / 1024.0f, rr = r * (0.45f + (float)(q >> 10 & 255u) / 255.0f * 0.5f);
            vg_rect((int)(cx + vd_sin(a) * rr), (int)(cy - vd_sin(a + 0.25f) * rr), 2, 2, 0x8A8A92);
        }
        break;
    }
    case PN_CORRODE: {                            /* rust spreading over the disc, the longer the more */
        float k = vd_minf(kx_pn_held_s[PN_CORRODE] / 3.0f, 1.0f) + 0.15f;
        uint32_t q = 0x9E3779B9u;
        fa_ring(cx, cy, r * 0.9f, 2.0f, 0x6A6A70);
        for (i = 0; i < 60; i++) {
            float a, rr, sz;
            q = q * 1664525u + 1013904223u, a = (float)(q >> 8 & 1023u) / 1024.0f;
            q = q * 1664525u + 1013904223u, rr = (float)(q >> 8 & 1023u) / 1024.0f * r * 0.85f;
            sz = (float)(q >> 20 & 15u) * 0.6f + 2.0f;
            if ((float)i / 60.0f < k)
                vg_circle(cx + vd_sin(a) * rr, cy - vd_sin(a + 0.25f) * rr, sz * k, i % 3 ? 0x7A3A1A : 0x4A200E);
        }
        break;
    }
    case PN_HAMMER: {                             /* a hammer swung down on the anvil each 1/8, sparks on the blow */
        float e = ph * 2.0f - (float)(int)(ph * 2.0f), lift = e < 0.12f ? 0.0f : (e - 0.12f) / 0.88f;
        float px = cx + r * 0.55f, py = cy + r * 0.05f, ang = 0.75f + 0.17f * lift;   /* (from the pivot, towards the anvil) */
        float hx = px + vd_sin(ang) * r * 0.8f, hy = py - vd_sin(ang + 0.25f) * r * 0.8f, nx = vd_sin(ang + 0.25f), ny = vd_sin(ang);
        vg_rect((int)(cx - r * 0.55f), (int)(cy + r * 0.28f), (int)(r * 0.7f), (int)(r * 0.18f), 0x4A4A50);   /* (the anvil) */
        vg_line(px, py, hx, hy, r * 0.07f, 0x8A6A40);
        vg_line(hx - nx * r * 0.16f, hy - ny * r * 0.16f, hx + nx * r * 0.16f, hy + ny * r * 0.16f, r * 0.2f, 0x9A9AA2);
        if (e < 0.25f)
            for (i = 0; i < 12; i++) {
                float a2 = 0.6f + (float)i * 0.025f + (i & 1 ? 0.3f : 0.0f), sl = (e / 0.25f) * r * 0.55f;
                vg_line(cx - r * 0.2f, cy + r * 0.27f, cx - r * 0.2f + vd_sin(a2) * sl, cy + r * 0.27f - vd_sin(a2 + 0.25f) * sl,
                        1.5f, vg_mix(0xFFE080, red, e * 4.0f));
            }
        break;
    }
    case PN_VOID: {                               /* a black hole: rings falling in, dust spiralling */
        for (i = 0; i < 5; i++) {
            float k = 1.0f - (t * 0.6f + (float)i * 0.2f - (float)(int)(t * 0.6f + (float)i * 0.2f));
            fa_ring(cx, cy, r * 0.9f * k, 1.5f, vg_mix(0x08080A, 0x6A1A10, k));
        }
        for (i = 0; i < 24; i++) {
            float k = 1.0f - (t * 0.4f + (float)i / 24.0f - (float)(int)(t * 0.4f + (float)i / 24.0f)), a = (float)i * 0.37f + k * 1.5f;
            vg_rect((int)(cx + vd_sin(a) * r * 0.9f * k), (int)(cy - vd_sin(a + 0.25f) * r * 0.9f * k), 2, 2, ink);
        }
        vg_circle(cx, cy, r * 0.22f, 0x000000);
        fa_ring(cx, cy, r * 0.22f, 2.0f, hot);
        break;
    }
    default: {                                    /* STROBE: the disc flashing at 1/32 */
        float s = ph * 8.0f;
        int on = s - (float)(int)s < 0.5f;
        vg_circle(cx, cy, r * 0.85f, on ? 0xF4F4F4 : 0x000000);
        fa_ring(cx, cy, r * 0.85f, 2.0f, red);
        break;
    }
    } else switch (fx) {                          /* ---- the core's hold effects ---- */
    case PF_R8: case PF_R16: case PF_R32: case PF_ROLL: {   /* the same slice, repeated: blocks lighting in turn */
        int nrep = fx == PF_R8 ? 2 : fx == PF_R16 ? 4 : fx == PF_R32 ? 8 : 4, cur = (int)(ph * (float)nrep * 2.0f) % nrep;
        float bw = r * 1.4f / (float)nrep;
        for (i = 0; i < nrep; i++)
            vg_rect((int)(cx - r * 0.7f + (float)i * bw + 1.0f), (int)(cy - r * 0.3f), (int)(bw - 2.0f), (int)(r * 0.6f),
                    i == cur ? red : 0x3A3A40);
        break;
    }
    case PF_REV:                                  /* an arrow round the ring, going back */
        for (i = 0; i < 18; i++) {
            float a = -t * 0.5f + (float)i / 24.0f;
            vg_circle(cx + vd_sin(a) * r * 0.7f, cy - vd_sin(a + 0.25f) * r * 0.7f, 2.0f + (float)i * 0.25f, vg_mix(0x2A2A30, red, (float)i / 18.0f));
        }
        break;
    case PF_LPF: case PF_HPF: {                   /* the filter's curve, its corner moving */
        float c = 0.5f + 0.4f * vd_sin(u * 0.5f), px = cx - r * 0.75f, py = cy;
        for (i = 1; i <= 30; i++) {
            float x = cx - r * 0.75f + (float)i * r * 0.05f, f = (float)i / 30.0f, g;
            g = fx == PF_LPF ? (f < c ? 0.0f : (f - c) * 2.5f) : (f > c ? 0.0f : (c - f) * 2.5f);
            vg_line(px, py, x, cy - r * 0.2f + g * r * 0.5f, 2.5f, red);
            px = x, py = cy - r * 0.2f + g * r * 0.5f;
        }
        break;
    }
    case PF_TAPE: {                               /* two reels slowing to a stop */
        float sp = 1.0f - vd_minf(u / 1.2f, 1.0f), ang = u * (1.2f - u * 0.5f * (1.0f - sp));
        fa_reel(cx - r * 0.38f, cy, r * 0.34f, ang, ink);
        fa_reel(cx + r * 0.38f, cy, r * 0.34f, ang * 0.9f, ink);
        vg_line(cx - r * 0.38f, cy + r * 0.34f, cx + r * 0.38f, cy + r * 0.34f, 2.0f, vg_mix(0x3A2A1A, red, sp));
        break;
    }
    case PF_FRZ:                                  /* frost: a crystal grows */
        for (i = 0; i < 6; i++) {
            float a = (float)i / 6.0f, l = r * 0.75f * vd_minf(u * 2.0f + 0.2f, 1.0f);
            vg_line(cx, cy, cx + vd_sin(a) * l, cy - vd_sin(a + 0.25f) * l, 2.0f, 0xA0D0FF);
            vg_line(cx + vd_sin(a) * l * 0.5f, cy - vd_sin(a + 0.25f) * l * 0.5f, cx + vd_sin(a + 0.06f) * l * 0.75f, cy - vd_sin(a + 0.31f) * l * 0.75f, 1.5f, 0xA0D0FF);
            vg_line(cx + vd_sin(a) * l * 0.5f, cy - vd_sin(a + 0.25f) * l * 0.5f, cx + vd_sin(a - 0.06f) * l * 0.75f, cy - vd_sin(a + 0.19f) * l * 0.75f, 1.5f, 0xA0D0FF);
        }
        break;
    case PF_OUP: case PF_ODN: {                   /* stairs of an octave, the step up or down lit */
        int up = fx == PF_OUP;
        for (i = 0; i < 4; i++) {
            float x = cx - r * 0.6f + (float)i * r * 0.3f, h = r * 0.15f * (float)(up ? i + 1 : 4 - i);
            vg_rect((int)x, (int)(cy + r * 0.4f - h), (int)(r * 0.28f), (int)h, i == (int)(ph * 4.0f) ? red : 0x3A3A40);
        }
        vg_tri(cx, cy - r * (up ? 0.65f : 0.15f), cx - r * 0.15f, cy - r * 0.4f, cx + r * 0.15f, cy - r * 0.4f, ink);
        break;
    }
    case PF_FLG: case PF_PHS: {                   /* interference: two rings of waves sliding past */
        int q;
        for (q = 0; q < 2; q++) {
            float px = 0, py = 0;
            for (i = 0; i <= 48; i++) {
                float a = (float)i / 48.0f, rr = r * (0.55f + 0.12f * vd_sin(a * (fx == PF_FLG ? 6.0f : 4.0f) + t * (q ? -0.4f : 0.3f)));
                float x = cx + vd_sin(a) * rr, y = cy - vd_sin(a + 0.25f) * rr;
                if (i)
                    vg_line(px, py, x, y, 2.0f, q ? red : ink);
                px = x, py = y;
            }
        }
        break;
    }
    case PF_RTHR: case PF_ETHR:                   /* rings thrown out, fading (the reverb's a haze, the echo's apart) */
        for (i = 0; i < 5; i++) {
            float k = u * (fx == PF_ETHR ? 1.2f : 0.7f) + (float)i * 0.2f;
            k -= (float)(int)k;
            fa_ring(cx, cy, r * 0.15f + k * r * 0.8f, fx == PF_ETHR ? 2.0f : 5.0f, vg_mix(red, 0x08080A, k));
        }
        break;
    case PF_GATE:                                 /* the ring chopped, its pieces blinking */
        for (i = 0; i < 16; i++) {
            float a = (float)i / 16.0f;
            int on = ((int)(ph * 16.0f) + i) % 2;
            vg_line(cx + vd_sin(a) * r * 0.6f, cy - vd_sin(a + 0.25f) * r * 0.6f, cx + vd_sin(a + 0.04f) * r * 0.6f,
                    cy - vd_sin(a + 0.29f) * r * 0.6f, 8.0f, on ? red : 0x2A2A30);
        }
        break;
    case PF_CRSW: {                               /* a grid getting coarser */
        int n = 2 + (int)((1.0f - vd_minf(u, 1.0f)) * 10.0f), x, y;
        float cs = r * 1.2f / (float)n;
        for (y = 0; y < n; y++)
            for (x = 0; x < n; x++)
                if ((x + y + (int)(t * 4.0f)) % 3)
                    vg_rect((int)(cx - r * 0.6f + (float)x * cs), (int)(cy - r * 0.6f + (float)y * cs), (int)cs - 1, (int)cs - 1,
                            (x * 7 + y * 3) % 5 ? 0x3A3A40 : red);
        break;
    }
    default: {                                    /* RANDOM: a die, a new face each 1/16 */
        static const uint16_t FACE[6] = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};   /* (pips on a 3 x 3) */
        uint32_t n16 = (uint32_t)(t * 8.0f);
        int f = (int)((n16 * 2654435761u) >> 29) % 6;
        vg_round((int)(cx - r * 0.45f), (int)(cy - r * 0.45f), (int)(r * 0.9f), (int)(r * 0.9f), (int)(r * 0.12f), ink);
        for (i = 0; i < 9; i++)
            if ((FACE[f] >> i) & 1u)
                vg_circle(cx + ((float)(i % 3) - 1.0f) * r * 0.25f, cy + ((float)(i / 3) - 1.0f) * r * 0.25f, r * 0.07f, 0x08080A);
        break;
    }
    }
    (void)lv;
}

static void kx_fx_badge(void)                     /* the display: the held effect's picture in a corner plate */
{
    int fx = kx_fx_top();
    if (fx < 0)
        return;
    vg_round(394, 94, 82, 104, 6, 0x000000);
    vg_frame(394, 94, 82, 104, 1, C_RED);
    kx_fx_art(fx, 435.0f, 136.0f, 34.0f, (float)(vibe_frames() - kx_fx_t0[fx]) / kx_sr, 1.0f);
    vg_text(kx_fx_cell(fx), 435, 176, VG_BOLD, C_WHITE, VG_CENTER);
}
