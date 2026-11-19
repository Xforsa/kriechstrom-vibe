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
}

/* after the page: the wear of the picture (fb is w x h: the display or the stage) */
static void kx_wear_fx(uint16_t *fb, int w, int h)
{
    static uint16_t row[1280];
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    int dmg = song.g[G_DMG], chr = song.g[G_DMGC], spl = song.g[G_SPLICE], hiss = song.g[G_HISS];
    int wall = song.g[G_WALL], y, x, i, lvl = kx_ic[IC_SCREEN];   /* 0 OFF, 1 LOW (the default), 2 FULL */
    if (w == 480) {                               /* (once a frame: the display's call) */
        static uint32_t seen;
        uint32_t hn = __atomic_load_n(&kx_hits, __ATOMIC_ACQUIRE);
        static uint32_t dstep = 0xFFFFFFFFu;
        uint32_t k, kick = 0;
        for (k = 0; k < NTRK && song.playing; k++) {   /* the drums' KICK or SNARE on the step playing now */
            const track_t *t = &trk[k];
            if (drum_track(t) && !t->p[P_MUTE]) {
                uint32_t idx = t->seq_idx % NSTEP | k << 8;
                if (idx != dstep) {
                    dstep = idx;
                    kick = (step_lanes(&t->step[t->seq_idx % NSTEP]) & 3u) != 0;
                }
                break;
            }
        }
        if (hn != seen || kick) {                 /* a hit (kx_audio.h), or the drums' */
            seen = hn;
            kx_fx_hit = kx_hit_amt > 0.6f ? kx_hit_amt : 0.6f;
        } else
            kx_fx_hit *= 0.7f;
        kx_fx_prev = heat;
    }
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
