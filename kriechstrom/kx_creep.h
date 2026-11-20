/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the creep. Kriechstrom is the leakage current that crawls across worn insulation and leaves a
 * branching mark (a Lichtenberg figure). Here it is the picture of the master's wear: a figure grown once at the start,
 * a discharge that zigzags round its heading and forks again and again into finer and finer filaments; shown as far as
 * DAMAGE and WALL have worn the mix. Drawn into pictures of its own when the wear changes (cr_cache), so a frame only
 * copies it and makes it as bright as the level. The display flow only; it reads the audio flow's values. */

#define CR_MAX 3000
typedef struct { int16_t x0, y0, x1, y1; uint8_t depth, order; } cr_seg;   /* in a 1024 square */
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

/* a discharge from (x, y) heading `head` (turns), `len` in units: short straight steps, each turned sharply off the
 * heading (a zigzag, not a curve); it forks often, the forks shorter and thinner. The forks wait in a queue, so the
 * strong discharges are grown first and the finest filaments last (where CR_MAX ends it) */
typedef struct { float x, y, head, len; uint8_t depth, order; } cr_fork;
#define CR_Q 1024
static cr_fork cr_q[CR_Q];
static uint32_t cr_qw, cr_qr;
static void cr_fork_add(float x, float y, float head, float len, int depth, int order)
{
    if (cr_qw - cr_qr < CR_Q && len > 6.0f) {
        cr_fork *f = &cr_q[cr_qw++ % CR_Q];
        f->x = x, f->y = y, f->head = head, f->len = len, f->depth = (uint8_t)depth;
        f->order = (uint8_t)(order > 255 ? 255 : order);
    }
}
static void cr_branch(float x, float y, float head, float len, int depth, int order)
{
    float a, gone = 0.0f;
    int i = 0;
    while (gone < len && cr_n < CR_MAX) {
        float step = (depth < 2 ? 7.0f : 4.0f) + cr_randf() * (depth < 2 ? 9.0f : 6.0f), nx, ny;
        a = head + (cr_randf() - 0.5f) * (depth < 1 ? 0.16f : 0.24f);   /* (up to ~40 degrees off, every step) */
        head += (cr_randf() - 0.5f) * 0.03f;        /* (the heading itself drifts a little) */
        nx = x + vd_sin(a + 0.25f) * step;
        ny = y - vd_sin(a) * step;
        if (nx < 2 || nx > 1022 || ny < 2 || ny > 1022)
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
        gone += step;
        i++;
        if (depth < 7 && cr_randf() < (depth < 2 ? 0.30f : 0.22f)) {
            float side = cr_randf() < 0.5f ? -1.0f : 1.0f;
            cr_fork_add(x, y, head + side * (0.05f + cr_randf() * 0.12f), (len - gone) * (0.35f + cr_randf() * 0.35f),
                        depth + 1, order + i);
        }
    }
}

static void cr_init(void)
{
    cr_n = 0;
    cr_qw = cr_qr = 0;
    /* from the foot of the insulation, up: one main discharge and two weaker ones beside it */
    cr_fork_add(512, 1020, 0.25f, 900, 0, 0);
    cr_fork_add(500, 1020, 0.25f + 0.09f, 420, 1, 10);
    cr_fork_add(524, 1020, 0.25f - 0.09f, 420, 1, 10);
    while (cr_qr != cr_qw && cr_n < CR_MAX) {
        cr_fork f = cr_q[cr_qr++ % CR_Q];
        cr_branch(f.x, f.y, f.head, f.len, f.depth, f.order);
    }
}

/* the figure in a box: `wear` 0..1 how much of it shows (by its order), `heat` 0..1 how hot. bg: the background's
 * colours (dark) */
static void cr_draw_in(int bx, int by, int bw, int bh, float wear, float heat, uint32_t col, int bg)
{
    uint32_t i, pass;
    float sx = (float)bw / 1024.0f, sy = (float)bh / 1024.0f, s = sx < sy ? sx : sy, lim = 8.0f + wear * 248.0f;
    uint32_t core = bg ? vg_mix(col, 0xD05030, 0.55f) : vg_mix(col, 0xFFF4EA, 0.65f + 0.35f * heat);
    uint32_t far = vg_mix(0x0A0A0B, col, 0.5f);
    for (pass = bg ? 1u : 0u; pass < 2u; pass++)
        for (i = 0; i < cr_n; i++) {
            const cr_seg *g = &cr[i];
            float o = (float)g->order, w, f;
            uint32_t c;
            if (o > lim)
                continue;
            if (!pass) {                          /* the glow, under the two strongest: wide, dark */
                if (g->depth < 2)
                    vg_line((float)bx + g->x0 * sx, (float)by + g->y0 * sy, (float)bx + g->x1 * sx,
                            (float)by + g->y1 * sy, (7.0f - 3.0f * (float)g->depth) * s * (1.0f + heat),
                            vg_mix(0x0D0D0F, col, 0.14f + 0.2f * heat));
                continue;
            }
            w = (g->depth == 0 ? 4.4f : g->depth == 1 ? 2.6f : g->depth == 2 ? 1.6f : 1.0f) * s * 1.6f;
            w = w < 0.55f ? 0.55f : w;
            f = (float)(g->depth > 2 ? g->depth - 2 : 0) * 0.2f;
            c = g->depth == 0 ? core : g->depth == 1 ? vg_mix(col, core, 0.35f) : vg_mix(col, far, f > 1.0f ? 1.0f : f);
            if (o > lim - 8.0f)                   /* (the front of the wear: just reached) */
                c = vg_mix(c, 0x0A0A0B, 0.5f);
            vg_line((float)bx + g->x0 * sx, (float)by + g->y0 * sy, (float)bx + g->x1 * sx, (float)by + g->y1 * sy, w, c);
        }
}

static void cr_draw(int bx, int by, int bw, int bh, float wear, float heat, uint32_t col)
{
    cr_draw_in(bx, by, bw, bh, wear, heat, col, 0);
}

/* how far the mix is worn: DAMAGE and WALL, a little of the others */
static float cr_wear(void)
{
    float w = (float)song.g[G_DMG] / 127.0f * 0.6f + (float)song.g[G_WALL] / 127.0f * 0.5f +
              (float)(song.g[G_SPLICE] + (song.g[G_HAUNT] ? song.g[G_HAMT] : 0) + song.g[G_HISS] + song.g[G_RES]) /
                  (127.0f * 4.0f) * 0.4f;
    return w > 1.0f ? 1.0f : w;
}

/* a cached picture of the figure: drawn again only when the wear moved; a frame copies it, as bright as k (256 = as
 * drawn). The stage's and the MASTER page's boxes; `restore` is the picture being drawn (vg_* go back to it) */
typedef struct { uint16_t *px; int w, h, wear; } cr_cache_t;
static uint16_t cr_px_stage[352 * 346], cr_px_master[48 * 48];
static cr_cache_t cr_stage = {cr_px_stage, 352, 346, -1}, cr_master = {cr_px_master, 48, 48, -1};
static void cr_cache_blit(cr_cache_t *c, uint16_t *fb, int fw, int fh, int x, int y, float wear, int k)
{
    int q = (int)(wear * 60.0f + 0.5f), j, i;
    if (q != c->wear) {
        c->wear = q;
        vg_begin_size(c->px, c->w, c->h);
        vg_clear(0x0D0D0F);
        cr_draw_in(0, 0, c->w, c->h, (float)q / 60.0f, 0.7f, 0xE01818, 0);
        vg_begin_size(fb, fw, fh);
    }
    for (j = 0; j < c->h && y + j < fh; j++)
        for (i = 0; i < c->w && x + i < fw; i++) {
            uint16_t p = c->px[j * c->w + i];
            int r = ((p >> 11) & 31) * k >> 8, g = ((p >> 5) & 63) * k >> 8, b = (p & 31) * k >> 8;
            fb[(y + j) * fw + x + i] = (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
        }
}
