/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the stage (SDK 23), 1280 x 720, the largest a host shows. The picture first: the four
 * tracks as four rings of steps round the centre, a hand sweeping each; in the middle the figure the drums grow and the
 * master's sound as a ring; the discharges behind it all. Words only in their own bands: the tracks on the left, the
 * master's chain on the right, the pads' effects below. Each part is one of the base controls as well:
 *   a step          the SEQ page's pads (K6), or ENTER on its cursor; here a click on the ring
 *   a track         F1..F4 (select), the pads 1-4 (mute: a click on its M)
 *   a knob          a drag up / down or the wheel; DAMAGE = encoder B, WALL = SHIFT + B, the rest FX > MASTER (K5)
 *   an effect       pads 9-16 held (MENU > PAD FX); here held with the mouse, or with fingers, several at once */

#define ST_W 1280
#define ST_H 720
static uint16_t kx_stage_fb[ST_W * ST_H];
static int kx_stage_on;

enum { SK_IN, SK_DMG, SK_CHAR, SK_WALL, SK_RES, SK_SPLICE, SK_HAUNT, SK_HISS, SK_N };
static const char *const SK_NAME[SK_N] = {"INPUT", "DAMAGE", "CHAR", "WALL", "RESONATE", "SPLICE", "HAUNT", "HISS"};
static const char *const SK_HOW[SK_N] = {"FX > INPUT", "ENCODER B", "FX > MASTER", "SHIFT + B", "FX > RESONATE",
                                         "FX > SPLICE", "FX > HAUNT", "FX > MASTER"};
static int sk_value(int k)
{
    switch (k) {
    case SK_IN: return kx_ic[IC_LEVEL];
    case SK_DMG: return song.g[G_DMG];
    case SK_CHAR: return song.g[G_DMGC];
    case SK_WALL: return song.g[G_WALL];
    case SK_RES: return song.g[G_RES];
    case SK_SPLICE: return song.g[G_SPLICE];
    case SK_HAUNT: return song.g[G_HAUNT] ? song.g[G_HAMT] : 0;
    default: return song.g[G_HISS];
    }
}
static void sk_turn(int k, int d)                 /* a knob of the stage moved d (command to the audio flow) */
{
    static const uint8_t G[SK_N] = {0, G_DMG, G_DMGC, G_WALL, G_RES, G_SPLICE, G_HAMT, G_HISS};
    if (k == SK_IN) {
        kx_post(KXC_INCFG, IC_LEVEL, 0, 0, d);
        return;
    }
    if (k == SK_HAUNT && !song.g[G_HAUNT] && d > 0)
        kx_post(KXC_PARAM, NTRK, G_HAUNT, 0, 1);
    kx_post(KXC_PARAM, NTRK, G[k], 0, clamp(song.g[G[k]] + d, GP[G[k]].min, GP[G[k]].max));
}

/* the layout: the rings' centre and sizes, the bands */
#define SR_CX 640
#define SR_CY 344
#define SR_R0 120                                 /* T1's ring from here .. */
#define SR_TH 38                                  /* .. this thick, the next 44 further out */
#define SR_STEP 44
#define SB_LW 330                                 /* the left band: the tracks */
#define SB_RX 952                                 /* the right band: the chain */
#define SB_BY 660                                 /* the bottom band: the effects */
#define SK_CW 152
#define SK_CH 132

static float st_pt_x(float a, float r) { return (float)SR_CX + vd_sin(a) * r; }   /* (a: turns from 12 o'clock) */
static float st_pt_y(float a, float r) { return (float)SR_CY - vd_sin(a + 0.25f) * r; }
static void st_arc(float a0, float a1, float r, float wd, uint32_t c)   /* a band of a ring from a0 to a1 */
{
    int n = 1 + (int)((a1 - a0) * 64.0f), i;
    float px = st_pt_x(a0, r), py = st_pt_y(a0, r);
    for (i = 1; i <= n; i++) {
        float a = a0 + (a1 - a0) * (float)i / (float)n, x = st_pt_x(a, r), y = st_pt_y(a, r);
        vg_line(px, py, x, y, wd, c);
        px = x, py = y;
    }
}
static void st_band(float a0, float a1, float ri, float ro, uint32_t c)   /* a sector of a ring, flat-ended */
{
    int n = 1 + (int)((a1 - a0) * 96.0f), i;
    for (i = 0; i < n; i++) {
        float b0 = a0 + (a1 - a0) * (float)i / (float)n, b1 = a0 + (a1 - a0) * (float)(i + 1) / (float)n;
        float x0 = st_pt_x(b0, ri), y0 = st_pt_y(b0, ri), x1 = st_pt_x(b0, ro), y1 = st_pt_y(b0, ro);
        float x2 = st_pt_x(b1, ro), y2 = st_pt_y(b1, ro), x3 = st_pt_x(b1, ri), y3 = st_pt_y(b1, ri);
        vg_tri(x0, y0, x1, y1, x2, y2, c);
        vg_tri(x0, y0, x2, y2, x3, y3, c);
    }
}
static float st_turns(float dx, float dy)        /* the angle of (dx, dy) in turns, clockwise from 12 o'clock */
{
    float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, a, r;
    if (ax < 1e-6f && ay < 1e-6f)
        return 0.0f;
    r = ax > ay ? ay / ax : ax / ay;              /* (atan on 0..1, then the octants) */
    a = r * (0.7853982f + 0.273f * (1.0f - r)) / 6.2831853f;
    if (ay > ax) a = 0.25f - a;                   /* a: from the x axis towards y, 0 .. 1/4 */
    /* the screen: y down; 12 o'clock is (0, -1), turning clockwise towards (1, 0) */
    if (dx >= 0 && dy < 0) return 0.25f - a;
    if (dx >= 0) return 0.25f + a;
    if (dy >= 0) return 0.75f - a;
    return 0.75f + a;
}

static void st_ring(uint32_t k)                   /* a track's steps round its ring */
{
    const track_t *t = &trk[k];
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, col = TRK_RGB[k], play = t->seq_idx % len;
    float r0 = (float)(SR_R0 + (int)k * SR_STEP), rm = r0 + (float)SR_TH * 0.5f, gap = 0.12f / (float)len;
    int drum = drum_track(t), lo = 127, hi = 0, mute = t->p[P_MUTE] != 0, sel = k == song.sel;
    for (i = 0; i < len; i++) {
        const step_t *st = &t->step[i];
        uint32_t j;
        if (st->time == ST_NOTE)
            for (j = 0; j < st->n; j++)
                lo = st->note[j] < lo ? st->note[j] : lo, hi = st->note[j] > hi ? st->note[j] : hi;
    }
    if (hi < lo) lo = 36, hi = 60;
    if (hi - lo < 12) hi = lo + 12;
    if (sel)                                      /* (the selected track: its ring edged in its colour) */
        st_arc(0.0f, 1.0f, r0 - 3.0f, 1.5f, vg_mix(C_BG, col, 0.6f)), st_arc(0.0f, 1.0f, r0 + SR_TH + 3.0f, 1.5f, vg_mix(C_BG, col, 0.6f));
    for (i = 0; i < len; i++) {                   /* the ground first, all of it: the beats as on a drum machine */
        float a0 = (float)i / (float)len + gap, a1 = (float)(i + 1u) / (float)len - gap;
        int on = song.playing && i == play;
        if (on)                                   /* (the ground is open: the rain and the scars show through; the step
                                                   * playing lit, each step's edge a tick, each beat's a longer one) */
            st_band(a0, a1, r0, r0 + (float)SR_TH, vg_mix(0x08080A, col, 0.3f));
        st_arc(a0, a1, r0, 1.0f, (i / 4u) % 2u ? 0x2A2A32 : 0x1E1E24);
        st_arc(a0, a1, r0 + (float)SR_TH, 1.0f, (i / 4u) % 2u ? 0x2A2A32 : 0x1E1E24);
        {
            float b = (float)i / (float)len, e = i % 4u ? 6.0f : (float)SR_TH;
            vg_line(st_pt_x(b, r0), st_pt_y(b, r0), st_pt_x(b, r0 + e), st_pt_y(b, r0 + e), 1.0f, i % 4u ? 0x2A2A32 : 0x3A3A44);
        }
    }
    for (i = 0; i < len; i++) {
        const step_t *st = &t->step[i];
        float a0 = (float)i / (float)len + gap, a1 = (float)(i + 1u) / (float)len - gap;
        int on = song.playing && i == play;
        uint32_t c = on ? C_WHITE : mute ? 0x3A3A40 : col;
        if (drum) {                               /* eight thin rings, KICK innermost */
            uint32_t l, hits = step_lanes(st);
            for (l = 0; l < NLANE; l++)
                if ((hits >> l) & 1u)
                    st_band(a0, a1, r0 + 2.0f + (float)l * 4.4f, r0 + 5.4f + (float)l * 4.4f, l < 2 ? c : vg_mix(C_BG, c, 0.75f));
        } else if (st->time == ST_NOTE && st->n) {
            uint32_t j;
            for (j = 0; j < st->n; j++) {
                float rr = r0 + 5.0f + (float)(st->note[j] - lo) / (float)(hi - lo) * (float)(SR_TH - 10);
                st_band(a0, a1, rr - 3.0f, rr + 3.0f, st->flags & SF_ACCENT && !on ? vg_mix(c, C_WHITE, 0.45f) : c);
            }
        } else if (st->time == ST_TIE)
            st_band(a0 - gap * 2.0f, a1, rm - 1.5f, rm + 1.5f, vg_mix(C_BG, c, 0.5f));
    }
    if (song.playing && sel) {                    /* the selected track's hand leaves a trail, fading, as a radar's */
        float a = ((float)play + 1.0f) / (float)len;
        int q;
        for (q = 0; q < 10; q++)
            st_band(a - (float)(q + 1) * 0.012f, a - (float)q * 0.012f, (float)SR_R0 - 8.0f, (float)(SR_R0 + 4 * SR_STEP),
                    vg_mix(0x08080A, col, 0.16f * (1.0f - (float)q / 10.0f)));
    }
    if (song.playing) {                           /* the hand: across the ring where it plays */
        float a = ((float)play + 0.5f) / (float)len;
        vg_line(st_pt_x(a, r0 - 4.0f), st_pt_y(a, r0 - 4.0f), st_pt_x(a, r0 + SR_TH + 4.0f), st_pt_y(a, r0 + SR_TH + 4.0f), 2.0f,
                mute ? 0x5A5A60 : vg_mix(col, C_WHITE, 0.6f));
    }
}

static void st_track_card(uint32_t k)            /* the left band: a track */
{
    const track_t *t = &trk[k];
    int y = 168 + (int)k * 120, mute = t->p[P_MUTE] != 0, lv;
    uint32_t col = TRK_RGB[k];
    char nm[17], n[2] = {(char)('1' + k), 0};
    vg_round(16, y, SB_LW - 28, 110, 6, k == song.sel ? vg_mix(0x141416, col, 0.16f) : 0x141416);
    vg_rect(16, y, 6, 110, mute ? 0x3A3A40 : col);
    vg_text(n, 36, y + 10, VG_BIG | VG_X2, mute ? 0x6A6A72 : col, VG_LEFT);
    vg_text(ENGINES[eng_idx(t->eng_req)]->name, 84, y + 14, VG_BOLD, mute ? 0x6A6A72 : TE_INK, VG_LEFT);
    sound_name(t, nm);
    vg_text(nm, 84, y + 36, VG_BIG, mute ? 0x6A6A72 : C_WHITE, VG_LEFT);
    {   /* its LEVEL, a fader to drag; the sound in it now */
        const param_desc_t *d = &TP[P_LEVEL];
        int fx = 84 + (int)((float)(t->p[P_LEVEL] - d->min) / (float)(d->max - d->min) * 166.0f);
        lv = (int)(vd_minf(kx_trk_pk[k] * 1.4f, 1.0f) * (float)(fx - 84));
        vg_rect(84, y + 82, 166, 8, 0x22222A);
        vg_rect(84, y + 82, fx - 84, 8, vg_mix(0x22222A, col, 0.35f));
        vg_rect(84, y + 82, lv, 8, mute ? 0x5A5A60 : col);
        vg_rect(fx - 2, y + 76, 4, 20, C_WHITE);
    }
    vg_round(SB_LW - 62, y + 8, 40, 26, 4, 0x2A2A30);      /* DICE: a new part */
    vg_text("DICE", SB_LW - 42, y + 13, VG_SMALL, TE_INK, VG_CENTER);
    vg_round(SB_LW - 62, y + 70, 40, 30, 4, mute ? C_RED : 0x2A2A30);
    vg_text("M", SB_LW - 42, y + 76, VG_BOLD, mute ? C_WHITE : TE_MID, VG_CENTER);
}

static void st_knob(int k)                        /* the right band: one of the chain */
{
    int x = SB_RX + 8 + (k & 1) * SK_CW, y = 24 + (k >> 1) * SK_CH, v = sk_value(k);
    float a = (float)clamp(v, 0, 127) / 127.0f, cx = (float)(x + SK_CW / 2 - 6), cy = (float)(y + 48);
    char b[8];
    int i, n = 30;
    vg_round(x, y, SK_CW - 12, SK_CH - 10, 8, vg_mix(0x141416, C_RED, a * 0.16f));
    for (i = 0; i < n; i++) {                     /* 270 degrees of ticks, lit as far as the value */
        float t = (float)i / (float)(n - 1), ang = 0.625f + t * 0.75f;
        vg_line(cx + vd_sin(ang) * 30.0f, cy - vd_sin(ang + 0.25f) * 30.0f, cx + vd_sin(ang) * 40.0f, cy - vd_sin(ang + 0.25f) * 40.0f,
                3.0f, t <= a && v ? (k == SK_IN ? C_WHITE : C_RED) : 0x2E2E34);
    }
    vg_itoa(v, b);
    vg_text(b, (int)cx, (int)cy - 10, VG_BIG, C_WHITE, VG_CENTER);
    vg_text(SK_NAME[k], (int)cx, y + 92, VG_BOLD, v ? TE_INK : 0x7A7A84, VG_CENTER);
}

static int st_fx_down[16];                        /* pads 9-16 held from the stage (by pointer: which one) */
static int st_shift;
static void st_pads(void)                         /* the bottom band: the pads' effects, held with the mouse */
{
    int i;
    for (i = 0; i < 8; i++) {
        int slot = i + (st_shift ? 8 : 0), x = SB_LW + 22 + i * 74, fx = clamp(kx_ic[IC_PF0 + slot], 0, KX_NFX - 1);
        int held = st_fx_down[slot];
        vg_round(x, SB_BY + 6, 68, 48, 6, held ? C_RED : 0x1A1A1E);
        vg_rect(x + 6, SB_BY + 6, 56, 2, st_shift ? 0xA0A0FF : i < 4 ? C_RED : 0xFF8030);
        vg_text(kx_fx_cell(fx), x + 34, SB_BY + 22, VG_BOLD, held ? C_WHITE : TE_INK, VG_CENTER);
    }
    vg_round(SB_LW - 64, SB_BY + 6, 64, 48, 6, st_shift ? 0x4A4A8A : 0x1A1A1E);
    vg_text("SHIFT", SB_LW - 32, SB_BY + 22, VG_BOLD, st_shift ? C_WHITE : TE_MID, VG_CENTER);
}

/* ---- the gloom: a ground made once (soot, rust, old scars of current, the corners dark), then each frame rain, ash
 * rising, smoke drifting, a shock ring from every kick, a sweep's fading trail ------------------------------------- */
static uint16_t st_bg[ST_W * ST_H];
static int st_bg_ok;
static lt_seg st_scar[1400];
static int st_scar_n;
static uint32_t st_h;
static uint32_t st_rnd(void) { st_h = st_h * 1664525u + 1013904223u; return st_h >> 8; }
static int st_rn(int n) { return (int)(st_rnd() % (uint32_t)n); }
static void st_bg_make(uint16_t *fb)
{
    int i, j;
    vg_begin_size(st_bg, ST_W, ST_H);
    vg_clear(0x08080A);
    st_h = 0x5EED2611u;
    for (i = 0; i < 46; i++)                      /* rust and soot, in blots */
        vg_circle((float)st_rn(ST_W), (float)st_rn(ST_H), 12.0f + (float)st_rn(70), (st_rnd() & 1u) ? 0x0E0A09 : 0x0B0B0D);
    for (i = 0; i < 9000; i++) {                  /* grain */
        uint32_t g = 0x0C0C0E + 0x010101u * (uint32_t)st_rn(10);
        vg_rect(st_rn(ST_W), st_rn(ST_H), 1, 1, g);
    }
    {   /* old scars of current, burnt into the ground, dark */
        int saved_n = st_scar_n = 0;
        lt_out = st_scar, lt_out_n = &st_scar_n, lt_out_cap = 1400;
        lt_rng = 0x2611u;
        lt_tree(-4.0f, 120.0f, 0.02f, 60.0f, 3.0f, 7, 0.07f);
        lt_tree(484.0f, 300.0f, 0.47f, 54.0f, 2.6f, 7, 0.07f);
        lt_tree(240.0f, 324.0f, -0.25f, 46.0f, 2.2f, 6, 0.07f);
        lt_tree(140.0f, -4.0f, 0.27f, 40.0f, 2.0f, 6, 0.07f);
        lt_tree(400.0f, -4.0f, 0.22f, 44.0f, 2.2f, 6, 0.07f);
        (void)saved_n;
        for (j = 0; j < st_scar_n; j++) {
            const lt_seg *g = &st_scar[j];
            float wd = (float)g->w * 0.25f * 2.2f;
            vg_line((float)g->x0 * 2.6667f, (float)g->y0 * 2.6667f - 66.0f, (float)g->x1 * 2.6667f, (float)g->y1 * 2.6667f - 66.0f,
                    wd * 3.5f + 3.0f, 0x160806);
            vg_line((float)g->x0 * 2.6667f, (float)g->y0 * 2.6667f - 66.0f, (float)g->x1 * 2.6667f, (float)g->y1 * 2.6667f - 66.0f,
                    wd, 0x48140C);
        }
    }
    for (i = 0; i < 90; i++) {                    /* the corners and edges fall into the dark */
        float a = (float)(90 - i) / 90.0f;
        vg_rect_a(0, i * 4, ST_W, 4, 0x000000, 0.0f + a * a * 0.6f);
        vg_rect_a(0, ST_H - (i + 1) * 4, ST_W, 4, 0x000000, a * a * 0.6f);
    }
    vg_begin_size(fb, ST_W, ST_H);
    st_bg_ok = 1;
}
static void st_weather(float t, float hit)       /* rain falling, ash rising, smoke drifting */
{
    int i;
    for (i = 0; i < 3; i++) {                     /* smoke: wide soft bands, drifting */
        float y = 120.0f + (float)i * 210.0f + vd_sin(t * 0.03f + (float)i * 0.3f) * 60.0f;
        vg_rect_a(0, (int)y, ST_W, 100, 0x3A3A44, 0.06f);
        vg_rect_a(0, (int)y + 22, ST_W, 56, 0x3A3A44, 0.07f);
        vg_rect_a(0, (int)y + 40, ST_W, 20, 0x3A3A44, 0.07f);
    }
    for (i = 0; i < 170; i++) {                   /* rain, slanting, grey */
        uint32_t q = (uint32_t)i * 2654435761u;
        float sp = 380.0f + (float)(q >> 24) * 1.6f, x = (float)((q >> 8) % ST_W) + t * 40.0f, y = te_wrap((float)(q & 1023u) / 1024.0f + t * sp / (float)ST_H) * (float)(ST_H + 40) - 20.0f;
        x = (float)((int)(x + y * 0.18f) % ST_W);
        vg_line(x, y, x - 5.0f, y + 26.0f, 1.0f, (q >> 4) & 1u ? 0x3E3E4A : 0x2A2A34);
    }
    for (i = 0; i < 60; i++) {                    /* ash and embers, rising slow; the kicks stir them */
        uint32_t q = (uint32_t)(i + 77) * 2246822519u;
        float u = te_wrap((float)(q & 4095u) / 4096.0f + t * (0.02f + (float)((q >> 12) & 15u) * 0.003f));
        float x = (float)((q >> 16) % ST_W) + vd_sin(t * 0.2f + (float)i) * 18.0f, y = (float)ST_H * (1.0f - u);
        uint32_t c = (q >> 3) % 4u == 0u ? vg_mix(0x5A1A0A, 0xFF6A2A, 0.3f + 0.7f * hit) : 0x4A4440;
        vg_rect((int)x, (int)y, (q >> 3) % 4u == 0u ? 3 : 2, (q >> 3) % 4u == 0u ? 3 : 2, c);
    }
}

/* stopped: the hub's card as a poster in the middle, drawn here: a dense figure burnt white at its core, the name
 * across it on a black band, VIBE FM under it. The rings come back with PLAY */
static lt_seg st_post[LT_BIG];
static int st_post_n;
static void st_poster(float hit, float e)       /* e: how far it is in, 0 .. 1 (the iris' opening) */
{
    int j, pass;
    float ir = e * 104.0f, sc = 0.6f;            /* (inside the sound's ring: the rings stay) */
    if (!st_post_n) {                             /* (grown once, the same each time) */
        int a;
        uint32_t keep = lt_rng;
        lt_out = st_post, lt_out_n = &st_post_n, lt_out_cap = LT_BIG;
        lt_rng = 0x2617u;
        for (a = 0; a < 10; a++)                  /* (dense as the card's: ten arms, deep) */
            lt_tree(240.0f, 160.0f, (float)a / 10.0f + (lt_u() - 0.5f) * 0.08f, 44.0f + 18.0f * lt_u(), 5.0f, 9, 0.42f / 6.2832f * 1.5f);
        lt_rng = keep;
    }
    {   /* a red haze behind it, as the card's */
        int q;
        for (q = 12; q > 0; q--)
            vg_circle((float)SR_CX, (float)SR_CY, e * (20.0f + (float)q * 7.0f), vg_mix(0x070708, 0x3A0806, (0.12f + 0.1f * hit) * (float)(13 - q) / 12.0f));
    }
    for (pass = 0; pass < 3; pass++)              /* glow, red, the white core */
        for (j = 0; j < st_post_n; j++) {
            const lt_seg *g = &st_post[j];
            float w = (float)g->w * 0.25f, x0 = (float)SR_CX + ((float)g->x0 - 240.0f) * sc, y0 = (float)SR_CY + ((float)g->y0 - 160.0f) * sc;
            float x1 = (float)SR_CX + ((float)g->x1 - 240.0f) * sc, y1 = (float)SR_CY + ((float)g->y1 - 160.0f) * sc;
            {
                float dx = x1 - (float)SR_CX, dy = y1 - (float)SR_CY;
                if (dx * dx + dy * dy > ir * ir)  /* (only inside the iris) */
                    continue;
            }
            if (pass == 0)
                vg_line(x0, y0, x1, y1, w * 2.2f + 1.5f, vg_mix(0x08080A, 0x6A0E06, 0.45f + 0.3f * hit));
            else if (pass == 1)
                vg_line(x0, y0, x1, y1, w * 0.9f + 0.4f, w > 0.6f ? 0xE02414 : 0x8A1A0C);
            else if (w > 0.8f)
                vg_line(x0, y0, x1, y1, w * 0.45f, vg_mix(0xFF7A60, 0xFFF4EE, vd_minf((w - 0.8f) / 1.6f, 1.0f)));
        }
    if (e > 0.35f) {                              /* the name's band opens from the middle, the name burns in, shaking */
        float bb = vd_minf((e - 0.35f) / 0.4f, 1.0f), u = vd_minf((e - 0.6f) / 0.4f, 1.0f);
        int half = (int)(98.0f * bb), nw, x, tw, jx;
        vg_rect(SR_CX - half, SR_CY - 14, half * 2, 26, 0x000000);
        vg_rect(SR_CX - half, SR_CY - 15, half * 2, 1, vg_mix(0x000000, C_RED, bb));
        vg_rect(SR_CX - half, SR_CY + 12, half * 2, 1, vg_mix(0x000000, C_RED, bb));
        if (u > 0.0f) {
            jx = u < 1.0f ? (int)((float)((int)(vibe_frames() >> 9) % 9 - 4) * (1.0f - u)) : 0;
            nw = vg_text_width("KRIECH", VG_BIG) + vg_text_width("STROM", VG_BIG);
            x = SR_CX - nw / 2 + jx;
            x += vg_text("KRIECH", x, SR_CY - 12, VG_BIG, vg_mix(0x000000, C_WHITE, u), VG_LEFT);
            vg_text("STROM", x, SR_CY - 12, VG_BIG, vg_mix(0x000000, C_RED, u), VG_LEFT);
            tw = vg_text_width("VIBE FM", VG_SMALL) + 12;
            vg_rect(SR_CX - tw / 2 - jx, SR_CY + 20, tw, 16, vg_mix(0x000000, C_RED, u));
            vg_text("VIBE FM", SR_CX - jx, SR_CY + 21, VG_SMALL, 0x000000, VG_CENTER);
        }
    }
}

void vibe_on_stage(uint16_t *fb, int w, int h)
{
    uint32_t k;
    char b[16];
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    float t = te_t(), hit = kx_big_now();
    kx_stage_on = 1;
    if (!st_bg_ok)
        st_bg_make(fb);
    memcpy(fb, st_bg, sizeof st_bg);
    st_weather(t, hit);
    /* the picture: the discharges behind everything, the rings, the figure and the sound in the middle */
    kx_lt_draw_at(0.0f, -66.0f, 2.6667f);
    {   /* the radar: faint rings to the edges, a shock ring out from every kick */
        int r;
        for (r = SR_R0 + 4 * SR_STEP + 30; r < 760; r += 70)
            st_arc(0.0f, 1.0f, (float)r, 1.0f, 0x16161A);
        {
            float age = (float)(vibe_frames() - kx_big_pt) / (kx_sr * 0.9f);
            if (kx_big_pt && age < 1.0f)
                st_arc(0.0f, 1.0f, 110.0f + age * 560.0f, 2.0f + 3.0f * (1.0f - age), vg_mix(0x08080A, 0xB02010, (1.0f - age) * (1.0f - age) * kx_big_pulse));
        }
        for (r = 0; r < 96; r++) {                /* the dial's ticks round the outside */
            float a = (float)r / 96.0f, r0 = (float)(SR_R0 + 4 * SR_STEP) + 2.0f, r1 = r0 + ((r % 8) ? 6.0f : 14.0f);
            vg_line(st_pt_x(a, r0), st_pt_y(a, r0), st_pt_x(a, r1), st_pt_y(a, r1), 1.0f, (r % 8) ? 0x26262C : 0x4A4A52);
        }
    }
    {   /* a red glow in the middle, breathing with the kicks */
        int q;
        for (q = 10; q > 0; q--)
            vg_circle((float)SR_CX, (float)SR_CY, 40.0f + (float)q * 9.0f, vg_mix(0x08080A, 0x5A0E08, (0.08f + 0.22f * hit) * (float)(11 - q) / 10.0f));
    }
    {
        static uint32_t bar = 0xFFFFFFFFu;
        uint32_t bb = song.playing ? trk[song.sel].seq_idx / 16u : bar;   /* (stopped: the figure stays as it is) */
        if (bb != bar || !kx_big_n)
            bar = bb, kx_big_grow();
        kx_big_draw_at(heat > 1.0f ? 1.0f : heat, 0.35f + 0.65f * (kx_fx_hit > 1.0f ? 1.0f : kx_fx_hit), (float)SR_CX,
                       (float)SR_CY, 1.15f);   /* (as big as the ring round it, never dim) */
    }
    {   /* the master's sound as a ring round the figure */
        uint32_t sw = kx_scope_w, i, n = 240;
        float px = 0, py = 0, fx = 0, fy = 0;
        for (i = 0; i <= n; i++) {
            int32_t s0 = kx_scope[(sw - n * 3u + (i % n) * 3u) & (KX_SCOPE - 1u)];
            float a = (float)i / (float)n, r = 104.0f + (float)s0 * (14.0f / 32768.0f) * 1.6f;
            float x = st_pt_x(a, r), y = st_pt_y(a, r);
            if (i) {
                vg_line(px, py, x, y, 4.0f, vg_mix(C_BG, C_WHITE, 0.18f));
                vg_line(px, py, x, y, 1.4f, C_WHITE);
            } else
                fx = x, fy = y;
            px = x, py = y;
        }
        (void)fx, (void)fy;
    }
    for (k = 0; k < NTRK; k++)
        st_ring(k);
    {   /* stopped: the iris opens on the poster; PLAY closes it on the rings (0.8 s either way) */
        static float pa = -1.0f;
        static uint64_t last;
        uint64_t now = vibe_frames();
        float dt = last && now > last ? (float)(now - last) / kx_sr : 0.0f, e;
        if (pa < 0.0f)
            pa = song.playing ? 0.0f : 1.0f;
        last = now;
        pa += (song.playing ? -1.0f : 1.0f) * dt / 0.8f;
        pa = pa < 0.0f ? 0.0f : pa > 1.0f ? 1.0f : pa;
        e = pa * pa * (3.0f - 2.0f * pa);
        if (e > 0.001f) {
            float ir = e * 104.0f;
            vg_circle((float)SR_CX, (float)SR_CY, ir, 0x070708);
            st_arc(0.0f, 1.0f, ir, 3.0f + 4.0f * (1.0f - e), vg_mix(0xFF6A40, 0x5A0E08, e));   /* (its edge, hot while it moves) */
            st_poster(hit, e);
        }
    }
    /* the bands: opaque, the words only here */
    vg_rect_a(0, 0, SB_LW, ST_H, 0x050506, 0.86f);   /* (dark plates over the soot: the ground shows through, faint) */
    vg_rect_a(SB_RX, 0, ST_W - SB_RX, ST_H, 0x050506, 0.86f);
    vg_rect_a(SB_LW, SB_BY, SB_RX - SB_LW, ST_H - SB_BY, 0x050506, 0.86f);
    vg_rect(SB_LW, 0, 2, ST_H, 0x2A2A30), vg_rect(SB_RX - 2, 0, 2, ST_H, 0x2A2A30), vg_rect(SB_LW, SB_BY, SB_RX - SB_LW, 2, 0x2A2A30);
    {   /* bolts at the plates' corners, a strip of dirty hazard tape over each */
        static const int BX[6] = {8, SB_LW - 10, SB_RX + 8, ST_W - 10, 8, ST_W - 10}, BY[6] = {ST_H - 10, ST_H - 10, ST_H - 10, ST_H - 10, 6, 6};
        int i, sx;
        for (i = 0; i < 6; i++) {
            vg_circle((float)BX[i], (float)BY[i], 4.0f, 0x2A2420);
            vg_line((float)BX[i] - 2.5f, (float)BY[i] + 1.5f, (float)BX[i] + 2.5f, (float)BY[i] - 1.5f, 1.2f, 0x0A0A0A);
        }
        for (sx = SB_RX + 30; sx < ST_W - 30; sx += 12)
            vg_tri((float)sx, (float)(ST_H - 22), (float)(sx + 6), (float)(ST_H - 22), (float)(sx + 11), (float)(ST_H - 30), 0x6A5A1C);
        vg_rect(SB_RX + 30, ST_H - 32, ST_W - SB_RX - 60, 2, 0x6A5A1C);
    }
    {
        int x = 20, wd = vg_text("KRIECH", x, 14, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
        vg_text("STROM", x + wd, 14, VG_BIG | VG_X2, C_RED, VG_LEFT);
    }
    vg_itoa(song.g[G_BPM], b);
    {
        int bw = vg_text(b, 20, 72, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
        vg_text("BPM", 26 + bw, 92, VG_BOLD, TE_MID, VG_LEFT);
    }
    {   /* the transport: a button that says what it does */
        int bx = SB_LW - 124, by = 74;
        vg_round(bx, by, 106, 42, 6, song.playing ? 0x2A2A30 : C_RED);
        if (song.playing)
            vg_rect(bx + 14, by + 13, 16, 16, C_WHITE), vg_text("STOP", bx + 40, by + 12, VG_BOLD, C_WHITE, VG_LEFT);
        else
            vg_tri((float)(bx + 14), (float)(by + 11), (float)(bx + 14), (float)(by + 31), (float)(bx + 31), (float)(by + 21), C_WHITE),
            vg_text("PLAY", bx + 40, by + 12, VG_BOLD, C_WHITE, VG_LEFT);
    }
    {   /* DICE's MOOD: a button, the next one on a click */
        int by = 126;
        vg_round(16, by, SB_LW - 34, 32, 5, 0x1A1A1E);
        vg_text("DICE MOOD", 28, by + 8, VG_SMALL, TE_MID, VG_LEFT);
        vg_text(MOOD_NAME[dice_mood % MOOD_N], 120, by + 7, VG_BOLD, C_WHITE, VG_LEFT);
        vg_text("NEXT >", SB_LW - 30, by + 8, VG_SMALL, C_RED, VG_RIGHT);
    }
    for (k = 0; k < NTRK; k++)
        st_track_card(k);
    for (k = 0; k < SK_N; k++)
        st_knob((int)k);
    {   /* how much the screen wears with the sound (MENU > DISPLAY): three buttons */
        static const char *const W3[3] = {"OFF", "LOW", "FULL"};
        int q, cur = clamp(kx_ic[IC_SCREEN], 0, 2);
        vg_text("SCREEN WEAR", SB_RX + 8, 556, VG_BOLD, TE_MID, VG_LEFT);
        for (q = 0; q < 3; q++) {
            int bx = SB_RX + 8 + q * 100;
            vg_round(bx, 580, 92, 44, 6, q == cur ? C_RED : 0x1A1A1E);
            vg_text(W3[q], bx + 46, 592, VG_BOLD, q == cur ? C_WHITE : TE_MID, VG_CENTER);
        }
    }
    st_pads();
    kx_wear_fx(fb, w, h);
}

static void st_level(uint32_t k, int x)          /* a track's LEVEL from where the pointer is on its fader */
{
    const param_desc_t *d = &TP[P_LEVEL];
    int v = d->min + (int)((float)clamp(x - 84, 0, 166) / 166.0f * (float)(d->max - d->min) + 0.5f);
    kx_post(KXC_PARAM, k, P_LEVEL, 0, v);
}
static void st_bpm(int d)
{
    kx_post(KXC_PARAM, NTRK, G_BPM, 0, clamp(song.g[G_BPM] + d, GP[G_BPM].min, GP[G_BPM].max));
}

/* the stage's input: clicks, drags, the wheel and fingers (pointers 0-9), in stage pixels */
static int st_ptr[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   /* what each pointer holds: 100 + knob, 200 + slot */
static int st_ptr_y[10];
static int st_last_x = -1, st_last_y = -1;
static void kx_stage_event(const vibe_event *e)
{
    int x, y, p = (int)(e->midi[0] % 10u);
    if (e->id == VIBE_STAGE_SHOWN || e->id == VIBE_STAGE_HIDDEN) {
        kx_stage_on = e->id == VIBE_STAGE_SHOWN;
        return;
    }
    if (e->id == VIBE_STAGE_WHEEL) {              /* (its value is the steps: it works where the pointer was) */
        x = st_last_x, y = st_last_y;
    } else {
        x = VIBE_STAGE_X(e), y = VIBE_STAGE_Y(e);
        st_last_x = x, st_last_y = y;
    }
    if (e->id == VIBE_STAGE_UP) {                 /* let go: a held effect ends */
        if (st_ptr[p] >= 200) {
            int slot = st_ptr[p] - 200;
            st_fx_down[slot] = 0;
            kx_post(KXC_PERF, (uint32_t)clamp(kx_ic[IC_PF0 + slot], 0, KX_NFX - 1), 0, 0, 0);
        }
        st_ptr[p] = -1;
        return;
    }
    if (x >= SB_RX && y < 24 + 4 * SK_CH) {       /* a knob of the chain */
        int k = ((y - 24) / SK_CH) * 2 + ((x - SB_RX - 8) >= SK_CW ? 1 : 0);
        if (k < 0 || k >= SK_N)
            return;
        if (e->id == VIBE_STAGE_WHEEL)
            sk_turn(k, e->value > 0 ? 4 : e->value < 0 ? -4 : 0);
        else if (e->id == VIBE_STAGE_DOWN)
            st_ptr[p] = 100 + k, st_ptr_y[p] = y;
        else if (e->id == VIBE_STAGE_MOVE && st_ptr[p] == 100 + k) {
            int d = (st_ptr_y[p] - y) / 2;
            if (d)
                sk_turn(k, d), st_ptr_y[p] = y;
        }
        return;
    }
    if (e->id == VIBE_STAGE_MOVE && st_ptr[p] >= 100 && st_ptr[p] < 200) {   /* (a knob dragged out of its box) */
        int d = (st_ptr_y[p] - y) / 2;
        if (d)
            sk_turn(st_ptr[p] - 100, d), st_ptr_y[p] = y;
        return;
    }
    if (e->id == VIBE_STAGE_MOVE && st_ptr[p] >= 300 && st_ptr[p] < 304) {   /* a level dragged */
        st_level((uint32_t)(st_ptr[p] - 300), x);
        return;
    }
    if (st_ptr[p] == 400 && e->id == VIBE_STAGE_MOVE) {   /* the tempo dragged */
        int d = (st_ptr_y[p] - y) / 4;
        if (d)
            st_bpm(d), st_ptr_y[p] = y;
        return;
    }
    if (e->id == VIBE_STAGE_WHEEL && x < 180 && y >= 70 && y < 120) {
        st_bpm(e->value > 0 ? 1 : -1);
        return;
    }
    if (e->id != VIBE_STAGE_DOWN)
        return;
    if (x >= SB_RX + 8 && y >= 580 && y < 624) {  /* SCREEN WEAR: OFF LOW FULL */
        int q = (x - SB_RX - 8) / 100;
        if (q >= 0 && q < 3)
            kx_post(KXC_INCFG, IC_SCREEN, 1, 0, q);
        return;
    }
    if (y >= SB_BY && x >= SB_LW - 64 && x < SB_RX) {   /* the effects, held; SHIFT */
        if (x < SB_LW) {
            st_shift = !st_shift;
            return;
        }
        {
            int i = (x - SB_LW - 22) / 74, slot;
            if (i < 0 || i > 7)
                return;
            slot = i + (st_shift ? 8 : 0);
            st_fx_down[slot] = 1;
            st_ptr[p] = 200 + slot;
            kx_pfx_hit[slot] = vibe_frames() ? vibe_frames() : 1;
            kx_post(KXC_PERF, (uint32_t)clamp(kx_ic[IC_PF0 + slot], 0, KX_NFX - 1), 1, 0, 0);
        }
        return;
    }
    if (x < SB_LW) {                              /* the left band: play, the tempo, the MOOD, a track, its M */
        if (y >= 70 && y < 120 && x >= SB_LW - 126) {
            kx_post(KXC_PLAY, 3, 0, 0, 0);
            return;
        }
        if (y >= 70 && y < 120 && x < 180) {      /* the tempo: drag it, or the wheel */
            st_ptr[p] = 400, st_ptr_y[p] = y;
            return;
        }
        if (y >= 124 && y < 160) {                /* the MOOD for DICE: the next */
            kx_post(KXC_TOOL, song.sel, TL_MOOD, 0, 0);
            return;
        }
        if (y >= 168 && y < 168 + 4 * 120) {
            uint32_t k = (uint32_t)(y - 168) / 120;
            int ly = y - 168 - (int)k * 120;
            if (ly >= 110)
                return;
            if (x >= SB_LW - 62 && x < SB_LW - 22 && ly >= 70 && ly < 100)
                kx_post(KXC_MUTE, k, 0, 0, 0);
            else if (x >= SB_LW - 62 && x < SB_LW - 22 && ly >= 8 && ly < 34)
                kx_post(KXC_TOOL, k, TL_DICE, 0, 0);
            else if (x >= 76 && x < 258 && ly >= 70 && ly < 104) {   /* the level: where clicked, then dragged */
                st_ptr[p] = 300 + (int)k;
                st_level(k, x);
            } else
                kx_post(KXC_SEL, k, 0, 0, 0);
        }
        return;
    }
    {   /* a ring: its step; the middle: PLAY */
        float dx = (float)(x - SR_CX), dy = (float)(y - SR_CY), r = vd_sqrt(dx * dx + dy * dy);
        if (r < (float)SR_R0 - 14.0f) {
            kx_post(KXC_PLAY, 3, 0, 0, 0);
            return;
        }
        int k = (int)((r - (float)SR_R0) / (float)SR_STEP);
        float in = r - (float)SR_R0 - (float)k * (float)SR_STEP;
        if (k >= 0 && k < NTRK && in >= 0.0f && in < (float)SR_TH) {
            const track_t *t = &trk[k];
            uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), s = (uint32_t)(st_turns(dx, dy) * (float)len), lane = 255u;
            if (s >= len)
                s = len - 1u;
            if (drum_track(t))
                lane = (uint32_t)clamp((int)((in - 3.0f) / 4.4f + 0.5f), 0, NLANE - 1);
            kx_post(KXC_STEP, (uint32_t)k, s, lane, 0);
        }
    }
}

static void kx_stage_init(void)
{
    cr_init();
    vibe_stage(kx_stage_fb, ST_W, ST_H, C_BG, 0);
}
