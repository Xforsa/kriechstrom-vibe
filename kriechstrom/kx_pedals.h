/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the pedalboard. The FM-1 has one PEDAL (INSERT) a track; the VIBE has the memory and the
 * time for more:
 *   each track   three pedals in a row: the FM-1's own (the project's INSERT) and two more after it
 *   the input    three pedals (a guitar's board: MUFF, SMASH, DAMG ..), before its level and sends
 *   the master   two pedals on the four tracks' and the input's mix, before the shared effects and the master's chain
 * Every pedal is one of KRIECHSTROM's thirteen (fx.c IT_*: SOFT HARD FOLD FUZZ CRUSH PHASR FLANG CHOR MUFF BASS SMASH DAMG
 * INFCT), run by the core's own code (ins_drive, ins_swept, ins_wreck, ins_damage, ins_infect) with a state of its own.
 * The core's hooks: fx.c KRST_EXT_PEDALS (after a track's INSERT) and KRST_EXT_BEGIN (INFCT's sources). The extra
 * pedals are not in the FM-1's project: they are kept in pedals.kxs, beside it. All of it in the audio flow. */

enum { PO_TRK0, PO_IN = NTRK, PO_MASTER, PO_N };   /* the owners: tracks 1..4, the input, the master */
#define PED_SLOTS 3
enum { PD_TYPE, PD_A, PD_B, PD_C, PD_MIX, PD_N };
static int16_t kx_ped[PO_N][PED_SLOTS][PD_N];      /* (a track's slot 0 is the project's INSERT: t->p, not this) */
static ins_t kx_ins_t[NTRK][PED_SLOTS], kx_ins_in[PED_SLOTS][2], kx_ins_m[2][2];
static track_t kx_ped_trk;                         /* (the input's and the master's pedals read their values here) */
static float kx_ped_lvl[PO_N][PED_SLOTS];          /* how much each pedal is doing now (the display's LED) */

static int kx_ped_slots(uint32_t o) { return o == PO_MASTER ? 2 : PED_SLOTS; }

/* one pedal on b with state s: fx.c track_insert, for any state, leaving the track's own INSERT as it is */
static void kx_insert_s(track_t *t, ins_t *s, int32_t *b, uint32_t n)
{
    int32_t ty = clamp(t->p[P_ITYPE], 0, IT_N - 1), tw;
    if (s->type != ty && !s->w)
        s->type = (uint8_t)ty;
    tw = s->type == ty && ty ? ins_mixq(t->p[P_IMIX]) : 0;
    if (!s->w) {
        if (!tw)
            return;
        memset(s, 0, sizeof *s);
        s->type = (uint8_t)ty;
    }
    if (s->type == IT_DAMAGE)
        ins_damage(s, t, b, n, tw);
    else if (s->type == IT_INFECT)
        ins_infect(s, t, b, n, tw);
    else if (s->type >= IT_MUFF)
        ins_wreck(s, t, b, n, tw);
    else if (s->type >= IT_PHASER)
        ins_swept(s, t, b, n, tw);
    else
        ins_drive(s, t, b, n, tw);
}

static void kx_ped_load(track_t *t, const int16_t *v)
{
    t->p[P_ITYPE] = v[PD_TYPE];
    t->p[P_IA] = v[PD_A];
    t->p[P_IB] = v[PD_B];
    t->p[P_IC] = v[PD_C];
    t->p[P_IMIX] = v[PD_MIX];
}

static float kx_ped_meter(const int32_t *b, uint32_t n)
{
    int32_t pk = 0;
    uint32_t i;
    for (i = 0; i < n; i += 4u) {
        int32_t a = b[i] < 0 ? -b[i] : b[i];
        pk = a > pk ? a : pk;
    }
    return (float)pk / 32768.0f;
}

/* fx.c KRST_EXT_PEDALS: a track's two more pedals, after its INSERT */
static void kx_ped_track(track_t *t, int32_t *b, uint32_t n)
{
    uint32_t k = trk_index(t) % NTRK, j;
    int16_t keep[PD_N];
    uint8_t run = t->ins_run;
    keep[0] = t->p[P_ITYPE], keep[1] = t->p[P_IA], keep[2] = t->p[P_IB], keep[3] = t->p[P_IC], keep[4] = t->p[P_IMIX];
    kx_ped_lvl[k][0] = keep[0] && keep[4] ? kx_ped_meter(b, n) : 0.0f;
    for (j = 1; j < PED_SLOTS; j++) {
        if (!kx_ped[k][j][PD_TYPE] && !kx_ins_t[k][j].w) {
            kx_ped_lvl[k][j] = 0.0f;
            continue;
        }
        kx_ped_load(t, kx_ped[k][j]);
        kx_insert_s(t, &kx_ins_t[k][j], b, n);
        kx_ped_lvl[k][j] = kx_ped_meter(b, n);
    }
    kx_ped_load(t, keep);
    t->ins_run = run;
}

/* fx.c KRST_EXT_BEGIN: INFCT on an extra pedal listens to its source too */
static void kx_ped_begin(void)
{
    uint32_t o, j;
    for (o = 0; o < PO_N; o++)
        for (j = 0; j < (uint32_t)kx_ped_slots(o); j++)
            if (kx_ped[o][j][PD_TYPE] == IT_INFECT)
                ifx_listen |= (uint8_t)(1u << clamp(kx_ped[o][j][PD_A], 0, NTRK - 1));
}

/* the input's pedals (kx_input.h, before its level): each side its own state */
static void kx_ped_input(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < PED_SLOTS; j++) {
        if (!kx_ped[PO_IN][j][PD_TYPE] && !kx_ins_in[j][0].w) {
            kx_ped_lvl[PO_IN][j] = 0.0f;
            continue;
        }
        kx_ped_load(&kx_ped_trk, kx_ped[PO_IN][j]);
        kx_insert_s(&kx_ped_trk, &kx_ins_in[j][0], l, n);
        kx_insert_s(&kx_ped_trk, &kx_ins_in[j][1], r, n);
        kx_ped_lvl[PO_IN][j] = kx_ped_meter(l, n);
    }
}

/* the master's pedals, on the dry mix (fx.c mix_l / mix_r) before the buses and the master's chain */
static void kx_ped_master(uint32_t n)
{
    uint32_t j;
    for (j = 0; j < 2u; j++) {
        if (!kx_ped[PO_MASTER][j][PD_TYPE] && !kx_ins_m[j][0].w) {
            kx_ped_lvl[PO_MASTER][j] = 0.0f;
            continue;
        }
        kx_ped_load(&kx_ped_trk, kx_ped[PO_MASTER][j]);
        kx_insert_s(&kx_ped_trk, &kx_ins_m[j][0], mix_l, n);
        kx_insert_s(&kx_ped_trk, &kx_ins_m[j][1], mix_r, n);
        kx_ped_lvl[PO_MASTER][j] = kx_ped_meter(mix_l, n);
    }
}

/* the values a pedal of owner o, slot j has now (a track's slot 0: the project's INSERT) */
static void kx_ped_get(uint32_t o, uint32_t j, int16_t *v)
{
    if (o < NTRK && j == 0u) {
        const track_t *t = &trk[o];
        v[0] = t->p[P_ITYPE], v[1] = t->p[P_IA], v[2] = t->p[P_IB], v[3] = t->p[P_IC], v[4] = t->p[P_IMIX];
    } else
        memcpy(v, kx_ped[o][j], sizeof kx_ped[o][j]);
}
static void kx_ped_set(uint32_t o, uint32_t j, const int16_t *v)
{
    if (o < NTRK && j == 0u) {
        track_t *t = &trk[o];
        static const uint8_t ID[PD_N] = {P_ITYPE, P_IA, P_IB, P_IC, P_IMIX};
        uint32_t f;
        for (f = 0; f < PD_N; f++)
            if (t->p[ID[f]] != v[f]) {
                t->p[ID[f]] = v[f];
                motion_capture(t, ID[f], v[f]);
            }
    } else
        memcpy(kx_ped[o][j], v, sizeof kx_ped[o][j]);
}

/* a field turned (the audio flow, KXC_PED): TYPE steps through the thirteen (a new type: its own defaults) */
static void kx_ped_turn(uint32_t o, uint32_t j, uint32_t f, int32_t d)
{
    int16_t v[PD_N];
    if (o >= PO_N || j >= (uint32_t)kx_ped_slots(o) || f >= PD_N)
        return;
    kx_ped_get(o, j, v);
    if (f == PD_TYPE) {
        int32_t ty = clamp(v[0] + (d > 0 ? 1 : -1), 0, IT_N - 1), k;
        if (ty == v[0])
            return;
        if (!v[0] || ty)
            for (k = 0; k < 3; k++)
                v[1 + k] = ins_desc(ty, (uint32_t)k)->def;
        if (!v[0])
            v[PD_MIX] = 127;
        v[0] = (int16_t)ty;
    } else {
        const param_desc_t *dd = f == PD_MIX ? &TP[P_IMIX] : ins_desc(v[0], f - 1u);
        v[f] = (int16_t)param_turn(dd, v[f], d);
    }
    kx_ped_set(o, j, v);
}

static void kx_ped_clear(void)                     /* (an empty project, the demo song: no extra pedals) */
{
    memset(kx_ped, 0, sizeof kx_ped);
}

/* ---- pedals.kxs: "KXP1", then kx_ped as it is (int16, little-endian) ---------------------------------------------- */
static int16_t kx_ped_wrote[PO_N][PED_SLOTS][PD_N];
static void kx_ped_poll(int now)                   /* the control flow (kx_store.h kx_store_poll) */
{
    static uint8_t b[4 + sizeof kx_ped];
    static int16_t cur[PO_N][PED_SLOTS][PD_N];
    memcpy(cur, kx_ped, sizeof cur);
    if (!memcmp(cur, kx_ped_wrote, sizeof cur))
        return;
    (void)now;
    memcpy(kx_ped_wrote, cur, sizeof cur);
    b[0] = 'K', b[1] = 'X', b[2] = 'P', b[3] = '1';
    memcpy(b + 4, cur, sizeof cur);
    vibe_store_write("pedals.kxs", b, (int)sizeof b);
}
static void kx_ped_boot(void)
{
    static uint8_t b[4 + sizeof kx_ped];
    int n = vibe_store_read("pedals.kxs", b, (int)sizeof b);
    uint32_t o, j, f;
    if (n == (int)sizeof b && b[0] == 'K' && b[1] == 'X' && b[2] == 'P' && b[3] == '1') {
        memcpy(kx_ped, b + 4, sizeof kx_ped);
        for (o = 0; o < PO_N; o++)
            for (j = 0; j < PED_SLOTS; j++) {
                kx_ped[o][j][PD_TYPE] = (int16_t)clamp(kx_ped[o][j][PD_TYPE], 0, IT_N - 1);
                for (f = 1; f < PD_N; f++)
                    kx_ped[o][j][f] = (int16_t)clamp(kx_ped[o][j][f], -64, 127);
            }
    }
    memcpy(kx_ped_wrote, kx_ped, sizeof kx_ped);
}
