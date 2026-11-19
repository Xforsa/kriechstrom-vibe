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
enum { PG_BROWSE = 200, PG_MASTER, PG_GRID, PG_MIXER, PG_PROJECT, PG_INPUT, PG_INPUT2, PG_MIDI };
#define IC_PAGE(pg) ((pg) == PG_INPUT ? 0 : (pg) == PG_INPUT2 ? 1 : 2)
#define IS_IC_PAGE(pg) ((pg) == PG_INPUT || (pg) == PG_INPUT2 || (pg) == PG_MIDI)
#define PG_MAX 24
static uint8_t kx_pl[PG_MAX], kx_pn;              /* the open family's pages, in order */
static uint8_t kx_fam, kx_fpage[FA_N];            /* the family open, each family's page */
static int kx_page = PG_BROWSE;                   /* the page shown */
static int kx_help, kx_shift, kx_in_asked, kx_slot_pick;
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
        ADD(PG_MIDI);
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
    case PG_MIDI: return "MIDI OUT & CV";
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
    if (IS_IC_PAGE(kx_page))
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
static const uint8_t IN_SLOTS[3][4] = {{IC_LEVEL, IC_DRIVE, IC_GATE, IC_FEED}, {IC_PAN, IC_CHO, IC_DLY, IC_REV},
                                       {IC_MOUT, IC_MCH, IC_CLK, IC_CV}};
static void kx_card_of(int pg, uint32_t slot, kx_card *c)
{
    if (IS_IC_PAGE(pg)) {                         /* the audio input's, MIDI OUT's (kx_input.h, kx_midi.h) */
        uint32_t f = IN_SLOTS[IC_PAGE(pg)][slot & 3u];
        int v = kx_ic[f];
        memset(c, 0, sizeof *c);
        c->on = 1;
        c->label = IC_DESC[f].label;
        if (f == IC_GATE || f == IC_FEED || f == IC_CV)
            str_cpy(c->val, IC_TRK[clamp(v, 0, NTRK)], sizeof c->val);
        else if (f == IC_MOUT || f == IC_CLK)
            str_cpy(c->val, v ? "ON" : "OFF", sizeof c->val);
        else if (f == IC_MCH) {
            char *q = vg_itoa(v, c->val);
            str_cpy(q + str_len(q), "-", 2);
            vg_itoa(v + 3, q + str_len(q));
        }
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
    case FA_MENU:
        if (kx_page == PG_PROJECT)
            kx_post(KXC_SLOT, (uint32_t)kx_slot_pick, kx_shift ? 2u : 1u, 0, 0);
        break;
    default: break;
    }
}
static const char *kx_f5_name(void)
{
    switch (kx_fam) {
    case FA_SOUND: return "OOPS";
    case FA_SEQ: return kx_shift ? "MOOD" : "DICE";
    case FA_MENU: return kx_page == PG_PROJECT ? (kx_shift ? "NEW" : "SAVE") : "";
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
        if (kx_page == PG_PROJECT) {
            if (enc == VE_1)
                kx_slot_pick = clamp(kx_slot_pick + (n > 0 ? 1 : -1), 0, 3);
        } else if (kx_page == PG_MIXER) {                /* the four tracks' LEVEL (SHIFT: PAN) */
            uint32_t id = kx_shift ? P_PAN : P_LEVEL;
            const param_desc_t *d = &TP[id];
            kx_post(KXC_PARAM, enc, id & 0xFFu, id >> 8, clamp(trk[enc].p[id] + n, d->min, d->max));
        } else if (IS_IC_PAGE(kx_page)) {
            uint32_t f = IN_SLOTS[IC_PAGE(kx_page)][enc];
            kx_post(KXC_INCFG, f, 0, 0, IC_DESC[f].max - IC_DESC[f].min < 20 ? (n > 0 ? 1 : -1) : n);
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
        else if (kx_page == PG_PROJECT)
            kx_post(KXC_SLOT, (uint32_t)kx_slot_pick, 0, 0, 0);
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

