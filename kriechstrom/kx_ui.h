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
#define C_DIM 0x9A9AA4
#define C_TEXT 0xD8D8DC
#define C_WHITE 0xF4F4F4
#define C_RED 0xE01818
#define C_RUST 0x8A1A10
static const uint32_t TRK_RGB[NTRK] = {0xE0301C, 0xE8A23A, 0xD9D2C2, 0x7896B0};   /* rust red, sulfur, bone, steel */

/* ---- families and pages ---------------------------------------------------------------------------------------- */
enum { FA_SOUND, FA_ENV, FA_LFO, FA_ARP, FA_FX, FA_SEQ, FA_MENU, FA_N };
static const char *const FA_NAME[FA_N] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ", "MENU"};
/* pages of our own, past PAGES' indices */
enum { PG_BROWSE = 200, PG_MASTER, PG_GRID, PG_MIXER, PG_PROJECT, PG_INPUT, PG_INPUT2, PG_MIDI, PG_USER, PG_DISPLAY,
       PG_PEDALS, PG_PEDALS_IN, PG_PEDALS_M, PG_FOOT, PG_SAMPLER };
#define IS_PED_PAGE(pg) ((pg) == PG_PEDALS || (pg) == PG_PEDALS_IN || (pg) == PG_PEDALS_M)
#define IC_PAGE(pg) ((pg) == PG_INPUT ? 0 : (pg) == PG_INPUT2 ? 1 : (pg) == PG_MIDI ? 2 : (pg) == PG_DISPLAY ? 3 : (pg) == PG_FOOT ? 4 : 5)
#define IS_IC_PAGE(pg) ((pg) == PG_INPUT || (pg) == PG_INPUT2 || (pg) == PG_MIDI || (pg) == PG_DISPLAY || (pg) == PG_FOOT || (pg) == PG_SAMPLER)
#define PG_MAX 24
static uint8_t kx_pl[PG_MAX], kx_pn;              /* the open family's pages, in order */
static uint8_t kx_fam, kx_fpage[FA_N];            /* the family open, each family's page */
static int kx_page = PG_BROWSE;                   /* the page shown */
static int kx_help, kx_shift, kx_in_asked, kx_slot_pick, kx_boot_done, kx_user_pick;
static int kx_shift_latch, kx_shift_used;         /* SHIFT tapped: it holds for the next control (a mouse has one button) */
static uint64_t kx_shift_t;
static int kx_start = 1, kx_start_sel;
static uint64_t kx_card_hot[4];
static uint64_t kx_soft_t[6];                     /* when F1-F6 were last pressed (they flash on the display) */
static uint64_t kx_idle_t;                        /* the last touch (the idle screen after 90 s) */
static float kx_foot_expr;                        /* the expression pedal's position (the FOOT PEDAL page) */
static int kx_foot_down;
static int kx_ped_focus[3];                       /* the pedal in focus on each pedal page */
static uint32_t kx_ped_owner(int pg) { return pg == PG_PEDALS_IN ? PO_IN : pg == PG_PEDALS_M ? PO_MASTER : song.sel; }
#define KX_PED_W 138                                /* a pedal's width on the board, and where each is */
static int kx_ped_x(uint32_t ns, uint32_t j)
{
    int gap = (472 - 24 - (int)ns * KX_PED_W) / (int)(ns + 1u);
    return 4 + 12 + gap + (int)j * (KX_PED_W + gap);
}
static int kx_ped_fi(int pg) { return pg == PG_PEDALS_IN ? 1 : pg == PG_PEDALS_M ? 2 : 0; }                   /* when encoder 1-4 last moved its value (the card lights up) */            /* the start screen: 0 where you left it, 1 the demo song, 2 empty */
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
    if (fop_page(g))
        return fop_ok();
    return g != GR_SLICES && g != GR_USER && g != GR_PATS && g != GR_SLOTS && g != GR_TOOLS && g != GR_BROWSE &&
           g != GR_TRK && PAGES[i].scope != SC_STEP;
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
        ADD(PG_SAMPLER);
        ADD(PG_USER);
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
        ADD(PG_PEDALS);
        for (i = 0; i < NPAGES; i++)              /* (the INSERT pages: the PEDALS page has them) */
            if (PAGES[i].fam == FAM_FX && kx_page_ok(i) && PAGES[i].graph != GR_INS)
                ADD(i);
        ADD(PG_INPUT);
        ADD(PG_INPUT2);
        ADD(PG_PEDALS_IN);
        ADD(PG_PEDALS_M);
        break;
    case FA_SEQ:
        ADD(PG_GRID);
        i = (uint32_t)kx_page_index("DETAIL");
        ADD(i);
        i = (uint32_t)kx_page_index("AUTOMATION");
        ADD(i);
        i = (uint32_t)kx_page_index("SONG");
        ADD(i);
        break;
    default:
        ADD(PG_MIXER);
        ADD(PG_PROJECT);
        ADD(PG_MIDI);
        ADD(PG_FOOT);
        ADD(PG_DISPLAY);
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
    case PG_USER: return "MY SOUNDS";
    case PG_DISPLAY: return "DISPLAY";
    case PG_FOOT: return "FOOT PEDAL";
    case PG_SAMPLER: return "SAMPLER";
    case PG_PEDALS: return "PEDALS";
    case PG_PEDALS_IN: return "INPUT PEDALS";
    case PG_PEDALS_M: return "MASTER PEDALS";
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
    if (IS_PED_PAGE(kx_page))
        return kx_page;
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
static const uint8_t IN_SLOTS[6][4] = {{IC_LEVEL, IC_DRIVE, IC_GATE, IC_FEED}, {IC_PAN, IC_CHO, IC_DLY, IC_REV},
                                       {IC_MOUT, IC_MCH, IC_CLK, IC_CV}, {IC_SCREEN, 255, 255, 255},
                                       {IC_EXPR, IC_FSW, 255, 255}, {IC_SSRC, IC_SBARS, IC_SSLOT, 255}};
static void kx_card_of(int pg, uint32_t slot, kx_card *c)
{
    if (IS_IC_PAGE(pg)) {                         /* the audio input's, MIDI OUT's (kx_input.h, kx_midi.h) */
        uint32_t f = IN_SLOTS[IC_PAGE(pg)][slot & 3u];
        int v;
        memset(c, 0, sizeof *c);
        if (f >= IC_N)
            return;
        v = kx_ic[f];
        c->on = 1;
        c->label = IC_DESC[f].label;
        if (f == IC_GATE || f == IC_FEED || f == IC_CV)
            str_cpy(c->val, IC_TRK[clamp(v, 0, NTRK)], sizeof c->val);
        else if (f == IC_SCREEN)
            str_cpy(c->val, IC_WEAR_N[clamp(v, 0, 2)], sizeof c->val);
        else if (f == IC_SSRC)
            str_cpy(c->val, IC_SSRC_N[clamp(v, 0, 1)], sizeof c->val);
        else if (f == IC_SBARS)
            str_cpy(c->val, IC_SBARS_N[clamp(v, 0, 3)], sizeof c->val);
        else if (f == IC_SSLOT)
            str_cpy(c->val, IC_SSLOT_N[clamp(v, 0, 2)], sizeof c->val);
        else if (f == IC_EXPR)
            str_cpy(c->val, IC_EXPR_N[clamp(v, 0, 3)], sizeof c->val);
        else if (f == IC_FSW)
            str_cpy(c->val, IC_FSW_N[clamp(v, 0, 3)], sizeof c->val);
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
    if (IS_PED_PAGE(pg)) {                        /* the pedal in focus: TYPE (SHIFT: MIX), its three */
        uint32_t o = kx_ped_owner(pg), j = (uint32_t)kx_ped_focus[kx_ped_fi(pg)];
        int16_t v[PD_N];
        const param_desc_t *dd;
        const char *un = "";
        kx_ped_get(o, j, v);
        c->on = 1;
        if (slot == 0u) {
            if (kx_shift) {
                c->label = "MIX";
                param_format(&TP[P_IMIX], v[PD_MIX], c->val, &un);
                c->pos = (float)v[PD_MIX] / 127.0f;
            } else {
                c->label = "PEDAL";
                str_cpy(c->val, N_ITYPE[clamp(v[0], 0, IT_N - 1)], sizeof c->val);
                c->pos = (float)v[0] / (float)(IT_N - 1);
                un = "MIX: SHIFT";
            }
            c->unit = un;
            return;
        }
        if (!v[0]) {
            c->on = 0;
            return;
        }
        dd = ins_desc(v[0], slot - 1u);
        c->label = dd->label;
        param_format(dd, v[slot], c->val, &un);
        c->unit = un;
        c->pos = dd->max > dd->min ? (float)(v[slot] - dd->min) / (float)(dd->max - dd->min) : 0.0f;
        return;
    }
    if (pg == PG_USER) {                          /* MY SOUNDS: the slot, its sound */
        static const char *const L[4] = {"SLOT", "SOUND", "ENGINE", ""};
        uint32_t s = (uint32_t)kx_user_pick % UP_SLOTS;
        c->on = slot < 3u;
        c->label = L[slot & 3u];
        if (slot == 0u)
            up_slot_label(c->val, s), c->pos = (float)s / (float)(UP_SLOTS - 1u);
        else if (slot == 1u)
            str_cpy(c->val, up_used(s) ? "USED" : "EMPTY", sizeof c->val);
        else if (slot == 2u)
            str_cpy(c->val, up_used(s) ? ENGINES[up_engine(s) % NENGINES]->name : "-", sizeof c->val);
        return;
    }
    if (pg < 0 || pg >= (int)NPAGES)
        return;
    p = &PAGES[pg];
    if (fop_page(p->graph)) {                     /* FM6's operators (ui_fm6op.c) */
        const char *lb, *un;
        int32_t r;
        uint32_t ic;
        char v[16];
        if (!fop_ok())
            return;
        c->on = 1;
        fop_card(slot, &lb, v, &un, &r, &ic);
        c->label = lb;
        str_cpy(c->val, v, sizeof c->val);
        c->unit = un;
        c->pos = r < 0 ? 0.0f : (float)r / 1000.0f;
        return;
    }
    if (p->graph == GR_SONG) {                    /* SECTION, TRACK, its PAT (A..D, -), REPS */
        static const char *const L[4] = {"SECTION", "TRACK", "PAT", "REPS"};
        uint32_t row = ui.song_row < CHAIN_ROWS ? ui.song_row : CHAIN_ROWS - 1u, k = ui.song_trk % NTRK, sl;
        int used = row < chain_config.count;
        c->on = 1;
        c->label = L[slot];
        if (slot == 0u)
            used ? vg_itoa((int)row + 1, c->val) : (void)str_cpy(c->val, "+ ADD", sizeof c->val);
        else if (slot == 1u)
            c->val[0] = 'T', c->val[1] = (char)('1' + k), c->val[2] = 0;
        else if (slot == 2u) {
            sl = used ? chain_config.row[row].slot[k] : CHAIN_SILENT;
            c->val[0] = used ? (sl < 4u ? (char)('A' + sl) : '-') : '-';
            c->val[1] = 0;
        } else if (used)
            vg_itoa(chain_config.row[row].repeat, c->val);
        else
            str_cpy(c->val, "-", sizeof c->val);
        return;
    }
    if (p->graph == GR_EVENTS) {                  /* AUTOMATION (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint16_t rw[EV_ROWS];
        uint32_t n, cc = ev_cur(rw, &n), r = ev_row(n), kk = cc >> 8, step, id;
        static const char *const L[4] = {"ROW", "STEP", "PARAM", "VALUE"};
        c->on = 1;
        c->label = L[slot];
        if (kk == EVK_TOP || kk == EVK_QNT) {
            static const char *const T[4] = {"ROW", "EVENTS", "LOCKS", "PLAY"}, *const Q[4] = {"ROW", "NUDGED", "", "QNTZ"};
            c->label = kk == EVK_TOP ? T[slot] : Q[slot];
            if (slot == 0u)
                str_cpy(c->val, "ALL", sizeof c->val);
            else if (slot == 1u)
                vg_itoa(kk == EVK_TOP ? (int)(motion_count(t) - motion_lock_count(t)) : (int)ev_nudged(t), c->val);
            else if (slot == 2u && kk == EVK_TOP)
                vg_itoa((int)motion_lock_count(t), c->val);
            else if (slot == 3u)
                str_cpy(c->val, kk == EVK_TOP ? (motion_enabled(t) ? "ON" : "OFF") : (t->p[P_SQNT] ? "ON" : "OFF"),
                        sizeof c->val);
            return;
        }
        step = ev_step_of(cc);
        id = kk == EVK_REC ? MOTION_ID(&motion.event[cc & 0xFFu]) : kk == EVK_ADD ? ui.ev_id :
             kk == EVK_CHANCE ? EV_CHANCE : kk == EVK_RATCH ? EV_RATCH : kk == EVK_VEL ? EV_VEL : EV_NUDGE;
        if (slot == 0u) {
            if (kk == EVK_ADD)
                str_cpy(c->val, "ADD", sizeof c->val);
            else {
                char *q = vg_itoa((int)r - 1, c->val);
                str_cpy(q + str_len(q), "/", 2);
                vg_itoa((int)n - 3, q + str_len(q));
            }
        } else if (slot == 1u)
            vg_itoa((int)step + 1, c->val);
        else if (!ev_id_ok(t, id))
            str_cpy(c->val, "-", sizeof c->val);
        else if (slot == 2u) {
            char nm[12];
            ev_name(t, id, nm);
            str_cpy(c->val, nm, sizeof c->val);
        } else if (id == EV_CHANCE || id == EV_RATCH || id == EV_NUDGE || id == EV_VEL) {
            const step_t *cs = &t->step[step % NSTEP];
            uint32_t sk = ev_skind(id);
            int32_t sv = kk == EVK_ADD ? ev_sadd(sk) : ev_sval(cs, sk);
            if (sk == EVK_CHANCE)
                c->unit = ev_chance_fmt(c->val, sv);
            else if (sk == EVK_NUDGE)
                ev_nudge_fmt(c->val, sv);
            else if (sk == EVK_RATCH)
                c->val[0] = 'x', c->val[1] = (char)('0' + sv), c->val[2] = 0;
            else
                vg_itoa(sv, c->val);
        } else {
            int16_t v = kk == EVK_REC ? motion.event[cc & 0xFFu].value : motion_base_value(t, id);
            const param_desc_t *d = track_desc(t, id);
            const char *un = "";
            param_format(d, v, c->val, &un);
            c->unit = un;
            c->label = kk == EVK_REC && (motion.event[cc & 0xFFu].param & MOTION_LOCK) ? "LOCK" : "AUTO";
            c->pos = d->max > d->min ? (float)(v - d->min) / (float)(d->max - d->min) : 0.0f;
        }
        if (ui.act == slot + 1u && act_col() == slot + 1u)
            c->unit = "ENTER";                    /* (the action this knob picked: ENTER does it) */
        return;
    }
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

static vibe_device_info kx_dev;                   /* (SDK 19/20: a bigger pad grid, a Launchpad 8 x 8) */
static void kx_pad(uint32_t id, int down, float vel)
{
    uint32_t row = id / 4u, col = id % 4u;
    if (id >= 16u) {                              /* a bigger grid's pads: on SEQ, all 64 steps of the track */
        if (kx_page == PG_GRID && down && id < NSTEP)
            kx_post(KXC_STEP, song.sel, id, drum_track(TSEL) ? ui.lane : 255u, 0);
        return;
    }
    if (kx_page == PG_GRID && kx_dev.pads >= NSTEP) {   /* (on 8 x 8 the first 16 are steps 1-16 too) */
        if (down)
            kx_post(KXC_STEP, song.sel, id, drum_track(TSEL) ? ui.lane : 255u, 0);
        return;
    }
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
    case FA_SOUND:
        if (kx_page == PG_SAMPLER) {
            static const uint8_t BARS[4] = {0, 1, 2, 4};
            if (kx_page == PG_SAMPLER && (kx_ic[IC_SSRC] == 0) && !kx_in_asked) {
                kx_in_asked = 1;
                vibe_request(VIBE_REQ_AUDIO_INPUT);
            }
            kx_post(KXC_SREC, (uint32_t)kx_ic[IC_SSRC], BARS[clamp(kx_ic[IC_SBARS], 0, 3)], (uint32_t)kx_ic[IC_SSLOT], 0);
        } else if (kx_page == PG_USER)
            kx_post(KXC_USER, (uint32_t)kx_user_pick, kx_shift ? 2u : 1u, 0, 0);
        else
            kx_post(KXC_OOPS, song.sel, 0, 0, 0);
        break;
    case FA_SEQ:
        if (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].graph == GR_SONG)
            kx_post(KXC_SONG, 0, 0, 0, 0);
        else
            kx_post(KXC_TOOL, song.sel, kx_shift ? TL_MOOD : TL_DICE, 0, 0);
        break;
    case FA_FX:
        if (IS_PED_PAGE(kx_page)) {               /* F5: the next pedal of the row */
            int fi = kx_ped_fi(kx_page);
            kx_ped_focus[fi] = (kx_ped_focus[fi] + 1) % kx_ped_slots(kx_ped_owner(kx_page));
            kx_card_hot[0] = kx_card_hot[1] = kx_card_hot[2] = kx_card_hot[3] = vibe_frames();
        } else if (kx_page == PG_INPUT || kx_page == PG_INPUT2) {
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
    case FA_SOUND:
        if (kx_page == PG_SAMPLER)
            return kx_rec_state ? "STOP" : "REC";
        return kx_page == PG_USER ? (kx_shift ? "ERASE" : "SAVE") : "OOPS";
    case FA_SEQ:
        if (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].graph == GR_SONG)
            return song.playing ? "STOP" : "PLAY";
        return kx_shift ? "MOOD" : "DICE";
    case FA_MENU: return kx_page == PG_PROJECT ? (kx_shift ? "NEW" : "SAVE") : "";
    case FA_FX:
        if (IS_PED_PAGE(kx_page))
            return "NEXT";
        return kx_page == PG_INPUT || kx_page == PG_INPUT2 ? IC_MODE_N[kx_ic[IC_MODE] % 3] : "";
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
    if (enc <= VE_4)
        kx_card_hot[enc] = vibe_frames();
    if (enc <= VE_4) {
        int pg = kx_knob_page();
        if (IS_PED_PAGE(kx_page)) {
            uint32_t f = enc == VE_1 ? (kx_shift ? PD_MIX : PD_TYPE) : PD_A + (enc - VE_2);
            kx_post(KXC_PED, kx_ped_owner(kx_page), (uint32_t)kx_ped_focus[kx_ped_fi(kx_page)], f,
                    f == PD_TYPE ? (n > 0 ? 1 : -1) : n);
        } else if (kx_page == PG_USER) {
            if (enc == VE_1)
                kx_user_pick = clamp(kx_user_pick + n, 0, UP_SLOTS - 1);
        } else if (kx_page == PG_PROJECT) {
            if (enc == VE_1)
                kx_slot_pick = clamp(kx_slot_pick + (n > 0 ? 1 : -1), 0, 3);
        } else if (kx_page == PG_MIXER) {                /* the four tracks' LEVEL (SHIFT: PAN) */
            uint32_t id = kx_shift ? P_PAN : P_LEVEL;
            const param_desc_t *d = &TP[id];
            kx_post(KXC_PARAM, enc, id & 0xFFu, id >> 8, clamp(trk[enc].p[id] + n, d->min, d->max));
        } else if (IS_IC_PAGE(kx_page)) {
            uint32_t f = IN_SLOTS[IC_PAGE(kx_page)][enc];
            if (f < IC_N)
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
        } else if (IS_PED_PAGE(kx_page)) {        /* DATA: the pedal in focus */
            int fi = kx_ped_fi(kx_page), ns = kx_ped_slots(kx_ped_owner(kx_page));
            kx_ped_focus[fi] = clamp(kx_ped_focus[fi] + (n > 0 ? 1 : -1), 0, ns - 1);
            kx_card_hot[0] = kx_card_hot[1] = kx_card_hot[2] = kx_card_hot[3] = vibe_frames();
        } else if (kx_shift)
            kx_post(KXC_ENGINE, 0, 0, 0, n > 0 ? 1 : -1);
        else
            kx_post(KXC_PRESET, 0, 0, 0, n);
    }
}

/* the foot pedal: the expression pedal turns what MENU > FOOT PEDAL says, the switch does what it says (from the
 * PEDAL jack, or clicked on that page) */
static void kx_foot_set(float v)
{
    int32_t q = (int32_t)(v * 127.0f + 0.5f);
    kx_foot_expr = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    if (kx_ic[IC_EXPR] == 1)
        kx_post(KXC_PARAM, NTRK, G_DMG, 0, q);
    else if (kx_ic[IC_EXPR] == 2)
        kx_post(KXC_PARAM, NTRK, G_WALL, 0, q);
    else if (kx_ic[IC_EXPR] == 3)
        kx_post(KXC_PARAM, song.sel, P_LEVEL, 0, q);
}
static void kx_foot_switch(int down)
{
    kx_foot_down = down;
    switch (kx_ic[IC_FSW]) {
    case 0: if (down) kx_post(KXC_PLAY, 3, 0, 0, 0); break;
    case 1: if (down) kx_post(KXC_TOOL, song.sel, TL_DICE, 0, 0); break;
    case 2: kx_post(KXC_PERF, PF_TAPE, down ? 1u : 0u, 0, 0); break;
    default: kx_post(KXC_PERF, PF_FRZ, down ? 1u : 0u, 0, 0); break;
    }
}

static void kx_button(uint32_t b, int down)
{
    if (b == VB_SHIFT) {                          /* held: as long as it is down; tapped: for the next control */
        if (down) {
            if (kx_shift_latch) {
                kx_shift_latch = kx_shift = 0;    /* (tapped again: off) */
                kx_shift_used = 1;
                return;
            }
            kx_shift = 1;
            kx_shift_used = 0;
            kx_shift_t = vibe_frames();
        } else if (!kx_shift_used && vibe_frames() - kx_shift_t < (uint64_t)(kx_sr * 0.5f))
            kx_shift_latch = 1;
        else
            kx_shift = kx_shift_latch = 0;
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
        kx_soft_t[b - VB_F1] = vibe_frames() ? vibe_frames() : 1;
        kx_soft(b - VB_F1);
        break;
    case VB_PLAY: kx_post(KXC_PLAY, 3, 0, 0, 0); break;
    case VB_STOP: kx_post(KXC_PLAY, 2, 0, 0, 0); if (!song.playing) kx_post(KXC_PANIC, 0, 0, 0, 0); break;
    case VB_REC: kx_post(KXC_REC, 0, 0, 0, 0); break;
    case VB_ENTER: case VB_DATA_PUSH:
        if (kx_page == PG_GRID)
            kx_post(KXC_STEP, song.sel, ui.cursor, drum_track(TSEL) ? ui.lane : 255u, 0);
        else if (kx_page == PG_PROJECT)
            kx_post(kx_shift ? KXC_DEMO : KXC_SLOT, (uint32_t)kx_slot_pick, 0, 0, 0);
        else if (kx_page == PG_USER)
            kx_post(KXC_USER, (uint32_t)kx_user_pick, 0, 0, 0);
        else if (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].graph == GR_EVENTS)
            kx_post(KXC_ACT, (uint32_t)kx_page, 0, 0, 0);
        break;
    default: break;
    }
}

static void kx_ui_init(void)
{
    vf_mode(VF_ON_HOLD);                          /* (DATA is the sounds' here: ENTER held 0.6 s = FOCUS) */
    vf_clicks(1);
    vibe_device(&kx_dev);
    kx_open(FA_SOUND, 0);
    kx_start_sel = kx_had_session ? 0 : 1;
}

static void kx_ui_event(const vibe_event *e)
{
    int fid;
    if (e->type <= VIBE_EV_ENCODER || e->type == VIBE_EV_NOTE || e->type == VIBE_EV_PEDAL || e->type == VIBE_EV_TOUCH)
        kx_idle_t = vibe_frames() ? vibe_frames() : 1;
    if (e->type == VIBE_EV_BUTTON || e->type == VIBE_EV_PAD || e->type == VIBE_EV_ENCODER || e->type == VIBE_EV_NOTE)
        kx_boot_done = 1;                         /* (the power-on picture ends at the first touch) */
    if (kx_start && kx_boot_done && e->type == VIBE_EV_BUTTON && e->down) {   /* the start screen */
        uint32_t b = e->id;
        int pick = -1;
        if (b == VB_F1 || b == VB_F2 || b == VB_F3)
            pick = (int)(b - VB_F1);
        else if (b == VB_ENTER || b == VB_DATA_PUSH)
            pick = kx_start_sel;
        else if (b == VB_RETURN || b == VB_HOME)
            pick = 0;
        if (pick >= 0) {
            if (pick == 1)
                kx_post(KXC_DEMO, 0, 0, 0, 0);
            else if (pick == 2)
                kx_post(KXC_SLOT, 0, 2, 0, 0);
            kx_start = 0;
            return;
        }
    }
    if (kx_start && kx_boot_done && e->type == VIBE_EV_ENCODER && e->id == VE_DATA) {
        kx_start_sel = (kx_start_sel + (e->value > 0 ? 1 : 2)) % 3;
        return;
    }
    if (kx_start && kx_boot_done && (e->type == VIBE_EV_PAD || e->type == VIBE_EV_NOTE ||
                                     (e->type == VIBE_EV_BUTTON && e->id == VB_PLAY)))
        kx_start = 0;                             /* (played instead: what is there now stays) */
    if (vf_event(e, &fid) != VF_NO)               /* FOCUS first (SDK 10) */
        return;
    if (kx_shift && ((e->type == VIBE_EV_BUTTON && e->id != VB_SHIFT && e->down) || (e->type == VIBE_EV_PAD && e->down) ||
                     e->type == VIBE_EV_ENCODER))
        kx_shift_used = 1;
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
        kx_pad(e->id, e->down, e->value);
        break;
    case VIBE_EV_TOUCH:                           /* a click into the display (plugin, browser): the steps, the pedals */
        if (e->id == VIBE_TOUCH_DOWN) {
            int x = VIBE_TOUCH_X(e), y = VIBE_TOUCH_Y(e);
            if (kx_page == PG_GRID && x >= 52 && x < 474 && y >= 110 && y < 206) {
                int cw = (466 - 44) / 16, c = (x - 52) / cw;
                uint32_t s = kx_bank() * 16u + (uint32_t)(c < 0 ? 0 : c > 15 ? 15 : c);
                uint32_t lane = drum_track(TSEL) ? (uint32_t)clamp(7 - (y - 110) * 8 / 96, 0, NLANE - 1) : 255u;
                kx_post(KXC_STEP, song.sel, s, lane, 0);
                if (lane < NLANE)
                    kx_post(KXC_LANE, lane, 0, 0, 0);
                kx_post(KXC_CURSOR, 0, 0, 0, (int32_t)s);
            } else if (IS_PED_PAGE(kx_page) && y >= 92 && y < 208) {
                int fi = kx_ped_fi(kx_page), ns = kx_ped_slots(kx_ped_owner(kx_page)), j;
                for (j = 0; j < ns; j++) {
                    int px = kx_ped_x((uint32_t)ns, (uint32_t)j);
                    if (x >= px && x < px + KX_PED_W)
                        kx_ped_focus[fi] = j;
                }
            }
        }
        if (kx_page == PG_FOOT) {                 /* the FOOT PEDAL page: drag the pedal, press the switch */
            int x = VIBE_TOUCH_X(e), y = VIBE_TOUCH_Y(e);
            static int drag;
            if (e->id == VIBE_TOUCH_DOWN)
                drag = x < 190 && y >= 96 && y < 210;
            if (drag && e->id != VIBE_TOUCH_UP)
                kx_foot_set((float)(196 - y) / 90.0f);
            if (e->id == VIBE_TOUCH_UP)
                drag = 0;
            if (x >= 352 && y >= 96 && y < 210 && e->id != VIBE_TOUCH_MOVE)
                kx_foot_switch(e->id == VIBE_TOUCH_DOWN);
        }
        break;
    case VIBE_EV_BUTTON:
        kx_button(e->id, e->down);
        break;
    case VIBE_EV_ENCODER:
        if (e->id < VE_COUNT)
            kx_turn(e->id, e->value);
        break;
    case VIBE_EV_PEDAL:                           /* the foot pedal (the PEDAL jack: a switch or an expression pedal) */
        if (e->id == VIBE_PEDAL_EXPR)
            kx_foot_set(e->value);
        else if (e->id == VIBE_PEDAL_SWITCH)
            kx_foot_switch(e->down);
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
    if (kx_shift_latch && kx_shift_used) {        /* (a tapped SHIFT did its one control) */
        kx_shift = kx_shift_latch = 0;
        kx_shift_used = 0;
    }
}

