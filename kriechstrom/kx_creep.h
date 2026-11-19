/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the creep. Kriechstrom is the leakage current that crawls across worn insulation and leaves a
 * branching mark (a Lichtenberg figure). Here it is the picture of the master's wear: a figure grown once at the start
 * (a seeded random walk that forks), shown as far as DAMAGE and WALL have worn the mix, flickering with its level.
 * Drawn from the display flow only; it reads the audio flow's values (no writes). */

#define CR_MAX 900
typedef struct { int16_t x0, y0, x1, y1; uint8_t depth, order; } cr_seg;   /* in 1/16 of a unit square's 1024 */
static cr_seg cr[CR_MAX];
static uint32_t cr_n;
static uint32_t cr_rng = 0x4B525354u;             /* "KRST" */

static uint32_t cr_rand(void)
{
    cr_rng ^= cr_rng << 13;
    cr_rng ^= cr_rng >> 17;
    cr_rng ^= cr_rng << 5;
    return cr_rng;
}
static float cr_randf(void) { return (float)(cr_rand() & 0xFFFFu) / 65535.0f; }

/* a branch from (x, y) heading `a` (turns), `len` steps; it forks now and then. Coordinates 0..1024 */
static void cr_branch(float x, float y, float a, int len, int depth, int order)
{
    int i;
    for (i = 0; i < len && cr_n < CR_MAX; i++) {
        float step = 10.0f + cr_randf() * 8.0f - (float)depth * 1.2f;
        float nx, ny;
        a += (cr_randf() - 0.5f) * 0.12f;
        if (depth == 0)
            a += (0.25f - a) * 0.06f;             /* (the trunk keeps climbing) */
        nx = x + vd_sin(a + 0.25f) * step;         /* (cos) */
        ny = y - vd_sin(a) * step;
        if (nx < 4 || nx > 1020 || ny < 4 || ny > 1020)
            return;
        cr[cr_n].x0 = (int16_t)x;
        cr[cr_n].y0 = (int16_t)y;
        cr[cr_n].x1 = (int16_t)nx;
        cr[cr_n].y1 = (int16_t)ny;
        cr[cr_n].depth = (uint8_t)depth;
        cr[cr_n].order = (uint8_t)(order + i > 255 ? 255 : order + i);
        cr_n++;
        x = nx;
        y = ny;
        if (depth < 5 && i > 1 && cr_randf() < 0.24f - (float)depth * 0.035f)
            cr_branch(x, y, a + (cr_randf() < 0.5f ? -1.0f : 1.0f) * (0.05f + cr_randf() * 0.10f),
                      (len - i) * 2 / 3 - (int)(cr_randf() * 6.0f), depth + 1, order + i + 1);
    }
}

static void cr_init(void)
{
    cr_n = 0;
    /* from the bottom edge, up and out: the insulation's weak point at the middle of its foot */
    cr_branch(512, 1016, 0.25f, 78, 0, 0);
    cr_branch(508, 1016, 0.25f + 0.10f, 44, 1, 4);
    cr_branch(516, 1016, 0.25f - 0.10f, 44, 1, 4);
}

/* the figure in a box: `wear` 0..1 how much of it shows (by its order), `heat` 0..1 how hot (the level) */
static void cr_draw(int bx, int by, int bw, int bh, float wear, float heat, uint32_t col)
{
    uint32_t i, pass;
    float sx = (float)bw / 1024.0f, sy = (float)bh / 1024.0f, lim = 6.0f + wear * 250.0f;
    uint32_t core = vg_mix(col, 0xFFF4EA, 0.55f + 0.45f * heat), dim = vg_mix(0x0A0A0B, 0x8A2A14, 0.85f);
    for (pass = heat > 0.05f ? 0u : 1u; pass < 2u; pass++)
    for (i = 0; i < cr_n; i++) {
        const cr_seg *s = &cr[i];
        float o = (float)s->order, w;
        uint32_t c;
        if (o > lim)
            continue;
        if (!pass) {                              /* the glow: under everything, wide and dark, the trunk and its forks */
            if (s->depth < 2)
                vg_line((float)bx + s->x0 * sx, (float)by + s->y0 * sy, (float)bx + s->x1 * sx, (float)by + s->y1 * sy,
                        (6.0f - 2.0f * (float)s->depth) * (0.5f + heat), vg_mix(0x0D0D0F, col, 0.10f + 0.22f * heat));
            continue;
        }
        w = (3.2f - (float)s->depth * 0.55f) * (sx > sy ? sy : sx) * 2.2f;
        w = w < 0.7f ? 0.7f : w;
        /* white hot at the trunk, the forks red, their ends rust; the newest part (the front of the wear) dark */
        c = o > lim - 10.0f ? dim : s->depth == 0 ? core : s->depth == 1 ? vg_mix(col, core, 0.25f * heat) :
            vg_mix(col, dim, (float)(s->depth - 1) * 0.28f);
        vg_line((float)bx + s->x0 * sx, (float)by + s->y0 * sy, (float)bx + s->x1 * sx, (float)by + s->y1 * sy, w, c);
    }
}

/* how far the mix is worn: DAMAGE and WALL, a little of the others */
static float cr_wear(void)
{
    float w = (float)song.g[G_DMG] / 127.0f * 0.6f + (float)song.g[G_WALL] / 127.0f * 0.5f +
              (float)(song.g[G_SPLICE] + (song.g[G_HAUNT] ? song.g[G_HAMT] : 0) + song.g[G_HISS] + song.g[G_RES]) /
                  (127.0f * 4.0f) * 0.4f;
    return w > 1.0f ? 1.0f : w;
}
