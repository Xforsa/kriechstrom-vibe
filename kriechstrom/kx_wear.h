/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the display wears with the sound. What the master does to the mix, the picture shows:
 *   the creep   the figure behind every page, grown as far as the mix is worn, its trunk pulsing with the level
 *   DAMAGE      on a hit, bands of the picture tear sideways (CHAR towards DIGITAL: in blocks, its colours split)
 *   SPLICE      on the sixteenths a band of the picture is cut and glued in from elsewhere
 *   HAUNT DRAG  over each bar's last beat the picture sags, as the tape slows
 *   HISS        grain; WALL: the scanlines close up
 * Only the picture: nothing here touches the sound or the core (the control flow, after the page is drawn). */

static uint16_t *kx_fb;                           /* the display being drawn (vibe_on_draw) */
#define BG_W 480
#define BG_H 320
static uint16_t kx_bg[BG_W * BG_H];               /* the creep, drawn when the wear changes */
static int kx_bg_wear = -1;
static uint32_t kx_fx_rng = 0x9E3779B9u;
static float kx_fx_prev, kx_fx_hit;               /* the level last frame, a hit's strength (decaying) */
static uint32_t kx_fx_step = 0xFFFFFFFFu;         /* the sixteenth last seen (SPLICE) */
static int kx_fx_band_y, kx_fx_band_h, kx_fx_band_src, kx_fx_band_t;

static uint32_t fx_rand(void)
{
    kx_fx_rng ^= kx_fx_rng << 13;
    kx_fx_rng ^= kx_fx_rng >> 17;
    kx_fx_rng ^= kx_fx_rng << 5;
    return kx_fx_rng;
}

static inline uint16_t px_scale(uint16_t p, int k)   /* k: 256 = as it is */
{
    int r = ((p >> 11) & 31) * k >> 8, g = ((p >> 5) & 63) * k >> 8, b = (p & 31) * k >> 8;
    return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
}

/* ---- the live lightning: on a hit, a discharge grown the way the site's pictures grow theirs (tools/make_hero.py,
 * core/kriechstrom/tools/krst_art.py tree): many short wandering steps, a fork now and then, thinner each time. What
 * hit shapes it: the KICK a big one up from the floor, SNARE and CLAP a branch in from the side, the hats short sparks
 * from the top, any other hit one from the corner; how hard, its length and width; DAMAGE how far it wanders, WALL its
 * extra arms and weight, HISS fine filaments around it. It glows dark red and fades in half a second (on FULL its core
 * is hot). MENU > DISPLAY: LIGHTNING off or on; nothing with SCREEN WEAR OFF. */
#define LT_N 5
#define LT_SEG 700
typedef struct { int16_t x0, y0, x1, y1; uint8_t w; } lt_seg;   /* (w: quarter pixels) */
static struct { lt_seg s[LT_SEG]; int n; uint64_t t0; float amt; } kx_lt[LT_N];
static lt_seg *lt_out;                            /* where lt_tree writes, how many it has, how many fit */
static int *lt_out_n, lt_out_cap;
static int kx_lt_next;
static int kx_lt_idle;                            /* the idle screen: from every edge and corner, hotter */
static uint64_t kx_lt_last;                      /* the last one's time (the idle screen sparks on its own in silence) */
static uint32_t lt_rng = 0x13579BDFu;
static float lt_u(void)
{
    lt_rng ^= lt_rng << 13;
    lt_rng ^= lt_rng >> 17;
    lt_rng ^= lt_rng << 5;
    return (float)(lt_rng & 0xFFFFu) / 65535.0f;
}
static float lt_g(void) { return (lt_u() + lt_u() + lt_u() - 1.5f) * 2.0f; }   /* (about a normal's) */
static void lt_tree(float x, float y, float ang, float len, float wid, int depth, float spread)
{
    float head = ang;
    int k, steps = 7 + (int)(lt_u() * 6.99f);
    if (depth <= 0 || wid < 0.35f)
        return;
    for (k = 0; k < steps && *lt_out_n < lt_out_cap; k++) {
        float l = len * (0.15f + lt_u() * 0.25f), nx, ny;
        head += lt_g() * spread;
        head = head * 0.85f + ang * 0.15f;
        nx = x + vd_sin(head + 0.25f) * l;
        ny = y + vd_sin(head) * l;
        {
            lt_seg *g = &lt_out[(*lt_out_n)++];
            g->x0 = (int16_t)x, g->y0 = (int16_t)y, g->x1 = (int16_t)nx, g->y1 = (int16_t)ny;
            g->w = (uint8_t)(wid * 4.0f > 255.0f ? 255.0f : wid * 4.0f);
        }
        if (lt_u() < 0.3f)                        /* (a fork: 0.4 .. 1.1 rad off, shorter, thinner) */
            lt_tree(nx, ny, head + (lt_u() < 0.5f ? -1.0f : 1.0f) * (0.4f + lt_u() * 0.7f) / 6.2832f, len * 0.72f,
                    wid * 0.55f, depth - 1, spread);
        x = nx, y = ny;
        wid *= 0.9f;
    }
}
/* kind: 0 KICK, 1 SNARE / CLAP, 2 a hat, 3 another hit; amt 0..1 */
static void kx_lt_spawn(int kind, float amt)
{
    int i = kx_lt_next, arms, a;
    float dmg = (float)song.g[G_DMG] / 127.0f, wall = (float)song.g[G_WALL] / 127.0f, hiss = (float)song.g[G_HISS] / 127.0f;
    float spread = 0.42f / 6.2832f * (0.8f + dmg * 1.4f), x, y, ang, len, wid;
    if (!kx_ic[IC_SCREEN] || !kx_ic[IC_LIGHT])
        return;
    kx_lt_next = (kx_lt_next + 1) % LT_N;
    kx_lt[i].n = 0;
    kx_lt[i].t0 = vibe_frames() ? vibe_frames() : 1;
    kx_lt[i].amt = amt;
    lt_rng ^= (uint32_t)vibe_frames() * 2654435761u;
    kx_lt_last = kx_lt[i].t0;
    if (kx_lt_idle) {                             /* (the idle screen: the corners, the top, the sides, the floor) */
        int side = (int)(lt_u() * 4.0f);
        float r = lt_u();
        if (kind == 0)
            side = 3;
        switch (side) {
        case 0: x = r < 0.5f ? -2.0f : 482.0f, y = -2.0f, ang = r < 0.5f ? 0.125f : 0.375f; break;   /* a top corner */
        case 1: x = 40.0f + lt_u() * 400.0f, y = -2.0f, ang = 0.25f; break;                       /* the top */
        case 2: x = r < 0.5f ? -2.0f : 482.0f, y = 40.0f + lt_u() * 200.0f, ang = r < 0.5f ? 0.0f : 0.5f; break;
        default: x = 20.0f + lt_u() * 440.0f, y = 322.0f, ang = -0.25f; break;                    /* the floor */
        }
        len = (kind == 2 ? 22.0f : 34.0f) + 44.0f * amt, wid = 1.8f + 3.0f * amt;
    } else switch (kind) {
    case 0: x = 250.0f + lt_u() * 210.0f, y = 322.0f, ang = -0.25f, len = 34.0f + 40.0f * amt, wid = 2.4f + 2.4f * amt; break;
    case 1: x = 482.0f, y = 120.0f + lt_u() * 150.0f, ang = 0.5f, len = 28.0f + 30.0f * amt, wid = 1.8f + 1.4f * amt; break;
    case 2: x = 280.0f + lt_u() * 190.0f, y = 26.0f, ang = 0.25f, len = 12.0f + 14.0f * amt, wid = 1.0f + 0.6f * amt; break;
    default: x = 470.0f, y = 312.0f, ang = -0.375f, len = 28.0f + 36.0f * amt, wid = 1.8f + 2.0f * amt; break;
    }
    lt_out = kx_lt[i].s, lt_out_n = &kx_lt[i].n, lt_out_cap = LT_SEG;
    wid *= 1.0f + wall * 0.8f;
    arms = 1 + (wall > 0.5f) + (wall > 0.85f);
    for (a = 0; a < arms; a++)
        lt_tree(x, y, ang + ((float)a - (float)(arms - 1) * 0.5f) * 0.06f, len, a ? wid * 0.7f : wid, 6, spread);
    for (a = 0; a < (int)(hiss * 5.0f); a++)      /* (HISS: fine filaments about it) */
        lt_tree(x + (lt_u() - 0.5f) * 60.0f, y + (lt_u() - 0.5f) * 30.0f, ang + (lt_u() - 0.5f) * 0.3f, 10.0f, 0.6f, 3,
                spread);
}
/* the idle screen's figure: grown anew each bar (each 2 s stopped), a new shape, from the root up over a third of a
 * second, as the site's figures; it flares on the hits */
#define LT_BIG 2400
static lt_seg kx_big[LT_BIG];
static int kx_big_n;
static uint64_t kx_big_t0, kx_big_pt;
static float kx_big_r2, kx_big_pulse;             /* (the figure's reach squared; the last hit's strength, when) */
static void kx_big_kick(float a)                  /* a hit: the figure flares and fades in a third of a second */
{
    float age = (float)(vibe_frames() - kx_big_pt) / (kx_sr * 0.35f), now = age < 1.0f ? kx_big_pulse * (1.0f - age) : 0.0f;
    if (a >= now)
        kx_big_pulse = a, kx_big_pt = vibe_frames() ? vibe_frames() : 1;
}
static float kx_big_now(void)
{
    float age = (float)(vibe_frames() - kx_big_pt) / (kx_sr * 0.35f);
    return kx_big_pt && age < 1.0f ? kx_big_pulse * (1.0f - age) * (1.0f - age) : 0.0f;
}
static void kx_big_grow(void)
{
    float dmg = (float)song.g[G_DMG] / 127.0f, wall = (float)song.g[G_WALL] / 127.0f;
    int a, arms = 5 + (int)(wall * 3.0f);
    kx_big_n = 0;
    lt_out = kx_big, lt_out_n = &kx_big_n, lt_out_cap = LT_BIG;
    lt_rng ^= (uint32_t)vibe_frames() * 2246822519u;
    float turn = lt_u();                          /* (turned anew each time: never the same arms in the same places) */
    for (a = 0; a < arms; a++)                    /* (krst_art.py splash: arms all round from the centre) */
        lt_tree(240.0f, 160.0f, turn + (float)a / (float)arms + (lt_u() - 0.5f) * 0.12f, 30.0f + 18.0f * lt_u(),
                3.4f + wall * 1.6f, 8, 0.42f / 6.2832f * (0.8f + dmg));
    kx_big_r2 = 1.0f;
    for (a = 0; a < kx_big_n; a++) {
        float dx = (float)kx_big[a].x1 - 240.0f, dy = (float)kx_big[a].y1 - 160.0f;
        if (dx * dx + dy * dy > kx_big_r2)
            kx_big_r2 = dx * dx + dy * dy;
    }
    kx_big_t0 = vibe_frames() ? vibe_frames() : 1;
}
/* the figure alive: every point of it trembles as an arc does (a smooth noise of where it is and of the time, the
 * tips more than the trunk; a point two branches share moves as one, so nothing comes apart), and current runs out
 * along it from the centre, a wave on every kick */
static void kx_big_pt_at(float x, float y, float t, float rr, float *ox, float *oy)
{
    float dx = x - 240.0f, dy = y - 160.0f, d = vd_sqrt(dx * dx + dy * dy), k = d / (rr > 1.0f ? rr : 1.0f) * 3.2f;
    *ox = x + vd_sin(t * 0.9f + x * 0.031f + y * 0.017f) * k + vd_sin(t * 2.3f + y * 0.05f) * k * 0.4f;
    *oy = y + vd_sin(t * 0.7f + y * 0.029f - x * 0.021f + 0.3f) * k + vd_sin(t * 2.1f + x * 0.05f + 0.6f) * k * 0.4f;
}
static void kx_big_draw_at(float heat, float hit, float ox, float oy, float sc)   /* (the stage: elsewhere, bigger) */
{
    int j, pass, n = kx_big_n;
    float g = (float)(vibe_frames() - kx_big_t0) / (kx_sr * 0.22f), reach, t = song.playing ? (float)vibe_frames() / kx_sr : 0.0f;   /* (stopped: still) */
    float rr = vd_sqrt(kx_big_r2), kick = (float)(vibe_frames() - kx_big_pt) / kx_sr, wave_k = kick * 420.0f;
    float wave = (t * 0.55f - (float)(int)(t * 0.55f)) * rr * 1.3f;  /* (the current's slow wave, out and out again) */
    g = g > 1.0f ? 1.0f : g;
    reach = g * (2.0f - g);                       /* (out from the centre, every arm at once, fast then slowing) */
    reach = reach * reach * kx_big_r2;
    hit = hit * 0.4f + kx_big_now();              /* (the drums' own hits: a flare that fades on every one) */
    hit = hit > 1.0f ? 1.0f : hit;
    for (pass = 0; pass < 2; pass++) {
        for (j = 0; j < n; j++) {
            const lt_seg *s2 = &kx_big[j];
            float ddx = (float)s2->x1 - 240.0f, ddy = (float)s2->y1 - 160.0f, d2 = ddx * ddx + ddy * ddy, d, run, x0, y0, x1, y1;
            if (g < 1.0f && d2 > reach)
                continue;
            float w = (float)s2->w * 0.25f;
            if (!pass && w < 0.9f)
                continue;
            d = vd_sqrt(d2);
            if (sc > 1.0f && d * sc > 100.0f)     /* (the stage: inside the sound's ring only) */
                continue;
            run = 1.0f - vd_minf((d - wave < 0.0f ? wave - d : d - wave) / 26.0f, 1.0f);   /* (the slow current here) */
            if (kx_big_pt && kick < 0.6f) {       /* (a kick's wave, faster, hotter) */
                float kk = 1.0f - vd_minf((d - wave_k < 0.0f ? wave_k - d : d - wave_k) / 18.0f, 1.0f);
                run = vd_minf(1.0f, run * 0.5f + kk * kx_big_pulse * (1.0f - kick / 0.6f) * 1.4f);
            } else
                run *= 0.5f;
            if (!song.playing)
                run = 0.0f;
            /* (red, its thick strands hot as the site's core, the thinnest dark; the growing tip brighter) */
            float tt = (w > 1.6f ? 0.55f + 0.35f * hit : w > 0.9f ? 0.15f + 0.3f * hit : 0.0f) + 0.5f * run;
            uint32_t c = pass ? vg_mix(w > 0.6f ? 0xE02414 : 0x7A160C, 0xFFE8DC, vd_minf(tt + 0.1f * heat, 1.0f))
                              : vg_mix(C_BG, 0x5A140C, 0.5f + 0.5f * hit);
            if (pass && g < 1.0f && d2 > reach * 0.8f)   /* (the growing tips, white hot) */
                c = vg_mix(c, 0xFFF0E8, 0.6f);
            c = vg_mix(C_BG, c, pass ? vd_minf(0.5f + 0.5f * hit + 0.4f * run, 1.0f) : 0.3f + 0.7f * hit);   /* (it breathes) */
            kx_big_pt_at((float)s2->x0, (float)s2->y0, t, rr, &x0, &y0);
            kx_big_pt_at((float)s2->x1, (float)s2->y1, t, rr, &x1, &y1);
            vg_line(ox + (x0 - 240.0f) * sc, oy + (y0 - 160.0f) * sc, ox + (x1 - 240.0f) * sc, oy + (y1 - 160.0f) * sc,
                    (pass ? w * (1.0f + 0.6f * run) : w * 3.0f + 1.0f) * (sc > 1.0f ? sc * 0.7f : 1.0f), c);
        }
    }
}
static void kx_big_draw(float heat, float hit) { kx_big_draw_at(heat, hit, 240.0f, 160.0f, 1.0f); }

static void kx_lt_draw_at(float ox, float oy, float sc)   /* the background, under the page (the stage: scaled) */
{
    int i, pass;
    if (!kx_ic[IC_SCREEN] || !kx_ic[IC_LIGHT])
        return;
    for (i = 0; i < LT_N; i++) {
        float u, b;
        int j;
        if (!kx_lt[i].n || !kx_lt[i].t0)
            continue;
        u = (float)(vibe_frames() - kx_lt[i].t0) / (kx_sr * 0.5f);
        if (u >= 1.0f) {
            kx_lt[i].n = 0;
            continue;
        }
        b = (1.0f - u) * (1.0f - u) * (0.45f + 0.55f * kx_lt[i].amt);
        for (pass = 0; pass < 2; pass++) {
            int hot = kx_ic[IC_SCREEN] == 2 || kx_lt_idle;
            uint32_t c = pass ? (hot ? vg_mix(0x3A0C08, 0xFFB090, b) : vg_mix(0x2A0806, 0xA02A16, b))
                              : vg_mix(C_BG, hot ? 0x701A10 : 0x4A120A, b);
            for (j = 0; j < kx_lt[i].n; j++) {
                const lt_seg *g = &kx_lt[i].s[j];
                float w = (float)g->w * 0.25f;
                vg_line(ox + (float)g->x0 * sc, oy + (float)g->y0 * sc, ox + (float)g->x1 * sc, oy + (float)g->y1 * sc,
                        (pass ? w : w * 2.6f + 1.0f) * (sc > 1.0f ? sc * 0.6f : 1.0f), c);
            }
        }
    }
}
static void kx_lt_draw(void) { kx_lt_draw_at(0.0f, 0.0f, 1.0f); }

/* the background: the creep, then pulsing with the level (its trunk brightest) */
static void kx_bg_paint(uint16_t *fb)
{
    int wear = (int)(cr_wear() * 40.0f + 0.5f), i, k;
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    if (wear != kx_bg_wear) {
        kx_bg_wear = wear;
        vg_begin(kx_bg);
        vg_clear(C_BG);
        cr_draw_in(250, 70, 300, 260, 0.30f + (float)wear / 40.0f * 0.70f, 0.0f, 0x4A160C, 1);
        vg_begin(fb);
    }
    k = kx_ic[IC_SCREEN] == 2 ? 105 + (int)(heat * 170.0f) + (int)(kx_fx_hit * 230.0f) :
        kx_ic[IC_SCREEN] == 1 ? 100 + (int)(heat * 80.0f) : 100;   /* (LOW: it breathes with the level, no flashes) */
    for (i = 0; i < BG_W * BG_H; i++)
        fb[i] = px_scale(kx_bg[i], k);
    kx_lt_draw();
}

static void kx_hits_frame(void)                   /* the hits of this frame: the tears' strength, the lightning */
{
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];                               /* (once a frame: the display's call) */
        static uint32_t seen;
        uint32_t hn = __atomic_load_n(&kx_hits, __ATOMIC_ACQUIRE);
        static uint32_t dstep = 0xFFFFFFFFu;
        uint32_t k, kick = 0;
        for (k = 0; k < NTRK && song.playing; k++) {   /* the drums' KICK or SNARE on the step playing now */
            const track_t *t = &trk[k];
            if (drum_track(t) && !t->p[P_MUTE]) {
                uint32_t idx = t->seq_idx % NSTEP | k << 8;
                if (idx != dstep) {
                    const step_t *st = &t->step[t->seq_idx % NSTEP];
                    uint32_t ln = step_lanes(st), ac = step_accents(st);
                    dstep = idx;
                    kick = (ln & 3u) != 0;
                    if (ln & 1u)                  /* (the live lightning: what hit shapes it) */
                        kx_lt_spawn(0, ac & 1u ? 1.0f : 0.7f), kx_big_kick(ac & 1u ? 1.0f : 0.85f);
                    if (ln & 6u)
                        kx_lt_spawn(1, ac & 6u ? 1.0f : 0.65f), kx_big_kick(0.6f);
                    if ((ln & 0x18u) && !(ln & 7u))
                        kx_lt_spawn(2, ac & 0x18u ? 0.9f : 0.5f);
                }
                break;
            }
        }
        if (hn != seen && k == NTRK)               /* (no drum track: a hit in the mix shapes one) */
            kx_lt_spawn(3, kx_hit_amt), kx_big_kick(kx_hit_amt);
        if (hn != seen || kick) {                 /* a hit (kx_audio.h), or the drums' */
            seen = hn;
            kx_fx_hit = kx_hit_amt > 0.6f ? kx_hit_amt : 0.6f;
        } else
            kx_fx_hit *= 0.7f;
        kx_fx_prev = heat;
    }

/* after the page: the wear of the picture (fb is w x h: the display or the stage) */
static void kx_wear_fx(uint16_t *fb, int w, int h)
{
    static uint16_t row[1280];
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    int dmg = song.g[G_DMG], chr = song.g[G_DMGC], spl = song.g[G_SPLICE], hiss = song.g[G_HISS];
    int wall = song.g[G_WALL], y, x, i, lvl = kx_ic[IC_SCREEN];   /* 0 OFF, 1 LOW (the default), 2 FULL */
    if (w == 480)                                 /* (once a frame: the display's call) */
        kx_hits_frame();
    if (!lvl)
        return;
    if (lvl == 1) {                               /* LOW: a hint of each, easy on the eyes */
        dmg = dmg * 2 / 5;
        chr = chr > 96 ? chr : 0;
        spl = spl / 3;
        hiss = hiss / 4;
        wall = wall / 3;
        if (kx_fx_hit > 0.0f && (fx_rand() & 3u))
            kx_fx_hit = 0.0f;                     /* (one hit in four tears it) */
    }
    /* DAMAGE: torn bands on the hits, sideways; digital: stepped */
    if (dmg > 8 && kx_fx_hit > 0.15f) {
        int n = 1 + dmg / 40, dig = chr > 64;
        for (i = 0; i < n; i++) {
            int by = (int)(fx_rand() % (uint32_t)h), bh = 3 + (int)(fx_rand() % (uint32_t)(6 + dmg / 5));
            int sh = (int)((float)((int)(fx_rand() % 61u) - 30) * kx_fx_hit * (float)dmg / 60.0f);
            if (dig)
                sh = sh / 8 * 8;
            for (y = by; y < by + bh && y < h; y++) {
                uint16_t *r = fb + y * w;
                for (x = 0; x < w; x++)
                    row[x] = r[((x - sh) % w + w) % w];
                memcpy(r, row, (size_t)w * 2u);
            }
        }
    }
    /* CHAR towards DIGITAL with DAMAGE: the red split off to the right */
    if (dmg > 20 && chr > 40) {
        int off = 1 + (dmg * chr) / 4000 + (int)(kx_fx_hit * 4.0f);
        for (y = 0; y < h; y++) {
            uint16_t *r = fb + y * w;
            for (x = w - 1; x >= off; x--)
                r[x] = (uint16_t)((r[x] & 0x07FFu) | (r[x - off] & 0xF800u));
        }
    }
    /* SPLICE: on a sixteenth, by chance, a band glued in from elsewhere, for a few frames */
    if (spl > 0) {
        uint32_t st = trk[song.sel].seq_idx;
        if (st != kx_fx_step) {
            kx_fx_step = st;
            if ((int)(fx_rand() % 160u) < spl && song.playing) {
                kx_fx_band_h = 8 + (int)(fx_rand() % 40u);
                kx_fx_band_y = (int)(fx_rand() % (uint32_t)(h - kx_fx_band_h));
                kx_fx_band_src = (int)(fx_rand() % (uint32_t)(h - kx_fx_band_h));
                kx_fx_band_t = 3;
            }
        }
        if (kx_fx_band_t > 0) {
            kx_fx_band_t--;
            for (y = 0; y < kx_fx_band_h; y++)
                memcpy(fb + (kx_fx_band_y + y) * w, fb + (kx_fx_band_src + y) * w, (size_t)w * 2u);
            for (x = 0; x < w; x++)
                fb[kx_fx_band_y * w + x] = 0xFFFFu;   /* (the cut) */
        }
    }
    /* HAUNT DRAG: the last beat of each bar, the picture sags (the rows from the bottom up, stretched) */
    if (song.g[G_HAUNT] == 2 && song.playing) {
        uint32_t per = (uint32_t)clamp(trk[song.sel].p[P_SLEN], 1, NSTEP), pos = trk[song.sel].seq_idx % 16u;
        (void)per;
        if (pos >= 12u) {
            int sag = (int)((pos - 11u) * (uint32_t)song.g[G_HAMT] / 40u);
            for (y = h - 1; y >= sag; y--)
                if ((y & 1) == 0)
                    memcpy(fb + y * w, fb + (y - sag) * w, (size_t)w * 2u);
        }
    }
    /* scanlines (WALL closes them up) and HISS's grain */
    {
        int k = 230 - wall / 3, every = wall > 64 ? 2 : 3;
        for (y = 0; y < h; y += every) {
            uint16_t *r = fb + y * w;
            for (x = 0; x < w; x++)
                r[x] = px_scale(r[x], k);
        }
    }
    if (hiss > 0) {
        int n = w * h * hiss / 2000;
        for (i = 0; i < n; i++) {
            uint32_t q = fx_rand() % (uint32_t)(w * h);
            uint16_t g = (uint16_t)(fx_rand() & 7u);
            fb[q] = (uint16_t)(fb[q] | (g << 11) | (g << 6) | g);
        }
    }
}
