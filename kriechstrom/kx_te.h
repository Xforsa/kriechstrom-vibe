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
    int b, top = y + 4, bot = y + h - 4, playing = song.playing;
    uint32_t c = mode ? C_RED : TE_LOW;
    for (b = 0; b <= 4; b++)
        vg_rect(x + b * (w - 1) / 4, top, 1, bot - top, b == 0 || b == 4 ? TE_MID : TE_LOW);
    if (mode == 1) {
        /* SUCK: a reversed swell rising into every SNARE and CLAP of the drum track's bar, cut dead on the hit. Each
         * swell fills as the playhead comes to it and flashes where it is cut */
        int dt = kx_drum_track(), s, any = 0;
        float len = (0.08f + 0.17f * a);          /* (how long a swell is, in bars) */
        uint32_t base = dt >= 0 ? trk[dt].seq_idx / 16u * 16u : 0u;
        for (s = 0; s < 16; s++) {
            int hit = dt >= 0 ? (int)((step_lanes(&trk[dt].step[(base + (uint32_t)s) % NSTEP]) >> 1) & 3u) != 0
                              : (s == 4 || s == 12);
            float e, st, f;
            int ex, sx, hh = bot - top - 4, fx;
            if (!hit)
                continue;
            any = 1;
            e = (float)s / 16.0f;
            st = e - len;
            ex = x + (int)(e * (float)(w - 1)), sx = x + (int)((st < 0.0f ? 0.0f : st) * (float)(w - 1));
            /* the outline, then what has risen so far */
            vg_line((float)sx, (float)bot, (float)ex, (float)(bot - hh), 1.0f, vg_mix(C_BG, c, 0.5f));
            vg_rect(ex - 1, top, 3, bot - top, vg_mix(C_BG, TE_INK, 0.4f));
            f = playing && p >= st && p < e ? (p - st) / (e - st) : 0.0f;
            if (f > 0.0f) {
                fx = sx + (int)(f * (float)(ex - sx));
                vg_tri((float)sx, (float)bot, (float)fx, (float)bot, (float)fx, (float)bot - f * (float)hh, c);
            }
            if (playing && p >= e && p < e + 0.03f)   /* (cut dead: a flash) */
                vg_rect(ex - 3, top, 7, bot - top, TE_INK);
        }
        if (!any && h > 60)
            vg_text("NO SNARE OR CLAP IN THIS BAR", x + w / 2, y + h / 2 - 8, VG_SMALL, TE_MID, VG_CENTER);
    } else {
        /* DRAG: the tape's speed, falling over the bar's last beat; a dot rides it */
        int px = x, py = top + 2, i;
        float depth = mode ? 0.3f + 0.7f * a : 0.0f;
        for (i = 1; i <= 40; i++) {
            float u = (float)i / 40.0f, d = u < 0.75f ? 0.0f : (u - 0.75f) / 0.25f;
            int nx = x + (int)(u * (float)(w - 1)), ny = top + 2 + (int)(d * d * (float)(bot - top - 4) * depth);
            vg_line((float)px, (float)py, (float)nx, (float)ny, 2.0f, c);
            px = nx, py = ny;
        }
        if (playing && mode) {
            float d = p < 0.75f ? 0.0f : (p - 0.75f) / 0.25f;
            vg_circle((float)x + p * (float)(w - 1), (float)(top + 2) + d * d * (float)(bot - top - 4) * depth, 5.0f, TE_INK);
        }
    }
    if (playing)
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
        int fx = x + k * fw * 2 + fw / 2, v = clamp(t->p[p->id[k]], 0, 127);
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


/* MOD: the four slots, each a source wired to a destination, as much as AMT; the slot KNOB 1 picks framed */
static void te_mod(int x, int y, int w, int h, const track_t *t)
{
    int k, rh = h / 4;
    for (k = 0; k < 4; k++) {
        int ry = y + k * rh + 2, src = clamp(t->p[P_M1SRC + 3 * k], 0, (int)NELEM(N_MSRC) - 1);
        int dst = clamp(t->p[P_M1DST + 3 * k], 0, (int)NELEM(N_MDST) - 1), amt = t->p[P_M1AMT + 3 * k];
        int on = src && dst, cx = x + 120, cw = w - 240, a = amt < 0 ? -amt : amt;
        char b[4] = {(char)('1' + k), 0};
        if (k == (int)mod_ui_slot)
            vg_frame(x, ry - 1, w, rh - 2, 1, C_RED);
        vg_text(b, x + 6, ry + 4, VG_BOLD, k == (int)mod_ui_slot ? C_RED : TE_MID, VG_LEFT);
        vg_text(N_MSRC[src], x + 26, ry + 4, VG_BOLD, on ? TE_INK : 0x7A7A84, VG_LEFT);
        vg_rect(cx, ry + rh / 2 - 1, cw, 2, on ? 0x3A3A40 : 0x1E1E22);
        if (on) {
            int bw = a * (cw / 2) / 64;
            vg_rect(amt >= 0 ? cx + cw / 2 : cx + cw / 2 - bw, ry + rh / 2 - 4, bw, 8, amt >= 0 ? C_RED : TE_INK);
            vg_circle((float)cx + (float)cw * te_wrap(te_t() * 0.5f + (float)k * 0.25f), (float)(ry + rh / 2), 2.5f, TE_INK);
        }
        vg_rect(cx + cw / 2, ry + 3, 1, rh - 8, TE_LOW);
        vg_text(N_MDST[dst], x + w - 8, ry + 4, VG_BOLD, on ? TE_INK : 0x7A7A84, VG_RIGHT);
    }
}

/* ARP 2: eight notes of the arpeggio, the off-beats pushed by SWING, each as solid as its CHANCE */
static void te_arp2(int x, int y, int w, int h, const track_t *t)
{
    float sw = (float)clamp(t->p[P_ASWING], 0, 100) / 100.0f, pr = (float)clamp(t->p[P_APROB], 0, 100) / 100.0f;
    int k, cw = w / 8, cur = (int)(te_pos16() * 0.5f) % 8;
    te_dots(x, y, w, h, 8);
    for (k = 0; k < 8; k++) {
        int bx = x + k * cw + 4 + ((k & 1) ? (int)(sw * (float)(cw / 2)) : 0), bh = 16 + ((k * 5) % 4) * 14;
        int keep = te_rnd((uint32_t)(k * 13 + (int)(te_t() * 0.5f))) < pr;
        uint32_t c = song.playing && k == cur ? TE_INK : keep ? C_RED : 0x3A1008;
        vg_rect(bx, y + h - 10 - bh, cw - 12, bh, c);
        if (!keep)
            vg_frame(bx, y + h - 10 - bh, cw - 12, bh, 1, C_RED);
    }
    vg_rect(x, y + h - 6, w, 1, TE_MID);
    if (t->p[P_AHOLD])
        te_label("HOLD", x + 4, y + 2, 1);
}

/* CHORD: the chord the keys play, on an octave of keys: the last one played lit, its root red */
static void te_chord(int x, int y, int w, int h, const track_t *t)
{
    static const uint8_t BLK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t tr = trk_index(t) % NTRK, mask = chord_last[tr].mask, k, wi = 0;
    int kw = (w - 160) / 7, kx0 = x + 150, ty = y + 4, kh = h - 10;
    vg_text(N_CHRD[clamp(t->p[P_CHRD], 0, (int)NELEM(N_CHRD) - 1)], x + 4, y + 10, VG_BIG, t->p[P_CHRD] ? TE_INK : 0x7A7A84, VG_LEFT);
    vg_text(N_VOIC[clamp(t->p[P_VOIC], 0, (int)NELEM(N_VOIC) - 1)], x + 4, y + 44, VG_BOLD, TE_MID, VG_LEFT);
    if (!t->p[P_CHRD])
        mask = 1u;
    for (k = 0; k < 12u; k++)                     /* the white keys first */
        if (!BLK[k]) {
            int on = (mask >> k) & 1u;
            vg_rect(kx0 + (int)wi * kw, ty, kw - 3, kh, on ? (k ? TE_INK : C_RED) : 0x2A2A30);
            wi++;
        }
    for (k = 0, wi = 0; k < 12u; k++) {
        if (!BLK[k]) {
            wi++;
            continue;
        }
        vg_rect(kx0 + (int)wi * kw - kw / 3 - 2, ty, kw * 2 / 3, kh * 6 / 10, (mask >> k) & 1u ? C_RED : 0x0E0E10);
    }
}

/* what turns the page's effect on, when it is off: a plate over the picture */
static void te_off(int x, int y, int w, int h, const char *how)
{
    int tw = vg_text_width(how, VG_BOLD) + 24;
    vg_rect(x + w / 2 - tw / 2, y + h / 2 - 13, tw, 26, 0x000000);
    vg_frame(x + w / 2 - tw / 2, y + h / 2 - 13, tw, 26, 1, TE_MID);
    vg_text(how, x + w / 2, y + h / 2 - 8, VG_BOLD, TE_INK, VG_CENTER);
}
static int te_sends(uint32_t id)                  /* any track sending to that effect */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (trk[k].p[id] > 0)
            return 1;
    return 0;
}
/* the page's effect off: how it goes on (0 if it is on, or the page has nothing to switch) */
static const char *te_off_how(const page_t *p, const track_t *t)
{
    if (te_is(p->title, "DLY"))
        return song.g[G_DMIX] ? (te_sends(P_DLY) ? 0 : "ON, BUT NO TRACK SENDS: FX > SENDS, DLY") : "OFF: ENC 4 (MIX) TURNS IT ON";
    if (te_is(p->title, "REVERB"))
        return te_sends(P_REV) ? 0 : "NO TRACK SENDS: FX > SENDS, REV";
    if (te_is(p->title, "CHORUS"))
        return te_sends(P_CHOR) ? 0 : "NO TRACK SENDS: FX > SENDS, CHO";
    if (te_is(p->title, "SLICER"))
        return t->p[P_SLCR] ? 0 : "OFF: ENC 1 (SLCR) TURNS IT ON";
    if (te_is(p->title, "DAMAGE"))
        return song.g[G_DMG] || song.g[G_WALL] || song.g[G_HISS] ? 0 : "OFF: TURN DMG, WALL OR HISS UP";
    if (te_is(p->title, "RESONATE"))
        return song.g[G_RES] ? 0 : "OFF: ENC 1 (AMT) TURNS IT ON";
    if (te_is(p->title, "SPLICE"))
        return song.g[G_SPLICE] ? 0 : "OFF: ENC 1 (AMT) TURNS IT ON";
    if (te_is(p->title, "HAUNT"))
        return song.g[G_HAUNT] ? 0 : "OFF: ENC 1 (MODE) SUCK OR DRAG";
    return 0;
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
    else if (te_is(p->title, "MOD"))
        te_mod(x + 4, y + 2, w - 8, h - 4, t);
    else if (te_is(p->title, "ARP 2"))
        te_arp2(x + 4, y + 2, w - 8, h - 4, t);
    else if (te_is(p->title, "CHORD"))
        te_chord(x + 4, y + 2, w - 8, h - 4, t);
    else
        return 0;
    {
        const char *how = te_off_how(p, t);
        if (how)
            te_off(x, y, w, h, how);
    }
    return 1;
}

/* ---- the cut: the picture torn in bands and slid, one band burnt red, like a tape spliced wrong. On a page change
 * (as long as SCREEN WEAR is not OFF) and in the power-on picture; a few frames, then clean */
static void te_tear(uint16_t *fb, float amt, uint32_t seed)
{
    int b, n = 2 + (int)(amt * 5.0f);
    static uint16_t row[480];
    for (b = 0; b < n; b++) {
        uint32_t r = te_hash(seed * 977u + (uint32_t)b * 131u);
        int y0 = 24 + (int)(r % 260u), hh = 2 + (int)((r >> 9) % 12u), sh = (int)((float)((int)((r >> 14) % 81u) - 40) * amt), y;
        for (y = y0; y < y0 + hh && y < 320; y++) {
            uint16_t *p = fb + y * 480;
            int x;
            for (x = 0; x < 480; x++)
                row[x] = p[((x - sh) % 480 + 480) % 480];
            if (b == 0)                              /* (the burnt band: red, the rest of it dark) */
                for (x = 0; x < 480; x++)
                    row[x] = (uint16_t)((row[x] & 0xF800u) | 0x7800u | ((row[x] >> 2) & 0x01E7u));
            memcpy(p, row, sizeof row);
        }
    }
}

/* the page's number, as on a machine's panel: a small white plate before the title */
static int te_index(int x, int y, int n)
{
    char b[4];
    b[0] = (char)('0' + (n / 10) % 10), b[1] = (char)('0' + n % 10), b[2] = 0;
    vg_rect(x, y + 1, 20, 15, TE_INK);
    vg_text(b, x + 10, y + 1, VG_BOLD, 0x000000, VG_CENTER);
    return 26;
}

/* crop marks at the corners of a picture */
static void te_marks(int x, int y, int w, int h)
{
    vg_rect(x, y, 6, 1, TE_MID), vg_rect(x, y, 1, 6, TE_MID);
    vg_rect(x + w - 6, y, 6, 1, TE_MID), vg_rect(x + w - 1, y, 1, 6, TE_MID);
    vg_rect(x, y + h - 1, 6, 1, TE_MID), vg_rect(x, y + h - 6, 1, 6, TE_MID);
    vg_rect(x + w - 6, y + h - 1, 6, 1, TE_MID), vg_rect(x + w - 1, y + h - 6, 1, 6, TE_MID);
}
