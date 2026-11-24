/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: SPINE, the song's backbone. It names the key the tracks are in (by their notes, or as set),
 * walks a progression of up to eight chords (a step each 1, 2 or 4 bars), and moves the tracks that follow it to each
 * chord's root by their TRN, as a hand on their TRN knobs would. On its page the keys move them live.
 *   SEQ > SPINE: ENC 1 on / off, ENC 2 the key (AUTO: as heard in the notes), ENC 3 the bars a step, ENC 4 the steps;
 *   DATA the chosen step's chord, ENTER the next step, F5 DICE a progression in the MOOD; pads 1-4: who follows.
 * The drum tracks never follow. Off, the tracks' TRN are as they were. */

static const char *const SP_KEYN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const uint8_t SP_MAJ[7] = {0, 2, 4, 5, 7, 9, 11}, SP_MIN[7] = {0, 2, 3, 5, 7, 8, 10};
static const char *const SP_RMIN[7] = {"i", "ii", "III", "iv", "v", "VI", "VII"};
static const char *const SP_RMAJ[7] = {"I", "ii", "iii", "IV", "V", "vi", "vii"};
static int kx_sp_key = 4, kx_sp_minor = 1;        /* as heard (the display finds it; the audio flow reads it) */
static int kx_sp_step;                            /* the step playing (the audio flow's) */
static int kx_sp_live;                            /* the keys' live move, semitones from the key */
static int kx_sp_sel;                             /* the step chosen on the page */

static int kx_sp_root(void) { return kx_ic[IC_SP_KEY] ? kx_ic[IC_SP_KEY] - 1 : kx_sp_key; }
static int kx_sp_semis(int deg)                   /* a degree's root above the key, nearest (-5 .. +6) */
{
    int s = (kx_sp_minor ? SP_MIN : SP_MAJ)[clamp(deg, 0, 6)];
    return s > 6 ? s - 12 : s;
}
static const char *kx_sp_roman(int deg) { return (kx_sp_minor ? SP_RMIN : SP_RMAJ)[clamp(deg, 0, 6)]; }

/* ---- the key, as heard: the routed tracks' notes against the 24 keys' profiles (Krumhansl and Kessler) -------- */
static void kx_sp_detect(void)
{
    static const float PMAJ[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
    static const float PMIN[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
    float h[12] = {0}, best = -1e9f, tot = 0.0f;
    uint32_t k, i, j;
    int r, m;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        if (drum_track(t) || !((kx_ic[IC_SP_ROUTE] >> k) & 1))
            continue;
        for (i = 0; i < len; i++) {
            const step_t *st = &t->step[i];
            if (st->time != ST_NOTE)
                continue;
            for (j = 0; j < st->n; j++)
                h[st->note[j] % 12u] += 1.0f, tot += 1.0f;
        }
    }
    if (tot < 3.0f)
        return;                                   /* (too few notes: the key stays as it was) */
    for (m = 0; m < 2; m++)
        for (r = 0; r < 12; r++) {
            float s = 0.0f;
            for (i = 0; i < 12u; i++)
                s += h[(i + (uint32_t)r) % 12u] * (m ? PMIN : PMAJ)[i];
            if (s > best)
                best = s, kx_sp_key = r, kx_sp_minor = m;
        }
}

/* ---- the audio flow: each bar, the next chord; the tracks moved there ----------------------------------------- */
static int16_t sp_base[NTRK], sp_qnt[NTRK], sp_scl[NTRK];      /* the tracks' TRN and QNT before SPINE took them */
static int sp_on, sp_bar, sp_lastbeat = -1, sp_was_step = -1, sp_was_live, sp_was_route;
static void sp_apply(void)
{
    uint32_t k;
    int off = kx_sp_semis(kx_ic[IC_SP_P0 + kx_sp_step]) + kx_sp_live;
    off = off > 12 ? off - 12 : off < -12 ? off + 12 : off;
    for (k = 0; k < NTRK; k++) {                  /* (a follower's QNT SEQ+T: TRN moves its steps too, in its scale) */
        int fol = ((kx_ic[IC_SP_ROUTE] >> k) & 1) && !drum_track(&trk[k]);
        trk[k].p[P_TRANS] = (int16_t)(fol ? clamp(sp_base[k] + off, -24, 24) : sp_base[k]);
        trk[k].p[P_QUANT] = (int16_t)(fol ? QN_SEQT : sp_qnt[k]);
        trk[k].p[P_SCALE] = (int16_t)(fol && sp_qnt[k] < QN_SEQ ? 0 : sp_scl[k]);   /* (its riff moved note for note: CHR,
                                                                                  * unless its steps were already kept in a scale) */
    }
    sp_was_step = kx_sp_step, sp_was_live = kx_sp_live, sp_was_route = kx_ic[IC_SP_ROUTE];
}
static void kx_spine_block(void)
{
    uint32_t k;
    int len = clamp(kx_ic[IC_SP_LEN], 1, 8), bars = 1 << clamp(kx_ic[IC_SP_BARS], 0, 2), b = (int)beat_n;
    if (!kx_ic[IC_SP_ON]) {
        if (sp_on) {                              /* off: the tracks as they were */
            for (k = 0; k < NTRK; k++)
                trk[k].p[P_TRANS] = sp_base[k], trk[k].p[P_QUANT] = sp_qnt[k], trk[k].p[P_SCALE] = sp_scl[k];
            sp_on = 0;
        }
        return;
    }
    if (!sp_on) {
        for (k = 0; k < NTRK; k++)
            sp_base[k] = trk[k].p[P_TRANS], sp_qnt[k] = trk[k].p[P_QUANT], sp_scl[k] = trk[k].p[P_SCALE];
        sp_on = 1, kx_sp_step = 0, sp_bar = 0;
        sp_apply();
    }
    if (!song.playing) {                          /* stopped: back to the first chord */
        sp_bar = 0, sp_lastbeat = -1;
        if (kx_sp_step)
            kx_sp_step = 0;
    } else if (b != sp_lastbeat) {
        if (b == 0 && sp_lastbeat >= 0 && ++sp_bar >= bars)
            sp_bar = 0, kx_sp_step = (kx_sp_step + 1) % len;
        sp_lastbeat = b;
    }
    if (kx_sp_step >= len)
        kx_sp_step = 0;
    if (kx_sp_step != sp_was_step || kx_sp_live != sp_was_live || kx_ic[IC_SP_ROUTE] != sp_was_route)
        sp_apply();
}

