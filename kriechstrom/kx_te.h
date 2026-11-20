/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the pictures of what each effect and each pedal does. Flat, drawn with a ruler: a dot grid,
 * thick lines, white and red, small capitals for names. Each one its own machine: DAMAGE is a tape with holes bitten in
 * it, WALL bricks laid as high as it is set, RESONATE four strings in the chord, SPLICE the tape cut into sixteenths
 * and glued back wrong, HAUNT a swell into the snare or the tape braking, HISS grain; the delay a train of echoes, the
 * reverb a room, a spring or a hall; each pedal what it does to a wave. They move with the transport and the sound.
 * The control flow, reading the core. */

#define TE_INK 0xEDEDF0
#define TE_MID 0xA8A8B0
#define TE_LOW 0x3A3A40
#define TE_DOT 0x232327
static float te_t(void) { return (float)vibe_frames() / kx_sr; }
static float te_wrap(float x) { float f = x - (float)(int)x; return f < 0.0f ? f + 1.0f : f; }
static float te_lvl(void)                          /* the master's level now, 0..1 */
{
    float a = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    return a > 1.0f ? 1.0f : a;
}
/* where the transport is, in sixteenths (0..16), smooth between the steps */
static float te_pos16(void)
{
    static uint32_t last = 0xFFFFFFFFu;
    static float at;
    uint32_t s = trk[song.sel].seq_idx % 16u;
    float t = te_t(), step = 15.0f / (float)clamp(song.g[G_BPM], 20, 300), f;
    if (!song.playing)
        return (float)s;
    if (s != last)
        last = s, at = t;
    f = (t - at) / step;
    return (float)s + (f > 0.99f ? 0.99f : f);
}
static uint32_t te_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}
static float te_rnd(uint32_t x) { return (float)(te_hash(x) & 0xFFFFu) / 65535.0f; }
static void te_dots(int x, int y, int w, int h, int step)
{
    int i, j;
    for (j = y + step / 2; j < y + h; j += step)
        for (i = x + step / 2; i < x + w; i += step)
            vg_rect(i, j, 1, 1, TE_DOT);
}
static void te_label(const char *s, int x, int y, int on)
{
    vg_text(s, x, y, VG_SMALL, on ? TE_MID : 0x8A8A94, VG_LEFT);
}

/* ---- the master's six, each its own machine (x, y, w, h: its box; a 0..1 how it is set; lv the mix's level) ---- */

/* DAMAGE: a tape running left, bitten: round holes on tape, square dropouts as CHAR goes DIGITAL */
static void te_tape(int x, int y, int w, int h, float a, float ch, float lv)
{
    int ty = y + h / 2 - h / 4, th = h / 2, i, n = (int)(a * 14.0f + 0.5f);
    float run = song.playing ? te_t() * 40.0f : 0.0f;
    uint32_t tape = ch < 0.5f ? 0x4A3A30 : 0x2C3036;
    vg_rect(x, ty, w, th, tape);
    vg_rect(x, ty, w, 1, vg_mix(tape, 0xFFFFFF, 0.25f));
    vg_rect(x, ty + th - 1, w, 1, 0x000000);
    for (i = 0; i < w; i += 9) {                   /* (its grain, running) */
        int sx = x + (int)(te_wrap(((float)i - run) / (float)w + 1.0f) * (float)w);
        vg_rect(sx, ty + 2, 1, th - 4, vg_mix(tape, 0x000000, 0.25f));
    }
    for (i = 0; i < n; i++) {
        float f = te_wrap(te_rnd((uint32_t)i * 7u + 1u) - run / (float)w * 0.6f + 1.0f);
        int sx = x + 4 + (int)(f * (float)(w - 8)), sy = ty + 2 + (int)(te_rnd((uint32_t)i * 13u + 5u) * (float)(th - 4));
        float r = 1.5f + te_rnd((uint32_t)i * 3u + 2u) * (2.0f + 3.0f * a) * (0.7f + 0.6f * lv);
        if (te_rnd((uint32_t)i * 17u + 3u) < ch) {
            vg_rect(sx - (int)r, sy - (int)r, (int)(2.0f * r) + 1, (int)(2.0f * r) + 1, 0x000000);
            vg_frame(sx - (int)r - 1, sy - (int)r - 1, (int)(2.0f * r) + 3, (int)(2.0f * r) + 3, 1, C_RED);
        } else {
            vg_circle((float)sx, (float)sy, r + 1.2f, C_RED);
            vg_circle((float)sx, (float)sy, r, 0x000000);
        }
    }
}

/* WALL: bricks laid from the bottom, as many as it is set; the top course shakes with the mix */
static void te_wall(int x, int y, int w, int h, float a, float lv)
{
    int rows = h >= 60 ? 5 : 3, bh = (h - 2) / rows, bw = bh * 2 + 2, cols = w / bw + 1, r, c, laid;
    int total = rows * cols, n = (int)(a * (float)total + 0.5f), k = 0;
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++) {
            int by = y + h - (r + 1) * bh, bx = x + c * bw - ((r & 1) ? bw / 2 : 0), sh = 0, ww = bw - 2;
            laid = k++ < n;
            if (bx < x)
                ww -= x - bx, bx = x;
            if (bx + ww > x + w)
                ww = x + w - bx;
            if (ww <= 0)
                continue;
            if (laid && k > n - cols && song.playing)
                sh = (int)(lv * 3.0f * vd_sin(te_t() * 7.0f + (float)c));
            if (laid)
                vg_rect(bx, by + sh, ww, bh - 2, vg_mix(0x8A2016, 0xD03A20, te_rnd((uint32_t)(r * 31 + c))));
            else
                vg_frame(bx, by, ww, bh - 2, 1, TE_LOW);
        }
}

/* RESONATE: four strings tuned to the chord, ringing with the mix */
static void te_strings(int x, int y, int w, int h, float a, float lv, int labels)
{
    int k, i, key = clamp(song.g[G_RKEY], 0, 11), ch = clamp(song.g[G_RCHD], 0, 7);
    float t = te_t();
    for (k = 0; k < (int)RS_N; k++) {
        int semi = RS_CHORD[ch][k], sy = y + (k * 2 + 1) * h / 8, lx = x + (labels ? 30 : 4), px = lx, py = sy;
        float f = 3.0f + (float)semi * 0.25f, amp = (float)h / 14.0f * a * (0.25f + 0.75f * lv);
        if (labels) {
            vg_text(N_RKEY[(key + semi) % 12], x, sy - 8, VG_BOLD, a > 0 ? TE_INK : TE_LOW, VG_LEFT);
        }
        vg_rect(lx - 3, sy - 3, 5, 7, TE_MID);
        vg_rect(x + w - 3, sy - 3, 5, 7, TE_MID);
        for (i = 1; i <= 32; i++) {
            float u = (float)i / 32.0f, s = vd_sin(u * 0.5f) * vd_sin(t * f + (float)k) * amp;
            int nx = lx + (int)(u * (float)(x + w - 3 - lx)), ny = sy + (int)s;
            vg_line((float)px, (float)py, (float)nx, (float)ny, k < 2 ? 2.0f : 1.4f, a > 0 ? (k ? TE_INK : C_RED) : TE_LOW);
            px = nx, py = ny;
        }
    }
}

/* SPLICE: the tape in sixteenths, cut and glued back wrong: a piece backwards, again, an octave down, or gone */
static void te_splice(int x, int y, int w, int h, float a)
{
    int n = 8, sw = w / n, i, cur = (int)te_pos16() % 16;
    for (i = 0; i < n; i++) {
        int sx = x + i * sw, cy = y + h / 2, step = (cur / n) * n + i;
        float r = te_rnd((uint32_t)step * 7919u + (uint32_t)(song.playing ? (int)(te_t() * 0.25f) : 0));
        int kind = r < a * 0.9f ? 1 + (int)(te_rnd((uint32_t)step * 31u) * 4.0f) : 0, on = i == cur % n;
        uint32_t c = on ? TE_INK : kind ? C_RED : TE_MID;
        int dy = kind == 3 ? h / 6 : 0;
        if (kind == 4) {                         /* (gone: an empty frame) */
            vg_frame(sx + 2, cy - h / 5, sw - 4, 2 * h / 5, 1, TE_LOW);
        } else {
            vg_rect(sx + 2, cy - h / 5 + dy, sw - 4, 2 * h / 5, on ? 0x2E2E34 : 0x1A1A1E);
            if (kind == 1)                       /* (backwards) */
                vg_tri((float)(sx + 7), (float)(cy + dy), (float)(sx + sw - 7), (float)(cy + dy - h / 8),
                       (float)(sx + sw - 7), (float)(cy + dy + h / 8), c);
            else if (kind == 2) {                /* (again) */
                vg_rect(sx + 6, cy + dy - h / 10, sw - 12, 3, c);
                vg_rect(sx + 6, cy + dy + h / 10 - 2, sw - 12, 3, c);
            } else                               /* (as it was, or dragged an octave down) */
                vg_tri((float)(sx + sw - 7), (float)(cy + dy), (float)(sx + 7), (float)(cy + dy - h / 8),
                       (float)(sx + 7), (float)(cy + dy + h / 8), c);
        }
        if (kind && i)                           /* (the cut) */
            vg_line((float)sx, (float)(cy - h / 3), (float)sx + 3.0f, (float)(cy + h / 3), 1.5f, C_RED);
    }
}

/* HAUNT: SUCK, a reversed swell rising into beats 2 and 4, cut dead there; DRAG, the tape's speed falling over the
 * bar's last beat. The bar, its beats, where it is */
static void te_haunt(int x, int y, int w, int h, int mode, float a)
{
    float p = te_pos16() / 16.0f;
    int b, top = y + 4, bot = y + h - 4;
    uint32_t c = mode ? C_RED : TE_LOW;
    for (b = 0; b <= 4; b++)
        vg_rect(x + b * (w - 1) / 4, top, 1, bot - top, b == 0 || b == 4 ? TE_MID : TE_LOW);
    if (mode == 1) {
        for (b = 1; b <= 3; b += 2) {
            int bx = x + (b + 1) * (w - 1) / 4, sx = bx - (int)((float)(w / 4) * (0.3f + 0.7f * a));
            vg_tri((float)sx, (float)bot, (float)bx, (float)bot, (float)bx, (float)(bot - (int)((float)(bot - top) * (0.3f + 0.7f * a))), c);
            vg_rect(bx - 1, top, 3, bot - top, TE_INK);
        }
    } else {
        int px = x, py = top + 2, i;
        for (i = 1; i <= 40; i++) {
            float u = (float)i / 40.0f, d = u < 0.75f ? 0.0f : (u - 0.75f) / 0.25f;
            int nx = x + (int)(u * (float)(w - 1)), ny = top + 2 + (int)(d * d * (float)(bot - top - 4) * (mode ? 0.3f + 0.7f * a : 0.0f));
            vg_line((float)px, (float)py, (float)nx, (float)ny, 2.0f, c);
            px = nx, py = ny;
        }
    }
    if (song.playing)
        vg_rect(x + (int)(p * (float)(w - 1)), top - 2, 2, bot - top + 4, TE_INK);
}

/* HISS: grain, as dense as it is set */
static void te_hiss(int x, int y, int w, int h, float a)
{
    int i, n = (int)(a * (float)(w * h) / 24.0f);
    uint32_t f = (uint32_t)(te_t() * 20.0f);
    for (i = 0; i < n; i++) {
        uint32_t r = te_hash((uint32_t)i * 2654435761u + f);
        vg_rect(x + (int)(r % (uint32_t)w), y + (int)((r >> 12) % (uint32_t)h), 1 + (int)((r >> 24) & 1u), 1,
                (r >> 26) & 1u ? TE_INK : TE_MID);
    }
}

/* ---- the pedals: each type what it does to a wave, drawn on its window (ty its type, v its values, lv how hard) */
static float te_shape(int ty, float s, float t, float d, float u)
{
    switch (ty) {
    case 1: { float k = 1.0f + d * 5.0f, e = vd_minf(vd_absf(s * k), 3.0f); return (s < 0 ? -1.0f : 1.0f) * e / (1.0f + e) * 1.6f; }   /* SOFT */
    case 2: return vd_clamp(s * (1.0f + d * 6.0f), -0.75f, 0.75f) * 1.3f;                                           /* HARD */
    case 3: return vd_sin(s * (0.25f + d * 1.2f));                                                                   /* FOLD */
    case 4: return (s < 0 ? -0.85f : 0.85f) + (te_rnd((uint32_t)(u * 400.0f) + (uint32_t)(t * 30.0f)) - 0.5f) * 0.3f * (0.3f + d);   /* FUZZ */
    case 5: { float st = 2.0f + (1.0f - d) * 10.0f; return (float)(int)(s * st + (s < 0 ? -0.5f : 0.5f)) / st; }  /* CRUSH */
    case 6: return s * (0.55f + 0.45f * vd_sin(u * 3.0f + t * (0.2f + d)));                                         /* PHASR */
    case 7: return (s + 0.7f * vd_sin(u * 2.0f + 0.05f + 0.04f * vd_sin(t * (0.3f + d)) * 4.0f)) * 0.6f;           /* FLANG */
    case 8: return s;                                                                                               /* CHOR */
    case 9: { float e = vd_minf(vd_absf(s * 6.0f), 4.0f); return (s < 0 ? -1.0f : 1.0f) * e / (1.0f + e) * 1.2f; }  /* MUFF */
    case 10: return 0.75f * vd_sin(u * 1.0f + t * 0.3f) + 0.25f * s;                                                 /* BASS */
    case 11: return s * (0.35f + 0.65f * (1.0f - d)) + (s < 0 ? -0.35f : 0.35f) * d;                                 /* SMASH */
    case 12: return te_rnd((uint32_t)(u * 12.0f) + (uint32_t)(t * 6.0f)) < d * 0.5f ? 0.0f : s;                       /* DAMG */
    default: return s * (0.3f + 0.7f * vd_absf(vd_sin(t * 2.0f)));                                                  /* INFCT */
    }
}
static void te_ped_icon(int ty, int x, int y, int w, int h, const int16_t *v, float lv, uint32_t col)
{
    float t = te_t(), d = (float)clamp(v[1], 0, 127) / 127.0f, amp = (float)h * 0.42f * (0.35f + 0.65f * vd_minf(lv, 1.0f));
    int i, px = x, py = y + h / 2, cy = y + h / 2, n = w / 2;
    vg_rect(x, cy, w, 1, 0x26262A);
    if (ty == 8) {                                /* CHOR: the wave and its double, drifting */
        int q;
        for (q = 0; q < 2; q++) {
            px = x, py = cy;
            for (i = 1; i <= n; i++) {
                float u = (float)i / (float)n, s = vd_sin(u * 2.0f + t * 0.4f + (float)q * (0.08f + 0.06f * vd_sin(t * (0.5f + d))));
                int nx = x + i * w / n, ny = cy - (int)(s * amp);
                vg_line((float)px, (float)py, (float)nx, (float)ny, q ? 1.4f : 2.2f, q ? vg_mix(col, 0x000000, 0.4f) : col);
                px = nx, py = ny;
            }
        }
        return;
    }
    if (ty == 13) {                               /* INFCT: the source's hits, the track following */
        vg_circle((float)(x + 9), (float)cy, 6.0f + 3.0f * lv, C_RED);
        vg_circle((float)(x + 9), (float)cy, 3.0f, 0x000000);
        x += 20, w -= 20, px = x, n = w / 2;
    }
    for (i = 1; i <= n; i++) {
        float u = (float)i / (float)n, s = vd_sin(u * 2.0f + t * 0.4f);
        int nx = x + i * w / n, ny = cy - (int)(vd_clamp(te_shape(ty, s, t, d, u), -1.1f, 1.1f) * amp);
        if (ty == 12 && ny == cy) {               /* (DAMG: the gaps stay gaps) */
            px = nx, py = ny;
            continue;
        }
        vg_line((float)px, (float)py, (float)nx, (float)ny, ty == 9 ? 3.0f : 2.0f, col);
        px = nx, py = ny;
    }
}

/* ---- the pages' own pictures (x 4 .. 476, y 96 .. 206): 1 if drawn ---------------------------------------------- */
static int te_is(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static float te_g(uint32_t g) { return (float)clamp(song.g[g], 0, 127) / 127.0f; }

/* DLY: the hit and its echoes along the bar, as far apart as TIME, each FDBK quieter, darker the lower COLR is */
static void te_delay(int x, int y, int w, int h)
{
    static const float S16[13] = {4, 2, 1, 0.5f, 1.3333f, 0.6667f, 8, 16, 32, 64, 3, 1.5f, 6};
    float st = S16[clamp(song.g[G_DTIME], 0, 12)], fb = te_g(G_DFDBK), co = te_g(G_DCOLOR), mx = te_g(G_DMIX);
    float span = st * 8.0f < 16.0f ? 16.0f : st * 8.0f, p = song.playing ? te_wrap(te_pos16() / span * (span / 16.0f)) : -1.0f;
    float lvl = 1.0f;
    int k, base = y + h - 10;
    te_dots(x, y, w, h, 8);
    vg_rect(x, base, w, 1, TE_LOW);
    for (k = 0; k < 24 && lvl > 0.04f; k++) {
        float pos = (float)k * st / span;
        int bx = x + 6 + (int)(pos * (float)(w - 20)), bh = (int)((float)(h - 22) * lvl * (k ? 0.3f + 0.7f * mx : 1.0f));
        uint32_t c = k ? vg_mix(0x5A1008, vg_mix(C_RED, TE_INK, 0.4f), co * lvl + 0.2f) : TE_INK;
        if (pos > 1.0f)
            break;
        if (p >= 0.0f && p >= pos && p < pos + 0.04f)
            c = TE_INK;
        vg_rect(bx, base - bh, k ? 8 : 10, bh, c);
        lvl *= k ? fb : 0.9f;
    }
    if (p >= 0.0f)
        vg_rect(x + 6 + (int)(p * (float)(w - 20)), y + 6, 1, h - 16, TE_MID);
    te_label(N_DTIME[clamp(song.g[G_DTIME], 0, 12)], x + w - 40, y + 4, 1);
}

/* REVERB: ROOM a room in depth, SPRING a coil, HALL arches going back; the sound bouncing in it */
static void te_reverb(int x, int y, int w, int h)
{
    int ty = clamp(song.g[G_RTYPE], 0, 2), i;
    float sz = te_g(G_RSIZE), dm = te_g(G_RDAMP), lv = te_lvl(), t = te_t();
    int cx = x + w / 2, cy = y + h / 2;
    te_dots(x, y, w, h, 8);
    if (ty == 0) {
        int fw = w - 40, fh = h - 12, bw = (int)((float)fw * (0.55f - 0.3f * sz)), bh = (int)((float)fh * (0.55f - 0.3f * sz));
        int fx = cx - fw / 2, fy = cy - fh / 2, bx = cx - bw / 2, by = cy - bh / 2;
        vg_frame(fx, fy, fw, fh, 2, TE_MID);
        vg_frame(bx, by, bw, bh, 2, TE_INK);
        vg_line((float)fx, (float)fy, (float)bx, (float)by, 1.5f, TE_MID);
        vg_line((float)(fx + fw), (float)fy, (float)(bx + bw), (float)by, 1.5f, TE_MID);
        vg_line((float)fx, (float)(fy + fh), (float)bx, (float)(by + bh), 1.5f, TE_MID);
        vg_line((float)(fx + fw), (float)(fy + fh), (float)(bx + bw), (float)(by + bh), 1.5f, TE_MID);
        for (i = 0; i < 6 + (int)((1.0f - dm) * 10.0f); i++) {   /* (the reflections) */
            float a = te_rnd((uint32_t)i * 7u) + t * 0.05f, r = (float)(fw / 2) * (0.3f + 0.7f * te_wrap(t * 0.4f + te_rnd((uint32_t)i)));
            vg_circle((float)cx + vd_sin(a + 0.25f) * r, (float)cy + vd_sin(a) * r * 0.5f, 1.5f + lv * 2.0f,
                      vg_mix(C_RED, TE_INK, 1.0f - r / (float)(fw / 2)));
        }
    } else if (ty == 1) {
        int n = 10 + (int)(sz * 14.0f), px = x + 20, py = cy;
        for (i = 0; i <= n * 8; i++) {
            float u = (float)i / (float)(n * 8), wob = lv * 4.0f * vd_sin(t * 9.0f + u * 6.0f);
            int nx = x + 20 + (int)(u * (float)(w - 40)) + (int)(vd_sin(u * (float)n + 0.25f) * 6.0f);
            int ny = cy + (int)((vd_sin(u * (float)n) * (float)(h / 3)) + wob);
            vg_line((float)px, (float)py, (float)nx, (float)ny, 2.0f, vd_sin(u * (float)n + 0.25f) > 0 ? TE_INK : TE_MID);
            px = nx, py = ny;
        }
        vg_rect(x + 10, cy - h / 3 - 4, 8, 2 * h / 3 + 8, TE_MID);
        vg_rect(x + w - 18, cy - h / 3 - 4, 8, 2 * h / 3 + 8, TE_MID);
    } else {
        for (i = 0; i < 6; i++) {                 /* (arches, smaller as they go back) */
            float f = 1.0f - (float)i * (0.12f - 0.06f * sz), r = (float)(h - 14) * 0.9f * f;
            int k, px2 = 0, py2 = 0;
            uint32_t c = vg_mix(TE_INK, TE_LOW, (float)i / 6.0f * (0.5f + dm * 0.5f));
            if (i == (int)(te_wrap(t * 0.6f) * 6.0f) && lv > 0.05f)
                c = C_RED;
            for (k = 0; k <= 16; k++) {
                float a = (float)k / 32.0f;
                int nx = cx + (int)(vd_sin(a + 0.25f) * r * 1.6f), ny = y + h - 4 - (int)(vd_sin(a) * r);
                if (k)
                    vg_line((float)px2, (float)py2, (float)nx, (float)ny, 2.0f, c);
                px2 = nx, py2 = ny;
            }
        }
        vg_rect(x + 8, y + h - 4, w - 16, 2, TE_MID);
    }
    te_label(N_RTYPE[ty], x + 6, y + 4, 1);
}

/* CHORUS: three voices of the same wave, drifting apart as deep as CDP, as fast as CRT */
static void te_chorus(int x, int y, int w, int h)
{
    float r = te_g(G_CRATE), d = te_g(G_CDEPTH), t = te_t();
    int q, i, cy = y + h / 2;
    te_dots(x, y, w, h, 8);
    for (q = 0; q < 3; q++) {
        int px = x, py = cy;
        float off = (float)(q - 1) * d * 0.22f * vd_sin(t * (0.2f + r * 2.0f) + (float)q);
        for (i = 1; i <= 120; i++) {
            float u = (float)i / 120.0f;
            int nx = x + (int)(u * (float)w), ny = cy - (int)(vd_sin(u * 3.0f + off + t * 0.3f) * (float)(h / 2 - 10));
            vg_line((float)px, (float)py, (float)nx, (float)ny, q == 1 ? 2.5f : 1.5f, q == 1 ? TE_INK : q ? C_RED : TE_MID);
            px = nx, py = ny;
        }
    }
}

/* SLICER: the pattern's sixteen steps, the closed ones as deep as DEPTH, the step playing lit */
static void te_slicer(int x, int y, int w, int h, const track_t *t)
{
    uint32_t pat = SL_PAT[(uint32_t)(clamp(t->p[P_SLPAT], 1, SL_NPAT) - 1) % SL_NPAT], i;
    int on = t->p[P_SLCR] > 0, cur = (int)te_pos16() % 16, cw = (w - 15 * 4) / 16;
    float dp = (float)clamp(t->p[P_SLDEPTH], 0, 127) / 127.0f;
    for (i = 0; i < 16u; i++) {
        int live = (pat >> i) & 1u, bx = x + (int)i * (cw + 4), bh = live ? h - 16 : (int)((float)(h - 16) * (1.0f - dp));
        uint32_t c = !on ? TE_LOW : (int)i == cur && song.playing ? TE_INK : live ? C_RED : 0x5A1008;
        vg_rect(bx, y + 4 + (h - 16 - bh), cw, bh < 2 ? 2 : bh, c);
        if (i % 4u == 0u)
            vg_rect(bx, y + h - 8, cw, 2, TE_MID);
    }
}

/* CLOCK: the four beats as lamps, the sixteenths under them pushed by SWING, a pendulum keeping time */
static void te_clock(int x, int y, int w, int h)
{
    float p = te_pos16(), sw = (float)clamp(song.g[G_SWING], 0, 100) / 100.0f;
    int b, i, lw = (w - 140) / 4, cx = x + w - 60, cy = y + h - 8;
    for (b = 0; b < 4; b++) {
        int on = song.playing && (int)p / 4 == b;
        vg_rect(x + b * lw + 4, y + 6, lw - 8, h / 2 - 8, on ? (b ? TE_INK : C_RED) : 0x1E1E22);
        vg_frame(x + b * lw + 4, y + 6, lw - 8, h / 2 - 8, 1, TE_LOW);
    }
    for (i = 0; i < 16; i++) {
        int sx = x + 4 + i * (4 * lw - 8) / 16 + ((i & 1) ? (int)(sw * (float)((4 * lw - 8) / 32)) : 0);
        vg_rect(sx, y + h / 2 + 10, 6, h / 2 - 20, song.playing && (int)p == i ? TE_INK : (i & 1) ? 0x5A1008 : TE_MID);
    }
    {   /* the pendulum */
        float a = song.playing ? vd_sin(p / 8.0f) * 0.08f : 0.0f;
        vg_tri((float)(cx - 34), (float)cy, (float)(cx + 34), (float)cy, (float)cx, (float)(y + 6), 0x1E1E22);
        vg_line((float)cx, (float)cy - 6.0f, (float)cx + vd_sin(a) * 80.0f, (float)cy - 6.0f - vd_sin(a + 0.25f) * 80.0f, 3.0f, TE_INK);
        vg_rect(cx + (int)(vd_sin(a) * 52.0f) - 6, cy - 6 - (int)(vd_sin(a + 0.25f) * 52.0f) - 4, 12, 8, C_RED);
    }
}

/* ENV DEST, LFO DEST: the source on the left, a wire to each place it goes, as thick as how much, red up, white down */
static void te_dest(int x, int y, int w, int h, const page_t *p, const track_t *t, const char *src)
{
    static const char *const ENVN[4] = {"FILTER", "PITCH", "SHAPE", "SYNC"};
    static const char *const LFON[4] = {"PITCH", "FILTER", "SHAPE", "LEVEL"};
    int k, sx = x + 70, sy = y + h / 2, nd = src[0] == 'E' ? 3 : 4;
    float ph = te_wrap(te_t() * 0.8f);
    vg_rect(x + 4, sy - 16, 60, 32, 0x2A2A30);
    vg_text(src, x + 34, sy - 8, VG_BOLD, TE_INK, VG_CENTER);
    for (k = 0; k < nd; k++) {
        int v = t->p[p->id[k]], dy = y + 8 + k * (h - 16) / nd + (h - 16) / nd / 2, dx = x + w - 120;
        float a = (float)(v < 0 ? -v : v) / 64.0f;
        uint32_t c = !v ? TE_LOW : v > 0 ? C_RED : TE_INK;
        vg_line((float)sx, (float)sy, (float)dx, (float)dy, 1.0f + 4.0f * vd_minf(a, 1.0f), c);
        if (v)
            vg_circle((float)sx + (float)(dx - sx) * ph, (float)sy + (float)(dy - sy) * ph, 3.0f, TE_INK);
        vg_rect(dx, dy - 10, 116, 20, 0x1E1E22);
        vg_text(src[0] == 'E' ? ENVN[k] : LFON[k], dx + 6, dy - 8, VG_BOLD, v ? TE_INK : 0x9A9AA4, VG_LEFT);
    }
    vg_rect(sx - 4, sy - 4, 8, 8, TE_INK);
}

/* LANES: the drum lanes' levels as faders, each with its glyph */
static void te_lanes(int x, int y, int w, int h, const page_t *p, const track_t *t, int first)
{
    int k, n = 4, fw = w / 8;
    for (k = 0; k < n; k++) {
        int fx = x + (first ? 0 : 4 * fw) + k * fw * 2 + fw / 2, v = clamp(t->p[p->id[k]], 0, 127);
        int fh = h - 30, ky = y + 4 + fh - v * fh / 127;
        vg_rect(fx + fw / 2 - 1, y + 4, 3, fh, TE_LOW);
        vg_rect(fx + 2, ky - 4, fw - 4, 9, TE_INK);
        kx_draw_glyph(fx + fw / 2 - 8, y + h - 22, 16, (uint32_t)((first ? 0 : 4) + k), 0.6f, C_RED);
    }
}

/* VOICE pages: the voices as lamps (POLY many, MONO one, LEG one tied, UNI stacked), glide as the slide between two
 * notes, detune as a doubled line, pan on its line */
static void te_voice(int x, int y, int w, int h, const track_t *t)
{
    int md = clamp(t->p[P_VOICE], 0, 3), k, n = md == 0 ? 6 : md == 3 ? 4 : 1;
    float gl = (float)clamp(t->p[P_GLIDE], 0, 127) / 127.0f, dt = (float)clamp(t->p[P_DETUNE], 0, 127) / 127.0f;
    int pan = clamp(t->p[P_PAN], -64, 63);
    for (k = 0; k < 6; k++) {
        int on = k < n;
        vg_rect(x + 6 + k * 22, y + 8, 16, md == 3 && on ? 30 : 16, on ? (k ? TE_INK : C_RED) : 0x1E1E22);
    }
    te_label(N_VOICE[md], x + 6, y + 44, 1);
    {   /* glide: from a low note to a high one */
        int gx = x + 150, gw = 150, lo = y + h - 16, hi = y + 18, s = (int)(gl * (float)(gw - 40));
        vg_rect(gx, lo, 20, 4, TE_MID);
        vg_line((float)(gx + 20), (float)lo, (float)(gx + 20 + s), (float)hi, 2.0f, md == 2 ? C_RED : TE_INK);
        vg_rect(gx + 20 + s, hi, gw - 20 - s, 4, TE_INK);
        te_label("GLIDE", gx, y + 2, gl > 0);
    }
    {   /* detune and pan */
        int dx = x + 330, dw = w - 334, py = y + h - 14, k2;
        for (k2 = 0; k2 < 2; k2++) {
            int px = dx, pyy = y + 30, i;
            for (i = 1; i <= 40; i++) {
                float u = (float)i / 40.0f;
                int nx = dx + (int)(u * (float)dw), ny = y + 30 - (int)(vd_sin(u * (2.0f + (float)k2 * dt * 0.6f)) * 12.0f);
                vg_line((float)px, (float)pyy, (float)nx, (float)ny, 1.5f, k2 ? C_RED : TE_INK);
                px = nx, pyy = ny;
            }
        }
        vg_rect(dx, py, dw, 1, TE_MID);
        vg_rect(dx + (pan + 64) * (dw - 6) / 127, py - 5, 6, 11, C_RED);
        te_label("L", dx - 10, py - 8, 1);
        te_label("R", dx + dw + 3, py - 8, 1);
    }
}

static int te_page(int pg, int x, int y, int w, int h)
{
    const page_t *p = &PAGES[pg];
    const track_t *t = TSEL;
    float lv = te_lvl();
    if (te_is(p->title, "DLY"))
        te_delay(x, y, w, h);
    else if (te_is(p->title, "REVERB"))
        te_reverb(x, y, w, h);
    else if (te_is(p->title, "CHORUS"))
        te_chorus(x, y, w, h);
    else if (te_is(p->title, "SLICER"))
        te_slicer(x + 4, y + 4, w - 8, h - 6, t);
    else if (te_is(p->title, "CLOCK"))
        te_clock(x, y, w, h);
    else if (te_is(p->title, "DAMAGE")) {           /* the master's wear: the tape, the wall, the grain */
        int cw = (w - 16) / 3;
        te_tape(x, y + 14, cw, h - 18, te_g(G_DMG), te_g(G_DMGC), lv);
        te_wall(x + cw + 8, y + 14, cw, h - 18, te_g(G_WALL), lv);
        vg_rect(x + 2 * cw + 16, y + 14, cw, h - 18, 0x0E0E10);
        te_hiss(x + 2 * cw + 16, y + 14, cw, h - 18, te_g(G_HISS));
        te_label("DAMAGE / CHAR", x, y, 1);
        te_label("WALL", x + cw + 8, y, 1);
        te_label("HISS", x + 2 * cw + 16, y, 1);
    } else if (te_is(p->title, "RESONATE"))
        te_strings(x + 6, y + 2, w - 12, h - 4, te_g(G_RES), lv, 1);
    else if (te_is(p->title, "SPLICE"))
        te_splice(x, y + 4, w, h - 8, te_g(G_SPLICE));
    else if (te_is(p->title, "HAUNT"))
        te_haunt(x + 4, y + 2, w - 8, h - 4, clamp(song.g[G_HAUNT], 0, 2), te_g(G_HAMT));
    else if (te_is(p->title, "ENV DEST"))
        te_dest(x, y, w, h, p, t, "ENV");
    else if (te_is(p->title, "LFO DEST"))
        te_dest(x, y, w, h, p, t, "LFO");
    else if (te_is(p->title, "LANES"))
        te_lanes(x + 8, y, w - 16, h, p, t, 1);
    else if (te_is(p->title, "LANES 2"))
        te_lanes(x + 8, y, w - 16, h, p, t, 0);
    else if (te_is(p->title, "VOICE") || te_is(p->title, "VOICE 2") || te_is(p->title, "VOICE 3"))
        te_voice(x + 4, y + 2, w - 8, h - 4, t);
    else
        return 0;
    return 1;
}
