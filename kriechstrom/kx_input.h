/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the audio input (IN 1, IN 2), something the FM-1 does not have.
 *
 * The input is a fifth voice in the mix: a guitar, a voice, a drum machine through KRIECHSTROM. It enters the mix where
 * the four tracks do (core fx.c mix_block, the KRST_EXT_MIX hook), so the whole master works on it: DAMAGE, WALL,
 * RESONATE, SPLICE, HAUNT and HISS, and the shared DELAY, REVERB and CHORUS through its sends. And it plays with the
 * tracks:
 *   DRIVE  a gain into a soft clip (the core's softclip) before everything
 *   GATE   the input sounds only while a track plays (T1..T4: its notes open it; the drums chop the guitar)
 *   FEEDS  the input stands in for a track as INFECT's source (T1..T4): a track whose INSERT is INFCT and listens to
 *          that track hears the input instead (VOX: the voice speaks through the pad; DUCK: the guitar pumps the bass)
 * The host's rate in, 44.1 kHz for the core: resampled as the output is (kx_audio.h), about 1.5 ms of delay. */

enum { IC_LEVEL, IC_DRIVE, IC_PAN, IC_CHO, IC_DLY, IC_REV, IC_GATE, IC_FEED, IC_MODE,
       IC_MOUT, IC_MCH, IC_CLK, IC_CV, IC_SCREEN, IC_EXPR, IC_FSW, IC_SSRC, IC_SBARS, IC_SSLOT, IC_XOUT, IC_LIGHT,
       IC_PF0, IC_PF1, IC_PF2, IC_PF3, IC_PF4, IC_PF5, IC_PF6, IC_PF7, IC_PF8, IC_PF9, IC_PF10, IC_PF11, IC_PF12, IC_PF13, IC_PF14, IC_PF15, IC_N };   /* (IC_EXPR, IC_FSW: the foot pedal, kx_ui.h) */   /* (IC_SCREEN: the display's wear, kx_wear.h) */   /* (the last four: MIDI OUT and CV, kx_midi.h) */
static const struct { const char *label; int16_t min, max, def; } IC_DESC[IC_N] = {
    {"LEVEL", 0, 127, 100}, {"DRIVE", 0, 127, 0}, {"PAN", -64, 63, 0}, {"CHO", 0, 127, 0}, {"DLY", 0, 127, 0},
    {"REV", 0, 127, 24}, {"GATE", 0, NTRK, 0}, {"FEEDS", 0, NTRK, 0}, {"INPUT", 0, 2, 0},
    {"MIDI OUT", 0, 1, 1}, {"CHANNEL", 1, 13, 1}, {"CLOCK", 0, 1, 0}, {"CV TRACK", 0, NTRK, 1},
    {"SCREEN WEAR", 0, 2, 1}, {"EXPRESSION", 0, 3, 1}, {"SWITCH", 0, 3, 0},
    {"SOURCE", 0, 1, 0}, {"LENGTH", 0, 3, 2}, {"SLOT", 0, 2, 0},   /* (the sampler, kx_sampler.h) */
    {"MULTI OUT", 0, 1, 1},                           /* (more outputs: kx_outs.h) */
    {"LIGHTNING", 0, 1, 1},                           /* (the live lightning behind the pages: kx_wear.h) */
    /* the effects pads 9-16 hold (0-7) and with SHIFT (8-15): MENU > PAD FX */
    {"PAD FX", 0, PF_NFX - 1, PF_R16},
    {"PAD FX", 0, PF_NFX - 1, PF_TAPE},
    {"PAD FX", 0, PF_NFX - 1, PF_FRZ},
    {"PAD FX", 0, PF_NFX - 1, PF_GATE},
    {"PAD FX", 0, PF_NFX - 1, PF_CRSW},
    {"PAD FX", 0, PF_NFX - 1, PF_ODN},
    {"PAD FX", 0, PF_NFX - 1, PF_FLG},
    {"PAD FX", 0, PF_NFX - 1, PF_RAND},
    {"PAD FX", 0, PF_NFX - 1, PF_ROLL},
    {"PAD FX", 0, PF_NFX - 1, PF_RTHR},
    {"PAD FX", 0, PF_NFX - 1, PF_ETHR},
    {"PAD FX", 0, PF_NFX - 1, PF_LPF},
    {"PAD FX", 0, PF_NFX - 1, PF_HPF},
    {"PAD FX", 0, PF_NFX - 1, PF_OUP},
    {"PAD FX", 0, PF_NFX - 1, PF_PHS},
    {"PAD FX", 0, PF_NFX - 1, PF_R32},
};
static int16_t kx_ic[IC_N];                       /* the audio flow writes them (KXC_INCFG); the display reads */
static const char *const IC_TRK[NTRK + 1] = {"OFF", "T1", "T2", "T3", "T4"};
static const char *const IC_MODE_N[3] = {"LINE", "MIC", "HI-Z"};
static const char *const IC_WEAR_N[3] = {"OFF", "LOW", "FULL"};
static const char *const IC_EXPR_N[4] = {"OFF", "DAMAGE", "WALL", "LEVEL"};   /* the expression pedal turns */
static const char *const IC_SSRC_N[2] = {"INPUT", "MIX"};
static const char *const IC_SBARS_N[4] = {"FREE", "1 BAR", "2 BARS", "4 BARS"};
static const char *const IC_SSLOT_N[3] = {"USR1", "USR2", "USR3"};
static const char *const IC_ONOFF_N[2] = {"OFF", "ON"};
static const char *const IC_FSW_N[4] = {"PLAY", "DICE", "STOP", "FREEZE"};    /* the footswitch: PLAY, DICE, tape STOP or
                                                                                 FREEZE while held */

/* ---- host rate -> 44.1 kHz ---------------------------------------------------------------------------------------- */
#define KX_HIN 4096u                              /* the host's input frames kept */
#define KX_KIN 2048u                              /* the core's input frames waiting */
static float kx_hin[2][KX_HIN];
static uint32_t kx_hin_w;
static double kx_hin_pos, kx_in_step;             /* the next core frame's place in host frames, host frames per core frame */
static float kx_in_kern[KX_PH + 1][KX_TAPS];
static int16_t kx_kin[2][KX_KIN];
static uint32_t kx_kin_w, kx_kin_r;
static int kx_in_on;                              /* the host gives input (a block came with IN 1) */
static float kx_in_peak;
static int32_t kx_in_mono[CTL];                    /* the input's last block, mono (the sampler's) */                          /* the display's (decaying) */
static uint32_t kx_in_gate_g;                     /* GATE's gain, Q15 */

static void kx_input_start(void)
{
    uint32_t i;
    kx_in_step = (double)kx_sr / 44100.0;
    kx_kernel(kx_in_kern, kx_sr > 44100.0f ? 44100.0f / kx_sr : 1.0f);
    kx_hin_pos = (double)(KX_TAPS / 2);
    kx_hin_w = 0;
    kx_kin_r = 0;
    kx_kin_w = 2u * CTL + KX_TAPS;                /* (a little in hand: the core renders a block ahead) */
    for (i = 0; i < KX_KIN; i++)
        kx_kin[0][i] = kx_kin[1][i] = 0;
    for (i = 0; i < IC_N; i++)
        kx_ic[i] = IC_DESC[i].def;
    kx_in_gate_g = 32768u;
}

static void kx_input_push(const float *a, const float *b, int frames)   /* vibe_on_audio, before the output */
{
    int j;
    kx_in_on = a != 0;
    for (j = 0; j < frames; j++) {
        uint32_t at = (kx_hin_w + (uint32_t)j) & (KX_HIN - 1u);
        float x = a ? a[j] : 0.0f, y = b ? b[j] : x;
        kx_hin[0][at] = x;
        kx_hin[1][at] = y;
    }
    kx_hin_w += (uint32_t)frames;
    if (kx_bypass) {                              /* 44.1 kHz: as it is */
        for (j = 0; j < frames; j++) {
            uint32_t at = (kx_hin_w - (uint32_t)frames + (uint32_t)j) & (KX_HIN - 1u);
            float x = kx_hin[0][at], y = kx_hin[1][at];
            x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
            y = y > 1.0f ? 1.0f : y < -1.0f ? -1.0f : y;
            if (kx_kin_w - kx_kin_r < KX_KIN) {
                kx_kin[0][kx_kin_w & (KX_KIN - 1u)] = (int16_t)(x * 32767.0f);
                kx_kin[1][kx_kin_w & (KX_KIN - 1u)] = (int16_t)(y * 32767.0f);
                kx_kin_w++;
            }
        }
        return;
    }
    while ((uint32_t)kx_hin_pos + (uint32_t)(KX_TAPS / 2) + 1u <= kx_hin_w) {
        uint32_t base = (uint32_t)kx_hin_pos, k;
        float frac = (float)(kx_hin_pos - (double)base), fp = frac * (float)KX_PH, al, sl = 0.0f, sr = 0.0f;
        int p = (int)fp;
        if (p >= KX_PH)
            p = KX_PH - 1;
        al = fp - (float)p;
        for (k = 0; k < (uint32_t)KX_TAPS; k++) {
            uint32_t at = (base + k - (uint32_t)(KX_TAPS / 2 - 1)) & (KX_HIN - 1u);
            float c = kx_in_kern[p][k] + (kx_in_kern[p + 1][k] - kx_in_kern[p][k]) * al;
            sl += kx_hin[0][at] * c;
            sr += kx_hin[1][at] * c;
        }
        sl = sl > 1.0f ? 1.0f : sl < -1.0f ? -1.0f : sl;
        sr = sr > 1.0f ? 1.0f : sr < -1.0f ? -1.0f : sr;
        if (kx_kin_w - kx_kin_r < KX_KIN) {
            kx_kin[0][kx_kin_w & (KX_KIN - 1u)] = (int16_t)(sl * 32767.0f);
            kx_kin[1][kx_kin_w & (KX_KIN - 1u)] = (int16_t)(sr * 32767.0f);
            kx_kin_w++;
        }
        kx_hin_pos += kx_in_step;
    }
}

/* a track plays: one of its voices is sounding (the GATE) */
static int kx_trk_sounds(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].env_out > 900)
            return 1;
    return 0;
}

/* the core's hook (fx.c mix_block, after the parts): the input's block into the mix, the sends, INFECT */
static void kx_in_mix(uint32_t n)
{
    int32_t xl[CTL], xr[CTL];
    uint32_t i, avail = kx_kin_w - kx_kin_r, pk = 0;
    int32_t lvl = LEVEL_Q12[kx_ic[IC_LEVEL] & 127], pan = kx_ic[IC_PAN], drv = kx_ic[IC_DRIVE];
    int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
    int32_t c = kx_ic[IC_CHO] * 258, d = kx_ic[IC_DLY] * 258, r = kx_ic[IC_REV] * 258;
    int32_t gain = 256 + drv * drv / 4;           /* Q8: x1 .. ~x16 */
    uint32_t gate = (uint32_t)kx_ic[IC_GATE], feed = (uint32_t)kx_ic[IC_FEED], gtarget = 32768u;
    if (gate && gate <= NTRK)
        gtarget = kx_trk_sounds(&trk[gate - 1u]) ? 32768u : 0u;
    for (i = 0; i < n; i++) {
        int32_t a = 0, b = 0;
        if (i < avail) {
            a = kx_kin[0][(kx_kin_r + i) & (KX_KIN - 1u)];
            b = kx_kin[1][(kx_kin_r + i) & (KX_KIN - 1u)];
        }
        if (drv) {
            a = softclip((a * gain) >> 8);
            b = softclip((b * gain) >> 8);
        }
        /* GATE: opens in ~1 ms, closes in ~12 ms (a chop, not a click) */
        kx_in_gate_g = gtarget > kx_in_gate_g ? kx_in_gate_g + (32768u - kx_in_gate_g + 31u) / 32u
                                              : kx_in_gate_g - (kx_in_gate_g + 511u) / 512u;
        if (kx_in_gate_g > 32768u)
            kx_in_gate_g = 32768u;
        a = (int32_t)(((int64_t)a * (int64_t)kx_in_gate_g) >> 15);
        b = (int32_t)(((int64_t)b * (int64_t)kx_in_gate_g) >> 15);
        xl[i] = a;
        xr[i] = b;
        {
            uint32_t m = (uint32_t)(a < 0 ? -a : a);
            pk = m > pk ? m : pk;
        }
    }
    kx_kin_r += avail < n ? avail : n;
    kx_ped_input(xl, xr, n);                      /* (the input's pedals, kx_pedals.h) */
    for (i = 0; i < n; i++)
        kx_in_mono[i] = (xl[i] + xr[i]) >> 1;     /* (the sampler records it: kx_sampler.h) */
    if (feed && feed <= NTRK && (ifx_listen >> (feed - 1u) & 1u)) {   /* INFECT: the input stands in for that track */
        int32_t *dst = ifx_buf[ifx_half][feed - 1u], fp = 0;
        for (i = 0; i < n; i++) {
            int32_t v = clamp((xl[i] + xr[i]) >> 1, -32767, 32767), m = v < 0 ? -v : v;
            dst[i] = v;
            fp = m > fp ? m : fp;
        }
        ifx_pk[ifx_half][feed - 1u] = fp;
    }
    if (kx_ic[IC_LEVEL] > 0) {
        int32_t ol[CTL], orr[CTL];
        for (i = 0; i < n; i++) {
            int32_t x = ((xl[i] >> 2) * lvl) >> 10, y = ((xr[i] >> 2) * lvl) >> 10, m = (x + y) >> 1;
            if (c)
                send_c[i] += mulq15(m, c);
            if (d)
                send_d[i] += mulq15(m, d);
            if (r)
                send_r[i] += mulq15(m, r);
            ol[i] = (x * gl) >> 12;
            orr[i] = (y * gr) >> 12;
            mix_l[i] += ol[i];
            mix_r[i] += orr[i];
        }
        kx_xo_input(ol, orr, n);
    }
    {
        float p = (float)pk / 32768.0f;
        kx_in_peak = p > kx_in_peak ? p : kx_in_peak * 0.9f;
    }
    kx_ped_master(n);                             /* (the master's pedals, on the mix with the input in it) */
}
