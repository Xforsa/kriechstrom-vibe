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
        vg_text(K[i], x, 7, VG_SMALL, on ? C_WHITE : 0xB4B4BC, VG_LEFT);
        if (on)
            vg_rect(x, 22, w, 2, C_RED);
        x += w + 12;
    }
    vg_text("MENU", x, 7, VG_SMALL, kx_fam == FA_MENU ? C_WHITE : 0xB4B4BC, VG_LEFT);   /* (always: a click opens it) */
    if (kx_fam == FA_MENU)
        vg_rect(x, 22, vg_text_width("MENU", VG_SMALL), 2, C_RED);
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
        uint32_t col = TRK_RGB[i], ink = mute ? 0x6A6A72 : sel ? C_WHITE : 0xC4C4CC;
        char nm[17], n[2] = {(char)('1' + i), 0};
        float a = kx_trk_pk[i];
        vg_rect(x, 32, 110, sel ? 3 : 1, mute ? 0x2A2A2E : sel ? col : vg_mix(C_BG, col, 0.45f));
        if (!mute)
            vg_rect(x, 32, (int)(a * 110.0f), 3, vg_mix(col, C_WHITE, 0.3f));
        vg_text(n, x, 39, VG_BOLD, mute ? 0x70707A : col, VG_LEFT);
        vg_text(ENGINES[eng_idx(t->eng_req)]->name, x + 13, 40, VG_SMALL, mute ? 0x70707A : 0x9A9AA4, VG_LEFT);
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
    int x = 8 + te_index(8, 77, (int)kx_fpage[kx_fam] + 1);   /* (the page's number in its family, a white plate) */
    vg_text(t, x, 78, VG_BOLD, C_RED, VG_LEFT);
    if (what)
        vg_text(what, x + vg_text_width(t, VG_BOLD) + 10, 78, VG_SMALL, 0x8A8A94, VG_LEFT);
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
        int x = 10 + (int)k * 115, w = 108, f = VG_BIG | VG_X2;   /* (the host's knob columns: x 10 + 115 k) */
        kx_card_of(pg, k, &c);
        int dcard = kx_data_on_cards() && (int)k == kx_data_card && c.on && c.label && c.label[0], ux = x + w, lred = 0;
        if (dcard) {                              /* (DATA's card: a red line over it, a red DATA plate in it; a long name:
                                                   * the name itself on red, no room for the word) */
            int tw = vg_text_width("DATA", VG_BOLD) + 8, uw = c.unit && c.unit[0] ? vg_text_width(c.unit, VG_SMALL) + 6 : 0;
            vg_rect(x - 4, y - 4, w + 8, 2, C_RED);
            if (vg_text_width(c.label, VG_SMALL) + uw + tw + 8 <= w) {
                vg_rect(x + w - tw + 2, y, tw, 16, C_RED);   /* (16 high: the 14 px letters whole) */
                vg_text("DATA", x + w - tw / 2 + 2, y + 1, VG_BOLD, C_BG, VG_CENTER);
                ux = x + w - tw - 4;
            } else
                lred = 1;
        }
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
        if (lred)
            vg_rect(x - 3, y, vg_text_width(c.label, VG_SMALL) + 6, 16, C_RED);
        vg_text(c.label, x, y + 2, VG_SMALL, lred ? C_BG : 0xCACAD2, VG_LEFT);
        if (vg_text_width(c.val, f) > w - 4)
            f = VG_BIG;
        vg_text(c.val, x - 1, f == VG_BIG ? y + 28 : y + 16, f, C_WHITE, VG_LEFT);
        if (c.unit && c.unit[0])
            vg_text(c.unit, ux, y + 2, VG_SMALL, 0xA8A8B0, VG_RIGHT);
        kx_ladder(x, y + 64, w, 4, c.pos, c.bip, col);
    }
}

static const char *const KX_START_KEY[4] = {"LAST", "DEMO", "EMPTY", "MINE"};   /* (the start screen's keys F1-F4) */
static void kx_draw_soft(void)
{
    static const char *const T0[4] = {"T1", "T2", "T3", "T4"};
    const char *const *T = kx_start ? KX_START_KEY : T0;
    uint32_t i;
    for (i = 0; i < 6u; i++) {
        int x = 2 + (int)i * 80;
        const char *s = i < 4u ? T[i] : kx_start ? "" : i == 4u ? kx_f5_name() : "UNDO";
        int sel = i < 4u && (kx_start ? (int)i == kx_start_sel : i == song.sel);
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
        vg_text(s, x + 39, 293, VG_BOLD, sel ? C_BG : i < 4u ? vg_mix(0x9A9AA4, TRK_RGB[i], 0.5f) : C_TEXT, VG_CENTER);
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
        vg_text("ENC 1 TURNS IT ON", x + w / 2, y + h / 2 + 16, VG_SMALL, TE_MID, VG_CENTER);
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
    int amt[6], i, bw = (w - 5 * 4) / 6, cs = h - 62;
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    char b[8];
    amt[0] = song.g[G_DMG];
    amt[1] = song.g[G_WALL];
    amt[2] = song.g[G_RES];
    amt[3] = song.g[G_SPLICE];
    amt[4] = song.g[G_HAUNT] ? song.g[G_HAMT] : 0;
    amt[5] = song.g[G_HISS];
    {   /* WEAR: how far the mix is worn, as a column of cells (the top one sparks with the mix) */
        int k, n = 6, ch = (cs - 4) / n, lit = (int)(cr_wear() * (float)n + 0.5f);
        vg_rect(x, y, cs + 40, cs, 0x0D0D0F);
        for (k = 0; k < n; k++) {
            int cy = y + cs - 2 - (k + 1) * ch;
            uint32_t c = k < lit ? (k == lit - 1 ? vg_mix(C_RED, 0xFFE0D0, heat > 1.0f ? 1.0f : heat) : C_RED) : 0x26262A;
            vg_rect(x + cs + 18, cy + 1, 16, ch - 2, c);
        }
        vg_text("WEAR", x + 6, y + 4, VG_SMALL, C_DIM, VG_LEFT);
        vg_itoa((int)(cr_wear() * 100.0f + 0.5f), b);
        vg_text(b, x + 6, y + 18, VG_BIG, C_WHITE, VG_LEFT);
    }
    kx_draw_reels(x + cs + 48, y, w - cs - 48, cs);
    {   /* the signal along the chain: sparks running left to right, as many and as fast as the mix is loud */
        float tm = (float)vibe_frames() / kx_sr;
        int n = 3 + (int)(heat * 14.0f), j;
        for (j = 0; j < n; j++) {
            float f = tm * (0.25f + heat * 0.6f) + (float)j / (float)n;
            float sx = (float)x + (f - (float)(int)f) * (float)w;
            vg_circle(sx, (float)(y + h - 61), 1.5f + heat * 1.5f, vg_mix(C_RED, C_WHITE, heat));
        }
    }
    for (i = 0; i < 6; i++) {
        int bx = x + i * (bw + 4), by = y + h - 58;
        float a = (float)clamp(amt[i], 0, 127) / 127.0f;
        int ix = bx + 4, iy = by + 20, iw = bw - 8, ih = 32;
        vg_round(bx, by, bw, 56, 4, a > 0 ? 0x1C1C20 : 0x121214);
        vg_rect(bx, by, bw, 2, a > 0 ? C_RED : 0x26262A);
        vg_text(N[i], bx + bw / 2, by + 3, VG_SMALL, a > 0 ? TE_INK : 0x9A9AA4, VG_CENTER);
        /* each effect its own machine, small (kx_te.h): the tape, the bricks, the strings, the cuts, the brake, grain */
        switch (i) {
        case 0: te_tape(ix, iy, iw, ih, a, te_g(G_DMGC), heat); break;
        case 1: te_wall(ix, iy, iw, ih, a, heat); break;
        case 2: te_strings(ix, iy - 2, iw, ih + 4, a, heat, 0); break;
        case 3: te_splice(ix - 2, iy, iw + 4, ih, a); break;
        case 4: te_haunt(ix, iy - 2, iw, ih + 4, a > 0 ? clamp(song.g[G_HAUNT], 0, 2) : 0, a); break;
        default: te_hiss(ix, iy, iw, ih, a); break;
        }
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
    for (r = 0; r < 6u && total; r++) {           /* (six whole rows: the seventh would be cut by the cards) */
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
        vg_text(b, lx + 8, ry, VG_SMALL, n == cur ? C_WHITE : r == 0u || r == 5u ? 0x7A7A84 : C_DIM, VG_LEFT);
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
    if (!kx_in_on) {                               /* nothing comes in: two jacks, a plug waiting */
        int j;
        te_dots(x, y, w, h - 20, 8);
        for (j = 0; j < 2; j++) {
            int jx = x + 40 + j * 90, jy = y + 34;
            vg_circle((float)jx, (float)jy, 20.0f, 0x2A2A30);
            vg_circle((float)jx, (float)jy, 12.0f, 0x111113);
            vg_circle((float)jx, (float)jy, 6.0f, 0x000000);
            vg_text(j ? "IN 2" : "IN 1", jx, jy + 26, VG_BOLD, TE_INK, VG_CENTER);
        }
        vg_rect(x + 240, y + 30, 60, 8, 0x8A8A92);   /* (the plug) */
        vg_rect(x + 300, y + 28, 40, 12, 0x3A3A42);
        vg_rect(x + 340, y + 32, w - 344, 4, 0x1E1E22);
        vg_text("NO INPUT: PLUG IN, OR ALLOW THE MICROPHONE", x + 4, y + h - 26, VG_BOLD, TE_INK, VG_LEFT);
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
        vg_text("CH", x, ry + 4, VG_SMALL, 0x8A8A94, VG_LEFT);
        vg_itoa(ch, b);
        vg_text(b, x + 22, ry + 2, VG_BOLD, kx_ic[IC_MOUT] ? TRK_RGB[k] : 0x70707A, VG_LEFT);
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
            vg_text(s2, x + 74 + (int)k * 60, ry, VG_BOLD, sl < 4u ? (on && k == ui.song_trk ? C_WHITE : TRK_RGB[k]) : 0x70707A,
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
        vg_text(l, cx + 4, cy, VG_SMALL, on ? C_WHITE : 0x8A8A94, VG_LEFT);
        if (up_used(s)) {
            up_name(s, b);
            b[10] = 0;
            vg_text(b, cx + 34, cy, VG_SMALL, on ? C_WHITE : C_TEXT, VG_LEFT);
        }
    }
}

/* ---- the pedalboard: each pedal drawn as one (its enclosure in its type's colour, a name plate, three knobs, the LED
 * lit by what it does, the footswitch), patch cables between them with the signal running along */
/* the pedals: flat graphite boxes, a stripe of the type's colour, three knobs with their names, a black window where
 * the pedal shows what it does to a wave (kx_te.h), its name, the footswitch and the LED */
static const uint32_t PED_RGB[IT_N] = {0x26262A, 0xE08A3A, 0xE0402A, 0xE8C040, 0xA060D0, 0x60C050, 0x40B8C8, 0x5A8AE8,
                                       0x50D0A0, 0xB8B850, 0xC07A40, 0xA8A8B0, 0xFF3A20, 0xE050A0};
static void kx_knob(float cx, float cy, float r, float pos, uint32_t ink)
{
    float a = 0.375f - pos * 0.75f, dx = vd_sin(a + 0.25f), dy = -vd_sin(a);   /* (270 degrees, as a real one) */
    vg_circle(cx, cy, r + 1.0f, 0x000000);
    vg_circle(cx, cy, r, 0x3C3C42);
    vg_circle(cx, cy, r - 2.0f, 0x18181B);
    vg_line(cx + dx * 2.0f, cy + dy * 2.0f, cx + dx * (r - 1.0f), cy + dy * (r - 1.0f), 2.2f, ink);
}
/* ---- the pedals: thirteen, each its own object (a face, a way to turn it, a screen, one thing only it has), in TE's
 * manner (flat colour, white print, numbered) worn as a NIN stage wears its gear. The controls: what each one is. */
enum { PK_FLAT, PK_CHICKEN, PK_FADER, PK_SQUARE, PK_DIAL, PK_CREAM, PK_HFADER, PK_BIG };
typedef struct { uint32_t body, ink; uint8_t knob, shape; } ped_face;   /* shape: 0 box, 1 wedge, 2 notched, 3 pill */
static const ped_face PED_FACE[IT_N] = {
    {0x111113, 0xF0F0EC, PK_FLAT, 0},
    {0xE6E2D8, 0x16161A, PK_FLAT, 3},             /* SOFT  white, the four colours */
    {0x4A2414, 0xF0E6DC, PK_CHICKEN, 0},          /* HARD  a rusted slab, chicken heads, rivets */
    {0xB4B6BA, 0x141416, PK_FADER, 2},            /* FOLD  aluminium, three faders, folded */
    {0x0E1A12, 0xF0F0EC, PK_FLAT, 0},             /* FUZZ  a bare board and its creature */
    {0x121214, 0xF0F0EC, PK_SQUARE, 0},           /* CRUSH hazard all round, square knobs, a bar of LEDs */
    {0xFF6A1A, 0x141416, PK_DIAL, 3},             /* PHASR TE orange, dials, a porthole */
    {0x2A4A8A, 0xF0F0EC, PK_CREAM, 1},            /* FLANG chipped blue enamel, a jet's stripes */
    {0x7AC8B0, 0x101412, PK_HFADER, 2},           /* CHOR  mint, sliders, a grille of waves */
    {0x2E1E16, 0xF0E6DC, PK_BIG, 1},              /* MUFF  one big knob, two small, on tape */
    {0x0C0C0E, 0xE8C060, PK_FLAT, 0},             /* BASS  black and brass, a needle */
    {0x6A6C70, 0xF0F0EC, PK_CHICKEN, 2},          /* SMASH dented steel, a red stomp */
    {0x1A1612, 0xF0F0EC, PK_SQUARE, 0},           /* DAMG  a board burnt, cracked, a wire hanging */
    {0x101A10, 0xB8FF60, PK_DIAL, 3}};            /* INFCT a tube of something green, bubbling */
static uint32_t ped_h;
static uint32_t ped_rnd(void) { ped_h = ped_h * 1664525u + 1013904223u; return ped_h >> 8; }
static int ped_rn(int n) { return (int)(ped_rnd() % (uint32_t)(n > 0 ? n : 1)); }
static void ped_ctl(int kind, float cx, float cy, float pos, uint32_t c, uint32_t body)
{
    float a = 0.375f - pos * 0.75f, dx = vd_sin(a + 0.25f), dy = -vd_sin(a);
    int i;
    switch (kind) {
    case PK_CHICKEN:                              /* a pointer, long */
        vg_circle(cx, cy + 1.0f, 9.0f, 0x000000);
        vg_circle(cx, cy, 8.0f, 0x1A1A1C);
        vg_line(cx - dx * 5.0f, cy - dy * 5.0f, cx + dx * 11.0f, cy + dy * 11.0f, 5.0f, 0x1A1A1C);
        vg_line(cx, cy, cx + dx * 10.0f, cy + dy * 10.0f, 1.6f, 0xF0F0EC);
        break;
    case PK_FADER: {                              /* a vertical slot, its cap */
        int top = (int)cy - 11, len = 22, py = top + (int)((1.0f - pos) * (float)len);
        vg_rect((int)cx - 1, top, 3, len, 0x141416);
        vg_rect((int)cx - 7, py - 3, 15, 7, 0x2A2A2E);
        vg_rect((int)cx - 7, py, 15, 1, 0xF0F0EC);
        break;
    }
    case PK_HFADER: {                             /* a horizontal slot */
        int l = (int)cx - 14, px = l + (int)(pos * 28.0f);
        vg_rect(l, (int)cy - 1, 29, 3, 0x0C1210);
        vg_round(px - 4, (int)cy - 7, 9, 15, 2, 0xF0F0EC);
        vg_rect(px, (int)cy - 5, 1, 11, 0x101412);
        break;
    }
    case PK_SQUARE:                               /* a square knob */
        vg_round((int)cx - 9, (int)cy - 8, 19, 19, 3, 0x000000);
        vg_round((int)cx - 9, (int)cy - 9, 18, 18, 3, c);
        vg_line(cx, cy, cx + dx * 7.0f, cy + dy * 7.0f, 2.0f, 0x000000);
        break;
    case PK_DIAL:                                 /* a dial in a ring of ticks */
        for (i = 0; i <= 10; i++) {
            float t = 0.375f - (float)i * 0.075f, tx = vd_sin(t + 0.25f), ty = -vd_sin(t);
            vg_line(cx + tx * 9.5f, cy + ty * 9.5f, cx + tx * 11.5f, cy + ty * 11.5f, 1.0f, vg_mix(body, 0x000000, 0.6f));
        }
        vg_circle(cx, cy, 8.0f, 0xF0F0EC);
        vg_line(cx, cy, cx + dx * 7.0f, cy + dy * 7.0f, 2.0f, 0x141416);
        break;
    case PK_CREAM:                                /* cream, a dark skirt */
        vg_circle(cx, cy, 11.0f, 0x141416);
        vg_circle(cx, cy, 8.0f, 0xE8DCC0);
        vg_line(cx, cy, cx + dx * 10.0f, cy + dy * 10.0f, 2.0f, 0x141416);
        break;
    case PK_BIG:                                  /* (MUFF's first: big) */
        vg_circle(cx, cy + 1.0f, 14.0f, 0x000000);
        vg_circle(cx, cy, 13.0f, 0x141416);
        vg_circle(cx, cy, 10.0f, 0x2A2A2E);
        vg_line(cx, cy, cx + dx * 12.0f, cy + dy * 12.0f, 2.5f, 0xF0F0EC);
        break;
    default:                                      /* flat, one colour */
        vg_circle(cx, cy + 1.0f, 10.0f, 0x000000);
        vg_circle(cx, cy, 10.0f, c);
        vg_circle(cx, cy, 7.0f, vg_mix(c, 0xFFFFFF, 0.18f));
        vg_circle(cx + dx * 6.0f, cy + dy * 6.0f, 1.8f, c == 0xE8E8E4 ? 0x141416 : 0xFFFFFF);
        break;
    }
}
static void ped_body(int ty, int x, int y, int w, int h, const ped_face *F, uint32_t col)
{
    int i;
    ped_h = 0x9E3779B9u * (uint32_t)(ty + 7);
    switch (F->shape) {
    case 1:                                       /* a wedge: the top slants back */
        vg_round(x, y + 4, w, h - 4, 5, F->body);
        vg_tri((float)x, (float)(y + 4), (float)(x + w), (float)y, (float)(x + w), (float)(y + 4), F->body);
        vg_line((float)x, (float)(y + 4), (float)(x + w), (float)y, 1.0f, vg_mix(F->body, 0xFFFFFF, 0.3f));
        vg_rect(x, y + h - 4, w, 4, vg_mix(F->body, 0x000000, 0.4f));   /* (its foot, the wedge's thick end) */
        break;
    case 2:                                       /* corners cut */
        vg_rect(x + 6, y, w - 12, h, F->body);
        vg_rect(x, y + 6, w, h - 12, F->body);
        vg_tri((float)x, (float)(y + 6), (float)(x + 6), (float)y, (float)(x + 6), (float)(y + 6), F->body);
        vg_tri((float)(x + w), (float)(y + 6), (float)(x + w - 6), (float)y, (float)(x + w - 6), (float)(y + 6), F->body);
        vg_tri((float)x, (float)(y + h - 6), (float)(x + 6), (float)(y + h), (float)(x + 6), (float)(y + h - 6), F->body);
        vg_tri((float)(x + w), (float)(y + h - 6), (float)(x + w - 6), (float)(y + h), (float)(x + w - 6), (float)(y + h - 6), F->body);
        break;
    case 3: vg_round(x, y, w, h, 14, F->body); break;   /* a pill */
    default: vg_round(x, y, w, h, 3, F->body); break;
    }
    switch (ty) {                                 /* its surface */
    case 2:                                       /* rust */
        for (i = 0; i < 60; i++) {
            uint32_t c = ty == 2 ? ((ped_rnd() % 3u) == 0u ? 0x7A3A1A : (ped_rnd() & 1u) ? 0x5E2C14 : 0x36180A)
                                 : ((ped_rnd() & 1u) ? 0x0A0806 : 0x2A2018);
            vg_circle((float)(x + 4 + ped_rn(w - 8)), (float)(y + 4 + ped_rn(h - 8)), 2.0f + (float)ped_rn(6), c);
        }
        break;
    case 3:                                       /* brushed, and folded: two creases */
        for (i = 1; i < h - 1; i += 2)
            vg_rect(x + 2, y + i, w - 4, 1, (ped_rnd() & 1u) ? 0xBEC0C4 : 0xACAEB2);
        vg_line((float)(x + 2), (float)(y + h - 30), (float)(x + 50), (float)(y + h - 2), 1.0f, 0x8A8C90);
        vg_line((float)(x + w - 40), (float)(y + 2), (float)(x + w - 2), (float)(y + 30), 1.0f, 0xD8DADE);
        break;
    case 4: case 12: {                            /* a board's traces */
        uint32_t cu = vg_mix(F->body, 0xB87333, 0.45f);
        for (i = 0; i < 9; i++) {
            int x0 = x + 4 + ped_rn(w - 8), y0 = y + 4 + ped_rn(h - 8), x1 = x + 4 + ped_rn(w - 8);
            vg_line((float)x0, (float)y0, (float)x1, (float)y0, 1.0f, cu);
            vg_line((float)x1, (float)y0, (float)x1, (float)(y0 + 8), 1.0f, cu);
            vg_circle((float)x1, (float)(y0 + 8), 1.8f, cu);
        }
        for (i = 0; ty == 12 && i < 26; i++)      /* (DAMG's board: burnt) */
            vg_circle((float)(x + 4 + ped_rn(w - 8)), (float)(y + 4 + ped_rn(h - 8)), 2.0f + (float)ped_rn(7),
                      (ped_rnd() & 1u) ? 0x050403 : 0x2A2018);
        break;
    }
    case 5: {                                     /* hazard stripes all round */
        int sx;
        for (sx = x; sx < x + w; sx += 10) {
            vg_tri((float)sx, (float)(y + h), (float)(sx + 5), (float)(y + h), (float)(sx + 9), (float)(y + h - 5), 0xE8C020);
        }
        break;
    }
    case 7:                                       /* chipped enamel, a jet's stripes */
        vg_rect(x, y + h - 36, w, 3, 0xE8DCC0), vg_rect(x, y + h - 31, w, 2, 0xE8DCC0);
        for (i = 0; i < 18; i++)
            vg_round(x + (ped_rnd() & 1u ? 1 : w - 6) , y + 8 + ped_rn(h - 14), 5, 4, 1, 0x3A3A40);
        break;
    case 8:                                       /* a grille of waves */
        for (i = 0; i < 4; i++) {
            int k;
            for (k = 0; k < 7; k++)
                vg_line((float)(x + 8 + k * 6), (float)(y + h - 34 + i * 4 + (k & 1) * 2), (float)(x + 14 + k * 6),
                        (float)(y + h - 34 + i * 4 + ((k + 1) & 1) * 2), 1.0f, 0x4A8A78);
        }
        break;
    case 9:                                       /* felt */
        for (i = 0; i < 120; i++)
            vg_rect(x + 2 + ped_rn(w - 4), y + 2 + ped_rn(h - 4), 2, 1, (ped_rnd() & 1u) ? 0x3A281E : 0x24160E);
        break;
    case 10:                                      /* a brass edge */
        vg_frame(x, y, w, h, 2, 0xB8903A);
        break;
    case 11:                                      /* dents, and hazard tape across */
        for (i = 0; i < 6; i++) {
            float dx = (float)(x + 10 + ped_rn(w - 20)), dy = (float)(y + 10 + ped_rn(h - 20));
            vg_circle(dx, dy, 6.0f, 0x5A5C60);
            vg_circle(dx - 1.0f, dy - 1.0f, 4.0f, 0x7A7C80);
        }
        vg_tri((float)x, (float)(y + h - 22), (float)(x + 22), (float)(y + h), (float)x, (float)(y + h), 0xE8C020);
        break;
    case 13: {                                    /* a tube of green, lit with the sound */
        vg_round(x + 3, y + 46, w - 6, 32, 10, 0x2A4A1A);
        break;
    }
    case 1:                                       /* white: a grid of dots, a speaker */
        for (i = 0; i < 12; i++)
            vg_circle((float)(x + w - 52 + (i % 4) * 4), (float)(y + h - 20 + (i / 4) * 4), 1.0f, 0xA8A49A);
        break;
    default: break;
    }
}
static void ped_mark(int ty, int x, int y, int w, int h, float lvl, const ped_face *F)   /* the one thing only it has */
{
    float t = te_t();
    int i;
    switch (ty) {
    case 2:                                       /* rivets */
        for (i = 0; i < 6; i++)
            vg_circle((float)(i < 3 ? x + 4 : x + w - 5), (float)(y + 12 + (i % 3) * 34), 2.0f, 0xC8B8A8);
        break;
    case 4: {                                     /* the fuzz creature: a ball of hair with eyes */
        float cx = (float)(x + w - 52), cy = (float)(y + h - 14);
        for (i = 0; i < 10; i++) {
            float a = (float)i * 0.1f;
            vg_line(cx, cy, cx + vd_sin(a) * (8.0f + lvl * 3.0f), cy + vd_sin(a + 0.25f) * (8.0f + lvl * 3.0f), 1.0f, 0x9A70D0);
        }
        vg_circle(cx, cy, 5.0f, 0x6A40A0);
        vg_circle(cx - 2.0f, cy - 1.0f, 1.2f, 0xFFFFFF), vg_circle(cx + 2.0f, cy - 1.0f, 1.2f, 0xFFFFFF);
        break;
    }
    case 5:                                       /* a bar of LEDs under the screen, as loud as it is */
        for (i = 0; i < 10; i++)
            vg_rect(x + 8 + i * 6, y + h - 25, 4, 2, (float)i < lvl * 10.0f ? (i > 7 ? 0xFF3020 : 0x60D050) : 0x1E2A1E);
        break;
    case 10: {                                    /* a needle meter */
        int mx = x + w - 64, my = y + h - 24;
        float a = -0.12f + 0.24f * (lvl > 1.0f ? 1.0f : lvl);
        vg_round(mx, my, 26, 18, 2, 0xE8DCB0);
        vg_line((float)(mx + 13), (float)(my + 17), (float)(mx + 13) + vd_sin(a) * 14.0f, (float)(my + 17) - vd_sin(a + 0.25f) * 14.0f, 1.0f, 0x141416);
        break;
    }
    case 12:                                      /* a crack and a wire hanging out */
        vg_line((float)(x + 22), (float)(y + h), (float)(x + 28), (float)(y + h - 10), 1.4f, 0x030303);
        vg_line((float)(x + 28), (float)(y + h - 10), (float)(x + 24), (float)(y + h - 18), 1.2f, 0x030303);
        vg_line((float)(x + w - 6), (float)(y + 10), (float)(x + w + 4), (float)(y + 22 + vd_sin(t) * 3.0f), 1.5f, 0xD02A1A);
        vg_line((float)(x + w + 4), (float)(y + 22 + vd_sin(t) * 3.0f), (float)(x + w + 1), (float)(y + 34), 1.5f, 0xD02A1A);
        break;
    case 13:                                      /* bubbles rising in the tube */
        for (i = 0; i < 5; i++) {
            float u = te_wrap(t * (0.3f + 0.1f * (float)i) + (float)i * 0.21f);
            vg_circle((float)(x + 8 + i * 4), (float)(y + 74) - u * 24.0f, 1.5f + lvl, 0x90E050);
        }
        break;
    default: break;
    }
    (void)F;
}
/* each pedal's screen, in its own way: what it does to a wave (te_shape), drawn as its kind of screen would */
static void ped_screen(int ty, int x, int y, int w, int h, const int16_t *v, float lv, int on)
{
    float t = te_t(), d = (float)clamp(v[1], 0, 127) / 127.0f, amp = (float)h * 0.42f * (0.35f + 0.65f * vd_minf(lv, 1.0f));
    int n = w / 2, i, cy = y + h / 2, px = x, py = cy;
    static int vy[200];
    uint32_t bg = 0x040405, ink = PED_RGB[ty], dim;
    if (n > 199)
        n = 199;
    for (i = 0; i <= n; i++) {
        float u = (float)i / (float)n, sn = vd_sin(u * 2.0f + t * 0.4f);
        vy[i] = cy - (int)(vd_clamp(te_shape(ty, sn, t, d, u), -1.1f, 1.1f) * amp);
    }
    switch (ty) {                                 /* the screen itself */
    case 1: bg = 0x9AA88A, ink = 0x1E2A1A; break;                 /* SOFT: a pale LCD */
    case 2: bg = 0x041008, ink = 0x60FF80; break;                 /* HARD: green phosphor */
    case 3: bg = 0x140C04, ink = 0xFFB030; break;                 /* FOLD: amber dots */
    case 4: bg = 0x0A0610, ink = 0xB080F0; break;                 /* FUZZ: a pocket synth's pixels */
    case 5: bg = 0x050805, ink = 0x60D050; break;                 /* CRUSH: LED columns */
    case 6: bg = 0x100602, ink = 0xFFFFFF; break;                 /* PHASR: a porthole */
    case 7: bg = 0x140A02, ink = 0xFFA040; break;                 /* FLANG: an amber tube */
    case 8: bg = 0x06201A, ink = 0x9AF0D0; break;                 /* CHOR: two waves */
    case 9: bg = 0x1A0604, ink = 0xFF6A30; break;                 /* MUFF: a valve's glow */
    case 10: bg = 0x050505, ink = 0xC8A050; break;                /* BASS: the weight under the line */
    case 11: bg = 0x0A0A0A, ink = 0xE02414; break;                /* SMASH: the peaks flattened */
    case 12: bg = 0x050303, ink = 0xFF3020; break;                /* DAMG: torn */
    case 13: bg = 0x061006, ink = 0x90E050; break;                /* INFCT: something green */
    default: break;
    }
    if (!on)
        ink = vg_mix(bg, ink, 0.3f);
    dim = vg_mix(bg, ink, 0.18f);
    if (ty != 6 && ty != 13)
        vg_rect(x, y, w, h, bg);
    switch (ty) {
    case 1:                                       /* LCD: a faint grid of its pixels, a dark line */
        for (i = x; i < x + w; i += 3) vg_rect(i, y, 1, h, vg_mix(bg, 0x000000, 0.06f));
        for (i = 1; i <= n; i++) vg_line((float)px, (float)py, (float)(x + i * w / n), (float)vy[i], 2.0f, ink), px = x + i * w / n, py = vy[i];
        break;
    case 2:                                       /* a scope: its grid, the trace glowing */
        for (i = x; i < x + w; i += 10) vg_rect(i, y, 1, h, 0x0C2414);
        vg_rect(x, cy, w, 1, 0x0C2414);
        for (i = 1; i <= n; i++) {
            int nx = x + i * w / n;
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 4.0f, vg_mix(bg, ink, 0.3f));
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 1.4f, ink);
            px = nx, py = vy[i];
        }
        break;
    case 3: case 4: {                             /* dots / chunky pixels */
        int g = ty == 3 ? 3 : 4, xx;
        for (xx = 0; xx < w; xx += g) {
            int k = xx * n / w, yy;
            int q = ((vy[k] - y) / g) * g + y;
            if (ty == 3)
                for (yy = y + 1; yy < y + h; yy += g) vg_rect(x + xx, yy, 1, 1, dim);
            vg_rect(x + xx, q, ty == 3 ? 2 : 3, ty == 3 ? 2 : 3, ink);
        }
        if (ty == 4)
            for (i = y; i < y + h; i += 2) vg_rect(x, i, w, 1, vg_mix(bg, 0x000000, 0.4f));
        break;
    }
    case 5: {                                     /* LED columns from the middle */
        int xx;
        for (xx = 0; xx < w; xx += 5) {
            int k = xx * n / w, top = vy[k] < cy ? vy[k] : cy, hh = vy[k] < cy ? cy - vy[k] : vy[k] - cy;
            vg_rect(x + xx, top, 4, hh + 1, (vy[k] < y + 5 || vy[k] > y + h - 5) ? 0xFF3020 : ink);
        }
        break;
    }
    case 6: {                                     /* the porthole: a sweep going round, the trace white */
        float a = te_wrap(t * 0.25f);
        vg_rect(x + (int)(a * (float)w), y + 2, 3, h - 4, vg_mix(0x100602, 0xFF6A1A, 0.5f));
        for (i = 1; i <= n; i++) vg_line((float)px, (float)py, (float)(x + i * w / n), (float)vy[i], 1.8f, ink), px = x + i * w / n, py = vy[i];
        break;
    }
    case 7:                                       /* an amber tube: lines across, the trace glowing */
        for (i = y; i < y + h; i += 3) vg_rect(x, i, w, 1, vg_mix(bg, ink, 0.07f));
        for (i = 1; i <= n; i++) {
            int nx = x + i * w / n;
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 5.0f, vg_mix(bg, ink, 0.25f));
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 1.6f, ink);
            px = nx, py = vy[i];
        }
        break;
    case 8: {                                     /* two waves, drifting apart and back */
        int q;
        for (q = 0; q < 2; q++) {
            px = x, py = cy;
            for (i = 1; i <= n; i++) {
                float u = (float)i / (float)n, sn = vd_sin(u * 2.0f + t * 0.4f + (float)q * (0.08f + 0.06f * vd_sin(t * (0.5f + d))));
                int nx = x + i * w / n, ny = cy - (int)(sn * amp);
                vg_line((float)px, (float)py, (float)nx, (float)ny, q ? 1.4f : 2.2f, q ? vg_mix(bg, ink, 0.55f) : ink);
                px = nx, py = ny;
            }
        }
        break;
    }
    case 9:                                       /* fat and glowing */
        for (i = 1; i <= n; i++) {
            int nx = x + i * w / n;
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 7.0f, vg_mix(bg, ink, 0.25f));
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 3.5f, ink);
            px = nx, py = vy[i];
        }
        break;
    case 10: case 11: {                           /* filled to the middle; SMASH: its flat tops in white */
        int xx;
        for (xx = 0; xx < w; xx += 2) {
            int k = xx * n / w, top = vy[k] < cy ? vy[k] : cy, hh = vy[k] < cy ? cy - vy[k] : vy[k] - cy;
            vg_rect(x + xx, top, 2, hh + 1, vg_mix(bg, ink, 0.6f));
            vg_rect(x + xx, vy[k], 2, 1, ty == 11 && (hh > (int)(amp * 0.8f)) ? 0xFFFFFF : ink);
        }
        break;
    }
    case 12: {                                    /* torn: pieces shifted, gaps */
        uint32_t r = (uint32_t)(t * 12.0f) * 2654435761u;
        for (i = 1; i <= n; i++) {
            int nx = x + i * w / n, off;
            r = r * 1664525u + 1013904223u;
            off = (r >> 28) == 0u && on ? (int)((r >> 20) & 7u) - 3 : 0;
            if (vy[i] != cy)
                vg_line((float)px, (float)(py + off), (float)nx, (float)(vy[i] + off), 2.0f, (r >> 27) == 1u ? 0xFFFFFF : ink);
            px = nx, py = vy[i];
        }
        break;
    }
    case 13:                                      /* the source's hits, the track following, green */
        vg_circle((float)(x + 9), (float)cy, 6.0f + 3.0f * lv, C_RED);
        vg_circle((float)(x + 9), (float)cy, 3.0f, 0x000000);
        for (i = 1; i <= n; i++) {
            int nx = x + 20 + i * (w - 20) / n;
            if (i == 1) px = x + 20;
            vg_line((float)px, (float)py, (float)nx, (float)vy[i], 2.0f, ink);
            px = nx, py = vy[i];
        }
        break;
    default:
        for (i = 1; i <= n; i++) vg_line((float)px, (float)py, (float)(x + i * w / n), (float)vy[i], 2.0f, ink), px = x + i * w / n, py = vy[i];
        break;
    }
}
static void kx_pedal(int x, int y, int w, int h, const int16_t *v, int focus, float lvl, int num)
{
    int ty = clamp(v[0], 0, IT_N - 1), k;
    uint32_t col = PED_RGB[ty];
    const ped_face *F = &PED_FACE[ty];
    static const uint32_t KC[3] = {0x3A78D8, 0x34B464, 0xE8E8E4};   /* (TE's knob colours, one each) */
    if (!ty) {                                                      /* an empty place on the board */
        char e[2] = {(char)('1' + num), 0};
        vg_round(x, y, w, h, 6, 0x111113);
        vg_frame(x, y, w, h, 1, focus ? C_RED : 0x2E2E34);
        vg_text(e, x + 6, y + 4, VG_BOLD, 0x9A9AA4, VG_LEFT);
        vg_text("EMPTY", x + w / 2, y + h / 2 - 18, VG_BOLD, focus ? TE_INK : 0x9A9AA4, VG_CENTER);
        if (focus)
            vg_text("ENC 1: A PEDAL", x + w / 2, y + h / 2 + 2, VG_SMALL, TE_MID, VG_CENTER);
        return;
    }
    vg_round(x + 2, y + 3, w, h, 6, 0x000000);                      /* (its shadow) */
    ped_body(ty, x, y, w, h, F, col);
    for (k = 0; k < 3; k++) {                                       /* the controls, their names printed */
        const param_desc_t *dd = ins_desc(ty, (uint32_t)k);
        float pos = dd->max > dd->min ? (float)(v[1 + k] - dd->min) / (float)(dd->max - dd->min) : 0.0f;
        float cx = (float)x + (float)w * (0.2f + 0.3f * (float)k), cy = (float)y + 32.0f;
        int kind = F->knob == PK_BIG ? (k ? PK_FLAT : PK_BIG) : F->knob;
        char lb[6];
        if (F->knob == PK_BIG)
            cx = k ? (float)x + (float)w * (0.5f + 0.3f * (float)(k - 1)) + 4.0f : (float)x + 26.0f, cy = (float)y + 32.0f;
        str_cpy(lb, dd->label, sizeof lb);
        ped_ctl(kind, cx, cy, pos, kind == PK_FLAT && F->knob == PK_BIG ? 0x5A4A40 : k == 2 && ty == 1 ? 0xFF6A1A : KC[k], F->body);
        vg_text(lb, (int)cx, y + 4, VG_SMALL, F->ink, VG_CENTER);   /* (above it: nothing under the name) */
    }
    {   /* the screen: what it does to a wave (its frame is the pedal's own) */
        int sx = x + 6, sy = y + 50, sw = w - 12, sh = 24;
        if (ty == 6)
            vg_round(sx - 2, sy - 2, sw + 4, sh + 4, 14, 0x141416), vg_round(sx, sy, sw, sh, 12, 0x100602);
        else if (ty == 13)
            vg_round(sx + 18, sy, sw - 22, sh, 8, 0x061006);
        else
            vg_rect(sx, sy, sw, sh, 0x040405), vg_frame(sx - 1, sy - 1, sw + 2, sh + 2, 1, vg_mix(F->body, 0x000000, 0.5f));
        ped_screen(ty, sx + (ty == 13 ? 18 : 0), sy, sw - (ty == 13 ? 22 : 0), sh, v, v[PD_MIX] ? lvl : 0.0f, v[PD_MIX] != 0);
        if (ty == 12 && v[PD_MIX] && lvl > 0.2f)   /* (DAMG's screen tears) */
            vg_rect(sx, sy + (int)(te_wrap(te_t() * 3.0f) * (float)sh), sw, 2, 0xFF3020);
    }
    ped_mark(ty, x, y, w, h, v[PD_MIX] ? lvl : 0.0f, F);
    if (ty == 9 || ty == 3) {                                       /* MUFF, FOLD: the name on masking tape */
        int tw = vg_text_width(N_ITYPE[ty], VG_BOLD);
        vg_rect(x + 7, y + h - 22, tw + 10, 17, 0xD9CFA8);
        vg_text(N_ITYPE[ty], x + 12, y + h - 21, VG_BOLD, 0x16161A, VG_LEFT);
    } else
        vg_text(N_ITYPE[ty], x + 8, y + h - 21, ty == 2 || ty == 11 ? VG_MED : VG_BOLD, F->ink, VG_LEFT);
    {                                                               /* the LED */
        float a = lvl > 1.0f ? 1.0f : lvl;
        int lx = x + w - 40, ly = y + h - 15;
        if (v[PD_MIX] && a > 0.05f)
            vg_rect_a(lx - 3, ly - 3, 11, 9, 0xFF3020, 0.35f * a);
        vg_rect(lx, ly, 5, 3, v[PD_MIX] ? vg_mix(0x501008, 0xFF3020, 0.4f + 0.6f * a) : 0x300606);
    }
    if (ty == 11) {                                                 /* SMASH: a big red stomp */
        vg_circle((float)(x + w - 17), (float)(y + h - 15), 11.0f, 0x000000);
        vg_circle((float)(x + w - 17), (float)(y + h - 16), 10.0f, v[PD_MIX] ? 0xE02414 : 0x701008);
        vg_circle((float)(x + w - 19), (float)(y + h - 18), 4.0f, v[PD_MIX] ? 0xFF6A50 : 0x902018);
    } else if (F->shape == 3 || ty == 1) {                          /* a round rubber key */
        vg_circle((float)(x + w - 17), (float)(y + h - 15), 10.0f, 0x0A0A0C);
        vg_circle((float)(x + w - 17), (float)(y + h - 16), 9.0f, v[PD_MIX] ? 0x5A5A60 : 0x3A3A40);
    } else {                                                        /* a square one */
        vg_round(x + w - 28, y + h - 26, 22, 22, 4, 0x0A0A0C);
        vg_round(x + w - 27, y + h - 26, 20, 19, 4, v[PD_MIX] ? 0x5A5A60 : 0x3A3A40);
        vg_round(x + w - 25, y + h - 24, 16, 6, 3, v[PD_MIX] ? 0x707078 : 0x48484E);
    }
    if (focus)                                                      /* (the one the knobs turn) */
        vg_frame(x - 3, y - 3, w + 6, h + 6, 2, C_RED);
}
static void kx_draw_pedals(int pg, int x, int y, int w, int h)
{
    uint32_t o = kx_ped_owner(pg), j, ns = (uint32_t)kx_ped_slots(o);
    int pw = KX_PED_W, px[PED_SLOTS], cy = y + h / 2 + 4;
    float tm = te_t();
    const char *in = o == PO_IN ? "IN" : o == PO_MASTER ? "MIX" : "SOUND";
    const char *out = o == PO_MASTER ? "MASTER" : "MIX";
    int16_t v[PED_SLOTS][PD_N];
    for (j = 0; j < ns; j++) {
        px[j] = kx_ped_x(ns, j) - 4 + x;
        kx_ped_get(o, j, v[j]);
    }
    /* the cables: IN to the first, one to the next, the last to OUT; the signal runs along as square pulses, as many
     * and as bright as the pedal before it works */
    for (j = 0; j <= ns; j++) {
        float x0 = j ? (float)(px[j - 1] + pw) : (float)x, x1 = j < ns ? (float)px[j] : (float)(x + w), a;
        int k;
        vg_rect((int)x0, cy - 1, (int)(x1 - x0), 3, 0x3A3A40);
        if (!j)
            a = o < NTRK ? kx_trk_pk[o] : kx_ped_lvl[o][0];
        else
            a = v[j - 1][0] && v[j - 1][PD_MIX] ? kx_ped_lvl[o][j - 1] * 2.0f : 0.0f;
        for (k = 0; k < 3 && a > 0.03f; k++) {
            float f = te_wrap(tm * 0.8f + (float)k / 3.0f);
            vg_rect((int)(x0 + f * (x1 - x0)) - 2, cy - 2, 4, 5, vg_mix(C_RED, TE_INK, vd_minf(a, 1.0f)));
        }
    }
    (void)in, (void)out;
    vg_tri((float)(x + w - 8), (float)(cy - 5), (float)(x + w - 8), (float)(cy + 6), (float)(x + w), (float)(cy + 1), TE_MID);
    for (j = 0; j < ns; j++)
        kx_pedal(px[j], y + 4, pw, h - 8, v[j], (int)j == kx_ped_focus[kx_ped_fi(pg)], kx_ped_lvl[o][j] * 2.0f, (int)j);
}

/* FOOT PEDAL: the expression pedal is a hand on a knob: what it turns (DAMAGE, WALL or the level) is shown as a gauge
 * beside it, moving with it. The switch, with what it does. Both can be clicked: drag the pedal, press the switch */
static void kx_draw_foot(int x, int y, int w, int h)
{
    int ex = x, ew = 170, gx = x + 196, sx = x + w - 120, cy = y + h - 36, k;
    float e = kx_foot_expr, lift = e * 50.0f;
    char b[12];
    int tgt = clamp(kx_ic[IC_EXPR], 0, 3);
    float val = tgt == 1 ? te_g(G_DMG) : tgt == 2 ? te_g(G_WALL) : tgt == 3 ? (float)TSEL->p[P_LEVEL] / 127.0f : 0.0f;
    te_dots(x, y, w, h, 8);
    /* the pedal, from the side: the base, the hinge at the heel, the treadle up as far as it is pushed */
    vg_rect(ex + 6, cy + 8, ew - 12, 10, 0x2A2A30);
    vg_circle((float)(ex + ew - 20), (float)(cy + 4), 7.0f, TE_MID);
    vg_line((float)(ex + ew - 20), (float)(cy + 2), (float)(ex + 14), (float)cy - lift, 14.0f, 0x3A3A42);
    vg_line((float)(ex + ew - 20), (float)(cy - 5), (float)(ex + 14), (float)cy - lift - 7.0f, 3.0f, tgt ? C_RED : TE_LOW);
    for (k = 0; k < 6; k++) {                     /* (its grip) */
        float u = 0.15f + (float)k * 0.12f;
        float gx0 = (float)(ex + ew - 20) + (float)(14 - (ew - 20)) * u, gy0 = (float)(cy - 2) - lift * u;
        vg_rect((int)gx0 - 1, (int)gy0 - 6, 3, 3, 0x6A6A72);
    }
    vg_itoa((int)(e * 100.0f + 0.5f), b);
    str_cpy(b + str_len(b), "%", 2);
    vg_text(b, ex + ew - 4, y + 2, VG_BIG, TE_INK, VG_RIGHT);
    vg_text("EXPRESSION", ex + 6, y + h - 18, VG_SMALL, TE_MID, VG_LEFT);
    /* what it turns: an arrow to a gauge of that value */
    vg_line((float)(ex + ew + 2), (float)(y + h / 2), (float)(gx - 6), (float)(y + h / 2), 2.0f, tgt ? C_RED : TE_LOW);
    vg_tri((float)(gx - 10), (float)(y + h / 2 - 5), (float)(gx - 10), (float)(y + h / 2 + 5), (float)(gx - 2), (float)(y + h / 2),
           tgt ? C_RED : TE_LOW);
    {
        float r = 40.0f, ccx = (float)(gx + 50), ccy = (float)(y + h / 2 + 22);
        int i;
        for (i = 0; i < 24; i++) {
            float a0 = 0.5f - (float)i / 24.0f * 0.5f, a1 = 0.5f - (float)(i + 1) / 24.0f * 0.5f;
            uint32_t c = tgt && (float)i / 24.0f < val ? C_RED : 0x2A2A30;
            vg_line(ccx + vd_sin(a0 + 0.25f) * r, ccy - vd_sin(a0) * r, ccx + vd_sin(a1 + 0.25f) * r, ccy - vd_sin(a1) * r, 7.0f, c);
        }
        vg_text(IC_EXPR_N[tgt], (int)ccx, (int)ccy - 18, VG_BOLD, tgt ? TE_INK : 0x9A9AA4, VG_CENTER);
        vg_itoa((int)(val * 127.0f + 0.5f), b);
        if (tgt)
            vg_text(b, (int)ccx, (int)ccy - 2, VG_BOLD, C_RED, VG_CENTER);
    }
    /* the switch: a big button, lit while pressed, its action on it */
    vg_round(sx, y + 8, 112, h - 30, 8, 0x26262A);
    vg_circle((float)(sx + 56), (float)(y + 40), 22.0f, 0x111113);
    vg_circle((float)(sx + 56), (float)(y + 40), 18.0f, kx_foot_down ? C_RED : 0x8A8A92);
    vg_circle((float)(sx + 56), (float)(y + 39), 12.0f, kx_foot_down ? 0xFF6A50 : 0xC8C8CE);
    vg_text(IC_FSW_N[clamp(kx_ic[IC_FSW], 0, 3)], sx + 56, y + 64, VG_BOLD, TE_INK, VG_CENTER);
    vg_text("SWITCH", sx + 56, y + h - 18, VG_SMALL, TE_MID, VG_CENTER);
}

/* MENU: its pages as tiles, each with its mark; the one ENC 1 / DATA picks framed */
static void kx_hub_icon(int i, int cx, int cy, uint32_t c)
{
    int k;
    switch (i) {
    case 0: vg_rect(cx - 10, cy - 8, 20, 16, c); vg_rect(cx - 6, cy - 8, 12, 5, 0x000000); break;      /* a disk */
    case 1: vg_rect(cx - 9, cy - 10, 14, 20, c); vg_tri((float)(cx + 5), (float)(cy - 10), (float)(cx + 9), (float)(cy - 6),
                                                        (float)(cx + 5), (float)(cy - 6), c); break;      /* a file */
    case 2: for (k = 0; k < 4; k++) vg_rect(cx - 11 + k * 6, cy - 6, 4, 12, k ? c : C_RED); break;     /* four parts */
    case 3: for (k = 0; k < 4; k++) vg_rect(cx - 10 + k * 6, cy - 2 - (k * 3) % 8, 3, 10, c); break;  /* faders */
    case 4: for (k = 0; k < 3; k++) vg_circle((float)(cx - 8 + k * 8), (float)cy, 3.0f, c); break;      /* jacks */
    case 5: vg_circle((float)cx, (float)cy, 9.0f, c); vg_circle((float)cx, (float)cy, 6.0f, 0x000000);
            for (k = 0; k < 5; k++) vg_rect(cx - 5 + k * 2 + (k > 2 ? 1 : 0), cy - 3 + (k == 2 ? 3 : 0), 1, 1, c); break;   /* DIN */
    case 6: vg_line((float)(cx - 10), (float)(cy + 6), (float)(cx + 10), (float)(cy - 4), 4.0f, c); break;   /* a treadle */
    case 7: vg_frame(cx - 11, cy - 8, 22, 16, 2, c); vg_line((float)(cx - 4), (float)(cy - 6), (float)(cx + 2), (float)(cy + 6), 1.5f, C_RED); break;
    case 8: for (k = 0; k < 4; k++) vg_rect(cx - 10 + (k & 1) * 11, cy - 9 + (k >> 1) * 10, 9, 8, k == 3 ? C_RED : c); break;   /* pads */
    default: vg_tri((float)(cx - 8), (float)(cy + 9), (float)(cx + 8), (float)(cy + 9), (float)cx, (float)(cy - 10), c);
             vg_line((float)cx, (float)(cy + 6), (float)(cx + 6), (float)(cy - 6), 2.0f, 0x000000); break;   /* a metronome */
    }
}
static void kx_draw_hub(int x, int y, int w, int h)
{
    int i, tw = (w - 4 * 6) / 5, th = (h - 6 - 16) / 2;   /* (the hint's line under the tiles) */
    for (i = 0; i < KX_HUB_N; i++) {
        int tx = x + (i % 5) * (tw + 6), ty = y + (i / 5) * (th + 6), on = i == kx_hub_sel;
        char b[4] = {(char)('0' + (i + 1) / 10), (char)('0' + (i + 1) % 10), 0};
        vg_rect(tx, ty, tw, th, on ? 0x2A2A30 : 0x16161A);
        vg_rect(tx, ty, tw, 2, on ? C_RED : 0x2A2A30);
        vg_text(b, tx + 4, ty + 4, VG_SMALL, 0x8A8A94, VG_LEFT);
        kx_hub_icon(i, tx + tw - 16, ty + 12, on ? TE_INK : 0x8A8A94);   /* (the icon over, the name under: apart) */
        vg_text(KX_HUB_NAME[i], tx + 5, ty + th - 16, VG_BOLD, on ? C_WHITE : TE_INK, VG_LEFT);
        if (on)
            vg_frame(tx - 1, ty - 1, tw + 2, th + 2, 1, C_RED);
    }
    vg_text("ENC 1 / DATA, ENTER OR A CLICK OPENS", x + w, y + h - 14, VG_SMALL, 0x8A8A94, VG_RIGHT);
}

/* PAD FX: pads 9-16 as they lie, and the same with SHIFT; each with the effect it holds, the chosen one red */
static void kx_draw_padfx(int x, int y, int w, int h)
{
    int half, k, sel = kx_pfx_sel;
    (void)w, (void)h;
    for (half = 0; half < 2; half++) {
        int hx = x + half * 236;
        vg_text(half ? "SHIFT + PADS 9-16" : "PADS 9-16", hx, y, VG_SMALL, half ? 0xA0A0FF : TE_MID, VG_LEFT);
        for (k = 0; k < 8; k++) {
            int slot = half * 8 + k, cx = hx + (k & 3) * 58, cy = y + 16 + (k >> 2) * 38, on = slot == sel;
            int fx = clamp(kx_ic[IC_PF0 + slot], 0, KX_NFX - 1);
            float age = (float)(vibe_frames() - kx_pfx_hit[slot]) / (kx_sr * 0.4f);
            char b[4];
            vg_rect(cx, cy, 56, 36, on ? vg_mix(C_BG, C_RED, 0.45f) : 0x16161A);
            vg_rect(cx, cy, 56, 2, on ? C_RED : half ? 0x34344A : 0x2A2A30);
            if (kx_pfx_hit[slot] && age < 1.0f)
                vg_rect_a(cx, cy, 56, 36, C_WHITE, 0.5f * (1.0f - age));
            vg_itoa(9 + k, b);
            vg_text(b, cx + 3, cy + 4, VG_SMALL, on ? C_WHITE : 0x7A7A84, VG_LEFT);
            vg_text(kx_fx_cell(fx), cx + 28, cy + 17, VG_BOLD, on ? C_WHITE : TE_INK, VG_CENTER);
            if (on)
                vg_frame(cx - 1, cy - 1, 58, 38, 1, C_RED);
        }
    }
    {
        int fx = clamp(kx_ic[IC_PF0 + sel], 0, KX_NFX - 1), tw;
        tw = vg_text(kx_fx_name(fx), x, y + 92, VG_BOLD, C_RED, VG_LEFT);
        vg_text(kx_fx_what(fx), x + tw + 10, y + 92, VG_SMALL, TE_INK, VG_LEFT);
        vg_text("HOLD THE PAD TO HEAR", x + w, y + 92, VG_SMALL, TE_MID, VG_RIGHT);
    }
}

/* MY PROJECTS: the name being given (its letter underlined), then 16 of the 32 entries; used ones white */
static void kx_draw_plib(int x, int y, int w, int h)
{
    int i, cw = w / 4, page = kxp_sel / 16, tw;
    char b[32];
    tw = vg_text(kxp_edit, x + 4, y, VG_BIG, TE_INK, VG_LEFT);
    {
        char pre[24];
        int px;
        memcpy(pre, kxp_edit, sizeof pre);
        pre[kxp_cur < 23 ? kxp_cur : 23] = 0;
        px = x + 4 + vg_text_width(pre, VG_BIG);
        vg_rect(px, y + 25, 11, 3, C_RED);
    }
    (void)tw;
    vg_text(kxp_x.mode == 1 ? "EXPORTING .." : kxp_x.mode == 2 ? "IMPORTING .." : "", x + w, y + 6, VG_SMALL, C_RED, VG_RIGHT);
    for (i = 0; i < 16; i++) {
        int e = page * 16 + i, cx = x + (i / 4) * cw, cy = y + 34 + (i % 4) * 19, sel = e == kxp_sel;
        b[0] = (char)('0' + (e + 1) / 10), b[1] = (char)('0' + (e + 1) % 10), b[2] = ' ', b[3] = 0;
        str_cpy(b + 3, kxp_name[e][0] ? kxp_name[e] : "--", sizeof b - 3);
        b[14] = 0;
        if (sel)
            vg_rect(cx, cy - 1, cw - 4, 18, vg_mix(C_BG, C_RED, 0.55f));
        vg_text(b, cx + 3, cy, VG_SMALL, sel ? C_WHITE : kxp_name[e][0] ? TE_INK : 0x6A6A72, VG_LEFT);
    }
    vg_text(page ? "17-32" : "1-16", x + w, y + h - 14, VG_SMALL, TE_MID, VG_RIGHT);
}

/* PROJECT FILES: the .kxp files in "VIBE FM/kriechstrom/" on the writing place */
static void kx_draw_pfiles(int x, int y, int w, int h)
{
    int i, first = kxp_lsel / 6 * 6;
    if (!kxp_src[0]) {
        vg_text("NO FOLDER YET", x + 4, y + 4, VG_BIG, TE_INK, VG_LEFT);
        vg_text("F5: PICK ONE (THE BROWSER), OR SET IT IN THE PLUGIN", x + 4, y + 40, VG_BOLD, TE_MID, VG_LEFT);
        vg_text("DROP A .KXP ON THE PLUGIN'S WINDOW TO IMPORT IT", x + 4, y + 62, VG_BOLD, TE_MID, VG_LEFT);
        return;
    }
    vg_text("VIBE FM / KRIECHSTROM", x + 4, y, VG_SMALL, TE_MID, VG_LEFT);
    if (!kxp_nlist)
        vg_text(kxp_list_want ? "LOOKING .." : "NO .KXP FILES THERE YET", x + 4, y + 24, VG_BOLD, TE_INK, VG_LEFT);
    for (i = first; i < kxp_nlist && i < first + 6; i++) {
        const char *nm = kxp_list[i] + str_len(kxp_list[i]);
        int cy = y + 18 + (i - first) * 15;
        while (nm > kxp_list[i] && nm[-1] != '/')
            nm--;
        if (i == kxp_lsel)
            vg_rect(x, cy - 1, w, 15, vg_mix(C_BG, C_RED, 0.55f));
        vg_text(nm, x + 4, cy, VG_SMALL, i == kxp_lsel ? C_WHITE : TE_INK, VG_LEFT);
    }
    vg_text(kxp_x.mode == 2 ? "IMPORTING .." : "ENTER IMPORTS INTO MY PROJECTS", x + w, y + h - 14, VG_SMALL, TE_MID, VG_RIGHT);
}

/* OUTPUTS: where the parts of the mix go when the host has more outputs (kx_outs.h), each pair with its level */
static void kx_draw_outs(int x, int y, int w, int h)
{
    static const char *const N[7] = {"MASTER", "T1", "T2", "T3", "T4", "EFFECTS", "INPUT"};
    int no = vibe_output_count(), k, on = kx_ic[IC_XOUT] != 0, rh = (h - 4) / 7;
    char b[24];
    for (k = 0; k < 7; k++) {
        int ry = y + k * rh, live = k == 0 || (on && 2 * k + 1 < no);
        float pk = k == 0 ? (kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1]) : kx_xo_peak[k - 1];
        uint32_t col = k >= 1 && k <= 4 ? (TRK_RGB[k - 1] == 0xD9D2C2 ? TE_INK : TRK_RGB[k - 1]) : k == 0 ? TE_INK : C_RED;
        vg_itoa(2 * k + 1, b);
        str_cpy(b + str_len(b), "/", 2);
        vg_itoa(2 * k + 2, b + str_len(b));
        vg_text(b, x + 4, ry, VG_BOLD, live ? TE_INK : 0x6A6A72, VG_LEFT);
        vg_text(N[k], x + 62, ry, VG_BOLD, live ? col : 0x6A6A72, VG_LEFT);
        if (live && k && 2 * k < no)
            vg_text(vibe_output_name(2 * k), x + 150, ry, VG_SMALL, TE_MID, VG_LEFT);
        if (no <= 2 && k)                         /* (the host has one pair: the bars' place says why) */
            continue;
        vg_rect(x + 260, ry + 5, w - 264, 6, 0x1E1E22);
        if (live)
            vg_rect(x + 260, ry + 5, (int)((float)(w - 264) * (pk > 1.0f ? 1.0f : pk)), 6, col);
    }
    if (no <= 2) {
        vg_text("THIS HOST HAS ONE PAIR:", x + 260, y + rh + 4, VG_SMALL, C_RED, VG_LEFT);
        vg_text("IN A DAW, GIVE THE PLUGIN", x + 260, y + rh * 2 + 4, VG_SMALL, TE_MID, VG_LEFT);
        vg_text("MORE OUTPUTS (UP TO 14)", x + 260, y + rh * 3 + 4, VG_SMALL, TE_MID, VG_LEFT);
    }
}

/* DISPLAY: the three ways the screen wears, side by side, the one set framed */
static void kx_draw_display(int x, int y, int w, int h)
{
    static const char *const N[3] = {"OFF", "LOW", "FULL"};
    int k, sw = (w - 32) / 3, sh = h - 30, cur = clamp(kx_ic[IC_SCREEN], 0, 2);
    for (k = 0; k < 3; k++) {
        int sx = x + 8 + k * (sw + 8), sy = y + 4, i, n = k == 0 ? 0 : k == 1 ? 5 : 16;
        vg_rect(sx, sy, sw, sh, 0x0A0A0B);
        vg_rect(sx + 4, sy + 4, sw / 3, 4, C_RED);                 /* (a page on it) */
        for (i = 0; i < 3; i++)
            vg_rect(sx + 4, sy + 14 + i * 10, sw - 8 - i * 20, 5, 0x3A3A40);
        for (i = 0; i < 4; i++)
            vg_rect(sx + 4 + i * (sw - 8) / 4, sy + sh - 22, (sw - 8) / 4 - 4, 16, 0x2A2A30);
        for (i = 0; i < n; i++) {                                 /* (its wear: cracks, torn bands) */
            uint32_t r = te_hash((uint32_t)(k * 97 + i * 13) + (uint32_t)(te_t() * (k == 2 ? 6.0f : 1.0f)));
            int cx = sx + 4 + (int)(r % (uint32_t)(sw - 8)), cy = sy + 4 + (int)((r >> 10) % (uint32_t)(sh - 8));
            if (k == 2 && (r >> 20) % 3u == 0u)
                vg_rect(sx, cy, sw, 2, vg_mix(C_RED, 0x000000, 0.3f));
            else
                vg_line((float)cx, (float)cy, (float)cx + (float)((r >> 22) % 17u) - 8.0f, (float)cy + 6.0f, 1.2f, C_RED);
        }
        vg_frame(sx - 2, sy - 2, sw + 4, sh + 4, 2, k == cur ? C_RED : 0x26262A);
        vg_text(N[k], sx + sw / 2, sy + sh + 6, VG_BOLD, k == cur ? TE_INK : 0x9A9AA4, VG_CENTER);
    }
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
    if (!usr_nz[k]) {                              /* empty: the bars it will take, a big REC on them */
        static const int BARS[4] = {4, 1, 2, 4};
        int nb = BARS[clamp(kx_ic[IC_SBARS], 0, 3)], i;
        te_dots(x, y, w, h - 20, 8);
        for (i = 0; i <= nb * 4; i++)
            vg_rect(x + 60 + i * (w - 70) / (nb * 4), y + (i % 4 ? 30 : 18), i % 4 ? 1 : 2, i % 4 ? 30 : 54, i % 4 ? TE_LOW : TE_MID);
        vg_circle((float)(x + 26), (float)(y + 44), 20.0f, 0x2A0806);
        vg_circle((float)(x + 26), (float)(y + 44), 14.0f, C_RED);
        {
            char m[64];
            str_cpy(m, IC_SSLOT_N[k], sizeof m);
            str_cpy(m + str_len(m), " IS EMPTY: F5 RECORDS ", sizeof m - str_len(m));
            str_cpy(m + str_len(m), kx_ic[IC_SSRC] ? "THE MIX" : "THE INPUT", sizeof m - str_len(m));
            vg_text(m, x + 60, y + 78, VG_BOLD, TE_INK, VG_LEFT);
        }
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
    vg_text("THE FOUR PARTS SONG CHAINS. WHOLE PROJECTS: MENU > PROJECTS", x + 4, y + 28,
            VG_SMALL, C_DIM, VG_LEFT);
    for (i = 0; i < 4u; i++) {
        int bx = x + (int)i * (w / 4), used = kx_slot_used[i];
        int pick = (uint32_t)kx_slot_pick == i;
        vg_rect(bx + 4, y + 52, w / 4 - 8, h - 52, pick ? vg_mix(C_BG, C_RED, 0.22f) : 0x111113);
        vg_rect(bx + 4, y + 52, w / 4 - 8, 2, pick ? C_RED : 0x2A2A2E);
        vg_text(SL[i], bx + 12, y + 58, VG_BIG, used ? C_WHITE : 0x70707A, VG_LEFT);
        vg_text(used ? kx_slot_name[i] : "EMPTY", bx + 12, y + 84, VG_SMALL, used ? C_TEXT : 0x8A8A94, VG_LEFT);
        (void)h;
    }
}

/* ---- the idle screen: after a minute and a half with nothing touched, the discharge and the master's scope, full */
static void kx_draw_idle(void)
{
    float heat = kx_peak[0] > kx_peak[1] ? kx_peak[0] : kx_peak[1];
    vg_clear(C_BG);
    kx_lt_idle = 1;                               /* (the lightning from every edge, on the hits; alone in silence) */
    kx_hits_frame();
    if (vibe_frames() - kx_lt_last > (uint64_t)(kx_sr * (song.playing ? 1.6f : 0.9f)))
        kx_lt_spawn(lt_u() < 0.5f ? 2 : 1, 0.35f + lt_u() * 0.4f);
    kx_lt_draw();
    {   /* the figure in the middle: grown anew each bar (each 2 s when stopped), flaring on the hits */
        static uint32_t bar = 0xFFFFFFFFu;
        uint32_t b = song.playing ? trk[song.sel].seq_idx / 16u : (uint32_t)(vibe_frames() / (uint64_t)(kx_sr * 2.0f));
        if (b != bar || !kx_big_n)
            bar = b, kx_big_grow();
        kx_big_draw(heat > 1.0f ? 1.0f : heat, kx_fx_hit > 1.0f ? 1.0f : kx_fx_hit);
    }
    kx_draw_scope(0, 200, 480, 100, C_WHITE, 1.0f);
    kx_logo(16, 286, VG_BOLD);
    kx_lt_idle = 0;
}

static int kx_streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static void kx_draw_help(void)
{
    /* HOME: the left column is this page (what encoders 1-4 turn here, DATA, ENTER, F5), the right one the controls
     * that are the same everywhere */
    static const char *const G[] = {
        "KEYS", "the selected track", "PADS 1-4", "mute (SHIFT: solo)", "PADS 5-8", "DICE (SHIFT: OOPS)",
        "PADS 9-16", "hold effects", "SHIFT + 9-16", "8 more (PAD FX)", "A / B", "level / DAMAGE",
        "SHIFT + B", "WALL", "F1-F4 / F6", "the track / UNDO", "MENU", "or SHIFT + HOME",
        "SHIFT", "hold, or tap once", "RETURN", "back to the last page",
    };
    static char lab[4][24];
    const char *r[44];
    int n = 0, pg = kx_page, kp = kx_knob_page(), k;
    const char *f5 = kx_f5_name(), *f5w = "", *dw = kx_data_on_cards() ? "the DATA card" :
                                                pg == PG_MIXER ? "the track's level" : "sound (SHIFT: engine)",
               *ew = kx_data_on_cards() ? "DATA to the next card" : "";
    for (k = 0; k < 4; k++) {
        kx_card c;
        static const char *const EN[4] = {"ENC 1", "ENC 2", "ENC 3", "ENC 4"};
        memset(&c, 0, sizeof c);
        if (kp < 0 && !IS_PED_PAGE(pg) && !IS_IC_PAGE(pg) && pg != PG_USER && pg != PG_PLIB && pg != PG_PFILES)
            break;
        kx_card_of(kp >= 0 ? kp : pg, (uint32_t)k, &c);
        if (!c.label || !c.label[0])
            continue;
        str_cpy(lab[k], c.label, sizeof lab[k]);
        if (IS_PED_PAGE(pg) && k == 0)
            str_cpy(lab[k], "PEDAL (SHIFT: MIX)", sizeof lab[k]);
        r[n++] = EN[k], r[n++] = lab[k];
    }
    if (IS_PED_PAGE(pg) && n == 2)
        r[n++] = "ENC 2-4", r[n++] = "the pedal's knobs";
    if (pg == PG_PROJECT)
        r[n++] = "ENC 1", r[n++] = "the slot, A-D";
    if (pg == PG_MIXER)
        r[n++] = "ENC 1-4", r[n++] = "T1-T4 level";
    if (pg == PG_MIXER)
        r[n++] = "SHIFT + ENC", r[n++] = "T1-T4 pan";
    if (pg == PG_GRID)
        dw = "the step", ew = "the step on / off";
    else if (pg >= 0 && pg < (int)NPAGES && PAGES[pg].scope == SC_STEP)
        dw = "the step";
    else if (IS_PED_PAGE(pg))
        dw = "the pedal (or tap it)";
    else if (pg == PG_PROJECT)
        ew = "load (SHIFT: demo)", dw = "the part";
    else if (pg >= 0 && pg < (int)NPAGES && (PAGES[pg].graph == GR_SONG || PAGES[pg].graph == GR_MOD ||
                                             fop_page(PAGES[pg].graph)))
        dw = "as ENC 1";
    else if (pg == PG_USER)
        ew = "load the sound", dw = "the slot";
    else if (pg == PG_PLIB)
        ew = "2x: load (SHIFT: del)", dw = "the project";
    else if (pg == PG_PFILES)
        ew = "import the file", dw = "the file";
    else if (pg == PG_HUB)
        ew = "open the tile's page", dw = "the tile";
    else if (pg == PG_PADFX)
        ew = "the next pad", dw = "the pad's effect";
    else if (pg >= 0 && pg < (int)NPAGES && PAGES[pg].graph == GR_EVENTS)
        ew = "the row's action", dw = "as ENC 1";
    if (kx_streq(f5, "OOPS")) f5w = "OOPS: random values";
    else if (kx_streq(f5, "DICE") || kx_streq(f5, "MOOD")) f5w = "DICE: a new part";
    else if (kx_streq(f5, "REC") || kx_streq(f5, "STOP")) f5w = pg == PG_SAMPLER ? "record at next bar" : "the song";
    else if (kx_streq(f5, "PLAY")) f5w = "play the song";
    else if (kx_streq(f5, "NEXT")) f5w = "the next pedal";
    else if (pg == PG_PLIB)
        f5w = "save (SHIFT: export)";
    else if (pg == PG_PFILES)
        f5w = "look again / pick a folder";
    else if (kx_streq(f5, "SAVE") || kx_streq(f5, "NEW") || kx_streq(f5, "ERASE"))
        f5w = pg == PG_PROJECT ? "save (SHIFT: new)" : "save (SHIFT: erase)";
    else if (pg == PG_PADFX) f5w = "the pad's own again";
    else if (f5[0]) f5w = "the input's mode";
    {   /* how this page's effect goes on and off */
        static const char *const OO[][2] = {
            {"MASTER", "each at 0 is off"}, {"PEDALS", "TYPE OFF, or MIX 0"}, {"INPUT PEDALS", "TYPE OFF, or MIX 0"},
            {"MASTER PEDALS", "TYPE OFF, or MIX 0"}, {"SENDS", "a send at 0: none"}, {"SLICER", "ENC 1: OFF GATE STUT"},
            {"DELAY", "MIX 0 is off"}, {"REVERB", "the tracks' REV sends"}, {"CHORUS", "the tracks' CHO sends"},
            {"DAMAGE", "each at 0 is off"}, {"RESONATE", "AMT 0 is off"}, {"SPLICE", "AMT 0 is off"},
            {"HAUNT", "MODE OFF / SUCK / DRAG"}, {"INPUT", "LEVEL 0 is off"}, {"ARP", "MODE OFF is off"},
        };
        uint32_t q;
        for (q = 0; q < sizeof OO / sizeof OO[0]; q++)
            if (kx_streq(kx_title(pg), OO[q][0])) {
                r[n++] = "ON / OFF", r[n++] = OO[q][1];
                break;
            }
        if (pg == PG_MASTER)
            r[n++] = "CLICK A TILE", r[n++] = "that effect's page";
    }
    r[n++] = "DATA", r[n++] = dw;
    if (ew[0])
        r[n++] = "ENTER/DATA", r[n++] = ew;
    if (f5w[0])
        r[n++] = "F5", r[n++] = f5w;
    r[n++] = "< >", r[n++] = "this family's pages";
    while (n < 22)                                /* (the page's column, then the right one) */
        r[n++] = "", r[n++] = "";
    for (k = 0; k < (int)(sizeof G / sizeof G[0]) && n < 44; k++)
        r[n++] = G[k] && kx_streq(G[k], "DICE (SHIFT: OOPS)") && pg == PG_MASTER ? "DMG WALL SPLC HAUNT" :
                 G[k] && kx_streq(G[k], "DICE (SHIFT: OOPS)") && IS_PED_PAGE(pg) ? "pedal 1-3 off, 4 all" : G[k];
    vg_controls(kx_title(pg), r, n / 2);
}

/* SHIFT held or tapped: what the pads and the other controls do with it */
static void kx_draw_shift(void)
{
    /* SHIFT held or tapped: the pads' second layer, each pad named where it is */
    static const char *const L[16] = {"SOLO 1", "SOLO 2", "SOLO 3", "SOLO 4", "OOPS 1", "OOPS 2", "OOPS 3", "OOPS 4",
                                      "", "", "", "", "", "", "", ""};
    uint32_t r, k;
    int x = 236, y = 92, cw = 58, ch = 23;
    vg_rect_a(x - 8, y - 4, 248, 120, 0x000000, 0.88f);
    vg_frame(x - 8, y - 4, 248, 120, 1, C_RED);
    vg_text(kx_shift_latch ? "SHIFT: THE PADS (FOR ONE)" : "SHIFT: THE PADS", x, y, VG_BOLD, C_RED, VG_LEFT);
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 4u; k++) {
            int px = x + (int)k * cw, py = y + 20 + (int)r * ch;
            uint32_t col = r == 0u ? TRK_RGB[k] : r == 1u ? TRK_RGB[k] : r == 2u ? 0xFF8030 : 0xA0A0FF;
            const char *t = L[r * 4u + k];
            vg_round(px, py, cw - 4, ch - 3, 3, vg_mix(C_BG, col, 0.35f));
            if (r >= 2u)                          /* (the effects MENU > PAD FX gives them, SHIFT's eight) */
                t = kx_fx_cell(clamp(kx_ic[IC_PF0 + 8 + (r - 2u) * 4u + k], 0, KX_NFX - 1));
            if (r == 1u && kx_page == PG_MASTER) {
                static const char *const M[4] = {"DAMAGE", "WALL", "SPLICE", "HAUNT"};
                t = M[k];
            } else if (r == 1u && IS_PED_PAGE(kx_page)) {
                static const char *const P[4] = {"PEDAL 1", "PEDAL 2", "PEDAL 3", "ALL"};
                t = P[k];
            }
            vg_text(t, px + (cw - 4) / 2, py + 2, VG_SMALL, C_WHITE, VG_CENTER);
        }
}

/* the start: where you left it, the demo song, or nothing */
static void kx_draw_start(void)
{
    static const char *const T[4] = {"WHERE YOU LEFT IT", "THE DEMO SONG", "AN EMPTY PROJECT", "ONE OF MY PROJECTS"};
    static const char *const W[4] = {"the music as it was", "E minor, 100 BPM: four parts and a SONG", "four tracks, no steps",
                                     "PROJECTS: pick one, ENTER twice loads it"};
    uint32_t i;
    vg_rect_a(0, 30, 480, 256, 0x000000, 0.88f);
    vg_text("START WITH", 24, 52, VG_BOLD, C_RED, VG_LEFT);
    for (i = 0; i < 4u; i++) {
        int y = 76 + (int)i * 44, on = (int)i == kx_start_sel, off = i == 0u && !kx_had_session;
        const char *f = KX_START_KEY[i];
        vg_rect(24, y, 432, 40, on ? vg_mix(C_BG, C_RED, 0.3f) : 0x141416);
        vg_rect(24, y, 4, 40, on ? C_RED : 0x2A2A2E);
        vg_text(f, 34, y + 11, VG_BOLD, off ? 0x70707A : C_RED, VG_LEFT);
        vg_text(T[i], 92, y + 2, VG_MED, off ? 0x70707A : C_WHITE, VG_LEFT);
        vg_text(off ? "nothing kept yet" : W[i], 92, y + 21, VG_SMALL, off ? 0x70707A : 0xB4B4BC, VG_LEFT);
    }
    vg_text("DATA + ENTER, A CLICK, OR THE KEY BELOW.  KEYS AND PADS PLAY AT ONCE", 240, 256, VG_SMALL, 0x8A8A94, VG_CENTER);
}

static void kx_draw_toast(void)
{
    uint32_t s1 = __atomic_load_n(&kx_msg_seq, __ATOMIC_ACQUIRE);
    static uint32_t pn;
    if (kxp_msg_n != pn) {                        /* (MY PROJECTS' own words: kx_proj.h) */
        pn = kxp_msg_n;
        memcpy(kx_toast, kxp_msg, sizeof kx_toast);
        kx_toast[23] = 0;
        kx_toast_until = kx_now_ms() + 1800u;
    }
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
    if (kx_page == PG_GRID && kx_dev.pads >= NSTEP) {   /* a Launchpad 8 x 8: all 64 steps, the playhead */
        const track_t *t = TSEL;
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), play = t->seq_idx % len;
        for (i = 0; i < NSTEP; i++) {
            const step_t *st = &t->step[i];
            int on = drum_track(t) ? (int)((step_lanes(st) >> ui.lane) & 1u) : step_on(st);
            if (song.playing && i == play)
                vibe_led_pad((int)i, C_WHITE, 1.0f);
            else
                vibe_led_pad((int)i, TRK_RGB[song.sel], i >= len ? 0.0f : on ? 0.9f : 0.06f);
        }
        return;
    }
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
        float a = kx_trk_pk[i];
        vibe_led_pad((int)i, TRK_RGB[i], t->p[P_MUTE] ? 0.05f : 0.35f + 0.65f * a);
        if (kx_page == PG_MASTER) {               /* DAMAGE WALL SPLICE HAUNT: lit while on */
            static const uint8_t G[4] = {G_DMG, G_WALL, G_SPLICE, G_HAUNT};
            vibe_led_pad(4 + (int)i, C_RED, song.g[G[i]] ? 1.0f : 0.06f);
        } else if (IS_PED_PAGE(kx_page)) {        /* the pedals: their colour, lit while they work */
            uint32_t o = kx_ped_owner(kx_page);
            int16_t v[PD_N] = {0};
            if (i < (uint32_t)kx_ped_slots(o))
                kx_ped_get(o, i, v);
            vibe_led_pad(4 + (int)i, v[0] ? PED_RGB[clamp(v[0], 0, IT_N - 1)] : C_WHITE,
                         i == 3u ? 0.3f : v[0] ? (v[PD_MIX] ? 1.0f : 0.08f) : 0.0f);
        } else
            vibe_led_pad(4 + (int)i, kx_shift ? C_WHITE : TRK_RGB[i], 0.25f);
        {   /* pads 9-16: the effects, flaring when pressed; on PAD FX the chosen one full */
            uint32_t r;
            for (r = 0; r < 2u; r++) {
                int slot = (int)(r * 4u + i) + (kx_shift ? 8 : 0);
                float age = (float)(vibe_frames() - kx_pfx_hit[slot]) / (kx_sr * 0.4f);
                float lv = kx_page == PG_PADFX && slot == kx_pfx_sel ? 1.0f : 0.2f;
                if (kx_pfx_hit[slot] && age < 1.0f && lv < 1.0f)
                    lv += 0.8f * (1.0f - age);
                vibe_led_pad(8 + (int)(r * 4u + i), kx_shift ? 0xA0A0FF : r ? 0xFF8030 : C_RED, lv);
            }
        }
    }
}

static void kx_labels(void)
{
    static const char *const K[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
    uint32_t i;
    for (i = 0; i < 6u; i++)
        vibe_label(VB_K1 + (int)i, K[i]);
    for (i = 0; i < 4u; i++)
        vibe_label(VB_F1 + (int)i, kx_start ? KX_START_KEY[i] : (const char *const[4]){"T1", "T2", "T3", "T4"}[i]);
    vibe_label(VB_F5, kx_start ? "" : kx_f5_name());
    vibe_label(VB_F6, kx_start ? "" : "UNDO");
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
        vg_text("BUILT ON FELUCCA", 240, 182, VG_SMALL, vg_mix(C_BG, 0x9A9AA4, a), VG_CENTER);
    }
    if ((t > 0.33f && t < 0.42f) || (t > 0.62f && t < 0.66f) || (t > 0.9f && t < 0.92f))
        te_tear(kx_fb, 0.8f, (uint32_t)(t * 60.0f));   /* (the logo cut in, as a tape spliced) */
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
    if (pg == PG_GRID || (pg >= 0 && pg < (int)NPAGES && PAGES[pg].scope == SC_STEP)) {
        kx_draw_grid(8, 96, 466, 110);            /* (DETAIL too: the steps, the cursor's own values below) */
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
    } else if (pg == PG_HUB) {
        kx_draw_hub(4, 96, 472, 98);
    } else if (pg == PG_PADFX) {
        kx_draw_padfx(4, 96, 472, 110);
    } else if (pg == PG_PLIB) {
        kx_draw_plib(4, 96, 472, 110);
    } else if (pg == PG_PFILES) {
        kxp_list_poll();
        kx_draw_pfiles(4, 96, 472, 110);
    } else if (pg == PG_OUTS) {
        kx_draw_outs(4, 94, 472, 114);
    } else if (pg == PG_DISPLAY) {
        kx_draw_display(4, 96, 472, 110);
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
            else if (!te_page(pg, 4, 96, 472, 110))   /* (kx_te.h: the page's own picture, else the sound) */
                kx_draw_scope(4, 98, 472, 106, p->scope == SC_GLOBAL ? C_RED : col, 1.2f);
            break;
        }
    }
    te_marks(2, 93, 476, pg == PG_MIXER ? 190 : 115);  /* (the picture's crop marks) */
    if (pg != PG_MIXER)
        kx_draw_cards(kp, 210);
    {
        static int last_order;
        int order = (int)kx_fam * 64 + (int)kx_fpage[kx_fam];
        kx_transition(kx_fb, pg, order > last_order ? 1 : -1);
        last_order = order;
    }
    {   /* the cut on a page change: a few frames torn, then clean */
        static int was = -2;
        static uint64_t cut;
        if (pg != was) {
            if (was != -2 && kx_ic[IC_SCREEN])
                cut = kx_idle_t ? kx_idle_t : 1;   /* (from the press that changed it: the last event's time) */
            was = pg;
        }
        if (cut) {
            float u = (float)(vibe_frames() - cut) / (kx_sr * 0.16f);
            if (u >= 1.0f)
                cut = 0;
            else
                te_tear(kx_fb, (1.0f - u) * (kx_ic[IC_SCREEN] == 2 ? 1.0f : 0.6f), (uint32_t)(cut >> 8) + (uint32_t)(u * 4.0f));
        }
    }
    kx_draw_soft();
    {   /* VIBE FM Link (SDK 24): a controller with a screen (an FM-1 under VIBE FM Link) shows the page and encoders 1-4 */
        uint32_t k;
        vibe_link_page(kx_title(pg));
        for (k = 0; k < 4u; k++) {
            kx_card c;
            kx_card_of(kp, k, &c);
            vibe_link_encoder(VE_1 + (int)k, c.on ? c.label : "", c.on ? c.val : "");
        }
    }
    kx_fx_badge();                                /* (a held effect's picture, in the corner) */
    kx_wear_fx(kx_fb, 480, 320);
    if (kx_shift_latch && kx_latch_enc >= 0 && vibe_frames() - kx_latch_t > (uint64_t)(kx_sr * 0.6f))
        kx_shift = kx_shift_latch = 0, kx_shift_used = 0, kx_latch_enc = -1;   /* (a tapped SHIFT's turn is over) */
    if (kx_shift && !kx_start && !kx_shift_latch && !kx_shift_used &&   /* (the pads' map: SHIFT held still 0.7 s) */
        vibe_frames() - kx_shift_t > (uint64_t)(kx_sr * 0.7f))
        kx_draw_shift();
    if (kx_start)
        kx_draw_start();
    kx_draw_toast();
}
