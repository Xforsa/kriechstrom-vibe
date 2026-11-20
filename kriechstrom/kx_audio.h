/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the audio flow.
 *
 * The KRIECHSTROM core runs as on the FM-1: blocks of CTL (32) frames at 44.1 kHz from fx.c mix_block, the sequencer,
 * the arpeggiator and the MIDI in its events. The host's rate is the DAW's: at 44.1 kHz the core's output goes out as
 * it is (bit for bit the FM-1's mix, before its DAC), at any other rate through a windowed-sinc resampler.
 *
 * Two flows (VIBE_PARALLEL_AUDIO): the control flow (events, drawing) never writes the core. It posts commands into
 * kx_q (one writer, one reader); the audio flow applies them at the start of each block, so every write of the
 * core's state happens in this flow, between two of its blocks, as the FM-1's main loop and audio ISR shared it. */

/* ---- commands: control flow -> audio flow ---------------------------------------------------------------------- */
typedef struct { uint8_t op, a, b, c; int32_t v; } kx_cmd;
#define KX_QN 512u
static kx_cmd kx_q[KX_QN];
static uint32_t kx_qw, kx_qr;                     /* kx_qw: the control flow's, kx_qr: the audio flow's */
static uint32_t kx_qlost;                         /* commands dropped on a full queue (control flow) */
static void kx_post(uint32_t op, uint32_t a, uint32_t b, uint32_t c, int32_t v)
{
    uint32_t w = kx_qw, r = __atomic_load_n(&kx_qr, __ATOMIC_ACQUIRE);
    kx_cmd *k;
    if (w - r >= KX_QN) {
        kx_qlost++;
        return;
    }
    k = &kx_q[w % KX_QN];
    k->op = (uint8_t)op;
    k->a = (uint8_t)a;
    k->b = (uint8_t)b;
    k->c = (uint8_t)c;
    k->v = v;
    __atomic_store_n(&kx_qw, w + 1u, __ATOMIC_RELEASE);
}
static void kx_apply(const kx_cmd *k);            /* kx_ctl.h */

/* ---- state the control flow reads (display only): single writer, the audio flow -------------------------------- */
static uint32_t kx_blocks;                        /* CTL blocks rendered */
static float kx_sr = 44100.0f;
static float kx_peak[2];                          /* the output's peak, decaying (meters) */
#define KX_SCOPE 2048u
static int16_t kx_scope[KX_SCOPE];                /* the master, at 44.1 kHz (mono sum), for the display and the stage */
static uint32_t kx_scope_w;
static uint32_t kx_hits;                          /* +1 per hit heard in the mix (a fast level over a slow one) */
static float kx_hit_amt;                          /* .. how hard, 0..1 */
static float kx_env_f, kx_env_s;
static uint32_t kx_hit_hold;

static void kx_midi_block(void);                  /* kx_midi.h */
static void kx_rec_block(const int32_t *o);       /* kx_sampler.h */

/* ---- the core at 44.1 kHz ------------------------------------------------------------------------------------- */
#define KX_RB 1024u                               /* core frames kept for the resampler (a power of two) */
static float kx_rb[2][KX_RB];
static float kx_xrb[KX_XCH][KX_RB];               /* the parts (kx_outs.h), at the same places, while kx_xo_on */
static uint32_t kx_rb_w;                          /* frames written (the ring's head) */
static uint32_t kx_ms_acc;

static void kx_core_block(void)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    kx_midi_block();
    kx_rec_block(o);
    for (i = 0; i < CTL; i++) {
        int32_t l = o[2u * i], r = o[2u * i + 1u];
        uint32_t at = (kx_rb_w + i) & (KX_RB - 1u);
        kx_rb[0][at] = (float)l * (1.0f / 32768.0f);
        kx_rb[1][at] = (float)r * (1.0f / 32768.0f);
        if (i & 1u)
            kx_scope[kx_scope_w++ & (KX_SCOPE - 1u)] = (int16_t)((l + r) >> 1);
    }
    {   /* hits: the block's level against its slow average (the display's tears follow the drums) */
        float e = 0.0f;
        for (i = 0; i < CTL; i++) {
            float v = kx_rb[0][(kx_rb_w + i) & (KX_RB - 1u)];
            e += v < 0 ? -v : v;
        }
        e *= 1.0f / (float)CTL;
        kx_env_f = e > kx_env_f ? e : kx_env_f * 0.8f;
        kx_env_s += (e - kx_env_s) * 0.01f;
        if (kx_hit_hold)
            kx_hit_hold--;
        else if (kx_env_f > kx_env_s * 1.8f + 0.02f) {
            kx_hit_amt = kx_env_f > 0.5f ? 1.0f : kx_env_f * 2.0f;
            __atomic_store_n(&kx_hits, kx_hits + 1u, __ATOMIC_RELEASE);
            kx_hit_hold = 120;                    /* (~90 ms before the next) */
        }
    }
    if (kx_xo_on) {                               /* the parts, at the master's volume (fx.c mix_block's out) */
        int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;
        uint32_t c;
        for (c = 0; c < KX_XCH; c++) {
            float pk = kx_xo_peak[c / 2u];
            for (i = 0; i < CTL; i++) {
                float v = (float)(((kx_xo[c][i] >> 2) * mg) >> 10) * (1.0f / 32768.0f);
                kx_xrb[c][(kx_rb_w + i) & (KX_RB - 1u)] = v;
                v = v < 0 ? -v : v;
                pk = v > pk ? v : pk;
            }
            kx_xo_peak[c / 2u] = pk;
        }
    }
    kx_rb_w += CTL;
    kx_blocks++;
    kx_ms_acc += CTL * 1000u;                     /* fm1_ms: the core's clock, in this flow (fm1_ms at 44.1 kHz) */
    while (kx_ms_acc >= FS) {
        kx_ms_acc -= FS;
        fm1_ms++;
    }
}

/* ---- the resampler: 44.1 kHz -> the host's rate ------------------------------------------------------------------
 * A Kaiser-windowed sinc, KX_TAPS taps, KX_PH phases with linear interpolation between them; its cutoff is the lower of
 * the two Nyquists (less a guard band). About 0.4 ms of latency. */
#define KX_TAPS 32
#define KX_PH 256
static float kx_kern[KX_PH + 1][KX_TAPS];
static double kx_pos;                             /* the next output frame's place in core frames (absolute) */
static double kx_step;                            /* core frames per host frame: 44100 / rate */
static int kx_bypass;                             /* the host runs at 44.1 kHz: no resampling, bit for bit */

static float kx_bessel_i0(float x)
{
    float s = 1.0f, t = 1.0f;
    int k;
    for (k = 1; k < 30; k++) {
        t *= (x * 0.5f / (float)k) * (x * 0.5f / (float)k);
        s += t;
        if (t < s * 1e-9f)
            break;
    }
    return s;
}

/* a kernel: cutoff fc of the slower side's Nyquist, in the input's samples (unity gain at DC in every phase) */
static void kx_kernel(float (*kern)[KX_TAPS], float fc)
{
    const float beta = 8.0f;
    int p, k;
    fc *= 0.94f;
    for (p = 0; p <= KX_PH; p++) {
        float frac = (float)p / (float)KX_PH, sum = 0.0f;
        for (k = 0; k < KX_TAPS; k++) {
            float x = (float)(k - KX_TAPS / 2 + 1) - frac;     /* taps at -15 .. +16 around the place */
            float sx = x * fc, w = x / (float)(KX_TAPS / 2), win;
            float s = sx == 0.0f ? 1.0f : vd_sin(sx * 0.5f) / (VD_PI * sx);   /* sin(pi sx) / (pi sx): turns */
            w = w < -1.0f ? -1.0f : w > 1.0f ? 1.0f : w;
            win = kx_bessel_i0(beta * vd_sqrt(1.0f - w * w)) / kx_bessel_i0(beta);
            kern[p][k] = s * fc * win;
            sum += kern[p][k];
        }
        for (k = 0; k < KX_TAPS; k++)
            kern[p][k] /= sum;
    }
}

static void kx_resampler_init(float sr)
{
    kx_sr = sr;
    kx_step = 44100.0 / (double)sr;
    kx_bypass = sr > 44099.0f && sr < 44101.0f;
    kx_kernel(kx_kern, sr < 44100.0f ? sr / 44100.0f : 1.0f);
    kx_pos = 0.0;
}

static void kx_pull(float *l, float *r, int frames, float *const *xo)
{
    int j;
    uint32_t c;
    if (kx_bypass) {                              /* 44.1 kHz: the core's frames as they are */
        for (j = 0; j < frames; j++) {
            uint32_t at;
            if ((uint32_t)kx_pos + 1u > kx_rb_w)
                kx_core_block();
            at = (uint32_t)kx_pos & (KX_RB - 1u);
            l[j] = kx_rb[0][at];
            r[j] = kx_rb[1][at];
            if (xo)
                for (c = 0; c < KX_XCH; c++)
                    if (xo[c])
                        xo[c][j] = kx_xrb[c][at];
            kx_pos += 1.0;
        }
        return;
    }
    for (j = 0; j < frames; j++) {
        uint32_t base = (uint32_t)kx_pos, k;
        float frac = (float)(kx_pos - (double)base), fp = frac * (float)KX_PH, a, sl = 0.0f, sr = 0.0f;
        int p = (int)fp;
        if (p >= KX_PH)                           /* (frac, a float, may round up to 1.0) */
            p = KX_PH - 1;
        const float *k0, *k1;
        while (base + (uint32_t)(KX_TAPS / 2) + 1u > kx_rb_w)
            kx_core_block();
        a = fp - (float)p;
        k0 = kx_kern[p];
        k1 = kx_kern[p + 1];
        for (k = 0; k < (uint32_t)KX_TAPS; k++) {
            uint32_t at = (base + k - (uint32_t)(KX_TAPS / 2 - 1)) & (KX_RB - 1u);
            float c = k0[k] + (k1[k] - k0[k]) * a;
            sl += kx_rb[0][at] * c;
            sr += kx_rb[1][at] * c;
        }
        l[j] = sl;
        r[j] = sr;
        if (xo)                                   /* (the parts, the same kernel) */
            for (c = 0; c < KX_XCH; c++)
                if (xo[c]) {
                    float s = 0.0f;
                    for (k = 0; k < (uint32_t)KX_TAPS; k++)
                        s += kx_xrb[c][(base + k - (uint32_t)(KX_TAPS / 2 - 1)) & (KX_RB - 1u)] * (k0[k] + (k1[k] - k0[k]) * a);
                    xo[c][j] = s;
                }
        kx_pos += kx_step;
    }
}

static void kx_audio_start(float sr)
{
    kx_resampler_init(sr);
    kx_rb_w = 0;
    /* the first output frame sits KX_TAPS / 2 frames into the core's stream: the kernel's left half has its past */
    kx_pos = kx_bypass ? 0.0 : (double)(KX_TAPS / 2);
}

/* the core's messages (ui.c ui_say: "DICE: GRIND", "STOP TO EDIT") to the display: a seqlock, odd while written */
static char kx_msg[24];
static uint32_t kx_msg_seq;
static void kx_commands(void)
{
    uint32_t w = __atomic_load_n(&kx_qw, __ATOMIC_ACQUIRE), r = kx_qr;
    while (r != w) {
        kx_apply(&kx_q[r % KX_QN]);
        r++;
    }
    __atomic_store_n(&kx_qr, r, __ATOMIC_RELEASE);
    if (ui.msg_t) {
        __atomic_store_n(&kx_msg_seq, kx_msg_seq + 1u, __ATOMIC_RELEASE);
        str_cpy(kx_msg, ui.msg, sizeof kx_msg);
        __atomic_store_n(&kx_msg_seq, kx_msg_seq + 1u, __ATOMIC_RELEASE);
        ui.msg_t = 0;
    }
}

static void kx_midi_out(int frame);
static void kx_daw_transport(void);
static void kx_daw_sync(void);
static void kx_midi_audio(void);
static void kx_snap_poll_audio(void);               /* kx_ctl.h: the core's MIDI OUT queue to the host */

static void kx_input_push(const float *a, const float *b, int frames);   /* kx_input.h */
void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    int j;
    kx_input_push(in, in ? vibe_input(1) : 0, frames);
    kx_daw_transport();
    kx_commands();
    kx_snap_poll_audio();
    {   /* more outputs (SDK 26): OUT 3.. the parts, when the host has them and MULTI OUT is on */
        float *xo[KX_XCH];
        int no = vibe_output_count(), c, on = kx_xo_cfg && no > 2;
        if (on != kx_xo_on) {
            memset(kx_xrb, 0, sizeof kx_xrb);     /* (a fresh start: no stale tail) */
            kx_xo_on = on;
        }
        for (c = 0; c < KX_XCH; c++)
            xo[c] = on && c + 2 < no ? vibe_output(c + 2) : 0;
        kx_pull(out_l, out_r, frames, on ? xo : 0);
        if (on)
            for (c = 0; c < KX_XCH; c++)
                if (xo[c]) {
                    int jj;
                    for (jj = 0; jj < frames; jj++)
                        xo[c][jj] = xo[c][jj] > 8.0f ? 8.0f : xo[c][jj] < -8.0f ? -8.0f : xo[c][jj];   /* (a DAW's floats: headroom) */
                }
        for (c = 0; c < 6; c++)
            kx_xo_peak[c] *= 0.97f;
    }
    for (j = 0; j < frames; j++) {
        float a = out_l[j] < 0 ? -out_l[j] : out_l[j], b = out_r[j] < 0 ? -out_r[j] : out_r[j];
        out_l[j] = out_l[j] > 1.0f ? 1.0f : out_l[j] < -1.0f ? -1.0f : out_l[j];
        out_r[j] = out_r[j] > 1.0f ? 1.0f : out_r[j] < -1.0f ? -1.0f : out_r[j];
        if (a > kx_peak[0])
            kx_peak[0] = a;
        if (b > kx_peak[1])
            kx_peak[1] = b;
    }
    kx_peak[0] *= 0.97f;
    kx_peak[1] *= 0.97f;
    kx_daw_sync();
    kx_midi_out(0);
    kx_midi_audio();
}
