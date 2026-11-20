/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the display, 480 x 320 (the control flow; it reads the core, never writes it).
 *   0 .. 28    the name, the six families (the keys right of ENTER), the tempo, PLAY and REC
 *   32 .. 72   the four tracks: engine, sound, level, MUTE
 *   76 .. 206  the page: its title and pages, and its picture (an envelope, the steps, the creep ..)
 *   210 .. 282 the four values on encoders 1-4
 *   286 .. 316 F1-F6 */

static uint64_t kx_now_ms(void) { return vibe_frames() * 1000u / (uint64_t)(kx_sr > 0 ? kx_sr : 48000.0f); }

static void kx_logo(int x, int y, int font)
{
    int w = vg_text("KRIECH", x, y, font, C_WHITE, VG_LEFT);
    vg_text("STROM", x + w, y, font, C_RED, VG_LEFT);
}

/* a line with a glow under it */
static void kx_gline(float x0, float y0, float x1, float y1, float w, uint32_t col)
{
    vg_line(x0, y0, x1, y1, w * 3.0f, vg_mix(C_BG, col, 0.18f));
    vg_line(x0, y0, x1, y1, w, col);
}

/* a value as a ladder of segments, the way a meter on a rack shows it (bip: from the middle) */
static void kx_ladder(int x, int y, int w, int h, float pos, int bip, uint32_t col)
{
    int n = 16, i, sw = (w - (n - 1) * 2) / n;
    int lit = (int)(pos * (float)n + 0.5f), mid = n / 2;
    for (i = 0; i < n; i++) {
        int on = bip ? (lit >= mid ? i >= mid && i < lit : i >= lit && i < mid) : i < lit;
        vg_rect(x + i * (sw + 2), y, sw, h, on ? (i == n - 1 && !bip ? C_WHITE : col) : 0x222226);
    }
}

static void kx_draw_header(void)
{
    static const char *const K[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
    char b[16];
    uint32_t i;
    int x = 120;
    kx_logo(8, 6, VG_BOLD);
    for (i = 0; i < 6u; i++) {                    /* the families (K1 .. K6): the open one red, underlined */
        int w = vg_text_width(K[i], VG_SMALL), on = kx_fam == i;
        vg_text(K[i], x, 7, VG_SMALL, on ? C_WHITE : 0x55555C, VG_LEFT);
        if (on)
            vg_rect(x, 22, w, 2, C_RED);
        x += w + 12;
    }
    if (kx_fam == FA_MENU) {
        vg_text("MENU", x, 7, VG_SMALL, C_WHITE, VG_LEFT);
        vg_rect(x, 22, vg_text_width("MENU", VG_SMALL), 2, C_RED);
    }
    vg_itoa(song.g[G_BPM], b);
    vg_text(b, 418, 4, VG_BIG, C_WHITE, VG_RIGHT);
    if (song.playing)
        vg_tri(430, 7, 430, 21, 442, 14, C_RED);
    else
        vg_rect(431, 8, 11, 11, 0x55555C);
    vg_circle(462, 14, 5, song.rec & (1u << song.sel) ? C_RED : 0x2A2A2E);
    if (kx_shift) {
        vg_round(330, 5, 44, 18, 3, C_RED);
        vg_text("SHIFT", 352, 7, VG_SMALL, C_WHITE, VG_CENTER);
    }
}

/* the four tracks: a line in its colour that is its level as it plays, its sound under it */
static void kx_draw_tracks(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        int x = 8 + (int)i * 118, sel = i == song.sel, mute = t->p[P_MUTE] != 0;
        uint32_t col = TRK_RGB[i], ink = mute ? 0x3A3A40 : sel ? C_WHITE : 0x9A9AA2;
        char nm[17], n[2] = {(char)('1' + i), 0};
        float a = (float)clamp(t->peak, 0, 32767) / 32768.0f;
        vg_rect(x, 32, 110, sel ? 3 : 1, mute ? 0x2A2A2E : sel ? col : vg_mix(C_BG, col, 0.45f));
        if (!mute)
            vg_rect(x, 32, (int)(a * 110.0f), 3, vg_mix(col, C_WHITE, 0.3f));
        vg_text(n, x, 39, VG_BOLD, mute ? 0x3A3A40 : col, VG_LEFT);
        vg_text(ENGINES[eng_idx(t->eng_req)]->name, x + 13, 40, VG_SMALL, mute ? 0x3A3A40 : 0x6A6A72, VG_LEFT);
        if (mute)
            vg_text("MUTE", x + 110, 40, VG_SMALL, C_RED, VG_RIGHT);
        else if (song.rec & (1u << i))
            vg_circle((float)(x + 106), 47.0f, 3.0f, C_RED);
        sound_name(t, nm);
        nm[13] = 0;
        vg_text(nm, x, 55, VG_MED, ink, VG_LEFT);
    }
}

/* the page's title, the line under it, and where it is among its family's pages */
static void kx_draw_title(const char *t, const char *what)
{
    uint32_t i;
    vg_text(t, 8, 78, VG_BOLD, C_RED, VG_LEFT);
    if (what)
        vg_text(what, 8 + vg_text_width(t, VG_BOLD) + 10, 78, VG_SMALL, 0x55555C, VG_LEFT);
    for (i = 0; i < kx_pn && kx_pn > 1u; i++) {
        int x = 470 - (int)(kx_pn - 1u - i) * 10;
        vg_rect(x - 2, i == kx_fpage[kx_fam] ? 81 : 83, 5, i == kx_fpage[kx_fam] ? 7 : 3,
                i == kx_fpage[kx_fam] ? C_RED : 0x3A3A40);
    }
}

/* the four values on encoders 1-4: big, nothing round them but a hairline */
static void kx_draw_cards(int pg, int y)
{
    uint32_t k;
    uint32_t col = TRK_RGB[song.sel];
    if ((pg >= 0 && pg < (int)NPAGES && PAGES[pg].scope == SC_GLOBAL) || IS_IC_PAGE(pg) || pg == PG_PEDALS_IN ||
        pg == PG_PEDALS_M)
        col = C_RED;
    vg_rect(8, y - 4, 464, 1, 0x26262A);
    for (k = 0; k < 4u; k++) {
        kx_card c;
        int x = 8 + (int)k * 118, w = 110, f = VG_BIG | VG_X2;
        kx_card_of(pg, k, &c);
        if (k)
            vg_rect(x - 5, y + 2, 1, 66, 0x26262A);
        if (!c.on)
            continue;
        {   /* the value just turned: its card lights up and fades over 0.6 s */
            float age = (float)(vibe_frames() - kx_card_hot[k]) / (kx_sr * 0.6f);
            if (kx_card_hot[k] && age < 1.0f) {
                float a = (1.0f - age) * (1.0f - age);
                vg_rect_a(x - 4, y - 2, w + 8, 72, col, 0.22f * a);
                vg_rect(x - 4, y - 3, (int)((float)(w + 8) * a), 2, col);
            }
        }
        vg_text(c.label, x, y + 2, VG_SMALL, 0x6A6A72, VG_LEFT);
        if (vg_text_width(c.val, f) > w - 4)
            f = VG_BIG;
        vg_text(c.val, x - 1, f == VG_BIG ? y + 28 : y + 16, f, C_WHITE, VG_LEFT);
        if (c.unit && c.unit[0])
            vg_text(c.unit, x + w, y + 2, VG_SMALL, 0x55555C, VG_RIGHT);
        kx_ladder(x, y + 64, w, 4, c.pos, c.bip, col);
    }
}

static void kx_draw_soft(void)
{
    static const char *const T[4] = {"T1", "T2", "T3", "T4"};
    uint32_t i;
    for (i = 0; i < 6u; i++) {
        int x = 2 + (int)i * 80;
        const char *s = i < 4u ? T[i] : i == 4u ? kx_f5_name() : "UNDO";
        int sel = i < 4u && i == song.sel;
        if (sel)
            vg_rect(x + 4, 288, 70, 28, vg_mix(C_BG, TRK_RGB[i], 0.85f));
        else if (i < 4u)
            vg_rect(x + 4, 313, 70, 3, vg_mix(C_BG, TRK_RGB[i], 0.5f));
        else if (s[0])
            vg_rect(x + 4, 313, 70, 3, 0x3A3A40);
        {   /* pressed: it flashes white and fades (0.25 s) */
            float age = (float)(vibe_frames() - kx_soft_t[i]) / (kx_sr * 0.25f);
            if (kx_soft_t[i] && age < 1.0f)
                vg_rect_a(x + 4, 288, 70, 28, C_WHITE, 0.6f * (1.0f - age));
        }
        vg_text(s, x + 39, 293, VG_BOLD, sel ? C_BG : i < 4u ? vg_mix(0x6A6A72, TRK_RGB[i], 0.5f) : C_TEXT, VG_CENTER);
    }
}

/* the master's scope (44.1 kHz, the last w * 2 frames), glowing */
static void kx_draw_scope(int x, int y, int w, int h, uint32_t col, float gain)
{
    uint32_t sw = kx_scope_w, i;
    float mid = (float)y + (float)h * 0.5f, k = (float)h * 0.5f * gain / 32768.0f, px = 0, py = 0;
    vg_rect(x, (int)mid, w, 1, 0x1C1C20);
    for (i = 0; i + 1u < (uint32_t)w; i++) {
        int32_t a = kx_scope[(sw - (uint32_t)w * 2u + i * 2u) & (KX_SCOPE - 1u)];
        float ya = mid - (float)a * k;
        ya = ya < (float)y ? (float)y : ya > (float)(y + h) ? (float)(y + h) : ya;
        if (i)
            vg_line(px, py, (float)(x + (int)i), ya, 4.0f, vg_mix(C_BG, col, 0.16f));
        px = (float)(x + (int)i);
        py = ya;
    }
    for (i = 0; i + 1u < (uint32_t)w; i++) {
        int32_t a = kx_scope[(sw - (uint32_t)w * 2u + i * 2u) & (KX_SCOPE - 1u)];
        float ya = mid - (float)a * k;
        ya = ya < (float)y ? (float)y : ya > (float)(y + h) ? (float)(y + h) : ya;
        if (i)
            vg_line(px, py, (float)(x + (int)i), ya, 1.4f, col);
        px = (float)(x + (int)i);
        py = ya;
    }
}

/* the envelope (ATK DEC SUS REL, 0..127 each) as a shape, filled faintly */
static void kx_draw_adsr(int x, int y, int w, int h, int a, int d, int s, int r, uint32_t col)
{
    float seg = (float)w / 4.2f, xa = (float)x + seg * (float)a / 127.0f + 2.0f, xd = xa + seg * (float)d / 127.0f;
    float xs = xd + seg * 0.9f, xr = xs + seg * (float)r / 127.0f, top = (float)y + 6.0f, bot = (float)(y + h - 2);
    float ys = bot - ((float)s / 127.0f) * (bot - top);
    int i;
    for (i = 0; i <= (int)(xr - (float)x); i += 3) {   /* (the area under it, in thin strokes) */
        float px = (float)(x + i), py = px < xa ? bot - (px - (float)x) / (xa - (float)x + 0.01f) * (bot - top) :
                   px < xd ? top + (px - xa) / (xd - xa + 0.01f) * (ys - top) : px < xs ? ys :
                   ys + (px - xs) / (xr - xs + 0.01f) * (bot - ys);
        vg_line(px, py, px, bot, 1.0f, vg_mix(C_BG, col, 0.13f));
    }
    kx_gline((float)x, bot, xa, top, 2.5f, col);
    kx_gline(xa, top, xd, ys, 2.5f, col);
    kx_gline(xd, ys, xs, ys, 2.5f, col);
    kx_gline(xs, ys, xr, bot, 2.5f, col);
    vg_circle(xa, top, 4, C_WHITE);
    vg_circle(xd, ys, 4, C_WHITE);
    vg_circle(xs, ys, 4, C_WHITE);
    vg_circle(xr, bot, 4, C_WHITE);
}

static void kx_draw_lfo(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    int i, wave = t->p[P_LWAVE], rate = t->p[P_LRATE];
    float cyc = 1.0f + (float)rate / 32.0f, mid = (float)y + (float)h * 0.5f, amp = (float)h * 0.42f, px = 0, py = 0;
    uint32_t r;
    vg_rect(x, (int)mid, w, 1, 0x1C1C20);
    for (i = 0; i <= w; i += 2) {
        float ph = (float)i / (float)w * cyc, f = ph - (float)(int)ph, v;
        switch (wave % 6) {
        case 0: v = vd_sin(f); break;
        case 1: v = f < 0.5f ? 4.0f * f - 1.0f : 3.0f - 4.0f * f; break;
        case 2: v = 1.0f - 2.0f * f; break;
        case 3: v = 2.0f * f - 1.0f; break;
        case 4: v = f < 0.5f ? 1.0f : -1.0f; break;
        default:
            r = (uint32_t)(int)ph * 2654435761u + 12345u;
            v = (float)(int32_t)(r ^ (r >> 13)) / 2147483648.0f;
            break;
        }
        if (i)
            kx_gline(px, py, (float)(x + i), mid - v * amp, 2.0f, col);
        px = (float)(x + i);
        py = mid - v * amp;
    }
    {   /* where the LFO is now: its phase, on the first cycle drawn */
        float f = (float)(t->lfo_ph >> 8) / 16777216.0f, lx = (float)x + f / cyc * (float)w;
        float ly = mid - ((float)t->lfo_val / 32768.0f) * amp;
        vg_rect((int)lx, y, 1, h, vg_mix(C_BG, col, 0.5f));
        kx_draw_dot(lx, ly < (float)y ? (float)y : ly > (float)(y + h) ? (float)(y + h) : ly, col);
    }
}

static void kx_draw_sends(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    static const char *const N[4] = {"DIST", "CHORUS", "DELAY", "REVERB"};
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int bx = x + (int)k * (w / 4) + 18, bw = w / 4 - 36, v = t->p[P_DIST + k], n = 12, j;
        int lit = (v * n + 63) / 127;
        for (j = 0; j < n; j++) {                 /* a vertical ladder */
            int sy = y + (h - 22) - (j + 1) * ((h - 22) / n);
            vg_rect(bx, sy + 1, bw, (h - 22) / n - 2, j < lit ? (k == 0u ? C_RED : col) : 0x222226);
        }
        vg_text(N[k], bx + bw / 2, y + h - 18, VG_SMALL, v ? C_TEXT : C_DIM, VG_CENTER);
    }
}

static void kx_draw_scale(int x, int y, int w, int h, const track_t *t)
{
    uint32_t m = scale_mask(t), root = (uint32_t)t->p[P_ROOT] % 12u, k;
    int kw = w / 12;
    for (k = 0; k < 12u; k++) {
        uint32_t n = (k + 12u - root) % 12u, on = (m >> n) & 1u, bl = key_black(k + 7u);
        int kx = x + (int)k * kw;
        vg_round(kx + 2, y + (bl ? 0 : h / 3), kw - 4, h * 2 / 3, 3, k == root ? C_RED : on ? C_TEXT : C_PANEL);
        vg_text(N_NOTE[k], kx + kw / 2, y + h - 18, VG_SMALL, k == root ? C_WHITE : on ? C_BG : C_DIM, VG_CENTER);
    }
}

static void kx_draw_arp(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    int i, n = 16, oct = clamp(t->p[P_AOCT], 0, 3) + 1, mode = t->p[P_AMODE];
    if (!mode) {
        vg_text("ARP OFF", x + w / 2, y + h / 2 - 12, VG_BIG, C_DIM, VG_CENTER);
        vg_text("KNOB 1 TURNS IT ON", x + w / 2, y + h / 2 + 16, VG_SMALL, C_LINE, VG_CENTER);
        return;
    }
    for (i = 0; i < n; i++) {
        int lv = (i % (4 * oct)), hgt = (h - 10) * (lv + 1) / (4 * oct);
        if (mode == 2) hgt = (h - 10) - hgt + (h - 10) / (4 * oct);
        vg_round(x + i * (w / n) + 3, y + h - hgt, w / n - 6, hgt, 2, (int)(t->arp_idx % 16u) == i ? C_WHITE : col);
    }
}

/* SEQ. A DRUM track: its eight lanes over the 16 steps shown (the pads), the lane being edited lit; a note track: the
 * 16 steps as a piano roll. Above, the whole pattern in small: which 16 are shown, where it plays */
static void kx_draw_grid(int x, int y, int w, int h)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, cur = ui.cursor % NSTEP, play = t->seq_idx % len;
    uint32_t col = TRK_RGB[song.sel], bank = kx_bank(), nb = (len + 15u) / 16u;
    int drum = drum_track(t), lx = x + 44, cw = (w - 44) / 16, gy = y + 14, gh = h - 14;
    /* the overview: every step of LEN as a tick, the shown 16 framed */
    for (i = 0; i < len; i++) {
        int tx = lx + (int)i * (w - 44) / (int)len, tw = (w - 44) / (int)len - 1;
        uint32_t on = step_on(&t->step[i]);
        vg_rect(tx, y + 2, tw > 1 ? tw : 1, 6, song.playing && i == play ? C_WHITE : on ? vg_mix(C_PANEL, col, 0.7f) : 0x222226);
    }
    if (nb > 1u)
        vg_frame(lx + (int)(bank * 16u) * (w - 44) / (int)len - 1, y, (int)(16u * (uint32_t)(w - 44) / len) + 1, 10, 1, C_WHITE);
    {
        char b[8];
        vg_itoa((int)bank + 1, b);
        str_cpy(b + str_len(b), "/", 2);
        vg_itoa((int)nb, b + str_len(b));
        vg_text(b, x, y, VG_SMALL, C_DIM, VG_LEFT);
    }
    if (drum) {
        uint32_t l, rh = (uint32_t)gh / NLANE;
        kx_glyph_tick(t);
        for (l = 0; l < NLANE; l++) {
            int ry = gy + (int)((NLANE - 1u - l) * rh), sel = l == ui.lane;
            kx_draw_glyph(lx - 14, ry + 1, (int)rh - 2, l, kx_glyph[l] + (sel ? 0.45f : 0.0f), sel ? C_WHITE : col);
            if (sel)                              /* (the rows are too low for eight names: the lane edited, named) */
                vg_text(drum_lane_abbr(t, l), x, clamp(ry + (int)rh / 2 - 8, gy, gy + gh - 16), VG_BOLD, C_WHITE, VG_LEFT);
            for (i = 0; i < 16u; i++) {
                uint32_t s = bank * 16u + i;
                const step_t *st = &t->step[s % NSTEP];
                int cx = lx + (int)i * cw, in = s < len, hit = in && ((step_lanes(st) >> l) & 1u);
                int acc = hit && (((step_accents(st) >> l) & 1u) || (st->flags & SF_ACCENT));
                uint32_t bg = !in ? C_BG : (i / 4u) % 2u ? 0x1B1B1F : 0x151518;
                vg_rect(cx + 1, ry + 1, cw - 2, (int)rh - 2, sel && in ? vg_mix(bg, col, 0.10f) : bg);
                if (hit)
                    vg_round(cx + 2, ry + 2, cw - 4, (int)rh - 4, 2, acc ? C_WHITE : col);
            }
        }
    } else {
        int lo = 127, hi = 0;
        for (i = 0; i < 16u; i++) {
            const step_t *st = &t->step[(bank * 16u + i) % NSTEP];
            uint32_t k;
            if (st->time == ST_NOTE)
                for (k = 0; k < st->n; k++) {
                    lo = st->note[k] < lo ? st->note[k] : lo;
                    hi = st->note[k] > hi ? st->note[k] : hi;
                }
        }
        if (hi < lo) lo = 48, hi = 60;
        lo -= 2;
        hi += 2;
        if (hi - lo < 14) hi = lo + 14;
        {   /* the rows: a note name on each C, the black keys darker */
            int nrow = hi - lo + 1, n;
            float rh = (float)gh / (float)nrow;
            for (n = lo; n <= hi; n++) {
                int ry = gy + gh - (int)((float)(n - lo + 1) * rh);
                vg_rect(lx, ry, w - 44, (int)rh, key_black((uint32_t)n + 7u) ? 0x121214 : 0x18181B);
                if (n % 12 == 0) {
                    char b[4];
                    note_name(b, (uint32_t)n);
                    vg_text(b, x, ry - 6, VG_SMALL, C_DIM, VG_LEFT);
                    vg_rect(lx, ry + (int)rh, w - 44, 1, 0x26262A);
                }
            }
            for (i = 0; i < 16u; i++) {
                uint32_t s = bank * 16u + i, k;
                const step_t *st = &t->step[s % NSTEP];
                int cx = lx + (int)i * cw;
                if (s >= len) {
                    vg_rect(cx, gy, cw, gh, C_BG);
                    continue;
                }
                if (i % 4u == 0u)
                    vg_rect(cx, gy, 1, gh, 0x2A2A30);
                if (st->time == ST_NOTE)
                    for (k = 0; k < st->n; k++) {
                        int ry = gy + gh - (int)((float)(st->note[k] - lo + 1) * rh), nl = (int)note_len(t, s);
                        int ww = cw * (nl > 0 ? nl : 1) - 3;
                        if (cx + ww > lx + (w - 44))
                            ww = lx + (w - 44) - cx - 1;
                        vg_round(cx + 2, ry, ww, (int)rh + 1 > 4 ? (int)rh + 1 : 4, 2,
                                 st->flags & SF_ACCENT ? vg_mix(col, C_WHITE, 0.55f) : col);
                        if (st->flags & SF_SLIDE)
                            vg_rect(cx + ww, ry + (int)rh / 2, 3, 2, C_WHITE);
                    }
            }
        }
    }
    /* the playhead's trail: the steps it just left, fading */
    if (song.playing) {
        uint32_t k;
        for (k = 1; k <= 4u; k++) {
            uint32_t s = (play + len - k) % len;
            if (s / 16u == bank)
                vg_rect_a(lx + (int)(s % 16u) * cw, gy, cw, gh, col, 0.20f / (float)k);
        }
        if (play / 16u == bank)
            vg_rect_a(lx + (int)(play % 16u) * cw, gy, cw, gh, C_WHITE, 0.12f);
    }
    /* the playhead and the cursor */
    if (song.playing && play / 16u == bank)
        vg_rect(lx + (int)(play % 16u) * cw, gy, cw, 2, C_WHITE), vg_rect(lx + (int)(play % 16u) * cw, gy + gh - 2, cw, 2, C_WHITE);
    if (cur / 16u == bank)
        vg_frame(lx + (int)(cur % 16u) * cw, gy, cw, gh, 1, C_RED);
}

/* MASTER: the creep (how worn the mix is) beside the scope, the chain below */
static void kx_draw_master(int x, int y, int w, int h)
{
    static const char *const N[6] = {"DAMAGE", "WALL", "RESONATE", "SPLICE", "HAUNT", "HISS"};
    int amt[6], i, bw = (w - 5 * 4) / 6, cs = h - 48;
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    char b[8];
    amt[0] = song.g[G_DMG];
    amt[1] = song.g[G_WALL];
    amt[2] = song.g[G_RES];
    amt[3] = song.g[G_SPLICE];
    amt[4] = song.g[G_HAUNT] ? song.g[G_HAMT] : 0;
    amt[5] = song.g[G_HISS];
    vg_rect(x, y, cs + 40, cs, 0x0D0D0F);
    cr_cache_blit(&cr_master, kx_fb, 480, 320, x + 20, y, cr_wear(), 190 + (int)(heat * 160.0f));
    vg_text("WEAR", x + 6, y + 4, VG_SMALL, C_DIM, VG_LEFT);
    vg_itoa((int)(cr_wear() * 100.0f + 0.5f), b);
    vg_text(b, x + 6, y + 18, VG_BOLD, C_WHITE, VG_LEFT);
    kx_draw_reels(x + cs + 48, y, w - cs - 48, cs);
    {   /* the signal along the chain: sparks running left to right, as many and as fast as the mix is loud */
        float tm = (float)vibe_frames() / kx_sr;
        int n = 3 + (int)(heat * 14.0f), j;
        for (j = 0; j < n; j++) {
            float f = tm * (0.25f + heat * 0.6f) + (float)j / (float)n;
            float sx = (float)x + (f - (float)(int)f) * (float)w;
            vg_circle(sx, (float)(y + h - 47), 1.5f + heat * 1.5f, vg_mix(C_RED, C_WHITE, heat));
        }
    }
    for (i = 0; i < 6; i++) {
        int bx = x + i * (bw + 4), by = y + h - 44;
        float a = (float)clamp(amt[i], 0, 127) / 127.0f;
        vg_round(bx, by, bw, 42, 4, vg_mix(C_PANEL, C_RED, a * 0.45f));
        vg_text(N[i], bx + bw / 2, by + 5, VG_SMALL, a > 0 ? C_WHITE : C_DIM, VG_CENTER);
        kx_ladder(bx + 6, by + 28, bw - 12, 6, a, 0, C_RED);
    }
}

/* SOUND: the selected track's sound, big; the sounds around it in the list (DATA steps through them) */
static void kx_draw_browse(int x, int y, int w, int h)
{
    const track_t *t = TSEL;
    char nm[17];
    uint32_t col = TRK_RGB[song.sel], total, cur = preset_pos(&total), r;
    int lx = x + w - 140;
    sound_name(t, nm);
    vg_text(ENGINES[eng_idx(t->eng_req)]->name, x + 4, y, VG_BOLD, col, VG_LEFT);
    vg_text(CAT_NAME[track_cat(t) % CAT_N], x + 10 + vg_text_width(ENGINES[eng_idx(t->eng_req)]->name, VG_BOLD), y,
            VG_BOLD, C_DIM, VG_LEFT);
    vg_text(nm, x + 2, y + 18, vg_text_width(nm, VG_BIG | VG_X2) < lx - x - 12 ? VG_BIG | VG_X2 : VG_BIG, C_WHITE, VG_LEFT);
    kx_draw_spectrum(x + 4, y + 64, lx - x - 16, h - 66, col);
    vg_rect(lx - 6, y, 1, h, C_LINE);
    for (r = 0; r < 7u && total; r++) {
        uint32_t n = preset_visible(cur, total, r), k, e;
        char b[17];
        int ry = y + (int)r * 17;
        if (n >= total)
            continue;
        e = preset_at(n, &k);
        if (kx_silent_engine(e))
            continue;
        if (e == NENGINES)
            up_name(k, b);
        else
            str_cpy(b, ENGINES[e]->presets[k].name, sizeof b);
        if (n == cur) {
            vg_round(lx, ry - 1, 136, 17, 3, vg_mix(C_PANEL, col, 0.3f));
            vg_rect(lx, ry - 1, 3, 17, col);
        }
        vg_text(b, lx + 8, ry, VG_SMALL, n == cur ? C_WHITE : r == 0u || r == 6u ? 0x4A4A50 : C_DIM, VG_LEFT);
    }
}

static void kx_draw_mixer(int x, int y, int w, int h)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)                    /* (kx_pics.h: needle meters) */
        kx_draw_vu(x + (int)i * (w / 4) + 3, y, w / 4 - 6, h, i);
}

/* INPUT: what comes in (its scope and level), where it goes */
static void kx_draw_input(int x, int y, int w, int h)
{
    uint32_t i, kw = kx_kin_w;
    float mid = (float)y + (float)(h - 28) * 0.5f, k = (float)(h - 28) * 0.45f / 32768.0f, px = 0, py = 0;
    if (!kx_in_on) {
        vg_text("NO INPUT", x + w / 2, y + 2, VG_BIG, C_DIM, VG_CENTER);
        vg_text("CONNECT IN 1 / IN 2, OR ALLOW THE MICROPHONE", x + w / 2, y + 30, VG_SMALL, C_DIM, VG_CENTER);
        return;
    }
    vg_rect(x, (int)mid, w - 30, 1, C_LINE);
    for (i = 0; i < (uint32_t)(w - 30); i++) {
        int32_t a = kx_kin[0][(kw - (uint32_t)(w - 30) * 2u + i * 2u) & (KX_KIN - 1u)];
        float yy = mid - (float)a * k;
        if (i)
            vg_line(px, py, (float)(x + (int)i), yy, 1.4f, C_WHITE);
        px = (float)(x + (int)i);
        py = yy;
    }
    {
        int mh = (int)(kx_in_peak * (float)(h - 28));
        vg_rect(x + w - 20, y, 14, h - 28, C_PANEL);
        vg_rect(x + w - 20, y + (h - 28) - mh, 14, mh, kx_in_peak > 0.9f ? C_RED : C_TEXT);
    }
    {
        char b[48];
        str_cpy(b, "IN > ", sizeof b);
        if (kx_ic[IC_GATE])
            str_cpy(b + str_len(b), "GATE BY T", sizeof b - str_len(b)), b[str_len(b) + 1] = 0, b[str_len(b)] = (char)('0' + kx_ic[IC_GATE]);
        str_cpy(b + str_len(b), kx_ic[IC_GATE] ? " > MIX > MASTER" : "MIX > MASTER", sizeof b - str_len(b));
        vg_text(b, x, y + h - 20, VG_SMALL, C_DIM, VG_LEFT);
        if (kx_ic[IC_FEED]) {
            str_cpy(b, "ALSO INFECT SOURCE AS T", sizeof b);
            b[str_len(b) + 1] = 0;
            b[str_len(b)] = (char)('0' + kx_ic[IC_FEED]);
            vg_text(b, x + w, y + h - 20, VG_SMALL, C_RED, VG_RIGHT);
        }
    }
}

/* MIDI OUT: the four tracks, each on its channel, its notes lit as they go out */
static void kx_draw_midi(int x, int y, int w, int h)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++) {
        int ry = y + (int)k * (h / 4), ch = (kx_ic[IC_MCH] - 1 + (int)k) % 16 + 1;
        char b[8];
        vg_text("CH", x, ry + 4, VG_SMALL, 0x55555C, VG_LEFT);
        vg_itoa(ch, b);
        vg_text(b, x + 22, ry + 2, VG_BOLD, kx_ic[IC_MOUT] ? TRK_RGB[k] : 0x3A3A40, VG_LEFT);
        vg_rect(x + 52, ry + h / 8, w - 120, 1, 0x26262A);
        for (i = 0; i < kx_mo_n[k] && i < 16u; i++) {
            int nx = x + 52 + (int)kx_mo_on[k][i] * (w - 120) / 128;
            vg_rect(nx - 2, ry + 3, 5, h / 4 - 8, TRK_RGB[k]);
        }
        if (kx_ic[IC_CV] == (int)k + 1)
            vg_text(kx_mo_n[k] ? "CV  GATE" : "CV", x + w, ry + 4, VG_SMALL, C_RED, VG_RIGHT);
    }
}

/* SONG: the sections, each track's pattern slot (A..D, - silent) and its repeats; the one playing lit */
static void kx_draw_song(int x, int y, int w, int h)
{
    uint32_t n = chain_config.count, sel = ui.song_row, first, r, k;
    int rh = 18, rows = h / rh;
    first = sel + 1u > (uint32_t)rows ? sel + 1u - (uint32_t)rows : 0u;
    for (k = 0; k < NTRK; k++)
        vg_text(k == 0 ? "T1" : k == 1 ? "T2" : k == 2 ? "T3" : "T4", x + 70 + (int)k * 60, y, VG_SMALL, TRK_RGB[k], VG_LEFT);
    vg_text("REPS", x + 330, y, VG_SMALL, C_DIM, VG_LEFT);
    for (r = first; r <= n && r < first + (uint32_t)rows - 1u && r < CHAIN_ROWS; r++) {
        int ry = y + 16 + (int)(r - first) * rh, on = r == sel, play = chain.running && r == chain.row;
        char b[8];
        if (on)
            vg_rect(x, ry - 1, w, rh - 1, vg_mix(C_BG, C_RED, 0.18f));
        if (play)
            vg_tri((float)x, (float)ry + 2, (float)x, (float)ry + 12, (float)x + 8, (float)ry + 7, C_WHITE);
        if (r == n) {
            vg_text("+ ADD", x + 14, ry, VG_SMALL, on ? C_WHITE : C_DIM, VG_LEFT);
            continue;
        }
        vg_itoa((int)r + 1, b);
        vg_text(b, x + 14, ry, VG_BOLD, on ? C_WHITE : C_TEXT, VG_LEFT);
        for (k = 0; k < NTRK; k++) {
            uint32_t sl = chain_config.row[r].slot[k];
            char s2[2] = {sl < 4u ? (char)('A' + sl) : '-', 0};
            vg_text(s2, x + 74 + (int)k * 60, ry, VG_BOLD, sl < 4u ? (on && k == ui.song_trk ? C_WHITE : TRK_RGB[k]) : 0x3A3A40,
                    VG_LEFT);
        }
        b[0] = 'x';
        vg_itoa(chain_config.row[r].repeat, b + 1);
        vg_text(b, x + 330, ry, VG_BOLD, C_TEXT, VG_LEFT);
    }
    if (!n)
        vg_text("TURN KNOB 3 TO ADD A SECTION. F5 PLAYS THE SONG", x + w, y + h - 16, VG_SMALL, C_DIM, VG_RIGHT);
}

/* AUTOMATION: the rows around the one picked (PLAY, QUANTIZE, each step's CHANCE / RATCH / NUDGE / VEL, each recorded
 * lock or event, + ADD) */
static void kx_draw_events(int x, int y, int w, int h)
{
    const track_t *t = TSEL;
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), cur = ev_row(n), first, r;
    int rh = 17, rows = h / rh;
    first = cur > (uint32_t)rows / 2u ? cur - (uint32_t)rows / 2u : 0u;
    for (r = first; r < n && r < first + (uint32_t)rows; r++) {
        uint32_t cc = rw[r], kk = cc >> 8, step = ev_step_of(cc), id;
        int ry = y + (int)(r - first) * rh, on = r == cur;
        char b[24], nm[12];
        if (on)
            vg_rect(x, ry - 1, w, rh - 1, vg_mix(C_BG, C_RED, 0.18f));
        if (kk == EVK_TOP) {
            vg_text(motion_enabled(t) ? "PLAY ON" : "PLAY OFF", x + 6, ry, VG_BOLD, on ? C_WHITE : C_TEXT, VG_LEFT);
            continue;
        }
        if (kk == EVK_QNT) {
            vg_text(t->p[P_SQNT] ? "QUANTIZE ON" : "QUANTIZE OFF", x + 6, ry, VG_BOLD, on ? C_WHITE : C_TEXT, VG_LEFT);
            continue;
        }
        if (kk == EVK_ADD) {
            vg_text("+ ADD", x + 6, ry, VG_BOLD, on ? C_WHITE : C_DIM, VG_LEFT);
            continue;
        }
        id = kk == EVK_REC ? MOTION_ID(&motion.event[cc & 0xFFu]) : kk == EVK_CHANCE ? EV_CHANCE : kk == EVK_RATCH ?
             EV_RATCH : kk == EVK_VEL ? EV_VEL : EV_NUDGE;
        str_cpy(b, "STEP ", sizeof b);
        vg_itoa((int)step + 1, b + 5);
        vg_text(b, x + 6, ry, VG_SMALL, C_DIM, VG_LEFT);
        ev_name(t, id, nm);
        vg_text(nm, x + 80, ry, VG_BOLD, on ? C_WHITE : TRK_RGB[song.sel], VG_LEFT);
        if (kk == EVK_REC) {
            const param_desc_t *d = track_desc(t, id);
            const char *un = "";
            param_format(d, motion.event[cc & 0xFFu].value, b, &un);
            vg_text((motion.event[cc & 0xFFu].param & MOTION_LOCK) ? "LOCK" : "AUTO", x + 200, ry, VG_SMALL, C_DIM, VG_LEFT);
            vg_text(b, x + 250, ry, VG_BOLD, C_TEXT, VG_LEFT);
        }
    }
}

/* MY SOUNDS: the 32 user slots, their names; ENC 1 picks, ENTER loads, F5 saves the selected track's sound there */
static void kx_draw_user(int x, int y, int w, int h)
{
    uint32_t s;
    int cw = w / 4, rh = h / 8;
    for (s = 0; s < UP_SLOTS; s++) {
        int cx = x + (int)(s / 8u) * cw, cy = y + (int)(s % 8u) * rh, on = s == (uint32_t)kx_user_pick;
        char b[16], l[4];
        up_slot_label(l, s);
        if (on)
            vg_rect(cx, cy, cw - 4, rh - 1, vg_mix(C_BG, C_RED, 0.25f));
        vg_text(l, cx + 4, cy, VG_SMALL, on ? C_WHITE : 0x55555C, VG_LEFT);
        if (up_used(s)) {
            up_name(s, b);
            b[10] = 0;
            vg_text(b, cx + 34, cy, VG_SMALL, on ? C_WHITE : C_TEXT, VG_LEFT);
        }
    }
}

/* ---- the pedalboard: each pedal drawn as one (its enclosure in its type's colour, a name plate, three knobs, the LED
 * lit by what it does, the footswitch), patch cables between them with the signal running along */
static const uint32_t PED_RGB[IT_N] = {0x26262A, 0xC8762A, 0xB8261C, 0xD8B23A, 0x6A3A8C, 0x4E8A3A, 0x2E8A9A, 0x3A62B0,
                                       0x3A9A82, 0x7A7A3A, 0x7A4A2A, 0x6A6A70, 0x8A2A14, 0xA0306A};
static void kx_knob(float cx, float cy, float r, float pos, uint32_t ink)
{
    float a = 0.375f - pos * 0.75f;               /* (270 degrees, as a real one) */
    vg_circle(cx, cy + 1.5f, r, 0x000000);
    vg_circle(cx, cy, r, 0x1A1A1C);
    vg_circle(cx, cy, r - 2.0f, 0x2C2C30);
    vg_line(cx, cy, cx + vd_sin(a + 0.25f) * (r - 3.0f), cy - vd_sin(a) * (r - 3.0f), 2.0f, ink);
}
static void kx_pedal(int x, int y, int w, int h, const int16_t *v, int focus, float lvl, int num)
{
    int ty = clamp(v[0], 0, IT_N - 1), k;
    uint32_t body = PED_RGB[ty], dark = vg_mix(body, 0x000000, 0.45f);
    char n[2] = {(char)('1' + num), 0};
    vg_round(x + 3, y + 4, w, h, 8, 0x000000);                      /* (its shadow) */
    vg_round(x, y, w, h, 8, ty ? body : 0x18181A);
    vg_round(x + 2, y + 2, w - 4, h / 2, 7, ty ? vg_mix(body, 0xFFFFFF, 0.10f) : 0x1C1C1F);   /* (the light on its top) */
    vg_round(x + 2, y + h / 2, w - 4, h / 2 - 2, 7, ty ? body : 0x18181A);
    if (focus)
        vg_frame(x - 3, y - 3, w + 6, h + 6, 2, C_WHITE);
    vg_text(n, x + 6, y + 4, VG_SMALL, ty ? vg_mix(body, 0x000000, 0.6f) : 0x3A3A40, VG_LEFT);
    if (!ty) {                                                      /* an empty place on the board */
        vg_text("EMPTY", x + w / 2, y + h / 2 - 8, VG_BOLD, 0x3A3A40, VG_CENTER);
        vg_text("ENC 1", x + w / 2, y + h / 2 + 10, VG_SMALL, 0x2E2E34, VG_CENTER);
        return;
    }
    for (k = 0; k < 3; k++) {                                       /* the three knobs */
        const param_desc_t *dd = ins_desc(ty, (uint32_t)k);
        float pos = dd->max > dd->min ? (float)(v[1 + k] - dd->min) / (float)(dd->max - dd->min) : 0.0f;
        float cx = (float)x + (float)w * (0.2f + 0.3f * (float)k);
        char lb[5];
        str_cpy(lb, dd->label, sizeof lb);       /* (four letters: three fit side by side) */
        kx_knob(cx, (float)y + 22.0f, 9.5f, pos, C_WHITE);
        vg_text(lb, (int)cx, y + 34, VG_SMALL, dark, VG_CENTER);
    }
    vg_rect(x + 8, y + 52, w - 16, 22, 0x0E0E10);                   /* the name plate */
    vg_text(N_ITYPE[ty], x + w / 2, y + 55, VG_BOLD, C_WHITE, VG_CENTER);
    {                                                               /* the LED: as bright as what it does */
        float a = lvl > 1.0f ? 1.0f : lvl;
        uint32_t led = v[PD_MIX] ? vg_mix(0x400808, 0xFF3020, 0.35f + 0.65f * a) : 0x300606;
        if (v[PD_MIX] && a > 0.05f)
            vg_circle((float)(x + w - 12), (float)(y + 9), 7.0f, vg_mix(body, 0xFF3020, 0.35f * a));
        vg_circle((float)(x + w - 12), (float)(y + 9), 3.5f, led);
    }
    vg_circle((float)(x + w / 2), (float)(y + h - 15), 10.0f, 0x0C0C0E);   /* the footswitch */
    vg_circle((float)(x + w / 2), (float)(y + h - 16), 8.0f, 0x8A8A90);
    vg_circle((float)(x + w / 2), (float)(y + h - 17), 5.0f, 0xC8C8CC);
}
static void kx_draw_pedals(int pg, int x, int y, int w, int h)
{
    uint32_t o = kx_ped_owner(pg), j, ns = (uint32_t)kx_ped_slots(o);
    int pw = 128, gap = (w - 60 - (int)ns * pw) / (int)(ns + 1u), px[PED_SLOTS];
    float tm = (float)vibe_frames() / kx_sr, sig = 0.0f;
    const char *in = o == PO_IN ? "IN 1/2" : o == PO_MASTER ? "MIX" : "SOUND";
    const char *out = o == PO_MASTER ? "MASTER" : "MIX";
    for (j = 0; j < ns; j++)
        px[j] = x + 30 + gap + (int)j * (pw + gap);
    for (j = 0; j < ns; j++)
        sig = kx_ped_lvl[o][j] > sig ? kx_ped_lvl[o][j] : sig;
    if (o < NTRK)
        sig = (float)clamp(trk[o].peak, 0, 32767) / 32768.0f;
    {   /* the cables: IN to the first, one to the next, the last to OUT; the signal runs along when there is some */
        int cy = y + h / 2 + 4, k;
        vg_line((float)x, (float)cy, (float)px[0], (float)cy, 4.0f, 0x111113);
        vg_line((float)x, (float)cy, (float)px[0], (float)cy, 2.0f, 0x3A3A40);
        for (j = 0; j + 1u < ns; j++) {
            vg_line((float)(px[j] + pw), (float)cy, (float)px[j + 1], (float)cy, 4.0f, 0x111113);
            vg_line((float)(px[j] + pw), (float)cy, (float)px[j + 1], (float)cy, 2.0f, 0x3A3A40);
        }
        vg_line((float)(px[ns - 1] + pw), (float)cy, (float)(x + w), (float)cy, 4.0f, 0x111113);
        vg_line((float)(px[ns - 1] + pw), (float)cy, (float)(x + w), (float)cy, 2.0f, 0x3A3A40);
        if (sig > 0.02f)
            for (k = 0; k < 9; k++) {
                float f = tm * 0.6f + (float)k / 9.0f;
                float sx = (float)x + (f - (float)(int)f) * (float)w;
                int inside = 0;
                for (j = 0; j < ns; j++)
                    inside |= sx > (float)px[j] - 2.0f && sx < (float)(px[j] + pw) + 2.0f;
                if (!inside)
                    vg_circle(sx, (float)cy, 2.0f, vg_mix(C_RED, C_WHITE, sig));
            }
        vg_text(in, x, cy - 20, VG_SMALL, C_DIM, VG_LEFT);
        vg_text(out, x + w, cy - 20, VG_SMALL, C_DIM, VG_RIGHT);
    }
    for (j = 0; j < ns; j++) {
        int16_t v[PD_N];
        kx_ped_get(o, j, v);
        kx_pedal(px[j], y + 4, pw, h - 8, v, (int)j == kx_ped_focus[kx_ped_fi(pg)], kx_ped_lvl[o][j] * 2.0f, (int)j);
    }
}

/* FOOT PEDAL: an expression pedal (its position) and a footswitch, drawn as they are on the floor */
static void kx_draw_foot(int x, int y, int w, int h)
{
    int cx = x + w / 4, cy = y + h / 2 + 18, fx = x + 3 * w / 4;
    float lift = kx_foot_expr * 34.0f;            /* (the side view: the toe end rises with the pedal pushed up) */
    char b[8];
    vg_round(cx - 58, cy + 8, 116, 14, 3, 0x1A1A1C);
    vg_tri((float)cx + 50.0f, (float)cy + 8.0f, (float)cx + 58.0f, (float)cy + 8.0f, (float)cx + 54.0f, (float)cy - 2.0f, 0x3A3A40);
    vg_line((float)cx + 54.0f, (float)cy, (float)cx - 54.0f, (float)cy - lift, 12.0f, 0x2C2C30);
    vg_line((float)cx + 54.0f, (float)cy - 5.0f, (float)cx - 54.0f, (float)cy - lift - 5.0f, 2.0f, 0x55555C);
    vg_itoa((int)(kx_foot_expr * 100.0f + 0.5f), b);
    str_cpy(b + str_len(b), "%", 2);
    vg_text(b, cx + 66, cy + 4, VG_SMALL, C_TEXT, VG_LEFT);
    vg_text(IC_EXPR_N[clamp(kx_ic[IC_EXPR], 0, 3)], cx, y + 2, VG_BOLD, kx_ic[IC_EXPR] ? C_RED : C_DIM, VG_CENTER);
    vg_text("EXPRESSION", cx, y + h - 16, VG_SMALL, C_DIM, VG_CENTER);
    vg_round(fx - 40, cy - 10, 80, 44, 6, 0x1A1A1C);
    vg_circle((float)fx, (float)cy + 4.0f, 14.0f, kx_foot_down ? 0xC8C8CC : 0x6A6A70);
    vg_circle((float)fx, (float)cy - 22.0f, 4.0f, kx_foot_down ? C_RED : 0x300606);
    vg_text(IC_FSW_N[clamp(kx_ic[IC_FSW], 0, 3)], fx, y + 2, VG_BOLD, C_RED, VG_CENTER);
    vg_text("SWITCH", fx, y + h - 16, VG_SMALL, C_DIM, VG_CENTER);
}

/* SAMPLER: the slot's sound as a waveform with SLICE's cuts on it; while it records, what comes in and how far it is */
static void kx_draw_sampler(int x, int y, int w, int h)
{
    uint32_t k = (uint32_t)clamp(kx_ic[IC_SSLOT], 0, 2), c, st = kx_rec_state;
    int mid = y + h / 2 - 6, amp = h / 2 - 16;
    char b[32];
    vg_rect(x, mid, w, 1, 0x26262A);
    if (st) {                                      /* recording, or waiting for the bar */
        uint32_t n = kx_rec_n, want = kx_rec_want ? kx_rec_want : KX_REC_MAX, i;
        float fill = (float)n / (float)want;
        int fw = (int)(fill * (float)w);
        for (c = 0; c < (uint32_t)fw && n; c++) {
            uint32_t a = c * n / (uint32_t)(fw ? fw : 1), e = (c + 1u) * n / (uint32_t)(fw ? fw : 1);
            int32_t lo = 0, hi = 0;
            for (i = a; i < e; i++) {
                lo = kx_rec[i] < lo ? kx_rec[i] : lo;
                hi = kx_rec[i] > hi ? kx_rec[i] : hi;
            }
            vg_rect(x + (int)c, mid - hi * amp / 32768, 1, (hi - lo) * amp / 32768 + 1, C_RED);
        }
        vg_rect(x + fw, y, 2, h - 20, C_WHITE);
        if (st == 1u || (kx_now_ms() / 300u) % 2u)
            vg_circle((float)(x + 10), (float)(y + 8), 6.0f, C_RED);
        vg_text(st == 1u ? "WAITING FOR THE BAR" : "RECORDING", x + 22, y, VG_BOLD, C_RED, VG_LEFT);
        return;
    }
    if (!usr_nz[k]) {
        vg_text("EMPTY SLOT", x + w / 2, y + 20, VG_BIG, C_DIM, VG_CENTER);
        vg_text("F5 RECORDS THE INPUT OR THE MIX FROM THE NEXT BAR", x + w / 2, y + 52, VG_SMALL, C_DIM, VG_CENTER);
        return;
    }
    for (c = 0; c < 120u; c++) {                   /* the slot's picture */
        int x0 = x + (int)(c * (uint32_t)w / 120u), cw = w / 120 + 1;
        int lo = kx_usr_wave[k][0][c], hi = kx_usr_wave[k][1][c];
        vg_rect(x0, mid - hi * amp * 3 / 512, cw, (hi - lo) * amp * 3 / 512 + 1, vg_mix(C_BG, TRK_RGB[song.sel], 0.75f));
    }
    if (slc_usr[k].len) {                          /* SLICE's AUTO slices: where it cut */
        uint32_t s;
        for (s = 0; s < slc_usr[k].nauto; s++) {
            int sx = x + (int)((uint64_t)slc_usr[k].apos[s] * (uint32_t)w / slc_usr[k].len);
            vg_rect(sx, y + 4, 1, h - 30, C_WHITE);
            vg_tri((float)sx, (float)y + 4.0f, (float)sx + 5.0f, (float)y + 4.0f, (float)sx, (float)y + 10.0f, C_WHITE);
        }
        str_cpy(b, IC_SSLOT_N[k], sizeof b);
        str_cpy(b + str_len(b), ": ", sizeof b - str_len(b));
        vg_itoa((int)slc_usr[k].nauto, b + str_len(b));
        str_cpy(b + str_len(b), " SLICES, KEYS FROM C4", sizeof b - str_len(b));
        vg_text(b, x + w, y + h - 18, VG_SMALL, C_DIM, VG_RIGHT);
    }
}

static void kx_draw_project(int x, int y, int w, int h)
{
    static const char *const SL[4] = {"A", "B", "C", "D"};
    uint32_t i;
    char nm[16];
    project_cur_name(nm);
    vg_text(nm[0] ? nm : "UNTITLED", x + 4, y, VG_BIG, C_WHITE, VG_LEFT);
    vg_text("ENC 1 SLOT  ENTER LOAD  F5 SAVE  SHIFT+F5 NEW  SHIFT+ENTER DEMO", x + 4, y + 28,
            VG_SMALL, C_DIM, VG_LEFT);
    for (i = 0; i < 4u; i++) {
        int bx = x + (int)i * (w / 4), used = kx_slot_used[i];
        int pick = (uint32_t)kx_slot_pick == i;
        vg_rect(bx + 4, y + 52, w / 4 - 8, h - 52, pick ? vg_mix(C_BG, C_RED, 0.22f) : 0x111113);
        vg_rect(bx + 4, y + 52, w / 4 - 8, 2, pick ? C_RED : 0x2A2A2E);
        vg_text(SL[i], bx + 12, y + 58, VG_BIG, used ? C_WHITE : 0x3A3A40, VG_LEFT);
        vg_text(used ? kx_slot_name[i] : "EMPTY", bx + 12, y + 84, VG_SMALL, used ? C_TEXT : 0x55555C, VG_LEFT);
        (void)h;
    }
}

/* ---- the idle screen: after a minute and a half with nothing touched, the discharge and the master's scope, full */
static void kx_draw_idle(void)
{
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    vg_clear(C_BG);
    cr_draw(60, 10, 360, 300, 0.4f + cr_wear() * 0.6f, heat, C_RED);
    kx_draw_scope(0, 200, 480, 100, C_WHITE, 1.0f);
    kx_logo(16, 286, VG_BOLD);
}

static void kx_draw_help(void)
{
    static const char *const R[] = {
        "KEYS", "the selected track", "PADS 1-4", "mute (SHIFT: select)", "PADS 5-8", "DICE (SHIFT: MOOD)",
        "PADS 9-12", "hold: 1/16 STOP FRZ GATE", "PADS 13-16", "KICK SNARE CLAP HAT",
        "K1-K6", "SOUND ENV LFO ARP FX SEQ", "ENC 1-4", "the four values", "DATA", "sounds (SHIFT: engine)",
        "A", "track level", "B", "DAMAGE (SHIFT: WALL)", "F1-F4", "select a track", "F5", "the page's action",
        "F6", "UNDO", "MENU", "mixer, project, clock", "< >", "the family's pages", "REC", "record the track",
    };
    vg_controls("KRIECHSTROM", R, (int)(sizeof R / sizeof R[0] / 2));
}

/* SHIFT held or tapped: what the pads and the other controls do with it */
static void kx_draw_shift(void)
{
    static const char *const ROW[4] = {"SELECT", "MOOD", "", "KICK SNARE CLAP HAT"};
    uint32_t r, k;
    int x = 250, y = 94;
    vg_rect_a(x - 8, y - 4, 238, 118, 0x000000, 0.82f);
    vg_frame(x - 8, y - 4, 238, 118, 1, C_RED);
    vg_text(kx_shift_latch ? "SHIFT (FOR THE NEXT ONE)" : "SHIFT", x, y, VG_BOLD, C_RED, VG_LEFT);
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 4u; k++) {
            int px = x + (int)k * 26, py = y + 20 + (int)r * 22;
            uint32_t col = r == 0u ? TRK_RGB[k] : r == 1u ? C_RED : 0x3A3A40;
            vg_round(px, py, 22, 18, 3, vg_mix(C_BG, col, r < 2u ? 0.8f : 0.4f));
        }
    for (r = 0; r < 4u; r++)
        vg_text(r == 2u ? "(hold: effects)" : ROW[r], x + 110, y + 21 + (int)r * 22, VG_SMALL, r < 2u ? C_WHITE : C_DIM, VG_LEFT);
}

/* the start: where you left it, the demo song, or nothing */
static void kx_draw_start(void)
{
    static const char *const T[3] = {"WHERE YOU LEFT IT", "THE DEMO SONG", "AN EMPTY PROJECT"};
    static const char *const W[3] = {"the music as it was", "E minor, 100 BPM: four parts and a SONG", "four tracks, no steps"};
    uint32_t i;
    vg_rect_a(0, 30, 480, 256, 0x000000, 0.86f);
    vg_text("START WITH", 24, 60, VG_BOLD, C_RED, VG_LEFT);
    for (i = 0; i < 3u; i++) {
        int y = 88 + (int)i * 56, on = (int)i == kx_start_sel, off = i == 0u && !kx_had_session;
        char f[3] = {'F', (char)('1' + i), 0};
        vg_rect(24, y, 432, 48, on ? vg_mix(C_BG, C_RED, 0.3f) : 0x141416);
        vg_rect(24, y, 4, 48, on ? C_RED : 0x2A2A2E);
        vg_text(f, 40, y + 14, VG_BOLD, off ? 0x3A3A40 : C_RED, VG_LEFT);
        vg_text(T[i], 74, y + 6, VG_BIG, off ? 0x3A3A40 : C_WHITE, VG_LEFT);
        vg_text(off ? "nothing kept yet" : W[i], 74, y + 30, VG_SMALL, off ? 0x3A3A40 : C_DIM, VG_LEFT);
    }
    vg_text("DATA + ENTER, OR F1 F2 F3.  THE KEYS AND PADS PLAY AT ONCE", 240, 262, VG_SMALL, 0x55555C, VG_CENTER);
}

static void kx_draw_toast(void)
{
    uint32_t s1 = __atomic_load_n(&kx_msg_seq, __ATOMIC_ACQUIRE);
    if (!(s1 & 1u) && s1 != kx_toast_seq) {
        char b[24];
        memcpy(b, kx_msg, sizeof b);
        if (__atomic_load_n(&kx_msg_seq, __ATOMIC_ACQUIRE) == s1) {
            b[23] = 0;
            memcpy(kx_toast, b, sizeof b);
            kx_toast_seq = s1;
            kx_toast_until = kx_now_ms() + 1600u;
        }
    }
    if (kx_toast[0] && kx_now_ms() < kx_toast_until) {
        int w = vg_text_width(kx_toast, VG_MED) + 32;
        vg_round(240 - w / 2 + 2, 236, w, 30, 6, 0x000000);
        vg_round(240 - w / 2, 234, w, 30, 6, C_RED);
        vg_text(kx_toast, 240, 241, VG_MED, C_WHITE, VG_CENTER);
    }
}

static void kx_leds(void)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        vibe_led_button(VB_K1 + (int)i, C_RED, kx_fam == i ? 1.0f : 0.12f);
    for (i = 0; i < 4u; i++)
        vibe_led_button(VB_F1 + (int)i, TRK_RGB[i], i == song.sel ? 1.0f : 0.15f);
    vibe_led_button(VB_PLAY, C_RED, song.playing ? 1.0f : 0.1f);
    vibe_led_button(VB_REC, C_RED, song.rec & (1u << song.sel) ? 1.0f : 0.1f);
    if (kx_page == PG_GRID) {
        const track_t *t = TSEL;
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), b = kx_bank(), play = t->seq_idx % len;
        for (i = 0; i < 16u; i++) {
            uint32_t s = b * 16u + i;
            const step_t *st = &t->step[s];
            int on = drum_track(t) ? (int)((step_lanes(st) >> ui.lane) & 1u) : step_on(st);
            float lv = s >= len ? 0.0f : on ? 0.9f : 0.06f;
            if (song.playing && s == play)
                vibe_led_pad((int)i, C_WHITE, 1.0f);
            else
                vibe_led_pad((int)i, TRK_RGB[song.sel], lv);
        }
        return;
    }
    for (i = 0; i < 4u; i++) {
        const track_t *t = &trk[i];
        float a = (float)clamp(t->peak, 0, 32767) / 32768.0f;
        vibe_led_pad((int)i, TRK_RGB[i], t->p[P_MUTE] ? 0.05f : 0.35f + 0.65f * a);
        vibe_led_pad(4 + (int)i, TRK_RGB[i], 0.25f);
        vibe_led_pad(8 + (int)i, C_RED, 0.2f);
        vibe_led_pad(12 + (int)i, C_WHITE, kx_drum_track() >= 0 ? 0.25f : 0.03f);
    }
}

static void kx_labels(void)
{
    static const char *const K[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
    uint32_t i;
    for (i = 0; i < 6u; i++)
        vibe_label(VB_K1 + (int)i, K[i]);
    vibe_label(VB_F1, "T1");
    vibe_label(VB_F2, "T2");
    vibe_label(VB_F3, "T3");
    vibe_label(VB_F4, "T4");
    vibe_label(VB_F5, kx_f5_name());
    vibe_label(VB_F6, "UNDO");
}

/* the power-on picture: the creep grows across the screen and the name burns in; any control ends it at once (the
 * keys and pads play from the first moment) */
static uint64_t kx_boot_t;
static int kx_splash(void)
{
    uint64_t now = kx_now_ms();
    float t, heat;
    int w;
    if (kx_boot_done)
        return 0;
    if (!kx_boot_t)
        kx_boot_t = now ? now : 1;
    t = (float)(now - kx_boot_t) / 2200.0f;
    if (t >= 1.0f) {
        kx_boot_done = 1;
        return 0;
    }
    heat = 0.4f + 0.6f * (float)(fx_rand() & 255u) / 255.0f * (1.0f - t);
    vg_clear(C_BG);
    cr_draw(90, 10, 300, 300, t < 0.7f ? t / 0.7f : 1.0f, heat, C_RED);
    w = vg_text_width("KRIECHSTROM", VG_BIG | VG_X2);
    if (t > 0.35f) {
        float a = t < 0.55f ? (t - 0.35f) / 0.2f : 1.0f;
        int x = 240 - w / 2, wk;
        vg_rect_a(0, 118, 480, 86, C_BG, 0.65f * a);
        wk = vg_text("KRIECH", x, 128, VG_BIG | VG_X2, vg_mix(C_BG, C_WHITE, a), VG_LEFT);
        vg_text("STROM", x + wk, 128, VG_BIG | VG_X2, vg_mix(C_BG, C_RED, a), VG_LEFT);
        vg_text("BUILT ON FELUCCA", 240, 182, VG_SMALL, vg_mix(C_BG, 0x6A6A72, a), VG_CENTER);
    }
    kx_wear_fx(kx_fb, 480, 320);
    return 1;
}

static void kx_ui_draw(void)
{
    int pg = kx_page, kp = kx_knob_page();
    const track_t *t = TSEL;
    uint32_t col = TRK_RGB[song.sel];
    if (kx_view_bank >= 0 && vibe_frames() - kx_view_t > (uint64_t)(kx_sr * 3.0f))
        kx_view_bank = -1;                        /* (the pads' bank follows the playhead again after 3 s) */
    kx_labels();
    kx_leds();
    {
        static int viewed = -1;
        int kp2 = kx_knob_page();
        if (kp2 != viewed && kp2 >= 0 && kp2 < (int)NPAGES) {
            viewed = kp2;
            kx_post(KXC_VIEW, (uint32_t)kp2, 0, 0, 0);
        }
    }
    if ((pg == PG_INPUT || pg == PG_INPUT2) && !kx_in_asked) {
        kx_in_asked = 1;                          /* (the browser asks for the microphone now, not at every start) */
        vibe_request(VIBE_REQ_AUDIO_INPUT);
    }
    if (kx_help) {
        kx_draw_help();
        return;
    }
    if (kx_splash())
        return;
    if (!kx_idle_t)
        kx_idle_t = vibe_frames() ? vibe_frames() : 1;
    if (vibe_frames() - kx_idle_t > (uint64_t)(kx_sr * 90.0f) && !kx_start) {
        kx_draw_idle();                           /* (a minute and a half untouched) */
        return;
    }
    kx_bg_paint(kx_fb);
    kx_draw_header();
    kx_draw_tracks();
    kx_family_pages(kx_fam);
    kx_draw_title(kx_title(pg), pg >= 0 && pg < (int)NPAGES ? page_what(PAGES[pg].title) : 0);
    if (pg == PG_GRID) {
        kx_draw_grid(8, 96, 466, 110);
    } else if (pg == PG_BROWSE) {
        kx_draw_browse(4, 96, 472, 110);
    } else if (pg == PG_MASTER) {
        kx_draw_master(4, 96, 472, 110);
    } else if (pg == PG_MIXER) {
        kx_draw_mixer(4, 96, 472, 186);
    } else if (pg == PG_PROJECT) {
        kx_draw_project(4, 98, 472, 110);
    } else if (IS_PED_PAGE(pg)) {
        kx_draw_pedals(pg, 4, 92, 472, 116);
    } else if (pg == PG_USER) {
        kx_draw_user(8, 96, 464, 110);
    } else if (pg == PG_SAMPLER) {
        kx_draw_sampler(8, 96, 464, 110);
    } else if (pg == PG_FOOT) {
        kx_draw_foot(8, 96, 464, 110);
    } else if (pg == PG_MIDI) {
        kx_draw_midi(8, 98, 464, 108);
    } else if (pg == PG_INPUT || pg == PG_INPUT2) {
        kx_draw_input(4, 96, 472, 110);
    } else if (pg >= 0 && pg < (int)NPAGES) {
        const page_t *p = &PAGES[pg];
        switch (p->graph) {
        case GR_ADSR: {
            float ex, ey;
            kx_draw_adsr(12, 98, 456, 104, t->p[p->id[0]], t->p[p->id[1]], t->p[p->id[2]], t->p[p->id[3]], col);
            if (p->id[0] == P_ATK && kx_env_pos(t, 12, 98, 456, 104, &ex, &ey))
                kx_draw_dot(ex, ey, col);         /* (where the newest note is on it now) */
            break;
        }
        case GR_LFO: kx_draw_lfo(12, 100, 456, 100, t, col); break;
        case GR_FX: kx_draw_sends(12, 96, 456, 110, t, col); break;
        case GR_SCALE: kx_draw_scale(12, 98, 456, 104, t); break;
        case GR_ARP: kx_draw_arp(12, 98, 456, 104, t, col); break;
        case GR_SONG: kx_draw_song(8, 96, 464, 110); break;
        case GR_EVENTS: kx_draw_events(8, 98, 464, 108); break;
        default:
            if (t->eng_req == ENGI_FM6 && (p->scope == SC_ENGINE || fop_page(p->graph)))
                kx_draw_fm6(12, 94, 456, 112, song.sel, fop_page(p->graph) ? (int)fop_op : -1);   /* (kx_pics.h) */
            else
                kx_draw_scope(4, 98, 472, 106, p->scope == SC_GLOBAL ? C_RED : col, 1.2f);
            break;
        }
    }
    if (pg != PG_MIXER)
        kx_draw_cards(kp, 210);
    {
        static int last_order;
        int order = (int)kx_fam * 64 + (int)kx_fpage[kx_fam];
        kx_transition(kx_fb, pg, order > last_order ? 1 : -1);
        last_order = order;
    }
    kx_draw_soft();
    kx_wear_fx(kx_fb, 480, 320);
    if (kx_shift && !kx_start)
        kx_draw_shift();
    if (kx_start)
        kx_draw_start();
    kx_draw_toast();
    {   /* FOCUS (SDK 10): DATA walks, ENTER presses; each spot stands for a control of the device */
        uint32_t i, x = 120;
        static const char *const K[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
        vf_begin();
        for (i = 0; i < 6u; i++) {
            int w = vg_text_width(K[i], VG_SMALL);
            vf_key(VB_K1 + (int)i, (int)x - 3, 3, w + 6, 22);
            x += (uint32_t)w + 12u;
        }
        for (i = 0; i < NTRK; i++)
            vf_key(VB_F1 + (int)i, 8 + (int)i * 118, 30, 110, 44);
        if (pg != PG_MIXER)
            for (i = 0; i < 4u; i++)
                vf_encoder(VE_1 + (int)i, 6 + (int)i * 118, 204, 114, 74);
        for (i = 0; i < 6u; i++)
            vf_key(VB_F1 + (int)i, 4 + (int)i * 80, 286, 74, 32);
        vf_end(C_WHITE);
    }
}
