/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: pictures on the pages (the control flow, reading the core): FM6's algorithm as operators and
 * their wires, the mixer's needle meters, the master's tape reels, the drum lanes' glyphs, the idle screen. */

/* ---- FM6: the algorithm (fm6_core.c FM6_ALG: per operator, the sixth first, its in bus, out bus, add, feedback) drawn as
 * the DX7 draws it: the carriers in a row at the bottom, their modulators stacked above, wires down, feedback as a loop */
static void kx_draw_fm6(int x, int y, int w, int h, uint32_t tr, int focus_op)
{
    const uint8_t *a = FM6_ALG[fm6_eff[tr % NTRK].alg & 31u];
    uint8_t mods[6], nm[6][6], tgt[6], bus[3][6], nb[3] = {0, 0, 0}, depth[6], col[6];
    uint32_t k, i, j, ncar = 0;
    memset(nm, 0, sizeof nm);
    memset(mods, 0, sizeof mods);
    memset(tgt, 0xFF, sizeof tgt);
    for (k = 0; k < 6u; k++) {                    /* who modulates whom, by running the buses as the core does */
        uint32_t f = a[k], ib = (f >> 4) & 3u, ob = f & 3u;
        if (ib)
            for (i = 0; i < nb[ib]; i++) {
                nm[k][mods[k]++] = bus[ib][i];
                tgt[bus[ib][i]] = (uint8_t)k;
            }
        if (!(f & FM6_OADD))
            nb[ob] = 0;
        bus[ob][nb[ob]++] = (uint8_t)k;
    }
    for (k = 6u; k-- > 0u;) {                     /* depth: 0 for a carrier, its target's + 1 for a modulator */
        uint32_t d = 0, t = k;
        while (tgt[t] != 0xFF && d < 6u)
            t = tgt[t], d++;
        depth[k] = (uint8_t)d;
    }
    for (k = 6u; k-- > 0u;)                       /* the carriers' columns, OP1 first (k 5) */
        if ((a[k] & 3u) == 0u && tgt[k] == 0xFF)
            col[k] = (uint8_t)ncar++;
    for (j = 1; j < 6u; j++)                      /* each modulator over its target (two on one: side by side) */
        for (k = 6u; k-- > 0u;)
            if (depth[k] == j) {
                uint32_t n = 0;
                col[k] = col[tgt[k]];
                for (i = 0; i < 6u; i++)
                    if (i != k && depth[i] == j && tgt[i] == tgt[k] && i > k)
                        n++;
                col[k] = (uint8_t)(col[k] + n);
                if (col[k] + 1u > ncar)
                    ncar = col[k] + 1u;
            }
    {
        int bw = 30, bh = 22, cw = w / (int)(ncar > 0u ? ncar : 1u), rows = 1, ox, oy;
        char num[2] = {0, 0};
        for (k = 0; k < 6u; k++)
            rows = depth[k] + 1 > rows ? depth[k] + 1 : rows;
        cw = cw > 70 ? 70 : cw;
        ox = x + (w - cw * (int)ncar) / 2;
        oy = y + h - bh - 14;
        vg_rect(ox, y + h - 6, cw * (int)ncar, 2, C_DIM);          /* the output */
        for (k = 0; k < 6u; k++) {                /* the wires first */
            int cx = ox + col[k] * cw + cw / 2, cy = oy - (int)depth[k] * (bh + 12);
            if (tgt[k] != 0xFF) {
                int tx = ox + col[tgt[k]] * cw + cw / 2, ty = oy - (int)depth[tgt[k]] * (bh + 12);
                vg_line((float)cx, (float)(cy + bh), (float)tx, (float)ty, 2.0f, 0x55555C);
            } else
                vg_line((float)cx, (float)(cy + bh), (float)cx, (float)(y + h - 6), 2.0f, 0x55555C);
        }
        for (k = 0; k < 6u; k++) {
            int cx = ox + col[k] * cw + cw / 2, cy = oy - (int)depth[k] * (bh + 12), opn = 6 - (int)k;
            int car = tgt[k] == 0xFF, foc = opn - 1 == focus_op;
            uint32_t fill = foc ? C_RED : car ? vg_mix(C_BG, TRK_RGB[tr], 0.7f) : 0x26262A;
            vg_round(cx - bw / 2, cy, bw, bh, 3, fill);
            if (foc)
                vg_frame(cx - bw / 2 - 2, cy - 2, bw + 4, bh + 4, 1, C_WHITE);
            num[0] = (char)('0' + opn);
            vg_text(num, cx, cy + 3, VG_BOLD, C_WHITE, VG_CENTER);
            if ((a[k] & 0xC0u) == 0xC0u) {        /* feedback: a loop from its top back into it */
                vg_line((float)(cx + bw / 2), (float)(cy + 4), (float)(cx + bw / 2 + 8), (float)(cy + 4), 2.0f, C_RED);
                vg_line((float)(cx + bw / 2 + 8), (float)(cy + 4), (float)(cx + bw / 2 + 8), (float)(cy - 6), 2.0f, C_RED);
                vg_line((float)(cx + bw / 2 + 8), (float)(cy - 6), (float)cx, (float)(cy - 6), 2.0f, C_RED);
                vg_line((float)cx, (float)(cy - 6), (float)cx, (float)cy, 2.0f, C_RED);
            }
        }
        {
            char b[12] = "ALG ";
            vg_itoa((int)(fm6_eff[tr % NTRK].alg & 31u) + 1, b + 4);
            vg_text(b, x + w, y, VG_BOLD, C_DIM, VG_RIGHT);
        }
    }
}

/* ---- the mixer: four needle meters (the needles with weight: they rise fast, fall slow), the level under each */
static float kx_vu[NTRK], kx_vu_pk[NTRK];
static void kx_draw_vu(int x, int y, int w, int h, uint32_t k)
{
    const track_t *t = &trk[k];
    float a = (float)clamp(t->peak, 0, 32767) / 32768.0f, ang, cx = (float)x + (float)w * 0.5f, cy = (float)(y + h - 20);
    float r = (float)w * 0.44f;
    int i;
    char b[8];
    a = t->p[P_MUTE] ? 0.0f : a;
    a = a > 0.0f ? 1.0f + vd_log2(a + 1e-6f) * 0.06f : 0.0f;    /* (dB-ish: -16 dB at the left end) */
    a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
    kx_vu[k] += (a - kx_vu[k]) * (a > kx_vu[k] ? 0.45f : 0.12f);
    kx_vu_pk[k] = kx_vu[k] > kx_vu_pk[k] ? kx_vu[k] : kx_vu_pk[k] * 0.985f;
    vg_round(x, y, w, h, 6, 0xD8D2C2);                          /* the face: paper */
    vg_round(x + 3, y + 3, w - 6, h - 6, 5, 0xEAE4D4);
    for (i = 0; i <= 10; i++) {                                 /* the scale; its top fifth red */
        float t2 = (float)i / 10.0f, an = 0.375f - 0.25f * t2;   /* (135 degrees .. 45, in turns) */
        float sx = cx + vd_sin(an + 0.25f) * r, sy = cy - vd_sin(an) * r;
        float ex = cx + vd_sin(an + 0.25f) * (r - (i % 5 ? 5.0f : 9.0f)), ey = cy - vd_sin(an) * (r - (i % 5 ? 5.0f : 9.0f));
        vg_line(sx, sy, ex, ey, 1.5f, i >= 8 ? 0xC02010 : 0x2A2A2E);
    }
    ang = 0.375f - 0.25f * kx_vu[k];
    vg_line(cx, cy, cx + vd_sin(ang + 0.25f) * (r - 2.0f), cy - vd_sin(ang) * (r - 2.0f), 2.0f, 0x111111);
    vg_circle(cx, cy, 4.0f, 0x111111);
    vg_circle((float)(x + w - 12), (float)(y + 12), 4.0f, kx_vu_pk[k] > 0.85f ? 0xE02010 : 0x5A1A10);
    b[0] = 'T', b[1] = (char)('1' + k), b[2] = 0;
    vg_text(b, x + 8, y + 6, VG_BOLD, TRK_RGB[k] == 0xD9D2C2 ? 0x6A6A70 : TRK_RGB[k], VG_LEFT);
    vg_itoa(t->p[P_LEVEL], b);
    vg_text(b, (int)cx, y + h - 18, VG_SMALL, 0x2A2A2E, VG_CENTER);
    if (t->p[P_MUTE])
        vg_text("MUTE", (int)cx, y + 22, VG_BOLD, 0xC02010, VG_CENTER);
}

/* ---- the master's tape: two reels, turning while it plays (DRAG slows them on the bar's last beat), the tape between
 * them showing the mix */
static float kx_reel_ph;
static uint64_t kx_reel_t;
static void kx_draw_reels(int x, int y, int w, int h)
{
    float speed = song.playing ? 1.0f : 0.0f, r = (float)h * 0.42f, lx = (float)x + r + 4.0f, rx = (float)(x + w) - r - 4.0f;
    float cy = (float)y + (float)h * 0.5f, dt;
    uint64_t now = vibe_frames();
    int k, s;
    if (song.g[G_HAUNT] == 2 && song.playing && trk[song.sel].seq_idx % 16u >= 12u)
        speed *= 1.0f - (float)song.g[G_HAMT] / 160.0f;         /* (DRAG: the tape slows) */
    dt = kx_reel_t ? (float)(now - kx_reel_t) / kx_sr : 0.0f;
    kx_reel_t = now;
    kx_reel_ph += dt * speed * 0.35f * (float)song.g[G_BPM] / 100.0f;
    kx_reel_ph -= (float)(int)kx_reel_ph;
    for (s = 0; s < 2; s++) {
        float cx = s ? rx : lx, rr = s ? r * 0.9f : r;          /* (the right one has more tape on it) */
        vg_circle(cx, cy, rr, 0x2A1A14);                        /* the tape wound on it */
        vg_circle(cx, cy, rr * 0.45f, 0x6A6A70);                /* the hub */
        vg_circle(cx, cy, rr * 0.18f, 0x111113);
        for (k = 0; k < 3; k++) {                                /* three spokes, turning */
            float an = kx_reel_ph * (s ? 1.1f : 1.0f) + (float)k / 3.0f;
            vg_line(cx + vd_sin(an + 0.25f) * rr * 0.2f, cy - vd_sin(an) * rr * 0.2f,
                    cx + vd_sin(an + 0.25f) * rr * 0.43f, cy - vd_sin(an) * rr * 0.43f, 3.0f, 0x111113);
        }
    }
    {   /* the tape between them: its lower run carries the mix */
        uint32_t sw = kx_scope_w, i, n = (uint32_t)(rx - lx);
        float py = 0.0f, by = cy + r * 0.92f;
        vg_line(lx, cy - r, rx, cy - r * 0.9f, 2.0f, 0x3A2A20);
        for (i = 0; i < n; i++) {
            int32_t v = kx_scope[(sw - n * 2u + i * 2u) & (KX_SCOPE - 1u)];
            float yy = by - (float)v * (r * 0.5f / 32768.0f);
            if (i)
                vg_line(lx + (float)i - 1.0f, py, lx + (float)i, yy, 1.5f, C_RED);
            py = yy;
        }
    }
}

/* ---- the drum lanes' glyphs: what each lane is, lit as it hits (the SEQ page's drum grid, at the left) */
static float kx_glyph[NLANE];
static uint32_t kx_glyph_step = 0xFFFFu;
static void kx_draw_glyph(int x, int y, int s, uint32_t l, float lit, uint32_t col)
{
    uint32_t c = vg_mix(0x3A3A40, col, lit < 0.0f ? 0.0f : lit > 1.0f ? 1.0f : lit);
    float cx = (float)x + (float)s * 0.5f, cy = (float)y + (float)s * 0.5f, r = (float)s * 0.42f;
    switch (l) {
    case 0: vg_circle(cx, cy, r, c); vg_circle(cx, cy, r * 0.55f, C_BG); break;              /* KICK: a drum's face */
    case 1: vg_round(x + 1, y + 2, s - 2, s - 4, 2, c); vg_line(cx - r, cy, cx + r, cy, 1.0f, C_BG); break;   /* SNARE */
    case 2: vg_tri(cx - r, cy + r, cx, cy - r, cx + r, cy + r, c); break;                   /* CLAP */
    case 3: vg_line(cx - r, cy, cx + r, cy, 2.0f, c); vg_line(cx - r * 0.6f, cy - 2.0f, cx + r * 0.6f, cy - 2.0f, 2.0f, c); break;   /* HAT closed */
    case 4: vg_line(cx - r, cy + 2.0f, cx + r, cy + 2.0f, 2.0f, c); vg_line(cx - r * 0.6f, cy - 3.0f, cx + r * 0.6f, cy - 3.0f, 2.0f, c); break;   /* open */
    case 5: vg_circle(cx, cy, r * 0.8f, c); break;                                          /* TOM */
    case 6: vg_line(cx - r, cy + r, cx + r, cy - r, 2.0f, c); break;                        /* RIM */
    default: vg_tri(cx, cy - r, cx - r * 0.7f, cy + r, cx + r * 0.7f, cy + r, c); vg_circle(cx, cy + r * 0.4f, 2.0f, C_BG); break;   /* BELL */
    }
}
static void kx_glyph_tick(const track_t *t)       /* a lane that hits on the step playing now lights up, then fades */
{
    uint32_t l, s = t->seq_idx % NSTEP;
    for (l = 0; l < NLANE; l++)
        kx_glyph[l] *= 0.8f;
    if (song.playing && s != kx_glyph_step) {
        uint32_t hits = step_lanes(&t->step[s]);
        kx_glyph_step = s;
        for (l = 0; l < NLANE; l++)
            if ((hits >> l) & 1u)
                kx_glyph[l] = 1.0f;
    }
}

