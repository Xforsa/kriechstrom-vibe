/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: more outputs (SDK 26). When the host gives the plugin more than OUT L/R, the parts of the
 * mix go out on their own, as they sound in it, for a DAW to record:
 *   OUT 1/2    the master, as always (everything, through the master's chain)
 *   OUT 3/4    T1  ..  OUT 9/10  T4: each track after its pedals, its LEVEL and PAN (its sends go to the effects)
 *   OUT 11/12  the shared effects' return (delay, reverb with HALL's width, chorus)
 *   OUT 13/14  the audio input, after its pedals, LEVEL and PAN
 * They are taken where the core sums them (fx.c KRST_EXT_TRACK, KRST_EXT_BUSES, KRST_EXT_WET; kx_input.h for the input),
 * at the master's volume, before the master's chain: the tracks and the effects add up to the mix before DAMAGE,
 * WALL and the rest. The audio flow only. */

#define KX_XCH 12                                 /* T1..T4 L R, FX L R, IN L R */
#define KX_XCH_FX 8
#define KX_XCH_IN 10
static int32_t kx_xo[KX_XCH][CTL];                /* this core block's parts */
static int32_t kx_xo_snap[2][CTL];                /* the mix before HALL's side (its width belongs to the effects) */
static int kx_xo_on;                              /* the host has more outputs and MULTI OUT is on (the audio flow) */
static int kx_xo_cfg = 1;                         /* MULTI OUT (kx_ic IC_XOUT, set where kx_ic is) */
static float kx_xo_peak[6];                       /* each pair's level, for the OUTPUTS page */

static void kx_xo_begin(void)                     /* fx.c KRST_EXT_BEGIN: a new block */
{
    if (kx_xo_on)
        memset(kx_xo, 0, sizeof kx_xo);
}

/* fx.c KRST_EXT_TRACK: the track's sum into the mix, as the core makes it (b mono; d the second voice of SPREAD) */
static void kx_xo_track(const struct track *tt, const int32_t *b, const int32_t *d, uint32_t n, int32_t lvl,
                        int32_t gl, int32_t gr, int32_t gld, int32_t grd)
{
    const track_t *t = (const track_t *)tt;
    uint32_t k = trk_index(t) % NTRK, i;
    int32_t *l = kx_xo[2u * k], *r = kx_xo[2u * k + 1u];
    if (!kx_xo_on)
        return;
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, y = d ? ((d[i] >> 2) * lvl) >> 10 : 0;
        l[i] += ((x * gl) >> 12) + ((y * gld) >> 12);
        r[i] += ((x * gr) >> 12) + ((y * grd) >> 12);
    }
}

static void kx_xo_buses(uint32_t n)               /* fx.c KRST_EXT_BUSES: the mix before HALL adds its side */
{
    if (!kx_xo_on)
        return;
    memcpy(kx_xo_snap[0], mix_l, n * sizeof mix_l[0]);
    memcpy(kx_xo_snap[1], mix_r, n * sizeof mix_r[0]);
}

static void kx_xo_wet(const int32_t *wet, uint32_t n)   /* fx.c KRST_EXT_WET: the buses' return, with HALL's side */
{
    uint32_t i;
    if (!kx_xo_on)
        return;
    for (i = 0; i < n; i++) {
        kx_xo[KX_XCH_FX][i] = wet[i] + (mix_l[i] - kx_xo_snap[0][i]);
        kx_xo[KX_XCH_FX + 1][i] = wet[i] + (mix_r[i] - kx_xo_snap[1][i]);
    }
}

static void kx_xo_input(const int32_t *l, const int32_t *r, uint32_t n)   /* kx_input.h: what the input adds */
{
    if (!kx_xo_on)
        return;
    memcpy(kx_xo[KX_XCH_IN], l, n * sizeof *l);
    memcpy(kx_xo[KX_XCH_IN + 1], r, n * sizeof *r);
}
