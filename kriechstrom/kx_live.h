/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: what moves on the pages because the sound moves (the control flow, reading the core):
 * the master's spectrum, where the envelope and the LFO are now, the playhead's trail, the signal along the master's
 * chain, peak holds; and the slide from one page to the next. */

/* ---- the master's spectrum: 28 bands, 40 Hz .. 10 kHz, from the scope's last 1024 samples (22 kHz: every other one) */
#define SP_N 28
static float sp_lvl[SP_N], sp_peak[SP_N];
static float sp_coef[SP_N];
static int sp_ready;
static void kx_spectrum(void)
{
    uint32_t b, i, sw = kx_scope_w;
    if (!sp_ready) {
        for (b = 0; b < SP_N; b++) {
            float f = 40.0f * vd_exp2((float)b / (float)(SP_N - 1) * 7.97f);   /* 40 Hz .. 10 kHz */
            sp_coef[b] = 2.0f * vd_sin(f / 22050.0f + 0.25f);                  /* 2 cos(2 pi f / fs) */
        }
        sp_ready = 1;
    }
    for (b = 0; b < SP_N; b++) {                  /* Goertzel, one band each, a Hann window */
        float s1 = 0.0f, s2 = 0.0f, p, db;
        uint32_t n = b < 8u ? 1024u : b < 16u ? 512u : 256u;
        for (i = 0; i < n; i++) {
            float wv = 0.5f - 0.5f * vd_sin((float)i / (float)n + 0.25f);
            float x = (float)kx_scope[(sw - n + i) & (KX_SCOPE - 1u)] * (1.0f / 32768.0f) * wv;
            float s0 = x + sp_coef[b] * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        p = s1 * s1 + s2 * s2 - sp_coef[b] * s1 * s2;
        db = 10.0f * vd_log2(p / (float)(n * n) + 1e-9f) * 0.30103f;           /* (log2 -> log10) */
        db = (db + 70.0f) / 60.0f;
        db = db < 0.0f ? 0.0f : db > 1.0f ? 1.0f : db;
        sp_lvl[b] = db > sp_lvl[b] ? db : sp_lvl[b] * 0.82f + db * 0.18f;
        sp_peak[b] = sp_lvl[b] > sp_peak[b] ? sp_lvl[b] : sp_peak[b] - 0.012f;
    }
}
static void kx_draw_spectrum(int x, int y, int w, int h, uint32_t col)
{
    int b, bw = w / SP_N;
    kx_spectrum();
    for (b = 0; b < SP_N; b++) {
        int bh = (int)(sp_lvl[b] * (float)h), ph = (int)(sp_peak[b] * (float)h), j, seg = 4;
        for (j = 0; j < bh; j += seg)             /* (in segments, as a rack meter) */
            vg_rect(x + b * bw, y + h - j - seg + 1, bw - 2, seg - 1,
                    vg_mix(vg_mix(C_BG, col, 0.5f), j > h * 3 / 4 ? C_WHITE : col, (float)j / (float)h));
        if (ph > 2)
            vg_rect(x + b * bw, y + h - ph, bw - 2, 2, C_WHITE);
    }
}

/* ---- where the selected track's newest voice is on its envelope (x along the shape kx_draw_adsr drew, y its level) */
static int kx_env_pos(const track_t *t, int x, int y, int w, int h, float *px, float *py)
{
    const voice_t *v = 0;
    uint32_t i;
    float seg = (float)w / 4.2f, a = (float)t->p[P_ATK], d = (float)t->p[P_DEC], r = (float)t->p[P_REL], s = (float)t->p[P_SUS];
    float xa = (float)x + seg * a / 127.0f + 2.0f, xd = xa + seg * d / 127.0f, xs = xd + seg * 0.9f, xr = xs + seg * r / 127.0f;
    float top = (float)y + 6.0f, bot = (float)(y + h - 2), lv, ys = bot - (s / 127.0f) * (bot - top);
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage && (!v || t->v[i].age > v->age))
            v = &t->v[i];
    if (!v)
        return 0;
    lv = (float)v->env / 16777216.0f;
    lv = lv < 0.0f ? 0.0f : lv > 1.0f ? 1.0f : lv;
    *py = bot - lv * (bot - top);
    if (v->stage == 1u)
        *px = (float)x + (xa - (float)x) * lv;
    else if (v->stage == 2u)
        *px = s >= 126.0f ? xd : xa + (xd - xa) * ((1.0f - lv) / (1.0f - s / 127.0f + 0.001f) > 1.0f ? 1.0f :
              (1.0f - lv) / (1.0f - s / 127.0f + 0.001f));
    else
        *px = xs + (xr - xs) * (1.0f - lv / (s / 127.0f + 0.001f) < 0.0f ? 0.0f : 1.0f - lv / (s / 127.0f + 0.001f) > 1.0f ? 1.0f :
              1.0f - lv / (s / 127.0f + 0.001f));
    if (v->stage == 2u && *px > xd)
        *px = xd + (xs - xd) * 0.5f;
    return 1;
}
static void kx_draw_dot(float x, float y, uint32_t col)
{
    vg_circle(x, y, 11.0f, vg_mix(C_BG, col, 0.18f));
    vg_circle(x, y, 7.0f, vg_mix(C_BG, col, 0.45f));
    vg_circle(x, y, 4.0f, C_WHITE);
}

/* ---- the slide between pages: the old page out to one side, the new one in, 0.22 s, the body only (y 76 .. 284) */
#define TR_Y 76
#define TR_H 208
static uint16_t kx_last[480 * TR_H], kx_was[480 * TR_H];
static uint64_t kx_tr_t;
static int kx_tr_dir, kx_tr_page = -1;
static void kx_transition(uint16_t *fb, int page, int order)
{
    float t;
    int y;
    if (page != kx_tr_page) {
        if (kx_tr_page >= 0 && kx_ic[IC_SCREEN]) {
            memcpy(kx_was, kx_last, sizeof kx_was);
            kx_tr_t = vibe_frames() ? vibe_frames() : 1;
            kx_tr_dir = order;
        }
        kx_tr_page = page;
    }
    memcpy(kx_last, fb + TR_Y * 480, sizeof kx_last);   /* (the new page as drawn, for the next change) */
    if (!kx_tr_t)
        return;
    t = (float)(vibe_frames() - kx_tr_t) / (kx_sr * 0.22f);
    if (t >= 1.0f) {
        kx_tr_t = 0;
        return;
    }
    t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);    /* (ease out) */
    {
        int s = (int)((1.0f - t) * 480.0f);             /* how much of the old page is still there */
        for (y = 0; y < TR_H; y++) {
            uint16_t *r = fb + (TR_Y + y) * 480;
            const uint16_t *o = kx_was + y * 480, *n = kx_last + y * 480;
            if (kx_tr_dir >= 0) {                       /* forward: the old one leaves to the left */
                memcpy(r, o + (480 - s), (size_t)s * 2u);
                memcpy(r + s, n, (size_t)(480 - s) * 2u);
            } else {
                memcpy(r, n + s, (size_t)(480 - s) * 2u);
                memcpy(r + (480 - s), o, (size_t)s * 2u);
            }
            if (s > 0 && s < 480)
                r[kx_tr_dir >= 0 ? s : 480 - s] = 0xF800u;   /* (the seam, red) */
        }
    }
}
