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

/* ---- the mixer: four needle meters (the needles with weight: they rise fast, fall slow). The face is an old VU's: aged
 * paper, the scale in dB from -20 to +3 (0 VU at -6 dBFS), its red zone, the pivot hidden under the face's edge. Under
 * it the strip: the level big, the pan, the three sends, and the track's spark, as hard as it plays */
static float kx_vu[NTRK], kx_vu_pk[NTRK];
static float kx_vu_pos(float db) { db = db < -20.0f ? -20.0f : db > 3.0f ? 3.0f : db; return (db + 20.0f) / 23.0f; }
static void kx_draw_vu(int x, int y, int w, int h, uint32_t k)
{
    const track_t *t = &trk[k];
    float a = kx_trk_pk[k], ang, cx = (float)x + (float)w * 0.5f;
    int fh = h * 56 / 100, i, sy = y + fh + 4;
    float r = (float)fh * 0.74f, cy = (float)(y + fh) + r * 0.2f, span = 0.085f;   /* (+-31 degrees) */
    uint32_t col = TRK_RGB[k] == 0xD9D2C2 ? 0x9A9AA2 : TRK_RGB[k], ink = 0x2A2622;
    char b[8];
    static const int8_t DB[] = {-20, -10, -7, -5, -3, -1, 0, 1, 2, 3};
    static const char *const DBN = "-  +";
    a = t->p[P_MUTE] ? 0.0f : a;
    a = a > 0.0f ? kx_vu_pos(6.02f * vd_log2(a + 1e-6f) + 6.0f) : 0.0f;
    kx_vu[k] += (a - kx_vu[k]) * (a > kx_vu[k] ? 0.45f : 0.12f);
    kx_vu_pk[k] = kx_vu[k] > kx_vu_pk[k] ? kx_vu[k] : kx_vu_pk[k] * 0.985f;
    vg_rect(x, y, w, fh, 0x18181A);                            /* the bezel, the face: aged paper */
    vg_rect(x + 3, y + 3, w - 6, fh - 6, 0xCFC5AA);
    vg_rect(x + 3, y + 3, w - 6, 10, 0xD8CFB6);
    for (i = 0; i < 2; i++) {                                  /* the red zone, a band over the scale from 0 */
        float p0 = kx_vu_pos(0.0f), p1 = 1.0f, j;
        for (j = p0; j < p1; j += 0.02f) {
            float a0 = 0.25f + span - 2.0f * span * j, a1 = 0.25f + span - 2.0f * span * (j + 0.02f);
            float rr = r + 4.0f + (float)i;
            vg_line(cx + vd_sin(a0 + 0.25f) * rr, cy - vd_sin(a0) * rr, cx + vd_sin(a1 + 0.25f) * rr, cy - vd_sin(a1) * rr,
                    1.6f, 0xB02818);
        }
    }
    {   /* the scale's arc */
        float j;
        for (j = 0.0f; j < 0.999f; j += 0.05f) {
            float a0 = 0.25f + span - 2.0f * span * j, a1 = 0.25f + span - 2.0f * span * (j + 0.05f);
            vg_line(cx + vd_sin(a0 + 0.25f) * r, cy - vd_sin(a0) * r, cx + vd_sin(a1 + 0.25f) * r, cy - vd_sin(a1) * r, 1.0f,
                    j >= kx_vu_pos(0.0f) - 0.01f ? 0xB02818 : ink);
        }
    }
    for (i = 0; i < (int)sizeof DB; i++) {                    /* the ticks, the numbers on the big ones */
        float p = kx_vu_pos((float)DB[i]), an = 0.25f + span - 2.0f * span * p, big = DB[i] == -20 || DB[i] == -10 ||
                  DB[i] == -5 || DB[i] == 0 || DB[i] == 3, num = DB[i] == -10 || DB[i] == -5 || DB[i] == 0;
        uint32_t c = DB[i] >= 0 ? 0xB02818 : ink;
        float ux = vd_sin(an + 0.25f), uy = -vd_sin(an);
        vg_line(cx + ux * r, cy + uy * r, cx + ux * (r + (big ? 7.0f : 4.0f)), cy + uy * (r + (big ? 7.0f : 4.0f)), 1.2f, c);
        if (num) {
            vg_itoa(DB[i] < 0 ? -DB[i] : DB[i], b);
            vg_text(b, (int)(cx + ux * (r + 13.0f)), (int)(cy + uy * (r + 13.0f)) - 7, VG_SMALL, c, VG_CENTER);
        }
    }
    (void)DBN;
    vg_text("-", x + 8, y + 8, VG_BOLD, ink, VG_LEFT);
    vg_text("+", x + w - 8, y + 8, VG_BOLD, 0xB02818, VG_RIGHT);
    vg_text("VU", (int)cx, (int)(cy - r * 0.55f) - 7, VG_BOLD, ink, VG_CENTER);
    b[0] = 'T', b[1] = (char)('1' + k), b[2] = 0;
    vg_text(b, x + 8, y + fh - 22, VG_BOLD, vg_mix(col, 0x000000, 0.3f), VG_LEFT);
    vg_circle((float)(x + w - 13), (float)(y + fh - 14), 4.5f, 0x2A2622);   /* the peak lamp */
    vg_circle((float)(x + w - 13), (float)(y + fh - 14), 3.5f, kx_vu_pk[k] > kx_vu_pos(0.0f) ? 0xF03018 : 0x5A1A10);
    ang = 0.25f + span - 2.0f * span * kx_vu[k];               /* the needle, from under the face's edge */
    {
        float ux = vd_sin(ang + 0.25f), uy = -vd_sin(ang), r0 = r * 0.24f;
        vg_line(cx + ux * r0 + 1.5f, cy + uy * r0 + 1.5f, cx + ux * (r + 3.0f) + 1.5f, cy + uy * (r + 3.0f) + 1.5f, 1.6f,
                0xA89E86);                                     /* (its shadow on the paper) */
        vg_line(cx + ux * r0, cy + uy * r0, cx + ux * (r + 3.0f), cy + uy * (r + 3.0f), 1.6f, 0x111111);
    }
    vg_rect(x + 3, y + fh - 6, w - 6, 3, 0x18181A);
    vg_rect(x, y + fh - 3, w, 3, 0x18181A);
    if (t->p[P_MUTE])
        vg_text("MUTE", (int)cx, y + 20, VG_BOLD, 0xB02818, VG_CENTER);
    /* the strip under it */
    vg_rect(x, sy, w, y + h - sy, C_PANEL);
    vg_rect(x, sy, w, 2, t->p[P_MUTE] ? 0x2A2A2E : col);
    vg_itoa(t->p[P_LEVEL], b);
    vg_text(b, x + 6, sy + 6, VG_BIG, t->p[P_MUTE] ? C_DIM : C_WHITE, VG_LEFT);
    vg_text("LEVEL", x + w - 6, sy + 8, VG_SMALL, C_DIM, VG_RIGHT);
    {   /* the pan: a mark on a line, L .. R */
        int px = x + 8 + (int)((float)(clamp(t->p[P_PAN], -64, 63) + 64) / 127.0f * (float)(w - 16)), ly = sy + 38;
        vg_rect(x + 8, ly, w - 16, 1, C_LINE);
        vg_rect(x + w / 2, ly - 3, 1, 7, C_LINE);
        vg_rect(px - 1, ly - 4, 3, 9, col);
        vg_text("L", x + 2, ly - 7, VG_SMALL, 0x7A7A84, VG_LEFT);
        vg_text("R", x + w - 2, ly - 7, VG_SMALL, 0x7A7A84, VG_RIGHT);
    }
    {   /* the sends: CHO, DLY, REV, three short bars */
        static const char *const SN[3] = {"C", "D", "R"};
        static const uint8_t SP[3] = {P_CHOR, P_DLY, P_REV};
        int bw = (w - 16 - 3 * 12) / 3, j;
        for (j = 0; j < 3; j++) {
            int bx = x + 6 + j * (bw + 14), by = sy + 52;
            float f = (float)clamp(t->p[SP[j]], 0, 127) / 127.0f;
            vg_text(SN[j], bx, by - 4, VG_SMALL, 0x7A7A84, VG_LEFT);
            vg_rect(bx + 11, by + 2, bw, 4, C_LINE);
            vg_rect(bx + 11, by + 2, (int)(f * (float)bw), 4, vg_mix(col, C_BG, 0.25f));
        }
    }
{   /* the track's level as a row of lamps */
        int j, n = 12, lw = (w - 12) / n;
        for (j = 0; j < n; j++)
            vg_rect(x + 6 + j * lw, y + h - 10, lw - 2, 5,
                    (float)j / (float)n < kx_vu[k] ? (j >= n - 2 ? 0xF03018 : col == 0x9A9AA2 ? 0xD8D8DC : col) : 0x26262A);
    }
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

