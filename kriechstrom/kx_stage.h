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
        st_band(a0, a1, r0, r0 + (float)SR_TH, on ? vg_mix(0x101014, col, 0.35f) : (i / 4u) % 2u ? 0x18181E : 0x111115);
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
    if (song.playing) {                           /* the hand: across the ring where it plays */
        float a = ((float)play + 0.5f) / (float)len;
        vg_line(st_pt_x(a, r0 - 4.0f), st_pt_y(a, r0 - 4.0f), st_pt_x(a, r0 + SR_TH + 4.0f), st_pt_y(a, r0 + SR_TH + 4.0f), 2.0f,
                mute ? 0x5A5A60 : vg_mix(col, C_WHITE, 0.6f));
    }
}

static void st_track_card(uint32_t k)            /* the left band: a track */
{
    const track_t *t = &trk[k];
    int y = 150 + (int)k * 120, mute = t->p[P_MUTE] != 0, lv;
    uint32_t col = TRK_RGB[k];
    char nm[17], n[2] = {(char)('1' + k), 0};
    vg_round(16, y, SB_LW - 28, 110, 6, k == song.sel ? vg_mix(0x141416, col, 0.16f) : 0x141416);
    vg_rect(16, y, 6, 110, mute ? 0x3A3A40 : col);
    vg_text(n, 36, y + 10, VG_BIG | VG_X2, mute ? 0x6A6A72 : col, VG_LEFT);
    vg_text(ENGINES[eng_idx(t->eng_req)]->name, 84, y + 14, VG_BOLD, mute ? 0x6A6A72 : TE_INK, VG_LEFT);
    sound_name(t, nm);
    vg_text(nm, 84, y + 36, VG_BIG, mute ? 0x6A6A72 : C_WHITE, VG_LEFT);
    lv = (int)(vd_minf(kx_trk_pk[k] * 1.4f, 1.0f) * 180.0f);   /* (its level now) */
    vg_rect(84, y + 82, 180, 8, 0x22222A);
    vg_rect(84, y + 82, lv, 8, mute ? 0x5A5A60 : col);
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
        int slot = i + (st_shift ? 8 : 0), x = SB_LW + 22 + i * 74, fx = clamp(kx_ic[IC_PF0 + slot], 0, PF_NFX - 1);
        int held = st_fx_down[slot];
        vg_round(x, SB_BY + 6, 68, 48, 6, held ? C_RED : 0x1A1A1E);
        vg_rect(x + 6, SB_BY + 6, 56, 2, st_shift ? 0xA0A0FF : i < 4 ? C_RED : 0xFF8030);
        vg_text(PF_CELL[fx], x + 34, SB_BY + 22, VG_BOLD, held ? C_WHITE : TE_INK, VG_CENTER);
    }
    vg_round(SB_LW - 64, SB_BY + 6, 64, 48, 6, st_shift ? 0x4A4A8A : 0x1A1A1E);
    vg_text("SHIFT", SB_LW - 32, SB_BY + 22, VG_BOLD, st_shift ? C_WHITE : TE_MID, VG_CENTER);
}

void vibe_on_stage(uint16_t *fb, int w, int h)
{
    uint32_t k;
    char b[16];
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    kx_stage_on = 1;
    vg_clear(C_BG);
    /* the picture: the discharges behind everything, the rings, the figure and the sound in the middle */
    kx_lt_draw_at(0.0f, -66.0f, 2.6667f);
    {
        static uint32_t bar = 0xFFFFFFFFu;
        uint32_t bb = song.playing ? trk[song.sel].seq_idx / 16u : (uint32_t)(vibe_frames() / (uint64_t)(kx_sr * 2.0f));
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
    /* the bands: opaque, the words only here */
    vg_rect(0, 0, SB_LW, ST_H, 0x0B0B0D);
    vg_rect(SB_RX, 0, ST_W - SB_RX, ST_H, 0x0B0B0D);
    vg_rect(SB_LW, SB_BY, SB_RX - SB_LW, ST_H - SB_BY, 0x0B0B0D);
    vg_rect(SB_LW, 0, 1, ST_H, 0x22222A), vg_rect(SB_RX - 1, 0, 1, ST_H, 0x22222A), vg_rect(SB_LW, SB_BY, SB_RX - SB_LW, 1, 0x22222A);
    {
        int x = 20, wd = vg_text("KRIECH", x, 14, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
        vg_text("STROM", x + wd, 14, VG_BIG | VG_X2, C_RED, VG_LEFT);
    }
    vg_itoa(song.g[G_BPM], b);
    {
        int bw = vg_text(b, 20, 72, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
        vg_text("BPM", 26 + bw, 92, VG_BOLD, TE_MID, VG_LEFT);
    }
    if (song.playing)
        vg_tri(SB_LW - 60, 76, SB_LW - 60, 116, SB_LW - 26, 96, C_RED);
    else
        vg_rect(SB_LW - 60, 78, 36, 36, 0x5A5A60);
    vg_text(MOOD_NAME[dice_mood % MOOD_N], SB_LW - 74, 92, VG_BOLD, TE_MID, VG_RIGHT);
    for (k = 0; k < NTRK; k++)
        st_track_card(k);
    for (k = 0; k < SK_N; k++)
        st_knob((int)k);
    {
        int wx = SB_RX + 8;
        vg_text("WEAR", wx, 562, VG_BOLD, TE_MID, VG_LEFT);
        vg_itoa((int)(cr_wear() * 100.0f + 0.5f), b);
        vg_text(b, wx, 582, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
    }
    st_pads();
    kx_wear_fx(fb, w, h);
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
            kx_post(KXC_PERF, (uint32_t)clamp(kx_ic[IC_PF0 + slot], 0, PF_NFX - 1), 0, 0, 0);
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
    if (e->id != VIBE_STAGE_DOWN)
        return;
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
            kx_post(KXC_PERF, (uint32_t)clamp(kx_ic[IC_PF0 + slot], 0, PF_NFX - 1), 1, 0, 0);
        }
        return;
    }
    if (x < SB_LW) {                              /* the left band: play, a track, its M */
        if (y >= 70 && y < 130 && x >= SB_LW - 70) {
            kx_post(KXC_PLAY, 3, 0, 0, 0);
            return;
        }
        if (y >= 150 && y < 150 + 4 * 120) {
            uint32_t k = (uint32_t)(y - 150) / 120;
            int ly = y - 150 - (int)k * 120;
            if (ly >= 110)
                return;
            if (x >= SB_LW - 62 && x < SB_LW - 22 && ly >= 70 && ly < 100)
                kx_post(KXC_MUTE, k, 0, 0, 0);
            else
                kx_post(KXC_SEL, k, 0, 0, 0);
        }
        return;
    }
    {   /* a ring: its step */
        float dx = (float)(x - SR_CX), dy = (float)(y - SR_CY), r = vd_sqrt(dx * dx + dy * dy);
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
