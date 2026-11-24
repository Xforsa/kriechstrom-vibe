/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: its own punch-in effects, held on the pads (MENU > PAD FX gives them a pad, pads 9-16 at
 * first), worked on the whole mix after the core's master, in the audio flow. The FM-1's core is not touched: with none
 * held (and their tails gone) the mix passes as it was, bit for bit.
 * Each comes in at its own pace (PN_GROW, in beats: the press and the radio at once, the builds over two bars) and lets
 * go in a sixteenth (BUILD at once: the drop).
 *   PRESS      a hydraulic press: the mix pumped on every quarter, as EBM pumps it; it comes in at once
 *   PISTON     a machine speeding up: the mix chopped by a piston, 1/4 then 1/8, 1/16, 1/32
 *   BUILD      the build before a drop: the low end cut away higher and higher; let go, it all drops back at once
 *   TUNNEL     the mix driven away: darker, into an echo and a long dark room; let go, it comes back
 *   SHREDDER   the bar cut on its 1/16s: first the slice repeated, then the last bar's slices shuffled, then some backwards
 *   BLACKOUT   the power fails: the tape slows, the treble goes, a hum, then nothing; let go, it comes back
 *   SHORTWAVE  a shortwave radio: a narrow band, its tuning slowly off, soft static; it comes in quickly
 *   CORRODE    eaten away the longer it is held: the rate and the bits fall, crackle grows
 * kx_pn_lv says how far each is in, kx_pn_depth how far it has grown (the display reads both). */

#define PN_N 8
enum { PN_PRESS, PN_PISTON, PN_BUILD, PN_TUNNEL, PN_SHREDDER, PN_BLACKOUT, PN_SHORTWAVE, PN_CORRODE };
static const char *const PN_NAME[PN_N] = {"PRESS", "PISTON", "BUILD", "TUNNEL", "SHREDDER", "BLACKOUT", "SHORTWAVE", "CORRODE"};
static const char *const PN_CELL[PN_N] = {"PRESS", "PISTN", "BUILD", "TUNNL", "SHRED", "BLACK", "RADIO", "RUST"};
static const char *const PN_WHAT[PN_N] = {
    "pumped on every quarter, EBM style", "chopped by a piston, speeding up", "the build before the drop: let go",
    "driven away into the dark", "the bar's 1/16s repeated, shuffled", "the power fails, it sinks into hum",
    "a shortwave radio, softly tuned off", "eaten away, the longer the worse"};

#define KX_NFX (PF_NFX + PN_N)                    /* the pads' effects: the core's 18, then these 8 */
static const char *kx_fx_cell(int fx) { return fx < PF_NFX ? PF_CELL[fx] : PN_CELL[(fx - PF_NFX) % PN_N]; }
static const char *kx_fx_name(int fx) { return fx < PF_NFX ? PF_NAME[fx] : PN_NAME[(fx - PF_NFX) % PN_N]; }

static uint8_t kx_fx_on[KX_NFX];                  /* the effects held now, from the pads, the stage, the foot (control flow) */
static uint64_t kx_fx_t0[KX_NFX];                 /* when each was pressed */
static int kx_fx_any(void)
{
    int i;
    for (i = 0; i < KX_NFX; i++)
        if (kx_fx_on[i])
            return 1;
    return 0;
}

static uint32_t kx_pn_held;                       /* (the audio flow's: KXC_PERF sets and clears it) */
static float kx_pn_lv[PN_N];                      /* each one's level now, 0 .. 1 */
static float kx_pn_depth[PN_N];                   /* how far each has grown, 0 .. 1 (two bars to all of it) */
static float kx_pn_held_s[PN_N];                  /* how long each has been held, in seconds */

#define PN_HN 262144u                             /* the last 5.9 s (SHREDDER's last bar, BLACKOUT's tape) */
static float pn_h[2][PN_HN];
static uint32_t pn_hw;
static uint32_t pn_rng = 0x2611BEEFu;
static float pn_rand(void) { pn_rng ^= pn_rng << 13; pn_rng ^= pn_rng >> 17; pn_rng ^= pn_rng << 5; return (float)(pn_rng >> 8) * (1.0f / 16777216.0f); }

/* PISTON: its gate's level */
static float pn_pi_g = 1.0f;
/* BUILD: its high-pass (two one-poles taken away) */
static float pn_bd_s1[2], pn_bd_s2[2];
/* TUNNEL: a low-pass, an echo, the dark room (four combs, two all-passes), the room's energy */
static float pn_tn_lp[2], pn_tn_e[2][32768], pn_v_energy;
static uint32_t pn_tn_ei;
#define PN_VC 4
static float pn_vc[PN_VC][2400], pn_vc_lp[PN_VC], pn_va[2][700];
static uint32_t pn_vc_i[PN_VC], pn_va_i[2];
static const uint32_t PN_VC_LEN[PN_VC] = {1557, 1617, 1491, 2277};
static const uint32_t PN_VA_LEN[2] = {556, 441};
/* SHREDDER: the slice playing (where it is read from, backwards or not), the sixteenth it began */
static uint32_t pn_sh_src, pn_sh_s16 = 0xFFFFFFFFu, pn_sh_bar;
static int pn_sh_rev;
/* BLACKOUT: the power (1 on, 0 off), the tape's reader */
static float pn_pow = 1.0f, pn_bread, pn_blp[2];
/* SHORTWAVE: a band-pass, the tuning's phase, the whistle */
static float pn_sw_lo[2], pn_sw_hi[2], pn_sw_ph;
/* CORRODE: the held sample, its counter, a low-pass */
static float pn_cr_hold[2], pn_cr_cnt, pn_cr_lp[2];

static void kx_pn_set(uint32_t fx, int down)      /* KXC_PERF on one of these: the audio flow */
{
    if (fx >= PN_N)
        return;
    if (down) {
        kx_pn_held |= 1u << fx;
        kx_pn_held_s[fx] = 0.0f;
        if (fx == PN_SHREDDER)
            pn_sh_s16 = 0xFFFFFFFFu;
    } else
        kx_pn_held &= ~(1u << fx);
}

static float pn_room(float x)                     /* the dark room: one sample in, one out (mono) */
{
    float s = 0.0f, y;
    uint32_t k;
    for (k = 0; k < PN_VC; k++) {
        float *b = pn_vc[k], o = b[pn_vc_i[k]];
        pn_vc_lp[k] += (o - pn_vc_lp[k]) * 0.3f;
        b[pn_vc_i[k]] = x + pn_vc_lp[k] * 0.9f;
        pn_vc_i[k] = pn_vc_i[k] + 1u == PN_VC_LEN[k] ? 0u : pn_vc_i[k] + 1u;
        s += o;
    }
    y = s * 0.25f;
    for (k = 0; k < 2u; k++) {
        float *b = pn_va[k], o = b[pn_va_i[k]], in = y + o * 0.5f;
        b[pn_va_i[k]] = in;
        y = o - in * 0.5f;
        pn_va_i[k] = pn_va_i[k] + 1u == PN_VA_LEN[k] ? 0u : pn_va_i[k] + 1u;
    }
    return y;
}
static const float PN_GROW[PN_N] = {0.15f, 8.0f, 8.0f, 4.0f, 8.0f, 8.0f, 0.5f, 8.0f};   /* (beats to all of it) */
static float pn_ease(float x) { x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; return x * x * (3.0f - 2.0f * x); }

static void kx_punch_block(int32_t *o, uint32_t n)
{
    uint32_t i, k, active = kx_pn_held;
    float beat = (float)beat_samples(), dt = (float)n / (float)FS, grow = 8.0f * beat / (float)FS;   /* (two bars, in s) */
    float up = (float)n / (0.02f * (float)FS), down = (float)n / (beat * 0.25f);   /* (in: 20 ms; out: a sixteenth) */
    for (k = 0; k < PN_N; k++) {
        int held = (kx_pn_held >> k) & 1u;
        float gs = PN_GROW[k] * beat / (float)FS;   /* (that one's time to all of it, in s) */
        kx_pn_lv[k] += held ? vd_minf(up, 1.0f - kx_pn_lv[k]) : -vd_minf(k == PN_BUILD ? 1.0f : down, kx_pn_lv[k]);
        if (held)
            kx_pn_held_s[k] += dt;
        kx_pn_depth[k] = pn_ease((PN_GROW[k] < 1.0f ? 0.0f : 0.12f) + (PN_GROW[k] < 1.0f ? 1.0f : 0.88f) * kx_pn_held_s[k] / (gs > 0.01f ? gs : 0.01f));
        if (kx_pn_lv[k] > 0.0f)
            active |= 1u << k;
    }
    if (pn_pow < 1.0f)
        active |= 1u << PN_BLACKOUT;
    if (pn_v_energy > 1e-6f)
        active |= 1u << PN_TUNNEL;
    for (i = 0; i < n; i++) {                     /* (the history is always kept: an effect starts from what was) */
        pn_h[0][pn_hw & (PN_HN - 1u)] = (float)o[2u * i] * (1.0f / 32768.0f);
        pn_h[1][pn_hw & (PN_HN - 1u)] = (float)o[2u * i + 1u] * (1.0f / 32768.0f);
        pn_hw++;
        if (!active)
            continue;
        {
            float l = pn_h[0][(pn_hw - 1u) & (PN_HN - 1u)], r = pn_h[1][(pn_hw - 1u) & (PN_HN - 1u)], a, d;
            uint32_t bi = (uint32_t)(beat > 1.0f ? beat : 1.0f), bpos = (beat_pos + i) % bi;
            float ph = (float)bpos / beat;                         /* (where in the beat) */
            if ((a = kx_pn_lv[PN_SHREDDER]) > 0.0f) {             /* SHREDDER: the bar's 1/16s */
                uint32_t sl = bi / 4u, barpos = (beat_n % 4u) * bi + bpos, s16 = barpos / sl, off = barpos % sl;
                float sl_l, sl_r, w;
                d = kx_pn_depth[PN_SHREDDER];
                if (s16 != pn_sh_s16) {           /* a new sixteenth: what it plays */
                    uint32_t now = pn_hw - 1u, bar_start = now - barpos, h;
                    if (pn_sh_s16 == 0xFFFFFFFFu)
                        pn_sh_src = now - off, pn_sh_bar = 0;   /* (the slice it was pressed in) */
                    if (s16 == 0u)
                        pn_sh_bar++;
                    h = (s16 * 2654435761u) ^ (pn_sh_bar * 40503u);
                    if (d < 0.35f)                /* first: the slice held, repeated */
                        pn_sh_rev = 0;
                    else {                        /* then: the last bar's slices, shuffled; then some backwards */
                        uint32_t j = d < 0.7f ? (s16 & ~1u) + ((h >> 7) & 1u) : (h >> 9) % 16u;
                        pn_sh_src = bar_start - 4u * bi + j * sl;
                        pn_sh_rev = d > 0.7f && ((h >> 13) % 3u) == 0u;
                    }
                    pn_sh_s16 = s16;
                }
                {
                    uint32_t at = (pn_sh_rev ? pn_sh_src + sl - 1u - off : pn_sh_src + off) & (PN_HN - 1u);
                    float e = (float)off / (float)sl;
                    w = e < 0.02f ? e / 0.02f : e > 0.98f ? (1.0f - e) / 0.02f : 1.0f;   /* (no clicks at the cuts) */
                    sl_l = pn_h[0][at] * w, sl_r = pn_h[1][at] * w;
                }
                l += (sl_l - l) * a, r += (sl_r - r) * a;
            }
            if ((a = kx_pn_lv[PN_PRESS]) > 0.0f) {                /* PRESS: the quarter's pump (a kick's sidechain) */
                float dd = kx_pn_depth[PN_PRESS], rel = vd_minf(ph / 0.62f, 1.0f), duck, pl, pr;
                duck = 1.0f - 0.78f * dd * (1.0f - rel * rel * (3.0f - 2.0f * rel));   /* (down on the beat, up through it) */
                pl = vd_tanh(l * 1.6f) * 0.78f * duck * 1.3f, pr = vd_tanh(r * 1.6f) * 0.78f * duck * 1.3f;
                l += (pl - l) * a, r += (pr - r) * a;
            }
            if ((a = kx_pn_lv[PN_PISTON]) > 0.0f) {               /* PISTON: chopped, speeding up */
                float div, x, gate;
                d = kx_pn_depth[PN_PISTON];
                div = d < 0.3f ? 1.0f : d < 0.6f ? 2.0f : d < 0.85f ? 4.0f : 8.0f;
                x = ph * div;
                x -= (float)(int)x;
                gate = x < 0.5f ? 1.0f : 0.0f;
                pn_pi_g += (gate - pn_pi_g) * 0.01f;   /* (2 ms edges) */
                {
                    float g = 1.0f - (1.0f - pn_pi_g) * (0.55f + 0.45f * d);
                    l *= 1.0f - a + a * g, r *= 1.0f - a + a * g;
                }
            }
            if ((a = kx_pn_lv[PN_BUILD]) > 0.0f) {                /* BUILD: the low end cut away, higher and higher */
                float fc, c, hl, hr;
                d = kx_pn_depth[PN_BUILD];
                fc = 30.0f * vd_exp2(d * 6.6f);           /* (30 Hz to about 3 kHz over its two bars) */
                c = 6.2831853f * fc / (float)FS;
                c = c > 0.9f ? 0.9f : c;
                for (k = 0; k < 2u; k++) {                /* (two poles: 12 dB an octave, a little ring at the corner) */
                    float x = k ? r : l;
                    pn_bd_s1[k] += (x - pn_bd_s1[k]) * c;
                    pn_bd_s2[k] += (pn_bd_s1[k] - pn_bd_s2[k]) * c;
                }
                hl = (l - pn_bd_s2[0]) * (1.0f + 0.6f * d), hr = (r - pn_bd_s2[1]) * (1.0f + 0.6f * d);
                l += (hl - l) * a, r += (hr - r) * a;
            }
            if ((a = kx_pn_lv[PN_TUNNEL]) > 0.0f || pn_v_energy > 1e-6f) {   /* TUNNEL: away into the dark */
                float c, e0, e1, y, send;
                uint32_t el = (uint32_t)(beat * 0.75f) & 32767u;
                d = kx_pn_depth[PN_TUNNEL] * a;
                c = 0.9f - 0.84f * d;              /* (darker as it goes) */
                pn_tn_lp[0] += (l - pn_tn_lp[0]) * c;
                pn_tn_lp[1] += (r - pn_tn_lp[1]) * c;
                e0 = pn_tn_e[0][(pn_tn_ei - el) & 32767u], e1 = pn_tn_e[1][(pn_tn_ei - el) & 32767u];
                pn_tn_e[0][pn_tn_ei & 32767u] = pn_tn_lp[0] * d + e0 * (0.3f + 0.4f * d);
                pn_tn_e[1][pn_tn_ei & 32767u] = pn_tn_lp[1] * d + e1 * (0.3f + 0.4f * d);
                pn_tn_ei++;
                send = (pn_tn_lp[0] + pn_tn_lp[1]) * 0.5f * d;
                y = pn_room(send);
                pn_v_energy = pn_v_energy * 0.9999f + y * y * 0.0001f + e0 * e0 * 0.00002f;
                if (a > 0.0f) {
                    float dry = 1.0f - 0.8f * d;
                    float tl = pn_tn_lp[0] * dry + e0 * 0.6f + y * 0.9f, tr = pn_tn_lp[1] * dry + e1 * 0.6f + y * 0.9f;
                    l += (tl - l) * a, r += (tr - r) * a;
                } else                            /* (let go: the dry back, the tail rings on under it) */
                    l += e0 * 0.6f + y * 0.9f, r += e1 * 0.6f + y * 0.9f;
            }
            if ((kx_pn_held >> PN_BLACKOUT) & 1u || pn_pow < 1.0f) {   /* BLACKOUT: the tape slows to nothing */
                float tgt = (kx_pn_held >> PN_BLACKOUT) & 1u ? 0.0f : 1.0f, bl, br, hum;
                uint32_t at;
                if (pn_pow >= 1.0f)                /* (it starts from the sound as it is, where it is) */
                    pn_bread = (float)(pn_hw - 1u), pn_blp[0] = l, pn_blp[1] = r;
                pn_pow += tgt < pn_pow ? -1.0f / (grow * (float)FS) : 1.0f / (0.25f * (float)FS);   /* (two bars down) */
                pn_pow = pn_pow < 0.0f ? 0.0f : pn_pow > 1.0f ? 1.0f : pn_pow;
                if ((float)(pn_hw - 1u) - pn_bread > (float)(PN_HN - 64u))
                    pn_bread = (float)(pn_hw - 1u) - (float)(PN_HN - 64u);
                at = (uint32_t)pn_bread & (PN_HN - 1u);   /* (read where it is, then move on: never ahead of the tape) */
                bl = pn_h[0][at], br = pn_h[1][at];
                if (pn_pow < 1.0f)
                    pn_bread += pn_pow * pn_pow;
                pn_blp[0] += (bl - pn_blp[0]) * (0.02f + 0.9f * pn_pow * pn_pow);
                pn_blp[1] += (br - pn_blp[1]) * (0.02f + 0.9f * pn_pow * pn_pow);
                hum = vd_sin((float)(pn_hw % 882u) / 882.0f) * 0.05f * vd_sqrt(1.0f - pn_pow) * (pn_pow > 0.02f ? 1.0f : 0.3f);
                if (pn_pow < 1.0f)
                    l = pn_blp[0] * pn_pow + hum, r = pn_blp[1] * pn_pow + hum;
            }
            if ((a = kx_pn_lv[PN_SHORTWAVE]) > 0.0f) {            /* SHORTWAVE: the band narrowing, drifting */
                float c, m = (l + r) * 0.5f, y, hi, lo, st;
                d = kx_pn_depth[PN_SHORTWAVE];
                pn_sw_ph += 0.04f / (float)FS;           /* (the tuning drifts slowly: no wobble) */
                c = 0.22f - 0.12f * d + 0.015f * vd_sin(pn_sw_ph);
                pn_sw_hi[0] += (m - pn_sw_hi[0]) * (0.005f + 0.03f * d);
                hi = m - pn_sw_hi[0];
                pn_sw_lo[0] += (hi - pn_sw_lo[0]) * c;
                pn_sw_lo[1] += (pn_sw_lo[0] - pn_sw_lo[1]) * c;
                lo = pn_sw_lo[1];
                st = (pn_rand() - 0.5f) * 0.02f * d;     /* (soft static, steady) */
                y = vd_tanh(lo * (1.5f + 2.0f * d)) * 0.95f + st;   /* (no whistle) */
                a *= 0.25f + 0.75f * d;            /* (the radio comes in as it grows) */
                l += (y - l) * a, r += (y - r) * a;
            }
            if ((a = kx_pn_lv[PN_CORRODE]) > 0.0f) {              /* CORRODE: worse the longer held */
                float hs = kx_pn_held_s[PN_CORRODE], div = 1.0f + hs * hs * 4.0f, q, cl, cr;
                div = div > 40.0f ? 40.0f : div;
                q = 96.0f / (1.0f + hs * 2.5f);
                q = q < 3.0f ? 3.0f : q;
                if ((pn_cr_cnt += 1.0f) >= div)
                    pn_cr_cnt -= div, pn_cr_hold[0] = (float)(int)(l * q) / q, pn_cr_hold[1] = (float)(int)(r * q) / q;
                pn_cr_lp[0] += (pn_cr_hold[0] - pn_cr_lp[0]) * 0.6f;
                pn_cr_lp[1] += (pn_cr_hold[1] - pn_cr_lp[1]) * 0.6f;
                cl = pn_cr_lp[0], cr = pn_cr_lp[1];
                if (pn_rand() < 0.0003f * (1.0f + hs * 3.0f))
                    cl += (pn_rand() - 0.5f) * 0.4f, cr += (pn_rand() - 0.5f) * 0.4f;
                l += (cl - l) * a, r += (cr - r) * a;
            }
            o[2u * i] = (int32_t)clamp((int32_t)(l * 32768.0f), -32768, 32767);
            o[2u * i + 1u] = (int32_t)clamp((int32_t)(r * 32768.0f), -32768, 32767);
        }
    }
}
