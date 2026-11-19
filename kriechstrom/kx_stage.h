/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the stage (SDK 23), 960 x 540. The whole machine at once: the four tracks' patterns with
 * their playheads, the creep (the master's wear, kx_creep.h) over the scope, and the master's chain as knobs.
 * A view and a shortcut, never the only way: each part is one of the base controls and says which.
 *   a step          the SEQ page's pads (K6), or ENTER on its cursor
 *   a track's name  F1..F4 (select), the pads 1-4 (mute: a click on its M)
 *   a knob          a drag up / down or the wheel; DAMAGE = encoder B, WALL = SHIFT + B, the rest FX > MASTER (K5) */

#define ST_W 960
#define ST_H 540
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

/* the layout */
#define SL_X 0
#define SL_Y 64
#define SL_H 88                                   /* a track's lane */
#define SL_LX 150                                 /* the steps from here .. */
#define SL_RX 596                                 /* .. to here */
#define SC_X 608                                  /* the creep's box */
#define SC_Y 64
#define SC_S 352
#define SR_Y 424                                  /* the chain */
#define SR_W 118

static void st_arc(float cx, float cy, float r, float a, uint32_t col, uint32_t bg)
{
    int i, n = 28;
    float px = 0, py = 0;
    for (i = 0; i <= n; i++) {                    /* 270 degrees, from bottom left round to bottom right */
        float t = (float)i / (float)n, ang = 0.375f - t * 0.75f, x = cx + vd_sin(ang + 0.25f) * r, y = cy - vd_sin(ang) * r;
        if (i)
            vg_line(px, py, x, y, 5.0f, t <= a ? col : bg);
        px = x;
        py = y;
    }
}

static void st_lane(uint32_t k)
{
    const track_t *t = &trk[k];
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, col = TRK_RGB[k], play = t->seq_idx % len;
    int y = SL_Y + (int)k * SL_H, drum = drum_track(t), lo = 127, hi = 0, mute = t->p[P_MUTE] != 0;
    float cw = (float)(SL_RX - SL_LX) / (float)len;
    char nm[17], n[2] = {(char)('1' + k), 0};
    vg_rect(0, y, SL_RX + 4, SL_H - 6, k == song.sel ? vg_mix(C_PANEL, col, 0.10f) : C_PANEL);
    vg_rect(0, y, 5, SL_H - 6, mute ? C_LINE : col);
    vg_text(n, 16, y + 8, VG_BIG, mute ? C_DIM : col, VG_LEFT);
    vg_text(ENGINES[eng_idx(t->eng_req)]->name, 42, y + 12, VG_BOLD, mute ? C_DIM : C_TEXT, VG_LEFT);
    sound_name(t, nm);
    nm[14] = 0;
    vg_text(nm, 16, y + 40, VG_MED, mute ? C_DIM : C_WHITE, VG_LEFT);
    vg_round(112, y + 58, 28, 20, 3, mute ? C_RED : C_LINE);
    vg_text("M", 126, y + 60, VG_BOLD, mute ? C_WHITE : C_DIM, VG_CENTER);
    for (i = 0; i < len; i++) {
        const step_t *st = &t->step[i];
        uint32_t j;
        if (st->time == ST_NOTE)
            for (j = 0; j < st->n; j++) {
                lo = st->note[j] < lo ? st->note[j] : lo;
                hi = st->note[j] > hi ? st->note[j] : hi;
            }
    }
    if (hi < lo) lo = 36, hi = 60;
    if (hi - lo < 12) hi = lo + 12;
    for (i = 0; i < len; i++) {
        const step_t *st = &t->step[i];
        int x0 = SL_LX + (int)((float)i * cw), x1 = SL_LX + (int)((float)(i + 1u) * cw), w = x1 - x0;
        int on = song.playing && i == play;
        vg_rect(x0, y + 4, w - 1, SL_H - 14, (i / 4u) % 2u ? 0x1C1C20 : 0x17171A);
        if (drum) {                               /* a hit per lane: KICK at the bottom, as a drum machine's rows */
            uint32_t l, hits = step_lanes(st);
            int rh = (SL_H - 18) / 8;
            for (l = 0; l < NLANE; l++) {
                int ry = y + SL_H - 12 - (int)(l + 1u) * rh;
                if ((hits >> l) & 1u)
                    vg_round(x0 + 2, ry + 1, w - 5, rh - 2, 2, on ? C_WHITE : mute ? C_LINE : col);
                else if (w > 10)
                    vg_rect(x0 + w / 2 - 1, ry + rh / 2 - 1, 2, 2, 0x2A2A30);
            }
        } else if (st->time == ST_NOTE && st->n) {
            uint32_t j;
            for (j = 0; j < st->n; j++) {
                int yy = y + SL_H - 16 - (int)((float)(st->note[j] - lo) / (float)(hi - lo) * (float)(SL_H - 26));
                vg_rect(x0 + 1, yy, w - 3, 6, on ? C_WHITE : mute ? C_LINE : (st->flags & SF_ACCENT ? vg_mix(col, C_WHITE, 0.5f) : col));
            }
        } else if (st->time == ST_TIE)
            vg_rect(x0, y + SL_H / 2 - 2, w, 3, vg_mix(col, C_BG, 0.5f));
        if (on)
            vg_rect(x0, y + 2, w - 1, 2, C_WHITE);
    }
}

void vibe_on_stage(uint16_t *fb, int w, int h)
{
    uint32_t k;
    char b[16];
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    kx_stage_on = 1;
    vg_clear(C_BG);
    /* the head: the name, the transport, the tempo */
    {
        int x = 20, wd = vg_text("KRIECH", x, 10, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
        vg_text("STROM", x + wd, 10, VG_BIG | VG_X2, C_RED, VG_LEFT);
    }
    vg_itoa(song.g[G_BPM], b);
    vg_text(b, 840, 10, VG_BIG | VG_X2, C_TEXT, VG_RIGHT);
    vg_text("BPM", 846, 30, VG_BOLD, C_DIM, VG_LEFT);
    if (song.playing)
        vg_tri(900, 14, 900, 46, 928, 30, C_RED);
    else
        vg_rect(902, 16, 26, 26, C_DIM);
    vg_text("MOOD", 600, 6, VG_SMALL, C_LINE, VG_RIGHT);
    vg_text(MOOD_NAME[dice_mood % MOOD_N], 600, 20, VG_BOLD, C_DIM, VG_RIGHT);
    for (k = 0; k < NTRK; k++)
        st_lane(k);
    /* the creep over the master's scope */
    vg_rect(SC_X, SC_Y, SC_S, SC_S - 6, 0x0D0D0F);
    cr_draw(SC_X, SC_Y, SC_S, SC_S - 6, cr_wear(), heat, C_RED);
    {
        uint32_t sw = kx_scope_w, i;
        float mid = (float)(SC_Y + SC_S - 50), px = 0, py = 0;
        for (i = 0; i < (uint32_t)SC_S; i++) {
            int32_t a = kx_scope[(sw - (uint32_t)SC_S * 2u + i * 2u) & (KX_SCOPE - 1u)];
            float yy = mid - (float)a * (40.0f / 32768.0f);
            if (i)
                vg_line(px, py, (float)(SC_X + (int)i), yy, 1.6f, C_WHITE);
            px = (float)(SC_X + (int)i);
            py = yy;
        }
    }
    vg_text("WEAR", SC_X + 12, SC_Y + 10, VG_SMALL, C_DIM, VG_LEFT);
    vg_itoa((int)(cr_wear() * 100.0f + 0.5f), b);
    vg_text(b, SC_X + 12, SC_Y + 26, VG_BIG, C_WHITE, VG_LEFT);
    /* the chain */
    for (k = 0; k < SK_N; k++) {
        int x = 6 + (int)k * (SR_W + 1), v = sk_value((int)k);
        float a = (float)clamp(v, 0, 127) / 127.0f;
        vg_round(x, SR_Y, SR_W - 4, 108, 6, vg_mix(C_PANEL, C_RED, a * 0.18f));
        st_arc((float)(x + SR_W / 2 - 2), (float)(SR_Y + 50), 26.0f, a, k == SK_IN ? C_WHITE : C_RED, C_LINE);
        vg_itoa(v, b);
        vg_text(b, x + SR_W / 2 - 2, SR_Y + 40, VG_BOLD, C_WHITE, VG_CENTER);
        vg_text(SK_NAME[k], x + SR_W / 2 - 2, SR_Y + 76, VG_BOLD, v ? C_TEXT : C_DIM, VG_CENTER);
        vg_text(SK_HOW[k], x + SR_W / 2 - 2, SR_Y + 92, VG_SMALL, 0x45454B, VG_CENTER);
    }

    kx_wear_fx(fb, w, h);
}

/* the stage's input: clicks, drags and the wheel, in stage pixels */
static int st_drag = -1;
static int st_drag_y, st_last_x = -1, st_last_y = -1;
static void kx_stage_event(const vibe_event *e)
{
    int x, y;
    if (e->id == VIBE_STAGE_WHEEL) {              /* (its value is the steps: it works where the last click was) */
        x = st_last_x;
        y = st_last_y;
    } else {
        x = VIBE_STAGE_X(e);
        y = VIBE_STAGE_Y(e);
        if (e->id == VIBE_STAGE_DOWN || e->id == VIBE_STAGE_MOVE)
            st_last_x = x, st_last_y = y;
    }
    if (e->id == VIBE_STAGE_SHOWN || e->id == VIBE_STAGE_HIDDEN) {
        kx_stage_on = e->id == VIBE_STAGE_SHOWN;
        return;
    }
    if (e->id == VIBE_STAGE_UP) {
        st_drag = -1;
        return;
    }
    if (y >= SR_Y && y < SR_Y + 108 && x >= 6) {  /* a knob of the chain */
        int k = (x - 6) / (SR_W + 1);
        if (k >= SK_N)
            return;
        if (e->id == VIBE_STAGE_WHEEL)
            sk_turn(k, e->value > 0 ? 4 : e->value < 0 ? -4 : 0);   /* (the wheel's steps may be fractional) */
        else if (e->id == VIBE_STAGE_DOWN) {
            st_drag = k;
            st_drag_y = y;
        } else if (e->id == VIBE_STAGE_MOVE && st_drag == k) {
            int d = (st_drag_y - y) / 2;
            if (d) {
                sk_turn(k, d);
                st_drag_y = y;
            }
        }
        return;
    }
    if (e->id != VIBE_STAGE_DOWN)
        return;
    if (y >= SL_Y && y < SL_Y + 4 * SL_H && x < SL_RX) {   /* a lane */
        uint32_t k = (uint32_t)(y - SL_Y) / SL_H;
        const track_t *t = &trk[k];
        int ly = y - SL_Y - (int)k * SL_H;
        if (x >= 112 && x < 140 && ly >= 58 && ly < 78) {
            kx_post(KXC_MUTE, k, 0, 0, 0);
        } else if (x < SL_LX) {
            kx_post(KXC_SEL, k, 0, 0, 0);
        } else {
            uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
            uint32_t s = (uint32_t)((float)(x - SL_LX) / ((float)(SL_RX - SL_LX) / (float)len));
            uint32_t lane = 255u;
            if (s >= len)
                return;
            if (drum_track(t))
                lane = (uint32_t)clamp((SL_H - 12 - ly) * 8 / (SL_H - 18), 0, NLANE - 1);
            kx_post(KXC_STEP, k, s, lane, 0);
        }
        return;
    }
    if (y < 56 && x > 880)
        kx_post(KXC_PLAY, 3, 0, 0, 0);
}

static void kx_stage_init(void)
{
    cr_init();
    vibe_stage(kx_stage_fb, ST_W, ST_H, C_BG, 0);
}
