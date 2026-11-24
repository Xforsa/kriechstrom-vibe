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
       PG_PEDALS, PG_PEDALS_IN, PG_PEDALS_M, PG_FOOT, PG_SAMPLER, PG_OUTS, PG_PLIB, PG_PFILES, PG_HUB, PG_PADFX };
#define IS_PED_PAGE(pg) ((pg) == PG_PEDALS || (pg) == PG_PEDALS_IN || (pg) == PG_PEDALS_M)
#define IC_PAGE(pg) ((pg) == PG_INPUT ? 0 : (pg) == PG_INPUT2 ? 1 : (pg) == PG_MIDI ? 2 : (pg) == PG_DISPLAY ? 3 : (pg) == PG_FOOT ? 4 : (pg) == PG_OUTS ? 6 : 5)
#define IS_IC_PAGE(pg) ((pg) == PG_INPUT || (pg) == PG_INPUT2 || (pg) == PG_MIDI || (pg) == PG_DISPLAY || (pg) == PG_FOOT || (pg) == PG_SAMPLER || \
                        (pg) == PG_OUTS)
#define PG_MAX 24
static uint8_t kx_pl[PG_MAX], kx_pn;              /* the open family's pages, in order */
static uint8_t kx_fam, kx_fpage[FA_N];            /* the family open, each family's page */
static int kx_page = PG_BROWSE;                   /* the page shown */
static int kx_help, kx_shift, kx_in_asked, kx_slot_pick, kx_boot_done, kx_user_pick;
static int kx_shift_latch, kx_shift_used;         /* SHIFT tapped: it holds for the next control (a mouse has one button) */
static uint64_t kx_shift_t;
static int kx_latch_enc = -1;                     /* a tapped SHIFT turning an encoder: it holds while that one turns */
static uint64_t kx_latch_t;
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
        ADD(PG_HUB);
        ADD(PG_PLIB);
        ADD(PG_PFILES);
        ADD(PG_PROJECT);
        ADD(PG_MIXER);
        ADD(PG_OUTS);
        ADD(PG_MIDI);
        ADD(PG_FOOT);
        ADD(PG_DISPLAY);
        ADD(PG_PADFX);
        i = (uint32_t)kx_page_index("CLOCK");
        ADD(i);
        break;
    }
#undef ADD
}

/* MENU's first page: every page of MENU as a tile (kx_draw_hub); ENC 1 or DATA picks, ENTER or a click opens */
static int kx_hub_sel;
static int kx_data_card;                          /* the card DATA turns on a page of values (0..3) */
static uint64_t kx_data_t;
#define KX_BACK 8
static int16_t kx_back[KX_BACK][3];               /* the pages RETURN goes back through: page, family, its index */
static int kx_back_n;
#define KX_HUB_N 10
static const char *const KX_HUB_NAME[KX_HUB_N] = {"PROJECTS", "FILES", "PARTS A-D", "MIXER", "OUTPUTS", "MIDI & CV",
                                                 "FOOT PEDAL", "DISPLAY", "PAD FX", "CLOCK"};
static int kx_pfx_sel;                            /* PAD FX: the slot chosen (0-7 pads 9-16, 8-15 the same with SHIFT) */
static uint64_t kx_pfx_hit[16];                   /* (when a slot's pad was last pressed: it flashes) */
/* what each hold effect does, in a line (PAD FX) */
static const char *const PF_WHAT[PF_NFX] = {
    "repeats the last 1/8 while held", "repeats the last 1/16", "repeats the last 1/32: a buzz",
    "the last half beat, backwards", "a low-pass closes", "a high-pass opens", "the tape slows to a stop",
    "holds the sound as it is", "an octave up", "an octave down", "a flanger", "a phaser",
    "throws into the reverb", "throws into the echo", "a half beat rolled", "chops it into 1/32",
    "bits crushed, sweeping", "a new effect every 1/16"};
static void kx_open(uint32_t fa, int next);
static void kx_menu(void)                         /* MENU: always its tiles (the page it was on is one RETURN away) */
{
    kx_fpage[FA_MENU] = 0;
    kx_open(FA_MENU, 0);
}
static void kx_hub_open(int i)                    /* the tile's page: MENU's page 1 + i */
{
    kx_fam = FA_MENU;
    kx_fpage[FA_MENU] = (uint8_t)(1 + clamp(i, 0, KX_HUB_N - 1));
    kx_open(FA_MENU, 0);
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
    case PG_PROJECT: return "PARTS A-D";
    case PG_INPUT: return "INPUT";
    case PG_INPUT2: return "INPUT SENDS";
    case PG_MIDI: return "MIDI OUT & CV";
    case PG_USER: return "MY SOUNDS";
    case PG_DISPLAY: return "DISPLAY";
    case PG_FOOT: return "FOOT PEDAL";
    case PG_SAMPLER: return "SAMPLER";
    case PG_OUTS: return "OUTPUTS";
    case PG_PLIB: return "PROJECTS";
    case PG_HUB: return "MENU";
    case PG_PADFX: return "PAD FX";
    case PG_PFILES: return "PROJECT FILES";
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
    if (IS_IC_PAGE(kx_page) || kx_page == PG_USER || kx_page == PG_PLIB || kx_page == PG_PFILES || kx_page == PG_HUB ||
        kx_page == PG_PADFX)
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
static const uint8_t IN_SLOTS[7][4] = {{IC_LEVEL, IC_DRIVE, IC_GATE, IC_FEED}, {IC_PAN, IC_CHO, IC_DLY, IC_REV},
                                       {IC_MOUT, IC_MCH, IC_CLK, IC_CV}, {IC_SCREEN, IC_LIGHT, 255, 255},
                                       {IC_EXPR, IC_FSW, 255, 255}, {IC_SSRC, IC_SBARS, IC_SSLOT, 255},
                                       {IC_XOUT, 255, 255, 255}};
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
        else if (f == IC_MOUT || f == IC_CLK || f == IC_XOUT || f == IC_LIGHT)
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
    if (pg == PG_PADFX) {                         /* PAD FX: the pad, its effect */
        int fx = clamp(kx_ic[IC_PF0 + kx_pfx_sel], 0, PF_NFX - 1);
        memset(c, 0, sizeof *c);
        c->unit = "";
        if (slot == 0u) {
            c->on = 1, c->label = kx_pfx_sel >= 8 ? "SHIFT + PAD" : "PAD";
            vg_itoa(9 + (kx_pfx_sel & 7), c->val);
            c->pos = (float)kx_pfx_sel / 15.0f;
        } else if (slot == 1u) {
            c->on = 1, c->label = "EFFECT";
            str_cpy(c->val, PF_CELL[fx], sizeof c->val);
            c->pos = (float)fx / (float)(PF_NFX - 1);
        }
        return;
    }
    if (pg == PG_HUB) {                           /* MENU: the tile */
        memset(c, 0, sizeof *c);
        if (slot == 0u) {
            c->on = 1, c->label = "PAGE", c->unit = "";
            str_cpy(c->val, KX_HUB_NAME[kx_hub_sel], sizeof c->val);
            c->pos = (float)kx_hub_sel / (float)(KX_HUB_N - 1);
        }
        return;
    }
    if (pg == PG_PLIB || pg == PG_PFILES) {       /* MY PROJECTS: the entry, the name's letter; PROJECT FILES: the file */
        static const char *const L[3] = {"PROJECT", "LETTER", "CHANGE"};
        memset(c, 0, sizeof *c);
        c->unit = "";
        if (pg == PG_PFILES) {
            if (slot == 0u) {
                c->on = 1, c->label = "FILE";
                vg_itoa(kxp_nlist ? kxp_lsel + 1 : 0, c->val);
                c->unit = "";
                c->pos = kxp_nlist > 1 ? (float)kxp_lsel / (float)(kxp_nlist - 1) : 0.0f;
            }
            return;
        }
        if (slot > 2u)
            return;
        c->on = 1;
        c->label = L[slot];
        if (slot == 0u) {
            c->val[0] = (char)('0' + (kxp_sel + 1) / 10), c->val[1] = (char)('0' + (kxp_sel + 1) % 10), c->val[2] = 0;
            c->pos = (float)kxp_sel / (float)(KXP_MAX - 1);
        } else if (slot == 1u) {
            vg_itoa(kxp_cur + 1, c->val);
            c->pos = (float)kxp_cur / 22.0f;
        } else {
            char ch = kxp_edit[kxp_cur] ? kxp_edit[kxp_cur] : ' ';
            c->val[0] = ch == ' ' ? '_' : ch, c->val[1] = 0;
        }
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
    if (p->scope == SC_STEP && p->title[0] == 'D') {   /* DETAIL: the cursor step's CHANCE RATCH NUDGE VEL (ui_draw.c) */
        static const char *const SD_LABEL[4] = {"CHANCE", "RATCH", "NUDGE", "VEL"};
        const track_t *t = TSEL;
        const step_t *st = &t->step[ui.cursor % NSTEP];
        uint32_t k = SD_KIND[slot & 3u];
        int32_t v = ev_sval(st, k);
        c->on = sd_takes(st, k);
        c->label = SD_LABEL[slot & 3u];
        c->unit = "";
        if (k == EVK_CHANCE) {
            c->unit = ev_chance_fmt(c->val, v);
            if (v > 100)
                c->label = "COND";
            c->pos = v > 100 ? (float)(v - 100) / (float)NCOND : (float)v / 100.0f;
        } else if (k == EVK_RATCH) {
            c->val[0] = 'x';
            vg_itoa(v, c->val + 1);
            c->pos = (float)(v - 1) / 3.0f;
        } else if (k == EVK_NUDGE) {
            ev_nudge_fmt(c->val, v);
            c->unit = v ? "/16" : "";
            c->bip = 1;
            c->pos = (float)(v + 8) / 15.0f;
        } else {
            vg_itoa(v, c->val);
            c->unit = st->flags & SF_ACCENT ? "ACC" : "";
            c->pos = (float)v / 127.0f;
        }
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
    if (p->graph == GR_MOD && slot == 0u) {       /* MOD: KNOB 1 picks the slot (ui_draw.c) */
        c->on = 1;
        c->label = "SLOT";
        vg_itoa((int)mod_ui_slot + 1, c->val);
        c->unit = "/4";
        c->pos = (float)mod_ui_slot / 3.0f;
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

/* does DATA turn a card on this page (not: SOUND's sounds, the steps, the lists, the rows, the pedals, the mixer)? */
static int kx_data_on_cards(void)
{
    int pg = kx_page;
    if (pg == PG_BROWSE || pg == PG_MIXER || pg == PG_GRID || pg == PG_HUB || pg == PG_PLIB || pg == PG_PFILES || pg == PG_PADFX ||
        pg == PG_PROJECT || pg == PG_USER || IS_PED_PAGE(pg) || kx_knob_page() < 0)
        return 0;
    if (pg >= 0 && pg < (int)NPAGES && (PAGES[pg].scope == SC_STEP || PAGES[pg].graph == GR_SONG ||
                                        PAGES[pg].graph == GR_EVENTS || PAGES[pg].graph == GR_MOD ||
                                        fop_page(PAGES[pg].graph)))
        return 0;
    return 1;
}
static void kx_data_fix(void)                     /* (the focus on a card that has a value) */
{
    int k;
    for (k = 0; k < 4; k++) {
        kx_card c;
        kx_card_of(kx_knob_page(), (uint32_t)((kx_data_card + k) & 3), &c);
        if (c.on && c.label && c.label[0]) {
            kx_data_card = (kx_data_card + k) & 3;
            return;
        }
    }
}

/* ---- the controls ----------------------------------------------------------------------------------------------- */
static uint8_t kx_pad_fx[8];                      /* the effect a pad 9-16 started (let go: that one ends) */
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
    case 0:                                       /* the tracks: mute; SHIFT: solo (again: all back) */
        if (down && !kx_shift)
            kx_post(KXC_MUTE, col, 0, 0, 0);
        else if (down) {
            uint32_t k, alone = !trk[col].p[P_MUTE];
            for (k = 0; k < NTRK; k++)
                alone &= k == col || trk[k].p[P_MUTE] != 0;
            for (k = 0; k < NTRK; k++)
                kx_post(KXC_MUTE, k, alone || k == col ? 2u : 1u, 0, 0);
        }
        break;
    case 1:
        if (!down)
            break;
        if (kx_page == PG_MASTER) {               /* MASTER: DAMAGE WALL SPLICE HAUNT on and off (as they were) */
            static const uint8_t G[4] = {G_DMG, G_WALL, G_SPLICE, G_HAUNT};
            static int16_t kept[4] = {64, 64, 64, 1};
            int16_t v = song.g[G[col]];
            if (v)
                kept[col] = v;
            kx_post(KXC_PARAM, NTRK, G[col], 0, v ? 0 : kept[col]);
        } else if (IS_PED_PAGE(kx_page)) {        /* a pedalboard: pedal 1-3 bypassed and back, pad 4 all */
            uint32_t o = kx_ped_owner(kx_page), ns = (uint32_t)kx_ped_slots(o), j;
            static int16_t kept[PO_N][PED_SLOTS];
            for (j = 0; j < ns; j++)
                if (col == j || col == 3u) {
                    int16_t v[PD_N];
                    kx_ped_get(o, j, v);
                    if (!v[PD_TYPE])
                        continue;
                    if (v[PD_MIX])
                        kept[o][j] = v[PD_MIX];
                    kx_post(KXC_PEDSET, o, j, PD_MIX, v[PD_MIX] ? 0 : (kept[o][j] ? kept[o][j] : 127));
                }
        } else                                    /* DICE: a new part (SHIFT: OOPS, a few of its values at random) */
            kx_post(kx_shift ? KXC_OOPS : KXC_TOOL, col, TL_DICE, 0, 0);
        break;
    default: {                                    /* pads 9-16: the effects MENU > PAD FX gives them, while held (SHIFT: eight more) */
        uint32_t slot = (row - 2u) * 4u + col + (kx_shift ? 8u : 0u), k = id - 8u;
        if (down) {
            kx_pad_fx[k] = (uint8_t)clamp(kx_ic[IC_PF0 + slot], 0, PF_NFX - 1);
            kx_pfx_hit[slot] = vibe_frames() ? vibe_frames() : 1;
            if (kx_page == PG_PADFX)
                kx_pfx_sel = (int)slot;           /* (on PAD FX: the pad pressed is the one to choose for) */
        }
        kx_post(KXC_PERF, kx_pad_fx[k], down ? 1u : 0u, 0, 0);
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
        if (kx_page == PG_PADFX)                  /* PAD FX: the chosen pad's own effect again */
            kx_post(KXC_INCFG, IC_PF0 + (uint32_t)kx_pfx_sel, 1, 0, IC_DESC[IC_PF0 + kx_pfx_sel].def);
        else if (kx_page == PG_PROJECT)
            kx_post(KXC_SLOT, (uint32_t)kx_slot_pick, kx_shift ? 2u : 1u, 0, 0);
        else if (kx_page == PG_PLIB) {
            if (kx_shift)
                kxp_export(kxp_sel);
            else
                kxp_save(kxp_sel);
        } else if (kx_page == PG_PFILES) {
            if (!kxp_src[0] && vibe_media_add(0) == 1)
                vibe_media_add(1);                 /* (the browser: a folder for the files) */
            kxp_list_want = 1;
        }
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
    case FA_MENU:
        if (kx_page == PG_PLIB)
            return kx_shift ? "EXPORT" : "SAVE";
        if (kx_page == PG_PFILES)
            return kxp_src[0] ? "REFRESH" : "FOLDER";
        if (kx_page == PG_PADFX)
            return "DEFAULT";
        return kx_page == PG_PROJECT ? (kx_shift ? "NEW" : "SAVE") : "";
    case FA_FX:
        if (IS_PED_PAGE(kx_page))
            return "NEXT";
        return kx_page == PG_INPUT || kx_page == PG_INPUT2 ? IC_MODE_N[kx_ic[IC_MODE] % 3] : "";
    default: return "";
    }
}

static void kx_pfx_turn(int n)                    /* PAD FX: the chosen pad's effect, round the 18 */
{
    int fx = (kx_ic[IC_PF0 + kx_pfx_sel] + (n > 0 ? 1 : -1) + PF_NFX) % PF_NFX;
    kx_post(KXC_INCFG, IC_PF0 + (uint32_t)kx_pfx_sel, 1, 0, fx);
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
        kx_data_card = (int)enc;                  /* (DATA turns the card last turned) */
        if (IS_PED_PAGE(kx_page)) {
            uint32_t f = enc == VE_1 ? (kx_shift ? PD_MIX : PD_TYPE) : PD_A + (enc - VE_2);
            kx_post(KXC_PED, kx_ped_owner(kx_page), (uint32_t)kx_ped_focus[kx_ped_fi(kx_page)], f,
                    f == PD_TYPE ? (n > 0 ? 1 : -1) : n);
        } else if (kx_page == PG_PADFX) {       /* ENC 1 the pad, ENC 2 its effect */
            if (enc == VE_1)
                kx_pfx_sel = clamp(kx_pfx_sel + (n > 0 ? 1 : -1), 0, 15);
            else if (enc == VE_2)
                kx_pfx_turn(n);
        } else if (kx_page == PG_HUB) {
            if (enc == VE_1)
                kx_hub_sel = clamp(kx_hub_sel + (n > 0 ? 1 : -1), 0, KX_HUB_N - 1);
        } else if (kx_page == PG_PLIB) {         /* the entry; the name: which letter, which character */
            static const char CH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_#!";
            if (enc == VE_1)
                kxp_pick(kxp_sel + n);
            else if (enc == VE_2)
                kxp_cur = clamp(kxp_cur + (n > 0 ? 1 : -1), 0, 22);
            else if (enc == VE_3) {
                int i, len = str_len(kxp_edit), at = 0, nc = (int)sizeof CH - 1;
                for (i = len; i < kxp_cur; i++)    /* (past the end: spaces up to here) */
                    kxp_edit[i] = ' ';
                kxp_edit[kxp_cur > len ? kxp_cur + 1 : len > kxp_cur ? len : kxp_cur + 1] = 0;
                for (i = 0; i < nc; i++)
                    if (CH[i] == kxp_edit[kxp_cur])
                        at = i;
                kxp_edit[kxp_cur] = CH[((at + (n > 0 ? 1 : -1)) % nc + nc) % nc];
                for (i = str_len(kxp_edit) - 1; i > 0 && kxp_edit[i] == ' '; i--)
                    kxp_edit[i] = 0;               /* (no trailing spaces) */
            }
        } else if (kx_page == PG_PFILES) {
            if (enc == VE_1)
                kxp_lsel = clamp(kxp_lsel + (n > 0 ? 1 : -1), 0, kxp_nlist ? kxp_nlist - 1 : 0);
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
        if (kx_shift && kx_fam == FA_SOUND && kx_page != PG_USER && kx_page != PG_SAMPLER) {
            kx_post(KXC_ENGINE, 0, 0, 0, n > 0 ? 1 : -1);   /* SHIFT + DATA on SOUND's pages: the track's engine */
            return;
        }
        if (kx_page == PG_GRID || (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].scope == SC_STEP)) {
            int32_t len = clamp(TSEL->p[P_SLEN], 1, NSTEP), c = ((int32_t)ui.cursor + n % len + len) % len;
            kx_post(KXC_CURSOR, 0, 0, 0, c);
            kx_view_bank = c / 16;
            kx_view_t = vibe_frames();
        } else if (kx_page == PG_HUB) {
            kx_hub_sel = clamp(kx_hub_sel + (n > 0 ? 1 : -1), 0, KX_HUB_N - 1);
        } else if (kx_page == PG_PADFX) {         /* PAD FX: DATA chooses the effect */
            kx_pfx_turn(n);
        } else if (kx_page >= 0 && kx_page < (int)NPAGES &&
                   (PAGES[kx_page].graph == GR_SONG || PAGES[kx_page].graph == GR_EVENTS ||
                    PAGES[kx_page].graph == GR_MOD || fop_page(PAGES[kx_page].graph))) {
            kx_turn(VE_1, n > 0 ? 1.0f : -1.0f);   /* (a page whose ENC 1 picks a row: DATA does it too) */
        } else if (kx_page == PG_PLIB) {          /* the lists: DATA walks them */
            kxp_pick(kxp_sel + (n > 0 ? 1 : -1));
        } else if (kx_page == PG_PFILES) {
            kxp_lsel = clamp(kxp_lsel + (n > 0 ? 1 : -1), 0, kxp_nlist ? kxp_nlist - 1 : 0);
        } else if (kx_page == PG_PROJECT) {
            kx_slot_pick = clamp(kx_slot_pick + (n > 0 ? 1 : -1), 0, 3);
        } else if (kx_page == PG_USER) {
            kx_user_pick = clamp(kx_user_pick + (n > 0 ? 1 : -1), 0, UP_SLOTS - 1);
        } else if (IS_PED_PAGE(kx_page)) {        /* DATA: the pedal in focus */
            int fi = kx_ped_fi(kx_page), ns = kx_ped_slots(kx_ped_owner(kx_page));
            kx_ped_focus[fi] = clamp(kx_ped_focus[fi] + (n > 0 ? 1 : -1), 0, ns - 1);
            kx_card_hot[0] = kx_card_hot[1] = kx_card_hot[2] = kx_card_hot[3] = vibe_frames();
        } else if (kx_page == PG_BROWSE) {        /* SOUND: the sounds (SHIFT: the engines) */
            if (kx_shift)
                kx_post(KXC_ENGINE, 0, 0, 0, n > 0 ? 1 : -1);
            else
                kx_post(KXC_PRESET, 0, 0, 0, n);
        } else if (kx_page == PG_MIXER) {         /* MIXER: the selected track's level (SHIFT: its pan) */
            kx_turn(VE_1 + (int)song.sel, n > 0 ? 1.0f : -1.0f);
        } else if (kx_data_on_cards()) {          /* any other page: the card in focus, as its own encoder */
            kx_data_fix();
            kx_turn(VE_1 + kx_data_card, (float)n);
            kx_data_t = vibe_frames();
        }
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
                kx_latch_enc = -1;
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
    case VB_HOME:                                 /* HOME: this page's controls; SHIFT + HOME: MENU (the plugin's Rack
                                                   * view has no MENU key) */
        if (kx_shift)
            kx_help = 0, kx_menu();
        else
            kx_help = !kx_help;
        break;
    case VB_RETURN:                               /* back: to the page before (none: SOUND) */
        if (kx_help)
            kx_help = 0;
        else if (kx_back_n > 0) {
            kx_back_n--;
            kx_page = kx_back[kx_back_n][0];
            kx_fam = (uint8_t)kx_back[kx_back_n][1];
            kx_fpage[kx_fam] = (uint8_t)kx_back[kx_back_n][2];
        } else
            kx_open(FA_SOUND, 0), kx_fpage[FA_SOUND] = 0, kx_page = PG_BROWSE;
        break;
    case VB_K1: case VB_K2: case VB_K3: case VB_K4: case VB_K5: case VB_K6:
        kx_open(b - VB_K1, 1);
        break;
    case VB_MENU:                                 /* (SHIFT: straight to PROJECTS) */
        if (kx_shift)
            kx_hub_open(0);
        else
            kx_menu();
        break;
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
        else if (kx_page == PG_HUB)
            kx_hub_open(kx_hub_sel);
        else if (kx_page == PG_PADFX)
            kx_pfx_sel = (kx_pfx_sel + 1) & 15;   /* (ENTER: the next pad, DATA chooses again) */
        else if (kx_page == PG_PLIB) {            /* LOAD (SHIFT: DELETE): pressed twice, within 3 s */
            static uint64_t armed;
            static int what = -1;
            int w = kxp_sel * 2 + (kx_shift ? 1 : 0);
            if (!kxp_name[kxp_sel][0])
                kxp_say("EMPTY: F5 SAVES HERE");
            else if (what == w && vibe_frames() - armed < (uint64_t)(kx_sr * 3.0f)) {
                if (kx_shift)
                    kxp_delete(kxp_sel);
                else
                    kxp_load(kxp_sel);
                what = -1;
            } else {
                what = w, armed = vibe_frames();
                kxp_say(kx_shift ? "AGAIN TO DELETE" : "AGAIN TO LOAD");
            }
        } else if (kx_page == PG_PFILES) {
            if (kxp_nlist)
                kxp_import(kxp_list[kxp_lsel]);
        }
        else if (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].graph == GR_EVENTS)
            kx_post(KXC_ACT, (uint32_t)kx_page, 0, 0, 0);
        else if (kx_data_on_cards()) {
            int k;                                /* (no ENTER of its own: DATA's focus to the next card that has a value) */
            for (k = 1; k <= 4; k++) {
                kx_card c;
                kx_card_of(kx_knob_page(), (uint32_t)((kx_data_card + k) & 3), &c);
                if (c.on && c.label && c.label[0]) {
                    kx_data_card = (kx_data_card + k) & 3;
                    break;
                }
            }
            kx_data_t = vibe_frames();
        }
        break;
    default: break;
    }
}

static void kx_ui_init(void)
{
    vf_enable(0);                                 /* (no SDK FOCUS: DATA and ENTER are each page's own, held or not) */
    vibe_device(&kx_dev);
    kx_open(FA_SOUND, 0);
    kx_start_sel = kx_had_session ? 0 : 1;
}

/* ---- clicks into the display (SDK 18: the plugin's and the browser's window) -----------------------------------------
 * The host keeps the VIBE FM layout's zones: the soft keys (y 282-320) are F1-F6, and on the four knob columns
 * (x 10-470, 115 px each, y 40-282) a drag up or down turns encoder 1-4 of that column; a plain click anywhere comes here.
 * What a click does is what is under it: a family's tab, a track, a page's dot, an entry of a list, a slot, a pedal, a
 * step, a master effect (its page), one of DISPLAY's three wears, a row of the start screen. */
static int kx_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static void kx_goto(uint32_t fa, const char *title)   /* a page of a family, by its title */
{
    uint32_t i;
    kx_family_pages(fa);
    for (i = 0; i < kx_pn; i++)
        if (kx_pl[i] < NPAGES && kx_same(PAGES[kx_pl[i]].title, title)) {
            kx_fam = (uint8_t)fa;
            kx_fpage[fa] = (uint8_t)i;
            kx_page = kx_pl[i];
            return;
        }
}
static void kx_touch(int x, int y)
{
    if (kx_help) {                                /* HOME's overlay: a click closes it */
        kx_help = 0;
        return;
    }
    if (y < 28) {                                 /* the header: the logo HOME, the families, PLAY / STOP */
        static const char *const K[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
        int tx = 120, i;
        if (x < 112) {
            kx_help = 1;
            return;
        }
        if (x >= 380) {
            kx_post(KXC_PLAY, 3, 0, 0, 0);
            return;
        }
        for (i = 0; i < 6; i++) {
            int w = vg_text_width(K[i], VG_SMALL);
            if (x >= tx - 6 && x < tx + w + 6) {
                kx_open((uint32_t)i, 1);
                return;
            }
            tx += w + 12;
        }
        if (x >= tx - 6)
            kx_menu();
        return;
    }
    if (y < 76) {                                 /* the four tracks: select */
        int k = (x - 8) / 118;
        if (k >= 0 && k < NTRK)
            kx_post(KXC_SEL, (uint32_t)k, 0, 0, 0);
        return;
    }
    if (y < 93) {                                 /* the title line: a page's dot (or the next page) */
        uint32_t i;
        kx_family_pages(kx_fam);
        for (i = 0; i < kx_pn && kx_pn > 1u; i++) {
            int dx = 470 - (int)(kx_pn - 1u - i) * 10;
            if (x >= dx - 5 && x < dx + 5) {
                kx_fpage[kx_fam] = (uint8_t)i;
                kx_page = kx_pl[i];
                return;
            }
        }
        kx_page_step(1);
        return;
    }
    if (y >= 206)                                 /* the cards: theirs is the drag (the host turns the encoder) */
        return;
    if (kx_page == PG_GRID || (kx_page >= 0 && kx_page < (int)NPAGES && PAGES[kx_page].scope == SC_STEP)) {
        if (x >= 52 && x < 474 && y >= 110) {     /* a step (and a drum lane) on or off */
            int cw = (466 - 44) / 16, c = (x - 52) / cw;
            uint32_t s = kx_bank() * 16u + (uint32_t)(c < 0 ? 0 : c > 15 ? 15 : c);
            uint32_t lane = drum_track(TSEL) ? (uint32_t)clamp(7 - (y - 110) * 8 / 96, 0, NLANE - 1) : 255u;
            if (kx_page == PG_GRID)
                kx_post(KXC_STEP, song.sel, s, lane, 0);
            if (lane < NLANE)
                kx_post(KXC_LANE, lane, 0, 0, 0);
            kx_post(KXC_CURSOR, 0, 0, 0, (int32_t)s);
        }
    } else if (IS_PED_PAGE(kx_page)) {            /* a pedal: the one the knobs turn */
        int fi = kx_ped_fi(kx_page), ns = kx_ped_slots(kx_ped_owner(kx_page)), j;
        for (j = 0; j < ns; j++) {
            int px = kx_ped_x((uint32_t)ns, (uint32_t)j);
            if (x >= px && x < px + KX_PED_W)
                kx_ped_focus[fi] = j;
        }
    } else if (kx_page == PG_BROWSE && x >= 336) {   /* a sound of the list */
        uint32_t total, cur = preset_pos(&total), r = (uint32_t)((y - 95) / 17);
        if (total && r < 7u) {
            int32_t d = (int32_t)preset_visible(cur, total, r) - (int32_t)cur;
            if (d)
                kx_post(KXC_PRESET, 0, 0, 0, d);
        }
    } else if (kx_page == PG_MASTER && y >= 148) {   /* an effect of the chain: its page */
        static const char *const P[6] = {"DAMAGE", "DAMAGE", "RESONATE", "SPLICE", "HAUNT", "DAMAGE"};
        int i = (x - 4) / ((472 - 20) / 6 + 4);
        kx_goto(FA_FX, P[clamp(i, 0, 5)]);
    } else if (kx_page == PG_MIXER) {             /* a meter: its track */
        int k = (x - 4) / 118;
        if (k >= 0 && k < NTRK)
            kx_post(KXC_SEL, (uint32_t)k, 0, 0, 0);
    } else if (kx_page == PG_PADFX && y >= 108 && y < 190) {   /* a pad of the two maps (kx_draw_padfx) */
        int half = x >= 240, col = ((x - 4) % 236) / 58, row = (y - 108) / 41;
        if (col >= 0 && col < 4 && row >= 0 && row < 2)
            kx_pfx_sel = half * 8 + row * 4 + col;
    } else if (kx_page == PG_HUB) {               /* a tile: its page */
        int col = (x - 4) / 95, row = (y - 96) / 52, i = row * 5 + col;   /* (kx_draw_hub: 89 + 6, 46 + 6) */
        if (col >= 0 && col < 5 && row >= 0 && row < 2 && i < KX_HUB_N)
            kx_hub_open(i);
    } else if (kx_page == PG_PROJECT && y >= 150) {
        kx_slot_pick = clamp((x - 4) / 118, 0, 3);
    } else if (kx_page == PG_PLIB && y >= 130) {
        int col = (x - 4) / 118, row = (y - 130) / 19;
        if (col >= 0 && col < 4 && row >= 0 && row < 4)
            kxp_pick(kxp_sel / 16 * 16 + col * 4 + row);
    } else if (kx_page == PG_PFILES && y >= 114) {
        int r = (y - 114) / 15;
        if (r >= 0 && r < 6 && kxp_lsel / 6 * 6 + r < kxp_nlist)
            kxp_lsel = kxp_lsel / 6 * 6 + r;
    } else if (kx_page == PG_USER) {
        int col = (x - 8) / 116, row = (y - 96) / 13;
        if (col >= 0 && col < 4 && row >= 0 && row < 8)
            kx_user_pick = col * 8 + row;
    } else if (kx_page == PG_DISPLAY) {           /* one of the three wears */
        int k = (x - 12) / 154;
        if (k >= 0 && k < 3)
            kx_post(KXC_INCFG, IC_SCREEN, 1, 0, k);
    } else if (kx_page == PG_FOOT) {              /* the pedal: as far down as clicked; the switch */
        if (x < 190)
            kx_foot_set((float)(186 - y) / 80.0f);
        else if (x >= 352)
            kx_foot_switch(1), kx_foot_switch(0);
    } else if (kx_page == PG_SAMPLER && x < 52 && y >= 116 && y < 166)
        kx_soft(4);                               /* (the big REC: as F5) */
}

static void kx_ui_event_body(const vibe_event *e);
static void kx_ui_event(const vibe_event *e)
{
    if (e->type <= VIBE_EV_ENCODER || e->type == VIBE_EV_NOTE || e->type == VIBE_EV_PEDAL || e->type == VIBE_EV_TOUCH)
        kx_idle_t = vibe_frames() ? vibe_frames() : 1;
    if (e->type == VIBE_EV_BUTTON || e->type == VIBE_EV_PAD || e->type == VIBE_EV_ENCODER || e->type == VIBE_EV_NOTE ||
        e->type == VIBE_EV_TOUCH)
        kx_boot_done = 1;                         /* (the power-on picture ends at the first touch) */
    if (kx_start && kx_boot_done && e->type == VIBE_EV_BUTTON && e->down) {   /* the start screen */
        uint32_t b = e->id;
        int pick = -1;
        if (b == VB_F1 || b == VB_F2 || b == VB_F3 || b == VB_F4)
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
            else if (pick == 3)
                kx_hub_open(0);                   /* (one of my projects: PROJECTS, to pick and load) */
            kx_start = 0;
            return;
        }
    }
    if (kx_start && kx_boot_done && e->type == VIBE_EV_TOUCH && e->id == VIBE_TOUCH_DOWN) {   /* a row clicked */
        int ty = VIBE_TOUCH_Y(e), pick = ty >= 76 && ty < 252 ? (ty - 76) / 44 : -1;
        if (pick == 1)
            kx_post(KXC_DEMO, 0, 0, 0, 0);
        else if (pick == 2)
            kx_post(KXC_SLOT, 0, 2, 0, 0);
        else if (pick == 3)
            kx_hub_open(0);
        if (pick >= 0)
            kx_start = 0;
        return;
    }
    if (kx_start && kx_boot_done && e->type == VIBE_EV_ENCODER && e->id == VE_DATA) {
        kx_start_sel = (kx_start_sel + (e->value > 0 ? 1 : 3)) % 4;
        return;
    }
    if (kx_start && kx_boot_done && (e->type == VIBE_EV_PAD || e->type == VIBE_EV_NOTE ||
                                     (e->type == VIBE_EV_BUTTON && e->id == VB_PLAY)))
        kx_start = 0;                             /* (played instead: what is there now stays) */
    {   /* remember the page this event leaves, for RETURN */
        int pg0 = kx_page, fa0 = kx_fam, fp0 = kx_fpage[kx_fam];
        int ret = e->type == VIBE_EV_BUTTON && e->id == VB_RETURN;
        kx_ui_event_body(e);
        if (kx_page != pg0)
            kx_data_card = 0;                     /* (a new page: DATA on its first card) */
        if (!ret && kx_page != pg0) {
            if (kx_back_n == KX_BACK) {
                memmove(kx_back[0], kx_back[1], sizeof kx_back[0] * (KX_BACK - 1));
                kx_back_n--;
            }
            kx_back[kx_back_n][0] = (int16_t)pg0, kx_back[kx_back_n][1] = (int16_t)fa0, kx_back[kx_back_n][2] = (int16_t)fp0;
            kx_back_n++;
        }
        return;
    }
}
static void kx_ui_event_body(const vibe_event *e)
{
    if (kx_shift_latch && kx_latch_enc >= 0 &&
        !(e->type == VIBE_EV_ENCODER && (int)e->id == kx_latch_enc && vibe_frames() - kx_latch_t < (uint64_t)(kx_sr * 0.6f)) &&
        !(e->type == VIBE_EV_BUTTON && e->id == VB_SHIFT))
        kx_shift = kx_shift_latch = 0, kx_shift_used = 0, kx_latch_enc = -1;   /* (the turn ended: SHIFT is let go) */
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
    case VIBE_EV_TOUCH:                           /* a click into the display (plugin, browser): kx_touch */
        if (e->id == VIBE_TOUCH_DOWN)
            kx_touch(VIBE_TOUCH_X(e), VIBE_TOUCH_Y(e));
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
    if (kx_shift_latch && kx_shift_used) {        /* (a tapped SHIFT did its one control; an encoder: its whole turn) */
        if (e->type == VIBE_EV_ENCODER)
            kx_latch_enc = (int)e->id, kx_latch_t = vibe_frames();
        else {
            kx_shift = kx_shift_latch = 0;
            kx_shift_used = 0;
            kx_latch_enc = -1;
        }
    }
}

