/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the controls and the pages (the control flow).
 *
 * The house layout: the six keys right of ENTER are SOUND ENV LFO ARP FX SEQ (a tap again: the family's next page),
 * encoders 1-4 the four values above F1-F6, F1-F4 the four tracks, F5 the page's action, F6 UNDO. DATA steps through
 * the sounds (on SEQ: the cursor), A is the track's LEVEL, B the master's DAMAGE (SHIFT + B: WALL). MENU: the mixer,
 * the project, the clock. HOME: what every control does; RETURN: back to SOUND.
 * The pages are KRIECHSTROM's own (params.c PAGES): their knobs do what the FM-1's do (kx_ctl.h KXC_EDIT). */

/* ---- looks ---------------------------------------------------------------------------------------------------- */
#define C_BG 0x09090A
#define C_PANEL 0x141416
#define C_LINE 0x26262A
#define C_DIM 0x6A6A70
#define C_TEXT 0xD8D8DC
#define C_WHITE 0xF4F4F4
#define C_RED 0xE01818
#define C_RUST 0x8A1A10
static const uint32_t TRK_RGB[NTRK] = {0xE0301C, 0xE8A23A, 0xD9D2C2, 0x7896B0};   /* rust red, sulfur, bone, steel */

/* ---- families and pages ---------------------------------------------------------------------------------------- */
enum { FA_SOUND, FA_ENV, FA_LFO, FA_ARP, FA_FX, FA_SEQ, FA_MENU, FA_N };
static const char *const FA_NAME[FA_N] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ", "MENU"};
/* pages of our own, past PAGES' indices */
enum { PG_BROWSE = 200, PG_MASTER, PG_GRID, PG_MIXER, PG_PROJECT, PG_INPUT, PG_INPUT2 };
#define PG_MAX 24
static uint8_t kx_pl[PG_MAX], kx_pn;              /* the open family's pages, in order */
static uint8_t kx_fam, kx_fpage[FA_N];            /* the family open, each family's page */
static int kx_page = PG_BROWSE;                   /* the page shown */
static int kx_help, kx_shift, kx_in_asked;
static float kx_enc_acc[VE_COUNT];
static uint32_t kx_toast_seq;
static char kx_toast[24];
static uint64_t kx_toast_until;
static int kx_view_bank = -1;                     /* SEQ: the 16 steps the pads and the cursor show (-1: follow) */
static uint64_t kx_view_t;

static int kx_page_index(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, title))
            return (int)i;
    return -1;
}

static int kx_page_ok(uint32_t i)                 /* a PAGES page this port shows */
{
    uint32_t g = PAGES[i].graph;
    if (!page_visible(i))
        return 0;
    return g != GR_FMOP && g != GR_FMOP2 && g != GR_FMENV && g != GR_SLICES && g != GR_EVENTS && g != GR_USER &&
           g != GR_PATS && g != GR_SLOTS && g != GR_TOOLS && g != GR_BROWSE && g != GR_SONG && g != GR_TRK &&
           PAGES[i].scope != SC_STEP;
}

static void kx_family_pages(uint32_t fa)
{
    uint32_t i;
    kx_pn = 0;
#define ADD(x) do { if (kx_pn < PG_MAX) kx_pl[kx_pn++] = (uint8_t)(x); } while (0)
    switch (fa) {
    case FA_SOUND:
        ADD(PG_BROWSE);
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_EDIT && kx_page_ok(i))
                ADD(i);
        break;
    case FA_ENV:
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_ENV && kx_page_ok(i))
                ADD(i);
        break;
    case FA_LFO:
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_LFO && kx_page_ok(i))
                ADD(i);
        break;
    case FA_ARP:
        for (i = 0; i < NPAGES; i++)
            if ((PAGES[i].fam == FAM_ARP || PAGES[i].fam == FAM_SCL) && kx_page_ok(i))
                ADD(i);
        break;
    case FA_FX:
        ADD(PG_MASTER);
        ADD(PG_INPUT);
        ADD(PG_INPUT2);
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_FX && kx_page_ok(i))
                ADD(i);
        break;
    case FA_SEQ:
        ADD(PG_GRID);
        i = (uint32_t)kx_page_index("DETAIL");
        ADD(i);
        break;
    default:
        ADD(PG_MIXER);
        ADD(PG_PROJECT);
        i = (uint32_t)kx_page_index("CLOCK");
        ADD(i);
        break;
    }
#undef ADD
}

static void kx_open(uint32_t fa, int next)
{
    if (fa == kx_fam && next)
        kx_fpage[fa] = (uint8_t)(kx_fpage[fa] + 1u);
    kx_fam = (uint8_t)fa;
    kx_family_pages(fa);
    if (kx_fpage[fa] >= kx_pn)
        kx_fpage[fa] = 0;
    kx_page = kx_pl[kx_fpage[fa]];
}

static void kx_page_step(int d)
{
    kx_family_pages(kx_fam);
    kx_fpage[kx_fam] = (uint8_t)((kx_fpage[kx_fam] + kx_pn + (uint32_t)(d > 0 ? 1 : kx_pn - 1)) % kx_pn);
    kx_page = kx_pl[kx_fpage[kx_fam]];
}

/* the page's title and the line under it (params.c page_name / page_what) */
static const char *kx_title(int pg)
{
    switch (pg) {
    case PG_BROWSE: return "SOUND";
    case PG_MASTER: return "MASTER";
    case PG_GRID: return "STEPS";
    case PG_MIXER: return "MIXER";
    case PG_PROJECT: return "PROJECT";
    case PG_INPUT: return "INPUT";
    case PG_INPUT2: return "INPUT SENDS";
    }
    if (pg >= 0 && pg < (int)NPAGES) {
        const page_t *p = &PAGES[pg];
        if (p->fam == FAM_EDIT && p->scope == SC_ENGINE) {
            const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
            return e->page_title[p->id[0] == P_E0 ? 0 : 1] ? e->page_title[p->id[0] == P_E0 ? 0 : 1] : p->title;
        }
        return page_name(p->title);
    }
    return "";
}

/* ---- what the encoders edit on the page shown ------------------------------------------------------------------- */
/* the page whose knobs the encoders turn (our pages lend one of KRIECHSTROM's), -1 none */
static int kx_knob_page(void)
{
    if (kx_page == PG_INPUT || kx_page == PG_INPUT2)
        return kx_page;
    if (kx_page >= 0 && kx_page < (int)NPAGES)
        return kx_page;
    if (kx_page == PG_BROWSE)
        return kx_page_index("EDIT 1");
    if (kx_page == PG_MASTER)
        return kx_page_index("DAMAGE");
    if (kx_page == PG_GRID)
        return kx_page_index("STEP");
    return -1;
}

typedef struct { const char *label; char val[12]; const char *unit; float pos; int bip, on; } kx_card;
static const uint8_t IN_SLOTS[2][4] = {{IC_LEVEL, IC_DRIVE, IC_GATE, IC_FEED}, {IC_PAN, IC_CHO, IC_DLY, IC_REV}};
static void kx_card_of(int pg, uint32_t slot, kx_card *c)
{
    if (pg == PG_INPUT || pg == PG_INPUT2) {      /* the audio input's (kx_input.h) */
        uint32_t f = IN_SLOTS[pg == PG_INPUT2][slot & 3u];
        int v = kx_ic[f];
        memset(c, 0, sizeof *c);
        c->on = 1;
        c->label = IC_DESC[f].label;
        if (f == IC_GATE || f == IC_FEED)
            str_cpy(c->val, IC_TRK[clamp(v, 0, NTRK)], sizeof c->val);
        else
            vg_itoa(v, c->val);
        c->unit = "";
        c->bip = IC_DESC[f].min < 0;
        c->pos = (float)(v - IC_DESC[f].min) / (float)(IC_DESC[f].max - IC_DESC[f].min);
        return;
    }
    const page_t *p;
    const param_desc_t *d;
    int16_t *vp;
    const char *u = "";
    memset(c, 0, sizeof *c);
    c->label = "";
    if (pg < 0 || pg >= (int)NPAGES)
        return;
    p = &PAGES[pg];
    if (p->scope == SC_STEP) {                    /* STEP: the cursor's step (ui_input.c step_edit) */
        const track_t *t = TSEL;
        const step_t *st = &t->step[ui.cursor % NSTEP];
        static const char *const L[4] = {"STEP", "NOTE", "TIME", "FLAG"}, *const LD[4] = {"STEP", "LANE", "HIT", "ACC"};
        static const char *const TM[3] = {"NOTE", "TIE", "REST"}, *const FL[4] = {"-", "ACC", "SLD", "A+S"};
        int drum = drum_track(t);
        c->on = 1;
        c->label = drum ? LD[slot] : L[slot];
        switch (slot) {
        case 0:
            vg_itoa(ui.cursor + 1, c->val);
            c->pos = (float)ui.cursor / (float)(t->p[P_SLEN] > 1 ? t->p[P_SLEN] - 1 : 1);
            break;
        case 1:
            if (drum)
                str_cpy(c->val, drum_lane_name(t, ui.lane), sizeof c->val);
            else if (step_on(st))
                note_name(c->val, st->note[0]);
            else
                str_cpy(c->val, "-", sizeof c->val);
            c->pos = drum ? (float)ui.lane / 7.0f : step_on(st) ? (float)st->note[0] / 127.0f : 0.0f;
            break;
        case 2:
            if (drum)
                str_cpy(c->val, (step_lanes(st) >> ui.lane) & 1u ? "ON" : "OFF", sizeof c->val);
            else
                str_cpy(c->val, TM[st->time % 3u], sizeof c->val);
            break;
        default:
            if (drum)
                str_cpy(c->val, (step_accents(st) >> ui.lane) & 1u ? "ACC" : "-", sizeof c->val);
            else
                str_cpy(c->val, FL[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], sizeof c->val);
            break;
        }
        return;
    }
    d = page_desc(p, slot, &vp);
    if (!d || !vp || !d->label)
        return;
    c->on = 1;
    c->label = d->label;
    param_format(d, *vp, c->val, &u);
    c->unit = u;
    c->bip = d->min < 0 && d->max > 0;
    c->pos = d->max > d->min ? (float)(*vp - d->min) / (float)(d->max - d->min) : 0.0f;
}

/* ---- the controls ----------------------------------------------------------------------------------------------- */
static const uint8_t PAD_FX[4] = {PF_R16, PF_TAPE, PF_FRZ, PF_GATE};
static const char *const PAD_FX_NAME[4] = {"1/16", "STOP", "FREEZE", "GATE"};
static uint8_t kx_pad_note[16];                   /* a drum pad held: the note it started (+ 1), its track in the high bits */

static int kx_drum_track(void)                    /* the track the drum pads play: the selected one if it is DRUM */
{
    uint32_t i;
    if (drum_track(TSEL))
        return song.sel;
    for (i = 0; i < NTRK; i++)
        if (drum_track(&trk[i]))
            return (int)i;
    return -1;
}

static uint32_t kx_bank(void)                     /* SEQ: the 16 steps shown */
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    if (kx_view_bank >= 0 && (uint32_t)kx_view_bank * 16u < len)
        return (uint32_t)kx_view_bank;
    return song.playing ? (t->seq_idx % len) / 16u : ui.cursor / 16u;
}

static void kx_pad(uint32_t id, int down, float vel)
{
    uint32_t row = id / 4u, col = id % 4u;
    if (kx_page == PG_GRID) {                     /* the SEQ page: the pads are the 16 steps shown */
        if (down) {
            uint32_t s = kx_bank() * 16u + id;
            kx_post(KXC_STEP, song.sel, s, drum_track(TSEL) ? ui.lane : 255u, 0);
            kx_post(KXC_CURSOR, 0, 0, 0, (int32_t)s);
            kx_view_bank = (int)(s / 16u);
            kx_view_t = vibe_frames();
        }
        return;
    }
    switch (row) {
    case 0:                                       /* the tracks: mute, SHIFT: select */
        if (down)
            kx_post(kx_shift ? KXC_SEL : KXC_MUTE, col, 0, 0, 0);
        break;
    case 1:                                       /* DICE on that track (SHIFT: the next MOOD) */
        if (down)
            kx_post(KXC_TOOL, col, kx_shift ? TL_MOOD : TL_DICE, 0, 0);
        break;
    case 2:                                       /* the FX layer's effects, while held */
        kx_post(KXC_PERF, PAD_FX[col], down ? 1u : 0u, 0, 0);
        break;
    default: {                                    /* the drum lanes KICK SNARE CLAP HAT, with velocity */
        int dt = kx_drum_track();
        if (dt < 0)
            break;
        if (down) {
            uint32_t v = (uint32_t)(vel * 127.0f + 0.5f);
            kx_pad_note[id] = (uint8_t)(DRUM_LANE_NOTE[col == 3u ? 3u : col] + 1u);
            kx_post(KXC_HIT, (uint32_t)dt, kx_pad_note[id] - 1u, 0, (int32_t)(v ? v : 1u));
            kx_pad_note[id] |= 0;
        } else if (kx_pad_note[id]) {
            kx_post(KXC_HIT, (uint32_t)dt, kx_pad_note[id] - 1u, 0, 0);
            kx_pad_note[id] = 0;
        }
        break;
    }
    }
}

static void kx_soft(uint32_t f)                   /* F1-F6 */
{
    if (f < 4u) {
        kx_post(KXC_SEL, f, 0, 0, 0);
        return;
    }
    if (f == 5u) {
        kx_post(KXC_UNDO, 0, 0, 0, 0);
        return;
    }
    switch (kx_fam) {                             /* F5: the page's action */
    case FA_SOUND: kx_post(KXC_OOPS, song.sel, 0, 0, 0); break;
    case FA_SEQ: kx_post(KXC_TOOL, song.sel, kx_shift ? TL_MOOD : TL_DICE, 0, 0); break;
    case FA_FX:
        if (kx_page == PG_INPUT || kx_page == PG_INPUT2) {
            int m = (kx_ic[IC_MODE] + 1) % 3;
            kx_post(KXC_INCFG, IC_MODE, 1, 0, m);
            vibe_input_mode(0, m);
            vibe_input_mode(1, m);
        }
        break;
    default: break;
    }
}
static const char *kx_f5_name(void)
{
    switch (kx_fam) {
    case FA_SOUND: return "OOPS";
    case FA_SEQ: return kx_shift ? "MOOD" : "DICE";
    case FA_FX: return kx_page == PG_INPUT || kx_page == PG_INPUT2 ? IC_MODE_N[kx_ic[IC_MODE] % 3] : "";
    default: return "";
    }
}

static void kx_turn(uint32_t enc, float detents)
{
    int32_t n;
    kx_enc_acc[enc] += detents;
    n = (int32_t)kx_enc_acc[enc];
    if (!n)
        return;
    kx_enc_acc[enc] -= (float)n;
    if (enc <= VE_4) {
        int pg = kx_knob_page();
        if (kx_page == PG_MIXER) {                /* the four tracks' LEVEL (SHIFT: PAN) */
            uint32_t id = kx_shift ? P_PAN : P_LEVEL;
            const param_desc_t *d = &TP[id];
            kx_post(KXC_PARAM, enc, id & 0xFFu, id >> 8, clamp(trk[enc].p[id] + n, d->min, d->max));
        } else if (kx_page == PG_INPUT || kx_page == PG_INPUT2) {
            uint32_t f = IN_SLOTS[kx_page == PG_INPUT2][enc];
            kx_post(KXC_INCFG, f, 0, 0, (f == IC_GATE || f == IC_FEED) ? (n > 0 ? 1 : -1) : n);
        } else if (pg >= 0)
            kx_post(KXC_EDIT, (uint32_t)pg, enc, 0, n);
        if (kx_page == PG_GRID && enc == VE_1) {
            kx_view_bank = -1;
        }
        return;
    }
    if (enc == VE_A) {                            /* the selected track's LEVEL */
        const param_desc_t *d = &TP[P_LEVEL];
        kx_post(KXC_PARAM, song.sel, P_LEVEL, 0, clamp(TSEL->p[P_LEVEL] + n * (kx_shift ? 1 : 3), d->min, d->max));
    } else if (enc == VE_B) {                     /* DAMAGE, SHIFT: WALL */
        uint32_t g = kx_shift ? G_WALL : G_DMG;
        kx_post(KXC_PARAM, NTRK, g, 0, clamp(song.g[g] + n * 2, GP[g].min, GP[g].max));
    } else if (enc == VE_DATA) {
        if (kx_page == PG_GRID) {
            int32_t len = clamp(TSEL->p[P_SLEN], 1, NSTEP), c = ((int32_t)ui.cursor + n % len + len) % len;
            kx_post(KXC_CURSOR, 0, 0, 0, c);
            kx_view_bank = c / 16;
            kx_view_t = vibe_frames();
        } else if (kx_shift)
            kx_post(KXC_ENGINE, 0, 0, 0, n > 0 ? 1 : -1);
        else
            kx_post(KXC_PRESET, 0, 0, 0, n);
    }
}

static void kx_button(uint32_t b, int down)
{
    if (b == VB_SHIFT) {
        kx_shift = down;
        return;
    }
    if (!down)
        return;
    if (kx_help && b != VB_HOME && b != VB_RETURN)
        kx_help = 0;
    switch (b) {
    case VB_HOME: kx_help = !kx_help; break;
    case VB_RETURN:
        if (kx_help)
            kx_help = 0;
        else
            kx_open(FA_SOUND, 0), kx_fpage[FA_SOUND] = 0, kx_page = PG_BROWSE;
        break;
    case VB_K1: case VB_K2: case VB_K3: case VB_K4: case VB_K5: case VB_K6:
        kx_open(b - VB_K1, 1);
        break;
    case VB_MENU: kx_open(FA_MENU, 1); break;
    case VB_LEFT: kx_page_step(-1); break;
    case VB_RIGHT: kx_page_step(1); break;
    case VB_F1: case VB_F2: case VB_F3: case VB_F4: case VB_F5: case VB_F6:
        kx_soft(b - VB_F1);
        break;
    case VB_PLAY: kx_post(KXC_PLAY, 3, 0, 0, 0); break;
    case VB_STOP: kx_post(KXC_PLAY, 2, 0, 0, 0); if (!song.playing) kx_post(KXC_PANIC, 0, 0, 0, 0); break;
    case VB_REC: kx_post(KXC_REC, 0, 0, 0, 0); break;
    case VB_ENTER: case VB_DATA_PUSH:
        if (kx_page == PG_GRID)
            kx_post(KXC_STEP, song.sel, ui.cursor, drum_track(TSEL) ? ui.lane : 255u, 0);
        break;
    default: break;
    }
}

static void kx_ui_init(void)
{
    kx_open(FA_SOUND, 0);
}

static void kx_ui_event(const vibe_event *e)
{
    switch (e->type) {
    case VIBE_EV_NOTE: {
        uint32_t vel = (uint32_t)(e->value * 127.0f + 0.5f);
        int midi = e->midi[1] == VIBE_NOTE_MIDI || e->midi[1] == VIBE_NOTE_HOST;
        if (e->down && !vel)
            vel = 1;
        kx_post(KXC_NOTE, e->id, e->down ? (vel > 127u ? 127u : vel) : 0u, midi ? 1u : 0u, e->midi[0]);
        break;
    }
    case VIBE_EV_PAD:
        if (e->id < 16u)
            kx_pad(e->id, e->down, e->value);
        break;
    case VIBE_EV_BUTTON:
        kx_button(e->id, e->down);
        break;
    case VIBE_EV_ENCODER:
        if (e->id < VE_COUNT)
            kx_turn(e->id, e->value);
        break;
    case VIBE_EV_MIDI: {
        uint32_t st = e->midi[0];
        if (st >= 0x80u && st < 0xF0u)
            kx_post(KXC_MIDI, st, e->midi[1] & 0x7Fu, e->midi[2] & 0x7Fu, 0);
        break;
    }
    default:
        break;
    }
}

/* ---- drawing ---------------------------------------------------------------------------------------------------- */
static uint64_t kx_now_ms(void) { return vibe_frames() * 1000u / (uint64_t)(kx_sr > 0 ? kx_sr : 48000.0f); }

static void kx_logo(int x, int y, int font)
{
    int w = vg_text("KRIECH", x, y, font, C_WHITE, VG_LEFT);
    vg_text("STROM", x + w, y, font, C_RED, VG_LEFT);
}

static void kx_draw_header(void)
{
    char b[16];
    const char *t = kx_title(kx_page);
    vg_rect(0, 0, 480, 30, C_BG);
    kx_logo(10, 7, VG_BOLD);
    vg_text(FA_NAME[kx_fam], 140, 7, VG_BOLD, C_DIM, VG_LEFT);
    vg_text(t, 140 + vg_text_width(FA_NAME[kx_fam], VG_BOLD) + 8, 7, VG_BOLD, C_WHITE, VG_LEFT);
    vg_itoa(song.g[G_BPM], b);
    vg_text(b, 396, 7, VG_BOLD, C_TEXT, VG_RIGHT);
    vg_text("BPM", 400, 9, VG_SMALL, C_DIM, VG_LEFT);
    if (song.playing)
        vg_tri(446, 8, 446, 22, 458, 15, C_RED);
    else
        vg_rect(446, 9, 11, 11, C_DIM);
    if (song.rec & (1u << song.sel))
        vg_circle(470, 15, 5, C_RED);
    vg_rect(0, 30, 480, 1, C_LINE);
}

static void kx_draw_tracks(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        int x = 4 + (int)i * 119, sel = i == song.sel, mute = t->p[P_MUTE] != 0;
        uint32_t col = TRK_RGB[i], lvl;
        char nm[17], n[2] = {(char)('1' + i), 0};
        float a = (float)clamp(t->peak, 0, 32767) / 32768.0f;
        vg_round(x, 34, 115, 38, 4, sel ? vg_mix(C_PANEL, col, 0.16f) : C_PANEL);
        if (sel)
            vg_frame(x, 34, 115, 38, 1, col);
        vg_rect(x, 34, 3, 38, mute ? C_LINE : col);
        vg_text(n, x + 9, 37, VG_BOLD, mute ? C_DIM : col, VG_LEFT);
        vg_text(ENGINES[eng_idx(t->eng_req)]->name, x + 22, 37, VG_SMALL, mute ? C_DIM : C_TEXT, VG_LEFT);
        sound_name(t, nm);
        nm[12] = 0;
        vg_text(nm, x + 9, 53, VG_SMALL, mute ? C_DIM : C_WHITE, VG_LEFT);
        lvl = (uint32_t)(a * 100.0f);
        vg_rect(x + 100, 70 - (int)(lvl * 30u / 100u), 10, (int)(lvl * 30u / 100u) + 1, mute ? C_LINE : col);
        if (mute)
            vg_text("MUTE", x + 98, 37, VG_SMALL, C_RED, VG_RIGHT);
    }
}

static void kx_draw_cards(int pg, int y)
{
    uint32_t k;
    uint32_t col = TRK_RGB[song.sel];
    if (pg >= 0 && pg < (int)NPAGES && PAGES[pg].scope == SC_GLOBAL)
        col = C_RED;
    for (k = 0; k < 4u; k++) {
        kx_card c;
        int x = 4 + (int)k * 119, w = 115;
        kx_card_of(pg, k, &c);
        vg_round(x, y, w, 72, 4, C_PANEL);
        if (!c.on)
            continue;
        vg_text(c.label, x + 8, y + 6, VG_SMALL, C_DIM, VG_LEFT);
        vg_text(c.val, x + 8, y + 22, VG_BIG, C_WHITE, VG_LEFT);
        if (c.unit && c.unit[0])
            vg_text(c.unit, x + w - 8, y + 30, VG_SMALL, C_DIM, VG_RIGHT);
        vg_rect(x + 8, y + 58, w - 16, 4, C_LINE);
        if (c.bip) {
            int mid = x + 8 + (w - 16) / 2, p = x + 8 + (int)(c.pos * (float)(w - 16));
            vg_rect(p < mid ? p : mid, y + 58, p < mid ? mid - p : p - mid + 1, 4, col);
        } else
            vg_rect(x + 8, y + 58, (int)(c.pos * (float)(w - 16)) + 1, 4, col);
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
        vg_round(x, 286, 76, 30, 4, sel ? vg_mix(C_PANEL, TRK_RGB[i], 0.25f) : C_PANEL);
        vg_text(s, x + 38, 293, VG_BOLD, sel ? C_WHITE : i < 4u ? vg_mix(C_DIM, TRK_RGB[i], 0.5f) : C_TEXT, VG_CENTER);
    }
}

/* the master's scope in a box (at 44.1 kHz, the last w * 2 frames) */
static void kx_draw_scope(int x, int y, int w, int h, uint32_t col, float gain)
{
    uint32_t sw = kx_scope_w, i;
    float mid = (float)y + (float)h * 0.5f, k = (float)h * 0.5f * gain / 32768.0f;
    vg_rect(x, (int)mid, w, 1, C_LINE);
    for (i = 0; i + 1u < (uint32_t)w; i++) {
        int32_t a = kx_scope[(sw - (uint32_t)w * 2u + i * 2u) & (KX_SCOPE - 1u)];
        int32_t b = kx_scope[(sw - (uint32_t)w * 2u + i * 2u + 2u) & (KX_SCOPE - 1u)];
        float ya = mid - (float)a * k, yb = mid - (float)b * k;
        ya = ya < (float)y ? (float)y : ya > (float)(y + h) ? (float)(y + h) : ya;
        yb = yb < (float)y ? (float)y : yb > (float)(y + h) ? (float)(y + h) : yb;
        vg_line((float)(x + (int)i), ya, (float)(x + (int)i + 1), yb, 1.4f, col);
    }
}

/* the envelope (ATK DEC SUS REL, 0..127 each) as a shape */
static void kx_draw_adsr(int x, int y, int w, int h, int a, int d, int s, int r, uint32_t col)
{
    float seg = (float)w / 4.2f, xa = (float)x + seg * (float)a / 127.0f, xd = xa + seg * (float)d / 127.0f;
    float xs = xd + seg * 0.9f, xr = xs + seg * (float)r / 127.0f, top = (float)y + 4.0f, bot = (float)(y + h - 2);
    float ys = bot - ((float)s / 127.0f) * (bot - top);
    vg_line((float)x, bot, xa, top, 2.5f, col);
    vg_line(xa, top, xd, ys, 2.5f, col);
    vg_line(xd, ys, xs, ys, 2.5f, col);
    vg_line(xs, ys, xr, bot, 2.5f, col);
    vg_circle(xa, top, 4, C_WHITE);
    vg_circle(xd, ys, 4, C_WHITE);
    vg_circle(xs, ys, 4, C_WHITE);
    vg_circle(xr, bot, 4, C_WHITE);
}

static void kx_draw_lfo(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    int i, wave = t->p[P_LWAVE], rate = t->p[P_LRATE];
    float cyc = 1.0f + (float)rate / 32.0f, mid = (float)y + (float)h * 0.5f, amp = (float)h * 0.42f, px = 0, py = 0;
    uint32_t r = 0x9E3779B9u;
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
            vg_line(px, py, (float)(x + i), mid - v * amp, 2.0f, col);
        px = (float)(x + i);
        py = mid - v * amp;
    }
}

/* the four sends of the selected track */
static void kx_draw_sends(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    static const char *const N[4] = {"DIST", "CHORUS", "DELAY", "REVERB"};
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int bx = x + (int)k * (w / 4) + 16, bw = w / 4 - 32, v = t->p[P_DIST + k];
        int bh = (h - 22) * clamp(v, 0, 127) / 127;
        vg_rect(bx, y, bw, h - 22, C_PANEL);
        vg_rect(bx, y + (h - 22) - bh, bw, bh, k == 0u ? C_RED : col);
        vg_text(N[k], bx + bw / 2, y + h - 18, VG_SMALL, C_DIM, VG_CENTER);
    }
}

/* the scale: the octave's keys, the scale's notes lit, the root red */
static void kx_draw_scale(int x, int y, int w, int h, const track_t *t)
{
    uint32_t m = scale_mask(t), root = (uint32_t)t->p[P_ROOT] % 12u, k;
    int kw = w / 12;
    for (k = 0; k < 12u; k++) {
        uint32_t n = (k + 12u - root) % 12u, on = (m >> n) & 1u, bl = key_black(k + 7u);
        int kx = x + (int)k * kw;
        vg_round(kx + 2, y + (bl ? 0 : h / 3), kw - 4, bl ? h * 2 / 3 : h * 2 / 3, 3,
                 k == root ? C_RED : on ? C_TEXT : C_PANEL);
        vg_text(N_NOTE[k], kx + kw / 2, y + h - 18, VG_SMALL, k == root ? C_WHITE : on ? C_BG : C_DIM, VG_CENTER);
    }
}

static void kx_draw_arp(int x, int y, int w, int h, const track_t *t, uint32_t col)
{
    int i, n = 16, oct = clamp(t->p[P_AOCT], 0, 3) + 1, mode = t->p[P_AMODE];
    if (!mode) {
        vg_text("ARP OFF", x + w / 2, y + h / 2 - 12, VG_BIG, C_DIM, VG_CENTER);
        return;
    }
    for (i = 0; i < n; i++) {
        int lv = (i % (4 * oct)), hgt = (h - 10) * (lv + 1) / (4 * oct);
        if (mode == 2) hgt = (h - 10) - hgt + (h - 10) / (4 * oct);
        vg_rect(x + i * (w / n) + 3, y + h - hgt, w / n - 6, hgt, (int)(t->arp_idx % 16u) == i ? C_WHITE : col);
    }
}

/* SEQ: the selected track's 64 steps, 4 rows of 16; the playhead; the cursor */
static void kx_draw_grid(int x, int y, int w, int h)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, cur = ui.cursor, play = t->seq_idx % len;
    uint32_t col = TRK_RGB[song.sel], bank = kx_bank();
    int drum = drum_track(t), cw = w / 16, rh = h / 4, lo = 127, hi = 0;
    for (i = 0; i < len; i++) {
        const step_t *st = &t->step[i];
        uint32_t k;
        if (st->time == ST_NOTE)
            for (k = 0; k < st->n; k++) {
                lo = st->note[k] < lo ? st->note[k] : lo;
                hi = st->note[k] > hi ? st->note[k] : hi;
            }
    }
    if (hi < lo) lo = 36, hi = 72;
    if (hi - lo < 12) hi = lo + 12;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        int cx = x + (int)(i % 16u) * cw, cy = y + (int)(i / 16u) * rh, in = i < len;
        uint32_t bg = !in ? C_BG : (i / 4u) % 2u ? 0x18181B : 0x121214;
        if (i / 16u == bank && in)
            bg = vg_mix(bg, col, 0.08f);
        vg_rect(cx + 1, cy + 1, cw - 2, rh - 2, bg);
        if (!in)
            continue;
        if (drum) {
            uint32_t l, hits = step_lanes(st);
            for (l = 0; l < NLANE; l++)
                if ((hits >> l) & 1u)
                    vg_rect(cx + 3 + (int)(l % 4u) * (cw - 6) / 4, cy + 4 + (int)(l / 4u) * (rh - 8) / 2,
                            (cw - 6) / 4 - 1, (rh - 8) / 2 - 2, l == ui.lane ? C_WHITE : col);
        } else if (st->time == ST_NOTE && st->n) {
            uint32_t k;
            for (k = 0; k < st->n; k++) {
                int yy = cy + rh - 6 - (int)((float)(st->note[k] - lo) / (float)(hi - lo) * (float)(rh - 12));
                vg_rect(cx + 2, yy, cw - 4, 4, st->flags & SF_ACCENT ? C_WHITE : col);
            }
        } else if (st->time == ST_TIE)
            vg_rect(cx, cy + rh / 2 - 1, cw, 3, vg_mix(col, C_BG, 0.5f));
        if (i == play && song.playing)
            vg_frame(cx + 1, cy + 1, cw - 2, rh - 2, 2, C_WHITE);
        else if (i == cur)
            vg_frame(cx + 1, cy + 1, cw - 2, rh - 2, 1, C_RED);
    }
}

/* MASTER: the chain of the whole mix, left to right, each block as strong as its amount */
static void kx_draw_master(int x, int y, int w, int h)
{
    static const char *const N[6] = {"DAMAGE", "WALL", "RESONATE", "SPLICE", "HAUNT", "HISS"};
    int amt[6], i, bw = (w - 5 * 6) / 6;
    amt[0] = song.g[G_DMG];
    amt[1] = song.g[G_WALL];
    amt[2] = song.g[G_RES];
    amt[3] = song.g[G_SPLICE];
    amt[4] = song.g[G_HAUNT] ? song.g[G_HAMT] : 0;
    amt[5] = song.g[G_HISS];
    kx_draw_scope(x, y, w, h - 52, C_RED, 1.0f + (float)amt[1] / 64.0f);
    for (i = 0; i < 6; i++) {
        int bx = x + i * (bw + 6), by = y + h - 46;
        float a = (float)clamp(amt[i], 0, 127) / 127.0f;
        vg_round(bx, by, bw, 44, 4, vg_mix(C_PANEL, C_RED, a * 0.55f));
        vg_text(N[i], bx + bw / 2, by + 6, VG_SMALL, a > 0 ? C_WHITE : C_DIM, VG_CENTER);
        vg_rect(bx + 6, by + 30, bw - 12, 4, C_LINE);
        vg_rect(bx + 6, by + 30, (int)(a * (float)(bw - 12)) + 1, 4, C_RED);
        if (i < 5)
            vg_tri((float)(bx + bw + 1), (float)(by + 18), (float)(bx + bw + 1), (float)(by + 26), (float)(bx + bw + 5),
                   (float)(by + 22), C_DIM);
    }
}

/* SOUND: the selected track's sound, big; the sounds before and after it in the list */
static void kx_draw_browse(int x, int y, int w, int h)
{
    const track_t *t = TSEL;
    char nm[17];
    uint32_t col = TRK_RGB[song.sel], total, cur = preset_pos(&total), r;
    sound_name(t, nm);
    vg_text(ENGINES[eng_idx(t->eng_req)]->name, x + 8, y + 4, VG_BOLD, col, VG_LEFT);
    vg_text(CAT_NAME[track_cat(t) % CAT_N], x + 8 + vg_text_width(ENGINES[eng_idx(t->eng_req)]->name, VG_BOLD) + 10,
            y + 4, VG_BOLD, C_DIM, VG_LEFT);
    vg_text(nm, x + 8, y + 26, VG_BIG | VG_X2, C_WHITE, VG_LEFT);
    kx_draw_scope(x + 8, y + 82, w / 2 - 16, h - 86, col, 1.5f);
    for (r = 0; r < 7u && total; r++) {
        uint32_t n = preset_visible(cur, total, r), k, e;
        char b[17];
        if (n >= total)
            continue;
        e = preset_at(n, &k);
        if (e == NENGINES)
            up_name(k, b);
        else
            str_cpy(b, ENGINES[e]->presets[k].name, sizeof b);
        vg_text(b, x + w / 2 + 16, y + 80 + (int)r * 16 - 8, VG_SMALL, n == cur ? C_WHITE : C_DIM, VG_LEFT);
        if (n == cur)
            vg_rect(x + w / 2 + 6, y + 80 + (int)r * 16 - 5, 4, 10, C_RED);
    }
}

static void kx_draw_mixer(int x, int y, int w, int h)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        int cx = x + (int)i * (w / 4), fw = w / 4 - 20, lv = t->p[P_LEVEL], pan = t->p[P_PAN];
        int fh = (h - 30) * clamp(lv, 0, 127) / 127;
        float a = (float)clamp(t->peak, 0, 32767) / 32768.0f;
        vg_rect(cx + 10, y, fw, h - 30, C_PANEL);
        vg_rect(cx + 10, y + h - 30 - fh, fw, fh, t->p[P_MUTE] ? C_LINE : vg_mix(C_PANEL, TRK_RGB[i], 0.6f));
        vg_rect(cx + 10 + fw - 8, y + (int)((1.0f - a) * (float)(h - 30)), 6, (int)(a * (float)(h - 30)), C_WHITE);
        vg_rect(cx + 10 + fw / 2 + pan * fw / 256, y + h - 24, 3, 8, C_WHITE);
        vg_rect(cx + 10, y + h - 20, fw, 1, C_LINE);
    }
    (void)h;
}

/* INPUT: what comes in (its scope and level), where it goes */
static void kx_draw_input(int x, int y, int w, int h)
{
    uint32_t i, kw = kx_kin_w;
    float mid = (float)y + (float)(h - 28) * 0.5f, k = (float)(h - 28) * 0.45f / 32768.0f, px = 0, py = 0;
    if (!kx_in_on) {
        vg_text("NO INPUT", x + w / 2, y + 20, VG_BIG, C_DIM, VG_CENTER);
        vg_text("CONNECT IN 1 / IN 2, OR ALLOW THE MICROPHONE", x + w / 2, y + 52, VG_SMALL, C_DIM, VG_CENTER);
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

static void kx_draw_project(int x, int y, int w, int h)
{
    vg_text("PROJECT", x + w / 2, y + h / 2 - 20, VG_BIG, C_DIM, VG_CENTER);
}

static void kx_draw_help(void)
{
    static const char *const R[] = {
        "KEYS", "play the selected track", "PADS 1-4", "mute track (SHIFT: select)", "PADS 5-8", "DICE: a new part (SHIFT: MOOD)",
        "PADS 9-12", "hold: 1/16, STOP, FREEZE, GATE", "PADS 13-16", "drums: KICK SNARE CLAP HAT",
        "SOUND ENV LFO ARP FX SEQ", "the pages (again: the next)", "ENCODERS 1-4", "the four values",
        "DATA", "the sounds (SHIFT: engine)", "A / B", "track LEVEL / DAMAGE (SHIFT: WALL)",
        "F1-F4", "select track 1-4", "F5 / F6", "the page's action / UNDO", "MENU", "mixer, project, clock",
        "PLAY STOP REC", "transport, record the track",
    };
    vg_controls("KRIECHSTROM", R, (int)(sizeof R / sizeof R[0] / 2));
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
        vg_round(240 - w / 2, 232, w, 30, 6, C_RED);
        vg_text(kx_toast, 240, 239, VG_MED, C_WHITE, VG_CENTER);
    }
}

static void kx_leds(void)
{
    uint32_t i, now = (uint32_t)kx_now_ms();
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
    (void)now;
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

static void kx_ui_draw(void)
{
    int pg = kx_page, kp = kx_knob_page();
    const track_t *t = TSEL;
    uint32_t col = TRK_RGB[song.sel];
    if (kx_view_bank >= 0 && vibe_frames() - kx_view_t > (uint64_t)(kx_sr * 3.0f))
        kx_view_bank = -1;                        /* (the pads' bank follows the playhead again after 3 s) */
    kx_labels();
    kx_leds();
    if ((pg == PG_INPUT || pg == PG_INPUT2) && !kx_in_asked) {
        kx_in_asked = 1;                          /* (the browser asks for the microphone now, not at every start) */
        vibe_request(VIBE_REQ_AUDIO_INPUT);
    }
    if (kx_help) {
        kx_draw_help();
        return;
    }
    vg_clear(C_BG);
    kx_draw_header();
    kx_draw_tracks();
    if (pg == PG_GRID) {
        kx_draw_grid(4, 78, 472, 128);
    } else if (pg == PG_BROWSE) {
        kx_draw_browse(4, 76, 472, 132);
    } else if (pg == PG_MASTER) {
        kx_draw_master(4, 78, 472, 130);
    } else if (pg == PG_MIXER) {
        kx_draw_mixer(4, 78, 472, 200);
    } else if (pg == PG_INPUT || pg == PG_INPUT2) {
        kx_draw_input(4, 80, 472, 124);
    } else if (pg == PG_PROJECT) {
        kx_draw_project(4, 78, 472, 200);
    } else if (pg >= 0 && pg < (int)NPAGES) {
        const page_t *p = &PAGES[pg];
        const char *what = page_what(p->title);
        switch (p->graph) {
        case GR_ADSR:
            if (p->id[0] == P_ATK)
                kx_draw_adsr(12, 84, 456, 104, t->p[P_ATK], t->p[P_DEC], t->p[P_SUS], t->p[P_REL], col);
            else
                kx_draw_adsr(12, 84, 456, 104, t->p[p->id[0]], t->p[p->id[1]], t->p[p->id[2]], t->p[p->id[3]], col);
            break;
        case GR_LFO: kx_draw_lfo(12, 86, 456, 100, t, col); break;
        case GR_FX: kx_draw_sends(12, 82, 456, 116, t, col); break;
        case GR_SCALE: kx_draw_scale(12, 84, 456, 110, t); break;
        case GR_ARP: kx_draw_arp(12, 84, 456, 110, t, col); break;
        default:
            kx_draw_scope(4, 80, 472, 118, p->scope == SC_GLOBAL ? C_RED : col, 1.2f);
            break;
        }
        if (what)
            vg_text(what, 476, 194, VG_SMALL, C_DIM, VG_RIGHT);
    }
    if (pg != PG_MIXER && pg != PG_PROJECT)
        kx_draw_cards(kp, 210);
    kx_draw_soft();
    kx_draw_toast();
}
