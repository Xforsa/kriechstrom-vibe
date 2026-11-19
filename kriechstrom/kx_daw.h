/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the DAW. Parameters it automates (the mixer by the SDK's convention, each track's sound,
 * envelope and sends, the master's effects), its transport and tempo.
 * A parameter the DAW moves comes as VIBE_EV_PARAM (control flow) and goes to the core as a command; what changes in
 * the core by other means (the encoders, a sound load, DICE, MIDI) goes back to the DAW from the audio flow, once a
 * block: kx_dawv holds the value last agreed with the DAW, so its own moves never come back to it. */

enum { DW_MIX_LEVEL, DW_MIX_PAN, DW_MIX_MUTE, DW_E0, DW_E7 = DW_E0 + 7, DW_ATK, DW_DEC, DW_SUS, DW_REL, DW_DIST, DW_CHO,
       DW_DLY, DW_REV, DW_PER };                  /* per track */
#define DW_TRK (NTRK * DW_PER)
enum { DW_SEL = DW_TRK, DW_DMG, DW_DMGC, DW_WALL, DW_HISS, DW_RES, DW_SPLICE, DW_HAUNT, DW_HAMT, DW_SWING, DW_N };

static vibe_param kx_params[DW_N];
static const char *kx_groups[DW_N];
static float kx_dawv[DW_N];                       /* the audio flow's */
static char kx_pid[DW_N][16], kx_pname[DW_N][16];
static const char *const KX_ONOFF[2] = {"OFF", "ON"};
static const char *const KX_TRACKS[NTRK] = {"T1", "T2", "T3", "T4"};

static const uint8_t DW_TP[DW_PER] = {P_LEVEL, P_PAN, P_MUTE, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
                                      P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV};
static const uint8_t DW_G[DW_N - DW_DMG] = {G_DMG, G_DMGC, G_WALL, G_HISS, G_RES, G_SPLICE, G_HAUNT, G_HAMT, G_SWING};

static void kx_pset(uint32_t j, const char *id, const char *name, float mn, float mx, float def, int steps,
                    const char *const *ch, const char *group)
{
    str_cpy(kx_pid[j], id, sizeof kx_pid[j]);
    str_cpy(kx_pname[j], name, sizeof kx_pname[j]);
    kx_params[j].id = kx_pid[j];
    kx_params[j].name = kx_pname[j];
    kx_params[j].min = mn;
    kx_params[j].max = mx;
    kx_params[j].def = def;
    kx_params[j].unit = "";
    kx_params[j].steps = steps;
    kx_params[j].choices = ch;
    kx_groups[j] = group;
}

/* the core's value of parameter j, as the DAW sees it */
static float kx_daw_of(uint32_t j)
{
    if (j < DW_TRK) {
        const track_t *t = &trk[j / DW_PER];
        uint32_t k = j % DW_PER, id = DW_TP[k];
        if (k >= DW_E0 && k <= DW_E7) {
            const param_desc_t *d = track_desc(t, id);
            return d->max > d->min ? (float)(t->p[id] - d->min) / (float)(d->max - d->min) : 0.0f;
        }
        return (float)t->p[id];
    }
    if (j == DW_SEL)
        return (float)(song.sel + 1u);
    return (float)song.g[DW_G[j - DW_DMG]];
}

/* .. and the DAW's value v into the core (audio flow) */
static void kx_daw_apply(uint32_t j, float v)
{
    int32_t x;
    if (j >= DW_N)
        return;
    if (j < DW_TRK) {
        track_t *t = &trk[j / DW_PER];
        uint32_t k = j % DW_PER, id = DW_TP[k];
        const param_desc_t *d = track_desc(t, id);
        if (k >= DW_E0 && k <= DW_E7)
            x = d->min + (int32_t)(v * (float)(d->max - d->min) + 0.5f);
        else
            x = (int32_t)(v + (v < 0 ? -0.5f : 0.5f));
        t->p[id] = (int16_t)clamp(x, d->min, d->max);
        if (id != P_MUTE && id != P_LEVEL && id != P_PAN)
            motion_capture(t, id, t->p[id]);
    } else if (j == DW_SEL) {
        track_select((uint32_t)clamp((int32_t)(v + 0.5f), 1, NTRK) - 1u);
    } else {
        uint32_t g = DW_G[j - DW_DMG];
        song.g[g] = (int16_t)clamp((int32_t)(v + 0.5f), GP[g].min, GP[g].max);
    }
    kx_dawv[j] = kx_daw_of(j);
}

static void kx_daw_sync(void)                     /* audio flow, every block */
{
    uint32_t j;
    for (j = 0; j < DW_N; j++) {
        float v = kx_daw_of(j);
        if (v != kx_dawv[j]) {
            kx_dawv[j] = v;
            vibe_param_set((int)j, v);
        }
    }
}

/* the DAW's transport: it plays, the steps play from their start at its tempo; it stops, they stop. PLAY on the VIBE
 * still plays while the DAW is stopped */
static int kx_daw_play;
static void kx_daw_transport(void)                /* audio flow, every block, before the core's */
{
    const vibe_transport_info *tp = vibe_transport();
    if (!tp || !tp->has) {
        if (kx_daw_play)
            transport_req = 2;
        kx_daw_play = 0;
        return;
    }
    if (tp->bpm >= 20.0f && tp->playing) {
        int32_t b = clamp((int32_t)(tp->bpm + 0.5f), GP[G_BPM].min, GP[G_BPM].max);
        if (song.g[G_BPM] != b)
            song.g[G_BPM] = (int16_t)b;
    }
    if (tp->playing && !kx_daw_play) {
        kx_daw_play = 1;
        transport_req = song.playing ? 3u : 1u;
    } else if (!tp->playing && kx_daw_play) {
        kx_daw_play = 0;
        transport_req = 2;
    }
}

static void kx_daw_init(void)
{
    static const char *const TAB[NTRK] = {"T1 · TRACK", "T2 · TRACK", "T3 · TRACK", "T4 · TRACK"};
    static const char *const SND[NTRK] = {"T1 · SOUND", "T2 · SOUND", "T3 · SOUND", "T4 · SOUND"};
    static const char *const ENV[NTRK] = {"T1 · ENVELOPE", "T2 · ENVELOPE", "T3 · ENVELOPE", "T4 · ENVELOPE"};
    static const char *const SEND[NTRK] = {"T1 · SENDS", "T2 · SENDS", "T3 · SENDS", "T4 · SENDS"};
    static const char *const PN[DW_PER] = {"LEVEL", "PAN", "MUTE", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8",
                                           "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DIST", "CHORUS", "DELAY", "REVERB"};
    static const char *const PI[DW_PER] = {"level", "pan", "mute", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8",
                                           "atk", "dec", "sus", "rel", "dist", "chorus", "delay", "reverb"};
    static const char *const GN[DW_N - DW_DMG] = {"DAMAGE", "CHARACTER", "WALL", "HISS", "RESONATE", "SPLICE", "HAUNT",
                                                  "HAUNT AMT", "SWING"};
    static const char *const GI[DW_N - DW_DMG] = {"damage", "damage_char", "wall", "hiss", "resonate", "splice", "haunt",
                                                  "haunt_amt", "swing"};
    static const char *const HM[3] = {"OFF", "SUCK", "DRAG"};
    uint32_t i, k, j;
    char id[16], nm[16];
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < DW_PER; k++) {
            const param_desc_t *d = &TP[DW_TP[k]];
            const char *grp = k <= DW_MIX_MUTE ? TAB[i] : k <= DW_E7 ? SND[i] : k <= DW_REL ? ENV[i] : SEND[i];
            j = i * DW_PER + k;
            if (k <= DW_MIX_MUTE) {               /* the SDK's mixer: mix_1_level .. (CONTROLLERS.md) */
                str_cpy(id, "mix_", sizeof id);
                id[4] = (char)('1' + i);
                id[5] = '_';
                str_cpy(id + 6, k == 0 ? "level" : k == 1 ? "pan" : "mute", sizeof id - 6);
            } else {
                id[0] = 't';
                id[1] = (char)('1' + i);
                id[2] = '_';
                str_cpy(id + 3, PI[k], sizeof id - 3);
            }
            nm[0] = 'T';
            nm[1] = (char)('1' + i);
            nm[2] = ' ';
            str_cpy(nm + 3, PN[k], sizeof nm - 3);
            if (k == DW_MIX_MUTE)
                kx_pset(j, id, nm, 0, 1, 0, 2, KX_ONOFF, grp);
            else if (k >= DW_E0 && k <= DW_E7)
                kx_pset(j, id, nm, 0, 1, 0, 0, 0, grp);
            else
                kx_pset(j, id, nm, d->min, d->max, d->def, 0, 0, grp);
        }
    kx_pset(DW_SEL, "mix_select", "TRACK", 1, NTRK, 1, NTRK, KX_TRACKS, "MASTER · MASTER");
    for (k = 0; k < DW_N - DW_DMG; k++) {
        const param_desc_t *d = &GP[DW_G[k]];
        kx_pset(DW_DMG + k, GI[k], GN[k], d->min, d->max, d->def, DW_G[k] == G_HAUNT ? 3 : 0,
                DW_G[k] == G_HAUNT ? HM : 0, "MASTER · MASTER");
    }
    vibe_params(kx_params, DW_N);
    vibe_param_groups(kx_groups);
    vibe_editor_accent(0xE01818);
    for (j = 0; j < DW_N; j++) {
        kx_dawv[j] = kx_daw_of(j);
        vibe_param_set((int)j, kx_dawv[j]);
        if (j < DW_TRK && j % DW_PER == DW_MIX_PAN)
            vibe_editor_control((int)j, VIBE_CTL_BIPOLAR);
        if (j < DW_TRK && j % DW_PER == DW_MIX_MUTE)
            vibe_editor_control((int)j, VIBE_CTL_TOGGLE);
    }
}
